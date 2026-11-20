// The action flow's state (ActionFlow.hpp).
#include "Battle/ActionFlow.hpp"

namespace VS::Battle {
// decomp: D_800F18EC (D_800F18EC@BATTLE/BATTLE.PRG)
VS::Menu::Framework::ActionSelection* actionSelection;
// decomp: _cameraMode (_cameraMode@BATTLE/BATTLE.PRG)
std::uint32_t actionFlowMode;
// decomp: D_800F18FC (D_800F18FC@BATTLE/BATTLE.PRG)
std::int32_t recoveryRewardMode;
// decomp: D_800F19CC (D_800F19CC@BATTLE/BATTLE.PRG)
Battle_ActionState* actionState;
}  // namespace VS::Battle

// D_8005FFAF was vs_main_scoredata.weaponKillStreak (+0x10F), not a separate
// global (#448; State/KillScoreLive.cpp ResetLiveWeaponKillStreak).
