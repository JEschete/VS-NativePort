#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Templates/ClosedRing.hpp"
#include "Effects/Templates/Ring.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/RingsLive.hpp"
#include "Effects/Templates/TiledRing.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Effects/Runtime.h"
}

namespace {
namespace rings = VS::Effects::Templates;
using rings::RingCommand;
using rings::RingCorner;
using rings::RingFrame;
using rings::RingPaletteEntry;
using rings::RingPoint;
using rings::RingProjection;
using rings::RingQuad;
using rings::RingState;
using rings::RingTexture;
using rings::RingTextureSource;
using rings::RingUv;
using VS::Core::Math::Vec3;

constexpr std::int32_t kHalf = 2048;
constexpr std::int32_t kQuarterTurn = 1024;

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

template <typename T>
T Required(std::optional<T>&& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *std::move(value);
}

struct Submission {
  RingQuad quad;
  std::int32_t depth{};
};

struct Event {
  std::uint8_t event{};
  Vec3 position{};
};

// Selects a ring whose tick moves nothing: every range and curve is zero
// except the half-way inner extent.
struct CreatedRing {};
constexpr CreatedRing kCreated{};

class FakeRingHost final : public rings::RingHost {
 public:
  FakeRingHost() = default;
  explicit FakeRingHost(CreatedRing /*created*/) : state(RingState{}) {
    inputs.definition.inner_extent = {kHalf, kHalf};
  }

  std::optional<RingState> state;
  std::optional<RingTextureSource> created_source;
  rings::RingTickInputs inputs{};
  bool refuse_tick = false;
  std::array<std::int32_t, 256> curve_values{};
  std::int32_t rand_value = 0;
  std::optional<rings::RingScratch> published;
  std::optional<bool> effect_view;
  std::vector<RingPoint> projected;
  std::int32_t depth = 100;
  std::optional<RingTexture> texture = RingTexture{
      .clut = 0x1234, .tpage = 0x567, .u = 16, .v = 32, .w = 64, .h = 8};
  std::vector<Submission> submissions;
  std::vector<RingPoint> transformed;
  std::vector<Event> events;

  void Create(const RingState& value, RingTextureSource source) override {
    state = value;
    created_source = source;
  }
  [[nodiscard]] std::optional<RingState> State() const override {
    return state;
  }
  void StoreState(const RingState& value) override { state = value; }
  [[nodiscard]] std::optional<rings::RingTickInputs> BeginTick(
      std::uint8_t /*definition_index*/) override {
    if (refuse_tick) {
      return std::nullopt;
    }
    return inputs;
  }
  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t /*distance*/) override {
    return curve_values.at(curve);
  }
  [[nodiscard]] std::int32_t Rand() override { return rand_value; }
  void PublishScratch(const rings::RingScratch& scratch) override {
    published = scratch;
  }
  void BeginProjection(bool view) override { effect_view = view; }
  // Screens number the points in projection order from 1.
  [[nodiscard]] RingProjection Project(RingPoint point) override {
    projected.push_back(point);
    return {.screen = static_cast<std::uint32_t>(projected.size()),
            .depth = depth};
  }
  [[nodiscard]] std::optional<RingTexture> AdvanceTexture() override {
    return texture;
  }
  void Submit(const RingQuad& quad, std::int32_t at) override {
    submissions.push_back({quad, at});
  }
  [[nodiscard]] Vec3 TransformByActor(RingPoint point) override {
    transformed.push_back(point);
    return {point.x + 1, point.y + 2, point.z + 3};
  }
  void EmitParticleEvent(std::uint8_t event, Vec3 position) override {
    events.push_back({event, position});
  }
};

const std::array<RingPaletteEntry, 1> kPalette{RingPaletteEntry{{10, 20, 30}}};

std::array<std::uint32_t, 4> Screens(const Submission& submission) {
  return submission.quad.screens;
}

rings::RingVectorRange Constant(Vec3 value) {
  return {VS::Core::Math::ScalarSpan{value.x, value.x},
          VS::Core::Math::ScalarSpan{value.y, value.y},
          VS::Core::Math::ScalarSpan{value.z, value.z}};
}

RingFrame RowFrame(std::optional<Vec3> jitter) {
  RingFrame frame;
  frame.scratch.position = {1000, 2000, 3000};
  frame.scratch.direction_jitter = jitter;
  frame.radius = {100, 200, 3000};
  frame.height = {0, 10, 20};
  return frame;
}
}  // namespace

