#pragma once
// The built-in scene reset script, which special operation 16 runs as a
// subroutine before returning to the calling script. ModuleData.cpp fills it
// from the module image.

#include <cstddef>
#include <cstdint>

namespace VS::Event {
// The copier's extent to normalOpcodeTable (0x800E9D30), not the script's
// size: ScriptVmLive.cpp copies the first kSubroutineScriptBytes.
inline constexpr std::size_t kSpecialSubroutineScriptExtent = 468;

// decomp: sceneResetScript (D_800E9B5C@BATTLE/BATTLE.PRG)
extern std::uint8_t specialSubroutineScript[kSpecialSubroutineScriptExtent];
}  // namespace VS::Event
