#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/FlightMovement.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Field/TerrainHeight.hpp"

namespace {
namespace ai = VS::Battle::Ai;
namespace field = VS::Field;

// Direction i steps (0, -1), (-1, -1), (-1, 0), (-1, 1), (0, 1), (1, 1),
// (1, 0) and (1, -1).
constexpr ai::NavigationDirectionCodes DirectionCodes() {
  return {0x30, 0x33, 0x03, 0x13, 0x10, 0x11, 0x01, 0x31};
}
constexpr int kNorth = 0;  // (0, -1)
constexpr int kEast = 6;   // (1, 0)
// The angle whose projection heading points along +x: toward a destination
// due east.
constexpr int kAlongPlusX = 0xC00;
constexpr int kAlongMinusX = 0x400;

constexpr std::uint16_t kNoFlight = 0x40;
constexpr std::uint16_t kSpecialTerrain = 0x20;
constexpr std::uint16_t kMayLand = 0x02;
constexpr int kMoveDirection = 5;
constexpr int kMoveHeading = 7;
constexpr int kMoveVector = 8;
constexpr std::int32_t kFall = -0x1000;

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value.has_value()) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

// Terrain cells: the signed 10-bit height in 64-unit steps.
constexpr std::uint16_t kHeight128 = 2;
constexpr std::uint16_t kHeight512 = 8;
constexpr std::uint16_t kDepth192 = 0x3FD;

constexpr std::uint32_t Tile(int x, int z) {
  return (static_cast<std::uint32_t>(z) << 16) | static_cast<std::uint32_t>(x);
}

constexpr std::int16_t Centre(int tile) {
  return static_cast<std::int16_t>((tile * 128) + 64);
}

constexpr std::int32_t Fixed(std::int32_t value) { return value * 0x1000; }

enum class Call : std::uint8_t {
  kPlatforms,
  kOccupancy,
  kCommand,
  kToward,
  kGround,
  kRelease,
  kBeginTerrain,
  kAlternate,
  kStep,
  kResolve,
  kRecover,
};

struct Note {
  Call call{};
  std::int64_t argument = 0;
  int extra = 0;
  friend bool operator==(const Note&, const Note&) = default;
};

struct FakeFlightWorld final : ai::FlightMovementWorld {
  ai::FlightDirectionCache cache;
  std::optional<ai::FlightRoom> room = ai::FlightRoom{
      .bounds = {.min_x = 1, .min_z = 1, .max_x = 30, .max_z = 30}};
  field::TerrainRow heights[field::kTerrainGridSide]{};
  std::uint16_t flags[field::kTerrainGridSide][field::kTerrainGridSide]{};
  std::vector<field::PlatformContact> platforms;
  std::vector<int> actors;
  std::size_t next_actor = 0;
  std::array<std::optional<ai::FlightObstacle>, 16> obstacles{};
  std::array<std::optional<ai::FlightTargetRecord>, 17> targets{};
  std::deque<std::optional<ai::FlightStep>> steps;
  // Movement results with the heading each leaves; 0 once they run out.
  std::deque<std::pair<int, std::int16_t>> resolutions;
  std::deque<int> occupancy;
  mutable std::vector<Note> notes;

  // Every direction tested and closed: the lowest reachable ground is zero.
  void CloseEveryDirection() {
    cache.fly_tested = 0xFF;
    cache.fly_open = 0;
  }

  [[nodiscard]] ai::NavigationDirectionCodes DirectionCodes() const override {
    return ::DirectionCodes();
  }
  [[nodiscard]] ai::FlightDirectionCache DirectionCache() const override {
    return cache;
  }
  void SetDirectionCache(const ai::FlightDirectionCache& value) override {
    cache = value;
  }
  [[nodiscard]] std::vector<field::PlatformContact> PlatformPositions()
      const override {
    notes.push_back({Call::kPlatforms});
    return platforms;
  }
  void RewindActors() override { next_actor = 0; }
  [[nodiscard]] std::optional<int> NextActor() override {
    if (next_actor >= actors.size()) return std::nullopt;
    return actors[next_actor++];
  }
  [[nodiscard]] std::optional<ai::FlightObstacle> Obstacle(
      int actor_id) const override {
    if (actor_id < 0 ||
        static_cast<std::size_t>(actor_id) >= obstacles.size()) {
      return std::nullopt;
    }
    return obstacles[static_cast<std::size_t>(actor_id)];
  }
  [[nodiscard]] std::optional<ai::FlightRoom> Room() const override {
    return room;
  }
  [[nodiscard]] std::optional<std::uint16_t> TileFlags(int x,
                                                       int z) const override {
    if (x < 0 || z < 0 || x >= field::kTerrainGridSide ||
        z >= field::kTerrainGridSide) {
      return std::nullopt;
    }
    return flags[z][x];
  }
  [[nodiscard]] field::TerrainGrid Terrain() const override {
    return field::TerrainGrid{
        std::span<const field::TerrainRow, field::kTerrainGridSide>(heights)};
  }
  [[nodiscard]] std::optional<ai::FlightTargetRecord> TargetRecord(
      int actor_id) const override {
    if (actor_id < 0 || static_cast<std::size_t>(actor_id) >= targets.size()) {
      return std::nullopt;
    }
    return targets[static_cast<std::size_t>(actor_id)];
  }
  [[nodiscard]] std::optional<ai::FlightStep> SelectVerticalStep(
      ai::FlightMover&) override {
    notes.push_back({Call::kStep});
    if (steps.empty()) return std::nullopt;
    const auto step = steps.front();
    steps.pop_front();
    return step;
  }
  [[nodiscard]] int ResolveDirectionalMovement(ai::FlightMover& mover,
                                               int angle, int mode) override {
    notes.push_back({Call::kResolve, angle, mode});
    if (resolutions.empty()) return 0;
    const auto [result, heading] = resolutions.front();
    resolutions.pop_front();
    mover.heading = heading;
    return result;
  }
  [[nodiscard]] int DestinationOccupancy(ai::FlightMover&,
                                         std::uint32_t tile) override {
    notes.push_back({Call::kOccupancy, tile});
    if (occupancy.empty()) return 0;
    const auto answer = occupancy.front();
    occupancy.pop_front();
    return answer;
  }
  void IssueCommand(ai::FlightMover&, int intent) override {
    notes.push_back({Call::kCommand, intent});
  }
  void Recover(ai::FlightMover&) override { notes.push_back({Call::kRecover}); }
  void ReleaseReservations(ai::FlightMover&) override {
    notes.push_back({Call::kRelease});
  }
  void BeginTerrainMovement(ai::FlightMover&) override {
    notes.push_back({Call::kBeginTerrain});
  }
  void BeginAlternateTerrainMovement(ai::FlightMover&) override {
    notes.push_back({Call::kAlternate});
  }
  void MoveTowardDestinationTile(ai::FlightMover&, int angle) override {
    notes.push_back({Call::kToward, angle});
  }
  void BeginGroundMovement(ai::FlightMover&, int angle) override {
    notes.push_back({Call::kGround, angle});
  }
};

// A flier at the centre of tile (10, 10), heading for (14, 10).
ai::FlightMover Flier() {
  ai::FlightMover mover;
  mover.tile = Tile(10, 10);
  mover.previous_tile = Tile(0, 0);
  mover.x = Centre(10);
  mover.z = Centre(10);
  mover.flies = true;
  mover.actor_id = 3;
  mover.radius = 32;
  mover.max_range = 64;
  mover.body_height = 96;
  mover.destination = Tile(14, 10);
  mover.stall_counter = 5;
  return mover;
}

struct Move {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
  friend bool operator==(const Move&, const Move&) = default;
};

Move Moved(const ai::FlightMover& mover) {
  return {mover.move_x, mover.move_y, mover.move_z};
}
}  // namespace

TEST_CASE("Flight obstruction finds a platform within reach along a direction",
          "[ai][flight-movement]") {
  FakeFlightWorld world;
  const auto mover = Flier();
  // Probing east one maximum range ahead: (1408, 0, 1344). The reach is the
  // radius plus 0x60, the height half the body height plus 0x20.
  world.platforms = {{.x = 1408 + 128, .height = 64, .z = 1344 - 128}};
  CHECK(ai::FlightDirectionObstruction(world, mover, kEast, true) == 1);
  CHECK(world.cache.obstruction_tested == 0x40);
  CHECK(world.cache.obstruction[kEast] == 1);

  world.notes.clear();
  world.platforms.clear();
  CHECK(ai::FlightDirectionObstruction(world, mover, kEast, true) == 1);
  CHECK(world.notes.empty());

  FakeFlightWorld outside;
  outside.platforms = {{.x = 1408 + 129, .height = 0, .z = 1344},
                       {.x = 1408, .height = 65, .z = 1344}};
  CHECK(ai::FlightDirectionObstruction(outside, mover, kEast, true) == 0);
  CHECK(outside.cache.obstruction_tested == 0x40);
  CHECK(outside.cache.obstruction[kEast] == 0);
}

TEST_CASE("Flight obstruction ignores what lies behind an ahead probe",
          "[ai][flight-movement]") {
  const auto mover = Flier();
  FakeFlightWorld ahead;
  ahead.platforms = {{.x = 1408 - 1, .height = 0, .z = 1344}};
  CHECK(ai::FlightDirectionObstruction(ahead, mover, kEast, true) == 0);

  // Probing in place, nothing is behind.
  FakeFlightWorld here;
  here.platforms = {{.x = 1344 - 1, .height = 0, .z = 1344}};
  CHECK(ai::FlightDirectionObstruction(here, mover, kEast, false) == 1);
}

TEST_CASE("Flight obstruction answers the blocking actor's ID plus one",
          "[ai][flight-movement]") {
  FakeFlightWorld world;
  const auto mover = Flier();
  const ai::FlightPoint probe{.x = 1408, .y = 0, .z = 1344};
  world.actors = {2, 3, 5};
  // Inactive and the mover itself never block.
  world.obstacles[2] = ai::FlightObstacle{.actor_id = 0, .position = probe};
  world.obstacles[3] = ai::FlightObstacle{.actor_id = 3, .position = probe};
  // The reach is both radii plus the obstacle's range; the height, half both
  // body heights.
  world.obstacles[5] = ai::FlightObstacle{
      .actor_id = 5,
      .radius = 16,
      .max_range = 8,
      .body_height = 32,
      .position = ai::FlightPoint{.x = 1408 + 56, .y = 64, .z = 1344 - 56}};
  CHECK(ai::FlightDirectionObstruction(world, mover, kEast, true) == 6);
  CHECK(world.cache.obstruction[kEast] == 6);

  FakeFlightWorld clear = world;
  clear.cache = {};
  Required(Required(clear.obstacles[5]).position).y = 65;
  CHECK(ai::FlightDirectionObstruction(clear, mover, kEast, true) == 0);
}

TEST_CASE("Flight obstruction refuses invalid directions and actors",
          "[ai][flight-movement]") {
  FakeFlightWorld world;
  const auto mover = Flier();
  CHECK_FALSE(ai::FlightDirectionObstruction(world, mover, 8, true));
  CHECK_FALSE(ai::FlightDirectionObstruction(world, mover, -1, true));

  world.actors = {7};
  CHECK_FALSE(ai::FlightDirectionObstruction(world, mover, kEast, true));
  world.obstacles[7] =
      ai::FlightObstacle{.actor_id = 7, .position = std::nullopt};
  CHECK_FALSE(ai::FlightDirectionObstruction(world, mover, kEast, true));
  CHECK(world.cache.obstruction_tested == 0);
}

TEST_CASE("Flight test opens an unflagged tile inside the room",
          "[ai][flight-movement]") {
  FakeFlightWorld world;
  const auto mover = Flier();
  CHECK(ai::CanFlyInDirection(world, mover, kEast) == true);
  CHECK(world.cache.fly_tested == 0x40);
  CHECK(world.cache.fly_open == 0x40);
  CHECK(world.cache.obstruction_tested == 0x40);

  world.notes.clear();
  world.flags[10][11] = kNoFlight;
  CHECK(ai::CanFlyInDirection(world, mover, kEast) == true);
  CHECK(world.notes.empty());
}

TEST_CASE("Flight test closes blocked, outside, flagged and previous tiles",
          "[ai][flight-movement]") {
  auto mover = Flier();

  FakeFlightWorld blocked;
  blocked.platforms = {{.x = 1408, .height = 0, .z = 1344}};
  CHECK(ai::CanFlyInDirection(blocked, mover, kEast) == false);

  FakeFlightWorld flagged;
  flagged.flags[10][11] = kNoFlight;
  CHECK(ai::CanFlyInDirection(flagged, mover, kEast) == false);

  FakeFlightWorld special;
  special.flags[10][11] = kSpecialTerrain;
  CHECK(ai::CanFlyInDirection(special, mover, kEast) == true);

  FakeFlightWorld outside;
  Required(outside.room).bounds.max_x = 10;
  CHECK(ai::CanFlyInDirection(outside, mover, kEast) == false);

  FakeFlightWorld previous;
  mover.previous_tile = Tile(11, 10);
  CHECK(ai::CanFlyInDirection(previous, mover, kEast) == false);

  for (const auto* world : {&blocked, &flagged, &outside, &previous}) {
    CHECK(world->cache.fly_tested == 0x40);
    CHECK(world->cache.fly_open == 0);
  }
}

TEST_CASE("Flight test refuses a tile under a low ceiling without caching it",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.flight_height = 128;
  FakeFlightWorld world;
  Required(world.room).ceiling = 512;
  CHECK(ai::CanFlyInDirection(world, mover, kEast) == false);
  CHECK(world.cache.fly_tested == 0);
  CHECK(world.cache.obstruction_tested == 0x40);

  world.heights[10][11] = kHeight512;
  CHECK(ai::CanFlyInDirection(world, mover, kEast) == true);
  CHECK(world.cache.fly_open == 0x40);
}

TEST_CASE("Flight test refuses a missing room or a tile beyond the maps",
          "[ai][flight-movement]") {
  auto mover = Flier();
  FakeFlightWorld roomless;
  roomless.room.reset();
  CHECK_FALSE(ai::CanFlyInDirection(roomless, mover, kEast));
  CHECK_FALSE(ai::CanFlyInDirection(roomless, mover, 8));

  FakeFlightWorld edge;
  Required(edge.room).bounds.max_x = 40;
  mover.tile = Tile(31, 10);
  CHECK_FALSE(ai::CanFlyInDirection(edge, mover, kEast));
  CHECK(edge.cache.fly_tested == 0);
}

TEST_CASE("Flight transition walks a non-flier on toward a near destination",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.destination = Tile(13, 10);  // 9 tiles squared: near
  FakeFlightWorld world;
  CHECK(ai::UpdateFlightGroundTransition(world, mover, 0x123, true) == false);
  CHECK(world.notes.empty());

  mover.movement_flags = kMayLand;
  CHECK(ai::UpdateFlightGroundTransition(world, mover, 0x123, true) == false);

  mover.flies = false;
  CHECK(ai::UpdateFlightGroundTransition(world, mover, 0x123, true) == true);
  CHECK(world.notes == std::vector<Note>{{Call::kToward, 0x123}});

  mover.flies.reset();
  CHECK_FALSE(ai::UpdateFlightGroundTransition(world, mover, 0x123, true));
}

TEST_CASE("Flight transition lands a flier under a low ceiling",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.destination = Tile(14, 10);  // 16 tiles squared: not near
  mover.movement_flags = kMayLand;
  mover.flight_height = 128;
  FakeFlightWorld world;
  CHECK(ai::UpdateFlightGroundTransition(world, mover, 0x123, true) == false);

  Required(world.room).ceiling = 512;
  world.occupancy = {0, 1};
  CHECK(ai::UpdateFlightGroundTransition(world, mover, 0x123, true) == false);
  CHECK(ai::UpdateFlightGroundTransition(world, mover, 0x123, true) == true);
  CHECK(world.notes == std::vector<Note>{{Call::kOccupancy, Tile(10, 10)},
                                         {Call::kOccupancy, Tile(10, 10)},
                                         {Call::kRelease},
                                         {Call::kBeginTerrain}});

  world.notes.clear();
  world.heights[10][10] = kHeight512;
  CHECK(ai::UpdateFlightGroundTransition(world, mover, 0x123, true) == false);
  CHECK(world.notes.empty());
}

TEST_CASE("Flight transition keeps a walker on the ground under the ceiling",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.movement_flags = kMayLand;
  mover.flies = false;
  FakeFlightWorld world;
  Required(world.room).ceiling = 512;
  CHECK(ai::UpdateFlightGroundTransition(world, mover, 0x123, true) == true);
  CHECK(ai::UpdateFlightGroundTransition(world, mover, 0x456, false) == true);
  CHECK(world.notes ==
        std::vector<Note>{{Call::kToward, 0x123}, {Call::kGround, 0x456}});

  world.room.reset();
  CHECK_FALSE(ai::UpdateFlightGroundTransition(world, mover, 0x123, true));
}

TEST_CASE("Flight movement keeps the R3000 zero-sum glide quotients (#342)",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.y = 124;
  mover.flight_height = 128;
  mover.ground_clearance = 256;
  FakeFlightWorld world;
  world.CloseEveryDirection();
  // Arrival -192 + 128 and floor -192 + 256 sum to zero: the shifted sum
  // divided by itself is -1, whose root is 21, so a step covers 509 of the
  // 512 units left and the glide height is 60 * 253^2 / 509^2 + 64.
  world.heights[10][14] = kDepth192;
  world.steps = {ai::FlightStep{.step = 256, .intent = 6}};
  world.resolutions = {{kMoveHeading, 0}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes == std::vector<Note>{{Call::kStep},
                                         {Call::kResolve, kAlongPlusX, 2},
                                         {Call::kCommand, kMoveVector}});
  CHECK(Moved(mover) == Move{Fixed(1600), Fixed(78), Fixed(1344)});
  CHECK(mover.movement_disabled == 1);
  CHECK(mover.stall_counter == 0);
}

TEST_CASE("Flight movement falls when the glide passes every neighbour",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.y = 1000;
  mover.flight_height = 128;
  mover.ground_clearance = 256;
  FakeFlightWorld world;
  world.CloseEveryDirection();
  world.heights[10][14] = kDepth192;
  world.steps = {ai::FlightStep{.step = 16, .intent = 6}};
  world.resolutions = {{kMoveHeading, 0}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes.back() == Note{Call::kCommand, 6});
  CHECK(Moved(mover) == Move{0, kFall, 0});
  CHECK(mover.stall_counter == 5);
}

TEST_CASE("Flight movement climbs before gliding when the way rises",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.flight_height = 128;
  mover.ground_clearance = 256;
  FakeFlightWorld world;
  world.CloseEveryDirection();
  world.steps = {ai::FlightStep{.step = 16, .intent = 6}};
  world.resolutions = {{kMoveHeading, 0}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes.back() == Note{Call::kCommand, 6});
  CHECK(Moved(mover) == Move{512, 128, 0});
  CHECK(mover.stall_counter == 0);
}

TEST_CASE("Flight movement glides one step down toward its destination",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.y = 1000;
  mover.flight_height = 2048;
  FakeFlightWorld world;
  world.CloseEveryDirection();
  // A step covers 256 of 512 units: 1000 * 240^2 / 256^2 above the floor.
  world.steps = {ai::FlightStep{.step = 16, .intent = 6}};
  world.resolutions = {{kMoveHeading, 0}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes.back() == Note{Call::kCommand, 6});
  CHECK(Moved(mover) == Move{16, 878 - 1000, 0});
  CHECK(mover.stall_counter == 0);
}

TEST_CASE("Flight movement flies directly when a step covers the distance",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.y = 64;
  mover.flight_height = 64;
  FakeFlightWorld world;
  world.CloseEveryDirection();
  world.steps = {ai::FlightStep{.step = 600, .intent = 6},
                 ai::FlightStep{.step = 32, .intent = 4}};
  world.resolutions = {{kMoveHeading, 0}, {kMoveHeading, 0}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes == std::vector<Note>{{Call::kStep},
                                         {Call::kResolve, kAlongPlusX, 2},
                                         {Call::kStep},
                                         {Call::kResolve, kAlongPlusX, 3},
                                         {Call::kCommand, 4}});
  CHECK(Moved(mover) == Move{0x1000, 0, 0});
  CHECK(mover.stall_counter == 0);
}

TEST_CASE("Flight movement follows the resolved heading or sinks",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.flight_height = 128;
  FakeFlightWorld world;
  world.CloseEveryDirection();
  world.steps = {ai::FlightStep{.step = 16, .intent = 6},
                 ai::FlightStep{.step = 16, .intent = 6}};
  world.resolutions = {{kMoveDirection, kEast}, {kMoveDirection, kEast}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes.back() == Note{Call::kCommand, 6});
  CHECK(Moved(mover) == Move{0x1000, 0, 0});
  CHECK(mover.stall_counter == 5);

  mover.y = 200;
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(Moved(mover) == Move{0, kFall, 0});
}

TEST_CASE("Flight movement hovers onto its tile beside a flagged destination",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.close_approach = 1;
  mover.destination = Tile(11, 10);
  mover.approach_target = 7;
  mover.flight_height = 128;
  mover.x = static_cast<std::int16_t>(Centre(10) + 40);
  mover.y = 120;
  FakeFlightWorld world;
  world.flags[10][11] = kNoFlight;
  world.targets[7] = ai::FlightTargetRecord{.y = 0};
  world.steps = {ai::FlightStep{.step = 16, .intent = 6}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes ==
        std::vector<Note>{{Call::kStep}, {Call::kCommand, kMoveVector}});
  // One step west toward the centre, already level with it in z.
  CHECK(Moved(mover) ==
        Move{Fixed(Centre(10) + 40 - 16), Fixed(128), Fixed(Centre(10))});
}

TEST_CASE("Flight movement rises toward the hover height unless blocked",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.close_approach = 1;
  mover.destination = Tile(10, 11);
  mover.approach_target = 7;
  mover.flight_height = 128;
  FakeFlightWorld world;
  world.flags[11][10] = kNoFlight;
  world.targets[7] = ai::FlightTargetRecord{.y = 0};
  world.steps = {ai::FlightStep{.step = 16, .intent = 6}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes.back() == Note{Call::kCommand, 6});
  CHECK(Moved(mover) == Move{0, 128, 0});

  FakeFlightWorld blocked = world;
  blocked.notes.clear();
  blocked.cache = {};
  blocked.platforms = {{.x = Centre(10), .height = 0, .z = Centre(10)}};
  blocked.steps = {ai::FlightStep{.step = 16, .intent = 6}};
  REQUIRE(ai::UpdateFlightMovement(blocked, mover, 0));
  CHECK(blocked.cache.obstruction_tested == 1 << kNorth);
  CHECK(Moved(mover) == Move{0, 0, 0});
}

TEST_CASE("Flight movement hovers at a target below the ground",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.close_approach = 1;
  mover.destination = Tile(10, 9);
  mover.approach_target = 7;
  mover.flight_height = 128;
  mover.y = 1000;
  FakeFlightWorld world;
  world.heights[10][10] = kHeight512;
  world.flags[9][10] = kNoFlight;
  world.targets[7] = ai::FlightTargetRecord{.y = 100};
  world.steps = {ai::FlightStep{.step = 16, .intent = 6},
                 ai::FlightStep{.step = 16, .intent = 6}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(Moved(mover) == Move{0, 100 - 1000, 0});

  Required(world.room).ceiling = 200;
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(Moved(mover) == Move{0, 200 - 1000, 0});
}

struct SettlingScene {
  ai::FlightMover mover = Flier();
  FakeFlightWorld world;

  SettlingScene() {
    mover.close_approach = 1;
    mover.destination = Tile(10, 10);
    mover.approach_target = 7;
    mover.flight_height = 128;
    mover.x = static_cast<std::int16_t>(Centre(10) + 40);
    mover.z = static_cast<std::int16_t>(Centre(10) - 30);
    world.heights[10][10] = kHeight128;
    world.targets[7] = ai::FlightTargetRecord{
        .y = -50, .tile_x = 3, .tile_z = 3, .collision_height = 40};
    for (int i = 0; i < 2; ++i) {
      world.steps.push_back(ai::FlightStep{.step = 16, .intent = 6});
    }
  }
};

TEST_CASE("Flight movement settles over its destination tile",
          "[ai][flight-movement]") {
  SettlingScene scene;
  auto& [mover, world] = scene;
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes.back() == Note{Call::kCommand, 6});
  CHECK(Moved(mover) == Move{-40, 256, 30});

  // Special terrain settles at the flight height alone.
  world.flags[10][10] = kSpecialTerrain;
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(Moved(mover) == Move{-40, 128, 30});
}

TEST_CASE("Flight movement settles onto a target or within a step",
          "[ai][flight-movement]") {
  SettlingScene scene;
  auto& [mover, world] = scene;
  world.flags[10][10] = kSpecialTerrain;
  // Onto a target standing on the destination.
  Required(world.targets[7]).tile_x = 10;
  Required(world.targets[7]).tile_z = 10;
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(Moved(mover) == Move{-40, -50 - 20, 30});

  // Within a step of the tile and the settling height.
  Required(world.targets[7]).tile_x = 3;
  world.steps = {ai::FlightStep{.step = 64, .intent = 6}};
  mover.y = 100;
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(Moved(mover) == Move{0, 0, 0});
}

TEST_CASE("Flight movement recovers when it cannot settle",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.destination = Tile(10, 10);
  FakeFlightWorld world;
  world.steps = {ai::FlightStep{.step = 16, .intent = 6}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes == std::vector<Note>{{Call::kStep}, {Call::kRecover}});

  mover.close_approach = 1;
  FakeFlightWorld blocked;
  blocked.platforms = {{.x = Centre(10), .height = 0, .z = Centre(10)}};
  blocked.steps = {ai::FlightStep{.step = 16, .intent = 6}};
  REQUIRE(ai::UpdateFlightMovement(blocked, mover, 0));
  CHECK(blocked.notes.back() == Note{Call::kRecover});
}

TEST_CASE("Flight movement hands non-fliers and missing steps back",
          "[ai][flight-movement]") {
  auto mover = Flier();
  mover.flies = false;
  FakeFlightWorld world;
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes == std::vector<Note>{{Call::kAlternate}});
  CHECK(mover.movement_disabled == 0);

  mover.flies.reset();
  CHECK_FALSE(ai::UpdateFlightMovement(world, mover, 0));

  mover.flies = true;
  world.notes.clear();
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes == std::vector<Note>{{Call::kStep}, {Call::kRecover}});
  CHECK(mover.movement_disabled == 1);

  world.notes.clear();
  world.steps = {ai::FlightStep{.step = 16, .intent = 6}};
  REQUIRE(ai::UpdateFlightMovement(world, mover, 0));
  CHECK(world.notes ==
        std::vector<Note>{{Call::kStep}, {Call::kResolve, kAlongPlusX, 2}});
}

TEST_CASE("Directional flight follows the resolved heading or the angle",
          "[ai][flight-movement]") {
  auto mover = Flier();
  FakeFlightWorld world;
  world.CloseEveryDirection();
  world.steps = {ai::FlightStep{.step = 16, .intent = 6},
                 ai::FlightStep{.step = 16, .intent = 4}};
  world.resolutions = {{kMoveDirection, kEast}, {kMoveHeading, kEast}};
  REQUIRE(ai::UpdateFlightDirectionalMovement(world, mover, kAlongMinusX));
  CHECK(world.notes == std::vector<Note>{{Call::kStep},
                                         {Call::kResolve, kAlongMinusX, 3},
                                         {Call::kCommand, 6}});
  CHECK(Moved(mover) == Move{0x1000, 0, 0});
  CHECK(mover.movement_disabled == 1);
  CHECK(mover.stall_counter == 0);

  REQUIRE(ai::UpdateFlightDirectionalMovement(world, mover, kAlongMinusX));
  CHECK(Moved(mover) == Move{-0x1000, 0, 0});
}

struct ClosedFlightScene {
  ai::FlightMover mover = Flier();
  FakeFlightWorld world;

  ClosedFlightScene() {
    mover.y = 200;
    world.CloseEveryDirection();
    world.steps = {ai::FlightStep{.step = 16, .intent = 6},
                   ai::FlightStep{.step = 16, .intent = 6},
                   ai::FlightStep{.step = 16, .intent = 6}};
    world.resolutions = {{kMoveHeading, 0}, {kMoveHeading, 0}, {0, 0}};
  }

  bool Update() {
    return ai::UpdateFlightDirectionalMovement(world, mover, kAlongPlusX);
  }

  // The flier falls, then lands as a walker without counting a stall.
  bool FallThenLand() {
    if (!Update()) return false;
    mover.flies = false;
    mover.stall_counter = 5;
    return Update();
  }
};

TEST_CASE("Directional flight falls or lands walkers",
          "[ai][flight-movement]") {
  ClosedFlightScene scene;
  auto& [mover, world] = scene;
  REQUIRE(scene.Update());
  CHECK(Moved(mover) == Move{0, kFall, 0});

  world.notes.clear();
  mover.flies = false;
  mover.stall_counter = 5;
  REQUIRE(scene.Update());
  CHECK(world.notes.back() == Note{Call::kAlternate});
  CHECK(mover.stall_counter == 5);
}

TEST_CASE("Directional flight gives up once it cannot step or resolve",
          "[ai][flight-movement]") {
  ClosedFlightScene scene;
  auto& [mover, world] = scene;
  REQUIRE(scene.FallThenLand());

  world.notes.clear();
  REQUIRE(scene.Update());
  CHECK(world.notes == std::vector<Note>{{Call::kStep},
                                         {Call::kResolve, kAlongPlusX, 3},
                                         {Call::kRelease},
                                         {Call::kRecover}});

  world.notes.clear();
  mover.movement_disabled = 0;
  REQUIRE(scene.Update());
  CHECK(world.notes == std::vector<Note>{{Call::kStep}, {Call::kRecover}});
  CHECK(mover.movement_disabled == 0);
}
