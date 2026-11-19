#pragma once
// The PLG trail records as the trail templates copy them through
// (Templates/TrailsLive.cpp): BasicTrailRecord (Templates/TrailCore.hpp) is
// their typed value.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Legacy/Psx.h"

namespace VS::Effects {
// decomp: TRAIL_SAMPLE_COUNT (src/EFFECT/trails5.h:3 and trails9.h:3)
inline constexpr std::size_t kLiveTrailSamples = 5;
inline constexpr std::size_t kLiveTrailNineSamples = 9;
inline constexpr std::size_t kLiveTrailSplinePoints = 4;

// decomp: VS_TRAIL_TYPES (src/EFFECT/trailEffect.h:28): one trail with
// `Samples` spline samples, in its PS1 layout.
template <std::size_t Samples>
struct LiveTrailRecord {
  // The origin, two control points and the target.
  std::array<SVECTOR, kLiveTrailSplinePoints> spline_points;
  // Emitter steps the trail spends crossing the spline.
  std::uint8_t step_count;
  // Samples taken so far, up to Samples.
  std::uint8_t spline_sample_count;
  // Ticks left once the trail reaches its target (0 ends it); also selects
  // the fade's transparency sample and texture column.
  std::uint8_t trail_collapse_counter;
  // Newest entry of the sample ring.
  std::uint8_t current_spline_sample;
  // Ring of the spline samples the trail draws.
  std::array<SVECTOR, Samples> spline_samples;
  // Position along the spline; (spline_progress >> 12) - 0x1000 is the
  // interpolation parameter.
  std::int32_t spline_progress;
  // Progress added per tick, eased toward the target.
  std::int32_t speed;
  // Initial speed step (6*ONE*ONE/steps^2 - 2*speed/steps); set at start,
  // never read.
  std::int32_t first_step;
};

// decomp: vs_Trail5 (src/EFFECT/trailEffect.h:52)
using LiveTrail = LiveTrailRecord<kLiveTrailSamples>;
// decomp: vs_Trail9 (src/EFFECT/trailEffect.h:53)
using LiveTrail9 = LiveTrailRecord<kLiveTrailNineSamples>;

namespace layout {
inline constexpr std::size_t kTrailBytes = 0x58;
static_assert(sizeof(LiveTrail) == kTrailBytes);
inline constexpr std::size_t kTrailNineBytes = 0x78;
static_assert(sizeof(LiveTrail9) == kTrailNineBytes);
}  // namespace layout
}  // namespace VS::Effects
