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
std::atomic<LogLevel> minimumLevel{LogLevel::Info};
std::atomic<std::uint64_t> logTick{0};
std::mutex logMutex;
std::FILE* logFile = nullptr;
std::FILE* mirrorFile = nullptr;
std::vector<std::string> enabledChannels;
LogObserver logObserver;
LogLevel consoleLevel = LogLevel::Trace;
bool flushEachMessage = true;

bool ChannelEnabled(LogLevel level, std::string_view channel) {
  return level >= LogLevel::Warn || enabledChannels.empty() ||
         std::ranges::find(enabledChannels, channel) != enabledChannels.end();
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
  std::lock_guard lock(logMutex);
  return std::exchange(logObserver, std::move(observer));
}

void SetLogLevel(LogLevel level) { minimumLevel = level; }

LogLevel SetConsoleLogLevel(LogLevel level) {
  std::lock_guard lock(logMutex);
  return std::exchange(consoleLevel, level);
}

bool SetLogFlushEachMessage(bool enabled) {
  std::lock_guard lock(logMutex);
  return std::exchange(flushEachMessage, enabled);
}

bool FlushLogFile() {
  std::lock_guard lock(logMutex);
  const bool primary = logFile == nullptr ||
                       (std::fflush(logFile) == 0 && std::ferror(logFile) == 0);
  const bool mirror = mirrorFile == nullptr || (std::fflush(mirrorFile) == 0 &&
                                                std::ferror(mirrorFile) == 0);
  return primary && mirror;
}

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
  std::lock_guard lock(logMutex);
  enabledChannels = std::move(parsed);
  return true;
}

bool IsLogEnabled(LogLevel level, std::string_view channel) {
  if (level == LogLevel::Off || level < minimumLevel.load()) {
    return false;
  }
  std::lock_guard lock(logMutex);
  return ChannelEnabled(level, channel);
}

void SetLogTick(std::uint64_t tick) { logTick = tick; }

bool SetLogFile(const char* path) {
  std::lock_guard lock(logMutex);
  if (logFile) {
    std::fclose(logFile);
    logFile = nullptr;
  }
  if (path && *path) {
    logFile = std::fopen(path, "w");
  }
  return !path || !*path || logFile != nullptr;
}

bool SetLogMirrorFile(const char* path) {
  std::lock_guard lock(logMutex);
  if (mirrorFile) {
    std::fclose(mirrorFile);
    mirrorFile = nullptr;
  }
  if (path && *path) mirrorFile = std::fopen(path, "w");
  return !path || !*path || mirrorFile != nullptr;
}

void LogWrite(LogLevel level, std::string_view channel,
              std::string_view message) {
  if (level == LogLevel::Off || level < minimumLevel.load()) {
    return;
  }
  std::unique_lock lock(logMutex);
  if (!ChannelEnabled(level, channel)) {
    return;
  }
  const std::uint64_t tick = logTick.load();
  if (level >= consoleLevel) {
    std::FILE* out = level >= LogLevel::Warn ? stderr : stdout;
    std::fprintf(out, "[%s] [tick=%" PRIu64 "] %.*s: %.*s\n", LevelName(level),
                 tick, static_cast<int>(channel.size()), channel.data(),
                 static_cast<int>(message.size()), message.data());
    std::fflush(out);
  }
  for (auto* file : {logFile, mirrorFile}) {
    if (!file) continue;
    std::fprintf(file, "[%s] [tick=%" PRIu64 "] %.*s: %.*s\n", LevelName(level),
                 tick, static_cast<int>(channel.size()), channel.data(),
                 static_cast<int>(message.size()), message.data());
    if (flushEachMessage || level >= LogLevel::Warn) {
      std::fflush(file);
    }
  }
  auto observer = logObserver;
  lock.unlock();
  if (observer) observer({level, tick, channel, message});
}

}  // namespace VS::Core
