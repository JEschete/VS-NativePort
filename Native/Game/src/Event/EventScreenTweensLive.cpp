// Hand-maintained (tools/port/owned.csv).
#include "Event/EventScreenTweensLive.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

#include "Core/Log.hpp"
#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Event/EventScreenTweens.hpp"
#include "Event/ScriptCommand.hpp"
#include "Render/ScreenOverlayLive.hpp"

extern "C" {
#include "Core/Scratch.h"
#include "Event/Camera.h"
#include "Event/InterpolationCurveLive.h"
#include "Event/Screen.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
#include "Render/FramebufferEffects.h"
#include "State/Timers.h"
}

namespace VS::Event {
namespace {
// The scratchpad word (0x1F800088) func_800BDBB4 kept each eased fraction in.
constexpr std::size_t kEasedFractionScratchOffset = 0x88;

std::atomic<std::uint64_t> refused_count{0};

void Skip(const char* operation, const char* reason) {
  refused_count.fetch_add(1, std::memory_order_relaxed);
  VS_LOG_ERROR("event.camera", "{} skipped: {}", operation, reason);
}

EventAngleTween LoadAngleTween(const Event_AngleTween& tween) {
  return {.changed = static_cast<std::uint8_t>(tween.hasChanged),
          .easing = static_cast<std::uint8_t>(tween.easingMode),
          .angle = tween.angle,
          .start = tween.startAngle,
          .delta = tween.deltaAngle,
          .elapsed = tween.elapsedFrames,
          .duration = tween.durationFrames};
}

void StoreAngleTween(Event_AngleTween& tween, const EventAngleTween& value) {
  tween.hasChanged = static_cast<char>(value.changed);
  tween.easingMode = static_cast<char>(value.easing);
  tween.angle = value.angle;
  tween.startAngle = value.start;
  tween.deltaAngle = value.delta;
  tween.elapsedFrames = value.elapsed;
  tween.durationFrames = value.duration;
}

// The reserved words are left as they were.
EventScreenTween LoadScreenTween(const Event_TripleTween& tween) {
  return {.easing = static_cast<std::uint8_t>(tween.easingMode),
          .current = {tween.currentValue0, tween.currentValue1,
                      tween.currentValue2},
          .start = {tween.startValue0, tween.startValue1, tween.startValue2},
          .delta = {tween.deltaValue0, tween.deltaValue1, tween.deltaValue2},
          .elapsed = tween.elapsedFrames,
          .duration = tween.durationFrames};
}

void StoreScreenTween(Event_TripleTween& tween, const EventScreenTween& value) {
  tween.easingMode = static_cast<char>(value.easing);
  tween.currentValue0 = value.current[0];
  tween.currentValue1 = value.current[1];
  tween.currentValue2 = value.current[2];
  tween.startValue0 = value.start[0];
  tween.startValue1 = value.start[1];
  tween.startValue2 = value.start[2];
  tween.deltaValue0 = value.delta[0];
  tween.deltaValue1 = value.delta[1];
  tween.deltaValue2 = value.delta[2];
  tween.elapsedFrames = value.elapsed;
  tween.durationFrames = value.duration;
}

EventScreenTweens LoadScreenTweens(const Event_CameraState& camera) {
  return {.scale = LoadScreenTween(camera.screenScale),
          .tint = LoadScreenTween(camera.screenTint),
          .translation = LoadScreenTween(camera.screenTranslation),
          .radial_warp = LoadScreenTween(camera.screenEffectTween)};
}

void StoreScreenTweens(Event_CameraState& camera,
                       const EventScreenTweens& tweens) {
  StoreScreenTween(camera.screenScale, tweens.scale);
  StoreScreenTween(camera.screenTint, tweens.tint);
  StoreScreenTween(camera.screenTranslation, tweens.translation);
  StoreScreenTween(camera.screenEffectTween, tweens.radial_warp);
}

class LiveTweenEasing final : public EventTweenEasing {
 public:
  std::int32_t EaseFraction(std::uint8_t mode, std::int16_t frame,
                            std::int16_t duration) override {
    return Event_EvaluateInterpolationCurveEntry(mode, frame, duration);
  }

