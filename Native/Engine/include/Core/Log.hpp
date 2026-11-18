#pragma once

#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

namespace VS::Core {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Off };

struct LogRecord {
  LogLevel level;
  std::uint64_t tick;
  std::string_view channel;
  std::string_view message;
};
using LogObserver = std::function<void(const LogRecord&)>;
// Synchronous observation outside the log mutex. Views live for the callback;
// observers must not throw or log recursively. Returns the previous observer.
LogObserver SetLogObserver(LogObserver observer);

void SetLogLevel(LogLevel level);
std::optional<LogLevel> ParseLogLevel(std::string_view name);
// Empty or "*" selects all channels; warnings/errors bypass this filter.
// Invalid lists leave it unchanged.
bool SetLogChannels(std::string_view channels);
bool IsLogEnabled(LogLevel level, std::string_view channel);
// Zero denotes startup; the host updates this before each logical game tick.
void SetLogTick(std::uint64_t tick);
// Additionally mirrors every line to `path` (truncated on open). Pass an empty
// path to close.
bool SetLogFile(const char* path);
// Optional second text sink for hosts retaining an existing diagnostic path.
// It receives exactly the same records and flushes as the primary file.
bool SetLogMirrorFile(const char* path);
// Console filtering does not discard file/observer records. Both setters return
// the previous policy; normal hosts retain per-message flushing by default.
LogLevel SetConsoleLogLevel(LogLevel level);
bool SetLogFlushEachMessage(bool enabled);
bool FlushLogFile();
void LogWrite(LogLevel level, std::string_view channel,
              std::string_view message);

// Formatting is skipped when the level/channel is disabled; the arguments are
// still evaluated.
template <class... Args>
void Log(LogLevel level, std::string_view channel,
         std::format_string<Args...> fmt, Args&&... args) {
  if (IsLogEnabled(level, channel)) {
    LogWrite(level, channel, std::format(fmt, std::forward<Args>(args)...));
  }
}

template <class... Args>
void LogTrace(std::string_view channel, std::format_string<Args...> fmt,
              Args&&... args) {
  Log<Args...>(LogLevel::Trace, channel, fmt, std::forward<Args>(args)...);
}

template <class... Args>
void LogDebug(std::string_view channel, std::format_string<Args...> fmt,
              Args&&... args) {
  Log<Args...>(LogLevel::Debug, channel, fmt, std::forward<Args>(args)...);
}

template <class... Args>
void LogInfo(std::string_view channel, std::format_string<Args...> fmt,
             Args&&... args) {
  Log<Args...>(LogLevel::Info, channel, fmt, std::forward<Args>(args)...);
}

template <class... Args>
void LogWarn(std::string_view channel, std::format_string<Args...> fmt,
             Args&&... args) {
  Log<Args...>(LogLevel::Warn, channel, fmt, std::forward<Args>(args)...);
}

template <class... Args>
void LogError(std::string_view channel, std::format_string<Args...> fmt,
              Args&&... args) {
  Log<Args...>(LogLevel::Error, channel, fmt, std::forward<Args>(args)...);
}

}  // namespace VS::Core
