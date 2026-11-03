#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/NavigationPrimitives.hpp"
#include "Field/TerrainHeight.hpp"

namespace {
namespace ai = VS::Battle::Ai;
constexpr int kSide = ai::kNavigationGridSide;
using Plane = std::array<std::array<std::uint16_t, kSide>, kSide>;

// Neighbor steps for directions 0..7: x in bits 0-1 and z in bits 4-5.
using Codes = ai::NavigationDirectionCodes;
constexpr Codes kDirections{0x30, 0x33, 0x03, 0x13, 0x10, 0x11, 0x01, 0x31};

struct FakeGrid final : ai::NavigationGrid {
  bool has_flags = true;
  bool has_heights = true;
  Plane flags{};
  Plane heights{};

  [[nodiscard]] bool HasTileFlags() const override { return has_flags; }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return flags.at(z).at(x);
  }
  void SetTileFlags(int x, int z, std::uint16_t value) override {
    flags.at(z).at(x) = value;
  }
  [[nodiscard]] bool HasOccupancyHeights() const override {
    return has_heights;
  }
  [[nodiscard]] std::uint16_t OccupancyHeight(int x, int z) const override {
    return heights.at(z).at(x);
  }
  void SetOccupancyHeight(int x, int z, std::uint16_t value) override {
    heights.at(z).at(x) = value;
  }
};

struct TerrainFixture {
  std::array<VS::Field::TerrainRow, kSide> rows{};

  [[nodiscard]] VS::Field::TerrainGrid Grid() const {
    return VS::Field::TerrainGrid{
        std::span<const VS::Field::TerrainRow, kSide>(rows)};
  }
};
}  // namespace

TEST_CASE("AI tile reservations set flags inside the grid only",
          "[ai][navigation]") {
  FakeGrid grid;
  grid.flags[4][3] = 0x11;
  CHECK(ai::ReserveAiDestinationTile(grid, 3, 4));
  CHECK(grid.flags[4][3] == 0x91);
  CHECK(ai::MarkAiTileDynamicObstacle(grid, 3, 4));
  CHECK(grid.flags[4][3] == 0x491);
  CHECK(ai::ReserveAiDestinationTile(grid, 31, 31));
  CHECK(grid.flags[31][31] == ai::kReservedTileFlag);
}

TEST_CASE("AI tile reservations refuse tiles outside the grid",
          "[ai][navigation]") {
  FakeGrid grid;
  const Plane before = grid.flags;
  CHECK_FALSE(ai::ReserveAiDestinationTile(grid, -1, 0));
  CHECK_FALSE(ai::ReserveAiDestinationTile(grid, 32, 0));
  CHECK_FALSE(ai::MarkAiTileDynamicObstacle(grid, 0, 32));
  CHECK_FALSE(ai::MarkAiTileDynamicObstacle(grid, 0, -1));
  CHECK(grid.flags == before);

  grid.has_flags = false;
  CHECK(ai::ReserveAiDestinationTile(grid, 40, 40));
  CHECK(ai::MarkAiTileDynamicObstacle(grid, 5, 5));
  CHECK(grid.flags == before);
}

TEST_CASE("AI tile occupants change the height under their byte offset",
          "[ai][navigation]") {
  FakeGrid grid;
  CHECK(ai::AddAiTileOccupant(grid, {.x = 3, .layer = 3, .z = 4}));
  CHECK(ai::AddAiTileOccupant(grid, {.x = 3, .layer = 0, .z = 4}));
  CHECK(grid.heights[4][3] == 2 * ai::kDynamicObstacleFlag);
  CHECK(ai::RemoveAiTileOccupant(grid, {.x = 3, .layer = 0, .z = 4}));
  CHECK(grid.heights[4][3] == ai::kDynamicObstacleFlag);
}

