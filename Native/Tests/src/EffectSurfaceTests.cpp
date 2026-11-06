#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Math/Interpolation.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Templates/Grid.hpp"
#include "Effects/Templates/Mesh10.hpp"
#include "Effects/Templates/MirrorSurface.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/RotatingSurface.hpp"
#include "Effects/Templates/Surface.hpp"
#include "Effects/Templates/SurfaceCore.hpp"
#include "Effects/Templates/SurfacesLive.hpp"
#include "Legacy/Psx.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Effects/Runtime.h"
}

namespace {
namespace surfaces = VS::Effects::Templates;
using surfaces::GridState;
using surfaces::Mesh10State;
using surfaces::RingPoint;
using surfaces::RingProjection;
using surfaces::RingTexture;
using surfaces::SheetState;
using surfaces::SurfaceAnimation;
using surfaces::SurfaceCommand;
using surfaces::SurfaceDefinition;
using surfaces::SurfaceScratch;
using surfaces::SurfaceTickInputs;
using VS::Core::Math::ScalarSpan;
using VS::Core::Math::Vec2;
using VS::Core::Math::Vec3;

constexpr std::int32_t kUnit = 4096;
constexpr std::int32_t kQuarterTurn = 1024;
constexpr std::int32_t kSceneDepth = 100;
constexpr RingTexture kTexture{
    .clut = 0x1234,
    .tpage = 0x567,
    .u = 16,
    .v = 32,
    .w = 64,
    .h = 8,
};

// The value of an optional the test has just required; the explicit throw
// lets bugprone-unchecked-optional-access see that REQUIRE stops the test.
template <typename T>
T Present(const std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value.has_value()) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

struct Packet {
  std::vector<std::uint32_t> words;
  std::int32_t depth{};
};

struct Emission {
  std::uint8_t event{};
  Vec3 position{};
};

// Records every service call. Screens number the projected points from 1;
// each curve id samples its entry of `curve_values`.
template <typename Record>
class FakeSurfaceHost final : public surfaces::SurfaceRecordHost<Record> {
 public:
  std::optional<Record> state;
  std::optional<SurfaceAnimation> animation;
  bool created = false;
  std::optional<SurfaceDefinition> definition = SurfaceDefinition{};
  std::vector<std::uint8_t> factor_curve;
  std::vector<std::uint8_t> factor_requests;
  SurfaceTickInputs inputs{};
  bool refuse_tick = false;
  std::array<std::int32_t, 256> curve_values{};
  std::int32_t rand_value = 0;
  std::optional<SurfaceScratch> published;
  std::optional<bool> effect_view;
  std::vector<RingPoint> projected;
  std::int32_t depth = kSceneDepth;
  std::optional<RingTexture> texture = kTexture;
  std::vector<Packet> packets;
  std::vector<RingPoint> transformed;
  std::vector<Emission> emissions;

  void Create(const Record& value,
              std::optional<SurfaceAnimation> started) override {
    state = value;
    animation = started;
    created = true;
  }
  [[nodiscard]] std::optional<Record> State() const override { return state; }
  void StoreState(const Record& value) override { state = value; }
  [[nodiscard]] std::optional<SurfaceDefinition> CreateDefinition(
      std::uint8_t /*definition_index*/) override {
    return definition;
  }
  [[nodiscard]] std::span<const std::uint8_t> FactorCurve(
      std::uint8_t curve) override {
    factor_requests.push_back(curve);
    return factor_curve;
  }
  [[nodiscard]] std::optional<SurfaceTickInputs> BeginTick(
      std::uint8_t /*definition_index*/) override {
    if (refuse_tick) {
      return std::nullopt;
    }
    return inputs;
  }
  [[nodiscard]] std::int32_t SampleCurve(std::int32_t curve,
                                         std::int32_t /*distance*/) override {
    return curve_values.at(static_cast<std::size_t>(curve));
  }
  [[nodiscard]] std::int32_t Rand() override { return rand_value; }
  void PublishScratch(const SurfaceScratch& scratch) override {
    published = scratch;
  }
  void BeginProjection(bool view) override { effect_view = view; }
  [[nodiscard]] RingProjection Project(RingPoint point) override {
    projected.push_back(point);
    return {.screen = static_cast<std::uint32_t>(projected.size()),
            .depth = depth};
  }
  [[nodiscard]] std::optional<RingTexture> AdvanceTexture() override {
    return texture;
  }
  void Submit(std::span<const std::uint32_t> packet, std::int32_t at) override {
    packets.push_back({{packet.begin(), packet.end()}, at});
  }
  [[nodiscard]] Vec3 TransformByActor(RingPoint point) override {
    transformed.push_back(point);
    return {point.x * 2, point.y * 2, point.z * 2};
  }
  void EmitParticleEvent(std::uint8_t event, Vec3 position) override {
    emissions.push_back({event, position});
  }
};

using FakeGridHost = FakeSurfaceHost<GridState>;
using FakeSheetHost = FakeSurfaceHost<SheetState>;
using FakeMeshHost = FakeSurfaceHost<Mesh10State>;

// A range every weight samples as `value`.
surfaces::RingVectorRange Fixed(Vec3 value) {
  return {ScalarSpan{value.x, value.x}, ScalarSpan{value.y, value.y},
          ScalarSpan{value.z, value.z}};
}

// A profile whose points of row r are (1, 1, r / 4) in Q12.
std::array<RingPoint, surfaces::kSurfaceProfilePoints> RowProfile() {
  std::array<RingPoint, surfaces::kSurfaceProfilePoints> profile{};
  for (std::size_t point = 0; point < profile.size(); ++point) {
    const auto row =
        static_cast<std::int16_t>(point / surfaces::kSurfaceColumns);
    profile.at(point) = {kUnit, kUnit,
                         static_cast<std::int16_t>(row * kQuarterTurn)};
  }
  return profile;
}

// Anchors a sheet at (1000, 2000, 3000) whose first row has radius 100 and
// height 50, each later row 10 wider and 5 taller.
void GrowSheet(FakeSheetHost& host) {
  SheetState state;
  state.factors.fill(200);
  host.state = state;
  SurfaceDefinition& definition = host.inputs.definition;
  definition.position = Fixed({1000, 2000, 3000});
  definition.extent = Fixed({100, 50, 0});
  definition.force_max = Fixed({10, 5, 0});
}

std::uint32_t Word(const Packet& packet, std::size_t index) {
  return packet.words.at(index);
}

// A grid anchored at (100, 50, 200) whose lines are 10 apart along x and 20
// along z.
void LayGrid(FakeGridHost& host) {
  host.state = GridState{};
  SurfaceDefinition& definition = host.inputs.definition;
  definition.position = Fixed({100, 50, 200});
  definition.extent = Fixed({10, 20, 0});
  definition.inner_extent = {1, 1};
}

// A grid whose texture starts at (16, 8) of a 64-wide frame and scrolls 5
// along u and 3 along v each tick.
void ScrollGrid(FakeGridHost& host) {
  host.state = GridState{.u = 16, .v = 8};
  SurfaceDefinition& definition = host.inputs.definition;
  definition.gravity = {ScalarSpan{5, 5}, ScalarSpan{3, 3}};
  definition.grid_width = 64;
  definition.grid_height = 0;
}
}  // namespace

TEST_CASE("Surface remainders keep the R3000 divide results",
          "[effects-surface-templates][effects]") {
  CHECK(surfaces::SurfaceRemainder(7, 3) == 1);
  CHECK(surfaces::SurfaceRemainder(-7, 3) == -1);
  CHECK(surfaces::SurfaceRemainder(7, -3) == 1);
  CHECK(surfaces::SurfaceRemainder(7, 0) == 7);
  CHECK(surfaces::SurfaceRemainder(-7, 0) == -7);
  CHECK(surfaces::SurfaceRemainder(5, -1) == 0);
  CHECK(surfaces::SurfaceRemainder(std::numeric_limits<std::int32_t>::min(),
                                   -1) == 0);
}

TEST_CASE("Surface words pack colours coordinates halfwords and plans",
          "[effects-surface-templates][effects]") {
  CHECK(surfaces::SurfaceColorWord({.r = 1, .g = 2, .b = 3}, 0x2E) ==
        0x2E030201U);
  CHECK(surfaces::SurfaceUvWord({.u = 0x10, .v = 0x20}, 0x3C30) == 0x3C302010U);
  CHECK(surfaces::SurfaceHalfwords({0x18000, -0x18000, 70000}) ==
        Vec3{-0x8000, -0x8000, 4464});
  CHECK(surfaces::SurfaceDepthVisible(0x7FF, 10));
  CHECK_FALSE(surfaces::SurfaceDepthVisible(0x800, 10));
  CHECK_FALSE(surfaces::SurfaceDepthVisible(10, 10));
  const surfaces::RingPlan plan = surfaces::PlanSurface(0x0305);
  CHECK(plan.lifetime == 5);
  CHECK(plan.definition_index == 3);
}

TEST_CASE("Doubled surface curve channels saturate from 256",
          "[effects-surface-templates][effects]") {
  FakeGridHost host;
  host.curve_values.at(1) = 64;
  host.curve_values.at(2) = 127;
  host.curve_values.at(3) = 128;
  host.curve_values.at(4) = 200;
  host.curve_values.at(5) = -1;
  CHECK(surfaces::DoubledCurveChannel(0, 0, host) == 128);
  CHECK(surfaces::DoubledCurveChannel(1, 0, host) == 128);
  CHECK(surfaces::DoubledCurveChannel(2, 0, host) == 254);
  CHECK(surfaces::DoubledCurveChannel(3, 0, host) == 255);
  CHECK(surfaces::DoubledCurveChannel(4, 0, host) == 255);
  CHECK(surfaces::DoubledCurveChannel(5, 0, host) == 254);
}

TEST_CASE("Surface positions follow the definition's anchor bits",
          "[effects-surface-templates][effects]") {
  FakeGridHost host;
  SurfaceTickInputs inputs{.effect_origin = {100, 200, 300},
                           .actor_translation = {1000, 2000, 3000}};
  inputs.definition.position = Fixed({10, 20, 30});
  inputs.definition.flags = 1;
  CHECK(surfaces::SampleSurfacePosition(inputs, 0, host) ==
        Vec3{110, 220, 330});
  inputs.definition.flags = 2;
  CHECK(surfaces::SampleSurfacePosition(inputs, 0, host) ==
        Vec3{1010, 2020, 3030});
  inputs.definition.flags = 3;
  CHECK(surfaces::SampleSurfacePosition(inputs, 0, host) == Vec3{10, 20, 30});
  inputs.definition.flags = surfaces::kRingEffectView | 1U;
  CHECK(surfaces::SampleSurfacePosition(inputs, 0, host) ==
        Vec3{110, 220, 330});
}

TEST_CASE("Surface forces wrap each bound to a halfword",
          "[effects-surface-templates][effects]") {
  FakeGridHost host;
  SurfaceDefinition definition;
  definition.force_min = Fixed({0x18000, 5, 0});
  definition.force_max = Fixed({1, 2, 3});
  definition.acceleration_min = Fixed({-4, 0, 0});
  definition.acceleration_max = Fixed({70000, 0, 0});
  const surfaces::SurfaceForces forces =
      surfaces::SampleSurfaceForces(definition, 0, host);
  CHECK(forces.force_min == Vec3{-0x8000, 5, 0});
  CHECK(forces.force_max == Vec3{1, 2, 3});
  CHECK(forces.acceleration_min == Vec3{-4, 0, 0});
  CHECK(forces.acceleration_max == Vec3{4464, 0, 0});
}

TEST_CASE("Surface tick events pass through the actor with the view flag",
          "[effects-surface-templates][effects]") {
  FakeMeshHost host;
  SurfaceDefinition definition;
  surfaces::EmitSurfaceTickEvent(definition, {1, 2, 3}, host);
  CHECK(host.emissions.empty());
  definition.tick_event = 3;
  surfaces::EmitSurfaceTickEvent(definition, {1, 2, 3}, host);
  REQUIRE(host.emissions.size() == 1);
  CHECK(host.emissions.back().event == 2);
  CHECK(host.emissions.back().position == Vec3{kUnit, 2 * kUnit, 3 * kUnit});
  CHECK(host.transformed.empty());
  definition.flags = surfaces::kRingEffectView;
  surfaces::EmitSurfaceTickEvent(definition, {1, 2, 3}, host);
  REQUIRE(host.emissions.size() == 2);
  CHECK(host.transformed == std::vector<RingPoint>{RingPoint{1, 2, 3}});
  CHECK(host.emissions.back().position ==
        Vec3{2 * kUnit, 4 * kUnit, 6 * kUnit});
}

TEST_CASE("Sheet factors spread a transparency curve over the rows",
          "[effects-surface-templates][effects]") {
  const auto none = surfaces::SheetFactors({}, surfaces::kSurfaceRows);
  CHECK(none[0] == 255);
  CHECK(none[4] == 255);
  CHECK(none[5] == 0);
  const std::array<std::uint8_t, 5> five{10, 64, 100, 128, 30};
  CHECK(surfaces::SheetFactors(five, surfaces::kSurfaceRows) ==
        std::array<std::int32_t, 9>{20, 128, 200, 255, 60, 0, 0, 0, 0});
  const std::array<std::uint8_t, 3> three{10, 64, 100};
  CHECK(surfaces::SheetFactors(three, surfaces::kMirrorSurfaceRows) ==
        std::array<std::int32_t, 9>{20, 20, 20, 20, 128, 128, 128, 128, 200});
  // The original read past a one-entry curve for the last row.
  const std::array<std::uint8_t, 1> single{90};
  CHECK(surfaces::SheetFactors(single, surfaces::kSurfaceRows)[4] == 180);
  CHECK(surfaces::SheetFactors(five, 1)[0] == 255);
  CHECK(surfaces::SheetFactors(five, 1)[1] == 0);
}

TEST_CASE("Sheet quad coordinates repeat four times or span sixteen columns",
          "[effects-surface-templates][effects]") {
  using surfaces::RingUv;
  using surfaces::SurfaceTextureSpan;
  const surfaces::RingQuadUvs repeated =
      surfaces::SheetQuadUvs(kTexture, SurfaceTextureSpan::kFourColumns,
                             surfaces::kSurfaceRows, {.row = 0, .column = 5});
  CHECK(repeated[0] == RingUv{32, 32});
  CHECK(repeated[1] == RingUv{48, 32});
  CHECK(repeated[2] == RingUv{32, 34});
  CHECK(repeated[3] == RingUv{48, 34});
  const surfaces::RingQuadUvs once =
      surfaces::SheetQuadUvs(kTexture, SurfaceTextureSpan::kSixteenColumns,
                             surfaces::kSurfaceRows, {.row = 3, .column = 15});
  CHECK(once[0] == RingUv{76, 38});
  CHECK(once[3] == RingUv{80, 40});
  const surfaces::RingQuadUvs mirrored = surfaces::SheetQuadUvs(
      kTexture, SurfaceTextureSpan::kSixteenColumns,
      surfaces::kMirrorSurfaceRows, {.row = 7, .column = 0});
  CHECK(mirrored[0] == RingUv{16, 39});
  CHECK(mirrored[3] == RingUv{20, 40});
}

