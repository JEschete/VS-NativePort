// Hand-maintained (tools/port/owned.csv).
#include "Effects/Transforms.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Curves.hpp"

namespace VS::Effects {
namespace {
namespace Math = VS::Core::Math;

// Definition flags (5BF94.c:5940-6310).
constexpr std::uint16_t kAnimatedLocal = 0x1;
constexpr std::uint16_t kAttached = 0x2;
constexpr std::uint16_t kKeepEndpointBone = 0x4;
constexpr std::uint16_t kAttachSourceMask = 0x18;
constexpr std::uint16_t kAttachToEmitter = 0x8;
constexpr std::uint16_t kAttachToReceiver = 0x10;
constexpr std::uint16_t kTrackAttachment = 0x20;
constexpr std::uint16_t kAbsolutePosition = 0x40;
constexpr std::uint16_t kTargeted = 0x80;
constexpr std::uint16_t kTrackTarget = 0x100;
constexpr std::uint16_t kKeepTargetBone = 0x200;
constexpr std::uint16_t kTargetSourceMask = 0xC00;
constexpr std::uint16_t kTargetEmitter = 0x400;
constexpr std::uint16_t kTargetReceiver = 0x800;
constexpr std::uint16_t kMotionMask = 0x3000;
constexpr std::uint16_t kLinearMotion = 0x1000;
constexpr std::uint16_t kSplineMotion = 0x2000;
constexpr std::uint16_t kFacingMask = 0xC000;
constexpr std::uint16_t kFaceSavedTarget = 0x4000;
constexpr std::uint16_t kFaceTarget = 0x8000;

// State flags.
constexpr std::uint16_t kAttachmentResolved = 0x1;
constexpr std::uint16_t kTargetResolved = 0x2;
constexpr std::uint16_t kFrameAdvances = 0x4;
constexpr std::uint16_t kSplineBuilt = 0x8;

// Definition mode: the target actor (low five bits), the easing (bits 5-6),
// the direction (bit 7) and the target bone (high byte).
constexpr std::uint16_t kTargetActorMask = 0x1F;
constexpr int kEasingShift = 5;
constexpr std::uint16_t kEasingMask = 0x3;
constexpr std::uint16_t kFromSavedTarget = 0x80;
constexpr int kTargetBoneShift = 8;

constexpr std::int32_t kOne = 4096;
constexpr std::int32_t kHalf = kOne / 2;
constexpr int kFractionBits = 12;
constexpr unsigned kHalfwordBits = 16;
// A sampled curve weights the line over 128 steps (5BF94.c:6037).
constexpr std::int32_t kCurveSteps = 128;
// A sampled curve spans the spline's three segments in 128 steps
// (96 = 3 * 4096 / 128, 5BF94.c:6066).
constexpr std::int32_t kCurveSplineStep = 96;
constexpr std::int32_t kSplineSegments = 3;
// The spline shape: control angle (low nibble) and jitter (second nibble),
// first and second control depth in eighths of the line (high nibbles).
constexpr std::uint16_t kNibble = 0xF;
constexpr int kNibbleBits = 4;
constexpr int kFirstDepthShift = 8;
constexpr int kSecondDepthShift = 12;
constexpr std::int32_t kDepthEighths = 8;
constexpr std::uint16_t kAngleJitterMask = 0xF0;
constexpr std::uint32_t kAngleJitterRange = 0xF00;
constexpr int kAngleShift = 8;
constexpr std::int32_t kRadiusScale = 32;
// The camera-offset attachment stops 0x600 short of the eye (5BF94.c:6171).
constexpr std::int32_t kCameraStandOff = 0x600;

enum class Easing : std::uint16_t {
  kLinear = 0,
  kCosine = 1,
  kCurve = 2,
};

// Effects_TransformDefinition (5BF94.c:200) field offsets.
constexpr std::size_t kAttachActorAt = 0x02;
constexpr std::size_t kAttachBoneAt = 0x03;
constexpr std::size_t kTranslationCurveAt = 0x04;
constexpr std::size_t kCurveAt = 0x05;
constexpr std::size_t kRotationCurveAt = 0x06;
constexpr std::size_t kScaleCurveAt = 0x07;
constexpr std::size_t kRotationXAt = 0x08;
constexpr std::size_t kModeAt = 0x0A;
constexpr std::size_t kRotationYAt = 0x0C;
constexpr std::size_t kLimitAt = 0x0E;
constexpr std::size_t kRadiusAt = 0x0F;
constexpr std::size_t kRotationZAt = 0x10;
constexpr std::size_t kShapeAt = 0x12;
constexpr std::size_t kRotationRangeAt = 0x14;
constexpr std::size_t kTranslationRangeAt = 0x20;
constexpr std::size_t kScaleRangeAt = 0x2C;
// A (start, end) halfword pair.
constexpr std::size_t kRangePairBytes = 4;
constexpr int kByteBits = 8;

using DefinitionRecord = std::span<const std::byte, kTransformDefinitionBytes>;

[[nodiscard]] std::uint8_t ByteOf(DefinitionRecord record,
                                  std::size_t at) noexcept {
  return std::to_integer<std::uint8_t>(record[at]);
}

[[nodiscard]] std::uint16_t HalfwordOf(DefinitionRecord record,
                                       std::size_t at) noexcept {
  return static_cast<std::uint16_t>(ByteOf(record, at) |
                                    (ByteOf(record, at + 1) << kByteBits));
}

[[nodiscard]] std::array<Math::ScalarSpan, Math::kMatrixOrder> RangesOf(
    DefinitionRecord record, std::size_t at) noexcept {
  std::array<Math::ScalarSpan, Math::kMatrixOrder> ranges{};
  for (std::size_t axis = 0; axis < ranges.size(); ++axis) {
    const std::size_t pair = at + (axis * kRangePairBytes);
    ranges.at(axis) = {
        .start = static_cast<std::int16_t>(HalfwordOf(record, pair)),
        .end = static_cast<std::int16_t>(HalfwordOf(record, pair + 2))};
  }
  return ranges;
}

[[nodiscard]] TransformDefinition DefinitionOf(
    DefinitionRecord record) noexcept {
  const std::uint8_t radius = ByteOf(record, kRadiusAt);
  return {.flags = HalfwordOf(record, 0),
          .attach_actor = ByteOf(record, kAttachActorAt),
          .attach_bone = ByteOf(record, kAttachBoneAt),
          .translation_curve = ByteOf(record, kTranslationCurveAt),
          .curve = ByteOf(record, kCurveAt),
          .rotation_curve = ByteOf(record, kRotationCurveAt),
          .scale_curve = ByteOf(record, kScaleCurveAt),
          .rotation_x = HalfwordOf(record, kRotationXAt),
          .mode = HalfwordOf(record, kModeAt),
          .rotation_y = HalfwordOf(record, kRotationYAt),
          .limit = ByteOf(record, kLimitAt),
          .radius = static_cast<std::uint8_t>(radius & kNibble),
          .radius_jitter = static_cast<std::uint8_t>(radius >> kNibbleBits),
          .rotation_z = HalfwordOf(record, kRotationZAt),
          .shape = HalfwordOf(record, kShapeAt),
          .rotation_range = RangesOf(record, kRotationRangeAt),
          .translation_range = RangesOf(record, kTranslationRangeAt),
          .scale_range = RangesOf(record, kScaleRangeAt)};
}

[[nodiscard]] Math::Mat3 MultiplyWrapping(const Math::Mat3& left,
                                          const Math::Mat3& right) noexcept {
  Math::Mat3 product;
  for (std::size_t i = 0; i < Math::kMatrixOrder; ++i) {
    for (std::size_t j = 0; j < Math::kMatrixOrder; ++j) {
      std::int64_t sum = 0;
      for (std::size_t k = 0; k < Math::kMatrixOrder; ++k) {
        sum += std::int64_t{left.m[i][k]} * right.m[k][j];
      }
      product.m[i][j] = static_cast<std::int16_t>(sum >> kFractionBits);
    }
  }
  return product;
}

[[nodiscard]] std::int32_t RowTranslation(const Math::Transform3& left,
                                          Math::Vec3 translation,
                                          std::size_t row) noexcept {
  const auto& m = left.rotation.m[row];
  const std::int64_t sum = (std::int64_t{m[0]} * translation.x) +
                           (std::int64_t{m[1]} * translation.y) +
                           (std::int64_t{m[2]} * translation.z);
  const std::array<std::int32_t, Math::kMatrixOrder> offset{
      left.translation.x, left.translation.y, left.translation.z};
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>((sum >> kFractionBits) + offset.at(row)));
}

// libgte CompMatrix as Gte.cpp models it: the rotation product and the right
// translation through the left transform, wrapping like the 32-bit words.
[[nodiscard]] Math::Transform3 Compose(const Math::Transform3& left,
                                       const Math::Transform3& right) noexcept {
  return {.rotation = MultiplyWrapping(left.rotation, right.rotation),
          .translation = {.x = RowTranslation(left, right.translation, 0),
                          .y = RowTranslation(left, right.translation, 1),
                          .z = RowTranslation(left, right.translation, 2)}};
}

// libgte ScaleMatrix: each column times its scale, truncated to halfwords.
[[nodiscard]] Math::Mat3 ScaleColumns(const Math::Mat3& matrix,
                                      Math::Vec3 scale) noexcept {
  const std::array<std::int32_t, Math::kMatrixOrder> columns{scale.x, scale.y,
                                                             scale.z};
  Math::Mat3 scaled;
  for (std::size_t i = 0; i < Math::kMatrixOrder; ++i) {
    for (std::size_t j = 0; j < Math::kMatrixOrder; ++j) {
      scaled.m[i][j] = static_cast<std::int16_t>(
          (std::int64_t{matrix.m[i][j]} * columns.at(j)) >> kFractionBits);
    }
  }
  return scaled;
}

[[nodiscard]] constexpr Math::Vec3 VectorOf(TransformPoint point) noexcept {
  return {.x = point.x, .y = point.y, .z = point.z};
}

[[nodiscard]] constexpr Math::Vec3 Halfwords(Math::Vec3 vector) noexcept {
  return {.x = Math::WrapSigned16(static_cast<std::uint32_t>(vector.x)),
          .y = Math::WrapSigned16(static_cast<std::uint32_t>(vector.y)),
          .z = Math::WrapSigned16(static_cast<std::uint32_t>(vector.z))};
}

void StoreXyz(TransformPoint& point, Math::Vec3 vector) noexcept {
  const Math::Vec3 halfwords = Halfwords(vector);
  point.x = static_cast<std::int16_t>(halfwords.x);
  point.y = static_cast<std::int16_t>(halfwords.y);
  point.z = static_cast<std::int16_t>(halfwords.z);
}

[[nodiscard]] constexpr Math::Vec3 Difference(Math::Vec3 to,
                                              Math::Vec3 from) noexcept {
  return {.x = Math::WrappingSubtract(to.x, from.x),
          .y = Math::WrappingSubtract(to.y, from.y),
          .z = Math::WrappingSubtract(to.z, from.z)};
}

[[nodiscard]] std::int32_t Length(Math::Vec3 delta) noexcept {
  return Math::SquareRootInteger(Math::WrappingAdd(
      Math::WrappingAdd(Math::WrappingMultiply(delta.x, delta.x),
                        Math::WrappingMultiply(delta.y, delta.y)),
      Math::WrappingMultiply(delta.z, delta.z)));
}

[[nodiscard]] constexpr Math::Vec3 Whole(Math::Vec3 fixed) noexcept {
  return {.x = fixed.x >> kFractionBits,
          .y = fixed.y >> kFractionBits,
          .z = fixed.z >> kFractionBits};
}

[[nodiscard]] constexpr Easing EasingOf(
    const TransformDefinition& definition) noexcept {
  return static_cast<Easing>((definition.mode >> kEasingShift) & kEasingMask);
}

[[nodiscard]] constexpr std::int32_t Steps(
    const TransformDefinition& definition) noexcept {
  return std::int32_t{definition.limit} + 1;
}

// The line a motion follows: from the world translation to the saved target,
// or back when the mode's direction bit is set.
struct MotionLine {
  Math::Vec3 start;
  Math::Vec3 end;
};

[[nodiscard]] MotionLine LineOf(const TransformDefinition& definition,
                                const TransformState& state,
                                bool halfword_world) noexcept {
  const Math::Vec3 saved = VectorOf(state.saved);
  const Math::Vec3 world = halfword_world ? Halfwords(state.world.translation)
                                          : state.world.translation;
  if ((definition.mode & kFromSavedTarget) != 0) {
    return {.start = saved, .end = world};
  }
  return {.start = world, .end = saved};
}

[[nodiscard]] std::int32_t SampleMotionCurve(
    const TransformDefinition& definition, const TransformState& state,
    TransformHost& host) {
  return host.SampleCurve(std::int32_t{definition.curve} + 1, state.frame)
      .value;
}

void ResolveEndpoint(const TransformEndpoint& endpoint,
                     TransformState& state) noexcept {
  if (endpoint.type == kActorPartEndpoint) {
    state.actor = endpoint.actor;
    state.bone = endpoint.part;
    return;
  }
  state.actor = kNoAttachment;
  state.attachment = Math::TranslationTransform({.x = endpoint.position.x,
                                                 .y = endpoint.position.y,
                                                 .z = endpoint.position.z});
}

void ResolveTargetEndpoint(const TransformEndpoint& endpoint,
                           TransformState& state) noexcept {
  if (endpoint.type == kActorPartEndpoint) {
    state.target_actor = endpoint.actor;
    state.target_bone = endpoint.part;
    return;
  }
  state.target_actor = kNoAttachment;
  state.saved = endpoint.position;
}

void InitializeAttachment(const TransformDefinition& definition,
                          const TransformEndpoints& endpoints,
                          TransformState& state) noexcept {
  switch (definition.flags & kAttachSourceMask) {
    case kAttachToReceiver:
      ResolveEndpoint(endpoints.emit_to, state);
      break;
    case kAttachToEmitter:
      ResolveEndpoint(endpoints.emit_from, state);
      break;
    case 0:
      state.actor = definition.attach_actor;
      state.bone = 0;
      break;
    default:
      break;
  }
  if ((definition.flags & kKeepEndpointBone) == 0) {
    state.bone = definition.attach_bone;
  }
}

void InitializeTarget(const TransformDefinition& definition,
                      const TransformEndpoints& endpoints,
                      TransformState& state) noexcept {
  switch (definition.flags & kTargetSourceMask) {
    case kTargetReceiver:
      ResolveTargetEndpoint(endpoints.emit_to, state);
      break;
    case kTargetEmitter:
      ResolveTargetEndpoint(endpoints.emit_from, state);
      break;
    case 0:
      state.target_actor =
          static_cast<std::uint8_t>(definition.mode & kTargetActorMask);
      state.target_bone = 0;
      break;
    default:
      break;
  }
  if ((definition.flags & kKeepTargetBone) == 0) {
    state.target_bone =
        static_cast<std::uint8_t>(definition.mode >> kTargetBoneShift);
  }
}

// The spline control offsets: a radius and an angle, each jittered when the
// definition asks, drawing the first control's value first.
struct ControlPair {
  std::int32_t first;
  std::int32_t second;
};

[[nodiscard]] ControlPair JitteredPair(std::int32_t base, std::int32_t range,
                                       TransformHost& host) {
  if (range == 0) {
    return {.first = base, .second = base};
  }
  const std::int32_t first = base + (host.Random() % range);
  const std::int32_t second = base + (host.Random() % range);
  return {.first = first, .second = second};
}

// A spline control around the line: its angle and radius across the line
// and its depth along it.
struct ControlPolar {
  std::int32_t angle;
  std::int32_t radius;
  std::int32_t depth;
};

[[nodiscard]] Math::Vec3 ControlOffset(ControlPolar control) noexcept {
  return Halfwords(
      {.x = Math::WrappingMultiply(Math::Cosine(control.angle),
                                   control.radius) >>
            kFractionBits,
       .y = Math::WrappingMultiply(Math::Sine(control.angle), control.radius) >>
            kFractionBits,
       .z = control.depth});
}

// The saved target's facing angle written over the attachment rotation as
// the original's word stores leave it (5BF94.c:6253): each word covers two
// halfwords, so the rotation's off-diagonal halves take the high halves.
void FaceAttachment(TransformState& state) noexcept {
  const std::int32_t cosine = Math::Cosine(state.saved.pad);
  const std::int32_t sine = Math::Sine(state.saved.pad);
  const auto low = [](std::int32_t word) {
    return Math::WrapSigned16(static_cast<std::uint32_t>(word));
  };
  const auto high = [](std::int32_t word) {
    return Math::WrapSigned16(static_cast<std::uint32_t>(word) >>
                              kHalfwordBits);
  };
  auto& m = state.attachment.rotation.m;
  m[0][0] = low(cosine);
  m[0][1] = high(cosine);
  m[0][2] = low(sine);
  m[1][0] = high(sine);
  m[1][1] = static_cast<std::int16_t>(kOne);
  m[1][2] = 0;
  m[2][0] = low(-sine);
  m[2][1] = high(-sine);
  m[2][2] = low(cosine);
}

void ResolveTarget(TransformState& state, TransformHost& host) {
  if (state.target_actor == kOriginAttachment) {
    const TransformPoint origin = host.EffectOrigin();
    state.saved.x = origin.x;
    state.saved.y = origin.y;
    state.saved.z = origin.z;
  } else {
    host.ActorBonePosition(state.target_actor, state.target_bone, state.saved);
  }
  state.flags |= kTargetResolved;
}

void FaceTarget(const TransformDefinition& definition, TransformState& state,
                const Math::Transform3& local) noexcept {
  const Math::Vec3 target = (definition.flags & kFaceSavedTarget) != 0
                                ? VectorOf(state.saved)
                                : state.attachment.translation;
  if (target != state.world.translation) {
    state.facing = {.rotation = Math::BuildLookAtOrientation(
                        state.world.translation, target),
                    .translation = state.world.translation};
  }
  state.world = Compose(state.facing, local);
}

void PlaceWorld(const TransformDefinition& definition, TransformState& state,
                const Math::Transform3& local) noexcept {
  if ((definition.flags & kAttached) == 0) {
    state.world = local;
    return;
  }
  if ((definition.flags & kFacingMask) == kFaceSavedTarget) {
    FaceAttachment(state);
  }
  state.world = Compose(state.attachment, local);
}

void MoveWorld(const TransformDefinition& definition,
               TransformState& state) noexcept {
  if ((definition.flags & kAbsolutePosition) != 0) {
    state.world.translation = state.position;
    return;
  }
  Math::Vec3& t = state.world.translation;
  t = {.x = Math::WrappingAdd(t.x, state.position.x),
       .y = Math::WrappingAdd(t.y, state.position.y),
       .z = Math::WrappingAdd(t.z, state.position.z)};
}

[[nodiscard]] bool UpdateTransform(const TransformDefinition& definition,
                                   TransformState& state, TransformHost& host) {
  const auto local = BuildAnimatedLocalTransform(definition, state.rotation,
                                                 host.CurveSamples());
  if (!local) {
    return false;
  }
  if ((definition.flags & kTargeted) != 0 &&
      state.target_actor != kNoAttachment &&
      ((definition.flags & kTrackTarget) != 0 ||
       (state.flags & kTargetResolved) == 0)) {
    ResolveTarget(state, host);
  }
  if (state.actor != kNoAttachment &&
      ((definition.flags & kTrackAttachment) != 0 ||
       (state.flags & kAttachmentResolved) == 0)) {
    static_cast<void>(ResolveTransformAttachment(state, host));
    state.flags |= kAttachmentResolved;
  }
  PlaceWorld(definition, state, *local);
  if ((definition.flags & kMotionMask) == kSplineMotion &&
      (state.flags & kSplineBuilt) == 0) {
    BuildTransformSplineControls(definition, state, host);
    state.flags |= kSplineBuilt;
  }
  if ((definition.flags & kTargeted) != 0) {
    static_cast<void>(AdvanceTransformMotion(definition, state, host));
  }
  MoveWorld(definition, state);
  if ((definition.flags & kFaceTarget) != 0) {
    FaceTarget(definition, state, *local);
  }
  state.view = Compose(host.ViewMatrix(), state.world);
  return true;
}
}  // namespace

