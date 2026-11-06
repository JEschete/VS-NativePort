// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/Grid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/SurfaceCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec3;

constexpr std::size_t kGridCentre = kGridLines / 2;
constexpr std::size_t kGridSpans = kGridLines - 1;
constexpr std::int32_t kGridSpanDivisor = static_cast<std::int32_t>(kGridSpans);
constexpr std::int32_t kGridQuadDivisor = 4;
// The texture page's blend mode bits and the base page and CLUT words
// (decomp grids.h:68-70).
constexpr unsigned kBlendShift = 5;
constexpr std::uint8_t kLowBits = 7;
constexpr std::uint16_t kGridPageBase = 0x18;
constexpr unsigned kClutRowShift = 6;
constexpr std::uint16_t kGridClutBase = 0x3C30;
// GridDepthMode slots: the farthest scene slot, three slots in front of the
// scene, and the step a nudged quad moves nearer.
constexpr std::int32_t kFarthestSlot = 0x7FE;
constexpr std::int32_t kFront15Slot = -15;
constexpr std::int32_t kFront13Slot = -13;
constexpr std::int32_t kFront11Slot = -11;
constexpr std::int32_t kNudge = 16;
// POLY_FT4 below the packet's link word: the length, the flat colour code
// and where each corner's two words start.
constexpr std::uint32_t kGridPacketLength = 0x0B000000;
constexpr std::uint8_t kGridQuadCode = 0x2E;
constexpr std::size_t kColorWord = 2;
constexpr std::size_t kFirstCornerWord = 3;
constexpr std::size_t kGridCornerWords = 2;
constexpr std::size_t kGridRestoreWord = 11;

[[nodiscard]] constexpr std::int16_t Halfword(std::int32_t value) noexcept {
  return math::WrapSigned16(static_cast<std::uint32_t>(value));
}

// What a grid tick computes before its points: the advanced state, the
// scratch values, the depth mode and the first line spacing and phase of
// each axis.
struct GridFrame {
  SurfaceTickInputs inputs{};
  GridState state{};
  SurfaceScratch scratch{};
  std::int32_t depth_mode{};
  std::int32_t x_spacing{};
  std::int32_t z_spacing{};
};

// decomp: grids.h:113-120 (the spacing accumulators and the phases).
void AdvanceGridMotion(GridFrame& frame, const SurfaceForces& forces,
                       Vec3 jitter) noexcept {
  GridState& state = frame.state;
  const Vec3 extent = frame.scratch.extent;
  state.x_speed = Halfword(state.x_speed + forces.acceleration_min.x);
  state.x_spacing =
      Halfword(state.x_spacing + forces.force_min.x + state.x_speed);
  frame.x_spacing = math::WrappingAdd(extent.x, state.x_spacing);
  state.z_speed = Halfword(state.z_speed + forces.acceleration_min.y);
  state.z_spacing =
      Halfword(state.z_spacing + forces.force_min.y + state.z_speed);
  frame.z_spacing = math::WrappingAdd(extent.y, state.z_spacing);
  state.x_phase = Halfword(math::WrappingAdd(state.x_phase, jitter.x));
  state.z_phase = Halfword(math::WrappingAdd(state.z_phase, jitter.y));
}

// decomp: grids.h:75-112 (the definition sampled at the grid's age).
std::optional<GridFrame> BeginGridTick(GridHost& host) {
  std::optional<GridFrame> opened = OpenSurfaceTick<GridFrame>(host);
  if (!opened.has_value()) {
    return std::nullopt;
  }
  GridFrame& frame = *opened;
  const SurfaceDefinition& definition = frame.inputs.definition;
  const SurfaceCurves& curves = definition.curves;
  const std::int32_t age = frame.state.age;
  SurfaceScratch& scratch = frame.scratch;
  const SurfaceForces forces = SampleSurfaceForces(definition, age, host);
  scratch.forces = forces;
  scratch.base_direction =
      SampleSurfaceRange(definition.direction, curves.direction, age, host);
  const Vec3 jitter = SampleSurfaceRange(definition.direction_jitter,
                                         curves.direction_jitter, age, host);
  scratch.direction_jitter = jitter;
  scratch.speed = math::InterpolateSevenBitPlane(
      definition.speed, host.SampleCurve(curves.speed, age));
  scratch.mass = math::InterpolateSevenBitPlane(
      definition.mass, host.SampleCurve(curves.mass, age));
  scratch.gravity = math::InterpolateSevenBitPlane(
      definition.gravity, host.SampleCurve(curves.gravity, age));
  frame.depth_mode = math::InterpolateSevenBitScalar(
      definition.inner_extent, host.SampleCurve(curves.inner_extent, age));
  AdvanceGridMotion(frame, forces, jitter);
  return opened;
}

// decomp: grids.h:121-136: the nine lines of one axis, mirrored about
// `origin`, the gap growing by `force` plus an accumulating `acceleration`.
std::array<std::int32_t, kGridLines> GridLines(
    std::int32_t origin, std::int32_t spacing, std::int32_t force,
    std::int32_t acceleration) noexcept {
  std::array<std::int32_t, kGridLines> lines{};
  std::int32_t offset = 0;
  std::int32_t growth = 0;
  for (std::size_t step = 0; step <= kGridCentre; ++step) {
    lines.at(kGridCentre - step) = math::WrappingSubtract(origin, offset);
    lines.at(kGridCentre + step) = math::WrappingAdd(offset, origin);
    offset = math::WrappingAdd(offset, spacing);
    spacing = math::WrappingAdd(spacing, math::WrappingAdd(growth, force));
    growth = math::WrappingAdd(growth, acceleration);
  }
  return lines;
}

// decomp: grids.h:137-152: every point sways along x by the sine of its
// row's x phase and along z by the cosine of its z phase.
std::array<RingPoint, kGridPoints> GridPoints(const GridFrame& frame) {
  const SurfaceScratch& scratch = frame.scratch;
  const SurfaceForces forces = scratch.forces.value_or(SurfaceForces{});
  const Vec3 sway = scratch.base_direction.value_or(Vec3{});
  const Vec3 jitter = scratch.direction_jitter.value_or(Vec3{});
  const math::Vec2 speed = scratch.speed.value_or(math::Vec2{});
  const std::array<std::int32_t, kGridLines> columns =
      GridLines(scratch.position.x, frame.x_spacing, forces.force_max.x,
                forces.acceleration_max.x);
  const std::array<std::int32_t, kGridLines> rows =
      GridLines(scratch.position.z, frame.z_spacing, forces.force_max.y,
                forces.acceleration_max.y);
  const std::int16_t height = Halfword(scratch.position.y);
  std::int32_t row_x_phase = frame.state.x_phase;
  std::int32_t row_z_phase = frame.state.z_phase;
  std::array<RingPoint, kGridPoints> points{};
  for (std::size_t row = 0; row < kGridLines; ++row) {
    std::int32_t x_angle = row_x_phase;
    std::int32_t z_angle = row_z_phase;
    row_x_phase = math::WrappingAdd(row_x_phase, speed.x);
    row_z_phase = math::WrappingAdd(row_z_phase, speed.y);
    for (std::size_t column = 0; column < kGridLines; ++column) {
      const std::int32_t x = math::WrappingAdd(
          columns.at(column), RingFixed(math::Sine(x_angle), sway.x));
      const std::int32_t z = math::WrappingAdd(
          rows.at(row), RingFixed(math::Cosine(z_angle), sway.y));
      points.at((row * kGridLines) + column) = {Halfword(x), height,
                                                Halfword(z)};
      x_angle = math::WrappingAdd(x_angle, jitter.x);
      z_angle = math::WrappingAdd(z_angle, jitter.y);
    }
  }
  return points;
}

// decomp: grids.h:183-194: the nine texture coordinates of one axis from
// the scroll before this tick; the scroll advances by `step` modulo `size`.
std::array<std::uint8_t, kGridLines> GridCoordinates(
    std::uint8_t origin, std::uint8_t& scroll, std::int32_t step,
    std::int32_t size) noexcept {
  const std::int32_t previous = scroll;
  scroll = static_cast<std::uint8_t>(
      SurfaceRemainder(math::WrappingAdd(previous, step), size));
  const std::int32_t start = origin + previous;
  std::array<std::uint8_t, kGridLines> coordinates{};
  for (std::size_t line = 0; line < kGridLines; ++line) {
    const auto fraction =
        (size * static_cast<std::int32_t>(line)) / kGridSpanDivisor;
    coordinates.at(line) = static_cast<std::uint8_t>(start + fraction);
  }
  return coordinates;
}

// decomp: grids.h:153-260: the colour, the texture coordinates, projection and
// the eight by eight quads.
void DrawGrid(GridFrame& frame,
              const std::array<RingPoint, kGridPoints>& points,
              GridHost& host) {
  const SurfaceDefinition& definition = frame.inputs.definition;
  const std::int32_t age = frame.state.age;
  const RingColor color{
      .r = DoubledCurveChannel(definition.curves.color[0], age, host),
      .g = DoubledCurveChannel(definition.curves.color[1], age, host),
      .b = DoubledCurveChannel(definition.curves.color[2], age, host)};
  const math::Vec2 scroll = frame.scratch.gravity.value_or(math::Vec2{});
  GridState& state = frame.state;
  const std::array<std::uint8_t, kGridLines> u =
      GridCoordinates(state.u, state.u_scroll, scroll.x, definition.grid_width);
  const std::array<std::uint8_t, kGridLines> v = GridCoordinates(
      state.v, state.v_scroll, scroll.y, definition.grid_height);
  host.BeginProjection((definition.flags & kRingEffectView) != 0);
  std::array<RingProjection, kGridPoints> projected{};
  for (std::size_t point = 0; point < kGridPoints; ++point) {
    projected.at(point) = host.Project(points.at(point));
  }
  for (std::size_t row = 0; row < kGridSpans; ++row) {
    for (std::size_t column = 0; column < kGridSpans; ++column) {
      const std::size_t first = (row * kGridLines) + column;
      const std::array<std::size_t, kRingQuadCorners> corners = {
          first, first + 1, first + kGridLines, first + kGridLines + 1};
      GridQuad quad{.color = color,
                    .uvs = {RingUv{u.at(column), v.at(row)},
                            RingUv{u.at(column + 1), v.at(row)},
                            RingUv{u.at(column), v.at(row + 1)},
                            RingUv{u.at(column + 1), v.at(row + 1)}},
                    .clut = state.clut,
                    .tpage = state.tpage};
      std::int32_t total = 0;
      for (std::size_t corner = 0; corner < kRingQuadCorners; ++corner) {
        const RingProjection& at = projected.at(corners.at(corner));
        total += at.depth;
        quad.screens.at(corner) = at.screen;
      }
      const std::int32_t depth =
          math::WrappingAdd(total / kGridQuadDivisor, frame.scratch.extent.z);
      if (SurfaceDepthVisible(depth, frame.inputs.near_clip)) {
        host.Submit(EncodeGridPacket(quad),
                    GridDepth(depth, frame.depth_mode, frame.inputs.near_clip));
      }
    }
  }
}
}  // namespace

// decomp: grids.h:59-74 (the create command).
GridState CreateGridState(RingPlan plan,
                          const SurfaceDefinition& definition) noexcept {
  const auto blend = static_cast<std::uint16_t>(
      (definition.transparency_curve & kLowBits) << kBlendShift);
  const auto page =
      static_cast<std::uint16_t>((definition.ring_segments & kLowBits) | blend);
  const auto clut_row = static_cast<std::uint16_t>(
      (definition.ring_positions & kLowBits) << kClutRowShift);
  return {.lifetime = plan.lifetime,
          .definition_index = plan.definition_index,
          .u = definition.grid_u,
          .v = definition.grid_v,
          .tpage = static_cast<std::uint16_t>(page + kGridPageBase),
          .clut = static_cast<std::uint16_t>(clut_row | kGridClutBase)};
}

std::int32_t GridDepth(std::int32_t depth, std::int32_t mode,
                       std::int32_t near_clip) noexcept {
  switch (static_cast<GridDepthMode>(mode)) {
    case GridDepthMode::kFarthest:
      return kFarthestSlot;
    case GridDepthMode::kFront15:
      return kFront15Slot;
    case GridDepthMode::kFront13:
      return kFront13Slot;
    case GridDepthMode::kFront11:
      return kFront11Slot;
    case GridDepthMode::kNudged:
      return depth - kNudge > near_clip ? depth - kNudge : depth;
    case GridDepthMode::kSorted:
    default:
      return depth;
  }
}

