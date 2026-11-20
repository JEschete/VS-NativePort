#pragma once
// The event script actor tables in the module image: the weapon draw state each
// weapon-mode operand selects and the animation bank bases the animation
// commands add to their ids. ModuleData.cpp fills them.

#include <cstddef>
#include <cstdint>

namespace VS::Event {
// Extents the copier took from the address gap to normalOpcodeTable
// (0x800E9D30), not capacities: the original reads three draw states and four
// bank bases, but ModuleData.cpp fills each whole extent from the module image
// and WeaponModeLive.cpp and ScriptActorCommandsLive.cpp bound reads by it.
inline constexpr std::size_t kWeaponDrawStateExtent = 121;
inline constexpr std::size_t kAnimationBankBaseExtent = 472;

// The actor weapon-drawn state each script weapon-mode operand selects.
// decomp: weaponDrawnModes (D_800E9B4C@BATTLE/BATTLE.PRG)
extern int weaponDrawStates[kWeaponDrawStateExtent];
// The animation-id bases the top two bits of an animation command select.
// decomp: animationBankBase (D_800E9B58@BATTLE/BATTLE.PRG)
extern std::uint8_t animationBankBases[kAnimationBankBaseExtent];
}  // namespace VS::Event
