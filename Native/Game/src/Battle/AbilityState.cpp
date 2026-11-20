// The player's battle actor and the Break Art and Battle Ability point
// requirements declared in AbilityState.hpp.
#include "Battle/AbilityState.hpp"

#include <array>
#include <cstdint>

namespace VS::Battle {

// decomp: vs_battle_characterState
// (vs_battle_characterState@BATTLE/BATTLE.PRG)
Battle_Actor* characterState;

// decomp: vs_main_artsPointsRequirements
// (vs_main_artsPointsRequirements@SLUS_010.40)
// Contents: module image (ModuleData.cpp).
BreakArtPointTable breakArtPointRequirements;

// decomp: vs_main_battleAbilitiesPointsRequirements
// (vs_main_battleAbilitiesPointsRequirements@SLUS_010.40)
// Contents: module image (ModuleData.cpp).
AbilityPointTable battleAbilityPointRequirements;

}  // namespace VS::Battle

// D_8005FFB0 was vs_main_scoredata.chainStreak (+0x110), not a separate global
// (#448; Battle/Abilities.cpp JudgeTimedInput).
