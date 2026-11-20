#pragma once
// The terrain probe the actor collision rules leave for the ledge checks, the
// moving-platform contact search depth and the surface id of each collision
// tile kind. Collision.cpp defines them; the module image fills
// tileSurfaceIds at start-up.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Gte.h"

namespace VS::Field {

// The inventory extent of D_800E9278, up to the next known global.
inline constexpr std::size_t kTileSurfaceIdBytes = 2744;

// decomp: D_800E9278 (src/BATTLE/BATTLE.PRG/44F14Private.h:5)
extern std::array<std::uint8_t, kTileSurfaceIdBytes> tileSurfaceIds;
// decomp: D_800F49F9 (src/BATTLE/BATTLE.PRG/3A1A0.h:64)
extern std::uint8_t collisionProbeDirection;
// decomp: D_800F4B00 (src/BATTLE/BATTLE.PRG/40564.c:37)
extern std::int16_t platformContactSearchDepth;
// decomp: D_800F4B08 (src/BATTLE/BATTLE.PRG/3A1A0.c:49); the original's
// D_800F4B0A is this probe's height, read here as .vy.
extern SVECTOR collisionProbePosition;

}  // namespace VS::Field
