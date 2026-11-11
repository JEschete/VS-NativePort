#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/RadialMovement.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/ScalarMath.hpp"
#include "Field/TerrainHeight.hpp"

namespace {
namespace ai = VS::Battle::Ai;
namespace field = VS::Field;
using ai::CommandIntent;

// Direction i steps (0, -1), (-1, -1), (-1, 0), (-1, 1), (0, 1), (1, 1),
// (1, 0) and (1, -1).
constexpr ai::NavigationDirectionCodes DirectionCodes() {
  return {0x30, 0x33, 0x03, 0x13, 0x10, 0x11, 0x01, 0x31};
}
constexpr ai::NavigationDirectionCodes kCodes = DirectionCodes();
// Bytes 1 and 3 of a tile word, which carry per-actor metadata.
constexpr std::uint32_t kTag = 0x5A000700;
constexpr std::uint32_t kNoSavedDestination = 0x800000;
constexpr std::uint16_t kTurnPeriod = 30;
constexpr std::uint8_t kNoTarget = 16;

// Angles whose projection heading (0xC00 - angle) points along +x, -x, +z
// and -z.
constexpr int kAlongPlusX = 0xC00;
constexpr int kAlongMinusX = 0x400;
constexpr int kAlongPlusZ = 0x800;
constexpr int kAlongMinusZ = 0;

// Terrain cells: the signed 10-bit height in 64-unit steps.
constexpr std::uint16_t kHeight128 = 2;
constexpr std::uint16_t kHeight192 = 3;

constexpr std::uint32_t Tile(int x, int z, std::uint32_t other = 0) {
  return other | (static_cast<std::uint32_t>(z) << 16) |
         static_cast<std::uint32_t>(x);
}

constexpr std::int16_t Centre(int tile) {
  return static_cast<std::int16_t>((tile * 128) + 64);
}

int Angle(int delta_x, int delta_z) {
  return VS::Core::Math::Arctangent2(delta_x, delta_z);
}

struct Heights {
  field::TerrainRow rows[field::kTerrainGridSide]{};

  [[nodiscard]] field::TerrainGrid Grid() const {
    return field::TerrainGrid{
        std::span<const field::TerrainRow, field::kTerrainGridSide>(rows)};
  }
};

enum class Event { kRelease, kBeginTerrain };

struct FakeRadialWorld final : ai::RadialMovementWorld {
  std::optional<ai::RoomTileBounds> bounds =
      ai::RoomTileBounds{.min_x = 1, .min_z = 1, .max_x = 30, .max_z = 30};
  std::uint32_t grid = 32U | (32U << 8);
  Heights heights;
  // Queued occupancy answers; `otherwise` once they run out.
  std::deque<std::optional<int>> occupancy;
  std::optional<int> otherwise = 1;
  std::vector<std::uint32_t> queried;
  std::vector<std::uint32_t> destinations_seen;
  std::array<std::optional<ai::RadialPoint>, 17> actors{};
  std::vector<CommandIntent> commands;
  std::vector<std::uint16_t> timers_at_command;
  std::vector<int> toward;
  std::vector<std::uint32_t> toward_destinations;
  std::vector<Event> events;

  [[nodiscard]] ai::NavigationDirectionCodes DirectionCodes() const override {
    return kCodes;
  }
  [[nodiscard]] std::optional<ai::RoomTileBounds> RoomBounds() const override {
    return bounds;
  }
  [[nodiscard]] std::uint32_t GridBounds() const override { return grid; }
  [[nodiscard]] field::TerrainGrid Terrain() const override {
    return heights.Grid();
  }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      ai::RadialMover& mover, std::uint32_t tile) override {
    queried.push_back(tile);
    destinations_seen.push_back(mover.destination);
    if (occupancy.empty()) return otherwise;
    const auto answer = occupancy.front();
    occupancy.pop_front();
    return answer;
  }
  [[nodiscard]] std::optional<ai::RadialPoint> ActorPosition(
      int actor_id) const override {
    if (actor_id < 0 || static_cast<std::size_t>(actor_id) >= actors.size()) {
      return std::nullopt;
    }
    return actors[static_cast<std::size_t>(actor_id)];
  }
  void IssueCommand(ai::RadialMover& mover, CommandIntent intent) override {
    commands.push_back(intent);
    timers_at_command.push_back(mover.turn_timer);
  }
  void MoveTowardDestinationTile(ai::RadialMover& mover, int angle) override {
    toward.push_back(angle);
    toward_destinations.push_back(mover.destination);
  }
  void ReleaseReservations(ai::RadialMover& /*mover*/) override {
    events.push_back(Event::kRelease);
  }
  void BeginTerrainMovement(ai::RadialMover& /*mover*/) override {
    events.push_back(Event::kBeginTerrain);
  }
};

