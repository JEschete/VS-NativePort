#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Effects/Curves.hpp"

// Live binding for the effect curves (Curves.hpp) over the current package
// (Effects_package): its curve block (block 4) within the extents recorded
// when it was bound (PackageExtents.hpp), its segmented curve table (block
// 11) as EffectResourcesLive.hpp bound it, and the sampled curve table
// (Effects_curveSamples). A curve the package lacks, a segment leaving the
// table and a sample past the table are refused with a diagnostic and read
// as 0, where the original indexed past them. The C-shaped entries keep the
// original signatures for the copied callers.
namespace VS::Effects {
// Effects_curveSamples (D_800F5330): slot i + 1 holds curve i's sample of
// the current frame.
[[nodiscard]] std::span<std::int32_t> LiveEffectCurveSamples();

// SampleEffectCurve and SampleSegmentedEffectCurve over the current package.
[[nodiscard]] CurveSample SampleLiveEffectCurve(std::int32_t curve_id,
                                                std::int32_t distance);
[[nodiscard]] CurveSample SampleLiveSegmentedEffectCurve(
    std::int32_t curve_index, std::int32_t distance);

// vs_battle_sampleCurve (5BF94.c:6324).
int LiveSampleEffectCurve(int curve_id, int distance);
// func_800D1104 (5BF94.c:6314).
void LiveSampleAllEffectCurves(int distance);
// func_800D11F4 (5BF94.c:6335).
int LiveSampleSegmentedEffectCurve(int curve_index, int distance);
// func_800D12D8 (5BF94.c:6365).
int LiveSegmentedEffectCurveDuration(int curve_index);
}  // namespace VS::Effects
