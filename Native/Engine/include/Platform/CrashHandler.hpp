#pragma once

namespace VS::Platform {

// Logs the exception and a symbolized stack (function, file, line) when the process crashes, including inside the
// game fiber. Development aid: the log is the first thing to read after a crash.
void InstallCrashHandler();

// Logs the current call stack (symbolized) under `why`: for errors that are not crashes, to find the caller.
void LogCallStack(const char* why);

// Development aid: a hardware write watchpoint on `bytes` (1, 2, 4 or 8) at `address` for the calling thread; each
// write logs the writing code's stack. One watchpoint at a time; null clears it.
void WatchWrites(const void* address, int bytes);

} // namespace VS::Platform
