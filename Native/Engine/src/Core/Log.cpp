#include "Core/Log.hpp"

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace VS::Core {

namespace {
std::atomic<LogLevel> g_level{LogLevel::Info};
std::atomic<std::uint64_t> g_tick{0};
std::mutex g_mutex;
std::FILE* g_file = nullptr;
std::vector<std::string> g_channels;
LogObserver g_observer;

bool ChannelEnabled(LogLevel level, std::string_view channel) {
  return level >= LogLevel::Warn || g_channels.empty() ||
         std::ranges::find(g_channels, channel) != g_channels.end();
}

const char* LevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Trace:
      return "trace";
    case LogLevel::Debug:
      return "debug";
    case LogLevel::Info:
      return "info";
    case LogLevel::Warn:
      return "warn";
    case LogLevel::Error:
      return "error";
    case LogLevel::Off:
      return "off";
  }
  return "?";
}
}  // namespace

LogObserver SetLogObserver(LogObserver observer) {
  std::lock_guard lock(g_mutex);
  return std::exchange(g_observer, std::move(observer));
}

void SetLogLevel(LogLevel level) { g_level = level; }

std::optional<LogLevel> ParseLogLevel(std::string_view name) {
  for (auto level : {LogLevel::Trace, LogLevel::Debug, LogLevel::Info,
                     LogLevel::Warn, LogLevel::Error, LogLevel::Off}) {
    if (name == LevelName(level)) {
      return level;
    }
  }
  return std::nullopt;
}

bool SetLogChannels(std::string_view channels) {
  std::vector<std::string> parsed;
  if (!channels.empty() && channels != "*") {
    for (;;) {
      const auto comma = channels.find(',');
      const auto item = channels.substr(0, comma);
      if (item.empty() ||
          item.find_first_of("* \t\r\n") != std::string_view::npos) {
        return false;
      }
      parsed.emplace_back(item);
      if (comma == std::string_view::npos) {
        break;
      }
      channels.remove_prefix(comma + 1);
    }
  }
  std::lock_guard lock(g_mutex);
  g_channels = std::move(parsed);
  return true;
}

bool IsLogEnabled(LogLevel level, std::string_view channel) {
  if (level == LogLevel::Off || level < g_level.load()) {
    return false;
  }
  std::lock_guard lock(g_mutex);
  return ChannelEnabled(level, channel);
}

void SetLogTick(std::uint64_t tick) { g_tick = tick; }

bool SetLogFile(const char* path) {
  std::lock_guard lock(g_mutex);
  if (g_file) {
    std::fclose(g_file);
    g_file = nullptr;
  }
  if (path && *path) {
    g_file = std::fopen(path, "w");
  }
  return !path || !*path || g_file != nullptr;
}

void LogWrite(LogLevel level, std::string_view channel,
              std::string_view message) {
  if (level == LogLevel::Off || level < g_level.load()) {
    return;
  }
  std::unique_lock lock(g_mutex);
  if (!ChannelEnabled(level, channel)) {
    return;
  }
  const std::uint64_t tick = g_tick.load();
  std::FILE* out = level >= LogLevel::Warn ? stderr : stdout;
  std::fprintf(out, "[%s] [tick=%" PRIu64 "] %.*s: %.*s\n", LevelName(level),
               tick, static_cast<int>(channel.size()), channel.data(),
               static_cast<int>(message.size()), message.data());
  std::fflush(out);
  if (g_file) {
    std::fprintf(g_file, "[%s] [tick=%" PRIu64 "] %.*s: %.*s\n",
                 LevelName(level), tick, static_cast<int>(channel.size()),
                 channel.data(), static_cast<int>(message.size()),
                 message.data());
    std::fflush(g_file);
  }
  auto observer = g_observer;
  lock.unlock();
  if (observer) observer({level, tick, channel, message});
}

}  // namespace VS::Core
