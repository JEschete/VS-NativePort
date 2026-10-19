#include "Platform/CrashHandler.hpp"

#include <memory>

// clang-format off: DbgHelp requires the Windows SDK types first.
#include <Windows.h>
#include <DbgHelp.h>
// clang-format on

#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <mutex>
#include <thread>

#include "Core/Log.hpp"

#pragma comment(lib, "dbghelp.lib")

namespace {
constexpr ULONG kSymbolNameCapacity = 256;
constexpr int kCrashFrameLimit = 48;
constexpr ULONG kCapturedFrameLimit = 32;
constexpr USHORT kLoggedFrameLimit = 16;
constexpr std::size_t kWindowsPathCapacity = 32768;
constexpr int kWatchdogSeconds = 10;
constexpr int kWatchdogExitCode = 124;
constexpr int kTerminationExitCode = 3;
constexpr int kWatchFrameLimit = 10;
constexpr int kWatchHitLimit = 32;
constexpr unsigned kDebugConditionShift = 16;
constexpr unsigned kDebugLengthShift = 18;
}  // namespace

namespace VS::Platform {

struct ProgressWatchdog::State {
  HANDLE thread = nullptr;
  std::mutex mutex;
  std::condition_variable changed;
  std::chrono::steady_clock::time_point progress =
      std::chrono::steady_clock::now();
  std::uint64_t tick = 0;
  bool stopped = false;
  std::thread worker;

  void Report() {
    CONTEXT context{};
    context.ContextFlags = CONTEXT_FULL;
    bool captured = false;
    if (SuspendThread(thread) != static_cast<DWORD>(-1)) {
      captured = GetThreadContext(thread, &context) != FALSE;
      ResumeThread(thread);
    }
    wchar_t executable[kWindowsPathCapacity]{};
    GetModuleFileNameW(nullptr, executable,
                       static_cast<DWORD>(std::size(executable)));
    const auto path =
        std::filesystem::path(executable).parent_path() / "vs-native-hang.log";
    std::FILE* report = _wfopen(path.c_str(), L"w");
    if (report == nullptr) return;
    std::fprintf(report,
                 "No logical progress for %d seconds; tick=%" PRIu64
                 "; context=%s\n",
                 kWatchdogSeconds, tick, captured ? "captured" : "unavailable");
    if (captured) {
      HANDLE process = GetCurrentProcess();
      SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
      SymInitialize(process, nullptr, TRUE);
      STACKFRAME64 frame{};
      frame.AddrPC = {context.Rip, 0, AddrModeFlat};
      frame.AddrFrame = {context.Rbp, 0, AddrModeFlat};
      frame.AddrStack = {context.Rsp, 0, AddrModeFlat};
      alignas(
          SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + kSymbolNameCapacity]{};
      for (ULONG depth = 0;
           depth < kCapturedFrameLimit && frame.AddrPC.Offset != 0; ++depth) {
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = kSymbolNameCapacity - 1;
        DWORD64 displacement = 0;
        const char* name =
            SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)
                ? symbol->Name
                : "?";
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof line;
        DWORD line_displacement = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset,
                                 &line_displacement, &line)) {
          std::fprintf(report, "#%lu %s (%s:%lu)\n", depth, name, line.FileName,
                       line.LineNumber);
        } else {
          std::fprintf(report, "#%lu %s +0x%" PRIX64 " [0x%" PRIX64 "]\n",
                       depth, name, static_cast<std::uint64_t>(displacement),
                       static_cast<std::uint64_t>(frame.AddrPC.Offset));
        }
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame,
                         &context, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr)) {
          break;
        }
      }
    }
    std::fclose(report);
  }
};

ProgressWatchdog::ProgressWatchdog(bool enabled) {
  if (!enabled) return;
  m_state_ = std::make_unique<State>();
  if (!DuplicateHandle(
          GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
          &m_state_->thread,
          THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
          FALSE, 0)) {
    m_state_.reset();
    return;
  }
  State* state = m_state_.get();
  state->worker = std::thread([state] {
    std::unique_lock lock(state->mutex);
    while (!state->stopped) {
      const auto observed = state->progress;
      if (!state->changed.wait_until(
              lock, observed + std::chrono::seconds(kWatchdogSeconds),
              [&] { return state->stopped || state->progress != observed; })) {
        lock.unlock();
        state->Report();
        std::_Exit(kWatchdogExitCode);
      }
    }
  });
}

