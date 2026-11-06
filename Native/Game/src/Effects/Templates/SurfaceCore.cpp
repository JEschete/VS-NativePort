// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/SurfaceCore.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/RingCore.hpp"

// The code the five templates share (decomp src/EFFECT/grids.h:23,
// meshes10.h:29, surfaces.h:34, mirroredSurfaces.h:34 and
// rotatingSurfaces.h:34): curve sampling with the anchor switch, the doubled
// colour channels, the tick event and the lifetime, and for the three sheets
// their create, motion, row colours and drawing.
namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec3;

// Row intensities without a transparency curve, and the value an exact 256
// reads as.
constexpr std::int32_t kFullIntensity = 255;
constexpr std::int32_t kChannelLimit = 256;
constexpr std::uint8_t kNeutralChannel = 128;
constexpr std::int32_t kSampleUnit = 128;
constexpr std::int32_t kEventScale = 4096;
constexpr std::int32_t kQuadDepthDivisor = 4;

[[nodiscard]] std::uint8_t ScaledChannel(
    std::int32_t factor, std::optional<std::int32_t> sample) noexcept {
  const std::int32_t value =
      sample.has_value() ? math::WrappingMultiply(factor, *sample) / kSampleUnit
                         : factor;
  return static_cast<std::uint8_t>(value);
}
}  // namespace

std::int32_t SurfaceRemainder(std::int32_t dividend,
                              std::int32_t divisor) noexcept {
  switch (divisor) {
    case 0:
      return dividend;
    case -1:
      return 0;
    default:
      return dividend % divisor;
  }
}

Vec3 SampleSurfaceRange(const RingVectorRange& range, std::uint8_t curve,
                        std::int32_t age, SurfaceHost& host) {
  const std::int32_t weight = host.SampleCurve(curve, age);
  return math::InterpolateSevenBitVector(range, weight);
}

Vec3 SampleSurfacePosition(const SurfaceTickInputs& inputs, std::int32_t age,
                           SurfaceHost& host) {
  const SurfaceDefinition& definition = inputs.definition;
  const Vec3 sampled = SampleSurfaceRange(
      definition.position, definition.curves.position, age, host);
  Vec3 offset{};
  switch (static_cast<RingAnchor>(definition.flags & kRingAnchorMask)) {
    case RingAnchor::kEffectOrigin:
      offset = inputs.effect_origin;
      break;
    case RingAnchor::kActor:
      offset = inputs.actor_translation;
      break;
    default:
      return sampled;
  }
  return {math::WrappingAdd(sampled.x, offset.x),
          math::WrappingAdd(sampled.y, offset.y),
          math::WrappingAdd(sampled.z, offset.z)};
}

SurfaceForces SampleSurfaceForces(const SurfaceDefinition& definition,
                                  std::int32_t age, SurfaceHost& host) {
  const std::int32_t force = host.SampleCurve(definition.curves.force, age);
  const std::int32_t acceleration =
      host.SampleCurve(definition.curves.acceleration, age);
  SurfaceForces forces;
  forces.force_min = SurfaceHalfwords(
      math::InterpolateSevenBitVector(definition.force_min, force));
  forces.force_max = SurfaceHalfwords(
      math::InterpolateSevenBitVector(definition.force_max, force));
  forces.acceleration_min = SurfaceHalfwords(math::InterpolateSevenBitVector(
      definition.acceleration_min, acceleration));
  forces.acceleration_max = SurfaceHalfwords(math::InterpolateSevenBitVector(
      definition.acceleration_max, acceleration));
  return forces;
}

std::uint8_t DoubledCurveChannel(std::int32_t curve, std::int32_t age,
                                 SurfaceHost& host) {
  if (curve == 0) {
    return kNeutralChannel;
  }
  const std::int32_t doubled =
      math::WrappingMultiply(host.SampleCurve(curve, age), 2);
  if (doubled >= kChannelLimit) {
    return std::numeric_limits<std::uint8_t>::max();
  }
  return static_cast<std::uint8_t>(doubled);
}

void EmitSurfaceTickEvent(const SurfaceDefinition& definition, Vec3 position,
                          SurfaceHost& host) {
  if (definition.tick_event == 0) {
    return;
  }
  const bool through_actor = (definition.flags & kRingEffectView) != 0;
  const Vec3 at =
      through_actor ? host.TransformByActor(RingPointAt(position)) : position;
  const auto event = static_cast<std::uint8_t>(definition.tick_event - 1);
  host.EmitParticleEvent(event, {math::WrappingMultiply(at.x, kEventScale),
                                 math::WrappingMultiply(at.y, kEventScale),
                                 math::WrappingMultiply(at.z, kEventScale)});
}

