#include "Core/Unported.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Diagnostics.h"
#include "Core/Log.hpp"

namespace VS::Core {

namespace {
constexpr std::size_t kDiagnosticLineBytes = 512;

struct UnportedRegistry {
  std::mutex mutex;
  std::map<std::string, u64, std::less<>> hits;
};

UnportedRegistry& Registry() {
  static UnportedRegistry registry;
  return registry;
}
}  // namespace

void RecordUnported(std::string_view site) {
  auto& registry = Registry();
  std::lock_guard lock(registry.mutex);
  auto it = registry.hits.find(site);
  if (it == registry.hits.end()) {
    registry.hits.emplace(std::string(site), 1);
    VS_LOG_WARN("unported", "{}", site);
  } else {
    ++it->second;
  }
}

std::vector<UnportedHit> UnportedSnapshot() {
  auto& registry = Registry();
  std::lock_guard lock(registry.mutex);
  std::vector<UnportedHit> snapshot;
  snapshot.reserve(registry.hits.size());
  for (const auto& [site, count] : registry.hits) {
    snapshot.push_back({site, count});
  }
  std::ranges::sort(snapshot, std::greater{}, &UnportedHit::count);
  return snapshot;
}

void DumpUnportedToLog() {
  for (const auto& hit : UnportedSnapshot()) {
    VS_LOG_INFO("unported", "{:>8}  {}", hit.count, hit.site);
  }
}

}  // namespace VS::Core

extern "C" void Core_RecordUnported(const char* site) {
  VS::Core::RecordUnported(site);
}

extern "C" void Core_Tracef(const char* format, ...) {
  if (!VS::Core::IsLogEnabled(VS::Core::LogLevel::Trace, "trace")) {
    return;
  }
  char line[VS::Core::kDiagnosticLineBytes]{};
  va_list args;
  va_start(args, format);
  std::vsnprintf(line, sizeof line, format, args);
  va_end(args);
  VS_LOG_TRACE("trace", "{}", line);
}

namespace {
void WriteFormatted(VS::Core::LogLevel level, const char* channel,
                    const char* format, va_list args) {
  if (!VS::Core::IsLogEnabled(level, channel)) {
    return;
  }
  char line[VS::Core::kDiagnosticLineBytes]{};
  std::vsnprintf(line, sizeof line, format, args);
  VS::Core::LogWrite(level, channel, line);
}
}  // namespace

extern "C" void Core_Debugf(const char* channel, const char* format, ...) {
  va_list args;
  va_start(args, format);
  WriteFormatted(VS::Core::LogLevel::Debug, channel, format, args);
  va_end(args);
}

extern "C" void Core_Warnf(const char* channel, const char* format, ...) {
  va_list args;
  va_start(args, format);
  WriteFormatted(VS::Core::LogLevel::Warn, channel, format, args);
  va_end(args);
}

extern "C" void Core_Errorf(const char* channel, const char* format, ...) {
  va_list args;
  va_start(args, format);
  WriteFormatted(VS::Core::LogLevel::Error, channel, format, args);
  va_end(args);
}
