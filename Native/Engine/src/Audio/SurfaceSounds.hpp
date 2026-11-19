#pragma once
// The sound id each object surface plays when an actor steps or lands on it.
#include <cstddef>
#include <cstdint>

namespace VS::Audio {
inline constexpr std::size_t kObjectSurfaceSoundIdCount = 2956;

// decomp: D_800E91A4 (D_800E91A4@BATTLE/BATTLE.PRG)
extern std::uint8_t objectSurfaceSoundIds[kObjectSurfaceSoundIdCount];
}  // namespace VS::Audio
