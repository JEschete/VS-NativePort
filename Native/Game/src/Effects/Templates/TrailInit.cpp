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

// decomp: EFFECT VS_TRAIL_INIT_FUNCTION (src/EFFECT/trailEffect.h:424).
void TrailInit(const TrailDefinition& definition, TrailScratch& scratch,
               std::span<TrailRecord> trails, std::int32_t /*distance*/,
               TrailHost& host) {
  StartTrails<kTrailHistory>(definition, scratch, trails, host);
}

// decomp: EFFECT VS_TRAIL_INIT_FUNCTION (src/EFFECT/trailEffect.h:424).
// decomp: PLG049.BIN vs_spiritSurge_0_init (src/EFFECT/trailEffect.h:424; was
// func_800FA76C). decomp: PLG050.BIN vs_effect_darkChant_1_init
// (src/EFFECT/trailEffect.h:424; was func_800FB390). decomp: PLG057.BIN
// vs_effect_thunderBurstLv1_1_init (src/EFFECT/trailEffect.h:424; was
// func_800FC600). decomp: PLG069.BIN vs_effect_avalancheLv1_1_init
// (src/EFFECT/trailEffect.h:424; was func_800FB278). decomp: PLG135.BIN
// vs_effect_rendingGale_1_init (src/EFFECT/trailEffect.h:424; was
// func_800FB6D8). decomp: PLG161.BIN vs_effect_gigaTempest_2_init
// (src/EFFECT/trailEffect.h:424; was func_800FC260). decomp: PLG165.BIN
// vs_effect_deathWail_0_init (src/EFFECT/trailEffect.h:424; was func_800FA784).
// decomp: PLG167.BIN vs_effect_lotusPalm_2_init (src/EFFECT/trailEffect.h:424;
// was func_800FC590). decomp: PLG168.BIN vs_effect_vertigo_0_init
// (src/EFFECT/trailEffect.h:424; was func_800FA87C). decomp: PLG169.BIN
// vs_effect_vermillionAura_0_init (src/EFFECT/trailEffect.h:424; was
// func_800FA76C). decomp: PLG232.BIN vs_effect_plg232_0_init
// (src/EFFECT/trailEffect.h:424; was func_800FA76C). The plugin instances are
// byte-identical; each overlay's renderer-slot entry (TrailsLive.cpp) binds
// this one definition.
void TrailNineInit(const TrailDefinition& definition, TrailScratch& scratch,
                   std::span<TrailNineRecord> trails, std::int32_t /*distance*/,
                   TrailNineHost& host) {
  StartTrails<kTrailNineHistory>(definition, scratch, trails, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG057.BIN vs_effect_thunderBurstLv1_2_init
// (src/EFFECT/trailEffect.h:424; was func_800FE70C).
void Plg057::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG073.BIN vs_effect_radialSurgeLv1_2_init
// (src/EFFECT/trailEffect.h:424; was func_800FC28C). decomp: PLG074.BIN
// func_800FC28C (src/EFFECT/PLG074.BIN/F8.c:13).
void Plg073::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG086.BIN vs_effect_judgment_3_init (src/EFFECT/trailEffect.h:424;
// was func_800FD0E8).
void Plg086::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_3_init (src/EFFECT/trailEffect.h:424;
// was func_800FCEC0).
void Plg087::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG128.BIN vs_effect_bloodySin_6_init (src/EFFECT/trailEffect.h:424;
// was func_800FF248).
void Plg128::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG134.BIN vs_effect_wyrmScorn_0_init (src/EFFECT/trailEffect.h:424;
// was func_800FA758).
void Plg134::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG137.BIN vs_effect_cherryRonde_1_init
// (src/EFFECT/trailEffect.h:424; was func_800FB27C).
void Plg137::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG141.BIN vs_effect_swallowSlash_0_init
// (src/EFFECT/trailEffect.h:424; was func_800FA758).
void Plg141::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_4_init
// (src/EFFECT/trailEffect.h:424; was func_800FE5D4).
void Plg146::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG148.BIN vs_effect_accursedUmbra_0_init
// (src/EFFECT/trailEffect.h:424; was func_800FA758).
void Plg148::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG242.BIN vs_effect_plg242_2_init (src/EFFECT/trailEffect.h:424; was
// func_800FC1B8).
void Plg242::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG255.BIN vs_effect_plg255_0_init (src/EFFECT/trailEffect.h:424; was
// func_800FA850).
void Plg255::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG265.BIN vs_effect_plg265_2_init (src/EFFECT/trailEffect.h:424; was
// func_800FCE4C).
void Plg265::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG266.BIN vs_effect_plg266_2_init (src/EFFECT/trailEffect.h:424; was
// func_800FDBF8).
void Plg266::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}

// decomp: PLG291.BIN vs_effect_plg291_0_init (src/EFFECT/trailEffect.h:424; was
// func_800FA770).
void Plg291::TrailInit(const Templates::TrailDefinition& definition,
                       Templates::TrailScratch& scratch,
                       std::span<Templates::TrailRecord> trails,
                       std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailInit(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects
