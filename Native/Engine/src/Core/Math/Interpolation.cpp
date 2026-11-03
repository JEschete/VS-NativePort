// Hand-maintained (tools/port/owned.csv).
#include "Core/Math/Interpolation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Core::Math {
namespace {
constexpr int kSevenBitShift = 7;
constexpr int kFractionBits = 12;
constexpr int kHalvedFractionBits = 13;
constexpr int kTwoThirdsShift = 14;
constexpr std::int32_t kFixedOne = 4096;
constexpr std::int32_t kFixedTwo = 8192;
constexpr std::int32_t kQuarterTurn = 1024;
constexpr std::int32_t kHalfTurn = 2048;
// ONE * 2 / 3 in the original's integer arithmetic.
constexpr std::int32_t kTwoThirds = 2730;

using LagrangeWeights = std::array<std::int32_t, kLagrangePoints>;

// The R3000 quotient of current * scale by total.
std::int32_t ScaledProgress(StepProgress progress,
                            std::int32_t scale) noexcept {
  return R3000Quotient(WrappingMultiply(progress.current, scale),
                       progress.total);
}

// The thirteen-bit weight of a linear step: current * 8192 / total.
std::int32_t TimedWeight(StepProgress progress) noexcept {
  return ScaledProgress(progress, kFixedTwo);
}

// The thirteen-bit weight of a half-cosine step: 4096 - cos(current * 2048 /
// total), from 0 at the start to 8192 at the end.
std::int32_t CosineWeight(StepProgress progress) noexcept {
  return WrappingSubtract(kFixedOne,
                          Cosine(ScaledProgress(progress, kHalfTurn)));
}

// Q12 Lagrange basis weights of the points at phase -1, 0, 1 and 2, in the
// original's operation order and truncation.
LagrangeWeights WeightsAt(std::int32_t phase) noexcept {
  const std::int32_t negative = WrappingNegate(phase);
  const std::int32_t minus_one = WrappingSubtract(phase, kFixedOne);
  const std::int32_t minus_two = WrappingSubtract(phase, kFixedTwo);
  const std::int32_t plus_one = WrappingAdd(phase, kFixedOne);
  const auto cubic = [](std::int32_t first, std::int32_t second,
                        std::int32_t third) noexcept {
    return WrappingMultiply(WrappingMultiply(first, second) >> kFractionBits,
                            third);
  };
  const auto outer = [](std::int32_t cubic_value) noexcept {
    return WrappingMultiply(cubic_value >> kFractionBits, kTwoThirds) >>
           kTwoThirdsShift;
  };
  return LagrangeWeights{
      outer(cubic(negative, minus_one, minus_two)),
      cubic(plus_one, minus_one, minus_two) >> kHalvedFractionBits,
      cubic(plus_one, negative, minus_two) >> kHalvedFractionBits,
      outer(cubic(plus_one, phase, minus_one))};
}

std::int32_t Combine(const std::array<std::int32_t, kLagrangePoints>& values,
                     const LagrangeWeights& weights) noexcept {
  std::int32_t sum = 0;
  for (std::size_t point = 0; point < values.size(); ++point) {
    sum = WrappingAdd(sum, WrappingMultiply(values[point], weights[point]));
  }
  return sum >> kFractionBits;
}

// One axis of the control points, in point order.
std::int32_t CombineAxis(const std::array<Vec3, kLagrangePoints>& points,
                         std::int32_t Vec3::* axis,
                         const LagrangeWeights& weights) noexcept {
  std::array<std::int32_t, kLagrangePoints> values{};
  for (std::size_t point = 0; point < values.size(); ++point) {
    values[point] = points[point].*axis;
  }
  return Combine(values, weights);
}
}  // namespace

// decomp: BATTLE.PRG func_800CFB68 (src/BATTLE/BATTLE.PRG/5BF94.c:5805) and
// func_800CFE1C (5BF94.c:5905).
std::int32_t InterpolateSevenBitScalar(ScalarSpan span,
                                       std::int32_t weight) noexcept {
  return WrappingAdd(
      WrappingMultiply(WrappingSubtract(span.end, span.start), weight) >>
          kSevenBitShift,
      span.start);
}

// decomp: BATTLE.PRG vs_battle_lerpVector (src/BATTLE/BATTLE.PRG/5BF94.c:5866)
// and vs_battle_lerpSvector (5BF94.c:5880).
Vec3 InterpolateSevenBitVector(
    const std::array<ScalarSpan, kMatrixOrder>& spans,
    std::int32_t weight) noexcept {
  return Vec3{InterpolateSevenBitScalar(spans[0], weight),
              InterpolateSevenBitScalar(spans[1], weight),
              InterpolateSevenBitScalar(spans[2], weight)};
}

// decomp: BATTLE.PRG vs_battle_lerp2DVector
// (src/BATTLE/BATTLE.PRG/5BF94.c:5894).
Vec2 InterpolateSevenBitPlane(const std::array<ScalarSpan, 2>& spans,
                              std::int32_t weight) noexcept {
  return Vec2{InterpolateSevenBitScalar(spans[0], weight),
              InterpolateSevenBitScalar(spans[1], weight)};
}

// decomp: BATTLE.PRG vs_battle_lerp (src/BATTLE/BATTLE.PRG/5BF94.c:6424).
std::int32_t InterpolateThirteenBitScalar(ScalarSpan span,
                                          std::int32_t weight) noexcept {
  return WrappingAdd(
      span.start,
      WrappingMultiply(WrappingSubtract(span.end, span.start), weight) /
          kFixedTwo);
}

// decomp: BATTLE.PRG func_800D169C (src/BATTLE/BATTLE.PRG/5BF94.c:6430) and
// func_800D17A8 (5BF94.c:6447).
Vec3 InterpolateThirteenBitVector(Vec3 start, Vec3 end,
                                  std::int32_t weight) noexcept {
  return Vec3{InterpolateThirteenBitScalar({start.x, end.x}, weight),
              InterpolateThirteenBitScalar({start.y, end.y}, weight),
              InterpolateThirteenBitScalar({start.z, end.z}, weight)};
}

// decomp: BATTLE.PRG func_800D1904 (src/BATTLE/BATTLE.PRG/5BF94.c:6469).
std::int32_t InterpolateTimedScalar(ScalarSpan span,
                                    StepProgress progress) noexcept {
  return InterpolateThirteenBitScalar(span, TimedWeight(progress));
}

// decomp: BATTLE.PRG func_800D1778 (src/BATTLE/BATTLE.PRG/5BF94.c:6442) and
// func_800D1884 (5BF94.c:6459).
Vec3 InterpolateTimedVector(Vec3 start, Vec3 end,
                            StepProgress progress) noexcept {
  return InterpolateThirteenBitVector(start, end, TimedWeight(progress));
}

// decomp: BATTLE.PRG func_800CFC8C (src/BATTLE/BATTLE.PRG/5BF94.c:5859) and
// func_800D18B4 (5BF94.c:6464), which compute the same sum.
std::int32_t InterpolateCosineScalar(ScalarSpan span,
                                     StepProgress progress) noexcept {
  return InterpolateThirteenBitScalar(span, CosineWeight(progress));
}

// decomp: BATTLE.PRG func_800D1718 (src/BATTLE/BATTLE.PRG/5BF94.c:6437) and
// func_800D1824 (5BF94.c:6454).
Vec3 InterpolateCosineVector(Vec3 start, Vec3 end,
                             StepProgress progress) noexcept {
  return InterpolateThirteenBitVector(start, end, CosineWeight(progress));
}

// decomp: BATTLE.PRG vs_battle_lerpRatio (src/BATTLE/BATTLE.PRG/5BF94.c:6382).
std::int32_t InterpolateFixedPointRatio(ScalarSpan span,
                                        StepProgress ratio) noexcept {
  const std::int32_t scaled = WrappingMultiply(
      WrappingSubtract(span.end, span.start), ScaledProgress(ratio, kFixedOne));
  return WrappingAdd(span.start, scaled / kFixedOne);
}

// decomp: BATTLE.PRG vs_battle_splineInterpolate
// (src/BATTLE/BATTLE.PRG/5BF94.c:6388).
Vec3 InterpolateLagrangeVector(const std::array<Vec3, kLagrangePoints>& points,
                               std::int32_t phase) noexcept {
  const LagrangeWeights weights = WeightsAt(phase);
  return Vec3{CombineAxis(points, &Vec3::x, weights),
              CombineAxis(points, &Vec3::y, weights),
              CombineAxis(points, &Vec3::z, weights)};
}

// decomp: BATTLE.PRG func_800D155C (src/BATTLE/BATTLE.PRG/5BF94.c:6412).
std::int32_t InterpolateLagrangeScalar(
    const std::array<std::int32_t, kLagrangePoints>& values,
    std::int32_t phase) noexcept {
  return Combine(values, WeightsAt(phase));
}

// decomp: ENDING.PRG _ease (src/ENDING/ENDING.PRG/D4.c:1293).
std::int32_t EvaluateEase(EaseCurve curve, StepProgress progress) noexcept {
  switch (curve) {
    case EaseCurve::kLinear:
      return ScaledProgress(progress, kFixedOne);
    case EaseCurve::kEaseOut:
      return Sine(ScaledProgress(progress, kQuarterTurn));
    case EaseCurve::kEaseInOut:
      return WrappingAdd(Cosine(WrappingAdd(ScaledProgress(progress, kHalfTurn),
                                            kHalfTurn)),
                         kFixedOne) >>
             1;
    case EaseCurve::kEaseIn:
      break;
  }
  return WrappingAdd(Sine(WrappingSubtract(
                         ScaledProgress(progress, kQuarterTurn), kQuarterTurn)),
                     kFixedOne);
}
}  // namespace VS::Core::Math
