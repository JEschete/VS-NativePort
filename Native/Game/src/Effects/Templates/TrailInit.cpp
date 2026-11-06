// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/TrailInit.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Effects/Templates/TrailCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// The inner extent is the starting speed in 1/4096ths of a span (progress
// counts 0x01000000 per span).
constexpr unsigned kSpeedShift = 12;

template <std::size_t History>
void StartTrails(const TrailDefinition& definition, TrailScratch& scratch,
                 std::span<BasicTrailRecord<History>> trails,
                 TrailServices& host) {
  scratch.lifetime = math::InterpolateSevenBitPlane(definition.lifetime, 0);
  const auto speed = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(
          math::InterpolateSevenBitScalar(definition.inner_extent, 0))
      << kSpeedShift);
  for (BasicTrailRecord<History>& trail : trails) {
    std::int32_t steps =
        host.RandomRangeExclusive(scratch.lifetime.x, scratch.lifetime.y);
    if (steps == 0) {
      steps = 1;
    }
    trail.step_count = static_cast<std::uint8_t>(steps);
    trail.sample_count = 1;
    trail.collapse = kTrailFullCollapseOf<History>;
    trail.current = 0;
    trail.samples.front() = trail.points.front();
    trail.progress = 0;
    trail.speed = speed;
    // The original divided by the drawn count, not its stored low byte.
    trail.first_step = TrailSpeedStep(trail, steps);
  }
}
}  // namespace

// decomp: EFFECT VS_TRAIL_INIT_FUNCTION (src/EFFECT/trails5.h:380).
void TrailInit(const TrailDefinition& definition, TrailScratch& scratch,
               std::span<TrailRecord> trails, std::int32_t /*distance*/,
               TrailHost& host) {
  StartTrails<kTrailHistory>(definition, scratch, trails, host);
}

// decomp: EFFECT VS_TRAIL_INIT_FUNCTION (src/EFFECT/trails9.h:387).
// decomp: PLG049.BIN func_800FA76C (src/EFFECT/PLG049.BIN/E0.c:4).
// decomp: PLG050.BIN func_800FB390 (src/EFFECT/PLG050.BIN/E0.c:10).
// decomp: PLG057.BIN func_800FC600 (src/EFFECT/PLG057.BIN/2D0.c:18).
// decomp: PLG069.BIN func_800FB278 (src/EFFECT/PLG069.BIN/E0.c:11).
// decomp: PLG135.BIN func_800FB6D8 (src/EFFECT/PLG135.BIN/E0.c:16).
// decomp: PLG161.BIN func_800FC260 (src/EFFECT/PLG161.BIN/F8.c:14).
// decomp: PLG165.BIN func_800FA784 (src/EFFECT/PLG165.BIN/F8.c:12).
// decomp: PLG167.BIN func_800FC590 (src/EFFECT/PLG167.BIN/E0.c:19).
// decomp: PLG168.BIN func_800FA87C (src/EFFECT/PLG168.BIN/1F0.c:6).
// decomp: PLG169.BIN func_800FA76C (src/EFFECT/PLG169.BIN/E0.c:12).
// decomp: PLG232.BIN func_800FA76C (src/EFFECT/PLG232.BIN/E0.c:6).
// The plugin instances are byte-identical; each overlay's renderer-slot entry
// (TrailsLive.cpp) binds this one definition.
void TrailNineInit(const TrailDefinition& definition, TrailScratch& scratch,
                   std::span<TrailNineRecord> trails, std::int32_t /*distance*/,
                   TrailNineHost& host) {
  StartTrails<kTrailNineHistory>(definition, scratch, trails, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG057.BIN func_800FE70C (src/EFFECT/PLG057.BIN/2D0.c:28).
void Plg057::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG073.BIN func_800FC28C (src/EFFECT/PLG073.BIN/F8.c:14).
// decomp: PLG074.BIN func_800FC28C (src/EFFECT/PLG074.BIN/F8.c:13).
void Plg073::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG086.BIN func_800FD0E8 (src/EFFECT/PLG086.BIN/F8.c:23).
void Plg086::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG087.BIN func_800FCEC0 (src/EFFECT/PLG087.BIN/1F0.c:25).
void Plg087::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG128.BIN func_800FF248 (src/EFFECT/PLG128.BIN/F8.c:256).
void Plg128::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG134.BIN func_800FA758 (src/EFFECT/PLG134.BIN/E0.c:12).
void Plg134::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG137.BIN func_800FB27C (src/EFFECT/PLG137.BIN/F8.c:11).
void Plg137::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG141.BIN func_800FA758 (src/EFFECT/PLG141.BIN/E0.c:6).
void Plg141::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG146.BIN func_800FE5D4 (src/EFFECT/PLG146.BIN/1D8.c:43).
void Plg146::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG148.BIN func_800FA758 (src/EFFECT/PLG148.BIN/E0.c:6).
void Plg148::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG242.BIN func_800FC1B8 (src/EFFECT/PLG242.BIN/F8.c:13).
void Plg242::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG255.BIN func_800FA850 (src/EFFECT/PLG255.BIN/1D8.c:6).
void Plg255::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG265.BIN func_800FCE4C (src/EFFECT/PLG265.BIN/2E8.c:34).
void Plg265::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG266.BIN func_800FDBF8 (src/EFFECT/PLG266.BIN/2E8.c:42).
void Plg266::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG291.BIN func_800FA770 (src/EFFECT/PLG291.BIN/F8.c:12).
void Plg291::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects
