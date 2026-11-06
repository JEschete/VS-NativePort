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

// decomp: EFFECT VS_TRAIL_TARGET_FUNCTION (src/EFFECT/trails5.h:243).
void TrailTarget(const TrailDefinition& definition, TrailScratch& scratch,
                 std::span<TrailRecord> trails, std::int32_t distance,
                 TrailHost& host) {
  AimTrails<kTrailHistory>(definition, scratch, trails, distance, host);
}

// decomp: EFFECT VS_TRAIL_TARGET_FUNCTION (src/EFFECT/trails9.h:243).
// decomp: PLG049.BIN func_800FA07C (src/EFFECT/PLG049.BIN/E0.c:2).
// decomp: PLG050.BIN func_800FACA0 (src/EFFECT/PLG050.BIN/E0.c:8).
// decomp: PLG057.BIN func_800FBF10 (src/EFFECT/PLG057.BIN/2D0.c:16).
// decomp: PLG069.BIN func_800FAB88 (src/EFFECT/PLG069.BIN/E0.c:9).
// decomp: PLG135.BIN func_800FAFE8 (src/EFFECT/PLG135.BIN/E0.c:14).
// decomp: PLG161.BIN func_800FBB70 (src/EFFECT/PLG161.BIN/F8.c:12).
// decomp: PLG165.BIN func_800FA094 (src/EFFECT/PLG165.BIN/F8.c:10).
// decomp: PLG167.BIN func_800FBEA0 (src/EFFECT/PLG167.BIN/E0.c:17).
// decomp: PLG168.BIN func_800FA18C (src/EFFECT/PLG168.BIN/1F0.c:4).
// decomp: PLG169.BIN func_800FA07C (src/EFFECT/PLG169.BIN/E0.c:10).
// decomp: PLG232.BIN func_800FA07C (src/EFFECT/PLG232.BIN/E0.c:4).
// The plugin instances are byte-identical; each overlay's renderer-slot entry
// (TrailsLive.cpp) binds this one definition.
void TrailNineTarget(const TrailDefinition& definition, TrailScratch& scratch,
                     std::span<TrailNineRecord> trails, std::int32_t distance,
                     TrailNineHost& host) {
  AimTrails<kTrailNineHistory>(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG057.BIN func_800FE030 (src/EFFECT/PLG057.BIN/2D0.c:26).
void Plg057::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG073.BIN func_800FBBB0 (src/EFFECT/PLG073.BIN/F8.c:12).
// decomp: PLG074.BIN func_800FBBB0 (src/EFFECT/PLG074.BIN/F8.c:9).
void Plg073::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG086.BIN func_800FCA0C (src/EFFECT/PLG086.BIN/F8.c:21).
void Plg086::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG087.BIN func_800FC7E4 (src/EFFECT/PLG087.BIN/1F0.c:23).
void Plg087::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG128.BIN func_800FEB6C (src/EFFECT/PLG128.BIN/F8.c:254).
void Plg128::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG134.BIN func_800FA07C (src/EFFECT/PLG134.BIN/E0.c:10).
void Plg134::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG137.BIN func_800FABA0 (src/EFFECT/PLG137.BIN/F8.c:9).
void Plg137::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG141.BIN func_800FA07C (src/EFFECT/PLG141.BIN/E0.c:4).
void Plg141::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG146.BIN func_800FDEF8 (src/EFFECT/PLG146.BIN/1D8.c:41).
void Plg146::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG148.BIN func_800FA07C (src/EFFECT/PLG148.BIN/E0.c:4).
void Plg148::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG242.BIN func_800FBADC (src/EFFECT/PLG242.BIN/F8.c:11).
void Plg242::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG255.BIN func_800FA174 (src/EFFECT/PLG255.BIN/1D8.c:4).
void Plg255::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG265.BIN func_800FC770 (src/EFFECT/PLG265.BIN/2E8.c:32).
void Plg265::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG266.BIN func_800FD51C (src/EFFECT/PLG266.BIN/2E8.c:40).
void Plg266::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}

// decomp: PLG291.BIN func_800FA094 (src/EFFECT/PLG291.BIN/F8.c:10).
void Plg291::TrailTarget(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailTarget(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects
