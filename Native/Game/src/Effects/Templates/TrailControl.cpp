// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/TrailControl.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/TrailCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// The control points between the origin (point 0) and the target (point 3).
constexpr std::size_t kFirstControl = 1;
constexpr std::size_t kSecondControl = 2;
// A control point's distance along the spline is a share of its length in
// 1/256ths.
constexpr std::int32_t kLengthShareScale = 256;

// One control point's random parts: its share of the spline's length, its
// radius about the spline's axis and its angle around it.
struct ControlSpread {
  std::int32_t share{};
  std::int32_t radius{};
  std::int32_t angle{};
};

// The speed sample plus, with a nonzero spread, a random value in
// [-spread / 2, spread / 2).
[[nodiscard]] std::int32_t LengthShare(std::int32_t speed, std::int32_t spread,
                                       TrailServices& host) {
  if (spread == 0) {
    return speed;
  }
  return math::WrappingSubtract(
      math::WrappingAdd(speed, TrailRemainder(host.Rand(), spread)),
      math::R3000Quotient(spread, 2));
}

// The plane's first value plus, with a nonzero second value, a random amount
// below it.
[[nodiscard]] std::int32_t ControlRadius(math::Vec2 range,
                                         TrailServices& host) {
  if (range.y == 0) {
    return range.x;
  }
  return math::WrappingAdd(range.x, TrailRemainder(host.Rand(), range.y));
}

// The base angle plus, with a nonzero jitter, a random value in
// [-jitter, jitter). The doubled jitter wraps as the original's word did.
[[nodiscard]] std::int32_t ControlAngle(std::int32_t base, std::int32_t jitter,
                                        TrailServices& host) {
  if (jitter == 0) {
    return base;
  }
  const std::int32_t turn =
      TrailRemainder(host.Rand(), math::WrappingMultiply(jitter, 2));
  return math::WrappingAdd(base, math::WrappingSubtract(turn, jitter));
}

// Both control points' random parts, drawn in the original's order: the two
// shares, the two radii, then the two angles.
[[nodiscard]] std::array<ControlSpread, 2> DrawControls(
    const TrailScratch& scratch, TrailServices& host) {
  std::array<ControlSpread, 2> draw{};
  auto& [first, second] = draw;
  first.share = LengthShare(scratch.speed.x, scratch.force_min.x, host);
  second.share = LengthShare(scratch.speed.y, scratch.force_min.y, host);
  first.radius = ControlRadius(scratch.mass, host);
  second.radius = ControlRadius(scratch.gravity_scale, host);
  first.angle =
      ControlAngle(scratch.base_direction.x, scratch.direction_jitter.x, host);
  second.angle =
      ControlAngle(scratch.base_direction.y, scratch.direction_jitter.y, host);
  return draw;
}

void SampleControls(const TrailDefinition& definition, TrailScratch& scratch,
                    std::int32_t distance, TrailServices& host) {
  const TrailCurves& curves = definition.curves;
  scratch.mass = SampleTrailPlane(definition.mass, curves.mass, distance, host);
  scratch.gravity_scale = SampleTrailPlane(
      definition.gravity_scale, curves.gravity_scale, distance, host);
  scratch.speed =
      SampleTrailPlane(definition.speed, curves.speed, distance, host);
  scratch.direction_jitter = SampleTrailVector(
      definition.direction_jitter, curves.direction_jitter, distance, host);
  scratch.base_direction =
      SampleTrailVector(definition.direction, curves.direction, distance, host);
  scratch.force_min = TrailHalfwords(
      SampleTrailVector(definition.force, curves.force, distance, host));
}

// The distance from the origin to the target, its squares summed in wrapping
// words as the original's.
[[nodiscard]] std::int32_t SplineLength(const TrailSpline& points) noexcept {
  const math::Vec3 from = TrailWide(points.front());
  const math::Vec3 to = TrailWide(points.back());
  const std::int32_t x = math::WrappingSubtract(to.x, from.x);
  const std::int32_t y = math::WrappingSubtract(to.y, from.y);
  const std::int32_t z = math::WrappingSubtract(to.z, from.z);
  return math::SquareRootInteger(
      math::WrappingAdd(math::WrappingAdd(math::WrappingMultiply(x, x),
                                          math::WrappingMultiply(y, y)),
                        math::WrappingMultiply(z, z)));
}

// The control point `along` units down the spline's axis, on the circle of
// its radius at its angle about the axis, in the look-at frame.
[[nodiscard]] TrailPoint LocalControl(const ControlSpread& spread,
                                      std::int32_t along) noexcept {
  return {math::WrapSigned16(static_cast<std::uint32_t>(
              RingFixed(math::Cosine(spread.angle), spread.radius))),
          math::WrapSigned16(static_cast<std::uint32_t>(
              RingFixed(math::Sine(spread.angle), spread.radius))),
          math::WrapSigned16(static_cast<std::uint32_t>(along))};
}

// The share of `length` in 1/256ths, truncated toward zero.
[[nodiscard]] std::int32_t ShareOf(std::int32_t length,
                                   std::int32_t share) noexcept {
  return math::R3000Quotient(math::WrappingMultiply(length, share),
                             kLengthShareScale);
}