TEST_CASE("AI tile occupants wrap and address by byte offset",
          "[ai][navigation]") {
  FakeGrid grid;
  // Removal wraps at 16 bits, as the original halfword store did.
  CHECK(ai::RemoveAiTileOccupant(grid, {.x = 0, .layer = 0, .z = 0}));
  CHECK(grid.heights[0][0] == 0xFC00);
  CHECK(ai::AddAiTileOccupant(grid, {.x = 0, .layer = 0, .z = 0}));
  CHECK(grid.heights[0][0] == 0);

  // Upper layer bits add a whole cell; x past the row reaches the next row.
  CHECK(ai::AddAiTileOccupant(grid, {.x = 5, .layer = 8, .z = 6}));
  CHECK(grid.heights[6][6] == ai::kDynamicObstacleFlag);
  CHECK(ai::AddAiTileOccupant(grid, {.x = 32, .layer = 0, .z = 0}));
  CHECK(grid.heights[1][0] == ai::kDynamicObstacleFlag);
}

TEST_CASE("AI tile occupants refuse invalid offsets and missing maps",
          "[ai][navigation]") {
  FakeGrid grid;
  const Plane before = grid.heights;
  CHECK_FALSE(ai::AddAiTileOccupant(grid, {.x = 3, .layer = 4, .z = 4}));
  CHECK_FALSE(ai::AddAiTileOccupant(grid, {.x = 31, .layer = 8, .z = 31}));
  CHECK_FALSE(ai::RemoveAiTileOccupant(grid, {.x = 0, .layer = 0, .z = 32}));
  CHECK_FALSE(
      ai::RemoveAiTileOccupant(grid, {.x = 255, .layer = 252, .z = 255}));
  CHECK(ai::AddAiTileOccupant(grid, {.x = 31, .layer = 0, .z = 31}));
  CHECK(grid.heights[31][31] == ai::kDynamicObstacleFlag);
}

TEST_CASE("AI tile occupants skip missing occupancy maps", "[ai][navigation]") {
  FakeGrid grid;
  const Plane before = grid.heights;
  grid.has_heights = false;
  CHECK_FALSE(ai::AddAiTileOccupant(grid, {.x = 1, .layer = 0, .z = 1}));
  grid.has_flags = false;
  CHECK(ai::AddAiTileOccupant(grid, {.x = 1, .layer = 0, .z = 1}));
  CHECK(ai::RemoveAiTileOccupant(grid, {.x = 255, .layer = 4, .z = 255}));
  CHECK(grid.heights == before);
}

TEST_CASE("AI adjacent steps block edges, voids and tall rises",
          "[ai][navigation]") {
  TerrainFixture terrain;
  terrain.rows[5][6] = 1;      // 64 above its neighbors.
  terrain.rows[5][3] = 0x3FF;  // Negative height: no ground.
  const auto grid = terrain.Grid();
  CHECK(ai::IsAdjacentTileStepBlocked(grid, kDirections, 5, 5, 6) == true);
  CHECK(ai::IsAdjacentTileStepBlocked(grid, kDirections, 6, 5, 2) == true);
  CHECK(ai::IsAdjacentTileStepBlocked(grid, kDirections, 4, 5, 2) == true);
  CHECK(ai::IsAdjacentTileStepBlocked(grid, kDirections, 5, 5, 0) == false);
  CHECK(ai::IsAdjacentTileStepBlocked(grid, kDirections, 5, 5, 4) == false);
  CHECK(ai::IsAdjacentTileStepBlocked(grid, kDirections, 5, 5, 3) == false);
  CHECK(ai::IsAdjacentTileStepBlocked(grid, kDirections, 0, 0, 1) == true);
  CHECK(ai::IsAdjacentTileStepBlocked(grid, kDirections, 31, 31, 5) == true);
  CHECK(ai::IsAdjacentTileStepBlocked(grid, kDirections, 31, 0, 7) == true);
}

TEST_CASE("AI adjacent steps refuse invalid directions and tiles",
          "[ai][navigation]") {
  TerrainFixture terrain;
  const auto grid = terrain.Grid();
  CHECK_FALSE(ai::IsAdjacentTileStepBlocked(grid, kDirections, 5, 5, -1));
  CHECK_FALSE(ai::IsAdjacentTileStepBlocked(grid, kDirections, 5, 5, 8));
  CHECK_FALSE(ai::IsAdjacentTileStepBlocked(grid, kDirections, -1, 5, 0));
  CHECK_FALSE(ai::IsAdjacentTileStepBlocked(grid, kDirections, 5, 32, 0));
  auto codes = kDirections;
  codes[0] = 0x00;  // No step.
  codes[1] = 0x02;  // A two-tile x step.
  codes[2] = 0x20;  // A two-tile z step.
  CHECK_FALSE(ai::IsAdjacentTileStepBlocked(grid, codes, 5, 5, 0));
  CHECK_FALSE(ai::IsAdjacentTileStepBlocked(grid, codes, 5, 5, 1));
  CHECK_FALSE(ai::IsAdjacentTileStepBlocked(grid, codes, 5, 5, 2));
}

