// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ParticleControl.hpp"

#include <cstddef>
#include <cstdint>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"
#include "Effects/Templates/ParticleCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec3;

// The speeds are 1/256ths of the ribbon's span.
constexpr std::int32_t kSpeedScale = 256;

// The ranges one ribbon's inner points sample, in the original's order.
struct InnerRanges {
  Core::Math::Vec2 first_radius{};
  Core::Math::Vec2 second_radius{};
  Core::Math::Vec2 speed{};
  Vec3 direction_jitter{};
  Vec3 base_direction{};
  ParticlePoint speed_jitter{};
};

// decomp: particleSetup.h:322-333.
[[nodiscard]] InnerRanges SampleInnerRanges(const ParticleSetFrame& frame,
                                            ParticleSetHost& host) {
  const ParticleSetDefinition& definition = frame.inputs.definition;
  const ParticleSetCurves& curves = definition.curves;
  const std::int32_t age = frame.state.age;
  InnerRanges ranges{};
  ranges.first_radius =
      SampleParticlePlane(definition.first_radius, curves.mass, age, host);
  ranges.second_radius = SampleParticlePlane(definition.second_radius,
                                             curves.gravity_scale, age, host);
  ranges.speed = SampleParticlePlane(definition.speed, curves.speed, age, host);
  ranges.direction_jitter = SampleParticleRange(
      definition.direction_jitter, curves.direction_jitter, age, host);
  ranges.base_direction =
      SampleParticleRange(definition.direction, curves.direction, age, host);
  ranges.speed_jitter = ParticlePointAt(
      SampleParticleRange(definition.speed_jitter, curves.force, age, host));
  return ranges;
}

// The speed `speed` moved by a centred draw within `jitter` (none for a zero
// jitter). decomp: particleSetup.h:335-353.
[[nodiscard]] std::int32_t JitteredSpeed(std::int32_t speed,
                                         std::int16_t jitter,
                                         ParticleSetHost& host) {
  if (jitter == 0) {
    return speed;
  }
  return math::WrappingSubtract(
      math::WrappingAdd(speed, ParticleRemainder(host.Rand(), jitter)),
      ParticleHalf(jitter));
}

// `span` scaled by a speed in 1/256ths.
[[nodiscard]] std::int32_t ScaledSpan(std::int32_t span,
                                      std::int32_t speed) noexcept {
  return math::R3000Quotient(math::WrappingMultiply(span, speed), kSpeedScale);
}

// The radius `range.x` plus a draw below `range.y` (none for zero).
// decomp: particleSetup.h:355-365.
[[nodiscard]] std::int32_t JitteredRadius(Core::Math::Vec2 range,
                                          ParticleSetHost& host) {
  if (range.y == 0) {
    return range.x;
  }
  return math::WrappingAdd(range.x, ParticleRemainder(host.Rand(), range.y));
}

// The angle `base` moved by a draw across [-jitter, jitter) (none for zero).
// decomp: particleSetup.h:367-377.
[[nodiscard]] std::int32_t JitteredAngle(std::int32_t base, std::int32_t jitter,
                                         ParticleSetHost& host) {
  if (jitter == 0) {
    return base;
  }
  const std::int32_t draw =
      ParticleRemainder(host.Rand(), math::WrappingMultiply(jitter, 2));
  return math::WrappingAdd(base, math::WrappingSubtract(draw, jitter));
}

// A point `radius` out at `angle` across the frame, `depth` along it.
[[nodiscard]] Vec3 LocalPoint(std::int32_t angle, std::int32_t radius,
                              std::int32_t depth) noexcept {
  return {ParticleFixed(math::Cosine(angle), radius),
          ParticleFixed(math::Sine(angle), radius), depth};
}

// decomp: particleSetup.h:312-396, one ribbon's two inner points.
void PlaceInnerPoints(ParticleControlSet& points, ParticleSetFrame& frame,
                      ParticleSetHost& host) {
  ParticleSetScratch& scratch = frame.scratch;
  const Vec3 first_point = ParticleVector(points.front());
  const Vec3 last_point = ParticleVector(points.back());
  scratch.aim_point = math::SubtractVectors(last_point, first_point);
  const Vec3& delta = scratch.aim_point;
  const std::int32_t span = math::SquareRootInteger(math::WrappingAdd(
      math::WrappingAdd(math::WrappingMultiply(delta.x, delta.x),
                        math::WrappingMultiply(delta.y, delta.y)),
      math::WrappingMultiply(delta.z, delta.z)));
  const InnerRanges ranges = SampleInnerRanges(frame, host);
  scratch.first_radius = ranges.first_radius;
  scratch.second_radius = ranges.second_radius;
  scratch.speed = ranges.speed;
  scratch.direction_jitter = ranges.direction_jitter;
  scratch.base_direction = ranges.base_direction;
  scratch.speed_jitter = ranges.speed_jitter;
  // The draws run in the original's order: both speeds, both radii, then
  // both angles.
  const std::int32_t first_depth = ScaledSpan(
      span, JitteredSpeed(ranges.speed.x, ranges.speed_jitter.x, host));
  const std::int32_t second_depth = math::WrappingAdd(
      span, ScaledSpan(span, JitteredSpeed(ranges.speed.y,
                                           ranges.speed_jitter.y, host)));
  const std::int32_t first_radius = JitteredRadius(ranges.first_radius, host);
  const std::int32_t second_radius = JitteredRadius(ranges.second_radius, host);
  const std::int32_t first_angle =
      JitteredAngle(ranges.base_direction.x, ranges.direction_jitter.x, host);
  const std::int32_t second_angle =
      JitteredAngle(ranges.base_direction.y, ranges.direction_jitter.y, host);
  // The local points are SVECTOR records: each component is a halfword.
  const ParticlePoint first_local =
      ParticlePointAt(LocalPoint(first_angle, first_radius, first_depth));
  const ParticlePoint second_local =
      ParticlePointAt(LocalPoint(second_angle, second_radius, second_depth));
  scratch.aim = math::BuildLookAtOrientation(first_point, last_point);
  const math::Transform3 frame_transform{.rotation = scratch.aim,
                                         .translation = first_point};
  scratch.aim_point =
      math::RotateTranslate(frame_transform, ParticleVector(first_local));
  points[1] = ParticlePointAt(scratch.aim_point);
  scratch.spawn_point =
      math::RotateTranslate(frame_transform, ParticleVector(second_local));
  points[2] = ParticlePointAt(scratch.spawn_point);
}
}  // namespace

// decomp: EFFECT VS_PARTICLE_CONTROL_FUNCTION
// (src/EFFECT/particleSetup.h:298).
void ParticleControl(ParticleSetFrame& frame, ParticleSetHost& host) {
  const std::size_t ribbons = ParticleRibbons(frame);
  for (std::size_t ribbon = 0; ribbon < ribbons; ++ribbon) {
    PlaceInnerPoints(frame.points.sets.at(ribbon), frame, host);
  }
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG045.BIN func_800FA2B0 (src/EFFECT/PLG045.BIN/110.c:3).
void Plg045::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG057.BIN func_800FA470 (src/EFFECT/PLG057.BIN/2D0.c:5).
void Plg057::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG065.BIN func_800FCCE0 (src/EFFECT/PLG065.BIN/F8.c:22).
// decomp: PLG065.BIN func_800FCCE0 (src/EFFECT/PLG065.BIN/F8.c:38).
void Plg065::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG082.BIN func_800FA2C8 (src/EFFECT/PLG082.BIN/128.c:11).
void Plg082::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG087.BIN func_800FEB08 (src/EFFECT/PLG087.BIN/1F0.c:34).
// decomp: PLG087.BIN func_800FEB08 (src/EFFECT/PLG087.BIN/1F0.c:50).
void Plg087::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG114.BIN func_800FA3A8 (src/EFFECT/PLG114.BIN/208.c:5).
void Plg114::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG114.BIN func_800FC04C (src/EFFECT/PLG114.BIN/208.c:17).
// decomp: PLG114.BIN func_800FC04C (src/EFFECT/PLG114.BIN/208.c:33).
void Plg114::ParticleControl2(Templates::ParticleSetFrame& frame,
                              Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG146.BIN func_800FBEE4 (src/EFFECT/PLG146.BIN/1D8.c:13).
// decomp: PLG146.BIN func_800FBEE4 (src/EFFECT/PLG146.BIN/1D8.c:29).
void Plg146::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG147.BIN func_800FAEBC (src/EFFECT/PLG147.BIN/F8.c:9).
// decomp: PLG147.BIN func_800FAEBC (src/EFFECT/PLG147.BIN/F8.c:25).
void Plg147::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG152.BIN func_800FAE18 (src/EFFECT/PLG152.BIN/128.c:14).
void Plg152::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG153.BIN func_800FB9E0 (src/EFFECT/PLG153.BIN/110.c:15).
void Plg153::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG156.BIN func_800FA2C8 (src/EFFECT/PLG156.BIN/128.c:5).
void Plg156::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG168.BIN func_800FC4B0 (src/EFFECT/PLG168.BIN/1F0.c:15).
// decomp: PLG168.BIN func_800FC4B0 (src/EFFECT/PLG168.BIN/1F0.c:31).
void Plg168::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG224.BIN func_800FA298 (src/EFFECT/PLG224.BIN/F8.c:5).
// decomp: PLG224.BIN func_800FA298 (src/EFFECT/PLG224.BIN/F8.c:21).
void Plg224::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG255.BIN func_800FC498 (src/EFFECT/PLG255.BIN/1D8.c:15).
// decomp: PLG255.BIN func_800FC498 (src/EFFECT/PLG255.BIN/1D8.c:31).
void Plg255::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG263.BIN func_800FC9AC (src/EFFECT/PLG263.BIN/110.c:347).
void Plg263::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG265.BIN func_800FB380 (src/EFFECT/PLG265.BIN/2E8.c:9).
// decomp: PLG265.BIN func_800FB380 (src/EFFECT/PLG265.BIN/2E8.c:25).
void Plg265::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG265.BIN func_800FEA94 (src/EFFECT/PLG265.BIN/2E8.c:43).
void Plg265::ParticleControl2(Templates::ParticleSetFrame& frame,
                              Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG266.BIN func_800FA488 (src/EFFECT/PLG266.BIN/2E8.c:5).
void Plg266::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG266.BIN func_800FC12C (src/EFFECT/PLG266.BIN/2E8.c:17).
// decomp: PLG266.BIN func_800FC12C (src/EFFECT/PLG266.BIN/2E8.c:33).
void Plg266::ParticleControl2(Templates::ParticleSetFrame& frame,
                              Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG300.BIN func_800FAF24 (src/EFFECT/PLG300.BIN/110.c:15).
void Plg300::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG301.BIN func_800FA2B0 (src/EFFECT/PLG301.BIN/110.c:11).
void Plg301::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}
}  // namespace VS::Effects
