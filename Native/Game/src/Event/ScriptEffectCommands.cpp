// Hand-maintained (tools/port/owned.csv).
#include "Event/ScriptEffectCommands.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Event {
namespace {

// Operand offsets after the opcode byte.
constexpr std::size_t kFirstOperand = 1;
constexpr std::size_t kSecondOperand = 2;
constexpr std::size_t kThirdOperand = 3;
constexpr std::size_t kFourthOperand = 4;

// Actor effect selectors: the top 4 bits pick how the rest names an actor.
// A model reference resolves to slot 1 when no actor has the model.
constexpr unsigned kReferenceTypeShift = 12;
constexpr unsigned kModelReference = 0;
constexpr unsigned kSlotReference = 1;
constexpr int kModelNotFoundSlot = 1;
constexpr int kNoActor = -1;

constexpr unsigned kVibrationPatternMask = 0x7;

// Tween fractions are 4096ths; weather wind directions wrap a turn.
constexpr unsigned kFractionShift = 12;
constexpr int kDirectionMask = 0xFFF;

// Refuses an instruction shorter than its operands.
[[nodiscard]] bool Fits(ScriptCommand command, std::size_t length,
                        ScriptEffectHost& host) {
  if (command.size() >= length) {
    return true;
  }
  host.Refuse(ScriptEffectRefusal::kShortCommand,
              static_cast<int>(command.size()));
  return false;
}

[[nodiscard]] ScriptStep HoldWhile(bool busy) {
  return busy ? ScriptStep::kHold : ScriptStep::kAdvance;
}

// A release ends the thread's frame while a cutscene skip runs.
[[nodiscard]] ScriptStep AfterRelease(const ScriptEffectHost& host) {
  return host.SkipActive() ? ScriptStep::kAdvanceAndYield
                           : ScriptStep::kAdvance;
}

[[nodiscard]] bool SettledState(int state) {
  return state == kEffectStateIdle || state == kEffectStateFinished;
}

[[nodiscard]] int Eased(std::int16_t start, std::int16_t delta,
                        std::int16_t fraction) {
  return start + ((delta * fraction) >> kFractionShift);
}

}  // namespace

ScriptStep CommandPlayActorEffect(ScriptCommand command,
                                  ScriptEffectHost& host) {
  if (!Fits(command, kPlayActorEffectLength, host)) {
    return ScriptStep::kAdvance;
  }
  const int effect = OperandHalf(command, kFirstOperand);
  const auto selector =
      static_cast<std::uint16_t>(OperandHalf(command, kThirdOperand));
  const unsigned reference_type = selector >> kReferenceTypeShift;
  const int actor =
      reference_type <= kSlotReference ? host.ResolveActor(selector) : kNoActor;
  const bool in_range =
      static_cast<unsigned>(actor) < static_cast<unsigned>(kEffectActorSlots);
  const EffectActorSlot slot =
      in_range ? host.ActorSlot(actor) : EffectActorSlot{};
  // A slot reference, or a model no actor has, targets an empty slot as
  // the original did: the effect starts and its target reads are no-ops.
  const bool absent_model =
      in_range && !slot.battle_actor && !slot.occupied &&
      (reference_type == kSlotReference || actor == kModelNotFoundSlot);
  const bool unavailable =
      !in_range ||
      (reference_type == kModelReference &&
       (!slot.battle_actor || slot.model_id != selector)) ||
      !slot.occupied;
  if (!absent_model && unavailable) {
    host.ReportUnavailableActor(effect, selector, actor);
    return ScriptStep::kHold;
  }
  if (absent_model) {
    host.ReportAbsentActor(effect, selector, actor);
  }
  host.StartActorEffect(effect, actor);
  return ScriptStep::kAdvance;
}

