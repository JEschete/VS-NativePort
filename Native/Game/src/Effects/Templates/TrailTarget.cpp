// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/TrailTarget.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/TrailCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

[[nodiscard]] math::Vec3 Offset(math::Vec3 point, math::Vec3 by) noexcept {
  return {math::WrappingAdd(point.x, by.x), math::WrappingAdd(point.y, by.y),
          math::WrappingAdd(point.z, by.z)};
}

// One axis of a trail's target: the offset plus a random value between the
// jitter and its negation, stored as a halfword.
[[nodiscard]] std::int16_t JitteredAxis(std::int32_t offset,
                                        std::int32_t jitter,
                                        TrailServices& host) {
  const std::int32_t spread =
      host.RandomRangeExclusive(jitter, math::WrappingNegate(jitter));
  return math::WrapSigned16(
      static_cast<std::uint32_t>(math::WrappingAdd(offset, spread)));
}

template <std::size_t History>
void AimTrails(const TrailDefinition& definition, TrailScratch& scratch,
               std::span<BasicTrailRecord<History>> trails,
               std::int32_t distance, TrailServices& host) {
  math::Vec3 offset = TrailHalfwords(
      SampleTrailVector(definition.attraction_offset,
                        definition.curves.attraction_offset, distance, host));
  const math::Vec3 jitter = TrailHalfwords(
      SampleTrailVector(definition.attraction_jitter,
                        definition.curves.attraction_jitter, distance, host));
  switch (scratch.flags & kTrailAttractionMask) {
    case kTrailAttractionTransform:
      // The original indexed the effect's transforms without a bound; a
      // host without the transform leaves the offset as sampled.
      if (const std::optional<math::Vec3> translation =
              host.AttractionTranslation()) {
        offset = TrailHalfwords(Offset(offset, *translation));
      }
      break;
    case kTrailAttractionRequest:
      offset = TrailHalfwords(Offset(offset, host.RequestPosition()));
      break;
    default:
      break;
  }
  scratch.attraction_offset = offset;
  scratch.attraction_jitter = jitter;
  for (BasicTrailRecord<History>& trail : trails) {
    TrailPoint& target = trail.points.back();
    target.x = JitteredAxis(offset.x, jitter.x, host);
    target.y = JitteredAxis(offset.y, jitter.y, host);
    target.z = JitteredAxis(offset.z, jitter.z, host);
  }
}
}  // namespace

// decomp: EFFECT VS_TRAIL_TARGET_FUNCTION (src/EFFECT/trailEffect.h:288).
void TrailTarget(const TrailDefinition& definition, TrailScratch& scratch,
                 std::span<TrailRecord> trails, std::int32_t distance,
                 TrailHost& host) {
  AimTrails<kTrailHistory>(definition, scratch, trails, distance, host);
}

// decomp: EFFECT VS_TRAIL_TARGET_FUNCTION (src/EFFECT/trailEffect.h:288).
// decomp: PLG049.BIN vs_spiritSurge_0_target (src/EFFECT/trailEffect.h:288; was
// func_800FA07C). decomp: PLG050.BIN vs_effect_darkChant_1_target
// (src/EFFECT/trailEffect.h:288; was func_800FACA0). decomp: PLG057.BIN
// vs_effect_thunderBurstLv1_1_target (src/EFFECT/trailEffect.h:288; was
// func_800FBF10). decomp: PLG069.BIN vs_effect_avalancheLv1_1_target
// (src/EFFECT/trailEffect.h:288; was func_800FAB88). decomp: PLG135.BIN
// vs_effect_rendingGale_1_target (src/EFFECT/trailEffect.h:288; was
// func_800FAFE8). decomp: PLG161.BIN vs_effect_gigaTempest_2_target
// (src/EFFECT/trailEffect.h:288; was func_800FBB70). decomp: PLG165.BIN
// vs_effect_deathWail_0_target (src/EFFECT/trailEffect.h:288; was
// func_800FA094). decomp: PLG167.BIN vs_effect_lotusPalm_2_target
// (src/EFFECT/trailEffect.h:288; was func_800FBEA0). decomp: PLG168.BIN
// vs_effect_vertigo_0_target (src/EFFECT/trailEffect.h:288; was func_800FA18C).
// decomp: PLG169.BIN vs_effect_vermillionAura_0_target
// (src/EFFECT/trailEffect.h:288; was func_800FA07C). decomp: PLG232.BIN
// vs_effect_plg232_0_target (src/EFFECT/trailEffect.h:288; was func_800FA07C).
// The plugin instances are byte-identical; each overlay's renderer-slot entry
// (TrailsLive.cpp) binds this one definition.
void TrailNineTarget(const TrailDefinition& definition, TrailScratch& scratch,
                     std::span<TrailNineRecord> trails, std::int32_t distance,
                     TrailNineHost& host) {
  AimTrails<kTrailNineHistory>(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG057.BIN vs_effect_thunderBurstLv1_2_target
// (src/EFFECT/trailEffect.h:288; was func_800FE030).
void Plg057::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG073.BIN vs_effect_radialSurgeLv1_2_target
// (src/EFFECT/trailEffect.h:288; was func_800FBBB0). decomp: PLG074.BIN
// func_800FBBB0 (src/EFFECT/PLG074.BIN/F8.c:9).
void Plg073::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG086.BIN vs_effect_judgment_3_target (src/EFFECT/trailEffect.h:288;
// was func_800FCA0C).
void Plg086::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_3_target
// (src/EFFECT/trailEffect.h:288; was func_800FC7E4).
void Plg087::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG128.BIN vs_effect_bloodySin_6_target
// (src/EFFECT/trailEffect.h:288; was func_800FEB6C).
void Plg128::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG134.BIN vs_effect_wyrmScorn_0_target
// (src/EFFECT/trailEffect.h:288; was func_800FA07C).
void Plg134::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG137.BIN vs_effect_cherryRonde_1_target
// (src/EFFECT/trailEffect.h:288; was func_800FABA0).
void Plg137::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG141.BIN vs_effect_swallowSlash_0_target
// (src/EFFECT/trailEffect.h:288; was func_800FA07C).
void Plg141::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_4_target
// (src/EFFECT/trailEffect.h:288; was func_800FDEF8).
void Plg146::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG148.BIN vs_effect_accursedUmbra_0_target
// (src/EFFECT/trailEffect.h:288; was func_800FA07C).
void Plg148::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG242.BIN vs_effect_plg242_2_target (src/EFFECT/trailEffect.h:288;
// was func_800FBADC).
void Plg242::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG255.BIN vs_effect_plg255_0_target (src/EFFECT/trailEffect.h:288;
// was func_800FA174).
void Plg255::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG265.BIN vs_effect_plg265_2_target (src/EFFECT/trailEffect.h:288;
// was func_800FC770).
void Plg265::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG266.BIN vs_effect_plg266_2_target (src/EFFECT/trailEffect.h:288;
// was func_800FD51C).
void Plg266::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG291.BIN vs_effect_plg291_0_target (src/EFFECT/trailEffect.h:288;
// was func_800FA094).
void Plg291::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects
