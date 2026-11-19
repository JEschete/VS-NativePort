// Hand-maintained (tools/port/owned.csv).
#include "Effects/Plugins/Plg128CameraLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>

#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Scratch.hpp"
#include "Effects/Camera.hpp"
#include "Effects/CameraLive.hpp"
#include "Effects/CameraLiveState.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Plugins/Plg128Camera.hpp"
#include "Effects/Runtime.h"
#include "Effects/TransformsLiveState.hpp"

namespace VS::Effects::Plg128 {
namespace {
namespace math = Core::Math;
using math::Position3;
using math::Vec3;

constexpr std::uintptr_t kEmitterScratchOffset = 0x1D0;
// func_800FDEC4 allocates the state with vs_main_allocHeapR(0xD0).
constexpr std::size_t kStateBytes = 0xD0;

// decomp: func_800FD9A4_t (src/EFFECT/PLG128.BIN/F8.c:10): the slot's state
// in the heap between commands, in its PS1 layout.
struct LiveCameraRecord {
  std::uint8_t lifetime{};
  std::uint8_t definition{};
  std::int16_t mode{};
  std::int32_t age{};
  LiveEffectCamera start_camera{};
  LiveEffectCamera end_camera{};
  VECTOR position{};
  VECTOR target{};
  MATRIX view{};
  std::int32_t near_clip{};
  std::int32_t reserved_cc{};
};
static_assert(sizeof(LiveCameraRecord) == kStateBytes);

using TransformSlots = decltype(LiveEffectInstance::transforms);
constexpr std::size_t kTransformCount = std::tuple_size_v<TransformSlots>;
// vectorRanges[] and pairRanges[] rows the camera reads.
constexpr std::size_t kDirectionRange = 0;
constexpr std::size_t kAttractionOffsetRange = 6;
constexpr std::size_t kAttractionJitterRange = 7;
constexpr std::size_t kSpeedRange = 2;

std::uint64_t refused_definitions = 0;
std::uint64_t refused_transforms = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

LiveParticleEmitterScratch& EmitterScratch() {
  return *reinterpret_cast<LiveParticleEmitterScratch*>(
      reinterpret_cast<std::uintptr_t>(VS::Core::ScratchBase()) +
      kEmitterScratchOffset);
}

[[nodiscard]] Position3 ShortOf(const SVECTOR& value) {
  return {value.vx, value.vy, value.vz};
}

void StoreShort(SVECTOR& record, Position3 value) {
  record.vx = value.x;
  record.vy = value.y;
  record.vz = value.z;
}

[[nodiscard]] CameraEffectState StateOf(const LiveCameraRecord& live) {
  return {.lifetime = live.lifetime,
          .definition = live.definition,
          .mode = live.mode,
          .age = live.age,
          .start_camera = LoadLiveEffectCameraPose(live.start_camera),
          .end_camera = LoadLiveEffectCameraPose(live.end_camera),
          .position = math::VectorOf(live.position),
          .target = math::VectorOf(live.target),
          .view = math::TransformOf(live.view),
          .near_clip = live.near_clip};
}

void StoreState(LiveCameraRecord& live, const CameraEffectState& state) {
  live.lifetime = state.lifetime;
  live.definition = state.definition;
  live.mode = state.mode;
  live.age = state.age;
  StoreLiveEffectCameraPose(live.start_camera, state.start_camera);
  StoreLiveEffectCameraPose(live.end_camera, state.end_camera);
  math::StoreVector(live.position, state.position);
  math::StoreVector(live.target, state.target);
  math::StoreTransform(live.view, state.view);
  live.near_clip = state.near_clip;
}

[[nodiscard]] CameraScratch ScratchOf(
    const LiveParticleEmitterScratch& scratch) {
  return {.flags = scratch.flags,
          .vector = ShortOf(scratch.vector),
          .position = math::VectorOf(scratch.position),
          .extent = math::VectorOf(scratch.extent),
          .base_direction = math::VectorOf(scratch.base_direction),
          .speed = {scratch.speed_range[0], scratch.speed_range[1]},
          .attraction_offset = ShortOf(scratch.attraction_offset),
          .attraction_jitter = ShortOf(scratch.attraction_jitter),
          .sin_angle0 = scratch.sin_angle0,
          .sin_angle1 = scratch.sin_angle1,
          .cos_angle0 = scratch.cos_angle0,
          .cos_angle1 = scratch.cos_angle1};
}

void StoreScratch(LiveParticleEmitterScratch& live,
                  const CameraScratch& scratch) {
  live.flags = scratch.flags;
  StoreShort(live.vector, scratch.vector);
  math::StoreVector(live.position, scratch.position);
  math::StoreVector(live.extent, scratch.extent);
  math::StoreVector(live.base_direction, scratch.base_direction);
  live.speed_range[0] = scratch.speed[0];
  live.speed_range[1] = scratch.speed[1];
  StoreShort(live.attraction_offset, scratch.attraction_offset);
  StoreShort(live.attraction_jitter, scratch.attraction_jitter);
  live.sin_angle0 = scratch.sin_angle0;
  live.sin_angle1 = scratch.sin_angle1;
  live.cos_angle0 = scratch.cos_angle0;
  live.cos_angle1 = scratch.cos_angle1;
}

[[nodiscard]] VectorRange VectorRangeOf(
    std::span<const std::int16_t, math::kVectorRangeHalfwords> ranges) {
  return {math::RangePair(ranges, 0), math::RangePair(ranges, 1),
          math::RangePair(ranges, 2)};
}

[[nodiscard]] CameraDefinition DefinitionOf(
    const LiveParticleDefinition& definition) {
  const std::span<const std::int16_t, math::kPlaneRangeHalfwords> speed =
      definition.pair_ranges[kSpeedRange];
  return {
      .flags = definition.flags,
      .position_curve = definition.position_curve,
      .direction_curve = definition.direction_curve,
      .speed_curve = definition.speed_curve,
      .attraction_offset_curve = definition.attraction_offset_curve,
      .attraction_jitter_curve = definition.attraction_jitter_curve,
      .inner_extent_curve = definition.inner_extent_curve,
      .position_range = VectorRangeOf(definition.position_range),
      .extent_range = VectorRangeOf(definition.extent_range),
      .direction_range =
          VectorRangeOf(definition.vector_ranges[kDirectionRange]),
      .speed_range = {math::RangePair(speed, 0), math::RangePair(speed, 1)},
      .attraction_offset_range =
          VectorRangeOf(definition.vector_ranges[kAttractionOffsetRange]),
      .attraction_jitter_range =
          VectorRangeOf(definition.vector_ranges[kAttractionJitterRange]),
      .inner_extent_range = math::RangePair(definition.inner_extent_range, 0),
      .transform_index = definition.transform_index,
      .attraction_transform = definition.attraction_transform};
}

class LiveCameraEffectHost final : public CameraEffectHost {
 public:
  explicit LiveCameraEffectHost(LiveEffectInstance* instance)
      : instance_(instance) {}

  [[nodiscard]] std::optional<CameraDefinition> Definition(
      std::uint8_t index) override {
    if (Effects_package == nullptr ||
        Effects_package->particle_definitions == nullptr ||
        index >= LiveParticleDefinitionCount()) {
      if (ShouldReport(refused_definitions)) {
        VS::Core::LogWarn(
            "effects",
            "plg128 camera: definition {} is outside the package's "
            "{} definitions; camera stopped",
            index, LiveParticleDefinitionCount());
      }
      return std::nullopt;
    }
    return DefinitionOf(
        Effects_package->particle_definitions->Definitions()[index]);
  }

  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t age) override {
    return LiveSampleEffectCurve(curve, age);
  }

  [[nodiscard]] Position3 EffectOrigin() override {
    return ShortOf(Effects_actionRequest.position);
  }

  [[nodiscard]] std::optional<Vec3> TransformTranslation(
      std::uint8_t index) override {
    const LiveTransformSlot* slot = Transform(index);
    if (slot == nullptr) {
      return std::nullopt;
    }
    return math::TransformOf(slot->world).translation;
  }

  [[nodiscard]] std::optional<std::int16_t> TransformFacing(
      std::uint8_t index) override {
    const LiveTransformSlot* slot = Transform(index);
    if (slot == nullptr) {
      return std::nullopt;
    }
    return slot->saved.pad;
  }