namespace {
struct FakeBuilder final : ai::SpecialTerrainBuilder {
  bool has_maps = true;
  int builds = 0;
  [[nodiscard]] bool HasNativeMaps() const override { return has_maps; }
  void BuildSpecialTerrainHeights() override { ++builds; }
};
}  // namespace

TEST_CASE("AI terrain heights build only with native maps",
          "[ai][navigation]") {
  FakeBuilder builder;
  CHECK(ai::BuildAiTerrainHeightsWithWorkspace(builder));
  CHECK(builder.builds == 1);
  builder.has_maps = false;
  CHECK_FALSE(ai::BuildAiTerrainHeightsWithWorkspace(builder));
  CHECK(builder.builds == 1);
}

namespace {
constexpr std::uint32_t kFirstGate = 0x400;
constexpr std::uint32_t kSecondGate = 0x04000000;
constexpr std::uint32_t kFirstClosed = 0x8000;
constexpr std::uint32_t kSecondClosed = 0x80000000;

// A first-gate word at tile (3, 4) selecting platform bit `selector`.
constexpr std::uint32_t FirstGateWord(std::uint32_t selector) {
  return kFirstGate | (3U << 5U) | 4U | (selector << 13U);
}

struct RouteFixture {
  std::array<std::uint32_t, ai::kRouteWordCount> routes{};
  std::array<ai::RouteGateEnd, ai::kRouteGateEndCount> ends{};

  bool Update(std::uint32_t visible) {
    return ai::UpdateRouteGateAvailability(routes, ends, visible);
  }
};
}  // namespace

TEST_CASE("AI visible platform masks wrap their indices at 32",
          "[ai][navigation]") {
  const std::array<std::int16_t, 4> indices{0, 5, 33, -1};
  CHECK(ai::VisiblePlatformMask(indices) == 0x80000023U);
  CHECK(ai::VisiblePlatformMask({}) == 0);
}

TEST_CASE("AI route gates close unless their platform is visible",
          "[ai][navigation]") {
  RouteFixture fixture;
  fixture.routes[1] = 0x1234;
  fixture.routes[2] = FirstGateWord(1);
  fixture.ends[0] = {.x = 3, .z = 4};
  REQUIRE(fixture.Update(0));
  CHECK(fixture.routes[0] == 0);
  CHECK(fixture.routes[1] == 0x1234);
  CHECK(fixture.routes[2] == (FirstGateWord(1) | kFirstClosed | kSecondClosed));

  fixture.routes[2] = FirstGateWord(1);
  REQUIRE(fixture.Update(1U << 1U));
  CHECK(fixture.routes[2] == (FirstGateWord(1) | kSecondClosed));
  CHECK(fixture.routes[ai::kRouteWordCount - 1] == 0);
}

TEST_CASE("AI route gates keep the original's shifted selector",
          "[ai][navigation]") {
  // A link end in the gate's column but another row still shifts the
  // selector, so the next matching end tests bit 1 << (1 << 1).
  RouteFixture fixture;
  fixture.ends[0] = {.x = 3, .z = 9};
  fixture.ends[2] = {.x = 3, .z = 4};
  fixture.routes[3] = FirstGateWord(1);
  REQUIRE(fixture.Update(1U << 1U));
  CHECK((fixture.routes[3] & kFirstClosed) != 0);
  fixture.routes[3] = FirstGateWord(1);
  REQUIRE(fixture.Update(1U << 2U));
  CHECK((fixture.routes[3] & kFirstClosed) == 0);
}