TEST_CASE("Sheet row colours scale each intensity by its curve sample",
          "[effects-surface-templates][effects]") {
  surfaces::SheetFrame frame;
  frame.state.factors[0] = 200;
  frame.state.factors[1] = 255;
  frame.color_samples = {64, std::nullopt, 128};
  const surfaces::RingColor first = surfaces::SheetRowColor(frame, 0);
  CHECK(first.r == 100);
  CHECK(first.g == 200);
  CHECK(first.b == 200);
  const surfaces::RingColor second = surfaces::SheetRowColor(frame, 1);
  CHECK(second.r == 127);
  CHECK(second.g == 255);
  CHECK(second.b == 255);
}

TEST_CASE("Grid depth modes pick fixed slots or nudge nearer",
          "[effects-surface-templates][effects]") {
  using surfaces::GridDepth;
  CHECK(GridDepth(500, 0, 10) == 500);
  CHECK(GridDepth(500, 1, 10) == 0x7FE);
  CHECK(GridDepth(500, 2, 10) == -15);
  CHECK(GridDepth(500, 3, 10) == -13);
  CHECK(GridDepth(500, 4, 10) == -11);
  CHECK(GridDepth(500, 5, 10) == 484);
  CHECK(GridDepth(20, 5, 10) == 20);
  CHECK(GridDepth(500, 9, 10) == 500);
}

TEST_CASE("Grid creates take the page and CLUT from the definition",
          "[effects-surface-templates][effects]") {
  FakeGridHost host;
  SurfaceDefinition& definition = host.definition.emplace();
  definition.transparency_curve = 0x0B;
  definition.ring_segments = 0x0A;
  definition.ring_positions = 0x0D;
  definition.grid_u = 7;
  definition.grid_v = 9;
  CHECK(VS::Effects::Plg053::Grid(SurfaceCommand::kCreate, 0x0204, host));
  const GridState state = Present(host.state);
  CHECK(state.lifetime == 4);
  CHECK(state.definition_index == 2);
  CHECK(state.u == 7);
  CHECK(state.v == 9);
  CHECK(state.tpage == 0x7A);
  CHECK(state.clut == 0x3D70);
  CHECK_FALSE(host.animation.has_value());
}

TEST_CASE("Grid packets carry one flat colour and four textured corners",
          "[effects-surface-templates][effects]") {
  const surfaces::GridQuad quad{
      .screens = {11, 12, 13, 14},
      .color = {.r = 1, .g = 2, .b = 3},
      .uvs = {surfaces::RingUv{1, 2}, surfaces::RingUv{3, 4},
              surfaces::RingUv{5, 6}, surfaces::RingUv{7, 8}},
      .clut = 0x3C30,
      .tpage = 0x7A};
  const auto words = surfaces::EncodeGridPacket(quad);
  CHECK(words[0] == 0x0B000000U);
  CHECK(words[1] == surfaces::kRingDrawModeWord);
  CHECK(words[2] == 0x2E030201U);
  CHECK(words[3] == 11);
  CHECK(words[4] == 0x3C300201U);
  CHECK(words[6] == 0x007A0403U);
  CHECK(words[10] == 0x0807U);
  CHECK(words[11] == surfaces::kRingRestoreModeWord);
  CHECK(words[12] == 0);
}

TEST_CASE("Grid ticks lay nine lines each way",
          "[effects-surface-templates][effects]") {
  FakeGridHost host;
  LayGrid(host);
  CHECK(VS::Effects::Plg061::Grid(SurfaceCommand::kTick, 0, host));
  REQUIRE(host.projected.size() == surfaces::kGridPoints);
  CHECK(host.projected.front() == RingPoint{60, 50, 120});
  CHECK(host.projected.at(4) == RingPoint{100, 50, 120});
  CHECK(host.projected.back() == RingPoint{140, 50, 280});
  CHECK(Present(host.state).age == 1);
}

TEST_CASE("Grid ticks draw sixty-four quads",
          "[effects-surface-templates][effects]") {
  FakeGridHost host;
  LayGrid(host);
  CHECK(VS::Effects::Plg061::Grid(SurfaceCommand::kTick, 0, host));
  REQUIRE(host.packets.size() == 64);
  CHECK(host.packets.front().depth == 0x7FE);
  CHECK(Word(host.packets.front(), 2) == 0x2E808080U);
  CHECK(Word(host.packets.front(), 7) == 10);
}

TEST_CASE("Grid texture scroll advances by the definition's step",
          "[effects-surface-templates][effects]") {
  FakeGridHost host;
  ScrollGrid(host);
  CHECK(VS::Effects::Plg073::Grid(SurfaceCommand::kTick, 0, host));
  REQUIRE_FALSE(host.packets.empty());
  CHECK(Word(host.packets.front(), 4) == 0x0810U);
  CHECK(Word(host.packets.front(), 6) == 0x0818U);
  CHECK(Present(host.state).u_scroll == 5);
  CHECK(Present(host.state).v_scroll == 3);
}

TEST_CASE("Grid texture scroll accumulates from tick to tick",
          "[effects-surface-templates][effects]") {
  FakeGridHost host;
  ScrollGrid(host);
  CHECK(VS::Effects::Plg073::Grid(SurfaceCommand::kTick, 0, host));
  host.packets.clear();
  CHECK(VS::Effects::Plg073::Grid(SurfaceCommand::kTick, 0, host));
  REQUIRE_FALSE(host.packets.empty());
  CHECK(Word(host.packets.front(), 4) == 0x0B15U);
  // A zero texture height leaves the scroll unwrapped, as the DIV did.
  CHECK(Present(host.state).v_scroll == 6);
}

TEST_CASE("Grid quads outside the visible depth range are not drawn",
          "[effects-surface-templates][effects]") {
  FakeGridHost host;
  host.state = GridState{.lifetime = 2};
  host.depth = 0x800;
  CHECK(VS::Effects::Plg128::Grid(SurfaceCommand::kTick, 0, host));
  CHECK(host.projected.size() == surfaces::kGridPoints);
  CHECK(host.packets.empty());
  host.depth = 30;
  host.inputs.near_clip = 30;
  CHECK_FALSE(VS::Effects::Plg128::Grid(SurfaceCommand::kTick, 0, host));
  CHECK(host.packets.empty());
  CHECK(Present(host.state).lifetime == 0);
  CHECK(Present(host.published).flags == 0);
}

TEST_CASE("Mesh texture bytes sample the frame's centre quarters and corners",
          "[effects-surface-templates][effects]") {
  const auto bytes = surfaces::Mesh10UvBytes(kTexture);
  CHECK(bytes == std::array<std::uint8_t, surfaces::kMesh10UvBytes>{
                     48, 32, 32, 36, 64, 36, 16, 40, 48, 40, 80, 40});
  const auto wrapped =
      surfaces::Mesh10UvBytes(RingTexture{.u = 250, .v = 250, .w = 12, .h = 8});
  CHECK(wrapped[0] == 0);
  CHECK(wrapped[1] == 250);
  CHECK(wrapped[3] == 254);
  CHECK(wrapped[7] == 2);
  CHECK(wrapped[10] == 6);
}

TEST_CASE("Mesh packets carry three Gouraud corners",
          "[effects-surface-templates][effects]") {
  const surfaces::Mesh10Triangle triangle{
      .screens = {21, 22, 23},
      .colors = {surfaces::RingColor{1, 2, 3}, surfaces::RingColor{4, 5, 6},
                 surfaces::RingColor{7, 8, 9}},
      .uvs = {surfaces::RingUv{1, 2}, surfaces::RingUv{3, 4},
              surfaces::RingUv{5, 6}},
      .clut = 0x1234,
      .tpage = 0x567};
  const auto words = surfaces::EncodeMesh10Packet(triangle);
  CHECK(words[0] == 0x0B000000U);
  CHECK(words[1] == surfaces::kRingDrawModeWord);
  CHECK(words[2] == 0x36030201U);
  CHECK(words[3] == 21);
  CHECK(words[4] == 0x12340201U);
  CHECK(words[7] == 0x05670403U);
  CHECK(words[8] == 0x36090807U);
  CHECK(words[10] == 0x0605U);
  CHECK(words[11] == surfaces::kRingRestoreModeWord);
}

namespace {
// A model whose vertex 1 takes the second colour and vertex 3 an invalid
// one; faces 1 to 3 name an invalid vertex, texture offset and colour.
struct MeshTables {
  std::array<surfaces::Mesh10Vertex, surfaces::kMesh10Vertices> vertices{};
  std::array<surfaces::Mesh10Face, surfaces::kMesh10Faces> faces{};

  MeshTables() {
    vertices[0].point = {100, 200, 300};
    vertices[1].color = 1;
    vertices[3].color = 2;
    vertices[2].point = {32000, 0, 0};
    vertices[5].point = {100, 200, 300};
    faces[0] = {.vertices = {0, 1, 2}, .uvs = {0, 2, 4}};
    faces[1].vertices = {10, 0, 0};
    faces[2].uvs = {11, 0, 0};
    faces[3].vertices = {3, 0, 0};
  }

  [[nodiscard]] surfaces::Mesh10Model Model() const {
    return {.vertices = vertices, .faces = faces};
  }
};

// Anchors a mesh at (1000, 0, 0), its first four vertices at full scale and
// the rest at half, its second colour sampling curves 1 to 3.
void ScaleMesh(FakeMeshHost& host) {
  host.state = Mesh10State{};
  SurfaceDefinition& definition = host.inputs.definition;
  definition.position = Fixed({1000, 0, 0});
  definition.extent = Fixed({kUnit, kUnit, kUnit});
  definition.attraction_jitter = Fixed({kUnit / 2, kUnit / 2, kUnit / 2});
  definition.curves.second_color = {1, 2, 3};
  host.curve_values.at(1) = 10;
  host.curve_values.at(2) = 20;
  host.curve_values.at(3) = 30;
}
}  // namespace

TEST_CASE("Mesh ticks scale move and saturate the model's vertices",
          "[effects-surface-templates][effects]") {
  const MeshTables tables;
  FakeMeshHost host;
  ScaleMesh(host);
  CHECK(VS::Effects::Plg069::Mesh10(SurfaceCommand::kTick, 0, tables.Model(),
                                    host));
  REQUIRE(host.projected.size() == surfaces::kMesh10Vertices);
  CHECK(host.projected[0] == RingPoint{1100, 200, 300});
  CHECK(host.projected[5] == RingPoint{1050, 100, 150});
  CHECK(host.projected[2] == RingPoint{32767, 0, 0});
  CHECK(host.projected[9] == RingPoint{1000, 0, 0});
  const SurfaceScratch scratch = Present(host.published);
  CHECK(scratch.attraction_jitter == Vec3{kUnit / 2, kUnit / 2, kUnit / 2});
  CHECK(scratch.aim.has_value());
  CHECK(host.emissions.empty());
}

TEST_CASE("Mesh ticks draw the valid faces in both colours",
          "[effects-surface-templates][effects]") {
  const MeshTables tables;
  FakeMeshHost host;
  ScaleMesh(host);
  CHECK(VS::Effects::Plg077::Mesh10(SurfaceCommand::kTick, 0, tables.Model(),
                                    host));
  REQUIRE(host.packets.size() == 13);
  const Packet& first = host.packets.front();
  CHECK(first.depth == kSceneDepth);
  CHECK(Word(first, 2) == 0x36808080U);
  CHECK(Word(first, 4) == 0x12342030U);
  CHECK(Word(first, 5) == 0x363C2814U);
  CHECK(Word(first, 6) == 2);
  CHECK(Word(first, 7) == 0x05672420U);
  CHECK(Word(first, 10) == 0x2440U);
}

TEST_CASE("Mesh rotation accumulates the spin under the base direction",
          "[effects-surface-templates][effects]") {
  const MeshTables tables;
  FakeMeshHost host;
  host.state = Mesh10State{.lifetime = 2};
  host.texture.reset();
  SurfaceDefinition& definition = host.inputs.definition;
  definition.direction = Fixed({1, 2, 3});
  definition.direction_jitter = Fixed({16, 32, 48});
  definition.tick_event = 2;
  CHECK(VS::Effects::Plg069::Mesh10(SurfaceCommand::kTick, 0, tables.Model(),
                                    host));
  CHECK_FALSE(VS::Effects::Plg069::Mesh10(SurfaceCommand::kTick, 0,
                                          tables.Model(), host));
  CHECK(Present(host.state).rotation == RingPoint{32, 64, 96});
  CHECK(Present(Present(host.published).aim_angles) == RingPoint{33, 66, 99});
  CHECK(host.packets.empty());
  REQUIRE(host.emissions.size() == 2);
  CHECK(host.emissions.back().event == 1);
}

TEST_CASE("Mesh creates start the definition's texture animation",
          "[effects-surface-templates][effects]") {
  const MeshTables tables;
  FakeMeshHost host;
  SurfaceDefinition& definition = host.definition.emplace();
  definition.texture_index = 6;
  definition.animation_id = 8;
  CHECK(VS::Effects::Plg077::Mesh10(SurfaceCommand::kCreate, 0x0307,
                                    tables.Model(), host));
  CHECK(Present(host.state).lifetime == 7);
  CHECK(Present(host.state).definition_index == 3);
  CHECK(Present(host.animation) == SurfaceAnimation{6, 8});
  CHECK(VS::Effects::Plg077::Mesh10(SurfaceCommand::kExpire, 0, tables.Model(),
                                    host));
  CHECK(Present(host.state).lifetime == 1);
  CHECK_FALSE(VS::Effects::Plg077::Mesh10(SurfaceCommand::kStop, 0,
                                          tables.Model(), host));
}

TEST_CASE("Surface ticks scale the profile by the growing radius and height",
          "[effects-surface-templates][effects]") {
  const auto profile = RowProfile();
  FakeSheetHost host;
  GrowSheet(host);
  CHECK(VS::Effects::Plg061::Surface(SurfaceCommand::kTick, 0, profile, host));
  REQUIRE(host.projected.size() == 80);
  CHECK(host.projected[0] == RingPoint{1100, 2050, 3000});
  CHECK(host.projected[16] == RingPoint{1110, 2055, 3027});
  CHECK(host.projected[64] == RingPoint{1140, 2070, 3140});
}

TEST_CASE("Surface ticks draw a quad per column between neighbouring rows",
          "[effects-surface-templates][effects]") {
  const auto profile = RowProfile();
  FakeSheetHost host;
  GrowSheet(host);
  CHECK(VS::Effects::Plg061::Surface(SurfaceCommand::kTick, 0, profile, host));
  REQUIRE(host.packets.size() == 64);
  CHECK(Word(host.packets.front(), 2) == 0x3EC8C8C8U);
  CHECK(Word(host.packets.front(), 4) == 0x12342010U);
  CHECK(Word(host.packets.front(), 9) == 17);
  CHECK(host.packets.front().depth == kSceneDepth);
}

TEST_CASE("Surface quads close each row onto its first column",
          "[effects-surface-templates][effects]") {
  const auto profile = RowProfile();
  FakeSheetHost host;
  GrowSheet(host);
  CHECK(VS::Effects::Plg077::Surface(SurfaceCommand::kTick, 0, profile, host));
  REQUIRE(host.packets.size() == 64);
  const Packet& last = host.packets.at(15);
  CHECK(Word(last, 3) == 16);
  CHECK(Word(last, 6) == 1);
  CHECK(Word(last, 9) == 32);
  CHECK(Word(last, 12) == 17);
  // Four copies of the texture: column 15 is the last quarter of a copy.
  CHECK(Word(last, 4) == 0x12342040U);
  CHECK(Word(last, 7) == 0x05672050U);
  CHECK_FALSE(Present(host.effect_view));
}

TEST_CASE("Mirrored surfaces walk the profile in and back out flipped",
          "[effects-surface-templates][effects]") {
  const auto profile = RowProfile();
  FakeSheetHost host;
  GrowSheet(host);
  host.inputs.definition.flags = surfaces::kRingEffectView;
  CHECK(VS::Effects::Plg146::MirrorSurface(SurfaceCommand::kTick, 0, profile,
                                           host));
  REQUIRE(host.projected.size() == 144);
  CHECK(host.projected[0] == RingPoint{1100, 2050, 3100});
  CHECK(host.projected[64] == RingPoint{1140, 2070, 3000});
  CHECK(host.projected[128] == RingPoint{1180, 1910, 3180});
  CHECK(host.packets.size() == 128);
  CHECK(Present(host.effect_view));
  CHECK(Present(host.state).age == 1);
}

TEST_CASE("Sheet ticks without a texture project but draw nothing",
          "[effects-surface-templates][effects]") {
  const auto profile = RowProfile();
  FakeSheetHost host;
  GrowSheet(host);
  host.texture.reset();
  CHECK(VS::Effects::Plg158::MirrorSurface(SurfaceCommand::kTick, 0, profile,
                                           host));
  CHECK(host.projected.size() == 144);
  CHECK(host.packets.empty());
  CHECK(Present(host.published).position == Vec3{1000, 2000, 3000});
  CHECK(Present(host.state).radius == 0);
  host.refuse_tick = true;
  CHECK_FALSE(VS::Effects::Plg158::MirrorSurface(SurfaceCommand::kTick, 0,
                                                 profile, host));
  CHECK(Present(host.state).age == 1);
}

TEST_CASE("Sheet creates read the transparency curve once",
          "[effects-surface-templates][effects]") {
  const auto profile = RowProfile();
  FakeSheetHost host;
  SurfaceDefinition& definition = host.definition.emplace();
  definition.transparency_curve = 5;
  definition.ring_segments = 7;
  definition.ring_positions = 9;
  host.factor_curve = {10, 64, 100, 128, 30};
  CHECK(VS::Effects::Plg087::Surface(SurfaceCommand::kCreate, 0x0203, profile,
                                     host));
  CHECK(host.factor_requests == std::vector<std::uint8_t>{5});
  CHECK(Present(host.state).factors ==
        std::array<std::int32_t, 9>{20, 128, 200, 255, 60, 0, 0, 0, 0});
  CHECK(Present(host.state).lifetime == 3);
  CHECK(Present(host.animation) == SurfaceAnimation{7, 9});
  definition.transparency_curve = 0;
  CHECK(VS::Effects::Plg166::MirrorSurface(SurfaceCommand::kCreate, 0, profile,
                                           host));
  CHECK(host.factor_requests.size() == 1);
  CHECK(Present(host.state).factors[8] == 255);
}

TEST_CASE("Sheet commands expire stop and refuse missing definitions",
          "[effects-surface-templates][effects]") {
  const auto profile = RowProfile();
  FakeSheetHost host;
  host.definition.reset();
  CHECK(VS::Effects::Plg061::Surface(SurfaceCommand::kCreate, 0x0105, profile,
                                     host));
  CHECK_FALSE(host.created);
  CHECK(
      VS::Effects::Plg061::Surface(SurfaceCommand::kExpire, 0, profile, host));
  CHECK_FALSE(host.state.has_value());
  CHECK_FALSE(
      VS::Effects::Plg061::Surface(SurfaceCommand::kTick, 0, profile, host));
  host.state = SheetState{.lifetime = 9};
  CHECK_FALSE(
      VS::Effects::Plg061::Surface(SurfaceCommand::kStop, 0, profile, host));
  CHECK(Present(host.state).lifetime == 9);
}

TEST_CASE("Sheet commands outside the four keep the slot and its state",
          "[effects-surface-templates][effects]") {
  const auto unknown = static_cast<SurfaceCommand>(GENERATE(0U, 5U, 9U));
  const auto profile = RowProfile();
  FakeSheetHost host;
  host.state = SheetState{.lifetime = 9};
  CHECK(VS::Effects::Plg061::Surface(unknown, 0, profile, host));
  CHECK(Present(host.state).lifetime == 9);
  CHECK(host.projected.empty());
}

