#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/TrailEmitter.hpp"

// The records, services and arithmetic shared by the five- and nine-sample
// trail stages (decomp src/EFFECT/trails5.h and trails9.h, which differ only
// in their history length). A trail is a four-point spline from an origin to
// an attraction target; it draws as a ribbon through its five (or nine)
// newest samples of that spline. TrailEmitter.hpp places the origins and runs
// the emitter's lifecycle; the target, control, init, update and render stages
// (TrailTarget.cpp ... TrailRender.cpp) work on the value records here. The
// effect package, the emitter scratch, the GTE, the texture animation and the
// frame's ordering table are reached through TrailHost or TrailNineHost
// (TrailsLive.hpp binds them).
namespace VS::Effects::Templates {
inline constexpr std::size_t kTrailSplinePoints = 4;
inline constexpr std::size_t kTrailHistory = kTrailTransparencySamples;
inline constexpr std::size_t kTrailNineHistory = kTrailNineTransparencySamples;
// A new trail fades from the last history entry to the first.
template <std::size_t History>
inline constexpr std::uint8_t kTrailFullCollapseOf = History - 1;
inline constexpr std::uint8_t kTrailFullCollapse = kTrailHistory - 1;
inline constexpr std::uint8_t kTrailNineFullCollapse = kTrailNineHistory - 1;
inline constexpr std::size_t kTrailAxes = Core::Math::kMatrixOrder;

// Emitter scratch flags the stages read: bits 6-8 choose what the attraction
// target is offset by (0x80 the action request, 0xC0 the attraction
// transform; other values nothing), and the ring flag that projects through
// the effect transform's view.
inline constexpr std::uint32_t kTrailAttractionMask = 0x1C0;
inline constexpr std::uint32_t kTrailAttractionRequest = 0x80;
inline constexpr std::uint32_t kTrailAttractionTransform = 0xC0;
inline constexpr std::uint32_t kTrailEffectView = kRingEffectView;

using TrailVectorRange = std::array<Core::Math::ScalarSpan, kTrailAxes>;
using TrailPlaneRange = std::array<Core::Math::ScalarSpan, 2>;

// Curve ids the stages sample at the emitter's curve distance; a zero colour
// curve keeps that channel at half intensity.
struct TrailCurves {
  std::uint8_t attraction_offset{};
  std::uint8_t attraction_jitter{};
  std::uint8_t mass{};
  std::uint8_t gravity_scale{};
  std::uint8_t speed{};
  std::uint8_t direction_jitter{};
  std::uint8_t direction{};
  std::uint8_t force{};
  std::uint8_t acceleration{};
  std::array<std::uint8_t, kTrailAxes> color{};
};

// The fields of a particle definition the trail stages read. The trails reuse
// the particle ranges under their own meaning: "mass" is the radius of the
// first control point, "gravity scale" that of the second, "speed" their
// distances along the spline, "force" the spread of those distances,
// "acceleration" the ribbon width and "lifetime" the step count.
struct TrailDefinition {
  TrailCurves curves{};
  TrailVectorRange attraction_offset{};
  TrailVectorRange attraction_jitter{};
  TrailVectorRange direction{};
  TrailVectorRange direction_jitter{};
  TrailVectorRange force{};
  TrailVectorRange acceleration{};
  TrailPlaneRange mass{};
  TrailPlaneRange gravity_scale{};
  TrailPlaneRange speed{};
  TrailPlaneRange lifetime{};
  Core::Math::ScalarSpan inner_extent{};
};

// The emitter-scratch values the stages read and leave behind
// (Effects_ParticleEmitterScratch). The attraction offset and jitter, the
// force and the acceleration are halfword vectors: the stages store them
// reduced to signed halfwords and read them back that way.
struct TrailScratch {
  std::uint32_t flags{};
  Core::Math::Vec3 attraction_offset{};
  Core::Math::Vec3 attraction_jitter{};
  Core::Math::Vec2 mass{};
  Core::Math::Vec2 gravity_scale{};
  Core::Math::Vec2 speed{};
  Core::Math::Vec2 lifetime{};
  Core::Math::Vec3 direction_jitter{};
  Core::Math::Vec3 base_direction{};
  Core::Math::Vec3 force_min{};
  Core::Math::Vec3 aim_point{};
  Core::Math::Vec3 spawn_point{};
  Core::Math::Mat3 aim_matrix{};
  Core::Math::Vec3 acceleration_min{};

  friend bool operator==(const TrailScratch&, const TrailScratch&) = default;
};

// A trail's four spline points: the origin, two control points and the target.
using TrailSpline = std::array<TrailPoint, kTrailSplinePoints>;

// One trail (Effects_Trail, or Effects_Trail9 with a nine-sample history): its
// spline, the step count it spends crossing the spline, the history of spline
// samples it draws and the recurrence that moves it along. `current` is the
// newest history entry; `collapse` counts the ticks left once the trail has
// reached its target.
template <std::size_t History>
struct BasicTrailRecord {
  TrailSpline points{};
  std::uint8_t step_count{};
  std::uint8_t sample_count{};
  std::uint8_t collapse{};
  std::uint8_t current{};
  std::array<TrailPoint, History> samples{};
  std::int32_t progress{};
  std::int32_t speed{};
  // The initial speed step, which nothing reads after init.
  std::int32_t first_step{};

  friend bool operator==(const BasicTrailRecord&,
                         const BasicTrailRecord&) = default;
};
using TrailRecord = BasicTrailRecord<kTrailHistory>;
using TrailNineRecord = BasicTrailRecord<kTrailNineHistory>;

// Why the render stage left a trail undrawn. The original read and wrote
// outside its five- or nine-entry history and transparency arrays for these; a
// trail the stages produce never has them.
enum class TrailFault : std::uint8_t {
  kHistoryOverflow,
  kCursorOutside,
  kCollapseOutside,
  kFadeUnderflow,
};

// Services the trail stages use whatever their history length. A host refuses
// by returning nothing; TrailsLive.hpp logs every refusal.
class TrailServices {
 public:
  TrailServices() = default;
  TrailServices(const TrailServices&) = delete;
  TrailServices(TrailServices&&) = delete;
  TrailServices& operator=(const TrailServices&) = delete;
  TrailServices& operator=(TrailServices&&) = delete;
  virtual ~TrailServices() = default;

  [[nodiscard]] virtual std::int32_t SampleCurve(std::uint8_t curve,
                                                 std::int32_t distance) = 0;
  // The BIOS generator: a value in [0, 0x7FFF].
  [[nodiscard]] virtual std::int32_t Rand() = 0;
  // A value between two bounds given in either order (vs_main_randRange).
  [[nodiscard]] virtual std::int32_t RandomRangeExclusive(std::int32_t a,
                                                          std::int32_t b) = 0;
  // The action request's position.
  [[nodiscard]] virtual Core::Math::Vec3 RequestPosition() = 0;
  // The translation of the definition's attraction transform, or nothing
  // when the effect instance lacks it.
  [[nodiscard]] virtual std::optional<Core::Math::Vec3>
  AttractionTranslation() = 0;
  // Loads the effect transform's view (true) or the frame's view matrix;
  // false when the effect instance lacks the transform.
  [[nodiscard]] virtual bool BeginProjection(bool effect_view) = 0;
  [[nodiscard]] virtual RingProjection Project(TrailPoint point) = 0;
  // Steps the texture animation; nothing when no texture can be drawn.
  [[nodiscard]] virtual std::optional<RingTexture> AdvanceTexture() = 0;
  [[nodiscard]] virtual std::int32_t NearClip() = 0;
  virtual void Submit(const RingQuad& quad, std::int32_t depth) = 0;
  virtual void ReportSkippedTrail(std::size_t index, TrailFault fault) = 0;
};

// The services plus the emitter's `History` transparency samples, newest
// history entry last.
template <std::size_t History>
class BasicTrailHost : public TrailServices {
 public:
  [[nodiscard]] virtual std::array<std::uint8_t, History> Transparency() = 0;
};
using TrailHost = BasicTrailHost<kTrailHistory>;
using TrailNineHost = BasicTrailHost<kTrailNineHistory>;

// One stage over an emitter's trails at its curve distance. Every stage has
// this shape so the emitter can bind an overlay's five instances as a set.
template <std::size_t History>
using BasicTrailStage = void (*)(const TrailDefinition& definition,
                                 TrailScratch& scratch,
                                 std::span<BasicTrailRecord<History>> trails,
                                 std::int32_t distance,
                                 BasicTrailHost<History>& host);
using TrailStage = BasicTrailStage<kTrailHistory>;
using TrailNineStage = BasicTrailStage<kTrailNineHistory>;

// An overlay's target, control, init, update and render instances, in the
// order the emitter runs them.
template <std::size_t History>
struct BasicTrailStages {
  BasicTrailStage<History> target = nullptr;
  BasicTrailStage<History> control = nullptr;
  BasicTrailStage<History> init = nullptr;
  BasicTrailStage<History> update = nullptr;
  BasicTrailStage<History> render = nullptr;
};
using TrailStages = BasicTrailStages<kTrailHistory>;
using TrailNineStages = BasicTrailStages<kTrailNineHistory>;

// The halfword vector a point holds, widened to words.
[[nodiscard]] constexpr Core::Math::Vec3 TrailWide(TrailPoint point) noexcept {
  return {point.x, point.y, point.z};
}

// A word vector stored into a halfword vector (each component's low 16 bits).
[[nodiscard]] constexpr TrailPoint TrailNarrow(
    Core::Math::Vec3 value) noexcept {
  return {Core::Math::WrapSigned16(static_cast<std::uint32_t>(value.x)),
          Core::Math::WrapSigned16(static_cast<std::uint32_t>(value.y)),
          Core::Math::WrapSigned16(static_cast<std::uint32_t>(value.z))};
}

// A word vector after a round trip through a halfword vector.
[[nodiscard]] constexpr Core::Math::Vec3 TrailHalfwords(
    Core::Math::Vec3 value) noexcept {
  return TrailWide(TrailNarrow(value));
}

// The remainder the R3000 DIV instruction leaves in HI: a zero divisor leaves
// the dividend and INT32_MIN % -1 gives 0.
[[nodiscard]] constexpr std::int32_t TrailRemainder(
    std::int32_t dividend, std::int32_t divisor) noexcept {
  if (divisor == 0) {
    return dividend;
  }
  if (divisor == -1) {
    return 0;
  }
  return dividend % divisor;
}

// A seven-bit interpolation of a range at its curve's sample.
[[nodiscard]] Core::Math::Vec3 SampleTrailVector(const TrailVectorRange& range,
                                                 std::uint8_t curve,
                                                 std::int32_t distance,
                                                 TrailServices& host);
[[nodiscard]] Core::Math::Vec2 SampleTrailPlane(const TrailPlaneRange& range,
                                                std::uint8_t curve,
                                                std::int32_t distance,
                                                TrailServices& host);

// The speed change of one update with `remaining` steps left:
// 2 * (travel - progress) / remaining^2 - 2 * speed / remaining, which brings
// the trail to rest at its target as the steps run out (trails5.h:407,
// trails9.h:414). The init stage stores its value for a new trail. Divisions
// truncate and every product wraps as the original's word arithmetic did.
template <std::size_t History>
[[nodiscard]] std::int32_t TrailSpeedStep(
    const BasicTrailRecord<History>& trail, std::int32_t remaining) noexcept;
extern template std::int32_t TrailSpeedStep(const TrailRecord& trail,
                                            std::int32_t remaining) noexcept;
extern template std::int32_t TrailSpeedStep(const TrailNineRecord& trail,
                                            std::int32_t remaining) noexcept;
}  // namespace VS::Effects::Templates
