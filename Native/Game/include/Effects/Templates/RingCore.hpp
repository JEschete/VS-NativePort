#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"

// The state, motion, geometry, colour and packet rules shared by the three
// ring effect templates (decomp src/EFFECT/rings.h, closedRings.h and
// tiledRings.h). A ring is three rows of points: its inner edge, a middle row
// at the sampled inner-extent fraction and its outer edge, swept around the
// ring's position. Every function works on value records; the effect
// package, the emitter scratch, the GTE, the texture animation and the frame
// ordering table are reached through RingHost (RingsLive.hpp binds it).
namespace VS::Effects::Templates {
inline constexpr std::size_t kRingRows = 3;
// Quads are drawn between neighbouring rows.
inline constexpr std::size_t kRingBands = kRingRows - 1;
inline constexpr std::size_t kOpenRingColumns = 9;
inline constexpr std::size_t kClosedRingColumns = 16;
inline constexpr std::size_t kRingQuadCorners = 4;
inline constexpr std::size_t kRingPacketWords = 15;
// Definition flags: bits 0-2 anchor the ring's position; this bit projects
// through the effect transform's view and places tick events through its
// world matrix.
inline constexpr std::uint32_t kRingAnchorMask = 7;
inline constexpr std::uint32_t kRingEffectView = 0x20000;
// The definition's transparency field selects one of eight palette entries.
inline constexpr std::uint8_t kRingPaletteMask = 7;
// Quads at or beyond this depth are not drawn.
inline constexpr std::int32_t kRingFarDepth = 0x800;
// GPU words of every ring quad: an E1 draw-mode word, a semi-transparent
// textured Gouraud quad (code 0x3E) and an E1 word restoring draw mode 0x200.
inline constexpr std::uint32_t kRingPacketLength = 0x0E000000;
inline constexpr std::uint32_t kRingLinkMask = 0x00FFFFFF;
inline constexpr std::uint32_t kRingDrawModeWord = 0xE1000000;
inline constexpr std::uint32_t kRingRestoreModeWord = 0xE1000200;
inline constexpr std::uint8_t kRingQuadCode = 0x3E;

// Renderer slot commands (Effects_EffectRenderer).
enum class RingCommand : std::uint32_t {
  kCreate = 1,
  kTick = 2,
  kExpire = 3,
  kStop = 4,
};

// Values of the definition's anchor bits.
enum class RingAnchor : std::uint32_t {
  kNone = 0,
  kEffectOrigin = 1,
  kActor = 2,
};

// Which definition pair starts the texture animation: the open ring uses
// (textureIndex, animationId), the closed and tiled rings (ringSegments,
// ringPositions).
enum class RingTextureSource : std::uint8_t {
  kTextureAndAnimation,
  kSegmentsAndPositions,
};

// The open ring's texture runs along the ring (upright) or across it
// (transposed: u and v swap roles and the quad corners turn).
enum class RingOrientation : std::uint8_t { kUpright, kTransposed };

// How many columns one copy of the closed ring's texture spans: four (the
// texture repeats four times around the ring) or sixteen (once).
enum class ClosedRingColumns : std::uint8_t { kFour = 4, kSixteen = 16 };

// The packed create parameter: low byte lifetime, next byte definition.
struct RingPlan {
  std::uint8_t lifetime{};
  std::uint8_t definition_index{};

  friend bool operator==(const RingPlan&, const RingPlan&) = default;
};

// The persistent ring record (vs_ringState, vs_closedRingState and
// vs_tiledRingState share it); the texture animation stays with the host.
struct RingState {
  std::uint8_t lifetime{};
  std::uint8_t definition_index{};
  std::int32_t radius{};
  std::int32_t radius_speed{};
  std::int32_t height{};
  std::int32_t height_speed{};
  std::int32_t angle{};
  std::int32_t age{};

  friend bool operator==(const RingState&, const RingState&) = default;
};

// One palette entry: the intensity of row 0, 1 and 2 (the original's
// factors[], read from an entry's first three bytes).
struct RingPaletteEntry {
  std::array<std::uint8_t, kRingRows> rows{};

