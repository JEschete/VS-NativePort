// Hand-maintained (tools/port/owned.csv).
#include "Event/ScriptRoomCommands.hpp"

#include <cstddef>
#include <cstdint>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptVm.hpp"

namespace VS::Event {
namespace {

// Operand offsets after the opcode byte.
constexpr std::size_t kFirstOperand = 1;
constexpr std::size_t kSecondOperand = 2;
constexpr std::size_t kThirdOperand = 3;
constexpr std::size_t kFourthOperand = 4;

[[nodiscard]] constexpr ScriptFlow Advance() {
  return ScriptFlow::Of(ScriptFlowKind::kAdvance);
}

[[nodiscard]] constexpr ScriptFlow Hold() {
  return ScriptFlow::Of(ScriptFlowKind::kHold);
}

[[nodiscard]] constexpr ScriptFlow Yield() {
  return ScriptFlow::Of(ScriptFlowKind::kAdvanceAndYield);
}

// A signed parallax operand in the background's 1/4096 steps.
[[nodiscard]] int ParallaxScale(ScriptCommand command, std::size_t offset) {
  const auto steps = static_cast<std::int8_t>(OperandByte(command, offset));
  return steps * kBackgroundParallaxOperandScale;
}

[[nodiscard]] ScriptCommand Bytes(const ScriptInstruction& instruction) {
  return instruction.bytes;
}

}  // namespace

// decomp: BATTLE.PRG func_800B9770 (4A0A8.c:1966).
ScriptFlow ScriptSetGimmickVisibility(const ScriptInstruction& instruction,
                                      ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetGimmickVisibilityLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::uint8_t platform = OperandByte(command, kFirstOperand);
  host.SetMovingPlatformVisibility(platform,
                                   OperandByte(command, kSecondOperand));
  return Advance();
}

// decomp: BATTLE.PRG func_800B979C (4A0A8.c:1972). The original spreads the
// operand's sign bit over the preserve flag (0 or 0xFF) and sets the depth
// only while that flag is set.
ScriptFlow ScriptOverrideDepthCueDistance(const ScriptInstruction& instruction,
                                          ScriptVmHost& host) {
  if (!InstructionFits(instruction, kOverrideDepthCueDistanceLength, host)) {
    return Advance();
  }
  const auto depth_cue = static_cast<std::uint16_t>(
      OperandHalf(Bytes(instruction), kFirstOperand));
  const bool keeps_depth = (depth_cue & kDepthCueOverrideBit) != 0;
  host.SetScriptBackdropEnabled(keeps_depth);
  if (keeps_depth) {
    host.SetRoomTextureLodDepth(
        static_cast<int>(depth_cue & kDepthCueDepthMask));
  }
  return Advance();
}

// decomp: BATTLE.PRG func_800B97E4 (4A0A8.c:1982). The mode byte is stored
// zero-extended into the draw mode word.
ScriptFlow ScriptSetRoomLightingMode(const ScriptInstruction& instruction,
                                     ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetRoomLightingModeLength, host)) {
    return Advance();
  }
  const std::uint8_t draw_mode = OperandByte(Bytes(instruction), kFirstOperand);
  host.SetRoomGeometryDrawMode(draw_mode);
  return Advance();
}

// decomp: BATTLE.PRG func_800B9818 (4A0A8.c:1994).
ScriptFlow ScriptShowGeometryGroup(const ScriptInstruction& instruction,
                                   ScriptVmHost& host) {
  if (!InstructionFits(instruction, kShowGeometryGroupLength, host)) {
    return Advance();
  }
  const std::uint8_t shown_group =
      OperandByte(Bytes(instruction), kFirstOperand);
  host.ShowRoomGeometryGroup(shown_group);
  return Advance();
}

// decomp: BATTLE.PRG func_800B983C (4A0A8.c:2000).
ScriptFlow ScriptHideGeometryGroup(const ScriptInstruction& instruction,
                                   ScriptVmHost& host) {
  if (!InstructionFits(instruction, kHideGeometryGroupLength, host)) {
    return Advance();
  }
  const std::uint8_t hidden_group =
      OperandByte(Bytes(instruction), kFirstOperand);
  host.HideRoomGeometryGroup(hidden_group);
  return Advance();
}

// decomp: BATTLE.PRG func_800B9860 (4A0A8.c:2006).
ScriptFlow ScriptSwapGeometryGroupFlags(const ScriptInstruction& instruction,
                                        ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSwapGeometryGroupFlagsLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::uint8_t first_group = OperandByte(command, kFirstOperand);
  const std::uint8_t second_group = OperandByte(command, kSecondOperand);
  host.SwapRoomGeometryGroupFlags(first_group, second_group);
  return Advance();
}

