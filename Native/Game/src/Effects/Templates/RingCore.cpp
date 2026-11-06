// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/RingCore.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"

// The tick shared by vs_ring (src/EFFECT/rings.h:26), vs_closedRing
// (src/EFFECT/closedRings.h:41) and vs_tiledRing (src/EFFECT/tiledRings.h:35).
// All three compile to the same sampling, motion, colour, depth-test, packet
// and lifetime code; only the points, texture coordinates and quad corners
// differ (Ring.cpp, ClosedRing.cpp and TiledRing.cpp).
namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec3;

constexpr unsigned kByteBits = 8;
constexpr unsigned kHalfwordBits = 16;
constexpr unsigned kCodeShift = 24;
// A colour curve scales the palette intensity by sample / 128.
constexpr std::int32_t kColorCurveUnit = 128;
// Tick events take the ring position in Q12 (ONE).
constexpr std::int32_t kEventPositionScale = 4096;
// The closed rings draw four quadrants of four columns, 1/16 turn apart.
constexpr std::size_t kQuadrants = 4;
constexpr std::size_t kQuadrantColumns = kClosedRingColumns / kQuadrants;
constexpr std::int32_t kClosedColumnTurn = 256;
// Packet word positions: the link tag, the draw-mode word, three words per
// corner (colour, screen, texture) and the restoring draw-mode word.
constexpr std::size_t kModeWord = 1;
constexpr std::size_t kFirstCornerWord = 2;
constexpr std::size_t kCornerWords = 3;
constexpr std::size_t kRestoreWord = kRingPacketWords - 1;

// The remainder the R3000 DIV instruction leaves in HI: a zero divisor leaves
// the dividend and INT32_MIN % -1 gives 0.
[[nodiscard]] constexpr std::int32_t R3000Remainder(
    std::int32_t dividend, std::int32_t divisor) noexcept {
  if (divisor == 0) {
    return dividend;
  }
  if (divisor == -1) {
    return 0;
  }
  return dividend % divisor;
}

[[nodiscard]] constexpr Vec3 Add(Vec3 left, Vec3 right) noexcept {
  return {math::WrappingAdd(left.x, right.x),
          math::WrappingAdd(left.y, right.y),
          math::WrappingAdd(left.z, right.z)};
}

[[nodiscard]] constexpr Vec3 Halfwords(Vec3 value) noexcept {
  return {math::WrapSigned16(static_cast<std::uint32_t>(value.x)),
          math::WrapSigned16(static_cast<std::uint32_t>(value.y)),
          math::WrapSigned16(static_cast<std::uint32_t>(value.z))};
}

// The sampled position moved by the definition's anchor; anchors 3-7 leave
// it as sampled, as the original's switch did.
[[nodiscard]] Vec3 AnchorPosition(Vec3 sampled, const RingTickInputs& inputs) {
  const auto anchor =
      static_cast<RingAnchor>(inputs.definition.flags & kRingAnchorMask);
  if (anchor == RingAnchor::kEffectOrigin) {
    return Add(sampled, inputs.effect_origin);
  }
  if (anchor == RingAnchor::kActor) {
    return Add(sampled, inputs.actor_translation);
  }
  return sampled;
}

// Position, extent, force, acceleration, direction and speed at the ring's
// age, in the original's order. Force and acceleration land in halfword
// vectors.
void SampleMotion(RingFrame& frame, RingHost& host) {
  const RingDefinition& definition = frame.inputs.definition;
  const RingCurves& curves = definition.curves;
  const std::int32_t age = frame.state.age;
  RingScratch& scratch = frame.scratch;
  scratch.position = AnchorPosition(
      SampleRingRange(definition.position, curves.position, age, host),
      frame.inputs);
  scratch.extent = SampleRingRange(definition.extent, curves.extent, age, host);
  const std::int32_t force = host.SampleCurve(curves.force, age);
  scratch.force_min =
      Halfwords(math::InterpolateSevenBitVector(definition.force_min, force));
  scratch.force_max =
      Halfwords(math::InterpolateSevenBitVector(definition.force_max, force));
  const std::int32_t acceleration = host.SampleCurve(curves.acceleration, age);
  scratch.acceleration_min = Halfwords(math::InterpolateSevenBitVector(
      definition.acceleration_min, acceleration));
  scratch.acceleration_max = Halfwords(math::InterpolateSevenBitVector(
      definition.acceleration_max, acceleration));
  scratch.base_direction =
      SampleRingRange(definition.direction, curves.direction, age, host);
  scratch.speed = math::InterpolateSevenBitPlane(
      definition.speed, host.SampleCurve(curves.speed, age));
}

