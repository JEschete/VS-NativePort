#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/Waypoints.hpp"
#include "Battle/Ai/WaypointsLive.hpp"
#include "Content/ModuleData.h"
#include "Field/TerrainHeight.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/Navigation.h"
}

namespace {
namespace ai = VS::Battle::Ai;
namespace field = VS::Field;
using Row = std::array<std::uint16_t, ai::kPatrolWaypointsPerRow>;
using Waypoints = std::array<std::uint16_t, ai::kRoomWaypointCount>;
using Routes = std::array<std::uint16_t, 4 * ai::kPatrolWaypointsPerRow>;

constexpr std::uint16_t Waypoint(int x, int z, bool active = true) {
  return static_cast<std::uint16_t>((active ? ai::kWaypointActive : 0U) |
                                    (static_cast<unsigned>(z) << 5) |
                                    static_cast<unsigned>(x));
}

constexpr std::int16_t Centre(int tile) {
  return static_cast<std::int16_t>((tile * 128) + 64);
}

struct Heights {
  field::TerrainRow rows[field::kTerrainGridSide]{};

  [[nodiscard]] field::TerrainGrid Grid() const {
    return field::TerrainGrid{
        std::span<const field::TerrainRow, field::kTerrainGridSide>(rows)};
  }
};

// Returns the scripted draws in order and records each range.
class ScriptedRandom final : public ai::PatrolRandom {
 public:
  explicit ScriptedRandom(std::vector<int> draws) : draws_(std::move(draws)) {}

  int NextMasked(int range) override {
    ranges.push_back(range);
    return draws_.at(ranges.size() - 1);
  }

  std::vector<int> ranges;

 private:
  std::vector<int> draws_;
};

// Row 1 patrols (4, 4), (8, 8) and (1, 1) with an inactive waypoint between
// the first two; row 3 holds no active waypoint.
Routes PatrolRoutes() {
  Routes routes{};
  const std::size_t row = ai::kPatrolWaypointsPerRow;
  routes[row + 0] = Waypoint(4, 4);
  routes[row + 1] = Waypoint(6, 6, false);
  routes[row + 2] = Waypoint(8, 8);
  routes[row + 7] = Waypoint(1, 1);
  routes[(3 * row) + 2] = Waypoint(5, 5, false);
  return routes;
}

ai::PatrolCursor Cursor(std::uint8_t index, std::uint8_t direction,
                        std::uint8_t cooldown = 0) {
  return {.index = index,
          .row = 1,
          .rate = 3,
          .direction = direction,
          .cooldown = cooldown};
}

constexpr std::uint8_t kBackward = 0xFF;

using Extent = std::pair<const std::uint16_t*, std::size_t>;

std::optional<Extent> ExtentOf(
    const std::optional<std::span<const std::uint16_t>>& waypoints) {
  if (!waypoints) {
    return std::nullopt;
  }
  return Extent{waypoints->data(), waypoints->size()};
}

// Empty when the search is refused or finds no active waypoint.
std::optional<ai::NearestWaypoint> NearestOf(
    const std::optional<ai::WaypointSearch>& search) {
  if (!search) {
    return std::nullopt;
  }
  return search->nearest;
}

std::optional<ai::NearestWaypoint> SearchFrom(const Waypoints& waypoints,
                                              std::int16_t y,
                                              const Heights& heights) {
  return NearestOf(ai::FindNearestActiveWaypoint(
      waypoints, {.x = Centre(4), .y = y, .z = Centre(4)}, heights.Grid()));
}
}  // namespace

TEST_CASE("Waypoint halfwords pack an active flag and a five-bit tile",
          "[ai][waypoints]") {
  CHECK(ai::IsActiveWaypoint(0x8000));
  CHECK_FALSE(ai::IsActiveWaypoint(0x7FFF));
  CHECK(ai::WaypointTileOf(0x83E1) == ai::WaypointTile{.x = 1, .z = 31});
  CHECK(ai::WaypointTileOf(0xFC00 | (5U << 5) | 3U) ==
        ai::WaypointTile{.x = 3, .z = 5});
}

TEST_CASE("Waypoint values read inside the row and refuse other indices",
          "[ai][waypoints]") {
  const Row row{Waypoint(1, 2), Waypoint(3, 4, false), 7, 8, 9, 10, 11, 12};
  CHECK(ai::WaypointValue(row, 0) == Waypoint(1, 2));
  CHECK(ai::WaypointValue(row, 1) == Waypoint(3, 4, false));
  CHECK(ai::WaypointValue(row, 7) == 12);
  CHECK_FALSE(ai::WaypointValue(row, 8));
  CHECK_FALSE(ai::WaypointValue(row, -1));
}

TEST_CASE("Waypoint tails start inside the row or at its end",
          "[ai][waypoints]") {
  const Row row{Waypoint(1, 2), Waypoint(3, 4, false), 7, 8, 9, 10, 11, 12};
  CHECK(ExtentOf(ai::WaypointsFrom(row, 6)) ==
        Extent{row.data() + 6, std::size_t{2}});
  CHECK(ExtentOf(ai::WaypointsFrom(row, 8)) ==
        Extent{row.data() + 8, std::size_t{0}});
  CHECK_FALSE(ai::WaypointsFrom(row, 9));
  CHECK_FALSE(ai::WaypointsFrom(row, -1));
}