  friend bool operator==(const RingPaletteEntry&,
                         const RingPaletteEntry&) = default;
};

// Three axes, as the GTE's matrices and vectors.
inline constexpr std::size_t kRingAxes = Core::Math::kMatrixOrder;
using RingVectorRange = std::array<Core::Math::ScalarSpan, kRingAxes>;

// Curve ids the tick samples at the ring's age; a zero colour curve keeps the
// palette intensity of that channel.
struct RingCurves {
  std::uint8_t position{};
  std::uint8_t extent{};
  std::uint8_t inner_extent{};
  std::uint8_t direction{};
  std::uint8_t direction_jitter{};
  std::uint8_t speed{};
  std::uint8_t force{};
  std::uint8_t acceleration{};
  std::array<std::uint8_t, kRingRows> color{};
};

// The fields of a particle definition the ring templates read.
struct RingDefinition {
  std::uint32_t flags{};
  std::uint8_t palette_index{};
  std::uint8_t tick_event{};
  RingCurves curves{};
  RingVectorRange position{};
  RingVectorRange extent{};
  Core::Math::ScalarSpan inner_extent{};
  RingVectorRange direction{};
  RingVectorRange direction_jitter{};
  RingVectorRange force_min{};
  RingVectorRange force_max{};
  RingVectorRange acceleration_min{};
  RingVectorRange acceleration_max{};
  std::array<Core::Math::ScalarSpan, 2> speed{};
};

// What one tick reads besides the state: the definition, the effect origin
// (the action request position), the translation of the definition's
// transform and the renderer's near clip depth.
struct RingTickInputs {
  RingDefinition definition{};
  Core::Math::Vec3 effect_origin{};
  Core::Math::Vec3 actor_translation{};
  std::int32_t near_clip{};
};

// The emitter-scratch values a tick leaves behind
// (Effects_ParticleEmitterScratch); force and acceleration are halfword
// vectors.
struct RingScratch {
  std::uint32_t flags{};
  Core::Math::Vec3 position{};
  Core::Math::Vec3 extent{};
  Core::Math::Vec3 force_min{};
  Core::Math::Vec3 force_max{};
  Core::Math::Vec3 acceleration_min{};
  Core::Math::Vec3 acceleration_max{};
  Core::Math::Vec3 base_direction{};
  Core::Math::Vec2 speed{};
  // Only the open ring samples a jitter.
  std::optional<Core::Math::Vec3> direction_jitter{};

  friend bool operator==(const RingScratch&, const RingScratch&) = default;
};

// A ring point as the halfword vector the GTE projects.
struct RingPoint {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};

  friend bool operator==(const RingPoint&, const RingPoint&) = default;
};

// A projected point: the packed screen word (SXY2) and its OTZ depth.
struct RingProjection {
  std::uint32_t screen{};
  std::int32_t depth{};

  friend bool operator==(const RingProjection&,
                         const RingProjection&) = default;
};

// The package texture the animation currently shows.
struct RingTexture {
  std::uint16_t clut{};
  std::uint16_t tpage{};
  std::uint8_t u{};
  std::uint8_t v{};
  std::uint8_t w{};
  std::uint8_t h{};
};

struct RingColor {
  std::uint8_t r{};
  std::uint8_t g{};
  std::uint8_t b{};

  friend bool operator==(const RingColor&, const RingColor&) = default;
};

struct RingUv {
  std::uint8_t u{};
  std::uint8_t v{};

  friend bool operator==(const RingUv&, const RingUv&) = default;
};

using RingQuadUvs = std::array<RingUv, kRingQuadCorners>;

// A point of the ring grid by row and column.
struct RingCorner {
  std::size_t row{};
  std::size_t column{};

  friend bool operator==(const RingCorner&, const RingCorner&) = default;
};

using RingQuadCorners = std::array<RingCorner, kRingQuadCorners>;

