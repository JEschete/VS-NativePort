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

[[nodiscard]] bool Fits(const ScriptInstruction& instruction,
                        std::size_t length, ScriptVmHost& host) {
  if (instruction.bytes.size() >= length) {
    return true;
  }
  host.Refuse(ScriptVmRefusal::kShortInstruction,
              static_cast<int>(instruction.bytes.size()));
  return false;
}

[[nodiscard]] constexpr ScriptFlow Advance() {
  return ScriptFlow::Of(ScriptFlowKind::kAdvance);
}

[[nodiscard]] ScriptCommand Bytes(const ScriptInstruction& instruction) {
  return instruction.bytes;
}

}  // namespace

// decomp: BATTLE.PRG func_800B9770 (4A0A8.c:1966).
ScriptFlow ScriptSetGimmickVisibility(const ScriptInstruction& instruction,
                                      ScriptVmHost& host) {
  if (!Fits(instruction, kSetGimmickVisibilityLength, host)) {
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
  if (!Fits(instruction, kOverrideDepthCueDistanceLength, host)) {
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
  if (!Fits(instruction, kSetRoomLightingModeLength, host)) {
    return Advance();
  }
  const std::uint8_t draw_mode = OperandByte(Bytes(instruction), kFirstOperand);
  host.SetRoomGeometryDrawMode(draw_mode);
  return Advance();
}

// decomp: BATTLE.PRG func_800B9818 (4A0A8.c:1994).
ScriptFlow ScriptShowGeometryGroup(const ScriptInstruction& instruction,
                                   ScriptVmHost& host) {
  if (!Fits(instruction, kShowGeometryGroupLength, host)) {
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
  if (!Fits(instruction, kHideGeometryGroupLength, host)) {
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
  if (!Fits(instruction, kSwapGeometryGroupFlagsLength, host)) {
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
  if (!Fits(instruction, kSetCollisionTileModeLength, host)) {
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
  if (!Fits(instruction, kAdjustGeometryDepthBiasLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const auto depth_bias =
      static_cast<std::int8_t>(OperandByte(command, kSecondOperand));
  host.AddRoomGeometryDepthBias(OperandByte(command, kFirstOperand),
                                depth_bias);
  return Advance();
}

}  // namespace VS::Event
