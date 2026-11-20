// The orbiting boss movement state (OrbitingBoss.hpp).
#include "Battle/Ai/OrbitingBoss.hpp"

namespace VS::Battle::Ai {
// decomp: D_800F17DC (D_800F17DC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<PathPoint, kRadialRetreatPathPoints> radialRetreatPath;
// decomp: D_800F5918 (D_800F5918@BATTLE/BATTLE.PRG)
std::int32_t orbitAngularVelocity;
// decomp: D_800F591C (D_800F591C@BATTLE/BATTLE.PRG)
std::int32_t orbitHeightOffset;
// decomp: D_800F5920 (D_800F5920@BATTLE/BATTLE.PRG)
ArenaOrbitState* arenaOrbit;
}  // namespace VS::Battle::Ai
