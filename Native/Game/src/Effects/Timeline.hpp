#pragma once
// The effect timeline as the live bindings run it: an instance's timeline on
// the game heap with its tracks after it (TimelinePlaybackLive.cpp loads and
// ticks it), and each track's view of its keys in the bound timeline block.
// The typed playback is in Effects/TimelinePlayback.hpp; the key decoders are
// in Effects/TimelinePlayback.hpp, Effects/TimelineTracks.hpp and
// Effects/TimelineCommands.hpp, and Content/PFile.hpp finds the sections.

#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Effects {
// decomp: func_800D5780_t2 (src/BATTLE/BATTLE.PRG/battleEffect.h:303): a
// track entry's header halfword, copied from the timeline section.
struct LiveTimelineTrackHeader {
  // The track's TimelineTrackKind.
  std::uint8_t kind;
  std::uint8_t key_count;
};

// decomp: func_800D5780_t (src/BATTLE/BATTLE.PRG/battleEffect.h:308): one
// running track.
struct LiveTimelineTrack {
  // The track's keys in the bound timeline block, read by the track's kind as
  // words, halfwords or byte records.
  const std::uint32_t* keys;
  LiveTimelineTrackHeader header;
  // The current key; -1 before the first.
  std::int16_t key_index;
  // The frame the curves are sampled at while particles are emitted.
  std::int16_t curve_frame;
  // Frames left in the current key.
  std::int16_t key_timer;
  // The current key's parameter; a child track keeps its delay (low half) and
  // next action target (high half) here.
  std::int32_t key_parameter;
};

// decomp: D_800F53B8_t4 (src/BATTLE/BATTLE.PRG/battleEffect.h:317): an effect
// instance's timeline, allocated from the game heap's high end with its tracks
// after it (LiveLoadEffectTimeline).
struct LiveEffectTimeline {
  std::uint8_t track_count;
  // kTimelineFrameHeld (TimelineCommands.hpp) and kTimelineHasChildTracks
  // (TimelinePlayback.hpp).
  std::uint8_t flags;
  std::int16_t frame;
  // A bit per track still running; zero once the timeline has finished.
  std::uint32_t active_tracks;

  // The bytes a timeline of `track_count` tracks takes on the heap.
  [[nodiscard]] static constexpr std::size_t Bytes(std::size_t track_count) {
    return sizeof(LiveEffectTimeline) +
           (track_count * sizeof(LiveTimelineTrack));
  }
  // The tracks after the header.
  [[nodiscard]] std::span<LiveTimelineTrack> Tracks() {
    return {reinterpret_cast<LiveTimelineTrack*>(this + 1), track_count};
  }
  [[nodiscard]] std::span<const LiveTimelineTrack> Tracks() const {
    return {reinterpret_cast<const LiveTimelineTrack*>(this + 1), track_count};
  }
};

namespace layout {
// The section stores the header as one halfword: the kind in its low byte.
static_assert(sizeof(LiveTimelineTrackHeader) == sizeof(std::uint16_t));
// The tracks follow the header without padding.
static_assert(sizeof(LiveEffectTimeline) % alignof(LiveTimelineTrack) == 0);
}  // namespace layout
}  // namespace VS::Effects
