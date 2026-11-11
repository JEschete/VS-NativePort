#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/GroundNavigation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/TerrainHeight.hpp"

namespace {
namespace ai = VS::Battle::Ai;
constexpr int kSide = ai::kNavigationGridSide;
constexpr int kSlots = ai::kNavigationActorSlots;
using Plane = std::array<std::array<std::uint16_t, kSide>, kSide>;

constexpr std::uint32_t Tile(int x, int z) {
  return static_cast<std::uint32_t>(x) | (static_cast<std::uint32_t>(z) << 16U);
}

constexpr std::int16_t Centre(int tile) {
  return static_cast<std::int16_t>((tile * 128) + 64);
}

struct Call {
  std::string_view name;
  int value = 0;
  bool operator==(const Call&) const = default;
};

// A whole room grid with every hook recorded; the actors, models and random
// values are plain fields a test sets.
struct FakeWorld final : ai::GroundNavigationWorld {
  std::uint32_t bounds = 0x2020;
  bool has_maps = true;
  bool empty_terrain = false;
  Plane flags{};
  std::array<VS::Field::TerrainRow, kSide> rows{};
  ai::NavigationDirectionCodes codes{0x30, 0x33, 0x03, 0x13,
                                     0x10, 0x11, 0x01, 0x31};
  bool has_room_data = false;
  std::optional<std::uint8_t> region = 0;
  std::uint8_t walk_valid = 0;
  std::uint8_t walk_passable = 0;
  std::uint8_t blocker_valid = 0;
  std::array<std::uint8_t, ai::kNavigationDirections> blockers{};
  std::vector<int> actor_list;
  std::size_t cursor = 0;
  std::array<std::optional<ai::GroundCollisionActor>, kSlots> actors{};
  std::array<std::optional<std::int16_t>, kSlots> facings{};
  std::array<std::optional<ai::GroundPoint>, kSlots> positions{};
  std::vector<int> randoms;
  std::vector<int> random_ranges;
  std::array<std::uint8_t, 4> hesitation{0, 20, 5, 0};
  bool has_room_ai = true;
  std::uint8_t contact = 0;
  bool has_gates = true;
  int gate = -1;
  ai::RouteGateEnd link_start{.x = 7, .z = 2};
  std::optional<ai::RoomTileBounds> limits =
      ai::RoomTileBounds{.min_x = 0, .min_z = 0, .max_x = 31, .max_z = 31};
  std::array<std::optional<ai::ActorDetourWait>, kSlots> others{};
  std::uint32_t adjacent = 0;
  int fly = 1;
  std::vector<Call> calls;

  [[nodiscard]] std::uint32_t RoomBounds() const override { return bounds; }
  [[nodiscard]] std::optional<ai::RoomTileBounds> RoomTileLimits()
      const override {
    return limits;
  }
  [[nodiscard]] bool HasTileMaps() const override { return has_maps; }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return flags.at(z).at(x);
  }
  [[nodiscard]] VS::Field::TerrainGrid Terrain() const override {
    if (empty_terrain) return {};
    return VS::Field::TerrainGrid{
        std::span<const VS::Field::TerrainRow, kSide>(rows)};
  }
  [[nodiscard]] ai::NavigationDirectionCodes DirectionCodes() const override {
    return codes;
  }
  [[nodiscard]] bool HasRoomData() const override { return has_room_data; }
  [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(
      int /*x*/, int /*z*/) const override {
    return region;
  }
  [[nodiscard]] std::uint8_t WalkValid() const override { return walk_valid; }
  void SetWalkValid(std::uint8_t bits) override { walk_valid = bits; }
  [[nodiscard]] std::uint8_t WalkPassable() const override {
    return walk_passable;
  }
  void SetWalkPassable(std::uint8_t bits) override { walk_passable = bits; }
  [[nodiscard]] std::uint8_t BlockerValid() const override {
    return blocker_valid;
  }
  void SetBlockerValid(std::uint8_t bits) override { blocker_valid = bits; }
  [[nodiscard]] std::uint8_t BlockerActor(int direction) const override {
    return blockers.at(static_cast<std::size_t>(direction));
  }
  void SetBlockerActor(int direction, std::uint8_t actor) override {
    blockers.at(static_cast<std::size_t>(direction)) = actor;
  }
  void RewindCollisionActors() override { cursor = 0; }
  [[nodiscard]] std::optional<int> NextCollisionActor() override {
    if (cursor >= actor_list.size()) return std::nullopt;
    return actor_list[cursor++];
  }
  [[nodiscard]] std::optional<ai::GroundCollisionActor> CollisionActor(
      int actor_id) const override {
    return actors.at(static_cast<std::size_t>(actor_id));
  }
  [[nodiscard]] std::optional<std::int16_t> ModelFacing(
      int actor_id) const override {
    return facings.at(static_cast<std::size_t>(actor_id));
  }
  [[nodiscard]] std::optional<ai::GroundPoint> ModelPosition(
      int actor_id) const override {
    return positions.at(static_cast<std::size_t>(actor_id));
  }
  [[nodiscard]] std::optional<ai::ActorDetourWait> OtherActorDetourWait(
      int actor_id) const override {
    return others.at(static_cast<std::size_t>(actor_id));
  }
  void SetOtherActorRepeatWait(int actor_id, std::uint8_t wait) override {
    auto& other = others.at(static_cast<std::size_t>(actor_id));
    if (!other.has_value()) throw std::logic_error("no AI state to nudge");
    other->repeat_wait = wait;
  }
  int NextRandomMasked(int range) override {
    random_ranges.push_back(range);
    if (randoms.empty()) return 0;
    const int value = randoms.front();
    randoms.erase(randoms.begin());
    return value;
  }
  [[nodiscard]] std::uint8_t HesitationLength(int kind) const override {
    return hesitation.at(static_cast<std::size_t>(kind));
  }
  [[nodiscard]] bool HasRoomAiState() const override { return has_room_ai; }
  [[nodiscard]] std::uint8_t ContactMask() const override { return contact; }
  void SetContactMask(std::uint8_t mask) override { contact = mask; }
  [[nodiscard]] bool HasRouteGates() const override { return has_gates; }
  int FindRouteGateAtTile(ai::GroundNavigator& /*navigator*/,
                          std::uint32_t /*tile*/) override {
    return gate;
  }
  [[nodiscard]] ai::RouteGateEnd RouteLinkStart(int link) const override {
    static_cast<void>(link);
    return link_start;
  }
  void Recover(ai::GroundNavigator& /*navigator*/) override {
    calls.push_back({"recover"});
  }
  void IssueCommand(ai::GroundNavigator& /*navigator*/, int intent) override {
    calls.push_back({"command", intent});
  }
  void UpdateNavigationAreaMovement(ai::GroundNavigator& /*n*/) override {
    calls.push_back({"area"});
  }
  void MoveWithHeightFilteredDirections(ai::GroundNavigator& /*n*/) override {
    calls.push_back({"filtered"});
  }
  void UpdateFlightMovement(ai::GroundNavigator& /*n*/, int angle) override {
    calls.push_back({"flight", angle});
  }
  void UpdateFlightDirectionalMovement(ai::GroundNavigator& /*n*/,
                                       int angle) override {
    calls.push_back({"flight-heading", angle});
  }
  void MoveToRadialDestination(ai::GroundNavigator& /*n*/, int angle) override {
    calls.push_back({"radial", angle});
  }
  void MoveAwayFromReference(ai::GroundNavigator& /*n*/, int angle) override {
    calls.push_back({"away", angle});
  }
  void RefreshMovementCooldown(ai::GroundNavigator& /*n*/) override {
    calls.push_back({"cooldown"});
  }
  std::uint32_t FindAdjacentMovementDestination(ai::GroundNavigator& /*n*/,
                                                int /*first*/, int /*second*/,
                                                std::uint32_t angle) override {
    calls.push_back({"adjacent", static_cast<int>(angle)});
    return adjacent;
  }
  int CanFlyInDirection(ai::GroundNavigator& /*n*/, int direction) override {
    calls.push_back({"fly", direction});
    return fly;
  }
};

ai::GroundNavigator Navigator() {
  ai::GroundNavigator navigator;
  navigator.actor_id = 1;
  navigator.previous_tile = -1;
  navigator.tile = Tile(3, 4);
  navigator.x = Centre(3);
  navigator.z = Centre(4);
  navigator.tile_step_limit = 3;
  navigator.step_height = 0x140;
  return navigator;
}

ai::GroundCollisionActor ModelAt(int x, int z) {
  ai::GroundCollisionActor actor;
  actor.model_x = static_cast<std::int16_t>(x);
  actor.model_z = static_cast<std::int16_t>(z);
  return actor;
}
}  // namespace

