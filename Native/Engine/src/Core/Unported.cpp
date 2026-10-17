#include "Core/Unported.hpp"

#include "Core/Log.hpp"

#include <algorithm>
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
