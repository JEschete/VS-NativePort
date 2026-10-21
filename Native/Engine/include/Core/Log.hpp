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
// Console filtering does not discard file/observer records. Both setters return
// the previous policy; normal hosts retain per-message flushing by default.
LogLevel SetConsoleLogLevel(LogLevel level);
bool SetLogFlushEachMessage(bool enabled);
bool FlushLogFile();
void LogWrite(LogLevel level, std::string_view channel,
              std::string_view message);

template <class... Args>
void Log(LogLevel level, std::string_view channel,
         std::format_string<Args...> fmt, Args&&... args) {
  if (IsLogEnabled(level, channel)) {
    LogWrite(level, channel, std::format(fmt, std::forward<Args>(args)...));
  }
}

}  // namespace VS::Core

#define VS_LOG_INFO(ch, ...) \
  ::VS::Core::Log(::VS::Core::LogLevel::Info, ch, __VA_ARGS__)
#define VS_LOG_WARN(ch, ...) \
  ::VS::Core::Log(::VS::Core::LogLevel::Warn, ch, __VA_ARGS__)
#define VS_LOG_ERROR(ch, ...) \
  ::VS::Core::Log(::VS::Core::LogLevel::Error, ch, __VA_ARGS__)
#define VS_LOG_DEBUG(ch, ...) \
  ::VS::Core::Log(::VS::Core::LogLevel::Debug, ch, __VA_ARGS__)
#define VS_LOG_TRACE(ch, ...) \
  ::VS::Core::Log(::VS::Core::LogLevel::Trace, ch, __VA_ARGS__)
