// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/TrailCore.hpp"

#include <cstddef>
#include <cstdint>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"

// The sampling and motion rules the five- and nine-sample trail stages share
// (src/EFFECT/trails5.h:243-449 and trails9.h:243-456). Each stage samples its
// ranges with vs_battle_lerpVector/lerp2DVector at the emitter's curve
// distance, and the init and update stages advance a trail by the same speed
// recurrence.
namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// A trail's progress is its Q24 position along the spline: 0x01000000 per span
// between neighbouring points, three spans from the first point to the last
// (update samples at phase (progress >> 12) - 4096).
constexpr std::int32_t kTrailTravel = 0x03000000;
}  // namespace

math::Vec3 SampleTrailVector(const TrailVectorRange& range, std::uint8_t curve,
                             std::int32_t distance, TrailServices& host) {
  return math::InterpolateSevenBitVector(range,
                                         host.SampleCurve(curve, distance));
}

math::Vec2 SampleTrailPlane(const TrailPlaneRange& range, std::uint8_t curve,
                            std::int32_t distance, TrailServices& host) {
  return math::InterpolateSevenBitPlane(range,
                                        host.SampleCurve(curve, distance));
}

// decomp: trails5.h:380-404 (init's first step) and trails5.h:407-446 (each
// update's speed change); trails9.h:387-412 and 414-454 are the same.
template <std::size_t History>
std::int32_t TrailSpeedStep(const BasicTrailRecord<History>& trail,
                            std::int32_t remaining) noexcept {
  const std::int32_t pull = math::R3000Quotient(
      math::WrappingMultiply(
          math::WrappingSubtract(kTrailTravel, trail.progress), 2),
      math::WrappingMultiply(remaining, remaining));
  const std::int32_t drag =
      math::R3000Quotient(math::WrappingMultiply(trail.speed, 2), remaining);
  return math::WrappingSubtract(pull, drag);
}

template std::int32_t TrailSpeedStep(const TrailRecord& trail,
                                     std::int32_t remaining) noexcept;
template std::int32_t TrailSpeedStep(const TrailNineRecord& trail,
                                     std::int32_t remaining) noexcept;
}  // namespace VS::Effects::Templates