TEST_CASE("Nearest waypoint search skips inactive waypoints and keeps a tie",
          "[ai][waypoints]") {
  const Heights heights;
  Waypoints waypoints{};
  waypoints[0] = Waypoint(4, 4, false);
  waypoints[1] = Waypoint(5, 3);
  waypoints[2] = Waypoint(3, 5);
  waypoints[3] = Waypoint(9, 9);
  CHECK(SearchFrom(waypoints, 0, heights) ==
        ai::NearestWaypoint{.tile = {.x = 5, .z = 3},
                            .squared_distance = 2U * 128U * 128U});

  std::swap(waypoints[1], waypoints[2]);
  CHECK(SearchFrom(waypoints, 0, heights) ==
        ai::NearestWaypoint{.tile = {.x = 3, .z = 5},
                            .squared_distance = 2U * 128U * 128U});
}

TEST_CASE("Nearest waypoint distance rises with the special terrain height",
          "[ai][waypoints]") {
  Heights heights;
  heights.rows[4][5] = 0x0010;
  const int raised =
      field::GroundHeightAtWorldPosition(heights.Grid(), Centre(5), Centre(4));
  REQUIRE(raised != 0);
  Waypoints waypoints{};
  waypoints[0] = Waypoint(5, 4);
  waypoints[1] = Waypoint(3, 4);

  CHECK(SearchFrom(waypoints, 0, heights) ==
        ai::NearestWaypoint{.tile = {.x = 3, .z = 4},
                            .squared_distance = 128U * 128U});
  CHECK(SearchFrom(waypoints, static_cast<std::int16_t>(raised), heights) ==
        ai::NearestWaypoint{.tile = {.x = 5, .z = 4},
                            .squared_distance = 128U * 128U});
}

TEST_CASE("Waypoint searches need special terrain only for active waypoints",
          "[ai][waypoints]") {
  Waypoints waypoints{};
  waypoints[2] = Waypoint(5, 5, false);
  const auto none =
      ai::FindNearestActiveWaypoint(waypoints, {}, field::TerrainGrid{});
  CHECK(none.has_value());
  CHECK_FALSE(NearestOf(none).has_value());
  waypoints[6] = Waypoint(5, 5);
  CHECK_FALSE(
      ai::FindNearestActiveWaypoint(waypoints, {}, field::TerrainGrid{}));
}

TEST_CASE("Patrols at their waypoint step to the next active one and wait",
          "[ai][waypoints][patrol]") {
  const auto routes = PatrolRoutes();
  ScriptedRandom random({});

  auto cursor = Cursor(0, 1);
  CHECK(ai::UpdateWaypointPatrol(cursor, routes, {.x = 4, .z = 4}, random) ==
        ai::WaypointTile{.x = 8, .z = 8});
  CHECK(cursor ==
        ai::PatrolCursor{
            .index = 2, .row = 1, .rate = 3, .direction = 1, .cooldown = 10});

  cursor = Cursor(0, kBackward, 4);
  CHECK(ai::UpdateWaypointPatrol(cursor, routes, {.x = 4, .z = 4}, random) ==
        ai::WaypointTile{.x = 1, .z = 1});
  CHECK(cursor.index == 7);
  CHECK(cursor.cooldown == 10);
  CHECK(random.ranges.empty());
}

TEST_CASE("Patrols at their waypoint wrap forward or hold without a direction",
          "[ai][waypoints][patrol]") {
  const auto routes = PatrolRoutes();
  ScriptedRandom random({});

  auto cursor = Cursor(7, 1);
  CHECK(ai::UpdateWaypointPatrol(cursor, routes, {.x = 1, .z = 1}, random) ==
        ai::WaypointTile{.x = 4, .z = 4});
  CHECK(cursor.index == 0);

  cursor = Cursor(2, 0);
  CHECK(ai::UpdateWaypointPatrol(cursor, routes, {.x = 8, .z = 8}, random) ==
        ai::WaypointTile{.x = 8, .z = 8});
  CHECK(cursor.index == 2);
  CHECK(cursor.cooldown == 10);
  CHECK(random.ranges.empty());
}

TEST_CASE("Patrols away from their waypoint keep walking on a nonzero draw",
          "[ai][waypoints][patrol]") {
  const auto routes = PatrolRoutes();
  const int cooldown = GENERATE(0, 1, 4);
  ScriptedRandom random({5});
  auto cursor = Cursor(2, 1, static_cast<std::uint8_t>(cooldown));
  CHECK(ai::UpdateWaypointPatrol(cursor, routes, {.x = 5, .z = 5}, random) ==
        ai::WaypointTile{.x = 8, .z = 8});
  CHECK(random.ranges == std::vector<int>{8});
  CHECK(cursor == ai::PatrolCursor{.index = 2,
                                   .row = 1,
                                   .rate = 3,
                                   .direction = 1,
                                   .cooldown = static_cast<std::uint8_t>(
                                       cooldown == 0 ? 0 : cooldown - 1)});
}

