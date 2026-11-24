// The player's mutable battle actor.
#include "Battle/AbilityState.hpp"

namespace VS::Battle {

// decomp: vs_battle_characterState
// (vs_battle_characterState@BATTLE/BATTLE.PRG)
VS::Battle::Actor* characterState;

}  // namespace VS::Battle

// D_8005FFB0 was vs_main_scoredata.chainStreak (+0x110), not a separate global
// (#448; Battle/Abilities.cpp JudgeTimedInput).
