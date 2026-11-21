// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraLensLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Core/Log.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/Camera.hpp"
#include "Event/EventCameraLens.hpp"
#include "Event/InterpolationCurveLive.hpp"
#include "Event/ScriptCamera.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "State/Timers.hpp"

namespace VS::Event {
namespace {
static_assert(kShakeOscillatorCount == kEventShakeOscillatorCount);

// decomp: cameraShakeScratch (0x1F800088): the shared envelope, then the
// look-at, eye and per-axis shake vectors. Zero, as a cold boot leaves it.
EventCameraScratch cameraScratch;

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS::Core::LogError("event.camera", "{} refused: {}", operation, reason);
  std::abort();
}

CameraState& RequiredCamera(const char* operation) {
  if (cameraWorkspace == nullptr) Refuse(operation, "missing event camera");
  return *cameraWorkspace;
}

EventLensTween LoadLens(const LensTween& lens) {
  return {.changed = lens.has_changed,
          .easing_mode = lens.easing_mode,
          .value = lens.value,
          .start_value = lens.start_value,
          .delta_value = lens.delta_value,
          .elapsed_frames = lens.elapsed_frames,
          .duration_frames = lens.duration_frames};
}

void StoreLens(LensTween& lens, const EventLensTween& tween) {
  lens.has_changed = tween.changed;
  lens.easing_mode = tween.easing_mode;
  lens.value = tween.value;
  lens.start_value = tween.start_value;
  lens.delta_value = tween.delta_value;
  lens.elapsed_frames = tween.elapsed_frames;
  lens.duration_frames = tween.duration_frames;
}

EventShakeOscillator LoadOscillator(const ShakeOscillator& record) {
  return {.envelope = record.envelope_shape,
          .displacement = record.displacement,
          .cycles = record.cycles,
          .amplitude = record.amplitude,
          .elapsed_frames = record.elapsed_frames,
          .duration_frames = record.duration_frames,
          .axis_mask = record.axis_mask};
}

// The record's first and last bytes are not the oscillator's and are kept.
void StoreOscillator(ShakeOscillator& record,
                     const EventShakeOscillator& oscillator) {
  record.envelope_shape = oscillator.envelope;
  record.displacement = oscillator.displacement;
  record.cycles = oscillator.cycles;
  record.amplitude = oscillator.amplitude;
  record.elapsed_frames = oscillator.elapsed_frames;
  record.duration_frames = oscillator.duration_frames;
  record.axis_mask = oscillator.axis_mask;
}

EventCameraShake LoadShake(const CameraState& camera) {
  EventCameraShake shake;
  for (std::size_t index = 0; index < shake.oscillators.size(); ++index) {
    shake.oscillators.at(index) =
        LoadOscillator(camera.shake_oscillators.at(index));
  }
  shake.active_count = camera.shake_active_count;
  shake.look_at_offset = camera.shake_look_at_offset;
  shake.eye_offset = camera.shake_eye_offset;
  return shake;
}

// The offsets are three halfwords each; the pad between them is kept.
void StoreShake(CameraState& camera, const EventCameraShake& shake) {
  for (std::size_t index = 0; index < shake.oscillators.size(); ++index) {
    StoreOscillator(camera.shake_oscillators.at(index),
                    shake.oscillators.at(index));
  }
  camera.shake_active_count = shake.active_count;
  camera.shake_look_at_offset = shake.look_at_offset;
  camera.shake_eye_offset = shake.eye_offset;
}

EventCameraLenses LoadLenses(const CameraState& camera) {
  return {.projection_distance = LoadLens(camera.projection_distance),
          .near_clip = LoadLens(camera.near_clip),
          .far_clip = LoadLens(camera.far_clip)};
}

void StoreLenses(CameraState& camera, const EventCameraLenses& lenses) {
  StoreLens(camera.projection_distance, lenses.projection_distance);
  StoreLens(camera.near_clip, lenses.near_clip);
  StoreLens(camera.far_clip, lenses.far_clip);
}

std::size_t CommandLength(std::uint8_t opcode) {
  switch (opcode) {
    case kOpcodeTweenProjectionDistance:
    case kOpcodeTweenNearClip:
    case kOpcodeTweenFarClip:
      return kCameraLensCommandLength;
    case kOpcodeStartCameraShake:
      return kCameraShakeCommandLength;
    default:
      return 1;
  }
}

class LiveEventCameraLensHost final : public EventCameraLensHost {
 public:
  int TickSpeed() override { return VS::State::tickSpeed; }