TEST_CASE("Ring commands create expire and stop renderer slots",
          "[effects-rings][effects]") {
  FakeRingHost host;
  CHECK(rings::ApplyRingCommand(RingCommand::kExpire, 0,
                                RingTextureSource::kTextureAndAnimation, host));
  CHECK_FALSE(host.state.has_value());
  CHECK(rings::ApplyRingCommand(RingCommand::kCreate, 0x0302,
                                RingTextureSource::kSegmentsAndPositions,
                                host));
  CHECK(host.state == RingState{.lifetime = 2, .definition_index = 3});
  CHECK(host.created_source == RingTextureSource::kSegmentsAndPositions);
  CHECK(rings::ApplyRingCommand(
      RingCommand::kExpire, 0, RingTextureSource::kSegmentsAndPositions, host));
  CHECK(Required(host.state).lifetime == 1);
  CHECK_FALSE(rings::ApplyRingCommand(
      RingCommand::kStop, 0, RingTextureSource::kSegmentsAndPositions, host));
}

TEST_CASE("Ring create parameters split into lifetime and definition bytes",
          "[effects-rings][effects]") {
  CHECK(rings::PlanRing(0x1234ABCD) ==
        rings::RingPlan{.lifetime = 0xCD, .definition_index = 0xAB});
  CHECK(rings::PlanRing(-1) ==
        rings::RingPlan{.lifetime = 0xFF, .definition_index = 0xFF});
  FakeRingHost open;
  CHECK(
      VS::Effects::Plg049::Ring(RingCommand::kCreate, 0x0105, kPalette, open));
  CHECK(open.created_source == RingTextureSource::kTextureAndAnimation);
  FakeRingHost closed;
  CHECK(VS::Effects::Plg053::ClosedRing(RingCommand::kCreate, 0x0105, kPalette,
                                        closed));
  CHECK(closed.created_source == RingTextureSource::kSegmentsAndPositions);
  FakeRingHost tiled;
  CHECK(VS::Effects::Plg044::TiledRing(RingCommand::kCreate, 0x0105, kPalette,
                                       tiled));
  CHECK(tiled.created_source == RingTextureSource::kSegmentsAndPositions);
}

TEST_CASE("Ring palette lookups mask the index and zero missing entries",
          "[effects-rings][effects]") {
  const std::array<RingPaletteEntry, 2> palette{RingPaletteEntry{{1, 2, 3}},
                                                RingPaletteEntry{{4, 5, 6}}};
  CHECK(rings::RingPaletteAt(palette, 1) == palette[1]);
  CHECK(rings::RingPaletteAt(palette, 9) == palette[1]);
  CHECK(rings::RingPaletteAt(palette, 8) == palette[0]);
  CHECK(rings::RingPaletteAt(palette, 5) == RingPaletteEntry{});
  CHECK(rings::RingPaletteAt({}, 0) == RingPaletteEntry{});
}

TEST_CASE("Ring texture rows and steps wrap in a byte",
          "[effects-rings][effects]") {
  CHECK(rings::RingRowCoordinates(16, 8, kHalf) ==
        std::array<std::uint8_t, 3>{16, 20, 24});
  CHECK(rings::RingRowCoordinates(250, 10, 4096) ==
        std::array<std::uint8_t, 3>{250, 4, 4});
  CHECK(rings::RingTextureStep(16, 8, 3, 8) == 19);
  CHECK(rings::RingTextureStep(200, 100, 1, 1) == 44);
  CHECK(rings::RingTextureStep(7, 100, 1, 0) == 7);
  CHECK(rings::RingFixed(-1, 4096) == -1);
  CHECK(rings::RingFixed(0x10000, 0x10000) == 0);
  CHECK(rings::RingPointAt({0x18000, -1, 3}) == RingPoint{-0x8000, -1, 3});
}

