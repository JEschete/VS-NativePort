#include "Platform/CrashHandler.hpp"

#include "Core/Log.hpp"

#include <Windows.h>
#include <DbgHelp.h>

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

} // namespace VS::Platform