// Radius and height grow by their speeds, which grow by the minimum
// acceleration; the rows tilt by the base direction's x angle.
void AdvanceRows(RingFrame& frame) {
  RingState& state = frame.state;
  const RingScratch& scratch = frame.scratch;
  state.radius_speed =
      math::WrappingAdd(state.radius_speed, scratch.acceleration_min.x);
  state.radius = math::WrappingAdd(
      state.radius, math::WrappingAdd(scratch.force_min.x, state.radius_speed));
  const std::int32_t inner = math::WrappingAdd(state.radius, scratch.extent.x);
  state.height_speed =
      math::WrappingAdd(state.height_speed, scratch.acceleration_min.y);
  state.height = math::WrappingAdd(
      state.height, math::WrappingAdd(scratch.force_min.y, state.height_speed));
  const std::int32_t outer = math::WrappingAdd(state.height, scratch.extent.y);
  const std::int32_t sine = math::Sine(scratch.base_direction.x);
  const std::int32_t cosine = math::Cosine(scratch.base_direction.x);
  const std::int32_t middle = RingFixed(outer, frame.fraction);
  frame.radius = {inner, math::WrappingAdd(RingFixed(middle, cosine), inner),
                  math::WrappingAdd(RingFixed(outer, cosine), inner)};
  frame.height = {0, RingFixed(middle, sine), RingFixed(outer, sine)};
}

// The speed range's single value, or a random step between its bounds.
[[nodiscard]] std::int32_t AngleStep(math::Vec2 speed, RingHost& host) {
  if (speed.x == speed.y) {
    return speed.x;
  }
  const std::int32_t low = std::min(speed.x, speed.y);
  const std::int32_t high = std::max(speed.x, speed.y);
  const std::int32_t choices =
      math::WrappingAdd(math::WrappingSubtract(high, low), 1);
  return math::WrappingAdd(R3000Remainder(host.Rand(), choices), low);
}

[[nodiscard]] std::uint8_t Channel(std::uint8_t factor, std::uint8_t curve,
                                   std::int32_t age, RingHost& host) {
  if (curve == 0) {
    return factor;
  }
  return static_cast<std::uint8_t>(
      math::WrappingMultiply(factor, host.SampleCurve(curve, age)) /
      kColorCurveUnit);
}

// Each row takes its palette intensity on all three channels; a channel with
// a curve scales it by the curve's sample.
void ColorRows(RingFrame& frame, const RingPaletteEntry& factors,
               RingHost& host) {
  const std::array<std::uint8_t, kRingRows>& curves =
      frame.inputs.definition.curves.color;
  const std::int32_t age = frame.state.age;
  for (std::size_t row = 0; row < kRingRows; ++row) {
    const std::uint8_t factor = factors.rows.at(row);
    frame.colors.at(row) = {.r = Channel(factor, curves[0], age, host),
                            .g = Channel(factor, curves[1], age, host),
                            .b = Channel(factor, curves[2], age, host)};
  }
}

[[nodiscard]] constexpr std::uint32_t ColorWord(RingColor color) noexcept {
  return color.r | (static_cast<std::uint32_t>(color.g) << kByteBits) |
         (static_cast<std::uint32_t>(color.b) << kHalfwordBits) |
         (static_cast<std::uint32_t>(kRingQuadCode) << kCodeShift);
}
}  // namespace

bool ApplyRingCommand(RingCommand command, std::int32_t packed,
                      RingTextureSource source, RingHost& host) {
  if (command == RingCommand::kCreate) {
    const RingPlan plan = PlanRing(packed);
    host.Create(
        {.lifetime = plan.lifetime, .definition_index = plan.definition_index},
        source);
    return true;
  }
  if (command == RingCommand::kExpire) {
    if (std::optional<RingState> state = host.State()) {
      state->lifetime = 1;
      host.StoreState(*state);
    }
    return true;
  }
  return command != RingCommand::kStop;
}

RingPaletteEntry RingPaletteAt(std::span<const RingPaletteEntry> palette,
                               std::uint8_t index) noexcept {
  const std::size_t entry = index & kRingPaletteMask;
  return entry < palette.size() ? palette[entry] : RingPaletteEntry{};
}

Vec3 SampleRingRange(const RingVectorRange& range, std::uint8_t curve,
                     std::int32_t age, RingHost& host) {
  return math::InterpolateSevenBitVector(range, host.SampleCurve(curve, age));
}

