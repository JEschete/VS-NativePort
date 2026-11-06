// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/TrailUpdate.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/TrailCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// The Lagrange phase is the progress in 1/4096ths of a span, less one span:
// phase -4096 is the origin and 8192 the target.
constexpr unsigned kPhaseShift = 12;
constexpr std::int32_t kPhaseOrigin = 0x1000;

template <std::size_t History>
void AdvanceTrail(BasicTrailRecord<History>& trail, std::int32_t remaining) {
  trail.speed =
      math::WrappingAdd(trail.speed, TrailSpeedStep(trail, remaining));
  trail.progress = math::WrappingAdd(trail.progress, trail.speed);
  trail.current = static_cast<std::uint8_t>(
      (static_cast<std::size_t>(trail.current) + 1) % History);
  const std::array<math::Vec3, math::kLagrangePoints> spline = {
      TrailWide(trail.points[0]), TrailWide(trail.points[1]),
      TrailWide(trail.points[2]), TrailWide(trail.points.back())};
  const std::int32_t phase =
      math::WrappingSubtract(trail.progress >> kPhaseShift, kPhaseOrigin);
  trail.samples.at(trail.current) =
      TrailNarrow(math::InterpolateLagrangeVector(spline, phase));
  if (trail.sample_count < History) {
    ++trail.sample_count;
  }
}

template <std::size_t History>
void UpdateTrails(std::span<BasicTrailRecord<History>> trails,
                  std::int32_t distance) {
  for (BasicTrailRecord<History>& trail : trails) {
    if (trail.collapse == 0) {
      continue;
    }
    if (distance >= trail.step_count) {
      --trail.collapse;
      if (trail.collapse < trail.step_count) {
        --trail.sample_count;
      }
      continue;
    }
    AdvanceTrail(trail, math::WrappingSubtract(trail.step_count, distance));
  }
}
}  // namespace

// decomp: EFFECT VS_TRAIL_UPDATE_FUNCTION (src/EFFECT/trails5.h:407).
void TrailUpdate(const TrailDefinition& /*definition*/,
                 TrailScratch& /*scratch*/, std::span<TrailRecord> trails,
                 std::int32_t distance, TrailHost& /*host*/) {
  UpdateTrails<kTrailHistory>(trails, distance);
}

// decomp: EFFECT VS_TRAIL_UPDATE_FUNCTION (src/EFFECT/trails9.h:414).
// decomp: PLG049.BIN func_800FA8E4 (src/EFFECT/PLG049.BIN/E0.c:5).
// decomp: PLG050.BIN func_800FB508 (src/EFFECT/PLG050.BIN/E0.c:11).
// decomp: PLG057.BIN func_800FC778 (src/EFFECT/PLG057.BIN/2D0.c:19).
// decomp: PLG069.BIN func_800FB3F0 (src/EFFECT/PLG069.BIN/E0.c:12).
// decomp: PLG135.BIN func_800FB850 (src/EFFECT/PLG135.BIN/E0.c:17).
// decomp: PLG161.BIN func_800FC3D8 (src/EFFECT/PLG161.BIN/F8.c:15).
// decomp: PLG165.BIN func_800FA8FC (src/EFFECT/PLG165.BIN/F8.c:13).
// decomp: PLG167.BIN func_800FC708 (src/EFFECT/PLG167.BIN/E0.c:20).
// decomp: PLG168.BIN func_800FA9F4 (src/EFFECT/PLG168.BIN/1F0.c:7).
// decomp: PLG169.BIN func_800FA8E4 (src/EFFECT/PLG169.BIN/E0.c:13).
// decomp: PLG232.BIN func_800FA8E4 (src/EFFECT/PLG232.BIN/E0.c:7).
// The plugin instances are byte-identical; each overlay's renderer-slot entry
// (TrailsLive.cpp) binds this one definition.
void TrailNineUpdate(const TrailDefinition& /*definition*/,
                     TrailScratch& /*scratch*/,
                     std::span<TrailNineRecord> trails, std::int32_t distance,
                     TrailNineHost& /*host*/) {
  UpdateTrails<kTrailNineHistory>(trails, distance);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG057.BIN func_800FE884 (src/EFFECT/PLG057.BIN/2D0.c:29).
void Plg057::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG073.BIN func_800FC404 (src/EFFECT/PLG073.BIN/F8.c:15).
// decomp: PLG074.BIN func_800FC404 (src/EFFECT/PLG074.BIN/F8.c:15).
void Plg073::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG086.BIN func_800FD260 (src/EFFECT/PLG086.BIN/F8.c:24).
void Plg086::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG087.BIN func_800FD038 (src/EFFECT/PLG087.BIN/1F0.c:26).
void Plg087::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG128.BIN func_800FF3C0 (src/EFFECT/PLG128.BIN/F8.c:257).
void Plg128::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG134.BIN func_800FA8D0 (src/EFFECT/PLG134.BIN/E0.c:13).
void Plg134::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG137.BIN func_800FB3F4 (src/EFFECT/PLG137.BIN/F8.c:12).
void Plg137::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG141.BIN func_800FA8D0 (src/EFFECT/PLG141.BIN/E0.c:7).
void Plg141::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG146.BIN func_800FE74C (src/EFFECT/PLG146.BIN/1D8.c:44).
void Plg146::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG148.BIN func_800FA8D0 (src/EFFECT/PLG148.BIN/E0.c:7).
void Plg148::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG242.BIN func_800FC330 (src/EFFECT/PLG242.BIN/F8.c:14).
void Plg242::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG255.BIN func_800FA9C8 (src/EFFECT/PLG255.BIN/1D8.c:7).
void Plg255::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG265.BIN func_800FCFC4 (src/EFFECT/PLG265.BIN/2E8.c:35).
void Plg265::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG266.BIN func_800FDD70 (src/EFFECT/PLG266.BIN/2E8.c:43).
void Plg266::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG291.BIN func_800FA8E8 (src/EFFECT/PLG291.BIN/F8.c:13).
void Plg291::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects
