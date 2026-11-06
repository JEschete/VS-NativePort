#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// The trail emitter behind PLG074's Radial Surge ribbons and the nine-sample
// trails of PLG049, PLG050 and the other trails9.h overlays: an origin stage
// that places the first spline point of every trail inside a sphere, box or
// ring, and a lifecycle that creates, ticks, expires and stops the emitter.
// Inputs are typed values and spans; randomness, trigonometry, rotation,
// memory and the shared trail stages are reached through interfaces, so this
// code reads no package, scratch block or PS1 address.
namespace VS::Effects {
struct TrailVector {
  std::int32_t x{};
  std::int32_t y{};
  std::int32_t z{};
  friend bool operator==(const TrailVector&, const TrailVector&) = default;
};

struct TrailPoint {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};
  friend bool operator==(const TrailPoint&, const TrailPoint&) = default;
};

inline constexpr std::size_t kMaxTrailsPerEmitter = 16;
inline constexpr std::size_t kTrailTransparencySamples = 5;
// The nine-sample emitters (trails9.h) hold at most eight trails.
inline constexpr std::size_t kMaxNineSampleTrailsPerEmitter = 8;
inline constexpr std::size_t kTrailNineTransparencySamples = 9;
inline constexpr std::uint8_t kOpaqueTrailTransparency = 0x80;
using TrailTransparency = std::array<std::uint8_t, kTrailTransparencySamples>;

// Particle definition flags the origin stage reads.
inline constexpr std::uint32_t kTrailAnchorMask = 0x7;
inline constexpr std::uint32_t kTrailAnchorEffectOrigin = 0x1;
inline constexpr std::uint32_t kTrailAnchorCaster = 0x2;
inline constexpr std::uint32_t kTrailShapeMask = 0x38;
inline constexpr std::uint32_t kTrailShapeSphere = 0x0;
inline constexpr std::uint32_t kTrailShapeBox = 0x8;
// Shapes 0x10-0x28 all place origins on a fixed four-point ring.
inline constexpr std::uint32_t kTrailShapeFourPointRingA = 0x10;
inline constexpr std::uint32_t kTrailShapeFourPointRingB = 0x18;
inline constexpr std::uint32_t kTrailShapeFourPointRingC = 0x20;
inline constexpr std::uint32_t kTrailShapeFourPointRingD = 0x28;
inline constexpr std::uint32_t kTrailShapeRing = 0x30;
inline constexpr std::uint32_t kTrailPlacementMask = 0x1800;
inline constexpr std::uint32_t kTrailPlacementCorners = 0x1000;
inline constexpr std::uint32_t kTrailPlacementNone = 0x1800;
inline constexpr std::uint32_t kTrailWorldEvents = 0x20000;
inline constexpr std::uint32_t kTrailRotateOrigins = 0x40000;

// One origin pass's inputs. Position and extent are the definition's ranges
// already sampled at the emitter's curve distance.
struct TrailOriginRequest {
  std::uint32_t flags{};
  TrailVector position{};
  TrailVector extent{};
  TrailVector effect_origin{};
  TrailVector caster_translation{};
  std::uint8_t ring_segments{};
  std::uint8_t ring_positions{};
};

// The emitter scratch fields the origin stage writes. Fields a pass does not
// reach keep their previous values, as the shared scratch block did.
struct TrailOriginScratch {
  TrailVector position{};
  TrailVector extent{};
  TrailVector aim_point{};
  std::int32_t sin_angle0{};
  std::int32_t sin_angle1{};
  std::int32_t cos_angle0{};
  std::int32_t cos_angle1{};
  std::int32_t ring_segments{};
  std::int32_t ring_positions{};
};

class TrailOriginServices {
 public:
  TrailOriginServices() = default;
  TrailOriginServices(const TrailOriginServices&) = delete;
  TrailOriginServices& operator=(const TrailOriginServices&) = delete;
  TrailOriginServices(TrailOriginServices&&) = delete;
  TrailOriginServices& operator=(TrailOriginServices&&) = delete;
  virtual ~TrailOriginServices() = default;

  // The BIOS generator: a value in [0, 0x7FFF].
  [[nodiscard]] virtual std::int32_t Rand() = 0;
  [[nodiscard]] virtual std::int32_t RandomRangeExclusive(std::int32_t a,
                                                          std::int32_t b) = 0;
  // Q12 sine and cosine of a 4096-step angle.
  [[nodiscard]] virtual std::int32_t Sin(std::int32_t angle) = 0;
  [[nodiscard]] virtual std::int32_t Cos(std::int32_t angle) = 0;
  // Rotates by the emitter transform's world rotation (no translation).
  [[nodiscard]] virtual TrailVector Rotate(TrailPoint point) = 0;
};

// Places the first spline point of every trail (trails5.h:27 and trails9.h:27;
// PLG074 func_800FB414). `origins` holds one point per trail and is updated in
// place; the "no placement" box mode leaves a point's previous value. `corner`
// is the overlay's emission cursor shared by the box-corner and ring
// placements. Coordinates wrap to 16 bits as the original SVECTOR stores did.
void GenerateTrailOrigins(const TrailOriginRequest& request,
                          std::span<TrailPoint> origins, std::int32_t& corner,
                          TrailOriginScratch& scratch,
                          TrailOriginServices& services);

// A new emitter's packed parameter: the low byte is its lifetime in ticks
// (zero lives until stopped), the next byte its particle definition.
struct TrailEmitterPlan {
  std::uint8_t lifetime{};
  std::uint8_t definition_index{};
};
[[nodiscard]] TrailEmitterPlan PlanTrailEmitter(std::int32_t packed);

// The trail count for a definition's first interval bound, capped at `limit`
// (sixteen five-sample or eight nine-sample trails). A negative bound has no
// count.
[[nodiscard]] std::optional<std::uint8_t> TrailCountFor(
    std::int16_t interval, std::size_t limit = kMaxTrailsPerEmitter);

template <std::size_t Samples>
struct SampledTransparency {
  std::array<std::uint8_t, Samples> samples{};
  // The original last sample of a one-entry curve read the byte after it;
  // that read is replaced with the curve's only entry.
  bool clamped = false;
};
using SampledTrailTransparency = SampledTransparency<kTrailTransparencySamples>;
// Five evenly spaced samples of a transparency curve (trails5.h:710). An
// empty curve yields the opaque default.
[[nodiscard]] SampledTrailTransparency SampleTrailTransparency(
    std::span<const std::uint8_t> curve);
// Nine evenly spaced samples of a transparency curve (trails9.h:716-734),
// with the same defaults.
[[nodiscard]] SampledTransparency<kTrailNineTransparencySamples>
SampleTrailNineTransparency(std::span<const std::uint8_t> curve);

// A trail sample's position as a Q12 particle position, wrapping like the
// original 32-bit products.
[[nodiscard]] TrailVector TrailEventPosition(TrailVector point);

enum class TrailEmitterCommand : std::uint32_t {
  kCreate = 1,
  kTick = 2,
  kExpire = 3,
  kStop = 4,
};

struct TrailEmitterCounters {
  std::uint8_t lifetime{};
  bool initialized = false;
  std::int32_t curve_sample_distance{};
};

// The memory and stage operations an emitter's lifecycle drives.
class TrailEmitterHost {
 public:
  TrailEmitterHost() = default;
  TrailEmitterHost(const TrailEmitterHost&) = delete;
  TrailEmitterHost& operator=(const TrailEmitterHost&) = delete;
  TrailEmitterHost(TrailEmitterHost&&) = delete;
  TrailEmitterHost& operator=(TrailEmitterHost&&) = delete;
  virtual ~TrailEmitterHost() = default;

  // Allocates and initializes the emitter for `plan`; a refused definition or
  // allocation leaves the slot without state.
  virtual void Create(TrailEmitterPlan plan) = 0;
  [[nodiscard]] virtual bool HasState() const = 0;
  [[nodiscard]] virtual TrailEmitterCounters Counters() const = 0;
  virtual void StoreCounters(const TrailEmitterCounters& counters) = 0;
  // Checks the emitter against the current package and loads its definition
  // flags into the shared scratch; false refuses the tick.
  [[nodiscard]] virtual bool BeginTick() = 0;
  // Runs the origin, target, control and init stages.
  virtual void InitializeTrails() = 0;
  // Runs the update and render stages and emits tick events.
  virtual void DrawTrails() = 0;
  virtual void ReleaseTrails() = 0;
};

// Runs one lifecycle command (trails5.h:677 and trails9.h:685; PLG074
// func_800FCFEC) and returns whether the emitter stays attached. The trail
// buffer is released when the lifetime runs out, on stop and on a refused tick;
// the emitter state itself belongs to the renderer slot. A slot without state
// detaches on its next tick.
[[nodiscard]] bool UpdateTrailEmitter(TrailEmitterCommand command,
                                      std::int32_t packed,
                                      TrailEmitterHost& host);
}  // namespace VS::Effects
