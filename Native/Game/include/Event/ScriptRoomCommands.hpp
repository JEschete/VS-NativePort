#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script's room opcodes (BATTLE.PRG 4A0A8.c:1966-2012 and
// 2160-2274): moving platform visibility, the scripted texture LOD depth, the
// room geometry draw mode, the geometry groups' visibility, flags and depth
// bias, the collision map's blocked cells, the tiled background's load,
// visibility, offset and parallax, the first-person view, the room weather
// and its ambience, and the room effects. Each handler
// with operands checks its instruction's length, decodes them as the original
// did and drives the room only through ScriptVmHost; the room systems check
// the operands.
#include <cstddef>
#include <cstdint>

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
// Opcodes 0x73, 0x74, 0x7B, 0x7C and 0x7E. The background wait and free
// opcodes (0x75 and 0x76) are one byte and read no operands. Opcode 0x7E's
// fourth byte is unused.
inline constexpr std::size_t kSetRoomBackgroundVisibleLength = 2;
inline constexpr std::size_t kLoadRoomBackgroundLength = 2;
inline constexpr std::size_t kSetBackgroundOffsetLength = 5;
inline constexpr std::size_t kSetFirstPersonViewLength = 2;
inline constexpr std::size_t kSetBackgroundParallaxLength = 4;

// Opcode 0x7E's operands are signed parallax scales in 1/32 steps; the
// background keeps them in 1/4096 steps (the original shifts them left 7).
inline constexpr int kBackgroundParallaxOperandScale = 1 << 7;

// Opcodes 0x77-0x7A: the weather, the weather variation, the room effects
// and the weather ambience.
inline constexpr std::size_t kSetEnvironmentParticlesLength = 5;
inline constexpr std::size_t kSetParticleVariationEnabledLength = 2;
inline constexpr std::size_t kControlRoomAnimationLength = 3;
inline constexpr std::size_t kMuteEnvironmentSoundLength = 2;

// Opcode 0x77's first operand: the weather kind in its high nibble and the
// intensity in its low one. Its wind direction operand is in 1/256 turns and
// its strength operand in steps of 4 above a base of 1/8 turn (512); the
// weather keeps both in 1/4096 turns. A nonzero frame count eases the
// intensity and wind there instead, turning the wind the short way round.
inline constexpr unsigned kWeatherKindShift = 4;
inline constexpr unsigned kWeatherIntensityMask = 0xF;
inline constexpr int kWeatherDirectionOperandScale = 16;
inline constexpr int kWeatherStrengthOperandScale = 4;
inline constexpr int kWeatherStrengthBase = 512;
inline constexpr int kWeatherFullTurn = 4096;
inline constexpr int kWeatherHalfTurn = kWeatherFullTurn / 2;

// Opcode 0x7A mutes the weather ambience by playing no sound.
inline constexpr int kMutedWeatherAmbientSound = -1;

// Opcode 0x79's modes; any other mode does nothing.
inline constexpr std::uint8_t kStartRoomAnimation = 0;
inline constexpr std::uint8_t kToggleRoomAnimation = 1;

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
// decomp: BATTLE.PRG func_800B9F28 (src/BATTLE/BATTLE.PRG/4A0A8.c:2160)
// Opcode 0x73: shows or hides the room's tiled background.
ScriptFlow ScriptSetRoomBackgroundVisible(const ScriptInstruction& instruction,
                                          ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9F4C (src/BATTLE/BATTLE.PRG/4A0A8.c:2166)
// Opcode 0x74: queues the tiled background's reads into a texture page.
ScriptFlow ScriptLoadRoomBackground(const ScriptInstruction& instruction,
                                    ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9F70 (src/BATTLE/BATTLE.PRG/4A0A8.c:2172)
// Opcode 0x75: holds until the tiled background's load finishes.
ScriptFlow ScriptWaitForRoomBackground(const ScriptInstruction& instruction,
                                       ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9F90 (src/BATTLE/BATTLE.PRG/4A0A8.c:2174)
// Opcode 0x76: releases the tiled background, then yields while a cutscene
// is being skipped.
ScriptFlow ScriptFreeRoomBackground(const ScriptInstruction& instruction,
                                    ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BA218 (src/BATTLE/BATTLE.PRG/4A0A8.c:2256)
// Opcode 0x7B: the tiled background's signed scroll offsets.
ScriptFlow ScriptSetBackgroundOffset(const ScriptInstruction& instruction,
                                     ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BA264 (src/BATTLE/BATTLE.PRG/4A0A8.c:2262)
// Opcode 0x7C: a nonzero operand disables the first-person view.
ScriptFlow ScriptSetFirstPersonView(const ScriptInstruction& instruction,
                                    ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BA2B4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2274)
// Opcode 0x7E: the tiled background's signed parallax scales.
ScriptFlow ScriptSetBackgroundParallax(const ScriptInstruction& instruction,
                                       ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9FC0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2180)
// Opcode 0x77: selects the room weather, then sets its intensity and wind
// or starts the weather tween towards them.
ScriptFlow ScriptSetEnvironmentParticles(const ScriptInstruction& instruction,
                                         ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BA0E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2211)
// Opcode 0x78: a zero operand enables the weather's random variation.
ScriptFlow ScriptSetParticleVariationEnabled(
    const ScriptInstruction& instruction, ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BA108 (src/BATTLE/BATTLE.PRG/4A0A8.c:2217)
// Opcode 0x7A: a nonzero operand mutes the weather ambience of a room with
// no ambient sound section, saving it; zero restores the saved ambience.
ScriptFlow ScriptMuteEnvironmentSound(const ScriptInstruction& instruction,
                                      ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BA194 (src/BATTLE/BATTLE.PRG/4A0A8.c:2238)
// Opcode 0x79: starts a room effect, first finishing an active one while a
// cutscene is skipped, or toggles its updates.
ScriptFlow ScriptControlRoomAnimation(const ScriptInstruction& instruction,
                                      ScriptVmHost& host);

}  // namespace VS::Event