TEST_CASE("Patrols keep walking after a zero draw until the cooldown ends",
          "[ai][waypoints][patrol]") {
  const auto routes = PatrolRoutes();
  ScriptedRandom random({0, 0, 0});
  auto cursor = Cursor(2, 1, 2);
  CHECK(ai::UpdateWaypointPatrol(cursor, routes, {.x = 5, .z = 5}, random) ==
        ai::WaypointTile{.x = 8, .z = 8});
  CHECK(cursor.cooldown == 1);
  CHECK(cursor.direction == 1);

  CHECK(ai::UpdateWaypointPatrol(cursor, routes, {.x = 5, .z = 5}, random) ==
        ai::WaypointTile{.x = 4, .z = 4});
  CHECK(cursor == ai::PatrolCursor{.index = 0,
                                   .row = 1,
                                   .rate = 3,
                                   .direction = kBackward,
                                   .cooldown = 10});

  cursor.cooldown = 0;
  CHECK(ai::UpdateWaypointPatrol(cursor, routes, {.x = 5, .z = 5}, random) ==
        ai::WaypointTile{.x = 8, .z = 8});
  CHECK(cursor.index == 2);
  CHECK(cursor.direction == 1);
  CHECK(random.ranges.size() == 3);
}

TEST_CASE("Patrol draws pass the rate's low five bits as a signed range",
          "[ai][waypoints][patrol]") {
  const auto [rate, range] = GENERATE(
      std::pair{0, 1}, std::pair{3, 8}, std::pair{30, 1 << 30},
      std::pair{31, INT_MIN}, std::pair{33, 2}, std::pair{255, INT_MIN});
  const auto routes = PatrolRoutes();
  ScriptedRandom random({1});
  auto cursor = Cursor(2, 1);
  cursor.rate = static_cast<std::uint8_t>(rate);
  CHECK(ai::UpdateWaypointPatrol(cursor, routes, {.x = 5, .z = 5}, random));
  CHECK(random.ranges == std::vector<int>{range});
}

TEST_CASE("Patrols refuse rows, indices and steps without active waypoints",
          "[ai][waypoints][patrol]") {
  const auto routes = PatrolRoutes();
  ScriptedRandom random({0});
  auto cursor = Cursor(0, 1);
  ai::WaypointTile tile{.x = 4, .z = 4};
  SECTION("row outside the routes") { cursor.row = 4; }
  SECTION("index outside the row") { cursor.index = 8; }
  SECTION("on a row with no active waypoint") {
    cursor.row = 3;
    tile = {.x = 0, .z = 0};
  }
  SECTION("turning back on a row with no active waypoint") {
    cursor.row = 3;
    cursor.index = 2;
  }
  const auto before = cursor;
  CHECK_FALSE(ai::UpdateWaypointPatrol(cursor, routes, tile, random));
  CHECK(cursor == before);
  CHECK(random.ranges.size() <= 1);
}

TEST_CASE("Live waypoint search writes only the nearest active tile",
          "[ai][waypoints][waypoints-live]") {
  auto* const saved_waypoints = Battle_Ai_roomWaypoints;
  auto* const saved_heights = Battle_Ai_specialTerrainHeights;
  Waypoints waypoints{};
  u_short heights[32][32]{};
  Battle_Ai_roomWaypoints = waypoints.data();
  Battle_Ai_specialTerrainHeights = heights;
  Battle_Ai_ActorAiState state{};
  state.position.vx = Centre(4);
  state.position.vz = Centre(4);
  Battle_Ai_WaypointTile destination{
      .x = 0xAA, .reserved1 = 0x11, .z = 0xBB, .reserved3 = 0x22};
  auto* const view = reinterpret_cast<Battle_Ai_WaypointSearchView*>(&state);

  waypoints[1] = Waypoint(9, 9, false);
  ai::FindLiveNearestActiveWaypoint(view, &destination);
  CHECK(destination.x == 0xAA);
  CHECK(destination.z == 0xBB);

  waypoints[3] = Waypoint(6, 4);
  waypoints[5] = Waypoint(4, 5);
  ai::FindLiveNearestActiveWaypoint(view, &destination);
  CHECK(destination.x == 4);
  CHECK(destination.reserved1 == 0x11);
  CHECK(destination.z == 5);
  CHECK(destination.reserved3 == 0x22);
  Battle_Ai_roomWaypoints = saved_waypoints;
  Battle_Ai_specialTerrainHeights = saved_heights;
}

TEST_CASE("Waypoint routines keep their original module entries",
          "[ai][waypoints][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [symbols](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800DD7CCU) ==
        reinterpret_cast<void*>(&ai::FindLiveNearestActiveWaypoint));
  CHECK(resolve(0x800E23ACU) ==
        reinterpret_cast<void*>(&ai::UpdateLiveWaypointPatrol));
}
