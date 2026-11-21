// Hand-maintained (tools/port/owned.csv).
#include "Effects/PrimaryEffect.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Effects {
namespace {
constexpr std::array kPrimaryEffectPhases{
    PrimaryEffectPhase::kIdle,
    PrimaryEffectPhase::kLoadingTextures,
    PrimaryEffectPhase::kLoadingPackage,
    PrimaryEffectPhase::kPlayingTracks,
    PrimaryEffectPhase::kAwaitingTrack,
    PrimaryEffectPhase::kLoadingPurgeOverlay,
    PrimaryEffectPhase::kLoadingPlugins,
    PrimaryEffectPhase::kReloadingMenu,
};
// Token bit 0 is reserved, so token 0 can mean none.
constexpr std::uint32_t kReservedTokenBit = 1;
constexpr unsigned kTokenWordShift = 5;
constexpr unsigned kTokenBitMask = kEffectTokenBits - 1;
static_assert(kEffectTokenBits == 1 << kTokenWordShift);

[[nodiscard]] constexpr bool PlaysTracks(PrimaryEffectPhase phase) {
  return phase == PrimaryEffectPhase::kPlayingTracks ||
         phase == PrimaryEffectPhase::kAwaitingTrack;
}

[[nodiscard]] constexpr std::uint32_t Bit(unsigned bit) {
  return std::uint32_t{1} << bit;
}
}  // namespace

std::optional<PrimaryEffectPhase> ToPrimaryEffectPhase(std::uint32_t value) {
  const auto phase = static_cast<PrimaryEffectPhase>(value);
  if (std::ranges::find(kPrimaryEffectPhases, phase) ==
      kPrimaryEffectPhases.end()) {
    return std::nullopt;
  }
  return phase;
}

// decomp: func_800CEEBC (src/BATTLE/BATTLE.PRG/5BF94.c:4938)
bool CanStartTrack(const PrimaryEffectState& state) {
  return PlaysTracks(state.phase) && state.started_tracks < state.track_count &&
         state.pending_tracks != 0;
}

// decomp: func_800CEF0C (src/BATTLE/BATTLE.PRG/5BF94.c:4947)
std::int32_t PlayableTrackCount(const PrimaryEffectState& state) {
  return PlaysTracks(state.phase) ? state.track_count : 0;
}

// decomp: func_800CEF38 (src/BATTLE/BATTLE.PRG/5BF94.c:4956)
bool RequeueTrack(PrimaryEffectState& state, int track) {
  if (track < 0 || track >= kPrimaryEffectTracks) {
    return false;
  }
  state.started_tracks = Core::Math::WrappingSubtract(state.started_tracks, 1);
  state.pending_tracks |= Bit(static_cast<unsigned>(track));
  return true;
}

// decomp: func_800CEF64 (src/BATTLE/BATTLE.PRG/5BF94.c:4962)
bool IsPrimaryEffectLoaded(const PrimaryEffectState& state) {
  return state.loaded;
}

// decomp: func_800CF478 (src/BATTLE/BATTLE.PRG/5BF94.c:5111)
void SetPrimaryEffectPhase(PrimaryEffectState& state,
                           PrimaryEffectPhase phase) {
  state.phase = phase;
}

// decomp: func_800CF48C (src/BATTLE/BATTLE.PRG/5BF94.c:5115)
PrimaryEffectPhase PrimaryEffectPhaseOf(const PrimaryEffectState& state) {
  return state.phase;
}

// decomp: func_800CF49C (src/BATTLE/BATTLE.PRG/5BF94.c:5117). The original
// also skipped a word whose bits 1-31 were all taken without scanning it.
std::optional<std::uint8_t> AllocateEffectToken(EffectTokenPool& pool) {
  for (std::size_t word = 0; word < pool.size(); ++word) {
    const auto bit =
        static_cast<unsigned>(std::countr_one(pool[word] | kReservedTokenBit));
    if (bit < static_cast<unsigned>(kEffectTokenBits)) {
      pool[word] |= Bit(bit);
      return static_cast<std::uint8_t>((word << kTokenWordShift) | bit);
    }
  }
  return std::nullopt;
}

// decomp: func_800CF514 (src/BATTLE/BATTLE.PRG/5BF94.c:5147)
bool ReleaseEffectToken(EffectTokenPool& pool, int token) {
  if (token < 0 || token >= kEffectTokens) {
    return false;
  }
  const auto index = static_cast<unsigned>(token);
  pool[index >> kTokenWordShift] &= ~Bit(index & kTokenBitMask);
  return true;
}
}  // namespace VS::Effects