  int EaseFraction(int mode, int frame, int duration) override {
    return EvaluateInterpolationCurveEntry(static_cast<std::int16_t>(mode),
                                           static_cast<std::int16_t>(frame),
                                           static_cast<std::int16_t>(duration));
  }

  int Sine(int angle) override { return Core::Math::Sine(angle); }
};
}  // namespace

EventCameraScratch& LiveEventCameraScratch() noexcept { return cameraScratch; }

void ResetLiveEventCameraScratch() noexcept { cameraScratch = {}; }

// decomp: BATTLE.PRG func_800BDF6C (F01233, src/BATTLE/BATTLE.PRG/4DEA8.c:4577,
// 0x800BDF6C-0x800BE01C)
void UpdateEventLensTweenEntry(LensTween* lens_tween) {
  if (lens_tween == nullptr) Refuse("UpdateEventLensTween", "missing tween");
  LiveEventCameraLensHost host;
  EventLensTween tween = LoadLens(*lens_tween);
  UpdateEventLensTween(tween, cameraScratch.envelope, host);
  StoreLens(*lens_tween, tween);
}

// decomp: BATTLE.PRG func_800BE01C (F01234, src/BATTLE/BATTLE.PRG/4DEA8.c:4595,
// 0x800BE01C-0x800BE180)
int UpdateEventShakeOscillatorEntry(ShakeOscillator* oscillator) {
  if (oscillator == nullptr) {
    Refuse("UpdateEventShakeOscillator", "missing oscillator");
  }
  LiveEventCameraLensHost host;
  EventShakeOscillator state = LoadOscillator(*oscillator);
  const bool active =
      UpdateEventShakeOscillator(state, cameraScratch.envelope, host);
  StoreOscillator(*oscillator, state);
  return active ? 1 : 0;
}

// decomp: BATTLE.PRG func_800BE180 (F01235, src/BATTLE/BATTLE.PRG/4DEA8.c:4633,
// 0x800BE180-0x800BE36C)
void UpdateEventCameraShakeEntry() {
  CameraState& camera = RequiredCamera("UpdateEventCameraShake");
  LiveEventCameraLensHost host;
  EventCameraShake shake = LoadShake(camera);
  UpdateEventCameraShake(shake, cameraScratch, host);
  StoreShake(camera, shake);
}
}  // namespace VS::Event

// decomp: BATTLE.PRG vs_battle_script_EA_EB_EC_EE_EF_camera (F01232,
// src/BATTLE/BATTLE.PRG/4DEA8.c:4512; was func_800BDC9C,
// 0x800BDC9C-0x800BDF6C), opcodes 0xEA-0xEC, 0xEE and 0xEF in the normal and
// skip tables. 0xEE returns the original's fall-through value 0xEF, which the
// script VM refuses as a pointer result.
std::intptr_t VS::Event::CommandCameraLensOrShakeOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  constexpr const char* kOperation = "CommandCameraLensOrShake";
  if (instruction == nullptr) event::Refuse(kOperation, "missing instruction");
  VS::Event::CameraState& camera = event::RequiredCamera(kOperation);
  event::EventCameraLenses lenses = event::LoadLenses(camera);
  event::EventCameraShake shake = event::LoadShake(camera);
  const std::optional<std::intptr_t> result = event::CommandCameraLensOrShake(
      event::ScriptCommand{instruction, event::CommandLength(instruction[0])},
      lenses, shake);
  if (!result.has_value()) event::Refuse(kOperation, "short instruction");
  event::StoreLenses(camera, lenses);
  event::StoreShake(camera, shake);
  return *result;
}
