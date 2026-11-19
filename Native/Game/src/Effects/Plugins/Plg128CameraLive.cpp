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
#include "Effects/Camera.h"
#include "Effects/Camera.hpp"
#include "Effects/CameraLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.h"
#include "Effects/PackageExtents.hpp"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg128Camera.hpp"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

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
  Effects_EffectCamera start_camera{};
  Effects_EffectCamera end_camera{};
  VECTOR position{};
  VECTOR target{};
  MATRIX view{};
  std::int32_t near_clip{};
  std::int32_t reserved_cc{};
};
static_assert(sizeof(LiveCameraRecord) == kStateBytes);

using TransformSlots = decltype(Effects_EffectInstance::transforms);
constexpr std::size_t kTransformCount = std::extent_v<TransformSlots>;
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

Effects_ParticleEmitterScratch& EmitterScratch() {
  return *reinterpret_cast<Effects_ParticleEmitterScratch*>(
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
    const Effects_ParticleEmitterScratch& scratch) {
  return {.flags = scratch.flags,
          .vector = ShortOf(scratch.vector),
          .position = math::VectorOf(scratch.position),
          .extent = math::VectorOf(scratch.extent),
          .base_direction = math::VectorOf(scratch.baseDirection),
          .speed = {scratch.speedRange[0], scratch.speedRange[1]},
          .attraction_offset = ShortOf(scratch.attractionOffset),
          .attraction_jitter = ShortOf(scratch.attractionJitter),
          .sin_angle0 = scratch.sinAngle0,
          .sin_angle1 = scratch.sinAngle1,
          .cos_angle0 = scratch.cosAngle0,
          .cos_angle1 = scratch.cosAngle1};
}

void StoreScratch(Effects_ParticleEmitterScratch& live,
                  const CameraScratch& scratch) {
  live.flags = scratch.flags;
  StoreShort(live.vector, scratch.vector);
  math::StoreVector(live.position, scratch.position);
  math::StoreVector(live.extent, scratch.extent);
  math::StoreVector(live.baseDirection, scratch.base_direction);
  live.speedRange[0] = scratch.speed[0];
  live.speedRange[1] = scratch.speed[1];
  StoreShort(live.attractionOffset, scratch.attraction_offset);
  StoreShort(live.attractionJitter, scratch.attraction_jitter);
  live.sinAngle0 = scratch.sin_angle0;
  live.sinAngle1 = scratch.sin_angle1;
  live.cosAngle0 = scratch.cos_angle0;
  live.cosAngle1 = scratch.cos_angle1;
}

[[nodiscard]] VectorRange VectorRangeOf(
    std::span<const std::int16_t, math::kVectorRangeHalfwords> ranges) {
  return {math::RangePair(ranges, 0), math::RangePair(ranges, 1),
          math::RangePair(ranges, 2)};
}

[[nodiscard]] CameraDefinition DefinitionOf(
    const Effects_ParticleDefinition& definition) {
  const std::span<const std::int16_t, math::kPlaneRangeHalfwords> speed =
      definition.pairRanges[kSpeedRange];
  return {.flags = definition.flags,
          .position_curve = definition.positionCurve,
          .direction_curve = definition.directionCurve,
          .speed_curve = definition.speedCurve,
          .attraction_offset_curve = definition.attractionOffsetCurve,
          .attraction_jitter_curve = definition.attractionJitterCurve,
          .inner_extent_curve = definition.innerExtentCurve,
          .position_range = VectorRangeOf(definition.positionRange),
          .extent_range = VectorRangeOf(definition.extentRange),
          .direction_range =
              VectorRangeOf(definition.vectorRanges[kDirectionRange]),
          .speed_range = {math::RangePair(speed, 0), math::RangePair(speed, 1)},
          .attraction_offset_range =
              VectorRangeOf(definition.vectorRanges[kAttractionOffsetRange]),
          .attraction_jitter_range =
              VectorRangeOf(definition.vectorRanges[kAttractionJitterRange]),
          .inner_extent_range = math::RangePair(definition.innerExtentRange, 0),
          .transform_index = definition.transformIndex,
          .attraction_transform = definition.attractionTransform};
}

class LiveCameraEffectHost final : public CameraEffectHost {
 public:
  explicit LiveCameraEffectHost(Effects_EffectInstance* instance)
      : instance_(instance) {}

  [[nodiscard]] std::optional<CameraDefinition> Definition(
      std::uint8_t index) override {
    if (Effects_package == nullptr || Effects_package->block5Data == nullptr ||
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
    return DefinitionOf(Effects_package->block5Data->definitions[index]);
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
    const Effects_TransformSlot* slot = Transform(index);
    if (slot == nullptr) {
      return std::nullopt;
    }
    return math::TransformOf(slot->world).translation;
  }

  [[nodiscard]] std::optional<std::int16_t> TransformFacing(
      std::uint8_t index) override {
    const Effects_TransformSlot* slot = Transform(index);
    if (slot == nullptr) {
      return std::nullopt;
    }
    return slot->opaqueBE;
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
  [[nodiscard]] const Effects_TransformSlot* Transform(std::uint8_t index) {
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

  Effects_EffectInstance* instance_ = nullptr;
};
}  // namespace

// decomp: PLG128.BIN func_800FDEC4 (F02068, src/EFFECT/PLG128.BIN/F8.c:146,
// 0x800FDEC4-0x800FE3D0). The original allocated the state unchecked and left
// the fields creation does not write as the heap held them; this clears them.
// Neither frees the state: the effect runtime's slot release
// (EffectInstancesLive.cpp) does once a command returns 0.
int UpdateLiveEffectCamera(Effects_RendererSlot* slot, unsigned int command,
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
    slot->rendererState =
        std::construct_at(static_cast<LiveCameraRecord*>(storage));
  } else if (kind == CameraCommand::kTick || kind == CameraCommand::kRelease) {
    if (slot->rendererState == nullptr) {
      VS::Core::LogWarn("effects", "plg128 camera: command {} without a state",
                        command);
      return 0;
    }
  } else {
    return kind == CameraCommand::kStop ? 0 : 1;
  }
  auto& live = *static_cast<LiveCameraRecord*>(slot->rendererState);
  Effects_ParticleEmitterScratch& live_scratch = EmitterScratch();
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
