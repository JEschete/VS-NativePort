#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>

#include "Core/Math/ScalarMath.hpp"

// Interpolation curves shared by the event camera, battle effects, particles
// and the staff roll. Arithmetic wraps like the original 32-bit registers and
// divisions follow the R3000 DIV result (R3000Quotient), so every input is
// defined. Every function takes and returns plain values: nothing reads module
// tables or raw pointers. InterpolationLive.hpp binds them to records.
namespace VS::Core::Math {
// The endpoints of one interpolated value.
struct ScalarSpan {
  std::int32_t start{};
  std::int32_t end{};

  friend bool operator==(const ScalarSpan&, const ScalarSpan&) = default;
};

// Progress through a timed interpolation: `current` of `total` steps.
struct StepProgress {
  std::int32_t current{};
  std::int32_t total{};
};

// A pair of full-word values (a 2D vector).
struct Vec2 {
  std::int32_t x{};
  std::int32_t y{};

  friend bool operator==(const Vec2&, const Vec2&) = default;
};

// Number of control points in the Lagrange curve.
inline constexpr int kLagrangePoints = 4;

// Easing curves of the ending sequence (_ease).
enum class EaseCurve : std::int16_t {
  kLinear = 0,
  kEaseOut = 1,
  kEaseInOut = 2,
  kEaseIn = 3,
};

// The curve the original's _ease dispatch takes for a raw mode: negative
// modes fall into the ease-out branch and modes above 3 into ease-in.
[[nodiscard]] constexpr EaseCurve EaseCurveOf(std::int16_t mode) noexcept {
  if (mode == static_cast<std::int16_t>(EaseCurve::kLinear)) {
    return EaseCurve::kLinear;
  }
  if (mode == static_cast<std::int16_t>(EaseCurve::kEaseInOut)) {
    return EaseCurve::kEaseInOut;
  }
  return mode < static_cast<std::int16_t>(EaseCurve::kEaseInOut)
             ? EaseCurve::kEaseOut
             : EaseCurve::kEaseIn;
}

// start + ((end - start) * weight >> 7): a seven-bit weight, 128 = end.
[[nodiscard]] std::int32_t InterpolateSevenBitScalar(
    ScalarSpan span, std::int32_t weight) noexcept;
[[nodiscard]] Vec3 InterpolateSevenBitVector(
    const std::array<ScalarSpan, kMatrixOrder>& spans,
    std::int32_t weight) noexcept;
[[nodiscard]] Vec2 InterpolateSevenBitPlane(
    const std::array<ScalarSpan, 2>& spans, std::int32_t weight) noexcept;

// start + (end - start) * weight / 8192: a thirteen-bit weight, 8192 = end.
[[nodiscard]] std::int32_t InterpolateThirteenBitScalar(
    ScalarSpan span, std::int32_t weight) noexcept;
[[nodiscard]] Vec3 InterpolateThirteenBitVector(Vec3 start, Vec3 end,
                                                std::int32_t weight) noexcept;

// Thirteen-bit interpolation weighted linearly by the step progress.
[[nodiscard]] std::int32_t InterpolateTimedScalar(
    ScalarSpan span, StepProgress progress) noexcept;
[[nodiscard]] Vec3 InterpolateTimedVector(Vec3 start, Vec3 end,
                                          StepProgress progress) noexcept;

// Thirteen-bit interpolation eased by a half cosine of the step progress.
[[nodiscard]] std::int32_t InterpolateCosineScalar(
    ScalarSpan span, StepProgress progress) noexcept;
[[nodiscard]] Vec3 InterpolateCosineVector(Vec3 start, Vec3 end,
                                           StepProgress progress) noexcept;

// start + (end - start) * (numerator * 4096 / denominator) / 4096.
[[nodiscard]] std::int32_t InterpolateFixedPointRatio(
    ScalarSpan span, StepProgress ratio) noexcept;

// Cubic Lagrange curve through four points at Q12 phase -1, 0, 1 and 2; a
// phase of 0..4096 runs between the second and third points.
[[nodiscard]] Vec3 InterpolateLagrangeVector(
    const std::array<Vec3, kLagrangePoints>& points,
    std::int32_t phase) noexcept;
[[nodiscard]] std::int32_t InterpolateLagrangeScalar(
    const std::array<std::int32_t, kLagrangePoints>& values,
    std::int32_t phase) noexcept;

// A Q12 easing fraction of the step progress, 0 at the start and 4096 when
// current reaches total.
[[nodiscard]] std::int32_t EvaluateEase(EaseCurve curve,
                                        StepProgress progress) noexcept;
}  // namespace VS::Core::Math
