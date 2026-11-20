// The event script actor tables (Actors.hpp).
#include "Event/Actors.hpp"

namespace VS::Event {
// decomp: weaponDrawnModes (D_800E9B4C@BATTLE/BATTLE.PRG)
// Contents: the module image (ModuleData.cpp).
int weaponDrawStates[kWeaponDrawStateExtent];

// decomp: animationBankBase (D_800E9B58@BATTLE/BATTLE.PRG)
// Contents: the module image (ModuleData.cpp).
std::uint8_t animationBankBases[kAnimationBankBaseExtent];
}  // namespace VS::Event