TEST_CASE("AI ground navigation refuses rooms without bounded maps",
          "[ai][ground-navigation]") {
  const auto navigator = Navigator();
  for (const std::uint32_t bounds :
       {0x0000U, 0x2000U, 0x0020U, 0x2120U, 0x2021U, 0xFFFFU}) {
    FakeWorld world;
    world.bounds = bounds;
    CHECK_FALSE(ai::DestinationOccupancy(world, navigator, Tile(1, 1)));
  }
  FakeWorld world;
  world.bounds = 0x0101;
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(0, 0)) == 1);
  world.has_maps = false;
  CHECK_FALSE(ai::DestinationOccupancy(world, navigator, Tile(0, 0)));
  world.has_maps = true;
  world.empty_terrain = true;
  CHECK_FALSE(ai::DestinationOccupancy(world, navigator, Tile(0, 0)));
}

TEST_CASE("AI destination occupancy rejects blocked and occupied tiles",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.bounds = 0x0808;
  auto navigator = Navigator();
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(5, 5)) == 1);
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(8, 0)) == 0);
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(0, 8)) == 0);
  for (const std::uint16_t blocked : {0x10, 0x20, 0x80, 0x8000}) {
    world.flags[5][5] = blocked;
    CHECK(ai::DestinationOccupancy(world, navigator, Tile(5, 5)) == 0);
  }
  world.flags[5][5] = 0x0100;
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(5, 5)) == 1);
  navigator.terrain_reject_mask = 1;
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(5, 5)) == 0);

  // The previous tile matches its low 24 bits against the x and z bytes.
  navigator.previous_tile = static_cast<std::int32_t>(Tile(2, 2));
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(2, 2) | 0x7F000000U) ==
        0);
  navigator.previous_tile |= 0x100;
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(2, 2)) == 1);
}

TEST_CASE("AI destination occupancy tests other actors' bounds",
          "[ai][ground-navigation]") {
  FakeWorld world;
  const auto navigator = Navigator();
  world.actor_list = {navigator.actor_id, 4};
  world.actors[4] = ModelAt(Centre(5), Centre(5));
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(5, 5)) == -1);
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(6, 5)) == 1);
  auto model = ModelAt(Centre(5), Centre(5));
  model.model_y = 129;
  world.actors[4] = model;
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(5, 5)) == 1);
  model.model_y = 128;
  world.actors[4] = model;
  CHECK(ai::DestinationOccupancy(world, navigator, Tile(5, 5)) == -1);
  // The navigator's own entry has no model and is skipped.
  CHECK_FALSE(world.actors[navigator.actor_id].has_value());
}

TEST_CASE("AI collision actor lists refuse invalid entries",
          "[ai][ground-navigation]") {
  const auto navigator = Navigator();
  FakeWorld world;
  world.actors[4] = ModelAt(0, 0);
  for (const auto& list : {std::vector<int>{16}, std::vector<int>{-1},
                           std::vector<int>{4, 4}, std::vector<int>{5}}) {
    world.actor_list = list;
    CHECK_FALSE(ai::DestinationOccupancy(world, navigator, Tile(5, 5)));
  }
}

TEST_CASE("AI directional blockers are probed once per frame",
          "[ai][ground-navigation]") {
  FakeWorld world;
  const auto navigator = Navigator();
  world.actor_list = {4};
  auto model = ModelAt(Centre(3) + 10, Centre(4));
  model.model_radius = 16;
  world.actors[4] = model;
  CHECK(ai::DirectionalBlockingActor(world, navigator, 6) == 5);
  CHECK(ai::DirectionalBlockingActor(world, navigator, 2) == 0);
  CHECK(world.blocker_valid == ((1U << 6U) | (1U << 2U)));
  world.actors[4].reset();
  CHECK(ai::DirectionalBlockingActor(world, navigator, 6) == 5);
  CHECK_FALSE(ai::DirectionalBlockingActor(world, navigator, 8));
  CHECK_FALSE(ai::DirectionalBlockingActor(world, navigator, -1));
  world.codes[0] = 0x22;
  CHECK_FALSE(ai::DirectionalBlockingActor(world, navigator, 0));
}

TEST_CASE("AI tile edges test flags, areas, steps and distance",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  const auto from = Tile(3, 4);
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6) == 1);
  world.flags[4][4] = 0x10;
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6) == 0);
  world.flags[4][4] = 0x01;
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6) == 0);
  navigator.is_stuck = 1;
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6) == 1);
}

TEST_CASE("AI tile edges limit distance, step height and movement",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  const auto from = Tile(3, 4);
  world.flags[4][4] = 0x01;
  navigator.is_stuck = 1;
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(7, 4), 6) == 0);
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(6, 4), 6) == 1);
  world.rows[4][6] = 0x3FB;  // 320 below the source: the step limit holds.
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(6, 4), 6) == 1);
  world.rows[4][6] = 0x3FA;
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(6, 4), 6) == 0);
  navigator.movement_flags = 0x08;
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(6, 4), 6) == 1);
  navigator.movement_disabled = 1;
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6) == 0);
}

TEST_CASE("AI tile edges keep steps that approach the destination",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.target_distance_filter = 1;
  navigator.destination = Tile(9, 4);
  const auto from = Tile(3, 4);
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6) == 1);
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(2, 4), 2) == 0);
  navigator.requested_movement_mode = 1;
  navigator.approach_target_id = 6;
  CHECK_FALSE(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6));
  world.positions[6] = ai::GroundPoint{Centre(5), Centre(4)};
  navigator.approach_range = 96 + 129;
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6) == -1);
  navigator.approach_range = 96 + 128;
  CHECK(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6) == 1);
  navigator.approach_target_id = 16;
  CHECK_FALSE(ai::CanTraverseTileEdge(world, navigator, from, Tile(4, 4), 6));
}

TEST_CASE("AI tile edges refuse invalid directions and rooms",
          "[ai][ground-navigation]") {
  FakeWorld world;
  const auto navigator = Navigator();
  CHECK_FALSE(ai::CanTraverseTileEdge(world, navigator, 0, Tile(1, 0), 8));
  CHECK_FALSE(ai::CanTraverseTileEdge(world, navigator, 0, Tile(1, 0), -1));
  CHECK(ai::CanTraverseTileEdge(world, navigator, 0, Tile(32, 0), 6) == 0);
  CHECK(ai::CanTraverseTileEdge(world, navigator, Tile(0, 40), 0, 0) == 0);
  world.bounds = 0x2100;
  CHECK_FALSE(ai::CanTraverseTileEdge(world, navigator, 0, Tile(1, 0), 6));
}

TEST_CASE("AI walk probes cache each direction for the frame",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.fine_x = 7;  // The east half of tile 3.
  navigator.fine_z = 8;
  CHECK(ai::CanWalkInDirection(world, navigator, 0) == true);
  CHECK(ai::CanWalkInDirection(world, navigator, 6) == true);
  world.flags[4][4] = 0x80;
  CHECK(ai::CanWalkInDirection(world, navigator, 6) == true);
  world.walk_valid = 0;
  CHECK(ai::CanWalkInDirection(world, navigator, 6) == false);
  CHECK(world.walk_valid == (1U << 6U));
  CHECK((world.walk_passable & (1U << 6U)) == 0);
  CHECK((world.walk_passable & 1U) != 0);
  world.flags[4][4] = 0;
  world.rows[4][4] = 0x0400;  // A higher terrain layer blocks the step.
  world.walk_valid = 0;
  CHECK(ai::CanWalkInDirection(world, navigator, 6) == false);
}

TEST_CASE("AI walk probes refuse invalid directions and tiles",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  CHECK_FALSE(ai::CanWalkInDirection(world, navigator, 8));
  CHECK_FALSE(ai::CanWalkInDirection(world, navigator, -1));
  navigator.tile = Tile(40, 0);
  navigator.fine_x = 9;
  navigator.fine_z = 9;
  CHECK_FALSE(ai::CanWalkInDirection(world, navigator, 6));
  world.has_room_data = true;
  world.region.reset();
  navigator = Navigator();
  navigator.fine_x = 7;
  navigator.fine_z = 8;
  CHECK_FALSE(ai::CanWalkInDirection(world, navigator, 6));
}

TEST_CASE("AI gap scans land past the gap or stop at the room edge",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.bounds = 0x0808;
  auto navigator = Navigator();
  const auto from = Tile(5, 5);
  CHECK(ai::FindGapLandingTile(world, navigator, Tile(5, 5) | 0x7F010000U, 6,
                               from) == (Tile(5, 5) | 0x010000U));
  CHECK(ai::FindGapLandingTile(world, navigator, Tile(8, 5), 6, from) == 0);
  world.flags[5][5] = 0x20;
  CHECK(ai::FindGapLandingTile(world, navigator, Tile(5, 5), 6, from) ==
        (Tile(6, 5) | (1U << 24U)));
  navigator.tile_step_limit = 1;
  CHECK(ai::FindGapLandingTile(world, navigator, Tile(5, 5), 6, from) ==
        Tile(6, 5));
  world.flags[5][6] = 0x20;
  world.flags[5][7] = 0x20;
  CHECK(ai::FindGapLandingTile(world, navigator, Tile(5, 5), 6, from) == 0);
  navigator.tile_step_limit = 0;
  CHECK(ai::FindGapLandingTile(world, navigator, Tile(5, 5), 6, from) ==
        Tile(6, 5));
  CHECK_FALSE(ai::FindGapLandingTile(world, navigator, Tile(5, 5), 8, from));
}

TEST_CASE("AI detour boundaries and progress follow the original",
          "[ai][ground-navigation]") {
  auto navigator = Navigator();
  navigator.tile = Tile(2, 3);
  navigator.destination = Tile(5, 1);
  navigator.navigation_flags = 0xFFFFFFFF;
  ai::SetDetourBoundary(navigator);
  CHECK(navigator.navigation_flags == (0x3FFFFFU | (6U << 22U) | (1U << 27U)));
  navigator.destination = navigator.tile;
  ai::SetDetourBoundary(navigator);
  CHECK(navigator.navigation_flags == 0x3FFFFFU);
}

namespace {
ai::GroundNavigator DetourNavigator() {
  auto navigator = Navigator();
  navigator.tile = Tile(2, 3);
  navigator.destination = navigator.tile;
  navigator.navigation_flags = 0x3FFFFFU;
  navigator.movement_enabled = 1;
  navigator.movement_wait = 9;
  navigator.detour_tile = navigator.tile | 0x0100;
  navigator.detour_repeats = 3;
  return navigator;
}
}  // namespace

TEST_CASE("AI detour progress counts repeats on the same tile",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = DetourNavigator();
  CHECK(ai::RefreshDetourProgress(world, navigator, 0xFFFFFFFF) ==
        (0xFFFFF01FU | (126U << 5U)));
  CHECK(navigator.detour_repeats == 4);
  CHECK(navigator.movement_wait == 0);
  static_cast<void>(ai::RefreshDetourProgress(world, navigator, 0));
  CHECK(navigator.detour_repeats == 0);
  CHECK(navigator.repeat_wait == 120);
}

TEST_CASE("AI detour progress picks a fresh boundary when it moves",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = DetourNavigator();
  static_cast<void>(ai::RefreshDetourProgress(world, navigator, 0xFFFFFFFF));
  static_cast<void>(ai::RefreshDetourProgress(world, navigator, 0));
  navigator.detour_tile = Tile(0, 0);
  navigator.movement_enabled = 0;
  CHECK(ai::RefreshDetourProgress(world, navigator, 0) == (31U << 5U));
  CHECK(navigator.detour_repeats == 0);
  world.randoms = {1};
  CHECK(ai::RefreshDetourProgress(world, navigator, 0) == (126U << 5U));
  CHECK(world.random_ranges == std::vector<int>{4, 4});
}

namespace {
struct FakeProbe final : ai::GroundDirectionProbe {
  std::array<std::optional<int>, ai::kNavigationDirections> results{};
  std::vector<int> asked;

  [[nodiscard]] std::optional<int> Probe(ai::GroundNavigator& /*navigator*/,
                                         int direction) override {
    asked.push_back(direction);
    return results.at(static_cast<std::size_t>(direction));
  }
};
}  // namespace

TEST_CASE("AI cardinal masks put direction 0 in bit 3",
          "[ai][ground-navigation]") {
  auto navigator = Navigator();
  FakeProbe probe;
  probe.results = {1, 0, 0, 0, 1, 0, 0, 0};
  CHECK(ai::CardinalPassabilityMask(navigator, probe) == 0b1010);
  CHECK(probe.asked == std::vector<int>{0, 2, 4, 6});
  probe.results[6].reset();
  CHECK_FALSE(ai::CardinalPassabilityMask(navigator, probe));
}

TEST_CASE("AI obstacle detours restart with a fresh cardinal mask",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.gait = 1;
  FakeProbe probe;
  probe.results = {0, 0, 1, 0, 0, 0, 1, 0};
  world.randoms = {1, 9};  // The turn bit is 1; hesitation does not start.
  world.hesitation = {};
  const auto flags = ai::InitializeObstacleDetour(world, navigator, 0, probe);
  CHECK(navigator.detour_run == 1);
  REQUIRE(flags.has_value());
  CHECK(*flags == (0x10U | (0b0101U << 18U)));
}

namespace {
constexpr std::uint32_t kDetourStop = 1U << 16U;
constexpr std::uint32_t kDetourChanged = 1U << 17U;
constexpr int kWalkBlockedBit = 11;
constexpr int kFlightBlockedBit = 12;

struct DetourFields {
  int heading = 0;
  bool clockwise = false;
  int steps = 1;
  int visited = 0;
  bool changed = false;
  int cell = 0;
  int bound_x = 0;
  int bound_z = 0;
};

// An active detour word (Battle_Ai_DetourWord) built field by field.
constexpr std::uint32_t DetourWord(const DetourFields& fields) {
  return 1U | (static_cast<std::uint32_t>(fields.heading) << 1U) |
         (fields.clockwise ? 0x10U : 0U) |
         (static_cast<std::uint32_t>(fields.steps) << 5U) |
         (static_cast<std::uint32_t>(fields.visited) << 12U) |
         (fields.changed ? kDetourChanged : 0U) |
         (static_cast<std::uint32_t>(fields.cell) << 18U) |
         (static_cast<std::uint32_t>(fields.bound_x) << 22U) |
         (static_cast<std::uint32_t>(fields.bound_z) << 27U);
}

FakeProbe OpenProbe() {
  FakeProbe probe;
  probe.results.fill(1);
  return probe;
}

std::optional<int> Follow(FakeWorld& world, ai::GroundNavigator& navigator,
                          FakeProbe& probe) {
  return ai::FollowObstacleDetour(world, navigator, probe, kWalkBlockedBit);
}

std::uint8_t RepeatWait(const FakeWorld& world, int actor) {
  const auto& other = world.others.at(static_cast<std::size_t>(actor));
  if (!other.has_value()) throw std::logic_error("the actor has no AI state");
  return other->repeat_wait;
}
}  // namespace

TEST_CASE("AI obstacle detours end a spent run without probing",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.detour_run = 3;
  navigator.target_distance_filter = 1;
  const auto word = DetourWord({.heading = 2, .steps = 0});
  navigator.navigation_flags = word;
  auto probe = OpenProbe();
  CHECK(Follow(world, navigator, probe) == -1);
  CHECK(navigator.navigation_flags == word);
  CHECK(navigator.gait == 3);
  CHECK(navigator.target_distance_filter == 0);
  CHECK(probe.asked.empty());
  CHECK(world.blocker_valid == 0);
}

TEST_CASE("AI obstacle detours nudge the actor on the turn side",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  // Heading 0 turns counter-clockwise, so the turn side is direction 2. The
  // cardinal cell already matches, so the detour keeps its heading.
  const auto word = DetourWord({.heading = 0, .cell = 15});
  navigator.navigation_flags = word;
  world.blocker_valid = 0xFF;
  world.blockers[2] = 5;  // Actor 4.
  world.others[4] = ai::ActorDetourWait{};
  world.randoms = {3};
  auto probe = OpenProbe();
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(navigator.navigation_flags == word);
  CHECK(RepeatWait(world, 4) == 7);
  // A committed or waiting blocker keeps its wait and draws no random.
  world.others[4] = ai::ActorDetourWait{.movement_flags = 0x20};
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(RepeatWait(world, 4) == 0);
  world.others[4] = ai::ActorDetourWait{.repeat_wait = 2};
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(RepeatWait(world, 4) == 2);
  CHECK(world.random_ranges == std::vector<int>{4});
}

TEST_CASE("AI obstacle detours nudge themselves and skip blockers without AI",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  // A clockwise turn from heading 0 puts the turn side at direction 6.
  navigator.navigation_flags =
      DetourWord({.heading = 0, .clockwise = true, .cell = 15});
  world.blocker_valid = 0xFF;
  world.blockers[6] = 2;  // The navigator's own actor 1.
  world.randoms = {1};
  auto probe = OpenProbe();
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(navigator.repeat_wait == 5);
  world.blockers[6] = 6;  // Actor 5 has no AI state: warned and skipped.
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(world.random_ranges == std::vector<int>{4});
  world.blockers[6] = 17;  // Outside the actor table.
  CHECK_FALSE(Follow(world, navigator, probe));
}

TEST_CASE("AI obstacle detours reverse when the tile ahead closes",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.blocker_valid = 0xFF;
  auto navigator = Navigator();
  auto probe = OpenProbe();
  probe.results[0] = 0;  // The cardinal cell is 0b0111.
  const auto word = DetourWord({.heading = 0, .visited = 3, .cell = 7});
  navigator.navigation_flags = word;
  // The closed probe alone does not reverse while the tile ahead is free.
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(navigator.navigation_flags == word);
  world.flags[3][3] = 0x10;
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(navigator.navigation_flags ==
        DetourWord(
            {.heading = 4, .clockwise = true, .changed = true, .cell = 7}));
  // Once changed, the heading is not tested again.
  probe.asked.clear();
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(probe.asked == std::vector<int>{0, 2, 4, 6});
}

TEST_CASE("AI obstacle detours reverse for an actor ahead unless near a target",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.blocker_valid = 0xFF;
  auto navigator = Navigator();
  auto probe = OpenProbe();
  probe.results[0] = 0;
  const auto word = DetourWord({.heading = 0, .visited = 3, .cell = 7});
  // Another actor on the tile ahead also reverses the turn.
  world.actor_list = {4};
  world.actors[4] = ModelAt(Centre(3), Centre(3));
  navigator.navigation_flags = word;
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(navigator.navigation_flags ==
        DetourWord(
            {.heading = 4, .clockwise = true, .changed = true, .cell = 7}));
  // A near-target detour never reverses.
  navigator.navigation_flags = word;
  navigator.near_target_detour = 1;
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(navigator.navigation_flags == word);
}

TEST_CASE("AI obstacle detours stop when the turn side leaves the room",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.pursuit_e4 = 1;
  navigator.pursuit_e7 = 2;
  navigator.pursuit_e8 = 3;
  auto probe = OpenProbe();
  const auto word = DetourWord({.heading = 0, .cell = 15});
  navigator.navigation_flags = word;
  // The flight bit on the turn side (2,4) does not stop a walking detour.
  world.flags[4][2] = 1U << kFlightBlockedBit;
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(navigator.pursuit_e4 == 1);
  CHECK(ai::FollowObstacleDetour(world, navigator, probe, kFlightBlockedBit) ==
        -1);
  CHECK(navigator.navigation_flags == (word | kDetourStop));
  CHECK(navigator.pursuit_e4 == 0);
  CHECK(navigator.pursuit_e7 == 0);
  CHECK(navigator.pursuit_e8 == 0);
  // A near-target detour ignores the room.
  navigator.navigation_flags = word;
  navigator.near_target_detour = 1;
  CHECK(ai::FollowObstacleDetour(world, navigator, probe, kFlightBlockedBit) ==
        5);
}

TEST_CASE("AI obstacle detours test each room limit on the turn side",
          "[ai][ground-navigation]") {
  struct Case {
    DetourFields fields;
    ai::RoomTileBounds limits;
  };
  // From tile (3,4) the turn sides are (2,4), (4,4), (3,5) and (3,3).
  for (const auto& [fields, limits] : {
           Case{{.heading = 0, .cell = 15}, {3, 0, 31, 31}},
           Case{{.heading = 0, .clockwise = true, .cell = 15}, {0, 0, 3, 31}},
           Case{{.heading = 2, .cell = 15}, {0, 0, 31, 4}},
           Case{{.heading = 2, .clockwise = true, .cell = 15}, {0, 4, 31, 31}},
       }) {
    FakeWorld world;
    auto navigator = Navigator();
    navigator.navigation_flags = DetourWord(fields);
    auto probe = OpenProbe();
    CHECK(Follow(world, navigator, probe) == 5);
    world.limits = limits;
    CHECK(Follow(world, navigator, probe) == -1);
    CHECK(navigator.navigation_flags == (DetourWord(fields) | kDetourStop));
  }
}

TEST_CASE("AI obstacle detours end past the boundary along z",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.destination = Tile(3, 9);  // Centre z 1216.
  auto probe = OpenProbe();
  // The boundary tile 6 (z 768) lies between the actor (576) and the target.
  const auto word = DetourWord({.heading = 0, .cell = 15, .bound_z = 6});
  navigator.navigation_flags = word;
  CHECK(Follow(world, navigator, probe) == 5);
  navigator.z = 800;
  CHECK(Follow(world, navigator, probe) == -1);
  CHECK(navigator.navigation_flags == word);
  // A near-target detour ends on the near side instead.
  navigator.near_target_detour = 1;
  CHECK(Follow(world, navigator, probe) == 5);
  navigator.z = Centre(4);
  CHECK(Follow(world, navigator, probe) == -1);
}

TEST_CASE("AI obstacle detours end past the boundary along x",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.destination = Tile(1, 4);  // Centre x 192.
  auto probe = OpenProbe();
  // The boundary tile 2 (x 256) lies between the target and the actor (448).
  const auto word = DetourWord({.heading = 0, .cell = 15, .bound_x = 2});
  navigator.navigation_flags = word;
  CHECK(Follow(world, navigator, probe) == 5);
  navigator.x = 200;
  CHECK(Follow(world, navigator, probe) == -1);
  navigator.near_target_detour = 1;
  CHECK(Follow(world, navigator, probe) == 5);
  navigator.x = Centre(3);
  CHECK(Follow(world, navigator, probe) == -1);
  CHECK(navigator.navigation_flags == word);
}

TEST_CASE("AI obstacle detours stop once the inner side opens",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  world.randoms = {2};
  auto probe = OpenProbe();
  // A new cardinal cell turns the detour; the inner side behind it is open.
  navigator.navigation_flags = DetourWord({.heading = 0});
  CHECK(Follow(world, navigator, probe) == -1);
  CHECK(navigator.navigation_flags ==
        (DetourWord({.heading = 0, .cell = 15}) | kDetourStop));
  CHECK(navigator.repeat_wait == 6);
  CHECK(probe.asked == std::vector<int>{0, 0, 2, 4, 6, 3, 2});
  // A waiting actor is not nudged again; a clockwise turn tests 5 and 6.
  probe.asked.clear();
  navigator.navigation_flags = DetourWord({.heading = 0, .clockwise = true});
  CHECK(Follow(world, navigator, probe) == -1);
  CHECK(probe.asked == std::vector<int>{0, 0, 2, 4, 6, 5, 6});
  CHECK(world.random_ranges == std::vector<int>{4});
}

TEST_CASE("AI obstacle detours stop after trying every cardinal",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  auto probe = OpenProbe();
  probe.results[1] = 0;  // Heading 6 turning counter-clockwise looks at 1.
  navigator.navigation_flags =
      DetourWord({.heading = 6, .visited = 7, .changed = true});
  CHECK(Follow(world, navigator, probe) == -1);
  CHECK(
      navigator.navigation_flags ==
      (DetourWord({.heading = 6, .visited = 15, .changed = true, .cell = 15}) |
       kDetourStop));
}

TEST_CASE("AI obstacle detours scan for an open heading or a free step",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  auto probe = OpenProbe();
  probe.results[3] = 0;  // The inner side behind stays walled.
  navigator.navigation_flags = DetourWord({.heading = 0});
  CHECK(Follow(world, navigator, probe) == 5);
  CHECK(navigator.navigation_flags ==
        DetourWord({.heading = 2, .visited = 1, .changed = true, .cell = 15}));
  // With every heading closed the free inner tile becomes the next step; its
  // tag and top byte are zero (#344).
  probe.results.fill(0);
  navigator.next_tile = ~0U;
  navigator.navigation_flags =
      DetourWord({.heading = 0, .changed = true, .cell = 15});
  CHECK(Follow(world, navigator, probe) == 14);
  CHECK(navigator.next_tile == Tile(2, 4));
  CHECK(navigator.navigation_flags ==
        DetourWord({.heading = 2, .visited = 1, .changed = true}));
}

TEST_CASE("AI obstacle detours end when the scan finds no way",
          "[ai][ground-navigation]") {
  FakeWorld world;
  for (const auto& [x, z] :
       std::array<std::array<int, 2>, 4>{{{2, 4}, {3, 3}, {4, 4}, {3, 5}}}) {
    world.flags.at(z).at(x) = 0x10;
  }
  auto navigator = Navigator();
  FakeProbe probe;
  probe.results.fill(0);
  navigator.navigation_flags =
      DetourWord({.heading = 0, .changed = true, .cell = 15});
  CHECK(Follow(world, navigator, probe) == -1);
  CHECK(navigator.navigation_flags ==
        DetourWord({.heading = 0, .visited = 1, .changed = true}));
  CHECK(probe.asked == std::vector<int>{0, 2, 4, 6, 3, 2, 0, 6, 4});
  // A clockwise scan runs the other way round.
  probe.asked.clear();
  navigator.navigation_flags = DetourWord(
      {.heading = 0, .clockwise = true, .changed = true, .cell = 15});
  CHECK(Follow(world, navigator, probe) == -1);
  CHECK(probe.asked == std::vector<int>{0, 2, 4, 6, 5, 6, 0, 2, 4});
}

TEST_CASE("AI obstacle detours refuse missing rooms and unmapped tiles",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.blocker_valid = 0xFF;
  auto navigator = Navigator();
  auto probe = OpenProbe();
  const auto word = DetourWord({.heading = 0, .changed = true, .cell = 15});
  navigator.navigation_flags = word;
  world.limits.reset();
  CHECK_FALSE(Follow(world, navigator, probe));
  CHECK(navigator.navigation_flags == word);
  world.limits = ai::RoomTileBounds{.max_x = 40, .max_z = 40};
  world.has_maps = false;
  CHECK_FALSE(Follow(world, navigator, probe));
  world.has_maps = true;
  // The clockwise turn side (32,4) is inside the limits but off the map.
  navigator.tile = Tile(31, 4);
  navigator.navigation_flags =
      DetourWord({.heading = 0, .clockwise = true, .changed = true});
  CHECK_FALSE(Follow(world, navigator, probe));
  navigator.tile = Tile(3, 4);
  navigator.navigation_flags = DetourWord({.heading = 0, .cell = 15});
  probe.results[0].reset();
  CHECK_FALSE(Follow(world, navigator, probe));
  CHECK(navigator.navigation_flags == DetourWord({.heading = 0, .cell = 15}));
}

TEST_CASE("AI hesitation turns start, count down and poll",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  CHECK(ai::UpdateHesitationTurn(world, navigator, 8, 1, 0) == 1);
  CHECK(navigator.mode_counters[0] == 1);
  CHECK(navigator.mode_counters[1] == 19);
  CHECK(navigator.heading == -0x320);
  CHECK(navigator.turn_timer == 64);
  CHECK(world.calls == std::vector<Call>{{"command", 16}});
  CHECK(ai::UpdateHesitationTurn(world, navigator, 8, 3, 0) == 0);
  CHECK(navigator.mode_counters[1] == 18);
}

namespace {
// Replays the first hesitation turn and its countdown.
void StartHesitation(FakeWorld& world, ai::GroundNavigator& navigator) {
  static_cast<void>(ai::UpdateHesitationTurn(world, navigator, 8, 1, 0));
  static_cast<void>(ai::UpdateHesitationTurn(world, navigator, 8, 3, 0));
}
}  // namespace

TEST_CASE("AI hesitation turns react once the reaction is pending",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  StartHesitation(world, navigator);
  CHECK(ai::UpdateHesitationTurn(world, navigator, 8, 2, 6) == 1);
  CHECK(navigator.mode_counters[12] == 2);
  CHECK(navigator.reaction_pending == 4);
  CHECK(navigator.heading == 0);
  navigator.reaction_pending = 1;
  CHECK(ai::UpdateHesitationTurn(world, navigator, 8, 2, 6) == 0);
  CHECK(navigator.reaction_pending == 0);
}

TEST_CASE("AI hesitation turns poll without a fresh random start",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  StartHesitation(world, navigator);
  static_cast<void>(ai::UpdateHesitationTurn(world, navigator, 8, 2, 6));
  navigator.reaction_pending = 1;
  static_cast<void>(ai::UpdateHesitationTurn(world, navigator, 8, 2, 6));
  CHECK(ai::UpdateHesitationTurn(world, navigator, 8, 3, 2) == 0);
  world.randoms = {9};
  CHECK(ai::UpdateHesitationTurn(world, navigator, 8, 1, 2) == 0);
  CHECK(navigator.mode_counters[5] == 0);
}

TEST_CASE("AI hesitation turns refuse invalid slots, kinds and tables",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  const auto before = navigator.mode_counters;
  CHECK_FALSE(ai::UpdateHesitationTurn(world, navigator, 8, 1, 7));
  CHECK_FALSE(ai::UpdateHesitationTurn(world, navigator, 8, 1, -1));
  CHECK_FALSE(ai::UpdateHesitationTurn(world, navigator, 8, 0, 0));
  CHECK_FALSE(ai::UpdateHesitationTurn(world, navigator, 8, 4, 0));
  world.hesitation = {};
  CHECK_FALSE(ai::UpdateHesitationTurn(world, navigator, 8, 1, 0));
  CHECK(navigator.mode_counters == before);
  CHECK(world.calls.empty());
}

namespace {
using Step = ai::VerticalMovementStep;

// The selected step, or a sentinel when the selection was refused.
Step StepFor(const ai::GroundNavigator& navigator) {
  return ai::SelectActorVerticalMovementStep(navigator).value_or(
      Step{.selected = true, .value = -1, .code = -1});
}

bool Same(const Step& left, const Step& right) {
  return left.selected == right.selected && left.value == right.value &&
         left.code == right.code;
}
}  // namespace

TEST_CASE("AI vertical steps prefer the current gait",
          "[ai][ground-navigation]") {
  ai::GroundNavigator navigator;
  navigator.walk_step = 3;
  navigator.run_step = 7;
  CHECK_FALSE(StepFor(navigator).selected);
  navigator.locomotion_flags = 0x2010;
  CHECK(Same(StepFor(navigator), {.selected = true, .value = 3, .code = 9}));
  navigator.gait = 1;
  CHECK(Same(StepFor(navigator), {.selected = true, .value = 7, .code = 10}));
  navigator.locomotion_flags = 0x1000;
  CHECK(Same(StepFor(navigator), {.selected = true, .value = 7, .code = 9}));
  navigator.locomotion_flags = 0;
  CHECK_FALSE(ai::SelectActorVerticalMovementStep(navigator));
}

TEST_CASE("AI facing tests use the actor or its movement target",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.movement_target = 16;
  world.facings[1] = 0;
  const bool south =
      ai::IsTileBehindActorTarget(world, navigator, Tile(3, 5)).value_or(false);
  const bool north =
      ai::IsTileBehindActorTarget(world, navigator, Tile(3, 3)).value_or(false);
  CHECK(south != north);
  navigator.facing_locked = 1;
  CHECK(ai::IsTileBehindActorTarget(world, navigator, Tile(3, 5)) == false);
  navigator.facing_locked = 0;
  navigator.movement_target = 2;
  CHECK_FALSE(ai::IsTileBehindActorTarget(world, navigator, Tile(3, 5)));
  world.positions[2] = ai::GroundPoint{Centre(3), Centre(9)};
  CHECK(ai::IsTileBehindActorTarget(world, navigator, Tile(3, 5)) == false);
  CHECK(ai::IsTileBehindActorTarget(world, navigator, Tile(3, 3)) == true);
  navigator.actor_id = 16;
  CHECK_FALSE(ai::IsTileBehindActorTarget(world, navigator, Tile(3, 5)));
}

TEST_CASE("AI descents find the first lower tile within range",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.has_room_data = true;
  auto navigator = Navigator();
  navigator.tile = Tile(5, 5);
  world.rows[5][6] = 2;
  world.rows[5][7] = 2;
  world.rows[5][8] = 1;
  CHECK(ai::FindAiDescentTile(world, navigator, Tile(6, 5), 6, 4) ==
        Tile(8, 5));
  CHECK(ai::FindAiDescentTile(world, navigator, Tile(6, 5), 6, 3) ==
        Tile(8, 5));
  CHECK(ai::FindAiDescentTile(world, navigator, Tile(6, 5), 6, 2) == 0);
  CHECK(ai::FindAiDescentTile(world, navigator, Tile(6, 5), 6, 0) == 0);
  world.flags[5][8] = 0x80;
  CHECK(ai::FindAiDescentTile(world, navigator, Tile(6, 5), 6, 4) == 0);
  world.has_room_data = false;
  CHECK(ai::FindAiDescentTile(world, navigator, Tile(6, 5), 6, 4) == 0);
}

TEST_CASE("AI descents refuse invalid directions and origins",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.has_room_data = true;
  auto navigator = Navigator();
  world.rows[5][6] = 2;
  CHECK_FALSE(ai::FindAiDescentTile(world, navigator, Tile(6, 5), 8, 4));
  CHECK(ai::FindAiDescentTile(world, navigator, Tile(32, 5), 6, 4) == 0);
  navigator.tile = Tile(40, 5);
  CHECK_FALSE(ai::FindAiDescentTile(world, navigator, Tile(6, 5), 6, 4));
}

TEST_CASE("AI contact occupancy bits wrap at 32 and need room state",
          "[ai][ground-navigation]") {
  FakeWorld world;
  ai::SetAiContactOccupancy(world, 3);
  CHECK(world.contact == 0x08);
  ai::SetAiContactOccupancy(world, 32);
  CHECK(world.contact == 0x09);
  ai::ClearAiContactOccupancy(world, 35);
  CHECK(world.contact == 0x01);
  ai::SetAiContactOccupancy(world, 9);
  CHECK(world.contact == 0x01);
  // Clearing bit 31 ORs a mask whose top bit is clear.
  ai::ClearAiContactOccupancy(world, 31);
  CHECK(world.contact == 0xFF);
  ai::SetAiContactOccupancy(world, 31);
  CHECK(world.contact == 0x00);
  world.has_room_ai = false;
  world.contact = 0x10;
  ai::SetAiContactOccupancy(world, 0);
  ai::ClearAiContactOccupancy(world, 4);
  CHECK(world.contact == 0x10);
}

TEST_CASE("AI terrain movement holds the current tile",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.stall_counter = 9;
  ai::BeginAiTerrainMovement(world, navigator);
  CHECK(navigator.next_tile == navigator.tile);
  CHECK(navigator.stall_counter == 0);
  navigator.stall_counter = 9;
  navigator.next_tile = 0;
  ai::BeginAiAlternateTerrainMovement(world, navigator);
  CHECK(navigator.next_tile == navigator.tile);
  CHECK(navigator.stall_counter == 0);
  CHECK(world.calls == std::vector<Call>{{"command", 11}, {"command", 12}});
}

TEST_CASE("AI tile movement recovers at or walks to the destination",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.destination = navigator.tile | 0x7F000100U;
  REQUIRE(ai::MoveActorTowardDestinationTile(world, navigator));
  navigator.destination = Tile(9, 2);
  REQUIRE(ai::MoveActorTowardDestinationTile(world, navigator));
  CHECK(navigator.move_x == Centre(9));
  CHECK(navigator.move_z == Centre(2));
  CHECK(world.calls == std::vector<Call>{{"recover"}, {"area"}});
}

TEST_CASE("AI route links clear when the actor leaves the gate",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.contact = 0xFF;
  auto navigator = Navigator();
  navigator.route_gate = 4;
  REQUIRE(ai::MoveActorTowardDestinationTile(world, navigator));
  CHECK(navigator.route_gate == 0);
  CHECK(navigator.route_exit == ~0U);
  CHECK(world.contact == 0xEF);
}

TEST_CASE("AI route links step through or wait at their gate",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.contact = 0xFF;
  auto navigator = Navigator();
  navigator.route_gate = 4;
  REQUIRE(ai::MoveActorTowardDestinationTile(world, navigator));
  navigator.route_gate = 4;
  navigator.tile |= 0x0200;
  REQUIRE(ai::MoveActorTowardDestinationTile(world, navigator));
  world.gate = 2;
  REQUIRE(ai::MoveActorTowardDestinationTile(world, navigator));
  CHECK(navigator.next_tile == Tile(7, 2));
  CHECK(navigator.route_gate == 0);
  CHECK(world.contact == 0xE7);
  CHECK(world.calls == std::vector<Call>{{"recover"}, {"command", 14}});
}

TEST_CASE("AI route links refuse missing gates and invalid links",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.route_gate = 2;
  navigator.tile |= 0x0300;
  world.has_gates = false;
  CHECK_FALSE(ai::MoveActorTowardDestinationTile(world, navigator));
  world.has_gates = true;
  world.gate = 8;
  CHECK_FALSE(ai::MoveActorTowardDestinationTile(world, navigator));
  CHECK_FALSE(ai::BeginAiGroundMovement(world, navigator, 0));
  CHECK(navigator.route_gate == 2);
  CHECK(world.calls.empty());
}

TEST_CASE("AI locomotion modes dispatch tile and heading movement",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.destination = navigator.tile;
  REQUIRE(ai::DispatchAiLocomotionMode(world, navigator, 0, 5));
  for (const std::uint8_t mode : {1, 2}) {
    navigator.locomotion_mode = mode;
    REQUIRE(ai::DispatchAiLocomotionMode(world, navigator, 0, 5));
    REQUIRE(ai::DispatchAiLocomotionMode(world, navigator, 1, 6));
  }
  navigator.locomotion_mode = 0;
  navigator.height_filtered_movement = 1;
  REQUIRE(ai::DispatchAiLocomotionMode(world, navigator, 1, 7));
  CHECK(world.calls == std::vector<Call>{{"recover"},
                                         {"flight", 5},
                                         {"flight-heading", 6},
                                         {"radial", 5},
                                         {"away", 6},
                                         {"filtered"}});
}

TEST_CASE("AI locomotion modes refuse invalid modes and kinds",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.locomotion_mode = 3;
  CHECK_FALSE(ai::DispatchAiLocomotionMode(world, navigator, 0, 0));
  CHECK_FALSE(ai::BeginAiMovementTowardPoint(world, navigator, 1));
  navigator.locomotion_mode = 0;
  CHECK_FALSE(ai::DispatchAiLocomotionMode(world, navigator, 2, 0));
  CHECK_FALSE(ai::DispatchAiLocomotionMode(world, navigator, -1, 0));
  CHECK(world.calls.empty());
}

TEST_CASE("AI point movement targets the move point and heads toward it",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  navigator.locomotion_mode = 1;
  navigator.move_x = navigator.x + 300;
  navigator.move_z = navigator.z - 40;
  REQUIRE(ai::BeginAiMovementTowardPoint(world, navigator, 1));
  CHECK(navigator.gait == 1);
  CHECK(navigator.toward_point == 1);
  CHECK(navigator.target_x == navigator.move_x);
  CHECK(navigator.target_z == navigator.move_z);
  CHECK(world.calls ==
        std::vector<Call>{
            {"flight-heading", VS::Core::Math::Arctangent2(300, -40)}});
}

TEST_CASE("AI directional movement follows detours first",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  // A flight detour heading 5 whose cardinal cell still holds keeps walking.
  navigator.navigation_flags = DetourWord({.heading = 5, .cell = 15});
  REQUIRE(ai::ResolveDirectionalMovement(world, navigator, 0, 2));
  CHECK(navigator.heading == 5);
  // A spent walking detour ends with a cooldown and a recovery.
  navigator.navigation_flags = DetourWord({.heading = 5, .steps = 0});
  REQUIRE(ai::ResolveDirectionalMovement(world, navigator, 0, 0));
  CHECK(world.calls == std::vector<Call>{{"fly", 5},
                                         {"fly", 0},
                                         {"fly", 2},
                                         {"fly", 4},
                                         {"fly", 6},
                                         {"command", 5},
                                         {"cooldown"},
                                         {"recover"}});
}

TEST_CASE("AI directional movement heads along open directions",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  // The heading keeps the unmasked angle.
  REQUIRE(ai::ResolveDirectionalMovement(world, navigator, 0x1000, 2));
  CHECK(navigator.heading == 0x1000);
  world.fly = 0;
  world.adjacent = 2U << 24U;
  REQUIRE(ai::ResolveDirectionalMovement(world, navigator, 0, 2));
  CHECK(navigator.movement_wait == 254);
  CHECK(world.calls == std::vector<Call>{{"fly", 0},
                                         {"command", 7},
                                         {"fly", 0},
                                         {"adjacent", 0},
                                         {"recover"}});
}

TEST_CASE("AI directional movement refuses invalid modes and codes",
          "[ai][ground-navigation]") {
  FakeWorld world;
  auto navigator = Navigator();
  CHECK_FALSE(ai::ResolveDirectionalMovement(world, navigator, 0, 4));
  CHECK_FALSE(ai::ResolveDirectionalMovement(world, navigator, 0, -1));
  world.codes[0] = 0;
  CHECK_FALSE(ai::ResolveDirectionalMovement(world, navigator, 0, 2));
  CHECK(world.calls.empty());
}

TEST_CASE("AI directional movement refuses a refused detour",
          "[ai][ground-navigation]") {
  FakeWorld world;
  world.limits.reset();
  auto navigator = Navigator();
  navigator.navigation_flags =
      DetourWord({.heading = 0, .changed = true, .cell = 15});
  CHECK_FALSE(ai::ResolveDirectionalMovement(world, navigator, 0, 0));
  CHECK(world.calls.empty());
}
