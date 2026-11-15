// Hand-maintained (tools/port/owned.csv).
#include "Event/ScriptSplashCommands.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptEffectCommands.hpp"
#include "Event/ScriptVm.hpp"

namespace VS::Event {
namespace {

// Operand offsets after the opcode byte, by opcode.
constexpr std::size_t kImageIdOperand = 1;
constexpr std::size_t kFirstPageOperand = 3;
constexpr std::size_t kConfigureSelectorOperand = 1;
constexpr std::size_t kConfigureXOperand = 2;
constexpr std::size_t kConfigureYOperand = 3;
constexpr std::size_t kConfigureScaleOperand = 4;
constexpr std::size_t kConfigureBrightnessOperand = 5;
constexpr std::size_t kTweenXOperand = 1;
constexpr std::size_t kTweenYOperand = 2;
constexpr std::size_t kTweenScaleOperand = 3;
constexpr std::size_t kTweenBrightnessOperand = 4;
constexpr std::size_t kTweenSelectorOperand = 5;
constexpr std::size_t kTweenDurationOperand = 6;

// The instruction's bytes when it holds all `length` of its opcode's, or
// nothing once a shorter span is refused.
[[nodiscard]] std::optional<ScriptCommand> Operands(
    const ScriptInstruction& instruction, std::size_t length,
    ScriptVmHost& host) {
  if (instruction.bytes.size() < length) {
    host.Refuse(ScriptVmRefusal::kShortInstruction,
                static_cast<int>(instruction.bytes.size()));
    return std::nullopt;
  }
  return ScriptCommand{instruction.bytes};
}

[[nodiscard]] constexpr ScriptFlow HoldWhen(bool hold) {
  return ScriptFlow::Of(hold ? ScriptFlowKind::kHold
                             : ScriptFlowKind::kAdvance);
}

[[nodiscard]] constexpr int Layer(std::uint8_t selector) {
  return selector >> kSplashLayerShift;
}

// A signed operand byte doubled, as the original's lb/sll pairs computed it.
[[nodiscard]] constexpr int Offset(std::uint8_t operand) {
  return static_cast<std::int8_t>(operand) * kSplashOffsetScale;
}

[[nodiscard]] constexpr int Scale(std::uint8_t operand) {
  return static_cast<int>(static_cast<unsigned>(operand) << kSplashScaleShift);
}

// The 16-bit difference the original stored with sh.
[[nodiscard]] constexpr std::int16_t Delta(int target, std::int16_t start) {
  return static_cast<std::int16_t>(target - start);
}

}  // namespace

// decomp: BATTLE.PRG func_800B6724 (src/BATTLE/BATTLE.PRG/4A0A8.c:609). The
// original read the set's loading bit through a missing set; the host
// answers false and reports it.
ScriptFlow ScriptWaitForSplashFile(const ScriptInstruction& /*instruction*/,
                                   ScriptVmHost& host) {
  return HoldWhen(host.SplashImageLoading());
}

// decomp: BATTLE.PRG vs_battle_script_loadGim
// (src/BATTLE/BATTLE.PRG/4A0A8.c:851). The image id halfword is sign-extended;
// the host refuses an unknown id.
ScriptFlow ScriptLoadSplashImage(const ScriptInstruction& instruction,
                                 ScriptVmHost& host) {
  if (const auto command =
          Operands(instruction, kLoadSplashImageLength, host)) {
    host.QueueSplashImage(OperandHalf(*command, kImageIdOperand),
                          OperandByte(*command, kFirstPageOperand));
  }
  return HoldWhen(false);
}

// decomp: BATTLE.PRG func_800B7014 (src/BATTLE/BATTLE.PRG/4A0A8.c:857). The
// tween stops before the layer changes, so the frame update cannot overwrite
// the new placement.
ScriptFlow ScriptConfigureSplashLayer(const ScriptInstruction& instruction,
                                      ScriptVmHost& host) {
  const auto operands =
      Operands(instruction, kConfigureSplashLayerLength, host);
  if (!operands || !host.SplashImagesPresent()) {
    return HoldWhen(false);
  }
  const ScriptCommand command = *operands;
  const std::uint8_t selector = OperandByte(command, kConfigureSelectorOperand);
  const int layer = Layer(selector);
  SplashTween tween = host.SplashImageTween(layer);
  tween.duration = 0;
  host.SetSplashImageTween(layer, tween);
  host.ConfigureSplashLayer(
      layer, static_cast<int>(selector & kSplashRoutineMask),
      {.x = Offset(OperandByte(command, kConfigureXOperand)),
       .y = Offset(OperandByte(command, kConfigureYOperand)),
       .scale = Scale(OperandByte(command, kConfigureScaleOperand)),
       .brightness = OperandByte(command, kConfigureBrightnessOperand)});
  return HoldWhen(false);
}

// decomp: BATTLE.PRG func_800B70E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:879). The
// start values are the layer's halfwords and brightness byte; the deltas
// wrap to 16 bits as the original's stores did.
ScriptFlow ScriptTweenSplashLayer(const ScriptInstruction& instruction,
                                  ScriptVmHost& host) {
  const auto operands = Operands(instruction, kTweenSplashLayerLength, host);
  if (!operands) {
    return HoldWhen(false);
  }
  const ScriptCommand command = *operands;
  const std::uint8_t selector = OperandByte(command, kTweenSelectorOperand);
  const int layer = Layer(selector);
  const std::optional<ImageLayerPlacement> placement =
      host.SplashLayerPlacement(layer);
  if (!placement.has_value()) {
    return HoldWhen(false);
  }
  if (host.SplashImageTween(layer).duration != 0) {
    return HoldWhen(true);
  }
  const auto start_x = static_cast<std::int16_t>(placement->x);
  const auto start_y = static_cast<std::int16_t>(placement->y);
  const auto start_scale = static_cast<std::int16_t>(placement->scale);
  const auto start_brightness = static_cast<std::int16_t>(
      static_cast<std::uint8_t>(placement->brightness));
  const SplashTween tween{
      .easing_mode = static_cast<std::uint8_t>(selector & kSplashEasingMask),
      .elapsed = 0,
      .duration = OperandByte(command, kTweenDurationOperand),
      .start_x = start_x,
      .start_y = start_y,
      .delta_x = Delta(Offset(OperandByte(command, kTweenXOperand)), start_x),
      .delta_y = Delta(Offset(OperandByte(command, kTweenYOperand)), start_y),
      .start_scale = start_scale,
      .delta_scale =
          Delta(Scale(OperandByte(command, kTweenScaleOperand)), start_scale),
      .start_brightness = start_brightness,
      .delta_brightness = Delta(OperandByte(command, kTweenBrightnessOperand),
                                start_brightness)};
  host.SetSplashImageTween(layer, tween);
  return HoldWhen(false);
}

// decomp: BATTLE.PRG func_800B71C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:905).
ScriptFlow ScriptFreeSplashImage(const ScriptInstruction& /*instruction*/,
                                 ScriptVmHost& host) {
  host.ReleaseSplashImages();
  return host.SkipState() == CutsceneSkip::kSkipping
             ? ScriptFlow::Of(ScriptFlowKind::kAdvanceAndYield)
             : HoldWhen(false);
}

// decomp: BATTLE.PRG func_800B71F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:911).
ScriptFlow ScriptWaitForSplashTween(const ScriptInstruction& /*instruction*/,
                                    ScriptVmHost& host) {
  for (int layer = 0; layer < static_cast<int>(kImageTweenCount); ++layer) {
    if (host.SplashImageTween(layer).duration != 0) {
      return HoldWhen(true);
    }
  }
  return HoldWhen(false);
}

}  // namespace VS::Event