ScriptStep CommandAdvanceEffectStage(ScriptCommand command,
                                     ScriptEffectHost& host) {
  if (Fits(command, kAdvanceEffectStageLength, host)) {
    host.ReleaseEffectTrigger(OperandByte(command, kFirstOperand));
    if (host.SkipActive()) {
      host.ClearPrimaryEffects();
    }
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandWaitEffectState(ScriptCommand command,
                                  ScriptEffectHost& host) {
  if (!Fits(command, kWaitEffectStateLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t state = OperandByte(command, kFirstOperand);
  if (!host.SkipActive() || !SettledState(state)) {
    return HoldWhile(host.EffectPlaybackState() != state);
  }
  if (!host.EffectWaitPending()) {
    host.ClearPrimaryEffects();
    host.SetEffectWaitPending(true);
    return ScriptStep::kHold;
  }
  if (host.EffectPlaybackState() != state) {
    return ScriptStep::kHold;
  }
  host.SetEffectWaitPending(false);
  return ScriptStep::kAdvance;
}

ScriptStep CommandYieldEffectRelease(ScriptCommand command,
                                     ScriptEffectHost& host) {
  if (!Fits(command, kYieldEffectReleaseLength, host)) {
    return ScriptStep::kAdvance;
  }
  return AfterRelease(host);
}

ScriptStep CommandWaitEffectContinuation(ScriptCommand command,
                                         ScriptEffectHost& host) {
  if (!Fits(command, kWaitEffectContinuationLength, host)) {
    return ScriptStep::kAdvance;
  }
  return HoldWhile(!host.EffectTriggerReady());
}

ScriptStep CommandWaitEffectRelease(ScriptCommand command,
                                    ScriptEffectHost& host) {
  if (!Fits(command, kWaitEffectReleaseLength, host)) {
    return ScriptStep::kAdvance;
  }
  if (!host.EffectWaitPending()) {
    host.ClearPrimaryEffects();
    host.SetEffectWaitPending(true);
    return ScriptStep::kHold;
  }
  if (!SettledState(host.EffectPlaybackState())) {
    return ScriptStep::kHold;
  }
  host.SetEffectWaitPending(false);
  return ScriptStep::kAdvance;
}

ScriptStep CommandMarkEffectStageReady(ScriptCommand command,
                                       ScriptEffectHost& host) {
  if (Fits(command, kMarkEffectStageReadyLength, host)) {
    host.RequeueEffectTrack(OperandByte(command, kFirstOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandPlayVibrationPattern(ScriptCommand command,
                                       ScriptEffectHost& host) {
  if (!Fits(command, kPlayVibrationPatternLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t selector = OperandByte(command, kFirstOperand);
  if (selector == kStopVibrationSelector) {
    host.StopVibration();
    return ScriptStep::kAdvance;
  }
  if ((selector & kBufferedVibrationBit) != 0) {
    // The original copied an absent pattern from a null pointer.
    const int pattern = static_cast<int>(selector & kVibrationPatternMask);
    if (!host.VibrationPatternLoaded(pattern)) {
      host.Refuse(ScriptEffectRefusal::kVibrationPattern, selector);
      return ScriptStep::kAdvance;
    }
    host.StartVibrationPattern(pattern, true);
    return ScriptStep::kAdvance;
  }
  if (selector >= kVibrationPatternCount) {
    host.Refuse(ScriptEffectRefusal::kVibrationPattern, selector);
    return ScriptStep::kAdvance;
  }
  if (host.VibrationPatternLoaded(selector)) {
    host.StartVibrationPattern(selector, false);
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandSetActorTrail(ScriptCommand command, ScriptEffectHost& host) {
  if (!Fits(command, kSetActorTrailLength, host)) {
    return ScriptStep::kAdvance;
  }
  const auto actor = static_cast<std::int16_t>(host.ResolveActor(
      static_cast<std::uint16_t>(OperandHalf(command, kSecondOperand))));
  const std::uint8_t bone = OperandByte(command, kFourthOperand);
  if (OperandByte(command, kFirstOperand) != 0) {
    host.StartWeaponTrail(actor, bone);
  } else {
    host.FadeWeaponTrails(actor, bone);
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandWaitScreenEffectLoad(ScriptCommand command,
                                       ScriptEffectHost& host) {
  if (!Fits(command, kWaitScreenEffectLoadLength, host)) {
    return ScriptStep::kAdvance;
  }
  return HoldWhile(host.ScreenEffectsLoading());
}

ScriptStep CommandSetScreenDistortion(ScriptCommand command,
                                      ScriptEffectHost& host) {
  if (Fits(command, kSetScreenDistortionLength, host)) {
    host.SetDistortionLayer(OperandHalf(command, kFirstOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandLoadScreenEffects(ScriptCommand command,
                                    ScriptEffectHost& host) {
  if (Fits(command, kLoadScreenEffectsLength, host)) {
    host.AcquireScreenEffects();
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandReservedScreenEffect(ScriptCommand command,
                                       ScriptEffectHost& host) {
  static_cast<void>(Fits(command, kReservedScreenEffectLength, host));
  return ScriptStep::kAdvance;
}

ScriptStep CommandFreeScreenEffects(ScriptCommand command,
                                    ScriptEffectHost& host) {
  if (!Fits(command, kFreeScreenEffectsLength, host)) {
    return ScriptStep::kAdvance;
  }
  host.ReleaseScreenEffects();
  return AfterRelease(host);
}

ScriptStep CommandSetScanlineOverlay(ScriptCommand command,
                                     ScriptEffectHost& host) {
  if (Fits(command, kSetScanlineOverlayLength, host)) {
    host.SetScanlineOverlay(OperandByte(command, kFirstOperand));
  }
  return ScriptStep::kAdvance;
}

std::uint16_t DrawEventScanlineOverlay(std::uint16_t frame, int width,
                                       int height, ScanlineCanvas& canvas) {
  const auto next = static_cast<std::uint16_t>(frame + 1);
  const int phase = next >> 1;
  for (int row = 0; row < height; ++row) {
    if (((row + phase) & 1) != 0) {
      canvas.DrawLine(row, width, kScanlineGrey);
    }
  }
  canvas.SetSubtractiveBlend();
  return next;
}

void UpdateEventImageTweens(std::span<SplashTween> tweens, int tick_speed,
                            ImageTweenHost& host) {
  const int step = tick_speed >> 1;
  int layer = 0;
  for (SplashTween& tween : tweens) {
    if (tween.duration != 0) {
      tween.elapsed = static_cast<std::uint8_t>(tween.elapsed + step);
      const auto fraction = static_cast<std::int16_t>(
          host.EaseFraction(tween.easing_mode, tween.elapsed, tween.duration));
      host.PlaceImageLayer(
          layer,
          ImageLayerPlacement{
              .x = Eased(tween.start_x, tween.delta_x, fraction),
              .y = Eased(tween.start_y, tween.delta_y, fraction),
              .scale = Eased(tween.start_scale, tween.delta_scale, fraction),
              .brightness = Eased(tween.start_brightness,
                                  tween.delta_brightness, fraction),
          });
      if (tween.elapsed >= tween.duration) {
        tween.duration = 0;
      }
    }
    ++layer;
  }
}

bool UpdateEventWeatherTween(WeatherTween& tween) {
  if (tween.duration == 0) {
    return false;
  }
  tween.elapsed = static_cast<std::uint8_t>(tween.elapsed + 1);
  const auto fraction = static_cast<std::int16_t>(
      (tween.elapsed << kFractionShift) / tween.duration);
  tween.wind_direction = static_cast<std::int16_t>(
      Eased(tween.start_wind_direction, tween.delta_wind_direction, fraction) &
      kDirectionMask);
  tween.wind_strength = static_cast<std::int16_t>(
      Eased(tween.start_wind_strength, tween.delta_wind_strength, fraction));
  tween.intensity = static_cast<std::int16_t>(
      Eased(tween.start_intensity, tween.delta_intensity, fraction));
  if (tween.elapsed == tween.duration) {
    tween.duration = 0;
  }
  return true;
}

}  // namespace VS::Event
