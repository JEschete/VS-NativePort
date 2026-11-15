#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script's fade, display and palette opcodes (BATTLE.PRG
// 4A0A8.c:2049-2122): the scripted screen fades and wipes and their wait,
// the display, primary room geometry and actor model switches, the two empty
// palette hooks and the palette transforms. Each handler checks its
// instruction's length, decodes its operands as the original did and drives
// the screen only through ScriptVmHost.
#include <cstddef>

#include "Event/ScriptVm.hpp"

namespace VS::Event {

// Instruction lengths in bytes, opcode included, from the event VM's opcode
// size table (BATTLE.PRG D_800E9C30, opcodes 0x69-0x70).
inline constexpr std::size_t kFadeOutAndWaitLength = 4;
inline constexpr std::size_t kFadeInLength = 4;
inline constexpr std::size_t kSetWipeAngleLength = 2;
inline constexpr std::size_t kConfigureScreenTransitionLength = 4;
inline constexpr std::size_t kSetRoomDisplayFlagsLength = 2;
inline constexpr std::size_t kWaitForScreenTransitionLength = 1;
inline constexpr std::size_t kTogglePaletteHookLength = 2;
inline constexpr std::size_t kTransformPalettesLength = 8;

// The screen transition phases the fade opcodes poll (func_8007C398,
// Field::TransitionPhase): idle, and the covered screen a fade-out ends on.
inline constexpr int kScreenTransitionIdle = 0;
inline constexpr int kScreenTransitionFadedOut = 5;
// The wipe angle operand counts sixteen-unit steps.
inline constexpr int kWipeAngleStep = 0x10;
// Each of opcode 0x6D's three switches is two bits: 0 turns its system off,
// 1 on, 2 and 3 leave it.
inline constexpr unsigned kDisplaySwitchMask = 3;
inline constexpr unsigned kRoomGeometrySwitchShift = 2;
inline constexpr unsigned kActorModelSwitchShift = 4;
// Opcode 0x70's selector byte: the target in the high nibble, the transform
// mode in the low one.
inline constexpr unsigned kPaletteTargetShift = 4;
inline constexpr unsigned kPaletteModeMask = 0xF;
// The duration is one bit shifted by the operand modulo 32 (the R3000's
// sllv), kept to its low five bits.
inline constexpr unsigned kShiftAmountMask = 31;
inline constexpr unsigned kPaletteDurationMask = 0x1F;

// decomp: BATTLE.PRG func_800B9A40 (src/BATTLE/BATTLE.PRG/4A0A8.c:2049)
// Opcode 0x69: starts the fade-out on an idle transition and holds until the
// transition reports the covered screen.
ScriptFlow ScriptFadeOutAndWait(const ScriptInstruction& instruction,
                                ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9A94 (src/BATTLE/BATTLE.PRG/4A0A8.c:2061)
// Opcode 0x6A.
ScriptFlow ScriptFadeIn(const ScriptInstruction& instruction,
                        ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9AC4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2067)
// Opcode 0x6B.
ScriptFlow ScriptSetWipeAngle(const ScriptInstruction& instruction,
                              ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9AE8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2073)
// Opcode 0x6C: the style of later room transitions.
ScriptFlow ScriptConfigureScreenTransition(const ScriptInstruction& instruction,
                                           ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9B18 (src/BATTLE/BATTLE.PRG/4A0A8.c:2079)
// Opcode 0x6D: the display, then the primary room geometry, then the actor
// models.
ScriptFlow ScriptSetRoomDisplayFlags(const ScriptInstruction& instruction,
                                     ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9BF8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2110)
// Opcode 0x6E: holds while any screen transition runs. It reads no operand.
ScriptFlow ScriptWaitForScreenTransition(const ScriptInstruction& instruction,
                                         ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9C18 (src/BATTLE/BATTLE.PRG/4A0A8.c:2112)
// Opcode 0x6F: runs one of two palette hooks, both empty in this executable.
ScriptFlow ScriptTogglePaletteHook(const ScriptInstruction& instruction,
                                   ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9C58 (src/BATTLE/BATTLE.PRG/4A0A8.c:2122)
// Opcode 0x70: transforms the selected actors' (target 0), the scene's (1) or
// the background's (2) colours; other targets do nothing. The actor
// selection runs until the host returns kSelectionEnd.
ScriptFlow ScriptTransformPalettes(const ScriptInstruction& instruction,
                                   ScriptVmHost& host);

}  // namespace VS::Event