// On tile (10, 10) at its centre, walking to tile (14, 10).
ai::RadialMover Mover() {
  return {.tile = Tile(10, 10, kTag),
          .saved_destination = kNoSavedDestination,
          .x = Centre(10),
          .z = Centre(10),
          .move_distance = 256,
          .turn_period = kTurnPeriod,
          .movement_target = kNoTarget,
          .destination = Tile(14, 10, kTag)};
}

using Tiles = std::vector<std::uint32_t>;
using Commands = std::vector<CommandIntent>;

ai::RoomTileBounds Bounds(int min_x, int min_z, int max_x, int max_z) {
  return {.min_x = static_cast<std::uint8_t>(min_x),
          .min_z = static_cast<std::uint8_t>(min_z),
          .max_x = static_cast<std::uint8_t>(max_x),
          .max_z = static_cast<std::uint8_t>(max_z)};
}
}  // namespace

// F01735 func_800E1388 (BATTLE/BATTLE.PRG/771A8.c:1330).
TEST_CASE("Radial projections walk the distance along the angle",
          "[ai][radial-movement]") {
  const auto project = [](int angle) {
    FakeRadialWorld world;
    auto mover = Mover();
    mover.destination = Tile(1, 1, kTag);
    REQUIRE(ai::ProjectClearRadialDestination(world, mover, Tile(10, 10, kTag),
                                              angle, 256));
    // The free test sees the old destination; the tag bytes are zero.
    CHECK(world.destinations_seen == Tiles{Tile(1, 1, kTag)});
    CHECK(world.queried == Tiles{mover.destination});
    return mover.destination;
  };
  CHECK(project(kAlongPlusX) == Tile(12, 10));
  CHECK(project(kAlongMinusX) == Tile(8, 10));
  CHECK(project(kAlongPlusZ) == Tile(10, 12));
  CHECK(project(kAlongMinusZ) == Tile(10, 8));
}

TEST_CASE("Radial projections shorten by an eighth until a tile is free",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  world.occupancy = {0, -1, 1};
  auto mover = Mover();
  REQUIRE(ai::ProjectClearRadialDestination(world, mover, Tile(10, 10),
                                            kAlongPlusX, 1024));
  CHECK(world.queried == Tiles{Tile(18, 10), Tile(17, 10), Tile(16, 10)});
  CHECK(mover.destination == Tile(16, 10));
}

TEST_CASE("Radial projections keep the eighth try when no tile is free",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  world.otherwise = 0;
  auto mover = Mover();
  REQUIRE(ai::ProjectClearRadialDestination(world, mover, Tile(10, 10),
                                            kAlongPlusX, 1024));
  CHECK(world.queried == Tiles{Tile(18, 10), Tile(17, 10), Tile(16, 10),
                               Tile(15, 10), Tile(14, 10), Tile(13, 10),
                               Tile(12, 10), Tile(11, 10)});
  CHECK(mover.destination == Tile(11, 10));
}

TEST_CASE("Radial projections clamp the free tile to the room's bounds",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  world.bounds = Bounds(1, 12, 20, 30);
  auto mover = Mover();
  REQUIRE(ai::ProjectClearRadialDestination(world, mover, Tile(18, 10),
                                            kAlongPlusX, 1024));
  CHECK(world.queried == Tiles{Tile(26, 10)});
  CHECK(mover.destination == Tile(20, 12));

  // A tile before the room's edge wraps to a byte and clamps to the far bound.
  world.queried.clear();
  REQUIRE(ai::ProjectClearRadialDestination(world, mover, Tile(2, 20),
                                            kAlongMinusX, 1024));
  CHECK(world.queried == Tiles{Tile(250, 20)});
  CHECK(mover.destination == Tile(20, 20));
}

TEST_CASE("Radial projections refuse a room without bounds or free answers",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  world.bounds.reset();
  auto mover = Mover();
  CHECK_FALSE(ai::ProjectClearRadialDestination(world, mover, Tile(10, 10),
                                                kAlongPlusX, 256));
  CHECK(world.queried.empty());
  CHECK(mover == Mover());

  FakeRadialWorld refusing;
  refusing.occupancy = {0, std::nullopt};
  CHECK_FALSE(ai::ProjectClearRadialDestination(refusing, mover, Tile(10, 10),
                                                kAlongPlusX, 1024));
  CHECK(refusing.queried.size() == 2);
  CHECK(mover == Mover());
}

// F01736 func_800E153C (BATTLE/BATTLE.PRG/771A8.c:1389).
TEST_CASE("Radial moves on the destination's tile recover",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  auto mover = Mover();
  mover.destination = Tile(10, 10, 0x12003400);
  mover.toward_point = 1;
  mover.target_x = Centre(12);
  mover.target_z = Centre(10);
  REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
  CHECK(world.queried.empty());
  CHECK(world.commands == Commands{CommandIntent::kSetHeading});
  CHECK(mover.heading == (Angle(-256, 0) & 0xFFF));
  CHECK(mover.turn_timer == kTurnPeriod);
}

TEST_CASE("Radial moves reproject a destination beyond the move distance",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  auto mover = Mover();
  mover.approach_directly = 1;
  mover.movement_flags = 2;
  REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
  // The angle from the destination's centre, projected back from the tile.
  const int angle = Angle(-512, 0);
  CHECK(world.queried == Tiles{Tile(12, 10)});
  CHECK(world.toward == std::vector<int>{angle});
  CHECK(world.toward_destinations == Tiles{Tile(12, 10)});
  CHECK(mover.destination == Tile(12, 10));
  CHECK(world.commands.empty());
}

TEST_CASE("Radial moves keep a destination within the move distance",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  auto mover = Mover();
  mover.move_distance = 512;
  mover.approach_directly = 1;
  mover.movement_flags = 4;
  REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
  CHECK(world.queried.empty());
  CHECK(world.toward_destinations == Tiles{Tile(14, 10, kTag)});
}

TEST_CASE("The squared move distance wraps to 32 bits",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  auto mover = Mover();
  mover.destination = Tile(12, 10, kTag);
  mover.move_distance = 0x10000;
  mover.approach_directly = 1;
  mover.movement_flags = 2;
  REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
  // 0x10000 squared is zero, so even a near destination is reprojected; the
  // projected tile wraps back onto the actor's.
  CHECK(world.queried == Tiles{Tile(10, 10)});
  CHECK(mover.destination == Tile(10, 10));
}

TEST_CASE("Direct radial movers step toward a near or direct destination",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  auto mover = Mover();
  mover.destination = Tile(12, 10, kTag);
  mover.movement_flags = 4;
  mover.saved_destination = Tile(3, 4, kTag);
  REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
  // Tile (12, 10) is within four tiles squared of (10, 10).
  CHECK(world.toward == std::vector<int>{Angle(-256, 0)});
  CHECK(world.toward_destinations == Tiles{Tile(3, 4, kTag)});
  CHECK(mover.destination == Tile(3, 4, kTag));
  CHECK(mover.move_attempts == 0);

  // Bit 23 marks the saved destination as unset.
  FakeRadialWorld unsaved;
  mover = Mover();
  mover.move_distance = 1024;
  mover.approach_directly = 1;
  mover.movement_flags = 2;
  mover.saved_destination = Tile(3, 4, kTag) | kNoSavedDestination;
  REQUIRE(ai::MoveToRadialDestination(unsaved, mover, 0));
  CHECK(unsaved.toward_destinations == Tiles{Tile(14, 10, kTag)});
}

TEST_CASE("Radial movers without direct steps count attempts",
          "[ai][radial-movement]") {
  const auto attempts = [](std::uint16_t flags, std::uint8_t direct,
                           std::uint32_t destination) {
    FakeRadialWorld world;
    auto mover = Mover();
    mover.move_distance = 1024;
    mover.movement_flags = flags;
    mover.approach_directly = direct;
    mover.destination = destination;
    REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
    CHECK(world.toward.empty());
    return mover.move_attempts;
  };
  CHECK(attempts(0x30, 1, Tile(14, 10)) == 1);
  // Four tiles away is not near.
  CHECK(attempts(6, 0, Tile(14, 10)) == 1);
  CHECK(attempts(6, 0, Tile(13, 13)) == 1);
}

TEST_CASE("Radial moves recover before the sixth attempt",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  auto mover = Mover();
  mover.move_distance = 1024;
  mover.move_attempts = 4;
  REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
  CHECK(mover.move_attempts == 5);
  CHECK(world.queried.empty());
  CHECK(world.commands == Commands{CommandIntent::kTurn});
}

TEST_CASE("The sixth radial attempt walks to a free destination",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  auto mover = Mover();
  mover.move_distance = 1024;
  mover.move_attempts = 5;
  REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
  CHECK(world.queried == Tiles{Tile(14, 10, kTag)});
  CHECK(mover.move_attempts == 0);
  CHECK(mover.next_tile == Tile(14, 10, kTag));
  CHECK(world.commands == Commands{CommandIntent::kWalkToTile});
}

TEST_CASE("The sixth radial attempt walks to the first free axis neighbour",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  world.occupancy = {0, 0, -1, 1};
  auto mover = Mover();
  mover.move_distance = 1024;
  mover.move_attempts = 5;
  REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
  CHECK(world.queried == Tiles{Tile(14, 10, kTag), Tile(14, 9, kTag),
                               Tile(13, 10, kTag), Tile(14, 11, kTag)});
  CHECK(mover.destination == Tile(14, 11, kTag));
  CHECK(mover.next_tile == Tile(14, 11, kTag));
  CHECK(mover.move_attempts == 0);
  CHECK(world.commands == Commands{CommandIntent::kWalkToTile});

  // Neighbours wrap to a byte.
  FakeRadialWorld edge;
  edge.occupancy = {0, 1};
  mover = Mover();
  mover.tile = Tile(1, 1);
  mover.x = Centre(1);
  mover.z = Centre(1);
  mover.move_distance = 1024;
  mover.move_attempts = 5;
  mover.destination = Tile(0, 0, kTag);
  REQUIRE(ai::MoveToRadialDestination(edge, mover, 0));
  CHECK(mover.destination == Tile(0, 255, kTag));
}

TEST_CASE("The sixth radial attempt recovers without a free tile",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  world.otherwise = 0;
  auto mover = Mover();
  mover.move_distance = 1024;
  mover.move_attempts = 5;
  REQUIRE(ai::MoveToRadialDestination(world, mover, 0));
  CHECK(world.queried.size() == 5);
  CHECK(mover.move_attempts == 6);
  CHECK(mover.destination == Tile(14, 10, kTag));
  CHECK(world.commands == Commands{CommandIntent::kTurn});

  FakeRadialWorld refusing;
  refusing.occupancy = {0, std::nullopt};
  mover = Mover();
  mover.move_distance = 1024;
  mover.move_attempts = 5;
  CHECK_FALSE(ai::MoveToRadialDestination(refusing, mover, 0));
  CHECK(refusing.commands.empty());
}

// F01737 func_800E1764 (BATTLE/BATTLE.PRG/771A8.c:1437).
TEST_CASE("Retreats keep the projection farther from the move point",
          "[ai][radial-movement]") {
  const auto retreat = [](int move_tile) {
    FakeRadialWorld world;
    auto mover = Mover();
    mover.move_x = Centre(move_tile);
    mover.move_z = Centre(10);
    REQUIRE(ai::MoveAwayFromReference(world, mover, kAlongPlusX));
    CHECK(world.queried == Tiles{Tile(12, 10), Tile(8, 10)});
    // Either destination is within the move distance, so the walk counts an
    // attempt and recovers.
    CHECK(mover.move_attempts == 1);
    CHECK(world.commands == Commands{CommandIntent::kTurn});
    return mover.destination;
  };
  CHECK(retreat(6) == Tile(12, 10));
  CHECK(retreat(14) == Tile(8, 10));
  // A tie keeps the opposite projection.
  CHECK(retreat(10) == Tile(8, 10));
}

TEST_CASE("Retreats refuse a room without bounds", "[ai][radial-movement]") {
  FakeRadialWorld world;
  world.bounds.reset();
  auto mover = Mover();
  CHECK_FALSE(ai::MoveAwayFromReference(world, mover, kAlongPlusX));
  CHECK(world.commands.empty());

  FakeRadialWorld refusing;
  refusing.occupancy = {1, std::nullopt};
  CHECK_FALSE(ai::MoveAwayFromReference(refusing, mover, kAlongPlusX));
  CHECK(refusing.queried.size() == 2);
}

// F01750 func_800E2CCC (BATTLE/BATTLE.PRG/771A8.c:2272).
TEST_CASE("Stalled fliers on a free tile restart terrain movement",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  auto mover = Mover();
  mover.flies = true;
  mover.stall_counter = 121;
  mover.movement_flags = 0x12;
  REQUIRE(ai::RecoverMovement(world, mover));
  CHECK(mover.stall_counter == 122);
  CHECK(world.queried == Tiles{Tile(10, 10, kTag)});
  CHECK(world.events == std::vector{Event::kRelease, Event::kBeginTerrain});
  CHECK(world.commands.empty());
}

namespace {
// Recovers a flier with the given stall count and flags on a tile answering
// `occupancy`; returns the stall count and the number of occupancy queries.
std::pair<std::int32_t, std::size_t> RecoverFlier(bool flies,
                                                  std::int32_t stalled,
                                                  std::uint16_t flags,
                                                  int occupancy) {
  FakeRadialWorld world;
  world.otherwise = occupancy;
  auto mover = Mover();
  mover.flies = flies;
  mover.stall_counter = stalled;
  mover.movement_flags = flags;
  REQUIRE(ai::RecoverMovement(world, mover));
  CHECK(world.events.empty());
  CHECK(world.commands == Commands{CommandIntent::kTurn});
  return {mover.stall_counter, world.queried.size()};
}
}  // namespace

TEST_CASE("Fliers below the stall limit or without the flags count and turn",
          "[ai][radial-movement]") {
  CHECK(RecoverFlier(true, 120, 0x12, 1) == std::pair{121, std::size_t{0}});
  CHECK(RecoverFlier(true, 121, 0x30, 1) == std::pair{122, std::size_t{0}});
  CHECK(RecoverFlier(true, 121, 0x04, 1) == std::pair{122, std::size_t{0}});
  CHECK(RecoverFlier(true, std::numeric_limits<std::int32_t>::max(), 0x30, 1) ==
        std::pair{std::numeric_limits<std::int32_t>::min(), std::size_t{0}});
  CHECK(RecoverFlier(false, 500, 0x12, 1) == std::pair{500, std::size_t{0}});
}

TEST_CASE("Stalled fliers on a tile that is not free count and turn",
          "[ai][radial-movement]") {
  CHECK(RecoverFlier(true, 121, 0x24, 0) == std::pair{122, std::size_t{1}});
  CHECK(RecoverFlier(true, 121, 0x24, -1) == std::pair{122, std::size_t{1}});

  FakeRadialWorld refusing;
  refusing.otherwise.reset();
  auto mover = Mover();
  mover.flies = true;
  mover.stall_counter = 121;
  mover.movement_flags = 0x12;
  CHECK_FALSE(ai::RecoverMovement(refusing, mover));
}

TEST_CASE("Recovering movers turn toward their target actor",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  world.actors[3] = ai::RadialPoint{.x = Centre(10), .z = Centre(13)};
  auto mover = Mover();
  mover.movement_target = 3;
  mover.toward_point = 1;
  mover.turn_timer = 7;
  REQUIRE(ai::RecoverMovement(world, mover));
  CHECK(mover.angle == (Angle(0, -384) & 0xFFF));
  CHECK(world.commands == Commands{CommandIntent::kTurn});
  CHECK(world.timers_at_command == std::vector<std::uint16_t>{kTurnPeriod});

  FakeRadialWorld missing;
  mover = Mover();
  mover.movement_target = 4;
  CHECK_FALSE(ai::RecoverMovement(missing, mover));
  CHECK(missing.commands.empty());
}

TEST_CASE("Recovering movers turn toward their target point",
          "[ai][radial-movement]") {
  FakeRadialWorld point;
  auto mover = Mover();
  mover.toward_point = 1;
  mover.target_x = Centre(7);
  mover.target_z = Centre(9);
  REQUIRE(ai::RecoverMovement(point, mover));
  CHECK(mover.heading == (Angle(384, 128) & 0xFFF));
  CHECK(mover.angle == 0);
  CHECK(point.commands == Commands{CommandIntent::kSetHeading});
  CHECK(mover.turn_timer == kTurnPeriod);
}

namespace {
// The angle a recovering mover facing `facing` + `look` turns to.
int RecoveredAngle(const Heights& heights, std::int16_t facing,
                   std::int16_t look) {
  FakeRadialWorld world;
  world.heights = heights;
  auto mover = Mover();
  mover.model_facing = facing;
  mover.model_look_yaw = look;
  REQUIRE(ai::RecoverMovement(world, mover));
  CHECK(world.commands == Commands{CommandIntent::kTurn});
  CHECK(mover.turn_timer == kTurnPeriod);
  return static_cast<int>(mover.angle);
}
}  // namespace

TEST_CASE("Recovering movers on level ground keep the facing quadrant",
          "[ai][radial-movement]") {
  const Heights flat;
  // Equal neighbours keep the first, starting from the facing quadrant.
  CHECK(RecoveredAngle(flat, 0, 0) == 0);
  CHECK(RecoveredAngle(flat, 0x200, 0) == 0);
  CHECK(RecoveredAngle(flat, 0x100, 0x101) == 2 << 9);
  CHECK(RecoveredAngle(flat, 0xC01, 0) == 6 << 9);
  CHECK(RecoveredAngle(flat, -1, 0) == 0);
}

TEST_CASE("Recovering movers turn toward the highest axis neighbour",
          "[ai][radial-movement]") {
  Heights east;
  east.rows[10][11] = kHeight192;
  CHECK(RecoveredAngle(east, 0, 0) == 6 << 9);
  // The search adds a growing stride: from 0 it visits 0, 2, 6 and 4.
  Heights tied;
  tied.rows[10][11] = kHeight128;
  tied.rows[11][10] = kHeight128;
  CHECK(RecoveredAngle(tied, 0, 0) == 6 << 9);
  CHECK(RecoveredAngle(tied, 0x800, 0) == 4 << 9);
}

TEST_CASE("Recovering movers stand without a neighbour inside the grid",
          "[ai][radial-movement]") {
  FakeRadialWorld world;
  world.grid = 1U | (1U << 8);
  auto mover = Mover();
  mover.tile = Tile(0, 0);
  mover.turn_timer = 7;
  mover.angle = 99;
  REQUIRE(ai::RecoverMovement(world, mover));
  CHECK(world.commands == Commands{CommandIntent::kStand});
  CHECK(mover.turn_timer == 7);
  CHECK(mover.angle == 99);

  // A neighbour inside the grid but outside the terrain refuses.
  FakeRadialWorld outside;
  outside.grid = 0xFFFF;
  mover = Mover();
  mover.tile = Tile(31, 10);
  CHECK_FALSE(ai::RecoverMovement(outside, mover));
  CHECK(outside.commands.empty());
}

TEST_CASE("Radial movement keeps its original module entries",
          "[ai][radial-movement][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [symbols](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800E1388U) ==
        reinterpret_cast<void*>(&ai::ProjectLiveClearRadialDestination));
  CHECK(resolve(0x800E153CU) ==
        reinterpret_cast<void*>(&ai::MoveLiveToRadialDestination));
  CHECK(resolve(0x800E1764U) ==
        reinterpret_cast<void*>(&ai::MoveLiveAwayFromReference));
  CHECK(resolve(0x800E2CCCU) ==
        reinterpret_cast<void*>(&ai::RecoverLiveMovement));
}
