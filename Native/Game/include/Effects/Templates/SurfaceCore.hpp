#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/RingCore.hpp"

// The commands, sampling, colour, depth and lifetime rules shared by five
// effect templates: the grid (decomp src/EFFECT/grids.h), the ten-vertex mesh
// (meshes10.h) and three sheets of sixteen-point rows around a vertical axis,
// the surface (surfaces.h), the mirrored surface (mirroredSurfaces.h) and the
// rotating surface (rotatingSurfaces.h). Points, projections, textures,
// colours and sheet quads are the ring records (RingCore.hpp). Every
// function works on value records; the effect package, the emitter scratch,
// the GTE, the texture animation and the frame ordering table are reached
// through SurfaceHost (SurfacesLive.hpp binds it).
namespace VS::Effects::Templates {
inline constexpr std::size_t kSurfaceColumns = kClosedRingColumns;
inline constexpr std::size_t kSurfaceRows = 5;
inline constexpr std::size_t kMirrorSurfaceRows = 9;
// A surface profile: five rows of sixteen unit points (Q12) the sheet scales
// by its radius and height.
inline constexpr auto kSurfaceProfilePoints = kSurfaceRows * kSurfaceColumns;
inline constexpr std::size_t kColorChannels = 3;
// The points of the largest sheet (the mirrored surface's nine rows); the
// other sheets fill the first five rows.
inline constexpr auto kSheetPoints = kMirrorSurfaceRows * kSurfaceColumns;

// Renderer slot commands (Effects_EffectRenderer).
enum class SurfaceCommand : std::uint32_t {
  kCreate = 1,
  kTick = 2,
  kExpire = 3,
  kStop = 4,
};

// How many columns one copy of a sheet's texture spans: four (it repeats
// four times around the sheet) or sixteen (once).
enum class SurfaceTextureSpan : std::uint8_t {
  kFourColumns = 4,
  kSixteenColumns = 16,
};

// Curve ids the ticks sample at the effect's age.
struct SurfaceCurves {
  std::uint8_t position{};
  std::uint8_t extent{};
  std::uint8_t inner_extent{};
  std::uint8_t direction{};
  std::uint8_t direction_jitter{};
  std::uint8_t speed{};
  std::uint8_t force{};
  std::uint8_t acceleration{};
  std::uint8_t attraction_jitter{};
  std::uint8_t mass{};
  std::uint8_t gravity{};
  std::array<std::uint8_t, kColorChannels> color{};
  // The mesh's second colour reads its curve ids from the minimum force
  // range's start halfwords (vectorRanges[2][0], [2][2] and [2][4]).
  std::array<std::int16_t, kColorChannels> second_color{};
};

// The fields of a particle definition the five templates read.
struct SurfaceDefinition {
  std::uint32_t flags{};
  std::uint8_t tick_event{};
  std::uint8_t texture_index{};
  std::uint8_t animation_id{};
  // The six-bit field next to the flags: the sheets' transparency curve; the
  // grid takes its low three bits as the texture page's blend mode.
  std::uint8_t transparency_curve{};
  std::uint8_t ring_segments{};
  std::uint8_t ring_positions{};
  SurfaceCurves curves{};
  RingVectorRange position{};
  RingVectorRange extent{};
  Core::Math::ScalarSpan inner_extent{};
  RingVectorRange direction{};
  RingVectorRange direction_jitter{};
  RingVectorRange force_min{};
  RingVectorRange force_max{};
  RingVectorRange acceleration_min{};
  RingVectorRange acceleration_max{};
  RingVectorRange attraction_jitter{};
  std::array<Core::Math::ScalarSpan, 2> speed{};
  std::array<Core::Math::ScalarSpan, 2> mass{};
  std::array<Core::Math::ScalarSpan, 2> gravity{};
  // The grid's texture: its origin is the low bytes of pairRanges[3][0] and
  // [3][2] (definition bytes 0xAC and 0xB0), its size the mass range's
  // start halfwords (pairRanges[0][0] and [0][2]).
  std::uint8_t grid_u{};
  std::uint8_t grid_v{};
  std::int16_t grid_width{};
  std::int16_t grid_height{};
};

// What one tick reads besides the state: the definition, the effect origin
// (the action request position), the translation of the definition's
// transform and the renderer's near clip depth.
struct SurfaceTickInputs {
  SurfaceDefinition definition{};
  Core::Math::Vec3 effect_origin{};
  Core::Math::Vec3 actor_translation{};
  std::int32_t near_clip{};
};

// The force and acceleration bounds, as the halfword vectors the scratch
// holds.
struct SurfaceForces {
  Core::Math::Vec3 force_min{};
  Core::Math::Vec3 force_max{};
  Core::Math::Vec3 acceleration_min{};
  Core::Math::Vec3 acceleration_max{};

  friend bool operator==(const SurfaceForces&, const SurfaceForces&) = default;
};

// The emitter-scratch values a tick leaves behind
// (Effects_ParticleEmitterScratch); a template leaves the fields it does not
// sample unset.
struct SurfaceScratch {
  std::uint32_t flags{};
  Core::Math::Vec3 position{};
  Core::Math::Vec3 extent{};
  std::optional<SurfaceForces> forces{};
  std::optional<Core::Math::Vec3> base_direction{};
  std::optional<Core::Math::Vec3> direction_jitter{};
  std::optional<Core::Math::Vec2> speed{};
  std::optional<Core::Math::Vec2> mass{};
  std::optional<Core::Math::Vec2> gravity{};
  // The mesh's halfword attraction jitter, its rotation angles (the scratch
  // vector) and its aim transform.
  std::optional<Core::Math::Vec3> attraction_jitter{};
  std::optional<RingPoint> aim_angles{};
  std::optional<Core::Math::Transform3> aim{};
};

// The texture table and animation program a create starts.
struct SurfaceAnimation {
  std::uint8_t table{};
  std::uint8_t program{};

  friend bool operator==(const SurfaceAnimation&,
                         const SurfaceAnimation&) = default;
};

// Services every template uses. A host refuses by returning nothing;
// SurfacesLive.hpp logs every refusal.
class SurfaceHost {
 public:
  SurfaceHost() = default;
  SurfaceHost(const SurfaceHost&) = delete;
  SurfaceHost(SurfaceHost&&) = delete;
  SurfaceHost& operator=(const SurfaceHost&) = delete;
  SurfaceHost& operator=(SurfaceHost&&) = delete;
  virtual ~SurfaceHost() = default;

  // The definition a create reads; nothing refuses the create and leaves the
  // slot without state.
  [[nodiscard]] virtual std::optional<SurfaceDefinition> CreateDefinition(
      std::uint8_t definition_index) = 0;
  // The entries of the package curve at `curve`, indexed as the sheets'
  // create reads it (from 0, unlike the sampler); empty when the package
  // lacks it.
  [[nodiscard]] virtual std::span<const std::uint8_t> FactorCurve(
      std::uint8_t curve) = 0;
  // The definition and anchors of one tick, or nothing to stop the effect.
  [[nodiscard]] virtual std::optional<SurfaceTickInputs> BeginTick(
      std::uint8_t definition_index) = 0;
  [[nodiscard]] virtual std::int32_t SampleCurve(std::int32_t curve,
                                                 std::int32_t distance) = 0;
  [[nodiscard]] virtual std::int32_t Rand() = 0;
  virtual void PublishScratch(const SurfaceScratch& scratch) = 0;
  // Loads the effect transform's view (true) or the frame's view matrix.
  virtual void BeginProjection(bool effect_view) = 0;
  [[nodiscard]] virtual RingProjection Project(RingPoint point) = 0;
  // Steps the texture animation; nothing when no texture can be drawn.
  [[nodiscard]] virtual std::optional<RingTexture> AdvanceTexture() = 0;
  // Links `packet` into the scene ordering table at `depth`: its first word
  // carries the length above an empty link the host fills in.
  virtual void Submit(std::span<const std::uint32_t> packet,
                      std::int32_t depth) = 0;
  // `point` through the effect transform's world matrix (rotation and
  // translation, no projection).
  [[nodiscard]] virtual Core::Math::Vec3 TransformByActor(RingPoint point) = 0;
  virtual void EmitParticleEvent(std::uint8_t event,
                                 Core::Math::Vec3 position) = 0;
};

// A host bound to one template's persistent record.
template <typename Record>
class SurfaceRecordHost : public SurfaceHost {
 public:
  // Allocates the slot state and starts `animation` when there is one.
  virtual void Create(const Record& state,
                      std::optional<SurfaceAnimation> animation) = 0;
  [[nodiscard]] virtual std::optional<Record> State() const = 0;
  virtual void StoreState(const Record& state) = 0;
};

[[nodiscard]] constexpr RingPlan PlanSurface(std::int32_t packed) noexcept {
  return PlanRing(packed);
}

// The remainder of the R3000 DIV the templates' `%` compiles to: HI keeps
// the dividend for a zero divisor and 0 for INT32_MIN / -1.
[[nodiscard]] std::int32_t SurfaceRemainder(std::int32_t dividend,
                                            std::int32_t divisor) noexcept;

// A GPU colour word: the colour's bytes under the primitive `code`.
[[nodiscard]] constexpr std::uint32_t SurfaceColorWord(
    RingColor color, std::uint8_t code) noexcept {
  constexpr unsigned kGreenShift = 8;
  constexpr unsigned kBlueShift = 16;
  constexpr unsigned kCodeShift = 24;
  return static_cast<std::uint32_t>(color.r) |
         (static_cast<std::uint32_t>(color.g) << kGreenShift) |
         (static_cast<std::uint32_t>(color.b) << kBlueShift) |
         (static_cast<std::uint32_t>(code) << kCodeShift);
}

// A GPU texture word: the coordinates under `high` (the CLUT, the texture
// page or nothing).
[[nodiscard]] constexpr std::uint32_t SurfaceUvWord(
    RingUv uv, std::uint16_t high) noexcept {
  constexpr unsigned kVShift = 8;
  constexpr unsigned kHighShift = 16;
  return static_cast<std::uint32_t>(uv.u) |
         (static_cast<std::uint32_t>(uv.v) << kVShift) |
         (static_cast<std::uint32_t>(high) << kHighShift);
}

// `value` stored into a halfword vector (SVECTOR) and read back.
[[nodiscard]] constexpr Core::Math::Vec3 SurfaceHalfwords(
    Core::Math::Vec3 value) noexcept {
  const auto narrow = [](std::int32_t component) {
    return static_cast<std::int32_t>(
        Core::Math::WrapSigned16(static_cast<std::uint32_t>(component)));
  };
  return {narrow(value.x), narrow(value.y), narrow(value.z)};
}
// True when a primitive at `depth` is drawn: nearer than kRingFarDepth and
// beyond the near clip.
[[nodiscard]] constexpr bool SurfaceDepthVisible(
    std::int32_t depth, std::int32_t near_clip) noexcept {
  return depth < kRingFarDepth && near_clip < depth;
}

// `range` sampled at `curve` and `age`.
[[nodiscard]] Core::Math::Vec3 SampleSurfaceRange(const RingVectorRange& range,
                                                  std::uint8_t curve,
                                                  std::int32_t age,
                                                  SurfaceHost& host);

// The sampled position moved by the definition's anchor bits (1: the effect
// origin, 2: the transform's translation; other values leave it).
[[nodiscard]] Core::Math::Vec3 SampleSurfacePosition(
    const SurfaceTickInputs& inputs, std::int32_t age, SurfaceHost& host);

// Force and acceleration bounds at `age`.
[[nodiscard]] SurfaceForces SampleSurfaceForces(
    const SurfaceDefinition& definition, std::int32_t age, SurfaceHost& host);

// One colour channel from a curve id: 128 without a curve, otherwise twice
// the sample, 255 from 256 up and wrapped into a byte below.
[[nodiscard]] std::uint8_t DoubledCurveChannel(std::int32_t curve,
                                               std::int32_t age,
                                               SurfaceHost& host);

// The tick event of `definition` at `position` (in Q12 to the event; through
// the transform's world matrix with the effect view flag); nothing without
// an event.
void EmitSurfaceTickEvent(const SurfaceDefinition& definition,
                          Core::Math::Vec3 position, SurfaceHost& host);

// The expire and stop commands and any other command that is neither a
// create nor a tick: true keeps the slot.
template <typename Record>
bool ApplySurfaceLifecycle(SurfaceCommand command,
                           SurfaceRecordHost<Record>& host) {
  if (command == SurfaceCommand::kStop) {
    return false;
  }
  if (command == SurfaceCommand::kExpire) {
    std::optional<Record> state = host.State();
    if (state.has_value()) {
      state->lifetime = 1;
      host.StoreState(*state);
    }
  }
  return true;
}

// One renderer command: a create reads the definition and passes it with
// the packed plan to `create` (a refused definition leaves the slot without
// state), a tick runs `tick`, and the other commands are the lifecycle's.
// False releases the slot.
template <typename Record, typename Create, typename Tick>
bool RunSurfaceCommand(SurfaceCommand command, std::int32_t packed,
                       SurfaceRecordHost<Record>& host, Create create,
                       Tick tick) {
  if (command == SurfaceCommand::kCreate) {
    const RingPlan plan = PlanSurface(packed);
    const std::optional<SurfaceDefinition> definition =
        host.CreateDefinition(plan.definition_index);
    if (definition.has_value()) {
      create(plan, *definition);
    }
    return true;
  }
  if (command != SurfaceCommand::kTick) {
    return ApplySurfaceLifecycle(command, host);
  }
  return tick();
}

// The end of every tick: the age advances and a finite lifetime runs down;
// false releases the slot when it runs out.
template <typename Record>
bool FinishSurfaceTick(Record& state, SurfaceRecordHost<Record>& host) {
  state.age = Core::Math::WrappingAdd(state.age, 1);
  const bool finite = state.lifetime != 0;
  if (finite) {
    --state.lifetime;
  }
  host.StoreState(state);
  return !finite || state.lifetime != 0;
}

// The first samples of every tick: the scratch's flags, its position
// (SampleSurfacePosition) and its extent at `age`.
void SampleSurfacePlacement(const SurfaceTickInputs& inputs, std::int32_t age,
                            SurfaceScratch& scratch, SurfaceHost& host);

// The start of every tick: the record and the definition at its index, then
// the placement samples into a `Frame` holding `inputs`, `state` and
// `scratch`. Nothing when the host has no record or refuses the tick.
template <typename Frame, typename Record>
std::optional<Frame> OpenSurfaceTick(SurfaceRecordHost<Record>& host) {
  const std::optional<Record> state = host.State();
  if (!state.has_value()) {
    return std::nullopt;
  }
  const std::optional<SurfaceTickInputs> inputs =
      host.BeginTick(state->definition_index);
  if (!inputs.has_value()) {
    return std::nullopt;
  }
  Frame frame{.inputs = *inputs, .state = *state};
  SampleSurfacePlacement(frame.inputs, frame.state.age, frame.scratch, host);
  return frame;
}

// The persistent record of the three sheets (vs_surfaceState,
// vs_mirrorSurfaceState and vs_rotatingSurfaceState): the surface and the
// mirrored surface keep no angle, and only the mirrored surface uses all
// nine row intensities.
struct SheetState {
  std::uint8_t lifetime{};
  std::uint8_t definition_index{};
  std::int32_t radius{};
  std::int32_t radius_speed{};
  std::int32_t height{};
  std::int32_t height_speed{};
  std::int32_t angle{};
  std::int32_t age{};
  std::array<std::int32_t, kMirrorSurfaceRows> factors{};

