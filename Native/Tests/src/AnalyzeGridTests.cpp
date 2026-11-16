#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Math/Interpolation.hpp"
#include "Effects/AnalyzeGrid.hpp"

namespace {
namespace effects = VS::Effects;
namespace grid = effects::Templates;
using VS::Core::Math::ScalarSpan;
using VS::Core::Math::Vec2;
using VS::Core::Math::Vec3;
using Command = effects::AnalyzeGridCommand;
using Entry = bool (*)(Command, effects::AnalyzeGridPlan,
                       effects::AnalyzeGridHost&);

class GridWorld final : public effects::AnalyzeGridHost {
 public:
  std::optional<grid::SurfaceDefinition> definition = grid::SurfaceDefinition{};
  grid::SurfaceTickInputs inputs{};
  std::optional<effects::AnalyzeGridState> state;
  std::optional<grid::SurfaceScratch> scratch;
  std::array<std::int32_t, 256> curves{};
  std::vector<grid::RingPoint> points;
  std::vector<effects::AnalyzeGridQuad> quads;
  std::int32_t depth = 100;
  std::uint8_t requested_definition = 0;
  bool allocate = true;
  bool tick = true;
  bool effect_view = false;

  std::optional<grid::SurfaceDefinition> CreateDefinition(
      effects::AnalyzeGridDefinitionId id) override {
    requested_definition = id.value;
    return definition;
  }
  bool Create(const effects::AnalyzeGridState& value) override {
    if (!allocate) return false;
    state = value;
    return true;
  }
  std::optional<effects::AnalyzeGridState> State() const override {
    return state;
  }
  void StoreState(const effects::AnalyzeGridState& value) override {
    state = value;
  }
  std::optional<grid::SurfaceTickInputs> BeginTick(
      effects::AnalyzeGridDefinitionId id) override {
    requested_definition = id.value;
    return tick ? std::optional(inputs) : std::nullopt;
  }
  std::int32_t SampleCurve(std::uint8_t curve, std::int32_t) override {
    return curves.at(curve);
  }
  void PublishScratch(const grid::SurfaceScratch& values) override {
    scratch = values;
  }
  void BeginProjection(bool value) override { effect_view = value; }
  effects::AnalyzeGridProjection Project(grid::RingPoint point) override {
    points.push_back(point);
    return {{point.x, point.z}, depth};
  }
  void Draw(const effects::AnalyzeGridQuad& quad) override {
    quads.push_back(quad);
  }
};

template <typename Value>
Value Present(const std::optional<Value>& value) {
  if (!value.has_value()) throw std::logic_error("missing grid test value");
  return *value;
}

grid::RingVectorRange Fixed(Vec3 value) {
  return {ScalarSpan{value.x, value.x}, ScalarSpan{value.y, value.y},
          ScalarSpan{value.z, value.z}};
}

void CheckCreate(Entry update) {
  GridWorld world;
  grid::SurfaceDefinition definition;
  definition.transparency_curve = 15;
  definition.ring_segments = 10;
  definition.ring_positions = 10;
  definition.grid_u = 241;
  definition.grid_v = 253;
  world.definition = definition;
  world.state = effects::AnalyzeGridState{.x_spacing = -17, .age = 99};
  REQUIRE(update(Command::kCreate, {2, {255}}, world));
  const auto state = Present(world.state);
  CHECK(world.requested_definition == 255);
  CHECK(state == effects::AnalyzeGridState{.lifetime = 2,
                                           .definition_index = 255,
                                           .u = 241,
                                           .v = 253,
                                           .tpage = 0xFA,
                                           .clut = 0x3CB0});
  CHECK(world.points.empty());
  CHECK(world.quads.empty());
  CHECK_FALSE(world.scratch.has_value());
}

void ConfigureMotion(GridWorld& world) {
  world.state = effects::AnalyzeGridState{.definition_index = 7};
  auto& definition = world.inputs.definition;
  definition.position = Fixed({100, 50, 200});
  definition.extent = Fixed({10, 20, 0});
  definition.force_min = Fixed({2, 3, 0});
  definition.force_max = Fixed({4, 6, 0});
  definition.acceleration_min = Fixed({1, 2, 0});
  definition.acceleration_max = Fixed({1, 2, 0});
}

void CheckMotion(Entry update) {
  GridWorld world;
  ConfigureMotion(world);
  REQUIRE(update(Command::kTick, {}, world));
  REQUIRE(world.points.size() == 81);
  CHECK(world.points.front() == grid::RingPoint{20, 50, 56});
  CHECK(world.points.at(40) == grid::RingPoint{100, 50, 200});
  CHECK(world.points.back() == grid::RingPoint{180, 50, 344});
  CHECK(Present(world.state).x_spacing == 3);
  CHECK(Present(world.state).z_spacing == 5);
  CHECK(world.requested_definition == 7);
  CHECK(world.quads.size() == 64);
}

void CheckDepth(Entry update) {
  const int depth = GENERATE(12, 13, 28, 29, 2047, 2048);
  const int mode = GENERATE(-1, 0, 1, 2, 3, 4, 5, 6);
  GridWorld world;
  world.state = effects::AnalyzeGridState{};
  world.depth = depth;
  world.inputs.near_clip = 12;
  world.inputs.definition.inner_extent = {mode, mode};
  REQUIRE(update(Command::kTick, {}, world));
  const bool visible = depth > 12 && depth < 2048;
  REQUIRE(world.quads.size() == (visible ? 64 : 0));
  if (!visible) return;
  int expected = depth;
  if (mode >= 1 && mode <= 4) {
    constexpr std::array<int, 4> kSlots{2046, -15, -13, -11};
    expected = kSlots.at(static_cast<std::size_t>(mode - 1));
  } else if (mode == 5 && depth > 28) {
    expected -= 16;
  }
  CHECK(world.quads.front().depth == expected);
  CHECK(world.quads.back().depth == expected);
}

void ConfigureTexture(GridWorld& world) {
  world.state = effects::AnalyzeGridState{.u = 250,
                                          .v = 3,
                                          .u_scroll = 10,
                                          .v_scroll = 255,
                                          .tpage = 0xFA,
                                          .clut = 0x3CB0};
  auto& definition = world.inputs.definition;
  definition.grid_width = 64;
  definition.grid_height = -64;
  definition.gravity = {ScalarSpan{-20, -20}, ScalarSpan{1, 1}};
  definition.curves.color = {1, 2, 3};
  world.curves.at(1) = 127;
  world.curves.at(2) = 128;
}

void CheckTexture(Entry update) {
  GridWorld world;
  ConfigureTexture(world);
  REQUIRE(update(Command::kTick, {}, world));
  REQUIRE(world.quads.size() == 64);
  const auto& first = world.quads.front();
  CHECK(first.color == grid::RingColor{254, 255, 0});
  CHECK(first.texture_coordinates ==
        grid::RingQuadUvs{grid::RingUv{4, 2}, grid::RingUv{12, 2},
                          grid::RingUv{4, 250}, grid::RingUv{12, 250}});
  CHECK(world.quads.back().texture_coordinates.back() == grid::RingUv{68, 194});
  CHECK(Present(world.state).u_scroll == 246);
  CHECK(Present(world.state).v_scroll == 0);
  CHECK(first.palette == 0x3CB0);
  CHECK(first.texture_page == 0xFA);
}

void CheckAnchorsAndWrapping(Entry update) {
  const int anchor = GENERATE(0, 1, 2, 3);
  GridWorld world;
  world.state = effects::AnalyzeGridState{
      .x_spacing = 32767, .x_speed = 32767, .x_phase = 32767};
  world.inputs.definition.flags =
      grid::kRingEffectView | static_cast<std::uint32_t>(anchor);
  world.inputs.definition.position = Fixed({1, 2, 3});
  world.inputs.definition.acceleration_min = Fixed({1, 0, 0});
  world.inputs.definition.force_min = Fixed({2, 0, 0});
  world.inputs.definition.direction_jitter = Fixed({1, 0, 0});
  world.inputs.effect_origin = {10, 20, 30};
  world.inputs.actor_translation = {100, 200, 300};
  REQUIRE(update(Command::kTick, {}, world));
  CHECK(Present(world.state).x_speed == -32768);
  CHECK(Present(world.state).x_spacing == 1);
  CHECK(Present(world.state).x_phase == -32768);
  CHECK(world.effect_view);
  const Vec3 expected = anchor == 1   ? Vec3{11, 22, 33}
                        : anchor == 2 ? Vec3{101, 202, 303}
                                      : Vec3{1, 2, 3};
  CHECK(Present(world.scratch).position == expected);
}

void CheckLifecycle(Entry update) {
  GridWorld world;
  world.state = effects::AnalyzeGridState{
      .age = std::numeric_limits<std::int32_t>::max()};
  REQUIRE(update(Command::kTick, {}, world));
  CHECK(Present(world.state).age == std::numeric_limits<std::int32_t>::min());
  REQUIRE(update(Command::kExpire, {}, world));
  CHECK(Present(world.state).lifetime == 1);
  CHECK_FALSE(update(Command::kTick, {}, world));
  CHECK(world.quads.size() == 128);
  const auto finished = world.state;
  CHECK_FALSE(update(Command::kStop, {}, world));
  CHECK(world.state == finished);
}

void CheckRefusals(Entry update) {
  GridWorld world;
  world.definition.reset();
  CHECK_FALSE(update(Command::kCreate, {}, world));
  CHECK_FALSE(world.state.has_value());
  world.definition = grid::SurfaceDefinition{};
  world.allocate = false;
  CHECK_FALSE(update(Command::kCreate, {}, world));
  CHECK_FALSE(update(Command::kTick, {}, world));
  CHECK_FALSE(update(Command::kExpire, {}, world));
  world.state = effects::AnalyzeGridState{.lifetime = 5, .age = 13};
  const auto saved = world.state;
  world.tick = false;
  CHECK_FALSE(update(Command::kTick, {}, world));
  CHECK(world.state == saved);
  CHECK(world.points.empty());
  CHECK_FALSE(world.scratch.has_value());
}
}  // namespace

