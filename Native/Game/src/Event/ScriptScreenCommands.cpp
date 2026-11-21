// Hand-maintained (tools/port/owned.csv).
#include "Event/ScriptScreenCommands.hpp"

#include <cstddef>
#include <cstdint>

#include "Event/ScriptActorCommands.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptVm.hpp"

namespace VS::Event {
namespace {

// Operand offsets after the opcode byte.
constexpr std::size_t kFirstOperand = 1;
constexpr std::size_t kSecondOperand = 2;
constexpr std::size_t kThirdOperand = 3;
constexpr std::size_t kFourthOperand = 4;
constexpr std::size_t kFifthOperand = 5;
constexpr std::size_t kSixthOperand = 6;
constexpr std::size_t kSeventhOperand = 7;

// The values of each two-bit switch of opcode 0x6D that change state.
constexpr unsigned kSwitchOff = 0;
constexpr unsigned kSwitchOn = 1;

// Opcode 0x70's targets.
constexpr unsigned kActorPaletteTarget = 0;
constexpr unsigned kScenePaletteTarget = 1;
constexpr unsigned kBackgroundPaletteTarget = 2;

[[nodiscard]] constexpr ScriptFlow Advance() {
  return ScriptFlow::Of(ScriptFlowKind::kAdvance);
}

[[nodiscard]] constexpr ScriptFlow Hold() {
  return ScriptFlow::Of(ScriptFlowKind::kHold);
}

[[nodiscard]] ScriptCommand Bytes(const ScriptInstruction& instruction) {
  return instruction.bytes;
}

// Opcodes 0x69, 0x6A and 0x6C: effect, speed and the wipe angle in steps.
[[nodiscard]] ScreenTransitionRequest TransitionRequest(ScriptCommand command) {
  return {.effect = OperandByte(command, kFirstOperand),
          .speed = OperandByte(command, kSecondOperand),
          .wipe_angle = OperandByte(command, kThirdOperand) * kWipeAngleStep};
}

// The two-bit switch at `shift` of opcode 0x6D's operand.
[[nodiscard]] constexpr unsigned Switch(std::uint8_t flags, unsigned shift) {
  return (static_cast<unsigned>(flags) >> shift) & kDisplaySwitchMask;
}

[[nodiscard]] constexpr std::int16_t SignedOffset(std::uint8_t value) {
  return static_cast<std::int16_t>(static_cast<std::int8_t>(value));
}

}  // namespace

// decomp: BATTLE.PRG vs_battle_script_69 (src/BATTLE/BATTLE.PRG/4DEA8.c:2636;
// was func_800B9A40). Phase 0 starts the fade-out, phase 5 (the covered screen)
// ends the wait and every other phase keeps waiting.
ScriptFlow ScriptFadeOutAndWait(const ScriptInstruction& instruction,
                                ScriptVmHost& host) {
  if (!InstructionFits(instruction, kFadeOutAndWaitLength, host)) {
    return Advance();
  }
  const int transition_phase = host.ScreenTransitionPhase();
  if (transition_phase == kScreenTransitionFadedOut) {
    return Advance();
  }
  if (transition_phase == kScreenTransitionIdle) {
    host.BeginScreenFadeOut(TransitionRequest(Bytes(instruction)));
  }
  return Hold();
}

// decomp: BATTLE.PRG vs_battle_script_6A (src/BATTLE/BATTLE.PRG/4DEA8.c:2648;
// was func_800B9A94).
ScriptFlow ScriptFadeIn(const ScriptInstruction& instruction,
                        ScriptVmHost& host) {
  if (!InstructionFits(instruction, kFadeInLength, host)) {
    return Advance();
  }
  host.BeginScreenFadeIn(TransitionRequest(Bytes(instruction)));
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_6B (src/BATTLE/BATTLE.PRG/4DEA8.c:2654;
// was func_800B9AC4).
ScriptFlow ScriptSetWipeAngle(const ScriptInstruction& instruction,
                              ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetWipeAngleLength, host)) {
    return Advance();
  }
  host.SetTransitionWipeAngle(OperandByte(Bytes(instruction), kFirstOperand) *
                              kWipeAngleStep);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_6C (src/BATTLE/BATTLE.PRG/4DEA8.c:2660;
// was func_800B9AE8).
ScriptFlow ScriptConfigureScreenTransition(const ScriptInstruction& instruction,
                                           ScriptVmHost& host) {
  if (!InstructionFits(instruction, kConfigureScreenTransitionLength, host)) {
    return Advance();
  }
  host.ConfigureScreenTransition(TransitionRequest(Bytes(instruction)));
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_6D (src/BATTLE/BATTLE.PRG/4DEA8.c:2666;
// was func_800B9B18). Switch values 2 and 3 leave their system unchanged.
ScriptFlow ScriptSetRoomDisplayFlags(const ScriptInstruction& instruction,
                                     ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetRoomDisplayFlagsLength, host)) {
    return Advance();
  }
  const std::uint8_t flags = OperandByte(Bytes(instruction), kFirstOperand);
  switch (Switch(flags, 0)) {
    case kSwitchOff:
      host.DisableDisplay();
      break;
    case kSwitchOn:
      host.EnableDisplay();
      break;
    default:
      break;
  }
  switch (Switch(flags, kRoomGeometrySwitchShift)) {
    case kSwitchOff:
      host.HidePrimaryRoomGeometry();
      break;
    case kSwitchOn:
      host.ShowPrimaryRoomGeometry();
      break;
    default:
      break;
  }
  switch (Switch(flags, kActorModelSwitchShift)) {
    case kSwitchOff:
      host.DisableAllActorModels();
      break;
    case kSwitchOn:
      host.EnableAllActorModels();
      break;
    default:
      break;
  }
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_6E (src/BATTLE/BATTLE.PRG/4DEA8.c:2697;
// was func_800B9BF8).
ScriptFlow ScriptWaitForScreenTransition(
    const ScriptInstruction& /*instruction*/, ScriptVmHost& host) {
  return host.ScreenTransitionPhase() != kScreenTransitionIdle ? Hold()
                                                               : Advance();
}

// decomp: BATTLE.PRG vs_battle_script_6F (src/BATTLE/BATTLE.PRG/4DEA8.c:2702;
// was func_800B9C18).
ScriptFlow ScriptTogglePaletteHook(const ScriptInstruction& instruction,
                                   ScriptVmHost& host) {
  if (!InstructionFits(instruction, kTogglePaletteHookLength, host)) {
    return Advance();
  }
  host.RunPaletteHook(OperandByte(Bytes(instruction), kFirstOperand) != 0
                          ? PaletteHook::kNonzeroOperand
                          : PaletteHook::kZeroOperand);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_70 (src/BATTLE/BATTLE.PRG/4DEA8.c:2712;
// was func_800B9C58). The selector halfword at operand 2 is read unaligned; the
// duration shift is the R3000's sllv, which uses the operand modulo 32.
ScriptFlow ScriptTransformPalettes(const ScriptInstruction& instruction,
                                   ScriptVmHost& host) {
  if (!InstructionFits(instruction, kTransformPalettesLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::uint8_t selector = OperandByte(command, kFirstOperand);
  const unsigned shift =
      OperandByte(command, kFourthOperand) & kShiftAmountMask;
  const PaletteTransform transform{
      .mode = static_cast<int>(selector & kPaletteModeMask),
      .duration = static_cast<int>((1U << shift) & kPaletteDurationMask),
      .red_offset = SignedOffset(OperandByte(command, kFifthOperand)),
      .green_offset = SignedOffset(OperandByte(command, kSixthOperand)),
      .blue_offset = SignedOffset(OperandByte(command, kSeventhOperand)),
  };
  switch (static_cast<unsigned>(selector) >> kPaletteTargetShift) {
    case kActorPaletteTarget: {
      host.BeginScriptActorSelection(
          static_cast<std::uint16_t>(OperandHalf(command, kSecondOperand)));
      for (std::uint16_t actor = host.NextScriptSelectedActor();
           actor != kSelectionEnd; actor = host.NextScriptSelectedActor()) {
        host.TransformActorColors(transform, actor);
      }
      break;
    }
    case kScenePaletteTarget:
      host.TransformSceneColors(transform);
      break;
    case kBackgroundPaletteTarget:
      host.TransformBackgroundColors(transform);
      break;
    default:
      break;
  }
  return Advance();
}

}  // namespace VS::Event