TEST_CASE("Open ring quad corners turn for transposed textures",
          "[effects-rings][effects]") {
  using rings::RingOrientation;
  const rings::RingQuadCorners upright =
      rings::OpenRingCorners(RingOrientation::kUpright, 1, 3);
  const rings::RingQuadCorners transposed =
      rings::OpenRingCorners(RingOrientation::kTransposed, 1, 3);
  CHECK(upright[0] == RingCorner{1, 3});
  CHECK(upright[1] == RingCorner{1, 4});
  CHECK(upright[2] == RingCorner{2, 3});
  CHECK(upright[3] == RingCorner{2, 4});
  CHECK(transposed[0] == RingCorner{1, 3});
  CHECK(transposed[1] == RingCorner{2, 3});
  CHECK(transposed[2] == RingCorner{1, 4});
  CHECK(transposed[3] == RingCorner{2, 4});
}

TEST_CASE("Open ring texture coordinates step through eighths of the texture",
          "[effects-rings][effects]") {
  using rings::RingOrientation;
  const RingTexture texture{.u = 16, .v = 32, .w = 8, .h = 8};
  const rings::RingQuadUvs upright =
      rings::OpenRingUvs(RingOrientation::kUpright, texture, kHalf, 0, 3);
  CHECK(upright[0] == RingUv{19, 32});
  CHECK(upright[1] == RingUv{20, 32});
  CHECK(upright[2] == RingUv{19, 36});
  CHECK(upright[3] == RingUv{20, 36});
  const rings::RingQuadUvs transposed =
      rings::OpenRingUvs(RingOrientation::kTransposed, texture, kHalf, 1, 3);
  CHECK(transposed[0] == RingUv{20, 35});
  CHECK(transposed[1] == RingUv{24, 35});
  CHECK(transposed[3] == RingUv{24, 36});
  CHECK(rings::OpenRingUvs(RingOrientation::kUpright, texture, kHalf, 2, 0) ==
        rings::RingQuadUvs{});
}

TEST_CASE("Sixteen column rings repeat the texture over their span",
          "[effects-rings][effects]") {
  const RingTexture texture{.u = 16, .v = 32, .w = 64, .h = 8};
  const rings::RingQuadUvs four =
      rings::SixteenColumnRingUvs(4, texture, kHalf, 0, 5);
  CHECK(four[0] == RingUv{32, 32});
  CHECK(four[3] == RingUv{48, 36});
  const rings::RingQuadUvs sixteen =
      rings::SixteenColumnRingUvs(16, texture, kHalf, 1, 5);
  CHECK(sixteen[0] == RingUv{36, 36});
  CHECK(sixteen[3] == RingUv{40, 40});
  const rings::RingQuadUvs one =
      rings::SixteenColumnRingUvs(1, texture, kHalf, 0, 9);
  CHECK(one[0] == RingUv{16, 32});
  CHECK(one[1] == RingUv{80, 32});
  CHECK(rings::SixteenColumnRingUvs(0, texture, kHalf, 0, 0) ==
        rings::RingQuadUvs{});
  CHECK(rings::SixteenColumnRingUvs(4, texture, kHalf, 2, 0) ==
        rings::RingQuadUvs{});
}

TEST_CASE("Closed ring corners close the last column onto the first",
          "[effects-rings][effects]") {
  const rings::RingQuadCorners last = rings::ClosedRingCorners(1, 15);
  CHECK(last[0] == RingCorner{1, 15});
  CHECK(last[1] == RingCorner{1, 0});
  CHECK(last[2] == RingCorner{2, 15});
  CHECK(last[3] == RingCorner{2, 0});
  const rings::RingQuadCorners inner = rings::ClosedRingCorners(0, 3);
  CHECK(inner[1] == RingCorner{0, 4});
  CHECK(inner[2] == RingCorner{1, 3});
}

TEST_CASE("Closed ring points mirror one quadrant into four",
          "[effects-rings][effects]") {
  const RingFrame frame = RowFrame(std::nullopt);
  const auto points = rings::ClosedRingPoints(frame);
  CHECK(points[0] == RingPoint{1100, 2000, 3000});
  CHECK(points[4] == RingPoint{1000, 2000, 3100});
  CHECK(points[8] == RingPoint{900, 2000, 3000});
  CHECK(points[12] == RingPoint{1000, 2000, 2900});
  CHECK(points[16] == RingPoint{1200, 2010, 3000});
  const std::int32_t cosine = VS::Core::Math::Cosine(256);
  const std::int32_t sine = VS::Core::Math::Sine(256);
  CHECK(points[1].x == 1000 + rings::RingFixed(100, cosine));
  CHECK(points[1].z == 3000 + rings::RingFixed(100, sine));
  CHECK(points[5].x == 1000 - rings::RingFixed(100, sine));
}