  void RecordEasedFraction(std::int32_t fraction) override {
    auto* scratch = static_cast<std::byte*>(Core_ScratchBase());
    if (scratch == nullptr) return;
    std::memcpy(scratch + kEasedFractionScratchOffset, &fraction,
                sizeof(fraction));
  }
};

// The live captured screen (Render/ScreenOverlayLive.hpp) and the screen
// overlay's radial warp.
class LiveScreenEffect final : public EventScreenEffectPort {
 public:
  void SetCaptureEnabled(std::int32_t enabled) override {
    Render::SetLiveFramebufferEffectEnabled(enabled);
  }
  void SetDepth(std::int32_t depth) override {
    Render::SetLiveFramebufferEffectDepth(static_cast<u_int>(depth));
  }
  void SetPreviousFrame(std::int32_t use_previous_frame) override {
    Render::SetLiveFramebufferPreviousFrame(use_previous_frame);
  }
  void SetScale(EventScreenPair scale) override {
    const Render_FramebufferEffectTranslation value = Translation(scale);
    Render::SetLiveFramebufferEffectScale(&value);
  }
  void SetTint(EventScreenTint tint) override {
    const CVECTOR value{
        .r = tint.red, .g = tint.green, .b = tint.blue, .cd = 0};
    Render::SetLiveFramebufferEffectTint(&value);
  }
  void SetOffset(EventScreenPair offset) override {
    const Render_FramebufferEffectTranslation value = Translation(offset);
    Render::SetLiveFramebufferEffectOffset(&value);
  }
  void SetBlendMode(std::int32_t blend_mode) override {
    Render::SetLiveFramebufferEffectBlendMode(blend_mode);
  }
  EventScreenPair ReadScale() override {
    Render_FramebufferEffectTranslation value{};
    Render::ReadLiveFramebufferEffectScale(&value);
    return {.x = value.x, .y = value.y};
  }
  EventScreenTint ReadTint() override {
    CVECTOR value{};
    Render::ReadLiveFramebufferEffectTint(&value);
    return {.red = value.r, .green = value.g, .blue = value.b};
  }
  EventScreenPair ReadOffset() override {
    Render_FramebufferEffectTranslation value{};
    Render::ReadLiveFramebufferEffectOffset(&value);
    return {.x = value.x, .y = value.y};
  }
  void SetRadialWarp(
      Effects::ScreenDistortion::RadialWarpSettings settings) override {
    Effects::ScreenDistortion::SetLiveRadialWarp(settings);
  }