std::size_t ReadTransformDefinitions(
    std::span<const std::byte> block,
    std::span<TransformDefinition> out) noexcept {
  const std::size_t count =
      std::min(block.size() / kTransformDefinitionBytes, out.size());
  for (std::size_t i = 0; i < count; ++i) {
    out[i] = DefinitionOf(block.subspan(i * kTransformDefinitionBytes)
                              .first<kTransformDefinitionBytes>());
  }
  return count;
}

// decomp: BATTLE.PRG func_800CFEF0 (src/BATTLE/BATTLE.PRG/5BF94.c:5940)
std::size_t InitializeTransformStates(
    std::span<const TransformDefinition> definitions,
    const TransformEndpoints& endpoints, std::span<TransformState> states) {
  const std::size_t count = std::min(definitions.size(), states.size());
  for (std::size_t i = 0; i < count; ++i) {
    const TransformDefinition& definition = definitions[i];
    TransformState& state = states[i];
    state.rotation = {static_cast<std::int16_t>(definition.rotation_x),
                      static_cast<std::int16_t>(definition.rotation_y),
                      static_cast<std::int16_t>(definition.rotation_z)};
    state.position = {};
    state.facing.rotation = Math::IdentityRotation();
    state.flags = 0;
    state.frame = 0;
    state.actor = 0;
    state.bone = 0;
    state.target_actor = 0;
    state.target_bone = 0;
    if ((definition.flags & kAttached) != 0) {
      InitializeAttachment(definition, endpoints, state);
    }
    if ((definition.flags & kTargeted) != 0) {
      InitializeTarget(definition, endpoints, state);
    }
  }
  return count;
}

