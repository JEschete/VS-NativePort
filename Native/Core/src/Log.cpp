#include "vs/core/Log.hpp"

#include <atomic>
#include <cstdio>
#include <mutex>

namespace VS::Core {

namespace {
std::atomic<LogLevel> g_level{LogLevel::Info};
std::mutex g_mutex;
std::FILE* g_file = nullptr;

const char* LevelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace: return "trace";
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warn: return "warn";
    case LogLevel::Error: return "error";
    }
    return "?";
}
} // namespace

void SetLogLevel(LogLevel level) { g_level = level; }

bool SetLogFile(const char* path)
{
    std::lock_guard lock(g_mutex);
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
    if (path && *path) {
        g_file = std::fopen(path, "w");
    }
    return g_file != nullptr;
}

void LogWrite(LogLevel level, std::string_view channel, std::string_view message)
{
    if (level < g_level.load()) {
        return;
    }
    std::lock_guard lock(g_mutex);
    std::FILE* out = level >= LogLevel::Warn ? stderr : stdout;
    std::fprintf(out, "[%s] %.*s: %.*s\n", LevelName(level), static_cast<int>(channel.size()), channel.data(),
                 static_cast<int>(message.size()), message.data());
    std::fflush(out);
    if (g_file) {
        std::fprintf(g_file, "[%s] %.*s: %.*s\n", LevelName(level), static_cast<int>(channel.size()),
                     channel.data(), static_cast<int>(message.size()), message.data());
        std::fflush(g_file);
    }
}

} // namespace VS::Core