std::optional<RingFrame> BeginRingTick(
    std::span<const RingPaletteEntry> palette, RingHost& host) {
  const std::optional<RingState> state = host.State();
  if (!state) {
    return std::nullopt;
  }
  const std::optional<RingTickInputs> inputs =
      host.BeginTick(state->definition_index);
  if (!inputs) {
    return std::nullopt;
  }
  RingFrame frame{.inputs = *inputs, .state = *state};
  const RingDefinition& definition = frame.inputs.definition;
  frame.scratch.flags = definition.flags;
  frame.fraction = math::InterpolateSevenBitScalar(
      definition.inner_extent,
      host.SampleCurve(definition.curves.inner_extent, frame.state.age));
  const RingPaletteEntry factors =
      RingPaletteAt(palette, definition.palette_index);
  SampleMotion(frame, host);
  AdvanceRows(frame);
  frame.state.angle = math::WrappingAdd(frame.state.angle,
                                        AngleStep(frame.scratch.speed, host));
  ColorRows(frame, factors, host);
  return frame;
}

std::array<std::uint8_t, kRingRows> RingRowCoordinates(
    std::uint8_t origin, std::uint8_t size, std::int32_t fraction) noexcept {
  return {origin, static_cast<std::uint8_t>(origin + RingFixed(size, fraction)),
          static_cast<std::uint8_t>(origin + size)};
}

std::uint8_t RingTextureStep(std::uint8_t origin, std::uint8_t size,
                             std::size_t step, std::size_t steps) noexcept {
  if (steps == 0) {
    return origin;
  }
  return static_cast<std::uint8_t>(origin + ((size * step) / steps));
}

std::array<RingPoint, kRingRows * kClosedRingColumns> ClosedRingPoints(
    const RingFrame& frame) {
  std::array<RingPoint, kRingRows * kClosedRingColumns> points{};
  const Vec3 origin = frame.scratch.position;
  for (std::size_t column = 0; column < kQuadrantColumns; ++column) {
    const std::int32_t angle =
        math::WrappingAdd(frame.state.angle, static_cast<std::int32_t>(column) *
                                                 kClosedColumnTurn);
    const std::int32_t cosine = math::Cosine(angle);
    const std::int32_t sine = math::Sine(angle);
    for (std::size_t row = 0; row < kRingRows; ++row) {
      const std::int32_t along = RingFixed(frame.radius.at(row), cosine);
      const std::int32_t across = RingFixed(frame.radius.at(row), sine);
      const std::int32_t height = frame.height.at(row);
      // Each quadrant turns the previous one a quarter turn: (x, z) becomes
      // (-z, x).
      const std::array<Vec3, kQuadrants> quadrants = {
          Vec3{along, height, across},
          Vec3{math::WrappingNegate(across), height, along},
          Vec3{math::WrappingNegate(along), height,
               math::WrappingNegate(across)},
          Vec3{across, height, math::WrappingNegate(along)}};
      for (std::size_t quadrant = 0; quadrant < kQuadrants; ++quadrant) {
        points.at((row * kClosedRingColumns) + (quadrant * kQuadrantColumns) +
                  column) = RingPointAt(Add(quadrants.at(quadrant), origin));
      }
    }
  }
  return points;
}

void ProjectRingPoints(const RingFrame& frame,
                       std::span<const RingPoint> points,
                       std::span<RingProjection> projections, RingHost& host) {
  host.BeginProjection((frame.scratch.flags & kRingEffectView) != 0);
  const std::size_t count = std::min(points.size(), projections.size());
  for (std::size_t index = 0; index < count; ++index) {
    projections[index] = host.Project(points[index]);
  }
}

RingQuadCorners ClosedRingCorners(std::size_t band,
                                  std::size_t column) noexcept {
  const std::size_t next = (column + 1) % kClosedRingColumns;
  return {RingCorner{band, column}, RingCorner{band, next},
          RingCorner{band + 1, column}, RingCorner{band + 1, next}};
}

void SubmitRingQuad(const RingFrame& frame, RingProjectedGrid grid,
                    const RingQuadCorners& corners, const RingQuadUvs& uvs,
                    const RingTexture& texture, RingHost& host) {
  RingQuad quad{.uvs = uvs, .clut = texture.clut, .tpage = texture.tpage};
  std::int32_t depth_sum = 0;
  for (std::size_t corner = 0; corner < kRingQuadCorners; ++corner) {
    const RingCorner& at = corners.at(corner);
    const std::size_t index = (at.row * grid.columns) + at.column;
    if (at.row >= kRingRows || at.column >= grid.columns ||
        index >= grid.points.size()) {
      return;
    }
    quad.screens.at(corner) = grid.points[index].screen;
    quad.colors.at(corner) = frame.colors.at(at.row);
    depth_sum = math::WrappingAdd(depth_sum, grid.points[index].depth);
  }
  const std::int32_t depth = math::R3000Quotient(
      depth_sum, static_cast<std::int32_t>(kRingQuadCorners));
  if (depth >= kRingFarDepth || depth <= frame.inputs.near_clip) {
    return;
  }
  host.Submit(quad, depth);
}

