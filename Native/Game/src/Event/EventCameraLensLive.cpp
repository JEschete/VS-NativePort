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
#include "Event/EventCameraLens.hpp"
#include "Event/ScriptCommand.hpp"

extern "C" {
#include "Event/Camera.h"
#include "Event/InterpolationCurveLive.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
#include "State/Timers.h"
}

namespace VS::Event {
namespace {
using Core::Math::Position3;

// The original addressed the oscillators from the first slot's displacement:
// a two-byte active count at 0x1A4, then two 12-byte oscillators.
constexpr std::size_t kShakeCountOffset = 0x1A4;
constexpr std::size_t kFirstOscillatorOffset = 0x1A6;
constexpr std::size_t kOscillatorStride = sizeof(Event_ShakeOscillator);
static_assert(offsetof(Event_CameraState, shakeOscillatorA) ==
              kShakeCountOffset);
static_assert(offsetof(Event_CameraState, shakeEyeOffsetX) ==
              kFirstOscillatorOffset +
                  kEventShakeOscillatorCount * kOscillatorStride);
// Oscillator fields, from the record's start.
using Oscillator = Event_ShakeOscillator;
constexpr std::size_t kEnvelopeField = offsetof(Oscillator, envelopeShape) + 1;
constexpr std::size_t kDisplacementField = offsetof(Oscillator, displacement);
constexpr std::size_t kCyclesField = offsetof(Oscillator, cycles);
constexpr std::size_t kAmplitudeField = offsetof(Oscillator, amplitude);
constexpr std::size_t kElapsedField = offsetof(Oscillator, elapsedFrames);
constexpr std::size_t kDurationField = offsetof(Oscillator, durationFrames);
constexpr std::size_t kAxisMaskField = offsetof(Oscillator, axisMask);
// The scratch words: the shared envelope (0x1F800088) and the
// cameraShakeScratch vectors behind it.
using ShakeScratch = Event_CameraShakeScratch;
constexpr std::size_t kEnvelopeScratch = 0x88;
constexpr std::size_t kEyeShakeScratch = 0xD8;
constexpr std::size_t kLookAtShakeScratch = 0xE0;
constexpr std::size_t kAxisShakeScratch = 0xE8;
static_assert(kEnvelopeScratch + offsetof(ShakeScratch, eyeShake) ==
              kEyeShakeScratch);
static_assert(kEnvelopeScratch + offsetof(ShakeScratch, lookAtShake) ==
              kLookAtShakeScratch);
static_assert(kEnvelopeScratch + offsetof(ShakeScratch, axisShake) ==
              kAxisShakeScratch);
constexpr std::size_t kHalfBytes = sizeof(std::int16_t);

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS_LOG_ERROR("event.camera", "{} refused: {}", operation, reason);
  std::abort();
}

Event_CameraState& RequiredCamera(const char* operation) {
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
          .eye_shake = LoadPosition(scratch, kEyeShakeScratch),
          .look_at_shake = LoadPosition(scratch, kLookAtShakeScratch),
          .axis_shake = LoadPosition(scratch, kAxisShakeScratch)};
}

void StoreScratch(const EventCameraScratch& values) {
  unsigned char* scratch = ScratchBytes();
  StoreAt(scratch, kEnvelopeScratch, values.envelope);
  StorePosition(scratch, kEyeShakeScratch, values.eye_shake);
  StorePosition(scratch, kLookAtShakeScratch, values.look_at_shake);
  StorePosition(scratch, kAxisShakeScratch, values.axis_shake);
}

std::int32_t LoadEnvelopeScratch() {
  return LoadAt<std::int32_t>(ScratchBytes(), kEnvelopeScratch);
}

void StoreEnvelopeScratch(std::int32_t envelope) {
  StoreAt(ScratchBytes(), kEnvelopeScratch, envelope);
}

EventLensTween LoadLens(const Event_LensTween& lens) {
  return {.changed = static_cast<std::uint8_t>(lens.hasChanged),
          .easing_mode = static_cast<std::uint8_t>(lens.easingMode),
          .value = lens.value,
          .start_value = lens.startValue,
          .delta_value = lens.deltaValue,
          .elapsed_frames = static_cast<std::uint8_t>(lens.elapsedFrames),
          .duration_frames = static_cast<std::uint8_t>(lens.durationFrames)};
}

void StoreLens(Event_LensTween& lens, const EventLensTween& tween) {
  lens.hasChanged = static_cast<char>(tween.changed);
  lens.easingMode = static_cast<char>(tween.easing_mode);
  lens.value = tween.value;
  lens.startValue = tween.start_value;
  lens.deltaValue = tween.delta_value;
  lens.elapsedFrames = static_cast<char>(tween.elapsed_frames);
  lens.durationFrames = static_cast<char>(tween.duration_frames);
}

