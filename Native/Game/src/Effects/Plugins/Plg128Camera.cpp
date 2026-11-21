// Hand-maintained (tools/port/owned.csv).
#include "Effects/Plugins/Plg128Camera.hpp"

#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Camera.hpp"

namespace VS::Effects::Plg128 {
namespace {
using Core::Math::Position3;
using Core::Math::Vec3;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrapSigned16;

constexpr int kFractionBits = 12;

[[nodiscard]] constexpr Vec3 Widen(Position3 value) noexcept {
  return {value.x, value.y, value.z};
}

[[nodiscard]] constexpr Position3 Narrow(Vec3 value) noexcept {
  return {WrapSigned16(static_cast<std::uint32_t>(value.x)),
          WrapSigned16(static_cast<std::uint32_t>(value.y)),
          WrapSigned16(static_cast<std::uint32_t>(value.z))};
}

// Halfword sums as the original's lhu/addu/sh.
[[nodiscard]] constexpr std::int16_t AddHalfword(std::int16_t left,
                                                 std::int32_t right) noexcept {
  return WrapSigned16(static_cast<std::uint32_t>(left) +
                      static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr Position3 AddHalfwords(Position3 left,
                                               Vec3 right) noexcept {
  return {AddHalfword(left.x, right.x), AddHalfword(left.y, right.y),
          AddHalfword(left.z, right.z)};
}

[[nodiscard]] constexpr Vec3 AddWords(Vec3 left, Vec3 right) noexcept {
  return {WrappingAdd(left.x, right.x), WrappingAdd(left.y, right.y),
          WrappingAdd(left.z, right.z)};
}

// (value * factor) >> 12 over the low word of the product (mult/mflo/sra).
[[nodiscard]] constexpr std::int32_t ScaleQ12(std::int32_t value,
                                              std::int32_t factor) noexcept {
  return WrappingMultiply(value, factor) >> kFractionBits;
}

[[nodiscard]] constexpr std::int32_t ToFixed(std::int32_t value) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value)
                                   << kFractionBits);
}

// The position switch func_800FDA04 and func_800FDC80 share.
void ResolveScratchPosition(CameraScratch& scratch,
                            const CameraAnchors& anchors) noexcept {
  switch (scratch.flags & kPositionSourceMask) {
    case kPositionFromOrigin:
      // vs_battle_addVecToSvec(&position, D_800F5310, &position).
      scratch.position =
          AddWords(scratch.position, Widen(anchors.effect_origin));
      break;
    case kPositionFromTransform:
      scratch.position =
          AddWords(scratch.position, anchors.transform_translation);
      break;
    default:
      break;
  }
}

// The flags-0 offset: a point on a sphere of the extent's radii, its
// elevation sine kept non-positive.
void SphereOffset(CameraScratch& scratch) {
  const Vec3& direction = scratch.base_direction;
  const Vec3& extent = scratch.extent;
  scratch.sin_angle0 = Core::Math::Sine(direction.x);
  if (scratch.sin_angle0 > 0) {
    scratch.sin_angle0 = Core::Math::WrappingNegate(scratch.sin_angle0);
  }
  scratch.cos_angle0 = Core::Math::Cosine(direction.x);
  scratch.sin_angle1 = Core::Math::Sine(direction.y);
  scratch.cos_angle1 = Core::Math::Cosine(direction.y);
  scratch.vector = Narrow(
      {ScaleQ12(ScaleQ12(extent.x, scratch.cos_angle0), scratch.cos_angle1),
       ScaleQ12(ScaleQ12(extent.y, scratch.cos_angle0), scratch.sin_angle1),
       ScaleQ12(extent.z, scratch.sin_angle0)});
}

// The flags-0x20 offset: a point on an ellipse in x and y with the extent's
// z kept as the depth.
void DiscOffset(CameraScratch& scratch) {
  const Vec3& extent = scratch.extent;
  scratch.sin_angle0 = Core::Math::Sine(scratch.base_direction.x);
  scratch.cos_angle0 = Core::Math::Cosine(scratch.base_direction.x);
  scratch.vector = Narrow({ScaleQ12(extent.x, scratch.cos_angle0),
                           ScaleQ12(extent.y, scratch.sin_angle0), extent.z});
}

// The tick's curve samples and range interpolations (F8.c:177-190).
void SampleDefinition(CameraEffectHost& host, const CameraDefinition& def,
                      CameraEffectState& state, CameraScratch& scratch) {
  using Core::Math::InterpolateSevenBitVector;
  const std::int32_t age = state.age;
  scratch.position = InterpolateSevenBitVector(
      def.position_range, host.SampleCurve(def.position_curve, age));
  scratch.extent = InterpolateSevenBitVector(
      def.extent_range, host.SampleCurve(def.position_curve, age));
  scratch.base_direction = InterpolateSevenBitVector(
      def.direction_range, host.SampleCurve(def.direction_curve, age));
  const Core::Math::Vec2 speed = Core::Math::InterpolateSevenBitPlane(
      def.speed_range, host.SampleCurve(def.speed_curve, age));
  scratch.speed = {speed.x, speed.y};
  scratch.attraction_offset = Narrow(InterpolateSevenBitVector(
      def.attraction_offset_range,
      host.SampleCurve(def.attraction_offset_curve, age)));
  scratch.attraction_jitter = Narrow(InterpolateSevenBitVector(
      def.attraction_jitter_range,
      host.SampleCurve(def.attraction_jitter_curve, age)));
  state.near_clip = Core::Math::InterpolateSevenBitScalar(
      def.inner_extent_range, host.SampleCurve(def.inner_extent_curve, age));
}

[[nodiscard]] std::int32_t TickEffectCamera(CameraEffectHost& host,
                                            CameraEffectState& state,
                                            CameraScratch& scratch) {
  const std::optional<CameraDefinition> definition =
      host.Definition(state.definition);
  if (!definition.has_value()) {
    return 0;
  }
  const std::uint32_t flags = definition->flags & kDefinitionFlagsMask;
  const std::optional<CameraAnchors> anchors =
      GatherCameraAnchors(host, *definition, flags, state.mode);
  if (!anchors.has_value()) {
    return 0;
  }
  scratch.flags = flags;
  SampleDefinition(host, *definition, state, scratch);
  if (state.mode == kEndpointMode) {
    ResolveCameraEndpoints(state, scratch, *anchors);
  } else if (state.mode == kActorFacingMode) {
    ResolveActorFacingCamera(state, scratch, *anchors);
  }
  switch (scratch.flags & kOffsetShapeMask) {
    case kOffsetShapeSphere:
      BuildCameraOffsetTransform(host, state, state.position);
      SphereOffset(scratch);
      break;
    case kOffsetShapeDisc:
      BuildCameraOffsetTransform(host, state, state.position);
      DiscOffset(scratch);
      break;
    default:
      // The last view and offset stay in use.
      break;
  }
  // SetRotMatrix/SetTransMatrix(&view) and RTV0TR of the offset.
  state.position =
      Core::Math::RotateTranslate(state.view, Widen(scratch.vector));
  if (ResolvesEndpoints(state.mode)) {
    StoreCameraEndpoints(state);
  }
  state.end_camera.near_clip = state.near_clip;
  state.end_camera.far_clip = scratch.speed[0];
  host.PublishCameraMotionEnds(state.start_camera, state.end_camera);
  state.age = WrappingAdd(state.age, 1);
  // A zero lifetime wraps to 255 frames.
  --state.lifetime;
  return state.lifetime != 0 ? 1 : 0;
}
}  // namespace

