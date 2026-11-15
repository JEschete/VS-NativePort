// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraLens.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {
namespace {
using Core::Math::Position3;
using Core::Math::R3000Quotient;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;
using Core::Math::WrapSigned16;

constexpr std::size_t kTargetOperand = 1;
constexpr std::size_t kEasingOperand = 2;
constexpr std::size_t kDurationOperand = 3;
constexpr std::size_t kCyclesOperand = 1;
constexpr std::size_t kAmplitudeOperand = 2;
constexpr std::size_t kAxisMaskOperand = 3;
constexpr std::size_t kEnvelopeOperand = 4;
constexpr std::size_t kShakeDurationOperand = 5;
// The fall-through return values of an opcode the handler does not serve.
constexpr std::intptr_t kUnservedBelowFarClip = 0xEA;
constexpr std::intptr_t kUnservedFromFarClip = 0xEF;
constexpr int kFixedShift = 12;
constexpr int kQ12One = 0x1000;
constexpr int kQuarterTurnShift = 10;
constexpr int kHalfTurnShift = 11;
constexpr int kQuarterTurn = 0x400;
constexpr unsigned kAxisFieldMask = 3;
constexpr unsigned kXAxisShift = 2;
constexpr unsigned kYAxisShift = 4;
constexpr unsigned kZAxisShift = 6;

// How a lens opcode turns its target byte into a value.
struct LensRule {
  int target_shift;
  int default_target;
  // The projection distance and far clip keep the old start value when the
  // target byte is zero.
  bool start_needs_target;
};

constexpr LensRule kProjectionDistanceRule{2, 0x200, true};
constexpr LensRule kNearClipRule{0, 0x40, false};
constexpr LensRule kFarClipRule{7, 0x1000, true};

void CommandLens(ScriptCommand command, const LensRule& rule,
                 EventLensTween& tween) {
  const std::uint8_t target = OperandByte(command, kTargetOperand);
  const int value = target == 0 ? rule.default_target
                                : static_cast<int>(target) << rule.target_shift;
  tween.duration_frames = OperandByte(command, kDurationOperand);
  if (tween.duration_frames == 0) {
    tween.value = WrapSigned16(static_cast<std::uint32_t>(value));
    tween.changed = 1;
    return;
  }
  if (target != 0 || !rule.start_needs_target) {
    tween.start_value = tween.value;
    tween.delta_value = WrapSigned16(
        static_cast<std::uint32_t>(WrappingSubtract(value, tween.start_value)));
  } else {
    tween.delta_value = WrapSigned16(static_cast<std::uint32_t>(value));
  }
  tween.easing_mode = OperandByte(command, kEasingOperand);
  tween.elapsed_frames = 0;
}

void CommandShake(ScriptCommand command, EventCameraShake& shake) {
  const std::uint8_t amplitude = OperandByte(command, kAmplitudeOperand);
  if (amplitude == 0) {
    for (EventShakeOscillator& oscillator : shake.oscillators) {
      oscillator.duration_frames = 0;
    }
    return;
  }
  for (EventShakeOscillator& oscillator : shake.oscillators) {
    if (oscillator.duration_frames != 0) continue;
    oscillator.cycles = OperandByte(command, kCyclesOperand);
    oscillator.amplitude = amplitude;
    oscillator.axis_mask = OperandByte(command, kAxisMaskOperand);
    oscillator.envelope = OperandByte(command, kEnvelopeOperand);
    oscillator.duration_frames = OperandByte(command, kShakeDurationOperand);
    oscillator.elapsed_frames = 0;
    return;
  }
}

// The elapsed frames after one tick: half the tick speed, stored in a byte and
// clamped to the duration.
std::uint8_t AdvancedFrames(std::uint8_t elapsed, std::uint8_t duration,
                            int tick_speed) {
  const auto advanced =
      static_cast<std::uint8_t>(WrappingAdd(elapsed, tick_speed >> 1));
  return duration < advanced ? duration : advanced;
}

std::int32_t Envelope(std::uint8_t shape, int elapsed, int duration,
                      std::int32_t previous, EventCameraLensHost& host) {
  switch (shape) {
    case kShakeEnvelopeFlat:
      return kQ12One;
    case kShakeEnvelopeFadeOut:
      return host.Sine(R3000Quotient(elapsed << kQuarterTurnShift, duration) +
                       kQuarterTurn);
    case kShakeEnvelopeSwell:
      return host.Sine(R3000Quotient(elapsed << kHalfTurnShift, duration));
    case kShakeEnvelopeFadeIn:
      return host.Sine(R3000Quotient(elapsed << kQuarterTurnShift, duration));
    default:
      return previous;
  }
}

// An axis factor from its two-bit field: the field minus one.
std::int32_t AxisFactor(std::uint8_t axis_mask, unsigned shift) {
  return static_cast<std::int32_t>((static_cast<unsigned>(axis_mask) >> shift) &
                                   kAxisFieldMask) -
         1;
}

std::int16_t WrappingSum(std::int16_t left, std::int16_t right) {
  return WrapSigned16(static_cast<std::uint32_t>(WrappingAdd(left, right)));
}

Position3 AddPositions(Position3 left, Position3 right) {
  return {.x = WrappingSum(left.x, right.x),
          .y = WrappingSum(left.y, right.y),
          .z = WrappingSum(left.z, right.z)};
}
}  // namespace