TEST_CASE("AI route gates refuse a stale-register first word",
          "[ai][navigation]") {
  RouteFixture fixture;
  fixture.routes[0] = kSecondGate;
  fixture.routes[1] = FirstGateWord(1);
  const auto before = fixture.routes;
  CHECK_FALSE(fixture.Update(1));
  CHECK(fixture.routes == before);
  // Without a visible platform no gate opens, so the stale value is unused.
  CHECK(fixture.Update(0));
  CHECK(fixture.routes[0] == (kSecondGate | kFirstClosed | kSecondClosed));
}

namespace {
struct FakeOverrideHost final : ai::NavigationOverrideHost {
  bool has_tile_flags = true;
  bool has_state = true;
  bool has_override = false;
  std::optional<std::uint32_t> tracked = 0;
  std::uint32_t destination = 0;
  bool matches = true;
  bool allocates = true;
  bool copies = true;
  std::vector<int> released;
  std::vector<ai::NavigationOverrideTarget> copied;
  std::optional<ai::NavigationOverrideTarget> target;
  std::uint32_t target_xz = 0;

  [[nodiscard]] bool HasTileFlags() const override { return has_tile_flags; }
  [[nodiscard]] bool HasActorState(int /*actor_id*/) const override {
    return has_state;
  }
  [[nodiscard]] bool HasOverride(int /*actor_id*/) const override {
    return has_override;
  }
  [[nodiscard]] std::optional<std::uint32_t> OverrideTrackedTile(
      int /*actor_id*/) const override {
    return tracked;
  }
  [[nodiscard]] std::uint32_t OverrideDestination(
      int /*actor_id*/) const override {
    return destination;
  }
  [[nodiscard]] bool MatchesActorAndRoom(int /*actor_id*/) const override {
    return matches;
  }
  void ReleaseEngagement(int actor_id) override {
    released.push_back(actor_id);
  }
  bool AllocateOverride(int /*actor_id*/) override {
    has_override = allocates;
    return allocates;
  }
  bool CopyOverrideState(int /*actor_id*/,
                         ai::NavigationOverrideTarget into) override {
    copied.push_back(into);
    return copies;
  }
  void SetScriptedTarget(int /*actor_id*/, ai::NavigationOverrideTarget into,
                         std::uint32_t xz) override {
    target = into;
    target_xz = xz;
  }
};
}  // namespace

TEST_CASE("AI navigation override status tracks the destination tile",
          "[ai][navigation]") {
  FakeOverrideHost host;
  CHECK(ai::HasActorNavigationState(host, 0) == true);
  CHECK(ai::HasActorNavigationState(host, 15) == true);
  CHECK(ai::ActorNavigationOverrideStatus(host, 3) == 2);
  host.has_override = true;
  host.tracked = 0x00040003;
  host.destination = 0x7F04AB03;  // Layer and status bytes are ignored.
  CHECK(ai::ActorNavigationOverrideStatus(host, 3) == 0);
  host.destination = 0x00040004;
  CHECK(ai::ActorNavigationOverrideStatus(host, 3) == 1);
  host.has_state = false;
  CHECK(ai::ActorNavigationOverrideStatus(host, 3) == 2);
  host.has_state = true;
  host.has_tile_flags = false;
  CHECK(ai::HasActorNavigationState(host, 3) == false);
  CHECK(ai::ActorNavigationOverrideStatus(host, 3) == 2);
}

TEST_CASE("AI navigation override status refuses invalid queries",
          "[ai][navigation]") {
  FakeOverrideHost host;
  host.has_override = true;
  CHECK_FALSE(ai::HasActorNavigationState(host, -1));
  CHECK_FALSE(ai::HasActorNavigationState(host, 16));
  CHECK_FALSE(ai::ActorNavigationOverrideStatus(host, 16));
  host.tracked.reset();
  CHECK_FALSE(ai::ActorNavigationOverrideStatus(host, 3));
}

TEST_CASE("AI navigation overrides start or restart a scripted move",
          "[ai][navigation]") {
  FakeOverrideHost host;
  using Target = ai::NavigationOverrideTarget;
  REQUIRE(ai::SetActorNavigationOverride(host, 5, 0x1234, 0x0567));
  CHECK(host.released == std::vector<int>{5});
  CHECK(host.copied == std::vector<Target>{Target::kNewRoute});
  CHECK(host.target == Target::kNewRoute);
  CHECK(host.target_xz == 0x05671234U);
}

