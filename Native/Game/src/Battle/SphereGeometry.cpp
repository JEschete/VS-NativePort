// Hand-maintained (tools/port/owned.csv).
#include "Battle/SphereGeometry.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;

constexpr std::int32_t kFixedOne = 4096;
constexpr std::int32_t kFixedPointFractionBits = 12;
constexpr std::int32_t kArcPhaseMask = 31;
constexpr std::size_t kSineSampleCount = 32;
constexpr std::size_t kCircleVertexCount = 33;
constexpr std::size_t kMaximumWireVertices = kTargetWireVertices;
constexpr std::int32_t kVolumeScale = 32;
constexpr std::int32_t kPitchScale = 16;
constexpr std::int32_t kConeStepScale = kFixedOne / 8;
constexpr std::uint32_t kCommandClassShift = 14;
constexpr std::uint32_t kCommandArcOffsetShift = 2;
constexpr std::uint32_t kCommandArcOffsetMask = 0x7C;
constexpr std::uint32_t kCommandArcSegmentShift = 9;
constexpr std::uint16_t kCommandArcSegmentMask = 0x1C;
constexpr std::uint32_t kCommandArcPhaseShift = 6;
constexpr std::uint16_t kCommandArcPhaseMask = 0x1C;
constexpr std::uint32_t kCommandRadiusShift = 10;
constexpr std::uint16_t kCommandRadiusMask = 7;
constexpr std::uint16_t kCommandPayloadMask = 0x3FF;
constexpr std::size_t kArcBasisValueCount = 16;
constexpr std::size_t kArcBasisStride = 4;
constexpr std::size_t kArcBasisClearBegin = 4;
constexpr std::size_t kArcBasisClearEnd = 11;
constexpr std::uint32_t kArcBasisIndexMask = 0xF;
constexpr std::uint32_t kArcBasisFirstSelector = 0x546465U;
constexpr std::uint32_t kArcBasisSecondSelector = 0x8A998AU;
constexpr std::size_t kConeRibCount = 4;
constexpr std::size_t kMirroredCoordinateCount = 3;
constexpr std::int32_t kMaximumArcSegments = 32;
constexpr std::int32_t kMaximumConeRadius = 8;
constexpr std::int32_t kSecondCircleRadiusThreshold = 5;
constexpr std::int32_t kConePhaseScale = 4;
constexpr std::int32_t kFirstConeCircleAngleScale = 7;
constexpr std::int32_t kSecondConeCircleAngleScale = 4;
constexpr std::int32_t kQuarterTurnCount = 4;
constexpr std::int32_t kAnimatedWireframeTypeCount = 3;
constexpr std::int32_t kMaximumWireframeType = 4;
constexpr std::int32_t kAnimatedCommandShapeStride = 16;
constexpr std::int32_t kStaticCommandShapeOffset = 0x2D;
constexpr std::int32_t kPhaseAAnimationScale = 3;
constexpr std::int32_t kPhaseBAnimationScale = 7;
constexpr std::int32_t kPhaseCAnimationScale = 11;
constexpr std::int32_t kShapePhaseMultiplier = 2;

std::int16_t WrapShort(std::int32_t value) noexcept {
  return math::WrapSigned16(static_cast<std::uint32_t>(value));
}

std::int16_t SaturateShort(std::int32_t value) noexcept {
  return static_cast<std::int16_t>(std::clamp(
      value,
      static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
      static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())));
}

bool CanWrite(std::size_t size, std::size_t index, std::size_t count) noexcept {
  return index <= size && count <= size - index;
}

bool Contains(std::size_t size, std::size_t offset,
              std::size_t count) noexcept {
  return offset <= size && count <= size - offset;
}

std::int32_t Phase(std::int32_t angle, std::uint32_t offset) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(angle) + offset);
}

std::int32_t Coordinate(std::int16_t origin, std::int16_t cosine_axis,
                        std::int32_t cosine, std::int16_t sine_axis,
                        std::int32_t sine) noexcept {
  const std::int32_t cosine_product =
      math::WrappingMultiply(cosine_axis, cosine);
  const std::int32_t sine_product = math::WrappingMultiply(sine_axis, sine);
  return math::WrappingAdd(
      origin, math::WrappingAdd(cosine_product >> kFixedPointFractionBits,
                                sine_product >> kFixedPointFractionBits));
}

TargetWireVertex MakeVertex(std::int32_t x, std::int32_t y, std::int32_t z,
                            std::int32_t segment) noexcept {
  return {.position = {WrapShort(x), WrapShort(y), WrapShort(z)},
          .segment = WrapShort(segment)};
}