// decomp: BATTLE.PRG func_800D01E4 (src/BATTLE/BATTLE.PRG/5BF94.c:6020)
TransformStatus InterpolateTransformPosition(
    const TransformDefinition& definition, TransformState& state,
    TransformHost& host) {
  const MotionLine line = LineOf(definition, state, true);
  Math::Vec3 moved;
  switch (EasingOf(definition)) {
    case Easing::kLinear:
      moved = Math::InterpolateTimedVector(
          line.start, line.end,
          {.current = state.frame, .total = Steps(definition)});
      break;
    case Easing::kCosine:
      moved = Math::InterpolateCosineVector(
          line.start, line.end,
          {.current = state.frame, .total = Steps(definition)});
      break;
    case Easing::kCurve:
      moved = Math::InterpolateTimedVector(
          line.start, line.end,
          {.current = SampleMotionCurve(definition, state, host),
           .total = kCurveSteps});
      break;
    default:
      return TransformStatus::kUnknownEasing;
  }
  state.position = Halfwords(moved);
  return TransformStatus::kApplied;
}

// decomp: BATTLE.PRG func_800D037C (src/BATTLE/BATTLE.PRG/5BF94.c:6047)
TransformStatus InterpolateTransformSpline(
    const TransformDefinition& definition, TransformState& state,
    TransformHost& host) {
  const MotionLine line = LineOf(definition, state, true);
  std::int32_t phase = 0;
  switch (EasingOf(definition)) {
    case Easing::kLinear:
      phase =
          ((state.frame * kSplineSegments * kOne) / Steps(definition)) - kOne;
      break;
    case Easing::kCosine:
      phase = ((-Math::Cosine((state.frame * kHalf) / Steps(definition)) *
                kSplineSegments) /
               2) +
              kHalf;
      break;
    case Easing::kCurve:
      phase = Math::WrappingSubtract(
          Math::WrappingMultiply(SampleMotionCurve(definition, state, host),
                                 kCurveSplineStep),
          kOne);
      break;
    default:
      return TransformStatus::kUnknownEasing;
  }
  state.position = Halfwords(
      Math::InterpolateLagrangeVector({line.start, VectorOf(state.control0),
                                       VectorOf(state.control1), line.end},
                                      phase));
  return TransformStatus::kApplied;
}