// decomp: BATTLE.PRG func_800BDC9C (F01232, src/BATTLE/BATTLE.PRG/4A0A8.c:3862,
// 0x800BDC9C-0x800BDF6C)
std::optional<std::intptr_t> CommandCameraLensOrShake(ScriptCommand command,
                                                      EventCameraLenses& lenses,
                                                      EventCameraShake& shake) {
  if (command.empty()) return std::nullopt;
  const std::uint8_t opcode = command.front();
  switch (opcode) {
    case kOpcodeTweenProjectionDistance:
    case kOpcodeTweenNearClip:
    case kOpcodeTweenFarClip:
      if (command.size() < kCameraLensCommandLength) return std::nullopt;
      if (opcode == kOpcodeTweenProjectionDistance) {
        CommandLens(command, kProjectionDistanceRule,
                    lenses.projection_distance);
      } else if (opcode == kOpcodeTweenNearClip) {
        CommandLens(command, kNearClipRule, lenses.near_clip);
      } else {
        CommandLens(command, kFarClipRule, lenses.far_clip);
      }
      return 0;
    case kOpcodeStartCameraShake:
      if (command.size() < kCameraShakeCommandLength) return std::nullopt;
      CommandShake(command, shake);
      return 0;
    default:
      return opcode < kOpcodeTweenFarClip ? kUnservedBelowFarClip
                                          : kUnservedFromFarClip;
  }
}

// decomp: BATTLE.PRG func_800BDF6C (F01233, src/BATTLE/BATTLE.PRG/4A0A8.c:3927,
// 0x800BDF6C-0x800BE01C)
void UpdateEventLensTween(EventLensTween& tween, std::int32_t& fraction_scratch,
                          EventCameraLensHost& host) {
  if (tween.duration_frames == 0) return;
  tween.elapsed_frames = AdvancedFrames(
      tween.elapsed_frames, tween.duration_frames, host.TickSpeed());
  fraction_scratch = host.EaseFraction(tween.easing_mode, tween.elapsed_frames,
                                       tween.duration_frames);
  const std::int32_t step =
      WrappingMultiply(tween.delta_value, fraction_scratch) >> kFixedShift;
  tween.value = WrapSigned16(
      static_cast<std::uint32_t>(WrappingAdd(tween.start_value, step)));
  if (tween.elapsed_frames == tween.duration_frames) tween.duration_frames = 0;
  tween.changed = 1;
}

// decomp: BATTLE.PRG func_800BE01C (F01234, src/BATTLE/BATTLE.PRG/4A0A8.c:3945,
// 0x800BE01C-0x800BE180)
bool UpdateEventShakeOscillator(EventShakeOscillator& oscillator,
                                std::int32_t& envelope_scratch,
                                EventCameraLensHost& host) {
  if (oscillator.duration_frames == 0) return false;
  const std::int32_t total_phase = oscillator.cycles * kQ12One;
  oscillator.elapsed_frames = AdvancedFrames(
      oscillator.elapsed_frames, oscillator.duration_frames, host.TickSpeed());
  const int elapsed = oscillator.elapsed_frames;
  const int duration = oscillator.duration_frames;
  envelope_scratch =
      Envelope(oscillator.envelope, elapsed, duration, envelope_scratch, host);
  const int wave = host.Sine(
      R3000Quotient(WrappingMultiply(total_phase, elapsed), duration));
  const std::int32_t swing =
      WrappingMultiply(wave, oscillator.amplitude) >> kFixedShift;
  oscillator.displacement = WrapSigned16(static_cast<std::uint32_t>(
      WrappingMultiply(swing, envelope_scratch) >> kFixedShift));
  if (oscillator.elapsed_frames == oscillator.duration_frames) {
    oscillator.duration_frames = 0;
  }
  return true;
}

// decomp: BATTLE.PRG func_800BE180 (F01235, src/BATTLE/BATTLE.PRG/4A0A8.c:3983,
// 0x800BE180-0x800BE36C)
void UpdateEventCameraShake(EventCameraShake& shake,
                            EventCameraScratch& scratch,
                            EventCameraLensHost& host) {
  scratch.eye_shake = {};
  scratch.look_at_shake = {};
  std::int16_t active = 0;
  for (EventShakeOscillator& oscillator : shake.oscillators) {
    if (!UpdateEventShakeOscillator(oscillator, scratch.envelope, host)) {
      continue;
    }
    const std::int32_t displacement = oscillator.displacement;
    const std::uint8_t mask = oscillator.axis_mask;
    const auto axis = [&](unsigned shift) {
      return WrapSigned16(static_cast<std::uint32_t>(
          WrappingMultiply(displacement, AxisFactor(mask, shift))));
    };
    scratch.axis_shake = {
        .x = axis(kXAxisShift), .y = axis(kYAxisShift), .z = axis(kZAxisShift)};
    if ((mask & kShakeAxisEye) != 0) {
      scratch.eye_shake = AddPositions(scratch.eye_shake, scratch.axis_shake);
    }
    if ((mask & kShakeAxisLookAt) != 0) {
      scratch.look_at_shake =
          AddPositions(scratch.look_at_shake, scratch.axis_shake);
    }
    ++active;
  }
  shake.active_count = active;
  if (active != 0) {
    shake.eye_offset = scratch.eye_shake;
    shake.look_at_offset = scratch.look_at_shake;
  }
}

}  // namespace VS::Event
