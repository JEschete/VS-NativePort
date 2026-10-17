#pragma once

namespace VS::Platform {

// Logs the exception and a symbolized stack (function, file, line) when the process crashes, including inside the
// game fiber. Development aid: the log is the first thing to read after a crash.
void InstallCrashHandler();

} // namespace VS::Platform