void PlaceControls(TrailSpline& points, TrailScratch& scratch,
                   TrailServices& host) {
  const std::int32_t length = SplineLength(points);
  const auto [first_spread, second_spread] = DrawControls(scratch, host);
  // The first point sits its share along the spline, the second its share
  // beyond the spline's full length.
  const TrailPoint first =
      LocalControl(first_spread, ShareOf(length, first_spread.share));
  const TrailPoint second = LocalControl(
      second_spread,
      math::WrappingAdd(length, ShareOf(length, second_spread.share)));
  const math::Vec3 origin = TrailWide(points.front());
  scratch.aim_matrix =
      math::BuildLookAtOrientation(origin, TrailWide(points.back()));
  const math::Transform3 aim{.rotation = scratch.aim_matrix,
                             .translation = origin};
  scratch.aim_point = math::RotateTranslate(aim, TrailWide(first));
  points.at(kFirstControl) = TrailNarrow(scratch.aim_point);
  scratch.spawn_point = math::RotateTranslate(aim, TrailWide(second));
  points.at(kSecondControl) = TrailNarrow(scratch.spawn_point);
}

template <std::size_t History>
void SpreadTrails(const TrailDefinition& definition, TrailScratch& scratch,
                  std::span<BasicTrailRecord<History>> trails,
                  std::int32_t distance, TrailServices& host) {
  for (BasicTrailRecord<History>& trail : trails) {
    SampleControls(definition, scratch, distance, host);
    PlaceControls(trail.points, scratch, host);
  }
}
}  // namespace

// decomp: EFFECT VS_TRAIL_CONTROL_FUNCTION (src/EFFECT/trails5.h:278).
// The original transformed through the GTE and left the look-at matrix
// loaded; the render stage loads its own before projecting.
void TrailControl(const TrailDefinition& definition, TrailScratch& scratch,
                  std::span<TrailRecord> trails, std::int32_t distance,
                  TrailHost& host) {
  SpreadTrails<kTrailHistory>(definition, scratch, trails, distance, host);
}

// decomp: EFFECT VS_TRAIL_CONTROL_FUNCTION (src/EFFECT/trails9.h:278).
// decomp: PLG049.BIN func_800FA294 (src/EFFECT/PLG049.BIN/E0.c:3).
// decomp: PLG050.BIN func_800FAEB8 (src/EFFECT/PLG050.BIN/E0.c:9).
// decomp: PLG057.BIN func_800FC128 (src/EFFECT/PLG057.BIN/2D0.c:17).
// decomp: PLG069.BIN func_800FADA0 (src/EFFECT/PLG069.BIN/E0.c:10).
// decomp: PLG135.BIN func_800FB200 (src/EFFECT/PLG135.BIN/E0.c:15).
// decomp: PLG161.BIN func_800FBD88 (src/EFFECT/PLG161.BIN/F8.c:13).
// decomp: PLG165.BIN func_800FA2AC (src/EFFECT/PLG165.BIN/F8.c:11).
// decomp: PLG167.BIN func_800FC0B8 (src/EFFECT/PLG167.BIN/E0.c:18).
// decomp: PLG168.BIN func_800FA3A4 (src/EFFECT/PLG168.BIN/1F0.c:5).
// decomp: PLG169.BIN func_800FA294 (src/EFFECT/PLG169.BIN/E0.c:11).
// decomp: PLG232.BIN func_800FA294 (src/EFFECT/PLG232.BIN/E0.c:5).
// The plugin instances are byte-identical; each overlay's renderer-slot entry
// (TrailsLive.cpp) binds this one definition.
void TrailNineControl(const TrailDefinition& definition, TrailScratch& scratch,
                      std::span<TrailNineRecord> trails, std::int32_t distance,
                      TrailNineHost& host) {
  SpreadTrails<kTrailNineHistory>(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG057.BIN func_800FE248 (src/EFFECT/PLG057.BIN/2D0.c:27).
void Plg057::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG073.BIN func_800FBDC8 (src/EFFECT/PLG073.BIN/F8.c:13).
// decomp: PLG074.BIN func_800FBDC8 (src/EFFECT/PLG074.BIN/F8.c:11).
void Plg073::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG086.BIN func_800FCC24 (src/EFFECT/PLG086.BIN/F8.c:22).
void Plg086::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG087.BIN func_800FC9FC (src/EFFECT/PLG087.BIN/1F0.c:24).
void Plg087::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG128.BIN func_800FED84 (src/EFFECT/PLG128.BIN/F8.c:255).
void Plg128::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG134.BIN func_800FA294 (src/EFFECT/PLG134.BIN/E0.c:11).
void Plg134::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG137.BIN func_800FADB8 (src/EFFECT/PLG137.BIN/F8.c:10).
void Plg137::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG141.BIN func_800FA294 (src/EFFECT/PLG141.BIN/E0.c:5).
void Plg141::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG146.BIN func_800FE110 (src/EFFECT/PLG146.BIN/1D8.c:42).
void Plg146::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG148.BIN func_800FA294 (src/EFFECT/PLG148.BIN/E0.c:5).
void Plg148::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG242.BIN func_800FBCF4 (src/EFFECT/PLG242.BIN/F8.c:12).
void Plg242::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG255.BIN func_800FA38C (src/EFFECT/PLG255.BIN/1D8.c:5).
void Plg255::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG265.BIN func_800FC988 (src/EFFECT/PLG265.BIN/2E8.c:33).
void Plg265::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG266.BIN func_800FD734 (src/EFFECT/PLG266.BIN/2E8.c:41).
void Plg266::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}

// decomp: PLG291.BIN func_800FA2AC (src/EFFECT/PLG291.BIN/F8.c:11).
void Plg291::TrailControl(const Templates::TrailDefinition& definition,
                          Templates::TrailScratch& scratch,
                          std::span<Templates::TrailRecord> trails,
                          std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailControl(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects
