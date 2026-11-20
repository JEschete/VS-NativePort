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
#include "Effects/Runtime.hpp"

static_assert(std::extent_v<decltype(VS::Effects::tokenBitmaps)> >=
              std::size_t{VS::Effects::kEffectTokenWords});

namespace VS::Effects {
namespace {
std::uint64_t refusedTracks = 0;
std::uint64_t refusedPhases = 0;
std::uint64_t refusedTokens = 0;
std::uint64_t exhaustedPools = 0;

[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

[[nodiscard]] PrimaryEffectState LiveState() {
  return LivePrimaryEffectState();
}

[[nodiscard]] std::span<int, kEffectTokenWords> LiveTokenWords() {
  return std::span(tokenBitmaps).first<kEffectTokenWords>();
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
  return {.phase = static_cast<PrimaryEffectPhase>(primaryPhase),
          .track_count = trackCount,
          .started_tracks = startedTrackCount,
          .pending_tracks = pendingTracks,
          .loaded = primaryLoaded != 0};
}

void StoreLivePrimaryEffectState(const PrimaryEffectState& state) {
  primaryPhase = std::to_underlying(state.phase);
  trackCount = state.track_count;
  startedTrackCount = state.started_tracks;
  pendingTracks = state.pending_tracks;
  primaryLoaded = state.loaded ? char{1} : char{0};
}

int LiveCanStartEffectTrack() { return CanStartTrack(LiveState()) ? 1 : 0; }

int LivePrimaryEffectTrackCount() { return PlayableTrackCount(LiveState()); }

void LiveRequeueEffectTrack(int track) {
  PrimaryEffectState state = LiveState();
  if (!RequeueTrack(state, track)) {
    if (ShouldReport(refusedTracks)) {
      VS::Core::LogWarn(
          "effects",
          "primary effect: requeue of track {} refused: past the {} "
          "tracks ({} refusals)",
          track, kPrimaryEffectTracks, refusedTracks);
    }
    return;
  }
  startedTrackCount = state.started_tracks;
  pendingTracks = state.pending_tracks;
}

int LiveIsPrimaryEffectLoaded() {
  return IsPrimaryEffectLoaded(LiveState()) ? 1 : 0;
}

void LiveSetPrimaryEffectPhase(int phase) {
  const auto known = ToPrimaryEffectPhase(static_cast<std::uint32_t>(phase));
  if (!known) {
    if (ShouldReport(refusedPhases)) {
      VS::Core::LogWarn(
          "effects",
          "primary effect: phase {} refused: not a runtime phase ({} "
          "refusals)",
          phase, refusedPhases);
    }
    return;
  }
  PrimaryEffectState state = LiveState();
  SetPrimaryEffectPhase(state, *known);
  primaryPhase = std::to_underlying(state.phase);
}

int LivePrimaryEffectPhase() {
  return static_cast<int>(
      std::to_underlying(PrimaryEffectPhaseOf(LiveState())));
}

int LiveAllocateEffectToken() {
  EffectTokenPool pool = LiveTokenPool();
  const auto token = AllocateEffectToken(pool);
  if (!token) {
    if (ShouldReport(exhaustedPools)) {
      VS::Core::LogWarn(
          "effects",
          "effect tokens: the pool is full; token 0 handed out ({} "
          "times)",
          exhaustedPools);
    }
    return 0;
  }
  CommitTokenPool(pool);
  return *token;
}

void LiveReleaseEffectToken(int token) {
  EffectTokenPool pool = LiveTokenPool();
  if (!ReleaseEffectToken(pool, token)) {
    if (ShouldReport(refusedTokens)) {
      VS::Core::LogWarn(
          "effects",
          "effect tokens: release of token {} refused: past the {} "
          "tokens ({} refusals)",
          token, kEffectTokens, refusedTokens);
    }
    return;
  }
  CommitTokenPool(pool);
}
}  // namespace VS::Effects
