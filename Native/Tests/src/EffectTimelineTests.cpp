#include <array>
#include <cstddef>
#include <cstdint>

#include "Effects/Runtime.h"
#include "Effects/Timeline.hpp"
#include "Effects/TimelinePlaybackLive.hpp"

// 5BF94.c:8442 used PS1 header-sized pointer arithmetic instead of indexing
// native tracks.
int Tests_EffectTimelineTrackStride(void) {
  constexpr std::size_t kTracks = 2;
  alignas(VS::Effects::LiveTimelineTrack)
      std::array<std::byte, VS::Effects::LiveEffectTimeline::Bytes(kTracks)>
          storage{};
  VS::Effects::LiveEffectInstance effect{};
  auto* timeline =
      reinterpret_cast<VS::Effects::LiveEffectTimeline*>(storage.data());
  const std::uint32_t first_key = 10;
  const std::uint32_t second_key = 20;
  effect.context.timeline = timeline;
  timeline->track_count = kTracks;
  timeline->active_tracks = 3;
  const auto tracks = timeline->Tracks();
  tracks[0].keys = &first_key;
  tracks[1].keys = &second_key;
  for (VS::Effects::LiveTimelineTrack& track : tracks) {
    track.header.kind = 2;
    track.header.key_count = 1;
  }
  // Both sound keys lie in the future: no audio side effects, but both native
  // key pointers are read.
  VS::Effects::LiveTickEffectTimeline(&effect, 0);
  return static_cast<int>(
      timeline->frame == 1 && timeline->active_tracks == 3 &&
      tracks[0].keys == &first_key && tracks[1].keys == &second_key &&
      tracks[0].key_index == 0 && tracks[1].key_index == 0);
}
