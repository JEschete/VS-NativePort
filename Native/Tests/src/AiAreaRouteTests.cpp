#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/AreaRoute.hpp"
#include "Battle/Ai/AreaRouteLive.hpp"
#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
}

namespace {
namespace ai = VS::Battle::Ai;
using ai::CommandIntent;
using Ends = std::array<ai::RouteGateEnd, ai::kRouteGateEndCount>;
using Contacts = std::vector<VS::Field::PlatformContact>;

constexpr int kArea = 3;
constexpr int kIndex = 5;
constexpr int kRoute = (kArea * 16) + kIndex;
constexpr std::uint32_t kXZ = 0xFF00FF;
constexpr int kTileUnits = 128;
constexpr std::int32_t kWorldTile = 1 << 19;

// Endpoint step kinds; kinds 0-3 step north, west, south and east.
constexpr unsigned kWestStep = 0x800;
constexpr unsigned kSouthStep = 0x1000;
constexpr unsigned kEastStep = 0x1800;
constexpr unsigned kLongStep2 = 0x2000;
constexpr unsigned kLongStep4 = 0x6000;
constexpr unsigned kGate = 0x400;
constexpr unsigned kClosed = 0x8000;

constexpr std::uint32_t Tile(int x, int z, std::uint32_t other = 0) {
  return other | (static_cast<std::uint32_t>(z) << 16) |
         static_cast<std::uint32_t>(x);
}

constexpr std::uint16_t Endpoint(int x, int z, unsigned flags = 0) {
  return static_cast<std::uint16_t>(flags | (static_cast<unsigned>(x) << 5) |
                                    static_cast<unsigned>(z));
}

constexpr std::int16_t Centre(int tile) {
  return static_cast<std::int16_t>((tile * kTileUnits) + 64);
}

constexpr std::size_t At(int value) { return static_cast<std::size_t>(value); }

bool Same(ai::RouteGateEnd end, int x, int z) {
  return end.x == x && end.z == z;
}

struct FakeRouteWorld final : ai::AreaRouteWorld {
  using Grid = std::array<std::array<std::uint16_t, 32>, 32>;

  std::array<ai::AreaRouteEndpoints, 256> routes{};
  bool has_routes = true;
  mutable std::vector<int> route_queries;
  bool has_maps = true;
  Grid flags{};    // [z][x]
  Grid heights{};  // [z][x]
  bool has_regions = true;
  std::array<std::array<std::uint8_t, 32>, 32> regions{};  // [z][x]
  std::array<std::uint8_t, 4> step_directions{0, 2, 4, 6};
  ai::NavigationDirectionCodes codes{0x30, 0x33, 0x03, 0x13,
                                     0x10, 0x11, 0x01, 0x31};
  std::optional<Ends> ends = Ends{};
  Contacts platforms;
  std::array<std::optional<std::uint16_t>, 4> dwell{};
  std::optional<std::uint8_t> closed = std::uint8_t{0};
  std::vector<int> contacts;
  std::map<std::uint32_t, int> occupancy;
  bool refuse_occupancy = false;
  std::vector<std::uint32_t> occupancy_queries;
  std::vector<ai::AreaRouteActor> occupancy_actors;
  std::array<bool, 16> present{};
  std::array<std::optional<ai::AreaRoutePoint>, 16> positions{};

  void SetRoute(std::uint16_t first, std::uint16_t second = 0) {
    routes.at(At(kRoute)) = {first, second};
  }

  // Replaces the links with platform 1's: link 2 boards at the gate tile and
  // link 3's platform end is the exit; the platform waits at link 2's
  // platform end.
  void SetGate(int x, int z) {
    auto& gate_ends = ends.emplace();
    gate_ends[4] = {.x = static_cast<std::uint8_t>(x),
                    .z = static_cast<std::uint8_t>(z)};
    gate_ends[5] = {.x = 20, .z = 21};
    gate_ends[7] = {.x = 22, .z = 23};
    platforms = Contacts(2);
    platforms[1] = {.x = 20 * kTileUnits,
                    .height = 0,
                    .z = (21 * kTileUnits) + 127,
                    .platform_index = 1};
    dwell[1] = std::uint16_t{5};
  }

  [[nodiscard]] std::optional<ai::AreaRouteEndpoints> Route(
      int index) const override {
    route_queries.push_back(index);
    if (!has_routes) return std::nullopt;
    return routes.at(At(index));
  }
  [[nodiscard]] bool HasTileMaps() const override { return has_maps; }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return flags.at(At(z)).at(At(x));
  }
  [[nodiscard]] std::uint16_t TileHeight(int x, int z) const override {
    return heights.at(At(z)).at(At(x));
  }
  [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(
      int x, int z) const override {
    if (!has_regions) return std::nullopt;
    return regions.at(At(z)).at(At(x));
  }
  [[nodiscard]] ai::NavigationDirectionCodes DirectionCodes() const override {
    return codes;
  }
  [[nodiscard]] std::uint8_t RouteStepDirection(int kind) const override {
    return step_directions.at(At(kind));
  }
  [[nodiscard]] std::optional<Ends> RouteGateEnds() const override {
    return ends;
  }
  [[nodiscard]] Contacts PlatformPositions() const override {
    return platforms;
  }
  [[nodiscard]] std::optional<std::uint16_t> PlatformDwellTicks(
      int platform) const override {
    return dwell.at(At(platform));
  }
  [[nodiscard]] std::optional<std::uint8_t> ClosedGates() const override {
    return closed;
  }
  void SetContactOccupancy(int bit) override { contacts.push_back(bit); }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      const ai::AreaRouteActor& actor, std::uint32_t tile) override {
    occupancy_queries.push_back(tile);
    occupancy_actors.push_back(actor);
    if (refuse_occupancy) return std::nullopt;
    const auto found = occupancy.find(tile & kXZ);
    return found == occupancy.end() ? 1 : found->second;
  }
  [[nodiscard]] bool HasActor(int slot) const override {
    return present.at(At(slot));
  }
  [[nodiscard]] std::optional<ai::AreaRoutePoint> ActorPosition(
      int slot) const override {
    return positions.at(At(slot));
  }
};

// An actor standing on tile (10, 10) with a tag and status in the tile word.
ai::AreaRouteActor Actor() {
  return {.target_distance_filter = 9,
          .tile = Tile(10, 10, 0x5A000700),
          .x = Centre(10),
          .z = Centre(10),
          .actor_id = 2,
          .turn_timer = 0,
          .turn_period = 30,
          .area = 1,
          .movement_target = ai::kAreaRouteNoTarget,
          .destination = Tile(1, 1),
          .next_tile = Tile(2, 2),
          .move_x = 0,
          .move_z = 0,
          .heading = 0,
          .angle = 0,
          .route_gate = 0,
          .route_exit = 0xA5000000};
}

std::optional<CommandIntent> Resolve(FakeRouteWorld& world,
                                     ai::AreaRouteActor& actor) {
  return ai::ResolveNavigationAreaRoute(world, actor, kIndex, kArea);
}

// A turn toward the centre of tile (x, z) from the actor's tile centre.
void CheckTurnToward(const ai::AreaRouteActor& actor, int x, int z,
                     std::int16_t angle) {
  CHECK(actor.move_x == Centre(x));
  CHECK(actor.move_z == Centre(z));
  CHECK(actor.angle == angle);
  CHECK(actor.turn_timer == 30);
}

struct StepCase {
  unsigned kind;
  std::int16_t direction;
  int dx;
  int dz;
};
}  // namespace

TEST_CASE("Route gate exits use the partner link's platform end",
          "[ai][route-gates]") {
  Ends ends{};
  ends[4] = {.x = 5, .z = 6};
  ends[7] = {.x = 9, .z = 10};
  CHECK(ai::ResolveRouteGateExit(ends, Tile(5, 6, 0xFF00FF00)) ==
        ai::RouteGateExit{.x = 9, .platform = 1, .z = 10});
  ends[6] = {.x = 5, .z = 6};
  ends[5] = {.x = 11, .z = 12};
  CHECK(ai::ResolveRouteGateExit(ends, Tile(5, 6)) ==
        ai::RouteGateExit{.x = 9, .platform = 1, .z = 10});
  ends[2] = {.x = 5, .z = 6};
  ends[1] = {.x = 13, .z = 14};
  CHECK(ai::ResolveRouteGateExit(ends, Tile(5, 6)) ==
        ai::RouteGateExit{.x = 13, .platform = 0, .z = 14});
  CHECK_FALSE(ai::ResolveRouteGateExit(ends, Tile(5, 7)).has_value());
  CHECK_FALSE(ai::ResolveRouteGateExit(ends, Tile(9, 10)).has_value());
}

TEST_CASE("Nearest route gate entry prefers the platform end under the point",
          "[ai][route-gates]") {
  Ends ends{};
  ends[1] = {.x = 20, .z = 20};
  ends[4] = {.x = 1, .z = 2};
  ends[5] = {.x = 7, .z = 8};
  ends[6] = {.x = 3, .z = 4};
  ends[7] = {.x = 9, .z = 10};
  const auto entry = [&ends](int platform, std::int32_t x, std::int32_t z) {
    return ai::FindNearestRouteGateEntry(ends, platform, x, z);
  };
  const auto under =
      entry(1, (9 * kWorldTile) + kWorldTile - 1, 10 * kWorldTile);
  REQUIRE(under.has_value());
  CHECK(Same(*under, 3, 4));
  const auto first = entry(1, 7 * kWorldTile, 8 * kWorldTile);
  REQUIRE(first.has_value());
  CHECK(Same(*first, 1, 2));
}

TEST_CASE("Nearest route gate entry falls back to the platform's first link",
          "[ai][route-gates]") {
  Ends ends{};
  ends[1] = {.x = 20, .z = 20};
  ends[4] = {.x = 1, .z = 2};
  ends[5] = {.x = 7, .z = 8};
  ends[6] = {.x = 3, .z = 4};
  ends[7] = {.x = 31, .z = 31};
  const auto entry = [&ends](int platform, std::int32_t x, std::int32_t z) {
    return ai::FindNearestRouteGateEntry(ends, platform, x, z);
  };
  const auto fallback = entry(1, 20 * kWorldTile, 20 * kWorldTile);
  REQUIRE(fallback.has_value());
  CHECK(Same(*fallback, 1, 2));
  const auto negative = entry(1, -1, -1);
  REQUIRE(negative.has_value());
  CHECK(Same(*negative, 1, 2));
  CHECK(entry(0, 0, 0).has_value());
  CHECK(entry(3, 0, 0).has_value());
  CHECK_FALSE(entry(4, 0, 0).has_value());
  CHECK_FALSE(entry(-1, 0, 0).has_value());
}

TEST_CASE("Route gate lookup matches a link at its platform's reported tile",
          "[ai][route-gates]") {
  Ends ends{};
  ends[4] = {.x = 5, .z = 6};
  ends[5] = {.x = 9, .z = 10};
  Contacts positions(2);
  positions[1] = {.x = (9 * kTileUnits) + 127,
                  .height = 0,
                  .z = 10 * kTileUnits,
                  .platform_index = 1};
  const auto find = [&](std::uint32_t tile, bool entry) {
    return ai::FindRouteGateAtTile(ends, positions, tile, entry);
  };
  CHECK(find(Tile(5, 6, 0xFF00FF00), true) == 2);
  CHECK(find(Tile(9, 10), false) == 2);
  CHECK(find(Tile(9, 10), true) == -1);
  CHECK(find(Tile(5, 6), false) == -1);
  ends[5] = {.x = 8, .z = 8};
  ends[6] = {.x = 5, .z = 6};
  ends[7] = {.x = 9, .z = 10};
  CHECK(find(Tile(5, 6), true) == 3);
  positions[1].x = std::int16_t{10 * kTileUnits};
  CHECK(find(Tile(5, 6), true) == -1);
  positions = Contacts(1);
  positions[0] = {.x = 9 * kTileUnits,
                  .height = 0,
                  .z = 10 * kTileUnits,
                  .platform_index = 0};
  CHECK(find(Tile(5, 6), true) == -1);
  positions = Contacts(5);
  CHECK_FALSE(find(Tile(5, 6), true).has_value());
}

TEST_CASE("Route endpoint pairs keep the first endpoint in the low halfword",
          "[ai][area-route]") {
  const ai::AreaRouteEndpoints endpoints{0x1234, 0xABCD};
  CHECK(ai::RouteEndpointPair(endpoints) == 0xABCD1234U);
  CHECK(ai::RouteEndpointValue(endpoints, 0) == 0x1234);
  CHECK(ai::RouteEndpointValue(endpoints, 1) == 0xABCD);
  CHECK_FALSE(ai::RouteEndpointValue(endpoints, 2).has_value());
  CHECK_FALSE(ai::RouteEndpointValue(endpoints, -1).has_value());
}

TEST_CASE("Navigation tile flags take z before x and refuse missing maps",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.flags[3][7] = 0x1234;
  CHECK(ai::NavigationTileFlags(world, 3, 7) == 0x1234);
  CHECK(ai::NavigationTileFlags(world, 7, 3) == 0);
  CHECK(ai::NavigationTileFlags(world, 31, 31) == 0);
  CHECK_FALSE(ai::NavigationTileFlags(world, 32, 0).has_value());
  CHECK_FALSE(ai::NavigationTileFlags(world, 0, -1).has_value());
  world.has_maps = false;
  CHECK_FALSE(ai::NavigationTileFlags(world, 3, 7).has_value());
}

TEST_CASE("Setting the area from a tile keeps only the area bits",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.flags[6][5] = 0x00A7;
  auto actor = Actor();
  CHECK(ai::SetNavigationAreaFromTile(world, actor, Tile(5, 6, 0xFF00FF00)) ==
        CommandIntent::kMoveDestination);
  CHECK(actor.area == 7);
  const auto before = actor;
  world.has_maps = false;
  CHECK_FALSE(ai::SetNavigationAreaFromTile(world, actor, Tile(5, 6)));
  CHECK(actor == before);
}

TEST_CASE("Area routes without a first endpoint keep moving to the destination",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.SetRoute(Endpoint(0, 9), Endpoint(4, 4));
  auto actor = Actor();
  const auto before = actor;
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor == before);
  CHECK(world.route_queries == std::vector<int>{kRoute});
  world.routes[255] = {Endpoint(12, 10), 0};
  CHECK(ai::ResolveNavigationAreaRoute(world, actor, 15, 15) ==
        CommandIntent::kMoveDestination);
  CHECK(world.route_queries.back() == 255);
  CHECK(actor.area == 15);
  CHECK(actor.destination == Tile(12, 10));
}

TEST_CASE("Area routes refuse indices outside the table and a missing room",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.SetRoute(Endpoint(12, 10));
  auto actor = Actor();
  const auto before = actor;
  CHECK_FALSE(ai::ResolveNavigationAreaRoute(world, actor, 16, kArea));
  CHECK_FALSE(ai::ResolveNavigationAreaRoute(world, actor, -1, kArea));
  CHECK_FALSE(ai::ResolveNavigationAreaRoute(world, actor, kIndex, 16));
  CHECK_FALSE(ai::ResolveNavigationAreaRoute(world, actor, kIndex, -1));
  CHECK(world.route_queries.empty());
  world.has_routes = false;
  CHECK_FALSE(Resolve(world, actor));
  CHECK(actor == before);
}

TEST_CASE("An endpoint at the actor's tile walks one step along the route",
          "[ai][area-route]") {
  const auto step = GENERATE(StepCase{0, 0, 0, -1}, StepCase{1, 2, -1, 0},
                             StepCase{2, 4, 0, 1}, StepCase{3, 6, 1, 0});
  FakeRouteWorld world;
  world.SetRoute(Endpoint(10, 10, step.kind << 11));
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDirection);
  CHECK(actor.heading == step.direction);
  CHECK(actor.area == kArea);
  CHECK(world.occupancy_queries ==
        std::vector{Tile(10 + step.dx, 10 + step.dz, 0x5A000700)});
  REQUIRE(world.occupancy_actors.size() == 1);
  CHECK(world.occupancy_actors[0].area == kArea);
}

TEST_CASE("The second endpoint is followed when it is at the actor's tile",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.SetRoute(Endpoint(3, 4), Endpoint(10, 10, kWestStep));
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDirection);
  CHECK(actor.heading == 2);
}

