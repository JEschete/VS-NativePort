// Hand-maintained (tools/port/owned.csv).
#include "Event/EventScreenTweensLive.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

#include "Core/Log.hpp"
#include "Core/Scratch.hpp"
#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Event/Camera.hpp"
#include "Event/EventScreenTweens.hpp"
#include "Event/InterpolationCurveLive.hpp"
#include "Event/ScriptCamera.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Render/FramebufferEffects.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::CVECTOR;

namespace VS::Event {
namespace {
// The scratchpad word (0x1F800088) func_800BDBB4 kept each eased fraction in.
constexpr std::size_t kEasedFractionScratchOffset = 0x88;

std::atomic<std::uint64_t> refused_count{0};

void Skip(const char* operation, const char* reason) {
  refused_count.fetch_add(1, std::memory_order_relaxed);
  VS::Core::LogError("event.camera", "{} skipped: {}", operation, reason);
}

EventAngleTween LoadAngleTween(const AngleTween& tween) {
  return {.changed = static_cast<std::uint8_t>(tween.has_changed),
          .easing = static_cast<std::uint8_t>(tween.easing_mode),
          .angle = tween.angle,
          .start = tween.start_angle,
          .delta = tween.delta_angle,
          .elapsed = tween.elapsed_frames,
          .duration = tween.duration_frames};
}

void StoreAngleTween(AngleTween& tween, const EventAngleTween& value) {
  tween.has_changed = static_cast<char>(value.changed);
  tween.easing_mode = static_cast<char>(value.easing);
  tween.angle = value.angle;
  tween.start_angle = value.start;
  tween.delta_angle = value.delta;
  tween.elapsed_frames = value.elapsed;
  tween.duration_frames = value.duration;
}

// The reserved words are left as they were.
EventScreenTween LoadScreenTween(const TripleTween& tween) {
  return {.easing = static_cast<std::uint8_t>(tween.easing_mode),
          .current = {tween.current_value0, tween.current_value1,
                      tween.current_value2},
          .start = {tween.start_value0, tween.start_value1, tween.start_value2},
          .delta = {tween.delta_value0, tween.delta_value1, tween.delta_value2},
          .elapsed = tween.elapsed_frames,
          .duration = tween.duration_frames};
}

void StoreScreenTween(TripleTween& tween, const EventScreenTween& value) {
  tween.easing_mode = static_cast<char>(value.easing);
  tween.current_value0 = value.current[0];
  tween.current_value1 = value.current[1];
  tween.current_value2 = value.current[2];
  tween.start_value0 = value.start[0];
  tween.start_value1 = value.start[1];
  tween.start_value2 = value.start[2];
  tween.delta_value0 = value.delta[0];
  tween.delta_value1 = value.delta[1];
  tween.delta_value2 = value.delta[2];
  tween.elapsed_frames = value.elapsed;
  tween.duration_frames = value.duration;
}

EventScreenTweens LoadScreenTweens(const CameraState& camera) {
  return {.scale = LoadScreenTween(camera.screen_scale),
          .tint = LoadScreenTween(camera.screen_tint),
          .translation = LoadScreenTween(camera.screen_translation),
          .radial_warp = LoadScreenTween(camera.screen_effect_tween)};
}

void StoreScreenTweens(CameraState& camera, const EventScreenTweens& tweens) {
  StoreScreenTween(camera.screen_scale, tweens.scale);
  StoreScreenTween(camera.screen_tint, tweens.tint);
  StoreScreenTween(camera.screen_translation, tweens.translation);
  StoreScreenTween(camera.screen_effect_tween, tweens.radial_warp);
}

class LiveTweenEasing final : public EventTweenEasing {
 public:
  std::int32_t EaseFraction(std::uint8_t mode, std::int16_t frame,
                            std::int16_t duration) override {
    return EvaluateInterpolationCurveEntry(mode, frame, duration);
  }

  void RecordEasedFraction(std::int32_t fraction) override {
    auto* scratch = static_cast<std::byte*>(VS::Core::ScratchBase());
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
    Render::SetLiveFramebufferEffectDepth(static_cast<std::uint32_t>(depth));
  }
  void SetPreviousFrame(std::int32_t use_previous_frame) override {
    Render::SetLiveFramebufferPreviousFrame(use_previous_frame);
  }
  void SetScale(EventScreenPair scale) override {
    const VS::Render::FramebufferEffectTranslation value = Translation(scale);
    Render::SetLiveFramebufferEffectScale(&value);
  }
  void SetTint(EventScreenTint tint) override {
    const CVECTOR value{
        .r = tint.red, .g = tint.green, .b = tint.blue, .cd = 0};
    Render::SetLiveFramebufferEffectTint(&value);
  }
  void SetOffset(EventScreenPair offset) override {
    const VS::Render::FramebufferEffectTranslation value = Translation(offset);
    Render::SetLiveFramebufferEffectOffset(&value);
  }
  void SetBlendMode(std::int32_t blend_mode) override {
    Render::SetLiveFramebufferEffectBlendMode(blend_mode);
  }
  EventScreenPair ReadScale() override {
    VS::Render::FramebufferEffectTranslation value{};
    Render::ReadLiveFramebufferEffectScale(&value);
    return {.x = value.x, .y = value.y};
  }
  EventScreenTint ReadTint() override {
    CVECTOR value{};
    Render::ReadLiveFramebufferEffectTint(&value);
    return {.red = value.r, .green = value.g, .blue = value.b};
  }
  EventScreenPair ReadOffset() override {
    VS::Render::FramebufferEffectTranslation value{};
    Render::ReadLiveFramebufferEffectOffset(&value);
    return {.x = value.x, .y = value.y};
  }
  void SetRadialWarp(
      Effects::ScreenDistortion::RadialWarpSettings settings) override {
    Effects::ScreenDistortion::SetLiveRadialWarp(settings);
  }

