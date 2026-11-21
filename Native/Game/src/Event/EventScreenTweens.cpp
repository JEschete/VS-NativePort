// Hand-maintained (tools/port/owned.csv).
#include "Event/EventScreenTweens.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Event {
namespace {
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;

constexpr std::int32_t kHalfTurn = 0x800;
constexpr unsigned kFractionBits = 12;
constexpr unsigned kDirectionShift = 4;
constexpr std::uint8_t kEasingMask = 0xF;
constexpr std::uint8_t kNegativeTurn = 0;
constexpr std::uint8_t kPositiveTurn = 2;
constexpr unsigned kScaleShift = 7;
constexpr unsigned kWarpScaleShift = 6;
constexpr std::int32_t kUnitScale = 0x1000;
constexpr std::uint8_t kNeutralTint = 0x80;
constexpr std::uint8_t kCaptureTint = 0x90;
constexpr std::int32_t kCaptureOffsetY = 2;
// Operand byte offsets within the transform and roll instructions.
constexpr std::size_t kOperand3 = 3;
constexpr std::size_t kOperand4 = 4;
constexpr std::size_t kOperand5 = 5;
constexpr std::size_t kTwoByteLength = 2;
constexpr std::size_t kThreeByteLength = 3;
constexpr std::size_t kFiveByteLength = 5;
constexpr std::size_t kSixByteLength = 6;

std::int16_t Half(std::int32_t value) {
  return static_cast<std::int16_t>(value);
}

std::int32_t SignedByte(ScriptCommand command, std::size_t offset) {
  return static_cast<std::int8_t>(OperandByte(command, offset));
}

// The original added the halfword elapsed time to half the tick speed (sra).
std::int16_t Advanced(std::int16_t elapsed, std::int32_t tick_speed) {
  return Half(WrappingAdd(elapsed, tick_speed >> 1));
}

// start + delta * eased >> 12, kept to the stored halfword.
std::int16_t Eased(std::int16_t start, std::int16_t delta, std::int32_t eased) {
  return Half(
      WrappingAdd(start, WrappingMultiply(delta, eased) >> kFractionBits));
}

// Starts `tween` from `start` towards `target` on the values it lists.
void StartScreenTween(EventScreenTween& tween, std::uint8_t easing,
                      std::span<const std::int32_t> start,
                      std::span<const std::int32_t> target) {
  tween.elapsed = 0;
  for (std::size_t index = 0; index < start.size(); ++index) {
    tween.start.at(index) = Half(start[index]);
    tween.delta.at(index) = Half(WrappingSubtract(target[index], start[index]));
  }
  tween.easing = easing;
}

void TweenScale(ScriptCommand command, EventScreenTween& tween,
                EventScreenEffectPort& screen) {
  tween.duration = OperandByte(command, kOperand4);
  const std::array<std::int32_t, 2> target = {
      static_cast<std::int32_t>(OperandByte(command, 1)) << kScaleShift,
      static_cast<std::int32_t>(OperandByte(command, 2)) << kScaleShift};
  if (tween.duration == 0) {
    screen.SetScale({.x = target[0], .y = target[1]});
    return;
  }
  const EventScreenPair scale = screen.ReadScale();
  const std::array<std::int32_t, 2> start = {scale.x, scale.y};
  StartScreenTween(tween, OperandByte(command, kOperand3), start, target);
}

void TweenTint(ScriptCommand command, EventScreenTween& tween,
               EventScreenEffectPort& screen) {
  tween.duration = OperandByte(command, kOperand5);
  if (tween.duration == 0) {
    screen.SetTint({.red = OperandByte(command, 1),
                    .green = OperandByte(command, 2),
                    .blue = OperandByte(command, kOperand3)});
    return;
  }
  const EventScreenTint tint = screen.ReadTint();
  const std::array<std::int32_t, kScreenTweenValues> start = {
      tint.red, tint.green, tint.blue};
  const std::array<std::int32_t, kScreenTweenValues> target = {
      OperandByte(command, 1), OperandByte(command, 2),
      OperandByte(command, kOperand3)};
  StartScreenTween(tween, OperandByte(command, kOperand4), start, target);
}

void TweenOffset(ScriptCommand command, EventScreenTween& tween,
                 EventScreenEffectPort& screen) {
  tween.duration = OperandByte(command, kOperand4);
  const std::array<std::int32_t, 2> target = {SignedByte(command, 1),
                                              SignedByte(command, 2)};
  if (tween.duration == 0) {
    screen.SetOffset({.x = target[0], .y = target[1]});
    return;
  }
  const EventScreenPair offset = screen.ReadOffset();
  const std::array<std::int32_t, 2> start = {offset.x, offset.y};
  StartScreenTween(tween, OperandByte(command, kOperand3), start, target);
}

// Waits (1) while a radial warp tween runs; the tween starts from the warp
// it last set.
std::int32_t TweenRadialWarp(ScriptCommand command, EventScreenTween& tween,
                             EventScreenEffectPort& screen) {
  if (tween.duration > 0) return 1;
  tween.duration = OperandByte(command, kOperand4);
  const std::array<std::int32_t, 2> target = {
      static_cast<std::int32_t>(OperandByte(command, 1)) << kWarpScaleShift,
      SignedByte(command, 2)};
  if (tween.duration == 0) {
    tween.current[0] = Half(target[0]);
    tween.current[1] = Half(target[1]);
    screen.SetRadialWarp({.scale = static_cast<std::uint16_t>(tween.current[0]),
                          .growth = tween.current[1]});
    return 0;
  }
  const std::array<std::int32_t, 2> start = {tween.current[0],
                                             tween.current[1]};
  StartScreenTween(tween, OperandByte(command, kOperand3), start, target);
  return 0;
}

// Mode 0 restores the plain screen; any other mode captures the previous
// frame, tinted and offset, at ordering depth `mode`.
void ConfigureCapture(std::int16_t mode, EventScreenEffectPort& screen) {
  if (mode == 0) {
    screen.SetBlendMode(0);
    screen.SetOffset({});
    screen.SetScale({.x = kUnitScale, .y = kUnitScale});
    screen.SetTint(
        {.red = kNeutralTint, .green = kNeutralTint, .blue = kNeutralTint});
    screen.SetDepth(0);
    screen.SetPreviousFrame(0);
    screen.SetCaptureEnabled(0);
    return;
  }
  screen.SetCaptureEnabled(1);
  screen.SetBlendMode(0);
  screen.SetOffset({.x = 0, .y = kCaptureOffsetY});
  screen.SetScale({.x = kUnitScale, .y = kUnitScale});
  screen.SetPreviousFrame(1);
  screen.SetTint(
      {.red = kCaptureTint, .green = kCaptureTint, .blue = kCaptureTint});
  screen.SetDepth(mode);
}
}  // namespace