TEST_CASE("Route steps onto raised tiles and out of the tile's region",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.SetRoute(Endpoint(10, 10, kEastStep));
  const std::uint16_t layer = GENERATE(0x400, 0x800, 0x1000);
  world.heights[10][11] = layer;
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kStepToTile);
  CHECK(actor.next_tile == Tile(11, 10, 0x5A000000));
  CHECK(actor.heading == 0);

  world.heights[10][11] = 0x23FF;
  world.heights[11][10] = 0x400;
  world.regions[10][11] = 0xFF;
  actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDirection);
  world.regions[10][10] = 1 << 6;
  actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kStepToTile);
  CHECK(actor.next_tile == Tile(11, 10, 0x5A000000));
  world.regions[10][10] = 0xBF;
  actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDirection);
}

TEST_CASE("Blocked or held route steps turn toward the step tile",
          "[ai][area-route]") {
  const int occupancy = GENERATE(0, -1);
  FakeRouteWorld world;
  world.SetRoute(Endpoint(10, 10, kEastStep));
  world.occupancy[Tile(11, 10)] = occupancy;
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kTurn);
  CheckTurnToward(actor, 11, 10, 3072);
  CHECK(actor.next_tile == Tile(2, 2));

  world.SetRoute(Endpoint(10, 10, kWestStep));
  world.occupancy[Tile(9, 10)] = occupancy;
  actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kTurn);
  CheckTurnToward(actor, 9, 10, 1024);
}

TEST_CASE("Route turns face a present target actor's position",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.SetRoute(Endpoint(10, 10, kEastStep));
  world.occupancy[Tile(11, 10)] = 0;
  world.present[3] = true;
  world.positions[3] = ai::AreaRoutePoint{
      .x = static_cast<std::int16_t>(Centre(10) - 128), .z = Centre(10)};
  auto actor = Actor();
  actor.movement_target = 3;
  CHECK(Resolve(world, actor) == CommandIntent::kTurn);
  CHECK(actor.move_x == Centre(10) - 128);
  CHECK(actor.move_z == Centre(10));
  CHECK(actor.angle == 1024);

  world.present[3] = false;
  actor = Actor();
  actor.movement_target = 3;
  CHECK(Resolve(world, actor) == CommandIntent::kTurn);
  CheckTurnToward(actor, 11, 10, 3072);

  world.present[3] = true;
  world.positions[3].reset();
  actor = Actor();
  actor.movement_target = 3;
  CHECK_FALSE(Resolve(world, actor));
  actor = Actor();
  actor.movement_target = 17;
  CHECK_FALSE(Resolve(world, actor));
}

TEST_CASE("Long route steps change area and cross several tiles",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.flags[10][10] = 0x0049;
  world.SetRoute(Endpoint(10, 10, kLongStep2 | kEastStep));
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kStepToTile);
  CHECK(actor.next_tile == Tile(12, 10, 0x5A000000));
  CHECK(actor.area == 9);
  CHECK(actor.target_distance_filter == 0);
  REQUIRE(world.occupancy_actors.size() == 1);
  CHECK(world.occupancy_actors[0].area == 9);
  CHECK(world.occupancy_actors[0].target_distance_filter == 0);

  world.SetRoute(Endpoint(10, 10, kLongStep4 | kSouthStep));
  actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kStepToTile);
  CHECK(actor.next_tile == Tile(10, 14, 0x5A000000));
}

TEST_CASE("A long step onto an occupied tile turns toward it",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.flags[10][10] = 0x0049;
  world.SetRoute(Endpoint(10, 10, kLongStep2 | kEastStep));
  world.occupancy[Tile(12, 10)] = -1;
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kTurn);
  CheckTurnToward(actor, 12, 10, 3072);
}

TEST_CASE("A blocked long step turns toward its tile from the endpoint",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.flags[10][10] = 0x0049;
  world.SetRoute(Endpoint(10, 10, kLongStep2 | kEastStep));
  world.occupancy[Tile(12, 10)] = 0;
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kTurn);
  CHECK(actor.destination == Tile(10, 10));
  CHECK(actor.area == 9);
  CheckTurnToward(actor, 12, 10, 3072);
  world.has_maps = false;
  actor = Actor();
  CHECK_FALSE(Resolve(world, actor));
}

