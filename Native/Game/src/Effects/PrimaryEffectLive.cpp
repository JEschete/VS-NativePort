// Hand-maintained (tools/port/owned.csv).
#include "Effects/PrimaryEffectLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

#include "Core/Log.hpp"
#include "Effects/PrimaryEffect.hpp"
#include "Effects/Runtime.h"

static_assert(std::extent_v<decltype(Effects_tokenBitmaps)> >=
              std::size_t{VS::Effects::kEffectTokenWords});

namespace VS::Effects {
namespace {
std::uint64_t refused_tracks = 0;
std::uint64_t refused_phases = 0;
std::uint64_t refused_tokens = 0;
std::uint64_t exhausted_pools = 0;

[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

[[nodiscard]] PrimaryEffectState LiveState() {
  return LivePrimaryEffectState();
}

[[nodiscard]] std::span<int, kEffectTokenWords> LiveTokenWords() {
  return std::span(Effects_tokenBitmaps).first<kEffectTokenWords>();
}

[[nodiscard]] EffectTokenPool LiveTokenPool() {
  EffectTokenPool pool{};
  std::ranges::transform(LiveTokenWords(), pool.begin(), [](int word) {
    return static_cast<std::uint32_t>(word);
  });
  return pool;
}

void CommitTokenPool(const EffectTokenPool& pool) {
  std::ranges::transform(
      pool, LiveTokenWords().begin(),
      [](std::uint32_t word) { return static_cast<int>(word); });
}
}  // namespace

PrimaryEffectState LivePrimaryEffectState() {
  return {.phase = static_cast<PrimaryEffectPhase>(Effects_primaryPhase),
          .track_count = Effects_trackCount,
          .started_tracks = Effects_startedTrackCount,
          .pending_tracks = Effects_pendingTracks,
          .loaded = Effects_primaryLoaded != 0};
}

void StoreLivePrimaryEffectState(const PrimaryEffectState& state) {
  Effects_primaryPhase = std::to_underlying(state.phase);
  Effects_trackCount = state.track_count;
  Effects_startedTrackCount = state.started_tracks;
  Effects_pendingTracks = state.pending_tracks;
  Effects_primaryLoaded = state.loaded ? char{1} : char{0};
}

int LiveCanStartEffectTrack() { return CanStartTrack(LiveState()) ? 1 : 0; }

int LivePrimaryEffectTrackCount() { return PlayableTrackCount(LiveState()); }

void LiveRequeueEffectTrack(int track) {
  PrimaryEffectState state = LiveState();
  if (!RequeueTrack(state, track)) {
    if (ShouldReport(refused_tracks)) {
      VS::Core::LogWarn(
          "effects",
          "primary effect: requeue of track {} refused: past the {} "
          "tracks ({} refusals)",
          track, kPrimaryEffectTracks, refused_tracks);
    }
    return;
  }
  Effects_startedTrackCount = state.started_tracks;
  Effects_pendingTracks = state.pending_tracks;
}

int LiveIsPrimaryEffectLoaded() {
  return IsPrimaryEffectLoaded(LiveState()) ? 1 : 0;
}

void LiveSetPrimaryEffectPhase(int phase) {
  const auto known = ToPrimaryEffectPhase(static_cast<std::uint32_t>(phase));
  if (!known) {
    if (ShouldReport(refused_phases)) {
      VS::Core::LogWarn(
          "effects",
          "primary effect: phase {} refused: not a runtime phase ({} "
          "refusals)",
          phase, refused_phases);
    }
    return;
  }
  PrimaryEffectState state = LiveState();
  SetPrimaryEffectPhase(state, *known);
  Effects_primaryPhase = std::to_underlying(state.phase);
}

int LivePrimaryEffectPhase() {
  return static_cast<int>(
      std::to_underlying(PrimaryEffectPhaseOf(LiveState())));
}

int LiveAllocateEffectToken() {
  EffectTokenPool pool = LiveTokenPool();
  const auto token = AllocateEffectToken(pool);
  if (!token) {
    if (ShouldReport(exhausted_pools)) {
      VS::Core::LogWarn(
          "effects",
          "effect tokens: the pool is full; token 0 handed out ({} "
          "times)",
          exhausted_pools);
    }
    return 0;
  }
  CommitTokenPool(pool);
  return *token;
}

void LiveReleaseEffectToken(int token) {
  EffectTokenPool pool = LiveTokenPool();
  if (!ReleaseEffectToken(pool, token)) {
    if (ShouldReport(refused_tokens)) {
      VS::Core::LogWarn(
          "effects",
          "effect tokens: release of token {} refused: past the {} "
          "tokens ({} refusals)",
          token, kEffectTokens, refused_tokens);
    }
    return;
  }
  CommitTokenPool(pool);
}
}  // namespace VS::Effects
