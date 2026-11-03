#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"

// Record bindings of Interpolation.hpp for generated callers that still pass
// the original argument order (tools/port/copy_fixups.py
// OWNED_CALL_BINDINGS). Range tables are read through fixed-extent spans of
// (start, end) halfword pairs, so a range of the wrong size does not compile
// and nothing is read past it. Each binding reads every input before writing,
// so an argument may alias the destination.
namespace VS::Core::Math {
// Halfwords in a (start, end) pair range for one value, two values (a plane)
// and three values (a vector).
inline constexpr std::size_t kScalarRangeHalfwords = 2;
inline constexpr std::size_t kPlaneRangeHalfwords = 4;
inline constexpr std::size_t kVectorRangeHalfwords = 6;

// The (start, end) pair of one value in a pair range.
[[nodiscard]] constexpr ScalarSpan RangePair(
    std::span<const std::int16_t> ranges, std::size_t index) noexcept {
  return ScalarSpan{ranges[index * 2], ranges[(index * 2) + 1]};
}

// func_800CFB68(start, end, weight). The bindings below keep the original
// argument orders, so adjacent same-typed parameters are expected.
[[nodiscard]] inline std::int32_t InterpolateLiveSevenBitScalar(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    std::int32_t start, std::int32_t end, std::int32_t weight) noexcept {
  return InterpolateSevenBitScalar({start, end}, weight);
}

// func_800CFE1C(endpoints, weight).
[[nodiscard]] inline std::int32_t InterpolateLiveShortPair(
    std::span<const std::int16_t, kScalarRangeHalfwords> endpoints,
    std::int32_t weight) noexcept {
  return InterpolateSevenBitScalar(RangePair(endpoints, 0), weight);
}

// vs_battle_lerpVector and vs_battle_lerpSvector(ranges, weight,
// destination): one pair each for x, y and z.
template <VectorRecord Destination>
void InterpolateLiveSevenBitVector(
    std::span<const std::int16_t, kVectorRangeHalfwords> ranges,
    std::int32_t weight, Destination* destination) noexcept {
  StoreVector(*destination,
              InterpolateSevenBitVector(
                  {RangePair(ranges, 0), RangePair(ranges, 1),
                   RangePair(ranges, 2)},
                  weight));
}

// vs_battle_lerp2DVector(ranges, weight, destination): two pairs into a
// two-word destination.
inline void InterpolateLiveSevenBitPlane(
    std::span<const std::int16_t, kPlaneRangeHalfwords> ranges,
    std::int32_t weight, std::span<std::int32_t, 2> destination) noexcept {
  const Vec2 value = InterpolateSevenBitPlane(
      {RangePair(ranges, 0), RangePair(ranges, 1)}, weight);
  destination[0] = value.x;
  destination[1] = value.y;
}

// vs_battle_lerp(start, end, weight).
[[nodiscard]] inline std::int32_t InterpolateLiveThirteenBitScalar(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    std::int32_t start, std::int32_t end, std::int32_t weight) noexcept {
  return InterpolateThirteenBitScalar({start, end}, weight);
}

// func_800D169C and func_800D17A8(start, end, weight, destination).
template <VectorRecord Start, VectorRecord End, VectorRecord Destination>
void InterpolateLiveThirteenBitVector(const Start* start, const End* end,
                                      std::int32_t weight,
                                      Destination* destination) noexcept {
  StoreVector(*destination, InterpolateThirteenBitVector(
                                VectorOf(*start), VectorOf(*end), weight));
}

// func_800D1904(start, end, total, current).
[[nodiscard]] inline std::int32_t InterpolateLiveTimedScalar(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    std::int32_t start, std::int32_t end, std::int32_t total,
    std::int32_t current) noexcept {
  return InterpolateTimedScalar({start, end}, {current, total});
}

// func_800D1778 and func_800D1884(start, end, total, current, destination).
template <VectorRecord Start, VectorRecord End, VectorRecord Destination>
void InterpolateLiveTimedVector(
    const Start* start, const End* end,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    std::int32_t total, std::int32_t current,
    Destination* destination) noexcept {
  StoreVector(*destination,
              InterpolateTimedVector(VectorOf(*start), VectorOf(*end),
                                     {current, total}));
}

// func_800CFC8C and func_800D18B4(start, end, total, current).
[[nodiscard]] inline std::int32_t InterpolateLiveCosineScalar(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    std::int32_t start, std::int32_t end, std::int32_t total,
    std::int32_t current) noexcept {
  return InterpolateCosineScalar({start, end}, {current, total});
}

// func_800D1718 and func_800D1824(start, end, total, current, destination).
template <VectorRecord Start, VectorRecord End, VectorRecord Destination>
void InterpolateLiveCosineVector(
    const Start* start, const End* end,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    std::int32_t total, std::int32_t current,
    Destination* destination) noexcept {
  StoreVector(*destination,
              InterpolateCosineVector(VectorOf(*start), VectorOf(*end),
                                      {current, total}));
}

// vs_battle_lerpRatio(start, end, numerator, denominator).
[[nodiscard]] inline std::int32_t InterpolateLiveFixedPointRatio(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    std::int32_t start, std::int32_t end, std::int32_t numerator,
    std::int32_t denominator) noexcept {
  return InterpolateFixedPointRatio({start, end}, {numerator, denominator});
}

// vs_battle_splineInterpolate(p0, p1, p2, p3, phase, destination).
template <VectorRecord P0, VectorRecord P1, VectorRecord P2, VectorRecord P3,
          VectorRecord Destination>
void InterpolateLiveLagrangeVector(const P0* p0, const P1* p1, const P2* p2,
                                   const P3* p3, std::int32_t phase,
                                   Destination* destination) noexcept {
  StoreVector(*destination,
              InterpolateLagrangeVector({VectorOf(*p0), VectorOf(*p1),
                                         VectorOf(*p2), VectorOf(*p3)},
                                        phase));
}

// func_800D155C(value0, value1, value2, value3, phase, destination).
inline void InterpolateLiveLagrangeScalar(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    std::int32_t value0, std::int32_t value1, std::int32_t value2,
    std::int32_t value3, std::int32_t phase,
    std::int32_t* destination) noexcept {
  *destination =
      InterpolateLagrangeScalar({value0, value1, value2, value3}, phase);
}

// _ease(mode, current, total): the raw mode selects the curve as the
// original's dispatch did (EaseCurveOf).
[[nodiscard]] inline std::int32_t EvaluateLiveEase(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    std::int16_t mode, std::int16_t current, std::int16_t total) noexcept {
  return EvaluateEase(EaseCurveOf(mode), {current, total});
}
}  // namespace VS::Core::Math