EventShakeOscillator LoadOscillator(const unsigned char* record) {
  return {.envelope = LoadAt<std::uint8_t>(record, kEnvelopeField),
          .displacement = LoadAt<std::int16_t>(record, kDisplacementField),
          .cycles = LoadAt<std::int16_t>(record, kCyclesField),
          .amplitude = LoadAt<std::int16_t>(record, kAmplitudeField),
          .elapsed_frames = LoadAt<std::uint8_t>(record, kElapsedField),
          .duration_frames = LoadAt<std::uint8_t>(record, kDurationField),
          .axis_mask = LoadAt<std::uint8_t>(record, kAxisMaskField)};
}

// Byte 0 and byte 11 of each record are not the oscillator's.
void StoreOscillator(unsigned char* record,
                     const EventShakeOscillator& oscillator) {
  StoreAt(record, kEnvelopeField, oscillator.envelope);
  StoreAt(record, kDisplacementField, oscillator.displacement);
  StoreAt(record, kCyclesField, oscillator.cycles);
  StoreAt(record, kAmplitudeField, oscillator.amplitude);
  StoreAt(record, kElapsedField, oscillator.elapsed_frames);
  StoreAt(record, kDurationField, oscillator.duration_frames);
  StoreAt(record, kAxisMaskField, oscillator.axis_mask);
}

unsigned char* OscillatorRecord(Event_CameraState& camera, std::size_t index) {
  return reinterpret_cast<unsigned char*>(&camera) + kFirstOscillatorOffset +
         index * kOscillatorStride;
}

EventCameraShake LoadShake(Event_CameraState& camera) {
  EventCameraShake shake;
  for (std::size_t index = 0; index < shake.oscillators.size(); ++index) {
    shake.oscillators.at(index) =
        LoadOscillator(OscillatorRecord(camera, index));
  }
  shake.active_count = camera.shakeOscillatorA.envelopeShape.count;
  shake.eye_offset = {.x = camera.shakeEyeOffsetX,
                      .y = camera.shakeEyeOffsetY,
                      .z = camera.shakeEyeOffsetZ};
  shake.look_at_offset = {.x = camera.shakeLookAtOffsetX,
                          .y = camera.shakeLookAtOffsetY,
                          .z = camera.shakeLookAtOffsetZ};
  return shake;
}

void StoreShake(Event_CameraState& camera, const EventCameraShake& shake) {
  for (std::size_t index = 0; index < shake.oscillators.size(); ++index) {
    StoreOscillator(OscillatorRecord(camera, index),
                    shake.oscillators.at(index));
  }
  camera.shakeOscillatorA.envelopeShape.count = shake.active_count;
  camera.shakeEyeOffsetX = shake.eye_offset.x;
  camera.shakeEyeOffsetY = shake.eye_offset.y;
  camera.shakeEyeOffsetZ = shake.eye_offset.z;
  camera.shakeLookAtOffsetX = shake.look_at_offset.x;
  camera.shakeLookAtOffsetY = shake.look_at_offset.y;
  camera.shakeLookAtOffsetZ = shake.look_at_offset.z;
}

EventCameraLenses LoadLenses(const Event_CameraState& camera) {
  return {.projection_distance = LoadLens(camera.projectionDistance),
          .near_clip = LoadLens(camera.nearClip),
          .far_clip = LoadLens(camera.farClip)};
}

void StoreLenses(Event_CameraState& camera, const EventCameraLenses& lenses) {
  StoreLens(camera.projectionDistance, lenses.projection_distance);
  StoreLens(camera.nearClip, lenses.near_clip);
  StoreLens(camera.farClip, lenses.far_clip);
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
void UpdateEventLensTweenEntry(Event_LensTween* lens_tween) {
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
int UpdateEventShakeOscillatorEntry(Event_ShakeOscillator* oscillator) {
  if (oscillator == nullptr) {
    Refuse("UpdateEventShakeOscillator", "missing oscillator");
  }
  auto* record = reinterpret_cast<unsigned char*>(oscillator);
  LiveEventCameraLensHost host;
  EventShakeOscillator state = LoadOscillator(record);
  std::int32_t envelope = LoadEnvelopeScratch();
  const bool active = UpdateEventShakeOscillator(state, envelope, host);
  StoreOscillator(record, state);
  StoreEnvelopeScratch(envelope);
  return active ? 1 : 0;
}

// decomp: BATTLE.PRG func_800BE180 (F01235, src/BATTLE/BATTLE.PRG/4A0A8.c:3983,
// 0x800BE180-0x800BE36C)
void UpdateEventCameraShakeEntry() {
  Event_CameraState& camera = RequiredCamera("UpdateEventCameraShake");
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
  Event_CameraState& camera = event::RequiredCamera(kOperation);
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