TEST_CASE("Open ring points spread over the jitter with truncated eighths",
          "[effects-rings][effects]") {
  using VS::Core::Math::Sine;
  const auto still = rings::OpenRingPoints(RowFrame(std::nullopt));
  CHECK(still[0] == RingPoint{1100, 2000, 3000});
  CHECK(still[8] == still[0]);
  CHECK(still[9] == RingPoint{1200, 2010, 3000});
  // Column 1 turns by -1020 / 8 = -127 (toward zero) and the spread starts
  // at -(7) / 2 = -3 (toward zero), plus 7 / 8 = 0.
  const auto spread = rings::OpenRingPoints(RowFrame(Vec3{-1020, 7, 0}));
  CHECK(rings::RingFixed(3000, Sine(-130)) !=
        rings::RingFixed(3000, Sine(-131)));
  CHECK(spread[(2 * 9) + 1].z == 3000 + rings::RingFixed(3000, Sine(-130)));
  CHECK(spread[(2 * 9) + 0].z == 3000 + rings::RingFixed(3000, Sine(-3)));
  // Column 8: -1020 and -3 + 7.
  CHECK(spread[(2 * 9) + 8].z == 3000 + rings::RingFixed(3000, Sine(-1016)));
}

TEST_CASE("Ring ticks grow radius and height from force and acceleration",
          "[effects-rings][effects]") {
  FakeRingHost host{kCreated};
  host.state = RingState{.radius = 100,
                         .radius_speed = 3,
                         .height = 200,
                         .height_speed = 4,
                         .angle = 10,
                         .age = 5};
  rings::RingDefinition& definition = host.inputs.definition;
  definition.extent = Constant({50, 60, 0});
  definition.force_min = Constant({5, 6, 0});
  definition.acceleration_min = Constant({1, 2, 0});
  definition.speed = {VS::Core::Math::ScalarSpan{16, 16},
                      VS::Core::Math::ScalarSpan{16, 16}};
  const RingFrame frame = Required(rings::BeginRingTick(kPalette, host));
  CHECK(frame.state.radius_speed == 4);
  CHECK(frame.state.radius == 109);
  CHECK(frame.state.height_speed == 6);
  CHECK(frame.state.height == 212);
  CHECK(frame.fraction == kHalf);
  CHECK(frame.radius == std::array<std::int32_t, 3>{159, 295, 431});
  CHECK(frame.height == std::array<std::int32_t, 3>{0, 0, 0});
  CHECK(frame.state.angle == 26);
}

TEST_CASE("Ring rows tilt by the base direction angle",
          "[effects-rings][effects]") {
  FakeRingHost host{kCreated};
  host.inputs.definition.extent = Constant({50, 60, 0});
  host.inputs.definition.direction = Constant({kQuarterTurn, 0, 0});
  const RingFrame frame = Required(rings::BeginRingTick(kPalette, host));
  CHECK(frame.scratch.base_direction == Vec3{kQuarterTurn, 0, 0});
  CHECK(frame.radius == std::array<std::int32_t, 3>{50, 50, 50});
  CHECK(frame.height == std::array<std::int32_t, 3>{0, 30, 60});
}

TEST_CASE("Ring anchors move the sampled position",
          "[effects-rings][effects]") {
  FakeRingHost host{kCreated};
  host.inputs.definition.position = Constant({7, 8, 9});
  host.inputs.effect_origin = {100, 200, 300};
  host.inputs.actor_translation = {1000, 2000, 3000};
  const std::array<Vec3, 4> expected{Vec3{7, 8, 9}, Vec3{107, 208, 309},
                                     Vec3{1007, 2008, 3009}, Vec3{7, 8, 9}};
  for (std::uint32_t anchor = 0; anchor < expected.size(); ++anchor) {
    host.inputs.definition.flags = anchor | 0x100;
    const RingFrame frame = Required(rings::BeginRingTick(kPalette, host));
    CHECK(frame.scratch.position == expected.at(anchor));
    CHECK(frame.scratch.flags == (anchor | 0x100));
  }
}

TEST_CASE("Ring rows take palette intensity scaled by colour curves",
          "[effects-rings][effects]") {
  FakeRingHost host{kCreated};
  host.inputs.definition.palette_index = 8;
  host.inputs.definition.curves.color = {5, 0, 0};
  host.curve_values[5] = 64;
  const RingFrame frame = Required(rings::BeginRingTick(kPalette, host));
  CHECK(frame.colors[0] == rings::RingColor{5, 10, 10});
  CHECK(frame.colors[1] == rings::RingColor{10, 20, 20});
  CHECK(frame.colors[2] == rings::RingColor{15, 30, 30});
  host.inputs.definition.palette_index = 1;
  const RingFrame missing = Required(rings::BeginRingTick(kPalette, host));
  CHECK(missing.colors[2] == rings::RingColor{});
}

TEST_CASE("Ring angle steps draw a random value between unordered bounds",
          "[effects-rings][effects]") {
  FakeRingHost host{kCreated};
  Required(host.state).angle = 10;
  host.inputs.definition.speed = {VS::Core::Math::ScalarSpan{20, 20},
                                  VS::Core::Math::ScalarSpan{10, 10}};
  host.rand_value = 25;
  const RingFrame frame = Required(rings::BeginRingTick(kPalette, host));
  CHECK(frame.scratch.speed == VS::Core::Math::Vec2{20, 10});
  CHECK(frame.state.angle == 23);
}

TEST_CASE("Ring quads outside the depth window are not submitted",
          "[effects-rings][effects]") {
  RingFrame frame;
  frame.colors = {rings::RingColor{1, 2, 3}, rings::RingColor{4, 5, 6},
                  rings::RingColor{7, 8, 9}};
  std::array<RingProjection, 4> points{
      RingProjection{11, -1}, RingProjection{12, -1}, RingProjection{13, -1},
      RingProjection{14, 0}};
  const rings::RingProjectedGrid grid{.points = points, .columns = 2};
  const rings::RingQuadCorners corners{RingCorner{0, 0}, RingCorner{0, 1},
                                       RingCorner{1, 0}, RingCorner{1, 1}};
  const RingTexture texture{.clut = 0x1234, .tpage = 0x567};
  FakeRingHost host;
  frame.inputs.near_clip = -1;
  rings::SubmitRingQuad(frame, grid, corners, {}, texture, host);
  REQUIRE(host.submissions.size() == 1);
  CHECK(host.submissions[0].depth == 0);
  CHECK(Screens(host.submissions[0]) ==
        std::array<std::uint32_t, 4>{11, 12, 13, 14});
  CHECK(host.submissions[0].quad.colors[3] == frame.colors[1]);
  CHECK(host.submissions[0].quad.tpage == 0x567);
  frame.inputs.near_clip = 0;
  rings::SubmitRingQuad(frame, grid, corners, {}, texture, host);
  CHECK(host.submissions.size() == 1);
  points.fill(RingProjection{0, rings::kRingFarDepth});
  frame.inputs.near_clip = -1;
  rings::SubmitRingQuad(frame, grid, corners, {}, texture, host);
  CHECK(host.submissions.size() == 1);
}

TEST_CASE("Ring packets link a draw mode word and a textured quad",
          "[effects-rings][effects]") {
  RingQuad quad{
      .screens = {0x00100020, 2, 3, 4},
      .colors = {rings::RingColor{1, 2, 3}, rings::RingColor{},
                 rings::RingColor{}, rings::RingColor{}},
      .uvs = {RingUv{5, 6}, RingUv{7, 8}, RingUv{9, 10}, RingUv{11, 12}},
      .clut = 0x1234,
      .tpage = 0x567};
  const auto words = rings::EncodeRingPacket(quad, 0xAB123456);
  CHECK(words[0] == 0x0E123456);
  CHECK(words[1] == 0xE1000000);
  CHECK(words[2] == 0x3E030201);
  CHECK(words[3] == 0x00100020);
  CHECK(words[4] == 0x12340605);
  CHECK(words[7] == 0x05670807);
  CHECK(words[13] == 0x00000C0B);
  CHECK(words[14] == 0xE1000200);
}

