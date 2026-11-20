#pragma once
// The reward flow's two flags: whether the loot menu opened for the defeated
// enemy's drops, and whether the rewards follow an action rather than an
// encounter's end. Rewards.cpp defines them.

#include <cstdint>

namespace VS::Battle {

// decomp: D_800F1860 (src/BATTLE/BATTLE.PRG/146C.c:316)
extern std::int32_t lootMenuOpened;
// decomp: D_800F1864 (src/BATTLE/BATTLE.PRG/146C.c:317)
extern std::int32_t rewardsFollowAction;

}  // namespace VS::Battle
