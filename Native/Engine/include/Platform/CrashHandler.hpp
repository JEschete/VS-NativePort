#pragma once

#include <cstdint>
#include <memory>

namespace VS::Platform {

class ProgressWatchdog {
public:
	explicit ProgressWatchdog(bool enabled);
	~ProgressWatchdog();
	void Pulse(std::uint64_t tick);
	ProgressWatchdog(const ProgressWatchdog&) = delete;
	ProgressWatchdog& operator=(const ProgressWatchdog&) = delete;

private:
	struct State;
	std::unique_ptr<State> m_state;
};

// Logs the exception and a symbolized stack (function, file, line) when the process crashes, including inside the
// game fiber. Development aid: the log is the first thing to read after a crash.
void InstallCrashHandler();

// Logs the current call stack (symbolized) under `why`: for errors that are not crashes, to find the caller.
void LogCallStack(const char* why);
void LogAddress(const void* address);

// Development aid: a hardware write watchpoint on `bytes` (1, 2, 4 or 8) at `address` for the calling thread; each
// write logs the writing code's stack. One watchpoint at a time; null clears it.
void WatchWrites(const void* address, int bytes);

} // namespace VS::Platform
