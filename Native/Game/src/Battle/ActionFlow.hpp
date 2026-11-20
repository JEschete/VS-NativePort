#pragma once
// The action flow's state: the selection the player is building, the flow and
// recovery-reward modes, and the live action state. ActionFlow.cpp defines
// them.

#include <cstdint>

namespace VS::Battle {
struct ActionState;
}  // namespace VS::Battle

namespace VS::Menu::Framework {
struct ActionSelection;
}  // namespace VS::Menu::Framework

namespace VS::Battle {

// decomp: D_800F18EC (src/BATTLE/BATTLE.PRG/146C.c:289)
extern VS::Menu::Framework::ActionSelection* actionSelection;
// decomp: _cameraMode, upstream cameraMode (src/BATTLE/BATTLE.PRG/146C.c:327)
extern std::uint32_t actionFlowMode;
// decomp: D_800F18FC (src/BATTLE/BATTLE.PRG/146C.c:329)
extern std::int32_t recoveryRewardMode;
// decomp: D_800F19CC (src/BATTLE/BATTLE.PRG/146C.c:335)
extern VS::Battle::ActionState* actionState;

}  // namespace VS::Battle