std::optional<CameraAnchors> GatherCameraAnchors(
    CameraEffectHost& host, const CameraDefinition& definition,
    std::uint32_t flags, std::int16_t mode) {
  CameraAnchors anchors;
  if (!ResolvesEndpoints(mode)) {
    return anchors;
  }
  const std::uint32_t position_source = flags & kPositionSourceMask;
  const std::uint32_t target_source = flags & kTargetSourceMask;
  const bool endpoints = mode == kEndpointMode;
  if (position_source == kPositionFromOrigin ||
      (endpoints && target_source == kTargetFromOrigin)) {
    anchors.effect_origin = host.EffectOrigin();
  }
  if (position_source == kPositionFromTransform) {
    const std::optional<Vec3> translation =
        host.TransformTranslation(definition.transform_index);
    if (!translation.has_value()) {
      return std::nullopt;
    }
    anchors.transform_translation = *translation;
  }
  if (endpoints && target_source == kTargetFromTransform) {
    const std::optional<Vec3> translation =
        host.TransformTranslation(definition.attraction_transform);
    if (!translation.has_value()) {
      return std::nullopt;
    }
    anchors.attraction_translation = *translation;
  }
  if (!endpoints) {
    const std::optional<std::int16_t> facing =
        host.TransformFacing(definition.transform_index);
    if (!facing.has_value()) {
      return std::nullopt;
    }
    anchors.facing = *facing;
  }
  return anchors;
}