 private:
  static Render_FramebufferEffectTranslation Translation(EventScreenPair pair) {
    return {.x = pair.x, .y = pair.y, .reserved8 = 0, .reservedC = 0};
  }
};

Event_CameraState* LiveCamera(const char* operation) {
  if (Event_camera == nullptr) Skip(operation, "missing event camera");
  return Event_camera;
}
}  // namespace

// decomp: BATTLE.PRG func_800BD57C (F01227, src/BATTLE/BATTLE.PRG/4A0A8.c:3647,
// 0x800BD57C-0x800BD610)
void UpdateEventAngleTweenEntry(Event_AngleTween* tween) {
  if (tween == nullptr) {
    Skip("UpdateEventAngleTween", "missing tween");
    return;
  }
  LiveTweenEasing easing;
  EventAngleTween value = LoadAngleTween(*tween);
  UpdateEventAngleTween(value, State_tickSpeed, easing);
  StoreAngleTween(*tween, value);
}

// decomp: BATTLE.PRG func_800BDAB4 (F01230, src/BATTLE/BATTLE.PRG/4A0A8.c:3812,
// 0x800BDAB4-0x800BDBB4)
void UpdateEventScreenTransformsEntry() {
  Event_CameraState* camera = LiveCamera("UpdateEventScreenTransforms");
  if (camera == nullptr) return;
  LiveTweenEasing easing;
  LiveScreenEffect screen;
  EventScreenTweens tweens = LoadScreenTweens(*camera);
  UpdateEventScreenTransforms(tweens, State_tickSpeed, easing, screen);
  StoreScreenTweens(*camera, tweens);
}

// decomp: BATTLE.PRG func_800BDBB4 (F01231, src/BATTLE/BATTLE.PRG/4A0A8.c:3838,
// 0x800BDBB4-0x800BDC9C)
int UpdateEventTripleTweenEntry(Event_TripleTween* tween) {
  if (tween == nullptr) {
    Skip("UpdateEventTripleTween", "missing tween");
    return 0;
  }
  LiveTweenEasing easing;
  EventScreenTween value = LoadScreenTween(*tween);
  const std::optional<std::int32_t> eased =
      UpdateEventTripleTween(value, State_tickSpeed, easing);
  StoreScreenTween(*tween, value);
  return eased.has_value() ? 1 : 0;
}

std::uint64_t RefusedEventScreenTweenCount() noexcept {
  return refused_count.load(std::memory_order_relaxed);
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800BB450 (F01198, src/BATTLE/BATTLE.PRG/4A0A8.c:2793,
// 0x800BB450-0x800BB474), opcode 0xE1.
std::intptr_t Event_CommandSetScreenCaptureOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  if (instruction == nullptr) {
    event::Skip("CommandSetScreenCapture", "missing instruction");
    return 0;
  }
  event::LiveScreenEffect screen;
  static_cast<void>(event::CommandSetScreenCapture(
      event::ScriptCommand{instruction, event::kSetScreenCaptureLength},
      screen));
  return 0;
}

// decomp: BATTLE.PRG func_800BD444 (F01226, src/BATTLE/BATTLE.PRG/4A0A8.c:3601,
// 0x800BD444-0x800BD57C), opcodes 0xE2 and 0xE3.
std::intptr_t Event_CommandTweenCameraRollOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  constexpr const char* kOperation = "CommandTweenCameraRoll";
  if (instruction == nullptr) {
    event::Skip(kOperation, "missing instruction");
    return 0;
  }
  Event_CameraState* camera = event::LiveCamera(kOperation);
  if (camera == nullptr) return 0;
  event::EventAngleTween camera_roll =
      event::LoadAngleTween(camera->cameraRoll);
  event::EventAngleTween screen_roll =
      event::LoadAngleTween(camera->capturedScreenRoll);
  if (!event::CommandTweenCameraRoll(
          event::ScriptCommand{instruction, event::kTweenRollLength},
          camera_roll, screen_roll)) {
    event::Skip(kOperation, "not a roll opcode");
    return 0;
  }
  event::StoreAngleTween(camera->cameraRoll, camera_roll);
  event::StoreAngleTween(camera->capturedScreenRoll, screen_roll);
  return 0;
}

// decomp: BATTLE.PRG func_800BD6C4 (F01229, src/BATTLE/BATTLE.PRG/4A0A8.c:3691,
// 0x800BD6C4-0x800BDAB4), opcodes 0xB9, 0xBA, 0xBF, 0xE4-0xE7 and 0xED.
std::intptr_t Event_CommandScreenTransformOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  constexpr const char* kOperation = "CommandScreenTransform";
  if (instruction == nullptr) {
    event::Skip(kOperation, "missing instruction");
    return 0;
  }
  const std::optional<std::size_t> length =
      event::ScreenTransformLength(instruction[0]);
  if (!length.has_value()) {
    event::Skip(kOperation, "not a screen transform opcode");
    return 0;
  }
  Event_CameraState* camera = event::LiveCamera(kOperation);
  if (camera == nullptr) return 0;
  event::LiveScreenEffect screen;
  event::EventScreenTweens tweens = event::LoadScreenTweens(*camera);
  const std::optional<std::int32_t> result = event::CommandScreenTransform(
      event::ScriptCommand{instruction, *length}, tweens, screen);
  event::StoreScreenTweens(*camera, tweens);
  return result.value_or(0);
}
