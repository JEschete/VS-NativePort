#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace VS::Core {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

void SetLogLevel(LogLevel level);
// Additionally mirrors every line to `path` (truncated on open). Pass an empty path to close.
bool SetLogFile(const char* path);
void LogWrite(LogLevel level, std::string_view channel, std::string_view message);

template <class... Args>
void Log(LogLevel level, std::string_view channel, std::format_string<Args...> fmt, Args&&... args)
{
    LogWrite(level, channel, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace VS::Core

#define VS_LOG_INFO(ch, ...) ::VS::Core::Log(::VS::Core::LogLevel::Info, ch, __VA_ARGS__)
#define VS_LOG_WARN(ch, ...) ::VS::Core::Log(::VS::Core::LogLevel::Warn, ch, __VA_ARGS__)
#define VS_LOG_ERROR(ch, ...) ::VS::Core::Log(::VS::Core::LogLevel::Error, ch, __VA_ARGS__)
#define VS_LOG_DEBUG(ch, ...) ::VS::Core::Log(::VS::Core::LogLevel::Debug, ch, __VA_ARGS__)