TEST_CASE("Ring ticks publish scratch and emit tick events in Q12",
          "[effects-rings][effects]") {
  FakeRingHost host;
  RingFrame frame;
  frame.state = RingState{.lifetime = 2, .age = 4};
  frame.scratch.position = {1, 2, 3};
  frame.inputs.definition.tick_event = 3;
  CHECK(rings::FinishRingTick(frame, host));
  CHECK(host.published == frame.scratch);
  REQUIRE(host.events.size() == 1);
  CHECK(host.events[0].event == 2);
  CHECK(host.events[0].position == Vec3{4096, 8192, 12288});
  CHECK(host.state == RingState{.lifetime = 1, .age = 5});
  frame.inputs.definition.flags = rings::kRingEffectView;
  CHECK_FALSE(rings::FinishRingTick(frame, host));
  CHECK(host.transformed == std::vector<RingPoint>{RingPoint{1, 2, 3}});
  CHECK(host.events.at(1).position == Vec3{8192, 16384, 24576});
}

TEST_CASE("Ring lifetimes of zero never run out", "[effects-rings][effects]") {
  FakeRingHost host;
  RingFrame frame;
  CHECK(rings::FinishRingTick(frame, host));
  CHECK(rings::FinishRingTick(frame, host));
  CHECK(host.state == RingState{.age = 2});
  CHECK(host.events.empty());
}

TEST_CASE("Ring ticks stop when the host has no state or refuses",
          "[effects-rings][effects]") {
  FakeRingHost empty;
  CHECK_FALSE(
      VS::Effects::Plg049::Ring(RingCommand::kTick, 0, kPalette, empty));
  FakeRingHost refused{kCreated};
  refused.refuse_tick = true;
  CHECK_FALSE(VS::Effects::Plg053::ClosedRing(RingCommand::kTick, 0, kPalette,
                                              refused));
  CHECK(refused.state == RingState{});
  CHECK(refused.projected.empty());
  CHECK(refused.published == std::nullopt);
}

TEST_CASE("Open ring ticks draw two bands of eight quads",
          "[effects-rings][effects]") {
  FakeRingHost host{kCreated};
  CHECK(VS::Effects::Plg049::Ring(RingCommand::kTick, 0, kPalette, host));
  CHECK(host.projected.size() == 27);
  REQUIRE(host.submissions.size() == 16);
  CHECK(Screens(host.submissions[0]) ==
        std::array<std::uint32_t, 4>{1, 2, 10, 11});
  CHECK(Screens(host.submissions[15]) ==
        std::array<std::uint32_t, 4>{17, 18, 26, 27});
  CHECK(host.submissions[0].quad.colors[2] == rings::RingColor{20, 20, 20});
  CHECK(Required(host.published).direction_jitter.has_value());
  CHECK(Required(host.state).age == 1);
}

TEST_CASE("Transposed open ring instances turn their quads",
          "[effects-rings][effects]") {
  FakeRingHost transposed{kCreated};
  CHECK(VS::Effects::Plg129::Ring(RingCommand::kTick, 0, kPalette, transposed));
  REQUIRE(transposed.submissions.size() == 16);
  CHECK(Screens(transposed.submissions[0]) ==
        std::array<std::uint32_t, 4>{1, 10, 2, 11});
  CHECK(transposed.submissions[0].quad.uvs[1] == RingUv{48, 32});
}

TEST_CASE("Second ring instances of PLG129 and PLG161 draw upright quads",
          "[effects-rings][effects]") {
  FakeRingHost ring2{kCreated};
  CHECK(VS::Effects::Plg129::Ring2(RingCommand::kTick, 0, kPalette, ring2));
  REQUIRE(ring2.submissions.size() == 16);
  CHECK(Screens(ring2.submissions[0]) ==
        std::array<std::uint32_t, 4>{1, 2, 10, 11});
  FakeRingHost later{kCreated};
  CHECK(VS::Effects::Plg161::Ring2(RingCommand::kTick, 0, kPalette, later));
  CHECK(Screens(later.submissions.at(0)) ==
        std::array<std::uint32_t, 4>{1, 2, 10, 11});
}

TEST_CASE("Closed ring ticks draw two bands of sixteen closing quads",
          "[effects-rings][effects]") {
  FakeRingHost host{kCreated};
  CHECK(VS::Effects::Plg053::ClosedRing(RingCommand::kTick, 0, kPalette, host));
  CHECK(host.projected.size() == 48);
  REQUIRE(host.submissions.size() == 32);
  CHECK(Screens(host.submissions[15]) ==
        std::array<std::uint32_t, 4>{16, 1, 32, 17});
  CHECK(Screens(host.submissions[16]) ==
        std::array<std::uint32_t, 4>{17, 18, 33, 34});
  CHECK_FALSE(Required(host.published).direction_jitter.has_value());
}