RingQuadUvs SixteenColumnRingUvs(std::size_t texture_columns,
                                 const RingTexture& texture,
                                 std::int32_t fraction, std::size_t band,
                                 std::size_t column) noexcept {
  if (band >= kRingBands || texture_columns == 0) {
    return {};
  }
  // decomp: closedRings.h VS_CLOSED_RING_TEXTURE_COLUMN, column & (N - 1).
  const std::size_t tile = column & (texture_columns - 1);
  const std::uint8_t left =
      RingTextureStep(texture.u, texture.w, tile, texture_columns);
  const std::uint8_t right =
      RingTextureStep(texture.u, texture.w, tile + 1, texture_columns);
  const std::array<std::uint8_t, kRingRows> across =
      RingRowCoordinates(texture.v, texture.h, fraction);
  const std::uint8_t top = across[band];
  const std::uint8_t bottom = across[band + 1];
  return {RingUv{.u = left, .v = top}, RingUv{.u = right, .v = top},
          RingUv{.u = left, .v = bottom}, RingUv{.u = right, .v = bottom}};
}

void DrawSixteenColumnRing(const RingFrame& frame, std::size_t texture_columns,
                           RingHost& host) {
  const std::array<RingPoint, kRingRows * kClosedRingColumns> points =
      ClosedRingPoints(frame);
  std::array<RingProjection, kRingRows * kClosedRingColumns> projections{};
  ProjectRingPoints(frame, points, projections, host);
  const std::optional<RingTexture> texture = host.AdvanceTexture();
  if (!texture) {
    return;
  }
  const RingProjectedGrid grid{.points = projections,
                               .columns = kClosedRingColumns};
  for (std::size_t band = 0; band < kRingBands; ++band) {
    for (std::size_t column = 0; column < kClosedRingColumns; ++column) {
      SubmitRingQuad(frame, grid, ClosedRingCorners(band, column),
                     SixteenColumnRingUvs(texture_columns, *texture,
                                          frame.fraction, band, column),
                     *texture, host);
    }
  }
}

bool FinishRingTick(RingFrame& frame, RingHost& host) {
  host.PublishScratch(frame.scratch);
  const RingDefinition& definition = frame.inputs.definition;
  if (definition.tick_event != 0) {
    // With the effect view the event lands at the position through the
    // effect transform's world matrix.
    const Vec3 position =
        (definition.flags & kRingEffectView) != 0
            ? host.TransformByActor(RingPointAt(frame.scratch.position))
            : frame.scratch.position;
    host.EmitParticleEvent(
        static_cast<std::uint8_t>(definition.tick_event - 1),
        {math::WrappingMultiply(position.x, kEventPositionScale),
         math::WrappingMultiply(position.y, kEventPositionScale),
         math::WrappingMultiply(position.z, kEventPositionScale)});
  }
  bool alive = true;
  frame.state.age = math::WrappingAdd(frame.state.age, 1);
  if (frame.state.lifetime != 0) {
    --frame.state.lifetime;
    alive = frame.state.lifetime != 0;
  }
  host.StoreState(frame.state);
  return alive;
}

std::array<std::uint32_t, kRingPacketWords> EncodeRingPacket(
    const RingQuad& quad, std::uint32_t link) noexcept {
  std::array<std::uint32_t, kRingPacketWords> words{};
  words[0] = (link & kRingLinkMask) | kRingPacketLength;
  words[kModeWord] = kRingDrawModeWord;
  // The first two texture words carry the CLUT and the texture page above
  // their coordinates.
  const std::array<std::uint32_t, kRingQuadCorners> texture_high = {
      quad.clut, quad.tpage, 0, 0};
  for (std::size_t corner = 0; corner < kRingQuadCorners; ++corner) {
    const std::size_t at = kFirstCornerWord + (corner * kCornerWords);
    const RingUv uv = quad.uvs.at(corner);
    words.at(at) = ColorWord(quad.colors.at(corner));
    words.at(at + 1) = quad.screens.at(corner);
    words.at(at + 2) = uv.u | (static_cast<std::uint32_t>(uv.v) << kByteBits) |
                       (texture_high.at(corner) << kHalfwordBits);
  }
  words[kRestoreWord] = kRingRestoreModeWord;
  return words;
}
}  // namespace VS::Effects::Templates
