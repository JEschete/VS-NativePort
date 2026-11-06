#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

// The effect package's sampled curves (block 4) and segmented curves
// (block 11), over bound byte views. The original indexed both blocks through
// bare pointers; here a curve or segment the block lacks is refused with a
// status instead of reading past it. CurvesLive.hpp binds them to the current
// package.
namespace VS::Effects {
// What a curve query returned.
enum class CurveSampleStatus : std::uint8_t {
  kSampled,
  // The original's own zero: curve id 0, an id past the header count or an
  // index past the segmented table.
  kNoCurve,
  // A negative curve id or index read before the block (refused).
  kNegativeCurve,
  // The header names more curves than the block binds (refused).
  kUnboundCurve,
  // A zero-length curve divided by zero (refused).
  kEmptyCurve,
  // A negative distance indexed before the curve (refused).
  kNegativeDistance,
  // A segment offset or step left the block (refused).
  kLeavesBlock,
  // No block is bound (refused).
  kNoBlock,
};

// A sampled value with its status; refused queries return 0.
struct CurveSample {
  std::int32_t value{};
  CurveSampleStatus status{CurveSampleStatus::kSampled};
};

// Whether a query was refused rather than sampled or answered with the
// original's own zero.
[[nodiscard]] constexpr bool IsCurveRefusal(CurveSampleStatus status) noexcept {
  return status != CurveSampleStatus::kSampled &&
         status != CurveSampleStatus::kNoCurve;
}

// The curve block's header count and the curves it binds, in order; an empty
// curve holds no samples.
struct EffectCurveBlock {
  std::uint32_t header_count{};
  std::span<const std::span<const std::uint8_t>> curves;
};

// Curves the sweep wrote and refused.
struct CurveSweep {
  std::int32_t written{};
  std::int32_t refused{};
};

// vs_battle_sampleCurve (5BF94.c:6324): curve `curve_id` (1-based) at
// `distance` modulo its length; 0 for id 0 or an id past the header count.
[[nodiscard]] CurveSample SampleEffectCurve(const EffectCurveBlock& block,
                                            std::int32_t curve_id,
                                            std::int32_t distance) noexcept;

// func_800D1104 (5BF94.c:6314): samples[i + 1] = curve i at `distance` for
// every curve the header counts. A refused curve keeps its previous sample;
// samples past the span are refused.
CurveSweep SampleAllEffectCurves(const EffectCurveBlock& block,
                                 std::int32_t distance,
                                 std::span<std::int32_t> samples) noexcept;

// func_800D11F4 (5BF94.c:6335): segmented curve `curve_index` of the block-11
// table {s32 count, s16 offsets[count]} at `distance`. A curve is a header
// byte (its length) followed by {step, value} pairs ended by a zero step: the
// value is interpolated across the step `distance` falls into, the final
// value at the curve's exact end and -1 past it.
[[nodiscard]] CurveSample SampleSegmentedEffectCurve(
    std::span<const std::uint8_t> table, std::int32_t curve_index,
    std::int32_t distance) noexcept;

// func_800D12D8 (5BF94.c:6365): the frames segmented curve `curve_index`
// spans (its steps plus one), or 0 past the table.
[[nodiscard]] CurveSample SegmentedEffectCurveDuration(
    std::span<const std::uint8_t> table, std::int32_t curve_index) noexcept;
}  // namespace VS::Effects