TEST_CASE("AI navigation overrides restart on the actor's own state",
          "[ai][navigation]") {
  FakeOverrideHost host;
  using Target = ai::NavigationOverrideTarget;
  host.has_override = true;
  // Repeat entry copies onto the actor's own state.
  REQUIRE(ai::SetActorNavigationOverride(host, 5, 1, 2));
  CHECK(host.copied.back() == Target::kActorState);
  CHECK(host.target == Target::kActorState);
  CHECK(host.target_xz == 0x00020001U);
}

TEST_CASE("AI navigation overrides skip actors without state",
          "[ai][navigation]") {
  FakeOverrideHost stateless;
  stateless.has_state = false;
  CHECK(ai::SetActorNavigationOverride(stateless, 5, 1, 2));
  CHECK(stateless.released.empty());
  CHECK_FALSE(stateless.target.has_value());
}

TEST_CASE("AI navigation overrides refuse invalid requests",
          "[ai][navigation]") {
  FakeOverrideHost host;
  CHECK_FALSE(ai::SetActorNavigationOverride(host, 16, 1, 2));
  CHECK_FALSE(ai::SetActorNavigationOverride(host, -1, 1, 2));
  host.matches = false;
  CHECK_FALSE(ai::SetActorNavigationOverride(host, 5, 1, 2));
  CHECK(host.released.empty());
  host.matches = true;
  host.allocates = false;
  CHECK_FALSE(ai::SetActorNavigationOverride(host, 5, 1, 2));
  CHECK(host.copied.empty());
  host.allocates = true;
  host.copies = false;
  CHECK_FALSE(ai::SetActorNavigationOverride(host, 5, 1, 2));
  CHECK_FALSE(host.target.has_value());
}

namespace {
struct FakeCopy final : ai::NavigationOverrideCopy {
  bool has_source = true;
  bool has_destination = true;
  std::vector<int> sources;
  std::optional<ai::NavigationOverrideSettings> settings;

  [[nodiscard]] bool HasSource(int /*actor_id*/) const override {
    return has_source;
  }
  [[nodiscard]] bool HasDestination() const override { return has_destination; }
  void CopySource(int actor_id) override { sources.push_back(actor_id); }
  void ApplySettings(const ai::NavigationOverrideSettings& value) override {
    settings = value;
  }
};
}  // namespace

TEST_CASE("AI navigation override copies apply the starting settings",
          "[ai][navigation]") {
  FakeCopy copy;
  REQUIRE(ai::CopyActorNavigationOverrideState(copy, 15));
  CHECK(copy.sources == std::vector<int>{15});
  REQUIRE(copy.settings.has_value());
  const auto settings = copy.settings.value_or(
      ai::NavigationOverrideSettings{.actor_id = 0xFF, .movement_enabled = 0});
  CHECK(settings.actor_id == 15);
  CHECK(settings.step_height == 0x140);
  CHECK(settings.tile_step_limit == 3);
}

TEST_CASE("AI navigation override copies clear the movement state",
          "[ai][navigation]") {
  FakeCopy copy;
  REQUIRE(ai::CopyActorNavigationOverrideState(copy, 15));
  REQUIRE(copy.settings.has_value());
  const auto settings = copy.settings.value_or(
      ai::NavigationOverrideSettings{.actor_id = 0xFF, .movement_enabled = 0});
  CHECK(settings.navigation_flags == 0);
  CHECK(settings.repeat_wait == 0);
  CHECK(settings.area_settings == 3);
  CHECK(settings.movement_enabled == 1);
}

TEST_CASE("AI navigation override copies refuse missing records",
          "[ai][navigation]") {
  FakeCopy refused;
  CHECK_FALSE(ai::CopyActorNavigationOverrideState(refused, 16));
  refused.has_destination = false;
  CHECK_FALSE(ai::CopyActorNavigationOverrideState(refused, 1));
  refused.has_destination = true;
  refused.has_source = false;
  CHECK_FALSE(ai::CopyActorNavigationOverrideState(refused, 1));
  CHECK(refused.sources.empty());
  CHECK_FALSE(refused.settings.has_value());
}