// decomp: surfaces.h:75-93, mirroredSurfaces.h:75-93 and
// rotatingSurfaces.h:83-101 (the row intensities of the create command).
// The original read past a one-entry curve for the last row; that row reads
// the curve's only entry here.
std::array<std::int32_t, kMirrorSurfaceRows> SheetFactors(
    std::span<const std::uint8_t> curve, std::size_t rows) noexcept {
  std::array<std::int32_t, kMirrorSurfaceRows> factors{};
  const std::size_t count = std::min(rows, factors.size());
  if (curve.empty() || count < 2) {
    std::fill_n(factors.begin(), count, kFullIntensity);
    return factors;
  }
  const std::size_t span = curve.size() >= 2 ? curve.size() - 1 : curve.size();
  const std::size_t steps = count - 1;
  for (std::size_t row = 0; row < count; ++row) {
    const std::size_t entry = std::min((span * row) / steps, curve.size() - 1);
    const std::int32_t factor = std::int32_t{curve[entry]} * 2;
    factors.at(row) = factor == kChannelLimit ? kFullIntensity : factor;
  }
  return factors;
}

void CreateSheet(RingPlan plan, const SurfaceDefinition& definition,
                 std::size_t rows, SheetHost& host) {
  const std::uint8_t curve = definition.transparency_curve;
  const std::span<const std::uint8_t> entries =
      curve != 0 ? host.FactorCurve(curve) : std::span<const std::uint8_t>{};
  SheetState state{.lifetime = plan.lifetime,
                   .definition_index = plan.definition_index};
  state.factors = SheetFactors(entries, rows);
  host.Create(state, SurfaceAnimation{.table = definition.ring_segments,
                                      .program = definition.ring_positions});
}

// decomp: grids.h:77-94, surfaces.h:100-118, mirroredSurfaces.h:100-118,
// rotatingSurfaces.h:108-125 and meshes10.h:65-82 (the flags, the anchored
// position and the extent).
void SampleSurfacePlacement(const SurfaceTickInputs& inputs, std::int32_t age,
                            SurfaceScratch& scratch, SurfaceHost& host) {
  const SurfaceDefinition& definition = inputs.definition;
  scratch.flags = definition.flags;
  scratch.position = SampleSurfacePosition(inputs, age, host);
  scratch.extent = SampleSurfaceRange(definition.extent,
                                      definition.curves.extent, age, host);
}

// decomp: surfaces.h:98-136, mirroredSurfaces.h:98-136 and
// rotatingSurfaces.h:106-141 (the sampling and the radius and height).
std::optional<SheetFrame> BeginSheetTick(SheetHost& host) {
  std::optional<SheetFrame> opened = OpenSurfaceTick<SheetFrame>(host);
  if (!opened.has_value()) {
    return std::nullopt;
  }
  SheetFrame& frame = *opened;
  const SurfaceDefinition& definition = frame.inputs.definition;
  const SurfaceCurves& curves = definition.curves;
  const std::int32_t age = frame.state.age;
  SurfaceScratch& scratch = frame.scratch;
  const SurfaceForces forces = SampleSurfaceForces(definition, age, host);
  scratch.forces = forces;
  scratch.direction_jitter = SampleSurfaceRange(
      definition.direction_jitter, curves.direction_jitter, age, host);
  scratch.speed = math::InterpolateSevenBitPlane(
      definition.speed, host.SampleCurve(curves.speed, age));
  SheetState& next = frame.state;
  next.height_speed =
      math::WrappingAdd(next.height_speed, forces.acceleration_min.y);
  next.height = math::WrappingAdd(
      next.height, math::WrappingAdd(forces.force_min.y, next.height_speed));
  frame.height = math::WrappingAdd(next.height, scratch.extent.y);
  next.radius_speed =
      math::WrappingAdd(next.radius_speed, forces.acceleration_min.x);
  next.radius = math::WrappingAdd(
      next.radius, math::WrappingAdd(forces.force_min.x, next.radius_speed));
  frame.radius = math::WrappingAdd(next.radius, scratch.extent.x);
  for (std::size_t channel = 0; channel < kColorChannels; ++channel) {
    const std::uint8_t curve = curves.color.at(channel);
    if (curve != 0) {
      frame.color_samples.at(channel) = host.SampleCurve(curve, age);
    }
  }
  return opened;
}

void GrowSheetRow(SheetRowGrowth& growth,
                  const SurfaceForces& forces) noexcept {
  growth.radius_step =
      math::WrappingAdd(growth.radius_step, forces.acceleration_max.x);
  growth.radius = math::WrappingAdd(
      growth.radius, math::WrappingAdd(growth.radius_step, forces.force_max.x));
  growth.height_step =
      math::WrappingAdd(growth.height_step, forces.acceleration_max.y);
  growth.height = math::WrappingAdd(
      growth.height, math::WrappingAdd(growth.height_step, forces.force_max.y));
}