// decomp: BATTLE.PRG func_800D0548 (src/BATTLE/BATTLE.PRG/5BF94.c:6076)
TransformStatus AdvanceTransformMotion(const TransformDefinition& definition,
                                       TransformState& state,
                                       TransformHost& host) {
  TransformStatus status = TransformStatus::kApplied;
  switch (definition.flags & kMotionMask) {
    case kLinearMotion:
      status = InterpolateTransformPosition(definition, state, host);
      break;
    case kSplineMotion:
      status = InterpolateTransformSpline(definition, state, host);
      break;
    default:
      break;
  }
  if ((state.flags & kFrameAdvances) != 0 &&
      std::int32_t{definition.limit} >= state.frame) {
    state.frame = static_cast<std::int16_t>(state.frame + 1);
  }
  return status;
}

// decomp: BATTLE.PRG func_800D05F4 (src/BATTLE/BATTLE.PRG/5BF94.c:6093)
void BuildTransformSplineControls(const TransformDefinition& definition,
                                  TransformState& state, TransformHost& host) {
  const MotionLine line = LineOf(definition, state, false);
  const std::int32_t distance = Length(Difference(line.end, line.start));
  const auto depth_of = [](std::int32_t eighths) {
    return eighths - kDepthEighths;
  };
  const std::int32_t first_depth =
      Math::WrappingMultiply(
          distance,
          depth_of((definition.shape >> kFirstDepthShift) & kNibble)) /
      kDepthEighths;
  const std::int32_t second_depth = Math::WrappingAdd(
      distance, Math::WrappingMultiply(
                    distance, depth_of(definition.shape >> kSecondDepthShift)) /
                    kDepthEighths);
  const ControlPair radius =
      JitteredPair(std::int32_t{definition.radius} * kRadiusScale,
                   std::int32_t{definition.radius_jitter} * kRadiusScale, host);
  const std::int32_t angle_range =
      (definition.shape & kAngleJitterMask) != 0
          ? static_cast<std::int32_t>(
                (std::uint32_t{definition.shape} << kNibbleBits) &
                kAngleJitterRange)
          : 0;
  const ControlPair angle = JitteredPair(
      static_cast<std::int32_t>(definition.shape & kNibble) << kAngleShift,
      angle_range, host);
  const Math::Transform3 frame{
      .rotation = Math::BuildLookAtOrientation(line.start, line.end),
      .translation = line.start};
  StoreXyz(state.control0,
           host.RotateTranslate(frame, ControlOffset({.angle = angle.first,
                                                      .radius = radius.first,
                                                      .depth = first_depth})));
  StoreXyz(state.control1,
           host.RotateTranslate(frame, ControlOffset({.angle = angle.second,
                                                      .radius = radius.second,
                                                      .depth = second_depth})));
}

// decomp: BATTLE.PRG func_800D0984 (src/BATTLE/BATTLE.PRG/5BF94.c:6152)
void BuildCameraAttachment(TransformState& state, TransformHost& host,
                           bool offset_toward_camera) {
  const TransformCameraView camera = host.FrameCamera();
  const Math::Vec3 start = Whole(camera.look_at);
  const Math::Vec3 end = Whole(camera.position);
  state.attachment = {.rotation = Math::BuildLookAtOrientation(start, end),
                      .translation = start};
  if (offset_toward_camera) {
    const std::int32_t distance = Length(Difference(end, start));
    state.attachment.translation = host.RotateTranslate(
        state.attachment,
        Halfwords({.x = 0, .y = 0, .z = distance - kCameraStandOff}));
  }
}

// decomp: BATTLE.PRG func_800D0B08 (src/BATTLE/BATTLE.PRG/5BF94.c:6182)
Math::Transform3 BuildEffectOriginAttachment(TransformHost& host) {
  const TransformPoint origin = host.EffectOrigin();
  return Math::TranslationTransform(
      {.x = origin.x, .y = origin.y, .z = origin.z});
}

// decomp: BATTLE.PRG func_800D0B30 (src/BATTLE/BATTLE.PRG/5BF94.c:6184)
std::optional<Math::Transform3> BuildAnimatedLocalTransform(
    const TransformDefinition& definition,
    std::array<std::int16_t, Math::kMatrixOrder>& rotation,
    std::span<const std::int32_t> curve_samples) {
  if ((definition.flags & kAnimatedLocal) == 0) {
    return Math::Transform3{.rotation = Math::IdentityRotation(),
                            .translation = {}};
  }
  const std::size_t largest =
      std::max({definition.rotation_curve, definition.translation_curve,
                definition.scale_curve});
  if (largest >= curve_samples.size()) {
    return std::nullopt;
  }
  const Math::Vec3 offset = Halfwords(Math::InterpolateSevenBitVector(
      definition.rotation_range, curve_samples[definition.rotation_curve]));
  for (std::size_t axis = 0; axis < Math::kMatrixOrder; ++axis) {
    const std::array<std::int32_t, Math::kMatrixOrder> components{
        offset.x, offset.y, offset.z};
    rotation.at(axis) =
        static_cast<std::int16_t>(rotation.at(axis) + components.at(axis));
  }
  const Math::Mat3 turned = Math::BuildRotationXyz(
      {.x = rotation[0], .y = rotation[1], .z = rotation[2]});
  const Math::Vec3 translation = Math::InterpolateSevenBitVector(
      definition.translation_range,
      curve_samples[definition.translation_curve]);
  const Math::Vec3 scale = Math::InterpolateSevenBitVector(
      definition.scale_range, curve_samples[definition.scale_curve]);
  return Math::Transform3{.rotation = ScaleColumns(turned, scale),
                          .translation = translation};
}

// decomp: BATTLE.PRG func_800D0C60 (src/BATTLE/BATTLE.PRG/5BF94.c:6204)
TransformStatus ResolveTransformAttachment(TransformState& state,
                                           TransformHost& host) {
  if (state.actor < kCameraAttachment) {
    host.ActorAnchorMatrix(state.actor, state.bone, state.attachment);
    const std::optional<std::int16_t> facing = host.ActorFacing(state.actor);
    if (!facing) {
      return TransformStatus::kNoPlacement;
    }
    state.saved.pad = *facing;
    return TransformStatus::kApplied;
  }
  switch (state.actor) {
    case kCameraAttachment:
    case kCameraOffsetAttachment:
      BuildCameraAttachment(state, host, state.actor != kCameraAttachment);
      break;
    case kOriginAttachment:
      state.attachment = BuildEffectOriginAttachment(host);
      break;
    default:
      break;
  }
  return TransformStatus::kApplied;
}

// decomp: BATTLE.PRG func_800D0D08 (src/BATTLE/BATTLE.PRG/5BF94.c:6227)
TransformPass UpdateEffectTransforms(
    std::span<const TransformDefinition> definitions,
    std::span<TransformState> states, TransformHost& host) {
  TransformPass pass;
  const std::size_t count = std::min(definitions.size(), states.size());
  for (std::size_t i = 0; i < count; ++i) {
    if (UpdateTransform(definitions[i], states[i], host)) {
      ++pass.applied;
    } else {
      ++pass.refused;
    }
  }
  return pass;
}
}  // namespace VS::Effects
