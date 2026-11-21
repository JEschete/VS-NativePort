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

// decomp: src/EFFECT/particleSetup.h:322-333.
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
// jitter). decomp: src/EFFECT/particleSetup.h:335-353.
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
// decomp: src/EFFECT/particleSetup.h:260-356.
[[nodiscard]] std::int32_t JitteredRadius(Core::Math::Vec2 range,
                                          ParticleSetHost& host) {
  if (range.y == 0) {
    return range.x;
  }
  return math::WrappingAdd(range.x, ParticleRemainder(host.Rand(), range.y));
}

// The angle `base` moved by a draw across [-jitter, jitter) (none for zero).
// decomp: src/EFFECT/particleSetup.h:260-356.
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

// decomp: src/EFFECT/particleSetup.h:260-356, one ribbon's two inner points.
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
// decomp: PLG045.BIN vs_effect_lightningBolt_0_control
// (src/EFFECT/particleSetup.h:260; was func_800FA2B0).
void Plg045::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG057.BIN vs_effect_thunderBurstLv1_0_control
// (src/EFFECT/particleSetup.h:260; was func_800FA470).
void Plg057::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG065.BIN vs_effect_gaeaStrikeLv1_3_control
// (src/EFFECT/particleSetup.h:260; was func_800FCCE0). decomp: PLG065.BIN
// vs_effect_gaeaStrikeLv1_3_control (src/EFFECT/particleSetup.h:260; was
// func_800FCCE0).
void Plg065::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG082.BIN vs_effect_thunderbolt_0_control
// (src/EFFECT/particleSetup.h:260; was func_800FA2C8).
void Plg082::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_4_control
// (src/EFFECT/particleSetup.h:260; was func_800FEB08). decomp: PLG087.BIN
// vs_effect_apocalypse_4_control (src/EFFECT/particleSetup.h:260; was
// func_800FEB08).
void Plg087::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG114.BIN vs_effect_numbingNeedle_0_control
// (src/EFFECT/particleSetup.h:260; was func_800FA3A8).
void Plg114::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG114.BIN vs_effect_numbingNeedle_1_control
// (src/EFFECT/particleSetup.h:260; was func_800FC04C). decomp: PLG114.BIN
// vs_effect_numbingNeedle_1_control (src/EFFECT/particleSetup.h:260; was
// func_800FC04C).
void Plg114::ParticleControl2(Templates::ParticleSetFrame& frame,
                              Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_2_control
// (src/EFFECT/particleSetup.h:260; was func_800FBEE4). decomp: PLG146.BIN
// vs_effect_blackNebula_2_control (src/EFFECT/particleSetup.h:260; was
// func_800FBEE4).
void Plg146::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG147.BIN vs_effect_bearClaw_1_control
// (src/EFFECT/particleSetup.h:260; was func_800FAEBC). decomp: PLG147.BIN
// vs_effect_bearClaw_1_control (src/EFFECT/particleSetup.h:260; was
// func_800FAEBC).
void Plg147::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG152.BIN vs_effect_riskbreak_1_control
// (src/EFFECT/particleSetup.h:260; was func_800FAE18).
void Plg152::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG153.BIN vs_effect_gravisAether_2_control
// (src/EFFECT/particleSetup.h:260; was func_800FB9E0).
void Plg153::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG156.BIN vs_effect_quickshock_0_control
// (src/EFFECT/particleSetup.h:260; was func_800FA2C8).
void Plg156::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG168.BIN vs_effect_vertigo_1_control
// (src/EFFECT/particleSetup.h:260; was func_800FC4B0). decomp: PLG168.BIN
// vs_effect_vertigo_1_control (src/EFFECT/particleSetup.h:260; was
// func_800FC4B0).
void Plg168::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG224.BIN vs_effect_plg224_0_control
// (src/EFFECT/particleSetup.h:260; was func_800FA298). decomp: PLG224.BIN
// vs_effect_plg224_0_control (src/EFFECT/particleSetup.h:260; was
// func_800FA298).
void Plg224::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG255.BIN vs_effect_plg255_1_control
// (src/EFFECT/particleSetup.h:260; was func_800FC498). decomp: PLG255.BIN
// vs_effect_plg255_1_control (src/EFFECT/particleSetup.h:260; was
// func_800FC498).
void Plg255::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG263.BIN vs_effect_plg263_3_control
// (src/EFFECT/particleSetup.h:260; was func_800FC9AC).
void Plg263::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG265.BIN vs_effect_plg265_1_control
// (src/EFFECT/particleSetup.h:260; was func_800FB380). decomp: PLG265.BIN
// vs_effect_plg265_1_control (src/EFFECT/particleSetup.h:260; was
// func_800FB380).
void Plg265::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG265.BIN vs_effect_plg265_3_control
// (src/EFFECT/particleSetup.h:260; was func_800FEA94).
void Plg265::ParticleControl2(Templates::ParticleSetFrame& frame,
                              Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG266.BIN vs_effect_plg266_0_control
// (src/EFFECT/particleSetup.h:260; was func_800FA488).
void Plg266::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG266.BIN vs_effect_plg266_1_control
// (src/EFFECT/particleSetup.h:260; was func_800FC12C). decomp: PLG266.BIN
// vs_effect_plg266_1_control (src/EFFECT/particleSetup.h:260; was
// func_800FC12C).
void Plg266::ParticleControl2(Templates::ParticleSetFrame& frame,
                              Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG300.BIN vs_effect_plg300_1_control
// (src/EFFECT/particleSetup.h:260; was func_800FAF24).
void Plg300::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}

// decomp: PLG301.BIN vs_effect_plg301_0_control
// (src/EFFECT/particleSetup.h:260; was func_800FA2B0).
void Plg301::ParticleControl(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleControl(frame, host);
}
}  // namespace VS::Effects
