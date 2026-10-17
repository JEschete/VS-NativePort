#include "Platform/CrashHandler.hpp"

#include "Core/Log.hpp"

#include <Windows.h>
#include <DbgHelp.h>

#include <cstring>

#pragma comment(lib, "dbghelp.lib")

namespace VS::Platform {

namespace {

LONG WINAPI OnCrash(EXCEPTION_POINTERS* info)
{
    const EXCEPTION_RECORD* rec = info->ExceptionRecord;
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
        VS_LOG_ERROR("crash", "access violation {} 0x{:X} at 0x{:X}",
                     rec->ExceptionInformation[0] == 1 ? "writing" : "reading", rec->ExceptionInformation[1],
                     reinterpret_cast<uintptr_t>(rec->ExceptionAddress));
    } else {
        VS_LOG_ERROR("crash", "exception 0x{:08X} at 0x{:X}", static_cast<unsigned>(rec->ExceptionCode),
                     reinterpret_cast<uintptr_t>(rec->ExceptionAddress));
    }
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    CONTEXT ctx = *info->ContextRecord;
    if (ctx.Rip == 0 && ctx.Rsp != 0) {
        // a call through a null function pointer: the return address is on top of the stack
        ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
        ctx.Rsp += 8;
        VS_LOG_ERROR("crash", "  (called a null function pointer; stack from the caller)");
    }
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256];
    for (int depth = 0; depth < 48; ++depth) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buffer);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        const char* name = SymFromAddr(process, frame.AddrPC.Offset, &disp, sym) ? sym->Name : "?";
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line)) {
            VS_LOG_ERROR("crash", "  #{} {} ({}:{})", depth, name, line.FileName, line.LineNumber);
        } else {
            VS_LOG_ERROR("crash", "  #{} {} +0x{:X}", depth, name, disp);
        }
    }
    SymCleanup(process);
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

void InstallCrashHandler()
{
    SetUnhandledExceptionFilter(&OnCrash);
}

void LogCallStack(const char* why)
{
    void* frames[32];
    const USHORT n = CaptureStackBackTrace(1, 32, frames, nullptr);
    HANDLE process = GetCurrentProcess();
    static bool initialized = false;
    if (!initialized) {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(process, nullptr, TRUE);
        initialized = true;
    }
    VS_LOG_WARN("stack", "{}", why);
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256];
    for (USHORT i = 0; i < n && i < 16; ++i) {
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buffer);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        const DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);
        const char* name = SymFromAddr(process, addr, &disp, sym) ? sym->Name : "?";
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        if (SymGetLineFromAddr64(process, addr, &lineDisp, &line)) {
            VS_LOG_WARN("stack", "  #{} {} ({}:{})", i, name, line.FileName, line.LineNumber);
        } else {
            VS_LOG_WARN("stack", "  #{} {}", i, name);
        }
    }
}

} // namespace VS::Platform

namespace {

constexpr DWORD WatchRequest = 0xE0564157;   // private exception code: "set the watchpoint from this context"
const void* g_watchAddress = nullptr;
int g_watchBytes = 0;
int g_watchHits = 0;

void LogContextStack(CONTEXT ctx)
{
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256];
    for (int depth = 0; depth < 10; ++depth) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr) || frame.AddrPC.Offset == 0) {
            break;
        }
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buffer);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        const char* name = SymFromAddr(process, frame.AddrPC.Offset, &disp, sym) ? sym->Name : "?";
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line)) {
            VS_LOG_WARN("watch", "  #{} {} ({}:{})", depth, name, line.FileName, line.LineNumber);
        } else {
            VS_LOG_WARN("watch", "  #{} {}", depth, name);
        }
    }
}

LONG WINAPI OnWatch(EXCEPTION_POINTERS* info)
{
    CONTEXT* ctx = info->ContextRecord;
    if (info->ExceptionRecord->ExceptionCode == WatchRequest) {
        const DWORD64 len = g_watchBytes == 8 ? 2 : g_watchBytes == 4 ? 3 : g_watchBytes == 2 ? 1 : 0;
        ctx->Dr0 = reinterpret_cast<DWORD64>(g_watchAddress);
        // DR7: local enable 0, condition 01 (write), length
        ctx->Dr7 = g_watchAddress ? (1ull | (1ull << 16) | (len << 18)) : 0;
        ctx->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP && (ctx->Dr6 & 1)) {
        ctx->Dr6 = 0;
        if (g_watchHits++ < 32) {
            unsigned long long value = 0;
            std::memcpy(&value, g_watchAddress, static_cast<size_t>(g_watchBytes));
            VS_LOG_WARN("watch", "write to {} (now 0x{:X}), hit {}", g_watchAddress, value, g_watchHits);
            LogContextStack(*ctx);
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

namespace VS::Platform {

void WatchWrites(const void* address, int bytes)
{
    static PVOID handler = nullptr;
    if (handler == nullptr) {
        handler = AddVectoredExceptionHandler(1, &OnWatch);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    }
    g_watchAddress = address;
    g_watchBytes = bytes;
    g_watchHits = 0;
    RaiseException(WatchRequest, 0, 0, nullptr);
    VS_LOG_WARN("watch", "watching {} bytes at {}", bytes, address);
}

} // namespace VS::Platform

extern "C" void Platform_WatchWrites(const void* address, int bytes)
{
    VS::Platform::WatchWrites(address, bytes);
}

extern "C" void Platform_LogCallStack(const char* why)
{
    VS::Platform::LogCallStack(why);
}
