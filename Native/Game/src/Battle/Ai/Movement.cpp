// The movement random state and patrol routes (Movement.hpp).
#include "Battle/Ai/Movement.hpp"

namespace VS::Battle::Ai {
// decomp: D_800F58C4 (D_800F58C4@BATTLE/BATTLE.PRG)
std::int32_t savedMovementRandomState;
// decomp: D_800F58D8 (D_800F58D8@BATTLE/BATTLE.PRG)
std::uint16_t (*patrolRoutes)[kPatrolRouteStops];
}  // namespace VS::Battle::Ai