namespace {
struct FakeRouteHost final : ai::ScriptedRouteHost {
  std::vector<ai::ScriptedRouteActor> actors;
  std::size_t next = 0;
  bool cyclic = false;
  std::optional<ai::ScriptedRoute> route;
  std::optional<ai::RouteModelPlacement> placement;
  std::uint16_t flags = 0;
  std::vector<ai::ScriptedRoute> stored;
  std::vector<std::string_view> calls;

  void BeginUpdate() override {
    next = 0;
    calls.push_back("begin");
  }
  [[nodiscard]] std::optional<ai::ScriptedRouteActor> NextActor() override {
    if (next >= actors.size()) {
      if (!cyclic || actors.empty()) return std::nullopt;
      next = 0;
    }
    return actors[next++];
  }
  void StampBudgetTick() override { calls.push_back("stamp"); }
  [[nodiscard]] std::optional<ai::ScriptedRoute> Route(int actor_id) override {
    static_cast<void>(actor_id);
    return route;
  }
  [[nodiscard]] std::optional<ai::RouteModelPlacement> Placement(
      int /*actor_id*/) const override {
    return placement;
  }
  [[nodiscard]] std::uint16_t TileFlags(int /*x*/, int /*z*/) const override {
    return flags;
  }
  void StoreRoute(const ai::ScriptedRoute& value) override {
    stored.push_back(value);
  }
  void ClearDirectionCache() override { calls.push_back("clear"); }
  void MoveTowardDestinationTile(ai::ScriptedRoute& value) override {
    stored.push_back(value);
    calls.push_back("gate");
  }
  void UpdateNavigationAreaMovement(ai::ScriptedRoute& value) override {
    stored.push_back(value);
    calls.push_back("area");
  }
  void DispatchTileMovement(ai::ScriptedRoute& value) override {
    stored.push_back(value);
    calls.push_back("tile");
  }
  void FinishRoute(ai::ScriptedRoute& /*value*/) override {
    calls.push_back("finish");
  }
  void SaveMovementRandomState() override { calls.push_back("save"); }
};

ai::ScriptedRoute BaseRoute() {
  return ai::ScriptedRoute{.actor_id = 4,
                           .movement_wait = 3,
                           .navigation_flags = 0x1040,
                           .scripted_target = 0x02000300};
}

ai::RouteModelPlacement BasePlacement() {
  return ai::RouteModelPlacement{.tile_x = 6,
                                 .tile_layer = 1,
                                 .tile_z = 4,
                                 .tile_status = 2,
                                 .x = 0x0310,
                                 .y = -8,
                                 .z = 0x0210,
                                 .position_pad = 9};
}

FakeRouteHost RouteHost(const ai::ScriptedRoute& route = BaseRoute()) {
  FakeRouteHost host;
  host.actors = {{.id = 4, .suspended = false}};
  host.route = route;
  host.placement = BasePlacement();
  host.flags = 0x1234;
  return host;
}

// The first stored route, or a default one when nothing was stored.
ai::ScriptedRoute FirstStored(const FakeRouteHost& host) {
  return host.stored.empty() ? ai::ScriptedRoute{} : host.stored.front();
}
}  // namespace

TEST_CASE("AI scripted routes refresh from the actor's model",
          "[ai][navigation]") {
  auto route = BaseRoute();
  route.scripted_target = 0x09000900;
  auto host = RouteHost(route);
  REQUIRE(ai::UpdateActorNavigationOverrides(host));
  REQUIRE(host.stored.size() == 2);
  const auto stored = FirstStored(host);
  CHECK(stored.movement_wait == 2);
  CHECK(stored.navigation_flags == 0x1020);
  CHECK(stored.ignore_floor == 1);
  CHECK(stored.movement_target == 16);
  CHECK(stored.tile == 0x02040106U);
}