std::array<std::uint32_t, kGridPacketWords> EncodeGridPacket(
    const GridQuad& quad) noexcept {
  std::array<std::uint32_t, kGridPacketWords> words{};
  words[0] = kGridPacketLength;
  words[1] = kRingDrawModeWord;
  words[kColorWord] = SurfaceColorWord(quad.color, kGridQuadCode);
  // Only the first two corners' texture words carry the CLUT and the page.
  const std::array<std::uint16_t, kRingQuadCorners> high = {quad.clut,
                                                            quad.tpage, 0, 0};
  for (std::size_t corner = 0; corner < kRingQuadCorners; ++corner) {
    const std::size_t word = kFirstCornerWord + (corner * kGridCornerWords);
    words.at(word) = quad.screens.at(corner);
    words.at(word + 1) = SurfaceUvWord(quad.uvs.at(corner), high.at(corner));
  }
  words[kGridRestoreWord] = kRingRestoreModeWord;
  return words;
}

// decomp: EFFECT VS_GRID_FUNCTION (src/EFFECT/grids.h:23).
bool Grid(SurfaceCommand command, std::int32_t packed, GridHost& host) {
  const auto create = [&host](RingPlan plan,
                              const SurfaceDefinition& definition) {
    host.Create(CreateGridState(plan, definition), std::nullopt);
  };
  const auto tick = [&host] {
    std::optional<GridFrame> frame = BeginGridTick(host);
    if (!frame.has_value()) {
      return false;
    }
    const std::array<RingPoint, kGridPoints> points = GridPoints(*frame);
    DrawGrid(*frame, points, host);
    host.PublishScratch(frame->scratch);
    return FinishSurfaceTick(frame->state, host);
  };
  return RunSurfaceCommand(command, packed, host, create, tick);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG053.BIN func_800F9818 (src/EFFECT/PLG053.BIN/18.c:8).
bool Plg053::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG061.BIN func_800FA3B0 (src/EFFECT/PLG061.BIN/18.c:12).
bool Plg061::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG073.BIN func_800FE3EC (src/EFFECT/PLG073.BIN/F8.c:23).
bool Plg073::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG077.BIN func_800FB294 (src/EFFECT/PLG077.BIN/18.c:341).
bool Plg077::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG082.BIN func_800FB5CC (src/EFFECT/PLG082.BIN/128.c:20).
bool Plg082::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG085.BIN func_800F9818 (src/EFFECT/PLG085.BIN/18.c:2).
bool Plg085::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG086.BIN func_800FB720 (src/EFFECT/PLG086.BIN/F8.c:16).
bool Plg086::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG087.BIN func_800FB4F8 (src/EFFECT/PLG087.BIN/1F0.c:18).
bool Plg087::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG128.BIN func_800FC2BC (src/EFFECT/PLG128.BIN/F8.c:58).
bool Plg128::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG137.BIN func_800FC524 (src/EFFECT/PLG137.BIN/F8.c:17).
bool Plg137::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG138.BIN func_800FB2F4 (src/EFFECT/PLG138.BIN/18.c:9).
bool Plg138::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG139.BIN func_800FA710 (src/EFFECT/PLG139.BIN/18.c:6).
bool Plg139::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG142.BIN func_800F9818 (src/EFFECT/PLG142.BIN/18.c:8).
bool Plg142::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG150.BIN func_800FA710 (src/EFFECT/PLG150.BIN/18.c:6).
bool Plg150::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG152.BIN func_800F9928 (src/EFFECT/PLG152.BIN/128.c:8).
bool Plg152::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG156.BIN func_800FB5CC (src/EFFECT/PLG156.BIN/128.c:14).
bool Plg156::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG161.BIN func_800FD4F4 (src/EFFECT/PLG161.BIN/F8.c:20).
bool Plg161::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG165.BIN func_800FC8D0 (src/EFFECT/PLG165.BIN/F8.c:21).
bool Plg165::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG168.BIN func_800FD104 (src/EFFECT/PLG168.BIN/1F0.c:36).
bool Plg168::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG170.BIN func_800FBF6C (src/EFFECT/PLG170.BIN/18.c:21).
bool Plg170::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG242.BIN func_800FA7F0 (src/EFFECT/PLG242.BIN/F8.c:6).
bool Plg242::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG272.BIN func_800F9818 (src/EFFECT/PLG272.BIN/18.c:8).
bool Plg272::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG291.BIN func_800FBA18 (src/EFFECT/PLG291.BIN/F8.c:18).
bool Plg291::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}

// decomp: PLG297.BIN func_800FB3FC (src/EFFECT/PLG297.BIN/18.c:17).
bool Plg297::Grid(Templates::SurfaceCommand command, std::int32_t packed,
                  Templates::GridHost& host) {
  return Templates::Grid(command, packed, host);
}
}  // namespace VS::Effects