TEST_CASE("An open platform gate boards the waiting platform",
          "[ai][area-route][route-gates]") {
  const std::uint8_t actor_id = GENERATE(std::uint8_t{0}, std::uint8_t{2});
  FakeRouteWorld world;
  world.SetGate(10, 10);
  world.closed = std::uint8_t{1 << 2};
  world.SetRoute(Endpoint(10, 10, kGate));
  auto actor = Actor();
  actor.actor_id = actor_id;
  CHECK(Resolve(world, actor) == CommandIntent::kStepToTile);
  CHECK(actor.next_tile == Tile(10, 10, 0x5A000300));
  CHECK(actor.route_gate == 3);
  CHECK(actor.route_exit == 0xA5170116U);
  CHECK(world.contacts ==
        (actor_id != 0 ? std::vector{3} : std::vector<int>{}));
  CHECK(world.occupancy_queries.empty());
}

TEST_CASE("Closed, stopped or absent platform gates turn toward the gate",
          "[ai][area-route][route-gates]") {
  FakeRouteWorld world;
  world.SetGate(10, 10);
  world.SetRoute(Endpoint(10, 10, kGate));
  SECTION("closed gate bit") { world.closed = std::uint8_t{1 << 3}; }
  SECTION("moving platform") { world.dwell[1].reset(); }
  SECTION("platform elsewhere") { world.platforms[1].x = 0; }
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kTurn);
  CheckTurnToward(actor, 10, 10, 0);
  CHECK(actor.route_gate == 0);
  CHECK(actor.route_exit == 0xA5000000U);
  CHECK(actor.next_tile == Tile(2, 2));
  CHECK(world.contacts.empty());
}

TEST_CASE("Platform gates refuse missing links, platforms or room state",
          "[ai][area-route][route-gates]") {
  FakeRouteWorld world;
  world.SetGate(10, 10);
  world.SetRoute(Endpoint(10, 10, kGate));
  SECTION("closed gates") { world.closed.reset(); }
  SECTION("route links") { world.ends.reset(); }
  SECTION("platform positions") { world.platforms = Contacts(5); }
  auto actor = Actor();
  CHECK_FALSE(Resolve(world, actor));
  CHECK(world.contacts.empty());
}

TEST_CASE("Closed endpoints keep the tile's area when no endpoint is open",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.flags[10][10] = 0x0046;
  auto actor = Actor();
  world.SetRoute(Endpoint(10, 10, kGate | kClosed));
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.area == 6);
  CHECK(actor.destination == Tile(1, 1));

  actor = Actor();
  world.SetRoute(Endpoint(3, 4, kGate | kClosed),
                 Endpoint(7, 8, kGate | kClosed));
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.area == 6);
  CHECK(actor.destination == Tile(1, 1));
}

TEST_CASE("A closed endpoint sends the actor to the open one",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.flags[10][10] = 0x0046;
  auto actor = Actor();
  world.SetRoute(Endpoint(3, 4, kGate | kClosed), Endpoint(7, 8));
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(7, 8));
  CHECK(actor.area == kArea);

  actor = Actor();
  world.SetRoute(Endpoint(3, 4), Endpoint(7, 8, kGate | kClosed));
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(3, 4));
}

TEST_CASE("Gates close with both flags, even in an undefined second endpoint",
          "[ai][area-route]") {
  FakeRouteWorld world;
  auto actor = Actor();
  world.SetRoute(Endpoint(3, 4), static_cast<std::uint16_t>(kGate | kClosed));
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(3, 4));

  actor = Actor();
  world.SetRoute(Endpoint(7, 8, kClosed), Endpoint(3, 4));
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(7, 8));
}

TEST_CASE("Raised endpoints send the actor to the other endpoint",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.flags[10][10] = 0x0046;
  world.SetRoute(Endpoint(7, 8), Endpoint(3, 4));
  world.heights[7][8] = 0x400;
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(7, 8));

  world.heights[8][7] = 0x800;
  actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(3, 4));
  CHECK(actor.area == kArea);

  world.SetRoute(Endpoint(3, 4), Endpoint(7, 8));
  actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(3, 4));
}

TEST_CASE("Raised endpoints with no other keep the tile's area",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.flags[10][10] = 0x0046;
  world.heights[8][7] = 0x800;
  world.heights[4][3] = 0x1000;
  world.SetRoute(Endpoint(3, 4), Endpoint(7, 8));
  auto actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(1, 1));
  CHECK(actor.area == 6);

  world.SetRoute(Endpoint(3, 4));
  actor = Actor();
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(1, 1));
  CHECK(actor.area == 6);

  world.has_maps = false;
  actor = Actor();
  CHECK_FALSE(Resolve(world, actor));
}