TEST_CASE("AI scripted routes move area routes toward the target",
          "[ai][navigation]") {
  auto route = BaseRoute();
  route.scripted_target = 0x09000900;
  auto host = RouteHost(route);
  REQUIRE(ai::UpdateActorNavigationOverrides(host));
  REQUIRE(host.stored.size() == 2);
  CHECK(host.calls == std::vector<std::string_view>{"begin", "stamp", "clear",
                                                    "area", "save"});
  CHECK(host.stored.back().move_x == 0x0900);
  CHECK(host.stored.back().destination == 0x00120012U);
}

TEST_CASE("AI scripted routes copy the model's position and areas",
          "[ai][navigation]") {
  auto route = BaseRoute();
  route.scripted_target = 0x09000900;
  auto host = RouteHost(route);
  REQUIRE(ai::UpdateActorNavigationOverrides(host));
  const auto stored = FirstStored(host);
  CHECK(stored.x == 0x0310);
  CHECK(stored.y == -8);
  CHECK(stored.position_pad == 9);
  CHECK(stored.fine_x == 0x0C);
  CHECK(stored.fine_z == 0x08);
  CHECK(stored.ground_area == 4);
  CHECK(stored.area_exception == 4);
}

TEST_CASE("AI scripted routes finish on arrival or continue by mode",
          "[ai][navigation]") {
  auto route = BaseRoute();
  route.scripted_target = 0x0220031F;  // 15 and 16 units away.
  auto near = RouteHost(route);
  REQUIRE(ai::UpdateActorNavigationOverrides(near));
  CHECK(near.calls == std::vector<std::string_view>{"begin", "stamp", "clear",
                                                    "finish", "save"});

  route = BaseRoute();
  route.route_gate = 1;
  route.scripted_target = 0x09000900;
  auto gated = RouteHost(route);
  REQUIRE(ai::UpdateActorNavigationOverrides(gated));
  CHECK(gated.calls[3] == std::string_view("gate"));
}

TEST_CASE("AI scripted tile routes dispatch until they arrive",
          "[ai][navigation]") {
  auto route = BaseRoute();
  route.locomotion_mode = 1;
  auto tiled = RouteHost(route);
  REQUIRE(ai::UpdateActorNavigationOverrides(tiled));
  CHECK(tiled.calls[3] == std::string_view("tile"));
  CHECK(tiled.stored.back().destination == 0x00040006U);

  route.destination = 0x7F040006;
  auto arrived = RouteHost(route);
  REQUIRE(ai::UpdateActorNavigationOverrides(arrived));
  CHECK(arrived.calls[3] == std::string_view("finish"));
}

TEST_CASE("AI scripted routes skip suspended and route-less actors",
          "[ai][navigation]") {
  auto host = RouteHost();
  host.actors = {{.id = 4, .suspended = true}};
  REQUIRE(ai::UpdateActorNavigationOverrides(host));
  CHECK(host.calls == std::vector<std::string_view>{"begin", "save"});
  host.actors = {{.id = 4, .suspended = false}};
  host.route.reset();
  host.calls.clear();
  REQUIRE(ai::UpdateActorNavigationOverrides(host));
  CHECK(host.calls == std::vector<std::string_view>{"begin", "stamp", "save"});
}

TEST_CASE("AI scripted routes refuse invalid actors and lists",
          "[ai][navigation]") {
  auto host = RouteHost();
  host.actors = {{.id = 16, .suspended = false}};
  CHECK_FALSE(ai::UpdateActorNavigationOverrides(host));
  host.actors = {{.id = 4, .suspended = false}};
  auto route = BaseRoute();
  route.actor_id = 5;
  host.route = route;
  CHECK_FALSE(ai::UpdateActorNavigationOverrides(host));
  host.route = BaseRoute();
  host.placement.reset();
  CHECK_FALSE(ai::UpdateActorNavigationOverrides(host));
  auto placement = BasePlacement();
  placement.tile_x = 32;
  host.placement = placement;
  CHECK_FALSE(ai::UpdateActorNavigationOverrides(host));
  CHECK(host.stored.empty());

  auto cyclic = RouteHost();
  cyclic.actors = {{.id = 4, .suspended = true}};
  cyclic.cyclic = true;
  CHECK_FALSE(ai::UpdateActorNavigationOverrides(cyclic));
  CHECK(cyclic.calls == std::vector<std::string_view>{"begin"});
}