TEST_CASE("F01893 Analyze create resets the selected definition",
          "[effects-analyze-grid][F01893]") {
  CheckCreate(effects::Plg005::UpdateAnalyzeGrid);
}
TEST_CASE("F01893 Analyze draws the accelerating lattice",
          "[effects-analyze-grid][F01893]") {
  CheckMotion(effects::Plg005::UpdateAnalyzeGrid);
}
TEST_CASE("F01893 Analyze preserves all depth modes and clip edges",
          "[effects-analyze-grid][F01893]") {
  CheckDepth(effects::Plg005::UpdateAnalyzeGrid);
}
TEST_CASE("F01893 Analyze preserves signed scrolling and curve colors",
          "[effects-analyze-grid][F01893]") {
  CheckTexture(effects::Plg005::UpdateAnalyzeGrid);
}
TEST_CASE("F01893 Analyze wraps halfwords and selects anchors",
          "[effects-analyze-grid][F01893]") {
  CheckAnchorsAndWrapping(effects::Plg005::UpdateAnalyzeGrid);
}
TEST_CASE(
    "F01893 Analyze expires after its final draw and leaves cleanup to its "
    "owner",
    "[effects-analyze-grid][F01893]") {
  CheckLifecycle(effects::Plg005::UpdateAnalyzeGrid);
}
TEST_CASE("F01893 Analyze refuses unavailable inputs without partial state",
          "[effects-analyze-grid][F01893]") {
  CheckRefusals(effects::Plg005::UpdateAnalyzeGrid);
}

TEST_CASE("F01926 Analyze create resets the selected definition",
          "[effects-analyze-grid][F01926]") {
  CheckCreate(effects::Plg054::UpdateAnalyzeGrid);
}
TEST_CASE("F01926 Analyze draws the accelerating lattice",
          "[effects-analyze-grid][F01926]") {
  CheckMotion(effects::Plg054::UpdateAnalyzeGrid);
}
TEST_CASE("F01926 Analyze preserves all depth modes and clip edges",
          "[effects-analyze-grid][F01926]") {
  CheckDepth(effects::Plg054::UpdateAnalyzeGrid);
}
TEST_CASE("F01926 Analyze preserves signed scrolling and curve colors",
          "[effects-analyze-grid][F01926]") {
  CheckTexture(effects::Plg054::UpdateAnalyzeGrid);
}
TEST_CASE("F01926 Analyze wraps halfwords and selects anchors",
          "[effects-analyze-grid][F01926]") {
  CheckAnchorsAndWrapping(effects::Plg054::UpdateAnalyzeGrid);
}
TEST_CASE(
    "F01926 Analyze expires after its final draw and leaves cleanup to its "
    "owner",
    "[effects-analyze-grid][F01926]") {
  CheckLifecycle(effects::Plg054::UpdateAnalyzeGrid);
}
TEST_CASE("F01926 Analyze refuses unavailable inputs without partial state",
          "[effects-analyze-grid][F01926]") {
  CheckRefusals(effects::Plg054::UpdateAnalyzeGrid);
}