TEST_CASE("Rotating surface steps pick a turn within the speed range",
          "[effects-surface-templates][effects]") {
  FakeSheetHost host;
  CHECK(surfaces::RotatingSurfaceStep(Vec2{5, 5}, host) == 5);
  host.rand_value = 10;
  CHECK(surfaces::RotatingSurfaceStep(Vec2{2, 8}, host) == 5);
  CHECK(surfaces::RotatingSurfaceStep(Vec2{8, 2}, host) == 5);
  host.rand_value = -10;
  CHECK(surfaces::RotatingSurfaceStep(Vec2{2, 8}, host) == -1);
  host.rand_value = 6;
  CHECK(surfaces::RotatingSurfaceStep(Vec2{-3, 0}, host) == -1);
}

TEST_CASE("Rotating surface rows repeat a quarter turn four times",
          "[effects-surface-templates][effects]") {
  surfaces::SheetFrame frame;
  frame.scratch.position = {1000, 2000, 3000};
  frame.scratch.direction_jitter = Vec3{kQuarterTurn, 0, 0};
  frame.radius = 400;
  frame.height = 50;
  const surfaces::SheetPoints points =
      surfaces::RotatingSurfacePoints(frame, 0);
  CHECK(points[0] == RingPoint{1400, 2000, 3000});
  CHECK(points[4] == RingPoint{1000, 2000, 3400});
  CHECK(points[8] == RingPoint{600, 2000, 3000});
  CHECK(points[12] == RingPoint{1000, 2000, 2600});
  CHECK(points[16] == RingPoint{1000, 2050, 3400});
  CHECK(points[32] == RingPoint{600, 2100, 3000});
  CHECK(frame.state.angle == 0);
}

TEST_CASE("Rotating surface ticks turn the sheet by the sampled speed",
          "[effects-surface-templates][effects]") {
  FakeSheetHost host;
  GrowSheet(host);
  host.inputs.definition.speed = {ScalarSpan{2, 2}, ScalarSpan{8, 8}};
  host.rand_value = 10;
  CHECK(VS::Effects::Plg073::RotatingSurface(SurfaceCommand::kTick, 0, host));
  CHECK(Present(host.state).angle == 5);
  CHECK(host.projected.size() == 80);
  CHECK(host.packets.size() == 64);
  CHECK(Present(Present(host.published).speed) == Vec2{2, 8});
  CHECK(VS::Effects::Plg128::RotatingSurface(SurfaceCommand::kTick, 0, host));
  CHECK(Present(host.state).angle == 10);
  CHECK(Present(host.state).age == 2);
}

namespace {
// Points Effects_package at an empty package while it lives.
class BlankLivePackage {
 public:
  BlankLivePackage() : previous_(Effects_package) {
    Effects_package = &blank_;
    VS::Effects::ForgetLivePackageExtents();
  }
  BlankLivePackage(BlankLivePackage&&) = delete;
  BlankLivePackage(const BlankLivePackage&) = delete;
  BlankLivePackage& operator=(BlankLivePackage&&) = delete;
  BlankLivePackage& operator=(const BlankLivePackage&) = delete;
  ~BlankLivePackage() { Effects_package = previous_; }

 private:
  Effects_EffectPackage blank_{};
  Effects_EffectPackage* previous_;
};
}  // namespace

TEST_CASE("Live surface entries refuse definitions the package lacks",
          "[effects-surface-templates][effects]") {
  const BlankLivePackage package;
  VS::test_support::ChannelWarnings warnings("effects");
  Effects_RendererSlot slot{};
  CHECK(surfaces::UpdateLivePlg053Grid(&slot, 1, 0x0105) == 1);
  CHECK(surfaces::UpdateLivePlg061Surface(&slot, 1, 0x0205) == 1);
  CHECK(surfaces::UpdateLivePlg069Mesh10(&slot, 1, 0x0305) == 1);
  CHECK(surfaces::UpdateLivePlg146MirrorSurface(&slot, 1, 0x0405) == 1);
  CHECK(slot.rendererState == nullptr);
  REQUIRE(warnings.messages.size() == 4);
  CHECK(warnings.messages.front().find("grid: definition 1") !=
        std::string::npos);
  CHECK(warnings.messages.back().find("definition 4") != std::string::npos);
}

TEST_CASE("Live surface entries answer commands without a state",
          "[effects-surface-templates][effects]") {
  const BlankLivePackage package;
  Effects_RendererSlot slot{};
  CHECK(surfaces::UpdateLivePlg073RotatingSurface(&slot, 3, 0) == 1);
  CHECK(surfaces::UpdateLivePlg073RotatingSurface(&slot, 9, 0) == 1);
  CHECK(surfaces::UpdateLivePlg073RotatingSurface(&slot, 2, 0) == 0);
  CHECK(surfaces::UpdateLivePlg073RotatingSurface(&slot, 4, 0) == 0);
  CHECK(surfaces::UpdateLivePlg077Mesh10(&slot, 2, 0) == 0);
  CHECK(surfaces::UpdateLivePlg300MirrorSurface(&slot, 3, 0) == 1);
  CHECK(surfaces::UpdateLivePlg061Grid(nullptr, 2, 0) == 0);
  CHECK(slot.rendererState == nullptr);
}