bool ArcTableRange(const SphereGeometryTables& tables,
                   std::uint16_t command) noexcept {
  const std::size_t offset =
      (static_cast<std::uint32_t>(command) << kCommandArcOffsetShift) &
      kCommandArcOffsetMask;
  return Contains(tables.arc_bases.size(), offset, kArcBasisStride);
}

std::size_t ArcVertexCount(std::uint16_t command) noexcept {
  std::size_t segments =
      (command >> kCommandArcSegmentShift) & kCommandArcSegmentMask;
  if (segments == 0) segments = kMaximumArcSegments;
  return segments + 1;
}

std::size_t ConeRadius(std::uint16_t command) noexcept {
  return ((command >> kCommandRadiusShift) & kCommandRadiusMask) + 1U;
}

std::size_t MirroredVertexCount(std::uint16_t command) noexcept {
  return ((command >> kCommandRadiusShift) & kCommandRadiusMask) + 2U;
}

std::optional<std::size_t> CommandVertexCount(
    std::uint16_t command, const SphereGeometryTables& tables) noexcept {
  switch (command >> kCommandClassShift) {
    case 0:
      if (!ArcTableRange(tables, command) ||
          tables.sine_samples.size() < kSineSampleCount) {
        return std::nullopt;
      }
      return ArcVertexCount(command);
    case 1: {
      const std::size_t radius = ConeRadius(command);
      return kConeRibCount * (radius + 1U) + kCircleVertexCount * 2U;
    }
    case 2: {
      const std::size_t offset = command & kCommandPayloadMask;
      const std::size_t count = MirroredVertexCount(command);
      if (!Contains(tables.mirrored_vertices.size(), offset,
                    count * kMirroredCoordinateCount)) {
        return std::nullopt;
      }
      return count * 2U;
    }
    default:
      return 0;
  }
}

bool AddVertexCount(std::size_t addition, std::size_t& total) noexcept {
  if (addition > kMaximumWireVertices - total) return false;
  total += addition;
  return true;
}

std::optional<std::size_t> RequiredWireframeVertices(
    std::size_t command_start, std::size_t command_end,
    const SphereGeometryTables& tables) noexcept {
  std::size_t total = 0;
  for (std::size_t command_index = command_start; command_index < command_end;
       ++command_index) {
    const auto command = tables.commands[command_index];
    const auto count = CommandVertexCount(command, tables);
    if (!count || !AddVertexCount(*count, total)) return std::nullopt;
  }
  return total;
}

SphereArcBasis ArcBasisFromCommand(const SphereGeometryTables& tables,
                                   std::uint16_t command) noexcept {
  std::array<std::int16_t, kArcBasisValueCount> basis{};
  const std::size_t offset =
      (static_cast<std::uint32_t>(command) << kCommandArcOffsetShift) &
      kCommandArcOffsetMask;
  for (std::size_t axis = 0; axis < kTargetWireAxes; ++axis) {
    basis[axis] = tables.arc_bases[offset + axis];
  }
  for (std::size_t index = kArcBasisClearBegin; index < kArcBasisClearEnd;
       ++index) {
    basis[index] = 0;
  }

  const std::uint32_t selector = (command >> 3U) & 0x1CU;
  const std::int16_t magnitude = tables.arc_bases[offset + 3];
  basis[(kArcBasisFirstSelector >> selector) & kArcBasisIndexMask] = magnitude;
  basis[(kArcBasisSecondSelector >> selector) & kArcBasisIndexMask] = magnitude;

  SphereArcBasis result{};
  for (std::size_t axis = 0; axis < result.size(); ++axis) {
    for (std::size_t component = 0; component < result[axis].size();
         ++component) {
      result[axis][component] = basis[axis * kArcBasisStride + component];
    }
  }
  return result;
}

void AppendMirroredVertices(std::uint16_t command,
                            const SphereGeometryTables& tables,
                            std::span<TargetWireVertex> output,
                            std::size_t& index) noexcept {
  const std::size_t source_offset = command & 0x3FFU;
  const std::size_t count = MirroredVertexCount(command);
  for (std::size_t pass = 0; pass < 2; ++pass) {
    for (std::size_t point = 0; point < count; ++point) {
      const std::size_t source =
          source_offset + point * kMirroredCoordinateCount;
      const auto x = tables.mirrored_vertices[source];
      const auto y = tables.mirrored_vertices[source + 1U];
      const auto z = tables.mirrored_vertices[source + 2U];
      output[index++] = MakeVertex(
          pass == 0 ? x : WrapShort(-static_cast<std::int32_t>(x)), y,
          pass == 0 ? z : WrapShort(-static_cast<std::int32_t>(z)),
          point == 0 ? 0 : 1);
    }
  }
}
}  // namespace

// decomp: BATTLE.PRG func_800C085C (src/BATTLE/BATTLE.PRG/57B1C.c:175).
Core::Math::Mat3 BuildSphereVolumeTransform(const TargetVolumeExtents& extents,
                                            std::int32_t angle) noexcept {
  const auto rotation = math::BuildEulerRotationMatrix(
      {-static_cast<std::int32_t>(extents[3]) * kPitchScale, angle, 0});
  math::Mat3 transform;
  for (std::size_t axis = 0; axis < kTargetWireAxes; ++axis) {
    for (std::size_t component = 0; component < kTargetWireAxes; ++component) {
      const std::int32_t scale =
          static_cast<std::int32_t>(extents[component]) * kVolumeScale;
      const std::int32_t product =
          math::WrappingMultiply(rotation.m[axis][component], scale);
      transform.m[axis][component] =
          SaturateShort(product >> kFixedPointFractionBits);
    }
  }
  return transform;
}

// decomp: BATTLE.PRG func_800C0758 (src/BATTLE/BATTLE.PRG/57B1C.c:147).
std::optional<std::size_t> AppendSphereArcVertices(
    const SphereArcBasis& basis, std::span<const std::int16_t> sine_samples,
    std::int32_t phase, std::int32_t segments,
    std::span<TargetWireVertex> output, std::size_t index) noexcept {
  if (segments < 0 || segments > kMaximumArcSegments ||
      sine_samples.size() < kSineSampleCount ||
      !CanWrite(output.size(), index,
                static_cast<std::size_t>(segments) + 1U)) {
    return std::nullopt;
  }

  for (std::int32_t point = 0; point <= segments; ++point) {
    const std::uint32_t sample =
        static_cast<std::uint32_t>(phase) + static_cast<std::uint32_t>(point);
    const auto cosine = sine_samples[(sample + 8U) & kArcPhaseMask];
    const auto sine = sine_samples[sample & kArcPhaseMask];
    const std::int32_t segment =
        point == 0
            ? 0
            : 1 + (segments != kMaximumArcSegments && segments - point < 2 ? 1
                                                                           : 0);
    output[index++] = MakeVertex(
        Coordinate(basis[0][0], basis[1][0], cosine, basis[2][0], sine),
        Coordinate(basis[0][1], basis[1][1], cosine, basis[2][1], sine),
        Coordinate(basis[0][2], basis[1][2], cosine, basis[2][2], sine),
        segment);
  }
  return index;
}

// decomp: BATTLE.PRG func_800C2254 (src/BATTLE/BATTLE.PRG/58578.c:770).
std::optional<std::size_t> AppendSphereCircleVertices(
    const SphereArcBasis& basis, std::int32_t angle,
    std::span<TargetWireVertex> output, std::size_t index) noexcept {
  if (!CanWrite(output.size(), index, kCircleVertexCount)) {
    return std::nullopt;
  }

  for (std::size_t point = 0; point < kCircleVertexCount; ++point) {
    const std::int32_t phase =
        Phase(angle, static_cast<std::uint32_t>(point) *
                         (kFixedOne / kMaximumArcSegments));
    const std::int32_t cosine = math::Cosine(phase);
    const std::int32_t sine = math::Sine(phase);
    output[index++] = MakeVertex(
        Coordinate(basis[0][0], basis[1][0], cosine, basis[2][0], sine),
        Coordinate(basis[0][1], basis[1][1], cosine, basis[2][1], sine),
        Coordinate(basis[0][2], basis[1][2], cosine, basis[2][2], sine),
        point == 0 ? 0 : 1);
  }
  return index;
}

// decomp: BATTLE.PRG func_800C2368 (src/BATTLE/BATTLE.PRG/58578.c:794).
std::optional<SphereConeAppendResult> AppendSphereConeVertices(
    std::int32_t step, std::int32_t radius, std::span<TargetWireVertex> output,
    std::size_t index) noexcept {
  if (step < 0 || step > kCommandPayloadMask || radius < 0 ||
      radius > kMaximumConeRadius) {
    return std::nullopt;
  }
  const std::size_t rib_count =
      kConeRibCount * (static_cast<std::size_t>(radius) + 1U);
  const std::int32_t scaled_radius = radius * kConeStepScale;
  const std::size_t circle_count = scaled_radius >= kSecondCircleRadiusThreshold
                                       ? kCircleVertexCount * 2U
                                       : kCircleVertexCount;
  if (!CanWrite(output.size(), index, rib_count + circle_count)) {
    return std::nullopt;
  }

  for (std::int32_t base = 0; base < kFixedOne;
       base += kFixedOne / kQuarterTurnCount) {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t phase = base - step * kConePhaseScale;
    for (std::int32_t point = 0; point <= radius; ++point) {
      output[index++] =
          MakeVertex((x * math::Cosine(phase)) >> kFixedPointFractionBits, y,
                     (z * math::Sine(phase)) >> kFixedPointFractionBits,
                     point == 0 ? 0 : 1);
      x += kConeStepScale;
      y -= kConeStepScale;
      z += kConeStepScale;
      phase += step;
    }
  }

  SphereArcBasis basis{};
  basis[0][1] = WrapShort(-scaled_radius);
  basis[1][0] = WrapShort(scaled_radius);
  basis[2][2] = WrapShort(scaled_radius);
  const auto first_circle = AppendSphereCircleVertices(
      basis, step * kFirstConeCircleAngleScale, output, index);
  if (!first_circle) return std::nullopt;
  index = *first_circle;
  if (scaled_radius >= kSecondCircleRadiusThreshold) {
    basis[0][1] = -kFixedOne / 2;
    basis[1][0] = kFixedOne / 2;
    basis[2][2] = kFixedOne / 2;
    const auto second_circle = AppendSphereCircleVertices(
        basis, step * kSecondConeCircleAngleScale, output, index);
    if (!second_circle) return std::nullopt;
    index = *second_circle;
  }
  return SphereConeAppendResult{.next_index = index, .basis = basis};
}

// decomp: BATTLE.PRG func_800C253C (src/BATTLE/BATTLE.PRG/58578.c:887).
std::optional<SphereWireframeBuildResult> BuildSphereWireframe(
    std::int32_t type, SphereWireframeState state,
    const SphereGeometryTables& tables,
    std::span<TargetWireVertex> output) noexcept {
  if (type < 0 || type > kMaximumWireframeType) return std::nullopt;

  const std::int32_t shape = state.shape;
  if (shape != 0) {
    state.shape = WrapShort(shape - 1);
  }
  std::int32_t command_shape = shape;
  if (type < kAnimatedWireframeTypeCount) {
    if (type == 0) {
      state.phase_a = WrapShort(static_cast<std::int32_t>(state.phase_a) +
                                (shape + 1) * kPhaseAAnimationScale);
      state.phase_c = WrapShort(static_cast<std::int32_t>(state.phase_c) +
                                (shape + 1) * kPhaseCAnimationScale);
    }
    state.phase_b =
        WrapShort(static_cast<std::int32_t>(state.phase_b) +
                  (shape * kShapePhaseMultiplier + 1) * kPhaseBAnimationScale);
    command_shape += type * kAnimatedCommandShapeStride;
  } else {
    command_shape = type + kStaticCommandShapeOffset;
  }
  if (command_shape < 0 ||
      !Contains(tables.shape_command_starts.size(),
                static_cast<std::size_t>(command_shape), 2)) {
    return std::nullopt;
  }

  const std::size_t command_start = tables.shape_command_starts[command_shape];
  const std::size_t command_end =
      tables.shape_command_starts[command_shape + 1];
  if (command_end < command_start || command_end > tables.commands.size()) {
    return std::nullopt;
  }
  const auto required =
      RequiredWireframeVertices(command_start, command_end, tables);
  if (!required || *required > output.size() ||
      *required > kMaximumWireVertices) {
    return std::nullopt;
  }

  std::array<TargetWireVertex, kMaximumWireVertices> generated{};
  std::span<TargetWireVertex> vertices(generated);
  std::size_t index = 0;
  SphereArcBasis basis{};
  for (std::size_t command_index = command_start; command_index < command_end;
       ++command_index) {
    const std::uint16_t command = tables.commands[command_index];
    switch (command >> kCommandClassShift) {
      case 0: {
        basis = ArcBasisFromCommand(tables, command);
        std::int32_t segments = static_cast<std::int32_t>(
            (command >> kCommandArcSegmentShift) & kCommandArcSegmentMask);
        if (segments == 0) segments = kMaximumArcSegments;
        const auto next = AppendSphereArcVertices(
            basis, tables.sine_samples,
            static_cast<std::int32_t>((command >> kCommandArcPhaseShift) &
                                      kCommandArcPhaseMask),
            segments, vertices, index);
        if (!next) return std::nullopt;
        index = *next;
        break;
      }
      case 1: {
        const auto appended = AppendSphereConeVertices(
            static_cast<std::int32_t>(command & kCommandPayloadMask),
            static_cast<std::int32_t>(ConeRadius(command)), vertices, index);
        if (!appended) return std::nullopt;
        index = appended->next_index;
        basis = appended->basis;
        break;
      }
      case 2:
        AppendMirroredVertices(command, tables, vertices, index);
        break;
      default:
        break;
    }
  }
  std::copy_n(generated.begin(), index, output.begin());
  return SphereWireframeBuildResult{.state = state, .vertex_count = index};
}
}  // namespace VS::Battle