// One textured quad in GPU corner order.
struct RingQuad {
  std::array<std::uint32_t, kRingQuadCorners> screens{};
  std::array<RingColor, kRingQuadCorners> colors{};
  RingQuadUvs uvs{};
  std::uint16_t clut{};
  std::uint16_t tpage{};

  friend bool operator==(const RingQuad&, const RingQuad&) = default;
};

// The projected grid of one ring: `columns` points per row.
struct RingProjectedGrid {
  std::span<const RingProjection> points;
  std::size_t columns{};
};

// Everything one tick computes before the template draws: the advanced
// state, the scratch values, the inner-extent fraction and the radius,
// height and colour of each row.
struct RingFrame {
  RingTickInputs inputs{};
  RingState state{};
  RingScratch scratch{};
  std::int32_t fraction{};
  std::array<std::int32_t, kRingRows> radius{};
  std::array<std::int32_t, kRingRows> height{};
  std::array<RingColor, kRingRows> colors{};
};

// Services the ring templates use. A host refuses by returning nothing;
// RingsLive.hpp logs every refusal.
class RingHost {
 public:
  RingHost() = default;
  RingHost(const RingHost&) = delete;
  RingHost(RingHost&&) = delete;
  RingHost& operator=(const RingHost&) = delete;
  RingHost& operator=(RingHost&&) = delete;
  virtual ~RingHost() = default;

  // Allocates the slot state and starts its texture animation from the
  // definition pair `source` names.
  virtual void Create(const RingState& state, RingTextureSource source) = 0;
  [[nodiscard]] virtual std::optional<RingState> State() const = 0;
  virtual void StoreState(const RingState& state) = 0;
  // The definition and anchors of one tick, or nothing to stop the ring.
  [[nodiscard]] virtual std::optional<RingTickInputs> BeginTick(
      std::uint8_t definition_index) = 0;
  [[nodiscard]] virtual std::int32_t SampleCurve(std::uint8_t curve,
                                                 std::int32_t distance) = 0;
  [[nodiscard]] virtual std::int32_t Rand() = 0;
  virtual void PublishScratch(const RingScratch& scratch) = 0;
  // Loads the effect transform's view (true) or the frame's view matrix.
  virtual void BeginProjection(bool effect_view) = 0;
  [[nodiscard]] virtual RingProjection Project(RingPoint point) = 0;
  // Steps the texture animation; nothing when no texture can be drawn.
  [[nodiscard]] virtual std::optional<RingTexture> AdvanceTexture() = 0;
  virtual void Submit(const RingQuad& quad, std::int32_t depth) = 0;
  // `point` through the effect transform's world matrix (rotation and
  // translation, no projection).
  [[nodiscard]] virtual Core::Math::Vec3 TransformByActor(RingPoint point) = 0;
  virtual void EmitParticleEvent(std::uint8_t event,
                                 Core::Math::Vec3 position) = 0;
};

// (value * factor) >> 12 over the low word of the product, as the original's
// mult/mflo/sra.
[[nodiscard]] constexpr std::int32_t RingFixed(std::int32_t value,
                                               std::int32_t factor) noexcept {
  constexpr unsigned kFixedShift = 12;
  return Core::Math::WrappingMultiply(value, factor) >> kFixedShift;
}

// `value` stored into the halfword vector the GTE loads.
[[nodiscard]] constexpr RingPoint RingPointAt(Core::Math::Vec3 value) noexcept {
  return {Core::Math::WrapSigned16(static_cast<std::uint32_t>(value.x)),
          Core::Math::WrapSigned16(static_cast<std::uint32_t>(value.y)),
          Core::Math::WrapSigned16(static_cast<std::uint32_t>(value.z))};
}

[[nodiscard]] constexpr RingPlan PlanRing(std::int32_t packed) noexcept {
  constexpr unsigned kByteBits = 8;
  const auto bits = static_cast<std::uint32_t>(packed);
  return {.lifetime = static_cast<std::uint8_t>(bits),
          .definition_index = static_cast<std::uint8_t>(bits >> kByteBits)};
}