  [[nodiscard]] math::Mat3 LookAt(Vec3 from, Vec3 to) override {
    return math::BuildLookAtOrientation(from, to);
  }

  void CaptureCamera(EffectCameraPose& pose) override {
    CaptureEffectCamera(LiveEffectCameraHost(), pose);
  }

  void BeginCameraMotion(std::int16_t duration) override {
    EffectCameraState camera = LoadLiveEffectCameraState();
    camera.motion_frame = 0;
    camera.motion_duration = duration;
    StoreLiveEffectCameraState(camera);
  }

  void SetTransitionMode(std::int32_t mode) override {
    LiveSetEffectCameraTransitionMode(mode);
  }

  void PublishCameraMotionEnds(const EffectCameraPose& start,
                               const EffectCameraPose& end) override {
    EffectCameraState camera = LoadLiveEffectCameraState();
    camera.motion_ends = {start, end};
    StoreLiveEffectCameraState(camera);
  }

 private:
  [[nodiscard]] const LiveTransformSlot* Transform(std::uint8_t index) {
    if (instance_ == nullptr || index >= kTransformCount) {
      if (ShouldReport(refused_transforms)) {
        VS::Core::LogWarn(
            "effects",
            "plg128 camera: transform {} is outside the instance's "
            "{} transforms; camera stopped",
            index, instance_ == nullptr ? 0 : kTransformCount);
      }
      return nullptr;
    }
    return &instance_->transforms[index];
  }

  LiveEffectInstance* instance_ = nullptr;
};
}  // namespace

// decomp: PLG128.BIN func_800FDEC4 (F02068, src/EFFECT/PLG128.BIN/F8.c:146,
// 0x800FDEC4-0x800FE3D0). The original allocated the state unchecked and left
// the fields creation does not write as the heap held them; this clears them.
// Neither frees the state: the effect runtime's slot release
// (EffectInstancesLive.cpp) does once a command returns 0.
int UpdateLiveEffectCamera(LiveRendererSlot* slot, unsigned int command,
                           int packed) {
  if (slot == nullptr) {
    VS::Core::LogWarn("effects", "plg128 camera: command {} without a slot",
                      command);
    return 0;
  }
  const auto kind = static_cast<CameraCommand>(command);
  if (kind == CameraCommand::kCreate) {
    void* storage =
        Core::Memory::AllocateLiveFromHighEnd(sizeof(LiveCameraRecord));
    if (storage == nullptr) {
      VS::Core::LogError("effects", "plg128 camera: heap exhausted for state");
      return 0;
    }
    slot->renderer_state =
        std::construct_at(static_cast<LiveCameraRecord*>(storage));
  } else if (kind == CameraCommand::kTick || kind == CameraCommand::kRelease) {
    if (slot->renderer_state == nullptr) {
      VS::Core::LogWarn("effects", "plg128 camera: command {} without a state",
                        command);
      return 0;
    }
  } else {
    return kind == CameraCommand::kStop ? 0 : 1;
  }
  auto& live = *static_cast<LiveCameraRecord*>(slot->renderer_state);
  LiveParticleEmitterScratch& live_scratch = EmitterScratch();
  CameraEffectState state = StateOf(live);
  CameraScratch scratch = ScratchOf(live_scratch);
  LiveCameraEffectHost host(Effects_currentInstance);
  const std::int32_t age = state.age;
  const std::int32_t alive =
      UpdateEffectCamera(host, state, scratch, command, packed);
  StoreState(live, state);
  StoreScratch(live_scratch, scratch);
  // Only a refused tick leaves the age alone.
  if (kind == CameraCommand::kTick && state.age != age) {
    // The original left its view in the GTE's rotation and translation.
    Core_Gte_SetRotMatrix(&live.view);
    Core_Gte_SetTransMatrix(&live.view);
  }
  return alive;
}
}  // namespace VS::Effects::Plg128