// decomp: PLG128.BIN func_800FD9A4 (F02064, src/EFFECT/PLG128.BIN/F8.c:36,
// 0x800FD9A4-0x800FDA04)
void BuildCameraOffsetTransform(CameraEffectHost& host,
                                CameraEffectState& state, Vec3 translation) {
  state.view.rotation = host.LookAt(state.target, state.position);
  state.view.translation = translation;
}

// decomp: PLG128.BIN func_800FDA04 (F02065, src/EFFECT/PLG128.BIN/F8.c:74,
// 0x800FDA04-0x800FDC80)
void ResolveCameraEndpoints(CameraEffectState& state, CameraScratch& scratch,
                            const CameraAnchors& anchors) noexcept {
  ResolveScratchPosition(scratch, anchors);
  state.position = scratch.position;
  switch (scratch.flags & kTargetSourceMask) {
    case kTargetFromTransform:
      scratch.attraction_offset = AddHalfwords(scratch.attraction_offset,
                                               anchors.attraction_translation);
      break;
    case kTargetFromOrigin:
      scratch.attraction_offset =
          AddHalfwords(scratch.attraction_offset, Widen(anchors.effect_origin));
      break;
    default:
      break;
  }
  state.target = Widen(scratch.attraction_offset);
}

// decomp: PLG128.BIN func_800FDC80 (F02066, src/EFFECT/PLG128.BIN/F8.c:81,
// 0x800FDC80-0x800FDE78)
void ResolveActorFacingCamera(CameraEffectState& state, CameraScratch& scratch,
                              const CameraAnchors& anchors) noexcept {
  ResolveScratchPosition(scratch, anchors);
  const Vec3 source = scratch.position;
  state.target = source;
  const std::int32_t back =
      Core::Math::WrappingNegate(scratch.attraction_jitter.x);
  state.position = {
      WrappingAdd(source.x, ScaleQ12(back, Core::Math::Sine(anchors.facing))),
      source.y,
      WrappingAdd(source.z,
                  ScaleQ12(back, Core::Math::Cosine(anchors.facing)))};
}

// decomp: PLG128.BIN func_800FDE78 (F02067, src/EFFECT/PLG128.BIN/F8.c:108,
// 0x800FDE78-0x800FDEC4). The scratch argument is unused.
void StoreCameraEndpoints(CameraEffectState& state) noexcept {
  state.end_camera.position = {ToFixed(state.position.x),
                               ToFixed(state.position.y),
                               ToFixed(state.position.z)};
  state.end_camera.look_at = {ToFixed(state.target.x), ToFixed(state.target.y),
                              ToFixed(state.target.z)};
}

// decomp: PLG128.BIN vs_effect_bloodySin_5 (F02068,
// src/EFFECT/PLG128.BIN/F8.c:120; was func_800FDEC4, 0x800FDEC4-0x800FE3D0)
std::int32_t UpdateEffectCamera(CameraEffectHost& host,
                                CameraEffectState& state,
                                CameraScratch& scratch, std::uint32_t command,
                                std::int32_t packed) {
  constexpr unsigned kByteBits = 8;
  constexpr unsigned kModeShift = 16;
  constexpr std::uint32_t kByteMask = 0xFF;
  const auto bits = static_cast<std::uint32_t>(packed);
  switch (static_cast<CameraCommand>(command)) {
    case CameraCommand::kCreate:
      state.definition = static_cast<std::uint8_t>(bits >> kByteBits);
      state.lifetime = static_cast<std::uint8_t>(bits);
      state.mode = WrapSigned16(bits >> kModeShift);
      state.age = 0;
      host.CaptureCamera(state.start_camera);
      host.CaptureCamera(state.end_camera);
      host.BeginCameraMotion(static_cast<std::int16_t>(bits & kByteMask));
      if (ResolvesEndpoints(state.mode)) {
        host.SetTransitionMode(kCameraTransition);
      }
      return 1;
    case CameraCommand::kTick:
      return TickEffectCamera(host, state, scratch);
    case CameraCommand::kRelease:
      state.lifetime = 1;
      return 1;
    case CameraCommand::kStop:
      return 0;
  }
  return 1;
}
}  // namespace VS::Effects::Plg128