// The create, expire and stop commands, and the result of any other command
// that is not a tick: true keeps the slot.
bool ApplyRingCommand(RingCommand command, std::int32_t packed,
                      RingTextureSource source, RingHost& host);

// The row intensities of palette entry `index` (its low three bits); an
// entry outside the palette reads as zero.
[[nodiscard]] RingPaletteEntry RingPaletteAt(
    std::span<const RingPaletteEntry> palette, std::uint8_t index) noexcept;

// `range` sampled at `curve` and the ring's age.
[[nodiscard]] Core::Math::Vec3 SampleRingRange(const RingVectorRange& range,
                                               std::uint8_t curve,
                                               std::int32_t age,
                                               RingHost& host);

// The first half of a tick: samples the definition, advances radius, height
// and angle and colours the rows. Nothing when the host has no state or
// refuses the tick.
[[nodiscard]] std::optional<RingFrame> BeginRingTick(
    std::span<const RingPaletteEntry> palette, RingHost& host);

// (origin, origin + size * fraction, origin + size): the texture coordinate
// of each row, wrapping in a byte.
[[nodiscard]] std::array<std::uint8_t, kRingRows> RingRowCoordinates(
    std::uint8_t origin, std::uint8_t size, std::int32_t fraction) noexcept;

// origin + size * step / steps, wrapping in a byte.
[[nodiscard]] std::uint8_t RingTextureStep(std::uint8_t origin,
                                           std::uint8_t size, std::size_t step,
                                           std::size_t steps) noexcept;

// The sixteen points of each row of a closed or tiled ring: four quarter
// turns, each mirrored into the other three quadrants.
[[nodiscard]] std::array<RingPoint, kRingRows * kClosedRingColumns>
ClosedRingPoints(const RingFrame& frame);

// Projects `points` row by row into `projections` through the view the
// definition selects.
void ProjectRingPoints(const RingFrame& frame,
                       std::span<const RingPoint> points,
                       std::span<RingProjection> projections, RingHost& host);

// The corners of the closed ring's quad at `column` of `band`; the last
// column closes the ring onto column 0.
[[nodiscard]] RingQuadCorners ClosedRingCorners(std::size_t band,
                                                std::size_t column) noexcept;

// Submits the quad on `corners` when its average depth is between the near
// clip and kRingFarDepth; each corner takes its row's colour.
void SubmitRingQuad(const RingFrame& frame, RingProjectedGrid grid,
                    const RingQuadCorners& corners, const RingQuadUvs& uvs,
                    const RingTexture& texture, RingHost& host);

// The texture coordinates of the quad at `column` of `band` of a
// sixteen-column ring when one copy of the texture spans `texture_columns`
// columns (a power of two up to sixteen): u steps through 1/`texture_columns`
// of the texture per column and v runs across the ring (inner edge,
// inner-extent fraction, outer edge). A one-column span stretches the whole
// texture over every quad, as the tiled ring does.
[[nodiscard]] RingQuadUvs SixteenColumnRingUvs(std::size_t texture_columns,
                                               const RingTexture& texture,
                                               std::int32_t fraction,
                                               std::size_t band,
                                               std::size_t column) noexcept;

// The drawing shared by the closed and tiled rings: projects
// ClosedRingPoints, steps the texture animation and, when the host has a
// texture, submits two bands of sixteen quads, the last of each band closing
// the ring onto column 0.
void DrawSixteenColumnRing(const RingFrame& frame, std::size_t texture_columns,
                           RingHost& host);

// The second half of a tick: the tick event, the age and the lifetime.
// Stores the state; false when the lifetime ran out.
bool FinishRingTick(RingFrame& frame, RingHost& host);

// The fifteen GPU words of `quad`, linked to the ordering entry `link`.
[[nodiscard]] std::array<std::uint32_t, kRingPacketWords> EncodeRingPacket(
    const RingQuad& quad, std::uint32_t link) noexcept;
}  // namespace VS::Effects::Templates
