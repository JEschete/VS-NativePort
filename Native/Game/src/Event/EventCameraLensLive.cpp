// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraLensLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "Core/Log.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Scratch.h"
#include "Event/Camera.hpp"
#include "Event/EventCameraLens.hpp"
#include "Event/InterpolationCurveLive.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.h"
#include "State/Timers.h"

namespace VS::Event {
namespace {
using Core::Math::Position3;

static_assert(kShakeOscillatorCount == kEventShakeOscillatorCount);
// The scratch words (decomp cameraShakeScratch at 0x1F800088): the shared
// envelope, then the look-at, eye and per-axis shake vectors.
constexpr std::size_t kEnvelopeScratch = 0x88;
constexpr std::size_t kLookAtShakeScratch = 0xD8;
constexpr std::size_t kEyeShakeScratch = 0xE0;
constexpr std::size_t kAxisShakeScratch = 0xE8;
constexpr std::size_t kHalfBytes = sizeof(std::int16_t);

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS_LOG_ERROR("event.camera", "{} refused: {}", operation, reason);
  std::abort();
}

CameraState& RequiredCamera(const char* operation) {
  if (Event_camera == nullptr) Refuse(operation, "missing event camera");
  return *Event_camera;
}

template <typename Value>
Value LoadAt(const unsigned char* base, std::size_t offset) {
  Value value{};
  std::memcpy(&value, base + offset, sizeof(value));
  return value;
}

template <typename Value>
void StoreAt(unsigned char* base, std::size_t offset, Value value) {
  std::memcpy(base + offset, &value, sizeof(value));
}

unsigned char* ScratchBytes() {
  return static_cast<unsigned char*>(Core_ScratchBase());
}

Position3 LoadPosition(const unsigned char* base, std::size_t offset) {
  return {.x = LoadAt<std::int16_t>(base, offset),
          .y = LoadAt<std::int16_t>(base, offset + kHalfBytes),
          .z = LoadAt<std::int16_t>(base, offset + 2 * kHalfBytes)};
}

// The shake writes three halfwords and keeps the pad.
void StorePosition(unsigned char* base, std::size_t offset, Position3 value) {
  StoreAt(base, offset, value.x);
  StoreAt(base, offset + kHalfBytes, value.y);
  StoreAt(base, offset + 2 * kHalfBytes, value.z);
}

EventCameraScratch LoadScratch() {
  const unsigned char* scratch = ScratchBytes();
  return {.envelope = LoadAt<std::int32_t>(scratch, kEnvelopeScratch),
          .look_at_shake = LoadPosition(scratch, kLookAtShakeScratch),
          .eye_shake = LoadPosition(scratch, kEyeShakeScratch),
          .axis_shake = LoadPosition(scratch, kAxisShakeScratch)};
}

void StoreScratch(const EventCameraScratch& values) {
  unsigned char* scratch = ScratchBytes();
  StoreAt(scratch, kEnvelopeScratch, values.envelope);
  StorePosition(scratch, kLookAtShakeScratch, values.look_at_shake);
  StorePosition(scratch, kEyeShakeScratch, values.eye_shake);
  StorePosition(scratch, kAxisShakeScratch, values.axis_shake);
}

std::int32_t LoadEnvelopeScratch() {
  return LoadAt<std::int32_t>(ScratchBytes(), kEnvelopeScratch);
}

void StoreEnvelopeScratch(std::int32_t envelope) {
  StoreAt(ScratchBytes(), kEnvelopeScratch, envelope);
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
  int TickSpeed() override { return State_tickSpeed; }

  int EaseFraction(int mode, int frame, int duration) override {
    return Event_EvaluateInterpolationCurveEntry(
        static_cast<std::int16_t>(mode), static_cast<std::int16_t>(frame),
        static_cast<std::int16_t>(duration));
  }

  int Sine(int angle) override { return Core::Math::Sine(angle); }
};
}  // namespace

// decomp: BATTLE.PRG func_800BDF6C (F01233, src/BATTLE/BATTLE.PRG/4A0A8.c:3927,
// 0x800BDF6C-0x800BE01C)
void UpdateEventLensTweenEntry(LensTween* lens_tween) {
  if (lens_tween == nullptr) Refuse("UpdateEventLensTween", "missing tween");
  LiveEventCameraLensHost host;
  EventLensTween tween = LoadLens(*lens_tween);
  std::int32_t fraction = LoadEnvelopeScratch();
  UpdateEventLensTween(tween, fraction, host);
  StoreLens(*lens_tween, tween);
  StoreEnvelopeScratch(fraction);
}

// decomp: BATTLE.PRG func_800BE01C (F01234, src/BATTLE/BATTLE.PRG/4A0A8.c:3945,
// 0x800BE01C-0x800BE180)
int UpdateEventShakeOscillatorEntry(ShakeOscillator* oscillator) {
  if (oscillator == nullptr) {
    Refuse("UpdateEventShakeOscillator", "missing oscillator");
  }
  LiveEventCameraLensHost host;
  EventShakeOscillator state = LoadOscillator(*oscillator);
  std::int32_t envelope = LoadEnvelopeScratch();
  const bool active = UpdateEventShakeOscillator(state, envelope, host);
  StoreOscillator(*oscillator, state);
  StoreEnvelopeScratch(envelope);
  return active ? 1 : 0;
}

// decomp: BATTLE.PRG func_800BE180 (F01235, src/BATTLE/BATTLE.PRG/4A0A8.c:3983,
// 0x800BE180-0x800BE36C)
void UpdateEventCameraShakeEntry() {
  CameraState& camera = RequiredCamera("UpdateEventCameraShake");
  LiveEventCameraLensHost host;
  EventCameraShake shake = LoadShake(camera);
  EventCameraScratch scratch = LoadScratch();
  UpdateEventCameraShake(shake, scratch, host);
  StoreShake(camera, shake);
  StoreScratch(scratch);
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800BDC9C (F01232, src/BATTLE/BATTLE.PRG/4A0A8.c:3862,
// 0x800BDC9C-0x800BDF6C), opcodes 0xEA-0xEC, 0xEE and 0xEF in the normal and
// skip tables. 0xEE returns the original's fall-through value 0xEF, which the
// script VM refuses as a pointer result.
std::intptr_t Event_CommandCameraLensOrShakeOpcode(
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
