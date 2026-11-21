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

// decomp: EFFECT VS_TRAIL_UPDATE_FUNCTION (src/EFFECT/trailEffect.h:453).
void TrailUpdate(const TrailDefinition& /*definition*/,
                 TrailScratch& /*scratch*/, std::span<TrailRecord> trails,
                 std::int32_t distance, TrailHost& /*host*/) {
  UpdateTrails<kTrailHistory>(trails, distance);
}

// decomp: EFFECT VS_TRAIL_UPDATE_FUNCTION (src/EFFECT/trailEffect.h:453).
// decomp: PLG049.BIN vs_spiritSurge_0_update (src/EFFECT/trailEffect.h:453; was
// func_800FA8E4). decomp: PLG050.BIN vs_effect_darkChant_1_update
// (src/EFFECT/trailEffect.h:453; was func_800FB508). decomp: PLG057.BIN
// vs_effect_thunderBurstLv1_1_update (src/EFFECT/trailEffect.h:453; was
// func_800FC778). decomp: PLG069.BIN vs_effect_avalancheLv1_1_update
// (src/EFFECT/trailEffect.h:453; was func_800FB3F0). decomp: PLG135.BIN
// vs_effect_rendingGale_1_update (src/EFFECT/trailEffect.h:453; was
// func_800FB850). decomp: PLG161.BIN vs_effect_gigaTempest_2_update
// (src/EFFECT/trailEffect.h:453; was func_800FC3D8). decomp: PLG165.BIN
// vs_effect_deathWail_0_update (src/EFFECT/trailEffect.h:453; was
// func_800FA8FC). decomp: PLG167.BIN vs_effect_lotusPalm_2_update
// (src/EFFECT/trailEffect.h:453; was func_800FC708). decomp: PLG168.BIN
// vs_effect_vertigo_0_update (src/EFFECT/trailEffect.h:453; was func_800FA9F4).
// decomp: PLG169.BIN vs_effect_vermillionAura_0_update
// (src/EFFECT/trailEffect.h:453; was func_800FA8E4). decomp: PLG232.BIN
// vs_effect_plg232_0_update (src/EFFECT/trailEffect.h:453; was func_800FA8E4).
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
// decomp: PLG057.BIN vs_effect_thunderBurstLv1_2_update
// (src/EFFECT/trailEffect.h:453; was func_800FE884).
void Plg057::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG073.BIN vs_effect_radialSurgeLv1_2_update
// (src/EFFECT/trailEffect.h:453; was func_800FC404). decomp: PLG074.BIN
// func_800FC404 (src/EFFECT/PLG074.BIN/F8.c:15).
void Plg073::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG086.BIN vs_effect_judgment_3_update (src/EFFECT/trailEffect.h:453;
// was func_800FD260).
void Plg086::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_3_update
// (src/EFFECT/trailEffect.h:453; was func_800FD038).
void Plg087::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG128.BIN vs_effect_bloodySin_6_update
// (src/EFFECT/trailEffect.h:453; was func_800FF3C0).
void Plg128::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG134.BIN vs_effect_wyrmScorn_0_update
// (src/EFFECT/trailEffect.h:453; was func_800FA8D0).
void Plg134::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG137.BIN vs_effect_cherryRonde_1_update
// (src/EFFECT/trailEffect.h:453; was func_800FB3F4).
void Plg137::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG141.BIN vs_effect_swallowSlash_0_update
// (src/EFFECT/trailEffect.h:453; was func_800FA8D0).
void Plg141::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_4_update
// (src/EFFECT/trailEffect.h:453; was func_800FE74C).
void Plg146::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG148.BIN vs_effect_accursedUmbra_0_update
// (src/EFFECT/trailEffect.h:453; was func_800FA8D0).
void Plg148::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG242.BIN vs_effect_plg242_2_update (src/EFFECT/trailEffect.h:453;
// was func_800FC330).
void Plg242::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG255.BIN vs_effect_plg255_0_update (src/EFFECT/trailEffect.h:453;
// was func_800FA9C8).
void Plg255::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG265.BIN vs_effect_plg265_2_update (src/EFFECT/trailEffect.h:453;
// was func_800FCFC4).
void Plg265::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG266.BIN vs_effect_plg266_2_update (src/EFFECT/trailEffect.h:453;
// was func_800FDD70).
void Plg266::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}

// decomp: PLG291.BIN vs_effect_plg291_0_update (src/EFFECT/trailEffect.h:453;
// was func_800FA8E8).
void Plg291::TrailUpdate(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailUpdate(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects
