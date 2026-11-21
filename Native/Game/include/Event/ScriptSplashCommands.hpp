#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script's splash image opcodes (BATTLE.PRG 4A0A8.c:609 and
// 851-914): waiting for the image file, queueing and freeing the image set,
// configuring a layer, starting a layer's tween and waiting for the tweens.
// Each handler checks its instruction's length, decodes its operands as the
// original did and drives the image set and the tween records only through
// ScriptVmHost.
//
// A layer selector is the top two bits of its byte, so it is 0-3. The image
// set has three layers (BATTLE.PRG vs_battle_loadGim allocates three) and
// the tween table three records plus padding (D_800F4B30, 64 bytes). The
// original did not check selector 3: it wrote past the image set's heap
// block and, when tweening, over the first bytes of
// D_800F4B70 (the script-controlled actors). Here the host refuses layer 3's
// image layer, so its tween is never started.
#include <cstddef>

#include "Event/ScriptVm.hpp"

namespace VS::Event {

// Instruction lengths in bytes, opcode included, from the event VM's opcode
// size table (BATTLE.PRG D_800E9C30, opcodes 0x0C and 0x16-0x1A).
inline constexpr std::size_t kWaitForSplashFileLength = 1;
inline constexpr std::size_t kLoadSplashImageLength = 4;
inline constexpr std::size_t kConfigureSplashLayerLength = 6;
inline constexpr std::size_t kTweenSplashLayerLength = 7;
inline constexpr std::size_t kFreeSplashImageLength = 1;
inline constexpr std::size_t kWaitForSplashTweenLength = 1;

// The layer selector's position in its byte; the low bits carry the draw
// routine (opcode 0x17) or the easing mode (opcode 0x18).
inline constexpr unsigned kSplashLayerShift = 6;
inline constexpr unsigned kSplashRoutineMask = 3;
inline constexpr unsigned kSplashEasingMask = 7;
// Offsets are stored doubled and the scale byte is the top of a 4.12 scale.
inline constexpr int kSplashOffsetScale = 2;
inline constexpr unsigned kSplashScaleShift = 7;

// decomp: BATTLE.PRG vs_battle_script_0C_waitGimLoad
// (src/BATTLE/BATTLE.PRG/4DEA8.c:390; was func_800B6724) Opcode 0x0C: holds
// while the image file loads.
ScriptFlow ScriptWaitForSplashFile(const ScriptInstruction& instruction,
                                   ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_16_loadGim
// (src/BATTLE/BATTLE.PRG/4DEA8.c:708; was vs_battle_script_loadGim) Opcode
// 0x16: queues the image set; the operands are a signed image id halfword and
// the first texture page.
ScriptFlow ScriptLoadSplashImage(const ScriptInstruction& instruction,
                                 ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_17_showGimImage
// (src/BATTLE/BATTLE.PRG/4DEA8.c:725; was func_800B7014) Opcode 0x17: stops the
// layer's tween and sets its draw routine, signed offsets, scale and
// brightness. Nothing happens without an image set.
ScriptFlow ScriptConfigureSplashLayer(const ScriptInstruction& instruction,
                                      ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_18_tweenGimImage
// (src/BATTLE/BATTLE.PRG/4DEA8.c:762; was func_800B70E8) Opcode 0x18: starts a
// tween from the layer's placement to the operands'. Holds while the layer's
// tween runs; advances without an image layer.
ScriptFlow ScriptTweenSplashLayer(const ScriptInstruction& instruction,
                                  ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_19_unloadGim
// (src/BATTLE/BATTLE.PRG/4DEA8.c:791; was func_800B71C4) Opcode 0x19: frees a
// ready image set; yields while a cutscene is skipped.
ScriptFlow ScriptFreeSplashImage(const ScriptInstruction& instruction,
                                 ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_1A_waitGimTweens
// (src/BATTLE/BATTLE.PRG/4DEA8.c:801; was func_800B71F4) Opcode 0x1A: holds
// while any of the three layers' tweens runs.
ScriptFlow ScriptWaitForSplashTween(const ScriptInstruction& instruction,
                                    ScriptVmHost& host);

}  // namespace VS::Event