 private:
  static VS::Render::FramebufferEffectTranslation Translation(
      EventScreenPair pair) {
    return {.x = pair.x, .y = pair.y, .reserved08 = 0, .reserved0c = 0};
  }
};

CameraState* LiveCamera(const char* operation) {
  if (cameraWorkspace == nullptr) Skip(operation, "missing event camera");
  return cameraWorkspace;
}
}  // namespace

// decomp: BATTLE.PRG func_800BD57C (F01227, src/BATTLE/BATTLE.PRG/4A0A8.c:3647,
// 0x800BD57C-0x800BD610)
void UpdateEventAngleTweenEntry(AngleTween* tween) {
  if (tween == nullptr) {
    Skip("UpdateEventAngleTween", "missing tween");
    return;
  }
  LiveTweenEasing easing;
  EventAngleTween value = LoadAngleTween(*tween);
  UpdateEventAngleTween(value, VS::State::tickSpeed, easing);
  StoreAngleTween(*tween, value);
}

// decomp: BATTLE.PRG func_800BDAB4 (F01230, src/BATTLE/BATTLE.PRG/4A0A8.c:3812,
// 0x800BDAB4-0x800BDBB4)
void UpdateEventScreenTransformsEntry() {
  CameraState* camera = LiveCamera("UpdateEventScreenTransforms");
  if (camera == nullptr) return;
  LiveTweenEasing easing;
  LiveScreenEffect screen;
  EventScreenTweens tweens = LoadScreenTweens(*camera);
  UpdateEventScreenTransforms(tweens, VS::State::tickSpeed, easing, screen);
  StoreScreenTweens(*camera, tweens);
}

// decomp: BATTLE.PRG func_800BDBB4 (F01231, src/BATTLE/BATTLE.PRG/4A0A8.c:3838,
// 0x800BDBB4-0x800BDC9C)
int UpdateEventTripleTweenEntry(TripleTween* tween) {
  if (tween == nullptr) {
    Skip("UpdateEventTripleTween", "missing tween");
    return 0;
  }
  LiveTweenEasing easing;
  EventScreenTween value = LoadScreenTween(*tween);
  const std::optional<std::int32_t> eased =
      UpdateEventTripleTween(value, VS::State::tickSpeed, easing);
  StoreScreenTween(*tween, value);
  return eased.has_value() ? 1 : 0;
}

std::uint64_t RefusedEventScreenTweenCount() noexcept {
  return refused_count.load(std::memory_order_relaxed);
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800BB450 (F01198, src/BATTLE/BATTLE.PRG/4A0A8.c:2793,
// 0x800BB450-0x800BB474), opcode 0xE1.
std::intptr_t VS::Event::CommandSetScreenCaptureOpcode(
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
std::intptr_t VS::Event::CommandTweenCameraRollOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  constexpr const char* kOperation = "CommandTweenCameraRoll";
  if (instruction == nullptr) {
    event::Skip(kOperation, "missing instruction");
    return 0;
  }
  VS::Event::CameraState* camera = event::LiveCamera(kOperation);
  if (camera == nullptr) return 0;
  event::EventAngleTween camera_roll =
      event::LoadAngleTween(camera->camera_roll);
  event::EventAngleTween screen_roll =
      event::LoadAngleTween(camera->captured_screen_roll);
  if (!event::CommandTweenCameraRoll(
          event::ScriptCommand{instruction, event::kTweenRollLength},
          camera_roll, screen_roll)) {
    event::Skip(kOperation, "not a roll opcode");
    return 0;
  }
  event::StoreAngleTween(camera->camera_roll, camera_roll);
  event::StoreAngleTween(camera->captured_screen_roll, screen_roll);
  return 0;
}

// decomp: BATTLE.PRG func_800BD6C4 (F01229, src/BATTLE/BATTLE.PRG/4A0A8.c:3691,
// 0x800BD6C4-0x800BDAB4), opcodes 0xB9, 0xBA, 0xBF, 0xE4-0xE7 and 0xED.
std::intptr_t VS::Event::CommandScreenTransformOpcode(
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
  VS::Event::CameraState* camera = event::LiveCamera(kOperation);
  if (camera == nullptr) return 0;
  event::LiveScreenEffect screen;
  event::EventScreenTweens tweens = event::LoadScreenTweens(*camera);
  const std::optional<std::int32_t> result = event::CommandScreenTransform(
      event::ScriptCommand{instruction, *length}, tweens, screen);
  event::StoreScreenTweens(*camera, tweens);
  return result.value_or(0);
}