// decomp: surfaces.h:137-152 and mirroredSurfaces.h:137-172 (the points of
// each row): x and z scale by the radius and y by the height, a flipped row
// negating the profile's y before the product.
SheetPoints ScaleSheetProfile(const SheetFrame& frame, SurfaceProfile profile,
                              std::span<const SheetProfileRow> rows) {
  SheetPoints points{};
  const Vec3 origin = frame.scratch.position;
  const SurfaceForces forces = frame.scratch.forces.value_or(SurfaceForces{});
  SheetRowGrowth growth{.radius = frame.radius, .height = frame.height};
  std::size_t next = 0;
  for (const SheetProfileRow& source :
       rows.first(std::min(rows.size(), kMirrorSurfaceRows))) {
    const std::size_t first =
        std::min(source.row, kSurfaceRows - 1) * kSurfaceColumns;
    for (const RingPoint& unit : profile.subspan(first, kSurfaceColumns)) {
      const std::int32_t lift =
          source.flipped ? -std::int32_t{unit.y} : std::int32_t{unit.y};
      points.at(next++) = RingPointAt(
          {math::WrappingAdd(origin.x, RingFixed(unit.x, growth.radius)),
           math::WrappingAdd(origin.y, RingFixed(lift, growth.height)),
           math::WrappingAdd(origin.z, RingFixed(unit.z, growth.radius))});
    }
    GrowSheetRow(growth, forces);
  }
  return points;
}

RingColor SheetRowColor(const SheetFrame& frame, std::size_t row) noexcept {
  const std::int32_t factor = frame.state.factors.at(row);
  const auto& samples = frame.color_samples;
  return {.r = ScaledChannel(factor, samples[0]),
          .g = ScaledChannel(factor, samples[1]),
          .b = ScaledChannel(factor, samples[2])};
}

RingQuadUvs SheetQuadUvs(const RingTexture& texture, SurfaceTextureSpan span,
                         std::size_t rows, RingCorner corner) noexcept {
  const auto segments = static_cast<std::size_t>(span);
  const std::size_t first = corner.column % segments;
  const std::size_t band = corner.row;
  const std::size_t bands = rows - 1;
  const std::uint8_t left =
      RingTextureStep(texture.u, texture.w, first, segments);
  const std::uint8_t right =
      RingTextureStep(texture.u, texture.w, first + 1, segments);
  const std::uint8_t top = RingTextureStep(texture.v, texture.h, band, bands);
  const std::uint8_t bottom =
      RingTextureStep(texture.v, texture.h, band + 1, bands);
  return {RingUv{left, top}, RingUv{right, top}, RingUv{left, bottom},
          RingUv{right, bottom}};
}

// decomp: surfaces.h:176-251, mirroredSurfaces.h:196-272 and
// rotatingSurfaces.h:211-289: projection, texture step and the quads.
void DrawSheet(const SheetFrame& frame, std::span<const RingPoint> points,
               SurfaceTextureSpan span, SheetHost& host) {
  const std::size_t rows =
      std::min(points.size() / kSurfaceColumns, kMirrorSurfaceRows);
  host.BeginProjection((frame.inputs.definition.flags & kRingEffectView) != 0);
  std::array<RingProjection, kSheetPoints> projected{};
  std::size_t next = 0;
  for (const RingPoint& point : points.first(rows * kSurfaceColumns)) {
    projected.at(next++) = host.Project(point);
  }
  const std::optional<RingTexture> texture = host.AdvanceTexture();
  if (!texture.has_value() || rows < 2) {
    return;
  }
  std::array<RingColor, kMirrorSurfaceRows> colors{};
  for (std::size_t row = 0; row < rows; ++row) {
    colors.at(row) = SheetRowColor(frame, row);
  }
  for (std::size_t band = 0; band + 1 < rows; ++band) {
    for (std::size_t column = 0; column < kSurfaceColumns; ++column) {
      const RingQuadCorners corners = ClosedRingCorners(band, column);
      RingQuad quad{.uvs = SheetQuadUvs(*texture, span, rows,
                                        {.row = band, .column = column}),
                    .clut = texture->clut,
                    .tpage = texture->tpage};
      std::int32_t total = 0;
      for (std::size_t corner = 0; corner < kRingQuadCorners; ++corner) {
        const RingCorner at = corners.at(corner);
        const RingProjection& point =
            projected.at((at.row * kSurfaceColumns) + at.column);
        total += point.depth;
        quad.screens.at(corner) = point.screen;
        quad.colors.at(corner) = colors.at(at.row);
      }
      const std::int32_t depth = total / kQuadDepthDivisor;
      if (SurfaceDepthVisible(depth, frame.inputs.near_clip)) {
        host.Submit(EncodeRingPacket(quad, 0), depth);
      }
    }
  }
}

bool FinishSheetTick(SheetFrame& frame, SheetHost& host) {
  host.PublishScratch(frame.scratch);
  return FinishSurfaceTick(frame.state, host);
}
}  // namespace VS::Effects::Templates