TEST_CASE("F01928 Analyze create resets the selected definition",
          "[effects-analyze-grid][F01928]") {
  CheckCreate(effects::Plg055::UpdateAnalyzeGrid);
}
TEST_CASE("F01928 Analyze draws the accelerating lattice",
          "[effects-analyze-grid][F01928]") {
  CheckMotion(effects::Plg055::UpdateAnalyzeGrid);
}
TEST_CASE("F01928 Analyze preserves all depth modes and clip edges",
          "[effects-analyze-grid][F01928]") {
  CheckDepth(effects::Plg055::UpdateAnalyzeGrid);
}
TEST_CASE("F01928 Analyze preserves signed scrolling and curve colors",
          "[effects-analyze-grid][F01928]") {
  CheckTexture(effects::Plg055::UpdateAnalyzeGrid);
}
TEST_CASE("F01928 Analyze wraps halfwords and selects anchors",
          "[effects-analyze-grid][F01928]") {
  CheckAnchorsAndWrapping(effects::Plg055::UpdateAnalyzeGrid);
}
TEST_CASE(
    "F01928 Analyze expires after its final draw and leaves cleanup to its "
    "owner",
    "[effects-analyze-grid][F01928]") {
  CheckLifecycle(effects::Plg055::UpdateAnalyzeGrid);
}
TEST_CASE("F01928 Analyze refuses unavailable inputs without partial state",
          "[effects-analyze-grid][F01928]") {
  CheckRefusals(effects::Plg055::UpdateAnalyzeGrid);
}

TEST_CASE("F01996 Analyze create resets the selected definition",
          "[effects-analyze-grid][F01996]") {
  CheckCreate(effects::Plg074::UpdateAnalyzeGrid);
}
TEST_CASE("F01996 Analyze draws the accelerating lattice",
          "[effects-analyze-grid][F01996]") {
  CheckMotion(effects::Plg074::UpdateAnalyzeGrid);
}
TEST_CASE("F01996 Analyze preserves all depth modes and clip edges",
          "[effects-analyze-grid][F01996]") {
  CheckDepth(effects::Plg074::UpdateAnalyzeGrid);
}
TEST_CASE("F01996 Analyze preserves signed scrolling and curve colors",
          "[effects-analyze-grid][F01996]") {
  CheckTexture(effects::Plg074::UpdateAnalyzeGrid);
}
TEST_CASE("F01996 Analyze wraps halfwords and selects anchors",
          "[effects-analyze-grid][F01996]") {
  CheckAnchorsAndWrapping(effects::Plg074::UpdateAnalyzeGrid);
}
TEST_CASE(
    "F01996 Analyze expires after its final draw and leaves cleanup to its "
    "owner",
    "[effects-analyze-grid][F01996]") {
  CheckLifecycle(effects::Plg074::UpdateAnalyzeGrid);
}
TEST_CASE("F01996 Analyze refuses unavailable inputs without partial state",
          "[effects-analyze-grid][F01996]") {
  CheckRefusals(effects::Plg074::UpdateAnalyzeGrid);
}