// decomp: BATTLE.PRG func_800B988C (4A0A8.c:2012). The script orders the
// operands x, layer, z, blocked; func_8008B808 takes x, z, layer, blocked.
ScriptFlow ScriptSetCollisionTileMode(const ScriptInstruction& instruction,
                                      ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetCollisionTileModeLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  host.SetCollisionCellBlocked(
      {.tile_x = OperandByte(command, kFirstOperand),
       .tile_z = OperandByte(command, kThirdOperand),
       .layer = OperandByte(command, kSecondOperand),
       .blocked = OperandByte(command, kFourthOperand)});
  return Advance();
}

// decomp: BATTLE.PRG func_800BA288 (4A0A8.c:2268). The bias byte is signed.
ScriptFlow ScriptAdjustGeometryDepthBias(const ScriptInstruction& instruction,
                                         ScriptVmHost& host) {
  if (!InstructionFits(instruction, kAdjustGeometryDepthBiasLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const auto depth_bias =
      static_cast<std::int8_t>(OperandByte(command, kSecondOperand));
  host.AddRoomGeometryDepthBias(OperandByte(command, kFirstOperand),
                                depth_bias);
  return Advance();
}

// decomp: BATTLE.PRG func_800B9F28 (4A0A8.c:2160). The byte is stored as
// the background's enabled flag unchanged.
ScriptFlow ScriptSetRoomBackgroundVisible(const ScriptInstruction& instruction,
                                          ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetRoomBackgroundVisibleLength, host)) {
    return Advance();
  }
  const std::uint8_t enabled = OperandByte(Bytes(instruction), kFirstOperand);
  host.SetTiledBackgroundEnabled(enabled);
  return Advance();
}

// decomp: BATTLE.PRG func_800B9F4C (4A0A8.c:2166). Whether the reads were
// queued is ignored; the wait opcode follows them.
ScriptFlow ScriptLoadRoomBackground(const ScriptInstruction& instruction,
                                    ScriptVmHost& host) {
  if (!InstructionFits(instruction, kLoadRoomBackgroundLength, host)) {
    return Advance();
  }
  const std::uint8_t texture_page =
      OperandByte(Bytes(instruction), kFirstOperand);
  host.QueueTiledBackgroundLoad(texture_page);
  return Advance();
}

// decomp: BATTLE.PRG func_800B9F70 (4A0A8.c:2172): returns
// func_8008F0FC() == 0, holding on every step before the last.
ScriptFlow ScriptWaitForRoomBackground(const ScriptInstruction& /*instruction*/,
                                       ScriptVmHost& host) {
  const bool loaded = host.FinishTiledBackgroundLoad();
  return loaded ? Advance() : Hold();
}

// decomp: BATTLE.PRG func_800B9F90 (4A0A8.c:2174): returns
// (D_800F4C2C == 2) * 4, read after the release.
ScriptFlow ScriptFreeRoomBackground(const ScriptInstruction& /*instruction*/,
                                    ScriptVmHost& host) {
  host.ReleaseTiledBackground();
  if (host.SkipState() == CutsceneSkip::kSkipping) {
    return Yield();
  }
  return Advance();
}

// decomp: BATTLE.PRG func_800BA218 (4A0A8.c:2256). Each offset is a
// little-endian halfword sign-extended by vs_battle_getShort.
ScriptFlow ScriptSetBackgroundOffset(const ScriptInstruction& instruction,
                                     ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetBackgroundOffsetLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::int16_t offset_x = OperandHalf(command, kFirstOperand);
  const std::int16_t offset_y = OperandHalf(command, kThirdOperand);
  host.SetRoomBackgroundOffset(offset_x, offset_y);
  return Advance();
}

// decomp: BATTLE.PRG func_800BA264 (4A0A8.c:2262). The operand is reduced
// to 0 or 1 before func_8007BDD8 takes it as the disabled flag.
ScriptFlow ScriptSetFirstPersonView(const ScriptInstruction& instruction,
                                    ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetFirstPersonViewLength, host)) {
    return Advance();
  }
  const bool disabled = OperandByte(Bytes(instruction), kFirstOperand) != 0;
  host.SetFirstPersonViewDisabled(disabled);
  return Advance();
}

// decomp: BATTLE.PRG func_800BA2B4 (4A0A8.c:2274). Both bytes are signed.
ScriptFlow ScriptSetBackgroundParallax(const ScriptInstruction& instruction,
                                       ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetBackgroundParallaxLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  host.SetBackgroundParallaxScale(ParallaxScale(command, kFirstOperand),
                                  ParallaxScale(command, kSecondOperand));
  return Advance();
}

}  // namespace VS::Event
