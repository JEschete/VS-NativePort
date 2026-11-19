#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Effects/Timeline.hpp"

// The effect timeline Live bindings' refusal report and current-key view
// (TimelineTracksLive.cpp, TimelinePlaybackLive.cpp).
namespace VS::Effects {
// Reports a refused timeline call; the first refusal and each power of two
// after it are logged.
inline void RefuseTimelineCall(std::string_view call,
                               std::string_view missing) {
  static std::uint64_t refusals = 0;
  if (std::has_single_bit(++refusals)) {
    VS::Core::LogWarn("effects", "timeline {} refused: {} ({} refusals)", call,
                      missing, refusals);
  }
}

// The `units_per_key` units of the track's current key, or empty (refused)
// without a track, keys or a current key.
template <typename Unit>
[[nodiscard]] std::span<const Unit> TimelineKeyUnits(
    const LiveTimelineTrack* track, std::size_t units_per_key,
    std::string_view call) {
  if (track == nullptr || track->keys == nullptr) {
    RefuseTimelineCall(call, "no track keys");
    return {};
  }
  const int index = track->key_index;
  if (index < 0 || index >= track->header.key_count) {
    RefuseTimelineCall(call, "no current key");
    return {};
  }
  const auto* units = reinterpret_cast<const Unit*>(track->keys);
  return {units + (static_cast<std::size_t>(index) * units_per_key),
          units_per_key};
}

// The action request's first `count` entries, at most all of them; more are
// refused.
template <typename Entry, std::size_t kSize>
[[nodiscard]] std::span<const Entry> FirstActionEntries(
    const std::array<Entry, kSize>& entries, std::size_t count,
    std::string_view call) {
  if (count > kSize) {
    RefuseTimelineCall(call, "action entries past the request's sixteen");
    count = kSize;
  }
  return std::span(entries).first(count);
}
}  // namespace VS::Effects
