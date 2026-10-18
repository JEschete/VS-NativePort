#include "Core/Unported.hpp"

#include "Core/Log.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <mutex>

namespace VS::Core {

namespace {
std::mutex g_mutex;
std::map<std::string, u64, std::less<>> g_hits;
} // namespace

void RecordUnported(std::string_view site)
{
    std::lock_guard lock(g_mutex);
    auto it = g_hits.find(site);
    if (it == g_hits.end()) {
        g_hits.emplace(std::string(site), 1);
        VS_LOG_WARN("unported", "{}", site);
    } else {
        ++it->second;
    }
}

std::vector<UnportedHit> UnportedSnapshot()
{
    std::lock_guard lock(g_mutex);
    std::vector<UnportedHit> out;
    for (const auto& [site, count] : g_hits) {
        out.push_back({site, count});
    }
    std::ranges::sort(out, std::greater{}, &UnportedHit::count);
    return out;
}

void DumpUnportedToLog()
{
    for (const auto& h : UnportedSnapshot()) {
        VS_LOG_INFO("unported", "{:>8}  {}", h.count, h.site);
    }
}

} // namespace VS::Core

extern "C" void Core_RecordUnported(const char* site)
{
    VS::Core::RecordUnported(site);
}

extern "C" void Core_Tracef(const char* format, ...)
{
    if (!VS::Core::IsLogEnabled(VS::Core::LogLevel::Trace, "trace")) {
        return;
    }
    char line[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    VS_LOG_TRACE("trace", "{}", line);
}

extern "C" void Core_Debugf(const char* channel, const char* format, ...)
{
    if (!VS::Core::IsLogEnabled(VS::Core::LogLevel::Debug, channel)) {
        return;
    }
    char line[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    VS_LOG_DEBUG(channel, "{}", line);
}