TEST_CASE("Closed ring instances repeat the texture four or one times",
          "[effects-rings][effects]") {
  FakeRingHost sixteen{kCreated};
  CHECK(VS::Effects::Plg053::ClosedRing(RingCommand::kTick, 0, kPalette,
                                        sixteen));
  CHECK(sixteen.submissions.at(5).quad.uvs[0] == RingUv{36, 32});
  FakeRingHost four{kCreated};
  CHECK(VS::Effects::Plg051::ClosedRing(RingCommand::kTick, 0, kPalette, four));
  CHECK(four.submissions.at(5).quad.uvs[0] == RingUv{32, 32});
  CHECK(four.submissions.at(5).quad.uvs[3] == RingUv{48, 36});
}

TEST_CASE("Tiled ring ticks stretch the texture over every quad",
          "[effects-rings][effects]") {
  FakeRingHost host{kCreated};
  CHECK(VS::Effects::Plg044::TiledRing(RingCommand::kTick, 0, kPalette, host));
  REQUIRE(host.submissions.size() == 32);
  CHECK(host.submissions[7].quad.uvs[0] == RingUv{16, 32});
  CHECK(host.submissions[7].quad.uvs[1] == RingUv{80, 32});
  CHECK(host.submissions[23].quad.uvs[2] == RingUv{16, 40});
  CHECK(Screens(host.submissions[31]) ==
        std::array<std::uint32_t, 4>{32, 17, 48, 33});
}

TEST_CASE("Ring ticks without a texture finish without drawing",
          "[effects-rings][effects]") {
  FakeRingHost host{kCreated};
  host.texture.reset();
  host.inputs.definition.tick_event = 1;
  CHECK(VS::Effects::Plg044::TiledRing(RingCommand::kTick, 0, kPalette, host));
  CHECK(host.projected.size() == 48);
  CHECK(host.submissions.empty());
  CHECK(host.events.size() == 1);
  CHECK(Required(host.state).age == 1);
}

namespace {
// Points the effect globals at an empty package for one test and restores
// them afterwards.
class EmptyLivePackage {
 public:
  EmptyLivePackage() {
    Effects_package = &package_;
    VS::Effects::ForgetLivePackageExtents();
  }
  EmptyLivePackage(const EmptyLivePackage&) = delete;
  EmptyLivePackage& operator=(const EmptyLivePackage&) = delete;
  EmptyLivePackage(EmptyLivePackage&&) = delete;
  EmptyLivePackage& operator=(EmptyLivePackage&&) = delete;
  ~EmptyLivePackage() { Effects_package = saved_; }

 private:
  Effects_EffectPackage* saved_ = Effects_package;
  Effects_EffectPackage package_{};
};
}  // namespace

TEST_CASE("Live ring entries refuse definitions the package lacks",
          "[effects-rings][effects]") {
  const EmptyLivePackage package;
  std::vector<std::string> warnings;
  VS::Core::LogObserver previous =
      VS::Core::SetLogObserver([&warnings](const VS::Core::LogRecord& record) {
        if (record.channel == "effects") {
          warnings.emplace_back(record.message);
        }
      });
  Effects_RendererSlot slot{};
  CHECK(rings::UpdateLivePlg049Ring(&slot, 1, 0x0105) == 1);
  CHECK(slot.rendererState == nullptr);
  VS::Core::SetLogObserver(std::move(previous));
  REQUIRE_FALSE(warnings.empty());
  CHECK(warnings.front().find("definition 1") != std::string::npos);
}

TEST_CASE("Live ring entries answer commands without a ring state",
          "[effects-rings][effects]") {
  const EmptyLivePackage package;
  Effects_RendererSlot slot{};
  CHECK(rings::UpdateLivePlg049Ring(&slot, 3, 0) == 1);
  CHECK(rings::UpdateLivePlg049Ring(&slot, 9, 0) == 1);
  CHECK(rings::UpdateLivePlg053ClosedRing(&slot, 2, 0) == 0);
  CHECK(rings::UpdateLivePlg044TiledRing(&slot, 4, 0) == 0);
  CHECK(rings::UpdateLivePlg049Ring(nullptr, 2, 0) == 0);
  CHECK(slot.rendererState == nullptr);
}