ProgressWatchdog::~ProgressWatchdog() {
  if (!m_state_) return;
  {
    std::lock_guard lock(m_state_->mutex);
    m_state_->stopped = true;
  }
  m_state_->changed.notify_one();
  m_state_->worker.join();
  CloseHandle(m_state_->thread);
}

void ProgressWatchdog::Pulse(std::uint64_t tick) {
  if (!m_state_) return;
  {
    std::lock_guard lock(m_state_->mutex);
    m_state_->progress = std::chrono::steady_clock::now();
    m_state_->tick = tick;
  }
  m_state_->changed.notify_one();
}

namespace {

void LogStackFrame(HANDLE process, DWORD64 address, Core::LogLevel level,
                   std::string_view channel, unsigned depth) {
  alignas(
      SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + kSymbolNameCapacity]{};
  auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = kSymbolNameCapacity - 1;
  DWORD64 displacement = 0;
  const char* name =
      SymFromAddr(process, address, &displacement, symbol) ? symbol->Name : "?";
  IMAGEHLP_LINE64 line{};
  line.SizeOfStruct = sizeof(line);
  DWORD line_displacement = 0;
  if (SymGetLineFromAddr64(process, address, &line_displacement, &line)) {
    Core::Log(level, channel, "  #{} {} ({}:{})", depth, name, line.FileName,
              line.LineNumber);
  } else if (level == Core::LogLevel::Error) {
    Core::Log(level, channel, "  #{} {} +0x{:X}", depth, name, displacement);
  } else {
    Core::Log(level, channel, "  #{} {}", depth, name);
  }
}

LONG WINAPI OnCrash(EXCEPTION_POINTERS* info) {
  const EXCEPTION_RECORD* rec = info->ExceptionRecord;
  if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
      rec->NumberParameters >= 2) {
    VS_LOG_ERROR("crash", "access violation {} 0x{:X} at 0x{:X}",
                 rec->ExceptionInformation[0] == 1 ? "writing" : "reading",
                 rec->ExceptionInformation[1],
                 reinterpret_cast<uintptr_t>(rec->ExceptionAddress));
  } else {
    VS_LOG_ERROR("crash", "exception 0x{:08X} at 0x{:X}",
                 static_cast<unsigned>(rec->ExceptionCode),
                 reinterpret_cast<uintptr_t>(rec->ExceptionAddress));
  }
  HANDLE process = GetCurrentProcess();
  HANDLE thread = GetCurrentThread();
  SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
  SymInitialize(process, nullptr, TRUE);

  CONTEXT ctx = *info->ContextRecord;
  if (ctx.Rip == 0 && ctx.Rsp != 0) {
    // a call through a null function pointer: the return address is on top of
    // the stack
    ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
    ctx.Rsp += sizeof(DWORD64);
    VS_LOG_ERROR("crash",
                 "  (called a null function pointer; stack from the caller)");
  }
  STACKFRAME64 frame{};
  frame.AddrPC.Offset = ctx.Rip;
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrFrame.Offset = ctx.Rbp;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrStack.Offset = ctx.Rsp;
  frame.AddrStack.Mode = AddrModeFlat;

  for (int depth = 0; depth < kCrashFrameLimit; ++depth) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx,
                     nullptr, SymFunctionTableAccess64, SymGetModuleBase64,
                     nullptr) ||
        frame.AddrPC.Offset == 0) {
      break;
    }
    LogStackFrame(process, frame.AddrPC.Offset, Core::LogLevel::Error, "crash",
                  depth);
  }
  SymCleanup(process);
  return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

void InstallCrashHandler() {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  SetUnhandledExceptionFilter(&OnCrash);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  std::signal(SIGABRT, [](int) {
    LogCallStack("runtime abort");
    std::_Exit(kTerminationExitCode);
  });
  std::set_terminate([] {
    if (const auto error = std::current_exception()) {
      try {
        std::rethrow_exception(error);
      } catch (const std::exception& exception) {
        VS_LOG_ERROR("crash", "uncaught exception: {}", exception.what());
      } catch (...) {
        VS_LOG_ERROR("crash", "uncaught nonstandard exception");
      }
    }
    LogCallStack("runtime termination");
    std::_Exit(kTerminationExitCode);
  });
}

void LogCallStack(const char* why) {
  void* frames[kCapturedFrameLimit];
  const USHORT count =
      CaptureStackBackTrace(1, kCapturedFrameLimit, frames, nullptr);
  HANDLE process = GetCurrentProcess();
  static bool initialized = false;
  if (!initialized) {
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);
    initialized = true;
  }
  VS_LOG_WARN("stack", "{}", why);
  for (USHORT index = 0; index < count && index < kLoggedFrameLimit; ++index) {
    LogStackFrame(process, reinterpret_cast<DWORD64>(frames[index]),
                  Core::LogLevel::Warn, "stack", index);
  }
}

