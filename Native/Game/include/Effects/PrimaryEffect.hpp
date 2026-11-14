#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <optional>

// The primary effect's playback state and the effect token pool of BATTLE.PRG
// (5BF94.c:5373-5397, 5546-5591). The primary effect walks a phase machine
// (Runtime.cpp's update switch); once its package is loaded it starts the
// package's tracks one at a time from a mask of pending tracks. Every effect
// holds a token from a pool of four 32-bit words.
namespace VS::Effects {
// The primary effect's phases; the original stored the bare number.
enum class PrimaryEffectPhase : std::uint32_t {
  kIdle = 0,
  kLoadingTextures = 2,
  kLoadingPackage = 3,
  kPlayingTracks = 4,
  kAwaitingTrack = 5,
  kLoadingPurgeOverlay = 6,
  kLoadingPlugins = 8,
  kReloadingMenu = 9,
};

// The phase numbered `value`; empty for a number the runtime never sets.
[[nodiscard]] std::optional<PrimaryEffectPhase> ToPrimaryEffectPhase(
    std::uint32_t value);

// The pending-track mask holds one bit per track.
inline constexpr int kPrimaryEffectTracks = 32;

// The primary effect's playback state. `phase` may hold a number outside the
// enumeration when it was stored without ToPrimaryEffectPhase; such a phase
// plays no tracks.
struct PrimaryEffectState {
  PrimaryEffectPhase phase = PrimaryEffectPhase::kIdle;
  std::int32_t track_count = 0;
  std::int32_t started_tracks = 0;
  std::uint32_t pending_tracks = 0;
  bool loaded = false;
};

// func_800CEEBC (5BF94.c:5373): whether a track can start: the effect plays
// or awaits a track, fewer tracks started than the package has (a signed
// comparison, as the original's) and a track is pending.
[[nodiscard]] bool CanStartTrack(const PrimaryEffectState& state);

// func_800CEF0C (5BF94.c:5382): the package's track count while the effect
// plays or awaits a track, and 0 otherwise.
[[nodiscard]] std::int32_t PlayableTrackCount(const PrimaryEffectState& state);

// func_800CEF38 (5BF94.c:5391): returns a started track to the pending mask,
// taking one from the started count (which wraps as the original's did).
// Refused, leaving the state, for a track outside the mask; the original
// shifted by the track's low five bits.
[[nodiscard]] bool RequeueTrack(PrimaryEffectState& state, int track);

// func_800CEF64 (5BF94.c:5397).
[[nodiscard]] bool IsPrimaryEffectLoaded(const PrimaryEffectState& state);

// func_800CF478 (5BF94.c:5546).
void SetPrimaryEffectPhase(PrimaryEffectState& state, PrimaryEffectPhase phase);

// func_800CF48C (5BF94.c:5550).
[[nodiscard]] PrimaryEffectPhase PrimaryEffectPhaseOf(
    const PrimaryEffectState& state);

// The effect token pool. Bit 0 of each word is never handed out, so token 0
// means none; token `word * 32 + bit` is bit `bit` of word `word`.
inline constexpr int kEffectTokenWords = 4;
inline constexpr int kEffectTokenBits = 32;
inline constexpr int kEffectTokens = kEffectTokenWords * kEffectTokenBits;
using EffectTokenPool = std::array<std::uint32_t, kEffectTokenWords>;

// func_800CF49C (5BF94.c:5552): marks and returns the lowest free token,
// scanning bits 1-31 of each word in turn; empty when the pool is full.
[[nodiscard]] std::optional<std::uint8_t> AllocateEffectToken(
    EffectTokenPool& pool);

// func_800CF514 (5BF94.c:5582): frees `token`. Refused, leaving the pool, for
// a token outside the pool; the original cleared a bit of whichever word the
// token addressed, before or past the pool.
[[nodiscard]] bool ReleaseEffectToken(EffectTokenPool& pool, int token);
}  // namespace VS::Effects
