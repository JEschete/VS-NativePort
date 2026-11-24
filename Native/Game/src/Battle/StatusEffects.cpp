// The status effect definitions, status and vitals timers and BATTLE.PRG menu
// globals declared in StatusEffects.hpp.
#include "Battle/StatusEffects.hpp"

#include <array>
#include <cstdint>

#include "Menu/Score/ScoreScreen.hpp"

namespace VS::Battle {

// decomp: D_800E8204 (D_800E8204@BATTLE/BATTLE.PRG)
// Contents: module image (ModuleData.cpp).
std::array<std::int32_t, kStatusLimbCount> limbStatusFlags;

// decomp: D_800E8490 (D_800E8490@BATTLE/BATTLE.PRG)
// Contents: module image (ModuleData.cpp).
std::int32_t vitalsTickCounter;

// decomp: D_800E8494 (D_800E8494@BATTLE/BATTLE.PRG)
// Contents: module image (ModuleData.cpp).
std::int32_t regeneratingLimb;

// decomp: D_800E8498 (D_800E8498@BATTLE/BATTLE.PRG)
// Contents: module image (ModuleData.cpp).
std::int32_t targetingRiskTimer;

// decomp: vs_battle_sceneBuffer (vs_battle_sceneBuffer@BATTLE/BATTLE.PRG)
void* sceneBuffer;

// decomp: vs_battle_menu9CursorMemory
// (vs_battle_menu9CursorMemory@BATTLE/BATTLE.PRG)
VS::Menu::Score::CursorMemory menu9CursorMemory;

// decomp: vs_battle_submenuState (vs_battle_submenuState@BATTLE/BATTLE.PRG)
std::int32_t submenuState;

// decomp: D_800F4590 (D_800F4590@BATTLE/BATTLE.PRG)
std::array<Actors::WeaponModel*, 1> previewWeaponModelAliases;

// decomp: D_800F4594 (D_800F4594@BATTLE/BATTLE.PRG)
std::int32_t previewShieldModelAlias;

}  // namespace VS::Battle