std::optional<std::size_t> ScreenTransformLength(std::uint8_t opcode) noexcept {
  switch (static_cast<ScreenTransformOpcode>(opcode)) {
    case ScreenTransformOpcode::kSetPreviousFrame:
    case ScreenTransformOpcode::kSetBlendMode:
      return kTwoByteLength;
    case ScreenTransformOpcode::kSetDepth:
    case ScreenTransformOpcode::kConfigureCapture:
      return kThreeByteLength;
    case ScreenTransformOpcode::kTweenScale:
    case ScreenTransformOpcode::kTweenOffset:
    case ScreenTransformOpcode::kTweenRadialWarp:
      return kFiveByteLength;
    case ScreenTransformOpcode::kTweenTint:
      return kSixByteLength;
  }
  return std::nullopt;
}

// decomp: BATTLE.PRG vs_battle_script_E1 (F01198,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3442; was func_800BB450).
bool CommandSetScreenCapture(ScriptCommand command,
                             EventScreenEffectPort& screen) {
  if (command.size() < kSetScreenCaptureLength) return false;
  screen.SetCaptureEnabled(OperandByte(command, 1));
  return true;
}

// decomp: BATTLE.PRG vs_battle_script_E2_E3_camera (F01226,
// src/BATTLE/BATTLE.PRG/4DEA8.c:4251; was func_800BD444).
bool CommandTweenCameraRoll(ScriptCommand command, EventAngleTween& camera_roll,
                            EventAngleTween& screen_roll) {
  if (command.size() < kTweenRollLength) return false;
  const std::uint8_t opcode = OperandByte(command, 0);
  if (opcode != kTweenCameraRollOpcode && opcode != kTweenScreenRollOpcode) {
    return false;
  }
  EventAngleTween& tween =
      opcode == kTweenCameraRollOpcode ? camera_roll : screen_roll;
  tween.duration = OperandByte(command, kOperand4);
  const std::int16_t target = OperandHalf(command, 1);
  if (tween.duration == 0) {
    tween.angle = target;
    tween.changed = 1;
    return true;
  }
  tween.start = tween.angle;
  std::int16_t delta = Half(WrappingSubtract(target, tween.angle));
  if (delta < -kHalfTurn) {
    delta = Half(delta + kFullTurn);
  } else if (delta > kHalfTurn) {
    delta = Half(delta - kFullTurn);
  }
  const std::uint8_t control = OperandByte(command, kOperand3);
  const auto direction = static_cast<std::uint8_t>(control >> kDirectionShift);
  if (direction == kNegativeTurn && delta > 0) {
    delta = Half(delta - kFullTurn);
  } else if (direction == kPositiveTurn && delta < 0) {
    delta = Half(delta + kFullTurn);
  }
  tween.delta = delta;
  tween.easing = static_cast<std::uint8_t>(control & kEasingMask);
  tween.elapsed = 0;
  return true;
}