void LogAddress(const void* address) {
  HANDLE process = GetCurrentProcess();
  SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
  SymInitialize(process, nullptr, TRUE);
  alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + kSymbolNameCapacity]{};
  auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = kSymbolNameCapacity - 1;
  DWORD64 displacement = 0;
  if (SymFromAddr(process, reinterpret_cast<DWORD64>(address), &displacement,
                  symbol)) {
    VS_LOG_ERROR("render", "address {} is {} +0x{:X}", address, symbol->Name,
                 displacement);
  }
}

}  // namespace VS::Platform

namespace {

constexpr DWORD kWatchRequest = 0xE0564157;  // private exception code: "set the
                                             // watchpoint from this context"
const void* g_watch_address = nullptr;
int g_watch_bytes = 0;
int g_watch_hits = 0;

void LogContextStack(CONTEXT ctx) {
  HANDLE process = GetCurrentProcess();
  HANDLE thread = GetCurrentThread();
  STACKFRAME64 frame{};
  frame.AddrPC.Offset = ctx.Rip;
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrFrame.Offset = ctx.Rbp;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrStack.Offset = ctx.Rsp;
  frame.AddrStack.Mode = AddrModeFlat;
  alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + kSymbolNameCapacity];
  for (int depth = 0; depth < kWatchFrameLimit; ++depth) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx,
                     nullptr, SymFunctionTableAccess64, SymGetModuleBase64,
                     nullptr) ||
        frame.AddrPC.Offset == 0) {
      break;
    }
    auto* sym = reinterpret_cast<SYMBOL_INFO*>(buffer);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = kSymbolNameCapacity - 1;
    DWORD64 disp = 0;
    const char* name =
        SymFromAddr(process, frame.AddrPC.Offset, &disp, sym) ? sym->Name : "?";
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(line);
    DWORD line_disp = 0;
    if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &line_disp, &line)) {
      VS_LOG_WARN("watch", "  #{} {} ({}:{})", depth, name, line.FileName,
                  line.LineNumber);
    } else {
      VS_LOG_WARN("watch", "  #{} {}", depth, name);
    }
  }
}

LONG WINAPI OnWatch(EXCEPTION_POINTERS* info) {
  CONTEXT* ctx = info->ContextRecord;
  if (info->ExceptionRecord->ExceptionCode == kWatchRequest) {
    constexpr DWORD64 kDwordLengthCode = 3;
    const DWORD64 len = g_watch_bytes == sizeof(std::uint64_t) ? 2
                        : g_watch_bytes == sizeof(std::uint32_t)
                            ? kDwordLengthCode
                        : g_watch_bytes == sizeof(std::uint16_t) ? 1
                                                                 : 0;
    ctx->Dr0 = reinterpret_cast<DWORD64>(g_watch_address);
    // DR7: local enable 0, condition 01 (write), length
    ctx->Dr7 = g_watch_address ? (1ull | (1ull << kDebugConditionShift) |
                                  (len << kDebugLengthShift))
                               : 0;
    ctx->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  if (info->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP &&
      (ctx->Dr6 & 1)) {
    ctx->Dr6 = 0;
    if (g_watch_hits++ < kWatchHitLimit) {
      std::uint64_t value = 0;
      std::memcpy(&value, g_watch_address, static_cast<size_t>(g_watch_bytes));
      VS_LOG_WARN("watch", "write to {} (now 0x{:X}), hit {}", g_watch_address,
                  value, g_watch_hits);
      LogContextStack(*ctx);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

namespace VS::Platform {

void WatchWrites(const void* address, int bytes) {
  static PVOID handler = nullptr;
  if (handler == nullptr) {
    handler = AddVectoredExceptionHandler(1, &OnWatch);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
  }
  g_watch_address = address;
  g_watch_bytes = bytes;
  g_watch_hits = 0;
  RaiseException(kWatchRequest, 0, 0, nullptr);
  VS_LOG_WARN("watch", "watching {} bytes at {}", bytes, address);
}

}  // namespace VS::Platform

extern "C" void Platform_WatchWrites(const void* address, int bytes) {
  VS::Platform::WatchWrites(address, bytes);
}

extern "C" void Platform_LogCallStack(const char* why) {
  VS::Platform::LogCallStack(why);
}