TEST_CASE("Area routes head for the nearer endpoint, the second on a tie",
          "[ai][area-route]") {
  FakeRouteWorld world;
  auto actor = Actor();
  world.SetRoute(Endpoint(12, 10), Endpoint(10, 13));
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(12, 10));
  world.SetRoute(Endpoint(10, 13), Endpoint(12, 10));
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(12, 10));
  world.SetRoute(Endpoint(12, 10), Endpoint(10, 12));
  CHECK(Resolve(world, actor) == CommandIntent::kMoveDestination);
  CHECK(actor.destination == Tile(10, 12));
  CHECK(world.occupancy_queries.empty());
}

TEST_CASE("Route steps refuse missing directions, regions and occupancy",
          "[ai][area-route]") {
  FakeRouteWorld world;
  world.SetRoute(Endpoint(10, 10, kEastStep));
  SECTION("direction") { world.step_directions[3] = 8; }
  SECTION("region") { world.has_regions = false; }
  SECTION("occupancy") { world.refuse_occupancy = true; }
  SECTION("height") { world.has_maps = false; }
  auto actor = Actor();
  CHECK_FALSE(Resolve(world, actor));
}

TEST_CASE("Area routing keeps its original module entries",
          "[ai][area-route][route-gates][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [symbols](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800DF9A8U) ==
        reinterpret_cast<void*>(&ai::ResolveLiveRouteGateExit));
  CHECK(resolve(0x800DFA54U) ==
        reinterpret_cast<void*>(&ai::FindLiveNearestRouteGateEntry));
  CHECK(resolve(0x800DFAF8U) ==
        reinterpret_cast<void*>(&ai::FindLiveRouteGateAtTile));
  CHECK(resolve(0x800DFBCCU) ==
        reinterpret_cast<void*>(&ai::ResolveLiveNavigationAreaRoute));
}

namespace {
struct LiveRouteFixture {
  Battle_Ai_FrameContext saved = *Battle_Ai_FrameState();
  std::array<u_int, 256> routes{};
  u_short flags[32][32]{};
  u_short heights[32][32]{};
  Battle_Ai_ActorAiState state{};

  LiveRouteFixture() {
    auto& frame = *Battle_Ai_FrameState();
    frame.roomData = routes.data();
    frame.tileFlags = flags;
    frame.heightMap = heights;
    state.tile.word = Tile(10, 10, 0x5A000700);
    state.reserved154Area = 1;
    state.destination.word = Tile(1, 1);
  }
  ~LiveRouteFixture() { *Battle_Ai_FrameState() = saved; }
  LiveRouteFixture(const LiveRouteFixture&) = delete;
  LiveRouteFixture& operator=(const LiveRouteFixture&) = delete;

  int Resolve() {
    return ai::ResolveLiveNavigationAreaRoute(
        reinterpret_cast<Battle_Ai_AreaRouteView*>(&state), kIndex, kArea);
  }
};
}  // namespace

TEST_CASE("Live area routing splits room route words and writes the AI state",
          "[ai][area-route][area-route-live]") {
  LiveRouteFixture fixture;
  fixture.routes[kRoute] =
      Endpoint(12, 10) | (static_cast<u_int>(Endpoint(10, 13)) << 16);
  CHECK(fixture.Resolve() == static_cast<int>(CommandIntent::kMoveDestination));
  CHECK(fixture.state.destination.word == Tile(12, 10));
  CHECK(fixture.state.reserved154Area == kArea);

  fixture.heights[10][12] = 0x400;
  CHECK(fixture.Resolve() == static_cast<int>(CommandIntent::kMoveDestination));
  CHECK(fixture.state.destination.word == Tile(10, 13));

  fixture.routes[kRoute] = static_cast<u_int>(Endpoint(10, 13)) << 16;
  fixture.state.reserved154Area = 1;
  fixture.state.destination.word = Tile(1, 1);
  CHECK(fixture.Resolve() == static_cast<int>(CommandIntent::kMoveDestination));
  CHECK(fixture.state.destination.word == Tile(1, 1));
  CHECK(fixture.state.reserved154Area == 1);
  CHECK(fixture.state.tile.word == Tile(10, 10, 0x5A000700));
}
