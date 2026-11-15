#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script's room opcodes (BATTLE.PRG 4A0A8.c:1966-2012 and 2268):
// moving platform visibility, the scripted texture LOD depth, the room
// geometry draw mode, the geometry groups' visibility, flags and depth bias
// and the collision map's blocked cells. Each handler checks its
// instruction's length, decodes its operands as the original did and drives
// the room only through ScriptVmHost; the room systems check the operands.
#include <cstddef>

#include "Event/ScriptVm.hpp"

namespace VS::Event {

// Instruction lengths in bytes, opcode included, from the event VM's opcode
// size table (BATTLE.PRG D_800E9C30, opcodes 0x60-0x62, 0x64-0x67 and 0x7D).
inline constexpr std::size_t kSetGimmickVisibilityLength = 3;
inline constexpr std::size_t kOverrideDepthCueDistanceLength = 3;
inline constexpr std::size_t kSetRoomLightingModeLength = 2;
inline constexpr std::size_t kShowGeometryGroupLength = 2;
inline constexpr std::size_t kHideGeometryGroupLength = 2;
inline constexpr std::size_t kSwapGeometryGroupFlagsLength = 3;
inline constexpr std::size_t kSetCollisionTileModeLength = 5;
inline constexpr std::size_t kAdjustGeometryDepthBiasLength = 3;

// The depth cue operand: its sign bit keeps a scripted texture LOD depth
// over the camera's far clip and its low eleven bits are that depth.
inline constexpr unsigned kDepthCueOverrideBit = 0x8000;
inline constexpr unsigned kDepthCueDepthMask = 0x7FF;

// decomp: BATTLE.PRG func_800B9770 (src/BATTLE/BATTLE.PRG/4A0A8.c:1966)
// Opcode 0x60: shows or hides a moving platform.
ScriptFlow ScriptSetGimmickVisibility(const ScriptInstruction& instruction,
                                      ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B979C (src/BATTLE/BATTLE.PRG/4A0A8.c:1972)
// Opcode 0x61: keeps a scripted texture LOD depth, or releases it to the
// camera's far clip.
ScriptFlow ScriptOverrideDepthCueDistance(const ScriptInstruction& instruction,
                                          ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B97E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1982)
// Opcode 0x62: selects the room geometry's draw mode.
ScriptFlow ScriptSetRoomLightingMode(const ScriptInstruction& instruction,
                                     ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9818 (src/BATTLE/BATTLE.PRG/4A0A8.c:1994)
// Opcode 0x64.
ScriptFlow ScriptShowGeometryGroup(const ScriptInstruction& instruction,
                                   ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B983C (src/BATTLE/BATTLE.PRG/4A0A8.c:2000)
// Opcode 0x65.
ScriptFlow ScriptHideGeometryGroup(const ScriptInstruction& instruction,
                                   ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9860 (src/BATTLE/BATTLE.PRG/4A0A8.c:2006)
// Opcode 0x66.
ScriptFlow ScriptSwapGeometryGroupFlags(const ScriptInstruction& instruction,
                                        ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B988C (src/BATTLE/BATTLE.PRG/4A0A8.c:2012)
// Opcode 0x67: blocks or unblocks a collision cell. The operands are the
// tile x, the layer, the tile z and the blocked byte.
ScriptFlow ScriptSetCollisionTileMode(const ScriptInstruction& instruction,
                                      ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BA288 (src/BATTLE/BATTLE.PRG/4A0A8.c:2268)
// Opcode 0x7D: adds a signed depth bias to a geometry group.
ScriptFlow ScriptAdjustGeometryDepthBias(const ScriptInstruction& instruction,
                                         ScriptVmHost& host);

}  // namespace VS::Event