// decomp: BATTLE.PRG func_800BD57C (F01227,
// src/BATTLE/BATTLE.PRG/4DEA8.c:4297).
void UpdateEventAngleTween(EventAngleTween& tween, std::int32_t tick_speed,
                           EventTweenEasing& easing) {
  if (tween.duration <= 0) return;
  tween.elapsed = Advanced(tween.elapsed, tick_speed);
  const std::int32_t eased =
      easing.EaseFraction(tween.easing, tween.elapsed, tween.duration);
  tween.angle = Eased(tween.start, tween.delta, eased);
  if (tween.elapsed >= tween.duration) tween.duration = 0;
  tween.changed = 1;
}

// decomp: BATTLE.PRG vs_battle_script_B9_BA_BF_E4_E5_E6_E7_ED (F01229,
// src/BATTLE/BATTLE.PRG/4DEA8.c:4341; was func_800BD6C4).
std::optional<std::int32_t> CommandScreenTransform(
    ScriptCommand command, EventScreenTweens& tweens,
    EventScreenEffectPort& screen) {
  if (command.empty()) return std::nullopt;
  const std::uint8_t opcode = OperandByte(command, 0);
  const std::optional<std::size_t> length = ScreenTransformLength(opcode);
  if (!length.has_value() || command.size() < *length) return std::nullopt;
  switch (static_cast<ScreenTransformOpcode>(opcode)) {
    case ScreenTransformOpcode::kSetDepth:
      screen.SetDepth(OperandHalf(command, 1));
      break;
    case ScreenTransformOpcode::kSetPreviousFrame:
      screen.SetPreviousFrame(OperandByte(command, 1));
      break;
    case ScreenTransformOpcode::kTweenScale:
      TweenScale(command, tweens.scale, screen);
      break;
    case ScreenTransformOpcode::kTweenTint:
      TweenTint(command, tweens.tint, screen);
      break;
    case ScreenTransformOpcode::kTweenOffset:
      TweenOffset(command, tweens.translation, screen);
      break;
    case ScreenTransformOpcode::kSetBlendMode:
      screen.SetBlendMode(OperandByte(command, 1));
      break;
    case ScreenTransformOpcode::kTweenRadialWarp:
      return TweenRadialWarp(command, tweens.radial_warp, screen);
    case ScreenTransformOpcode::kConfigureCapture:
      ConfigureCapture(OperandHalf(command, 1), screen);
      break;
  }
  return 0;
}

// decomp: BATTLE.PRG func_800BDBB4 (F01231,
// src/BATTLE/BATTLE.PRG/4DEA8.c:4488).
std::optional<std::int32_t> UpdateEventTripleTween(EventScreenTween& tween,
                                                   std::int32_t tick_speed,
                                                   EventTweenEasing& easing) {
  if (tween.duration == 0) return std::nullopt;
  tween.elapsed = Advanced(tween.elapsed, tick_speed);
  const std::int32_t eased =
      easing.EaseFraction(tween.easing, tween.elapsed, tween.duration);
  easing.RecordEasedFraction(eased);
  for (std::size_t index = 0; index < tween.current.size(); ++index) {
    tween.current.at(index) =
        Eased(tween.start.at(index), tween.delta.at(index), eased);
  }
  if (tween.elapsed == tween.duration) tween.duration = 0;
  return eased;
}

// decomp: BATTLE.PRG func_800BDAB4 (F01230,
// src/BATTLE/BATTLE.PRG/4DEA8.c:4462).
void UpdateEventScreenTransforms(EventScreenTweens& tweens,
                                 std::int32_t tick_speed,
                                 EventTweenEasing& easing,
                                 EventScreenEffectPort& screen) {
  if (UpdateEventTripleTween(tweens.scale, tick_speed, easing).has_value()) {
    screen.SetScale(
        {.x = tweens.scale.current[0], .y = tweens.scale.current[1]});
  }
  if (UpdateEventTripleTween(tweens.tint, tick_speed, easing).has_value()) {
    const auto& current = tweens.tint.current;
    screen.SetTint({.red = static_cast<std::uint8_t>(current[0]),
                    .green = static_cast<std::uint8_t>(current[1]),
                    .blue = static_cast<std::uint8_t>(current[2])});
  }
  if (UpdateEventTripleTween(tweens.translation, tick_speed, easing)
          .has_value()) {
    screen.SetOffset({.x = tweens.translation.current[0],
                      .y = tweens.translation.current[1]});
  }
  if (UpdateEventTripleTween(tweens.radial_warp, tick_speed, easing)
          .has_value()) {
    screen.SetRadialWarp(
        {.scale = static_cast<std::uint16_t>(tweens.radial_warp.current[0]),
         .growth = tweens.radial_warp.current[1]});
  }
}
}  // namespace VS::Event
