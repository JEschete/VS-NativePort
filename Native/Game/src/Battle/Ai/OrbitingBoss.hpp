#pragma once
// The orbiting boss movement: the radial retreat path read from the module
// image, the orbit's angular velocity and height offset, and the arena orbit
// state. OrbitingBoss.cpp defines them.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/Ai/Movement.hpp"

namespace VS::Battle::Ai {

inline constexpr std::size_t kRadialRetreatPathPoints = 36;

// One retreat path point's x and z.
using PathPoint = std::array<std::int16_t, 2>;

// decomp: D_800F17DC (src/BATTLE/BATTLE.PRG/7EE98.c:12)
extern std::array<PathPoint, kRadialRetreatPathPoints> radialRetreatPath;
// decomp: D_800F5918 (src/BATTLE/BATTLE.PRG/7EE98.c:13)
extern std::int32_t orbitAngularVelocity;
// decomp: D_800F591C (src/BATTLE/BATTLE.PRG/7EE98.c:14)
extern std::int32_t orbitHeightOffset;
// decomp: D_800F5920 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:536)
extern ArenaOrbitState* arenaOrbit;

}  // namespace VS::Battle::Ai