  friend bool operator==(const SheetState&, const SheetState&) = default;
};

using SheetHost = SurfaceRecordHost<SheetState>;

// The row intensities of a sheet with `rows` rows (five or nine): 255 each
// without a transparency curve, otherwise twice the curve's entries at even
// steps over all but its last entry, an exact 256 reading 255.
[[nodiscard]] std::array<std::int32_t, kMirrorSurfaceRows> SheetFactors(
    std::span<const std::uint8_t> curve, std::size_t rows) noexcept;

// The create command of a sheet with `rows` rows.
void CreateSheet(RingPlan plan, const SurfaceDefinition& definition,
                 std::size_t rows, SheetHost& host);

// What a sheet tick computes before its rows: the advanced state, the
// scratch values and the radius and height of the first row.
struct SheetFrame {
  SurfaceTickInputs inputs{};
  SheetState state{};
  SurfaceScratch scratch{};
  std::int32_t radius{};
  std::int32_t height{};
  // Colour curve samples; a channel without a curve keeps the intensity.
  std::array<std::optional<std::int32_t>, kColorChannels> color_samples{};
};

// The first half of a sheet tick: samples the definition and advances the
// radius and height. Nothing when the host has no state or refuses the tick.
[[nodiscard]] std::optional<SheetFrame> BeginSheetTick(SheetHost& host);

// The growth of the radius and height from one row to the next.
struct SheetRowGrowth {
  std::int32_t radius_step{};
  std::int32_t radius{};
  std::int32_t height_step{};
  std::int32_t height{};
};

// Advances `growth` by the maximum force and acceleration.
void GrowSheetRow(SheetRowGrowth& growth, const SurfaceForces& forces) noexcept;

// The colour of row `row`: its intensity on each channel, scaled by
// sample / 128 on a channel with a curve.
[[nodiscard]] RingColor SheetRowColor(const SheetFrame& frame,
                                      std::size_t row) noexcept;

// Projects `points` (rows of kSurfaceColumns) and draws a quad between each
// pair of neighbouring rows at each column, the last closing the row onto
// column 0, coloured by row.
void DrawSheet(const SheetFrame& frame, std::span<const RingPoint> points,
               SurfaceTextureSpan span, SheetHost& host);

// The texture coordinates of the quad whose first corner is `corner`
// (between rows `corner.row` and `corner.row` + 1) of a sheet with `rows`
// rows.
[[nodiscard]] RingQuadUvs SheetQuadUvs(const RingTexture& texture,
                                       SurfaceTextureSpan span,
                                       std::size_t rows,
                                       RingCorner corner) noexcept;

// Publishes the scratch, then advances the age and the lifetime.
bool FinishSheetTick(SheetFrame& frame, SheetHost& host);

// An overlay's surface profile: five rows of sixteen Q12 unit points.
using SurfaceProfile = std::span<const RingPoint, kSurfaceProfilePoints>;
using SheetPoints = std::array<RingPoint, kSheetPoints>;

// The profile row a sheet row scales, and whether its height points down.
struct SheetProfileRow {
  std::size_t row{};
  bool flipped{};
};

// One sheet row per entry of `rows`: the profile row scaled by the radius
// (x and z) and the height (y) around the scratch position, both growing
// from row to row (GrowSheetRow).
[[nodiscard]] SheetPoints ScaleSheetProfile(
    const SheetFrame& frame, SurfaceProfile profile,
    std::span<const SheetProfileRow> rows);

// One renderer command of a sheet with `rows` rows: `build_points` turns a
// tick's frame into the rows of points DrawSheet projects. False releases
// the slot.
template <typename BuildPoints>
bool RunSheet(SurfaceCommand command, std::int32_t packed, std::size_t rows,
              SurfaceTextureSpan span, SheetHost& host,
              BuildPoints build_points) {
  const auto create = [rows, &host](RingPlan plan,
                                    const SurfaceDefinition& definition) {
    CreateSheet(plan, definition, rows, host);
  };
  const auto tick = [rows, span, &host, &build_points] {
    std::optional<SheetFrame> frame = BeginSheetTick(host);
    if (!frame.has_value()) {
      return false;
    }
    const SheetPoints points = build_points(*frame);
    DrawSheet(*frame,
              std::span<const RingPoint>(points).first(rows * kSurfaceColumns),
              span, host);
    return FinishSheetTick(*frame, host);
  };
  return RunSurfaceCommand(command, packed, host, create, tick);
}
}  // namespace VS::Effects::Templates
