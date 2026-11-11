#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/AreaRoute.hpp"
#include "Battle/Ai/AreaRouteLive.hpp"
#include "Battle/Ai/Wander.hpp"
#include "Battle/Ai/WanderLive.hpp"
#include "Content/ModuleData.h"
#include "Field/TerrainHeight.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
}

namespace {
namespace ai = VS::Battle::Ai;
namespace field = VS::Field;
using ai::WanderTileChoice;

// Direction i steps (0, -1), (-1, -1), (-1, 0), (-1, 1), (0, 1), (1, 1),
// (1, 0) and (1, -1).
constexpr ai::NavigationDirectionCodes DirectionCodes() {
  return {0x30, 0x33, 0x03, 0x13, 0x10, 0x11, 0x01, 0x31};
}
constexpr ai::NavigationDirectionCodes kCodes = DirectionCodes();
constexpr std::uint32_t kTileOther = 0x5A000700;
constexpr int kChoiceRange = 0x1000;
constexpr int kTimerRange = 128;
// The heading from tile (10, 10) toward tile (12, 10): direction 6.
constexpr int kTowardEast = 3072;

constexpr std::uint32_t Tile(int x, int z, std::uint32_t other = kTileOther) {
  return other | (static_cast<std::uint32_t>(z) << 16) |
         static_cast<std::uint32_t>(x);
}

constexpr std::int16_t Centre(int tile) {
  return static_cast<std::int16_t>((tile * 128) + 64);
}

constexpr std::size_t At(int value) { return static_cast<std::size_t>(value); }

bool InGrid(int x, int z) { return x >= 0 && z >= 0 && x < 32 && z < 32; }

// Terrain cells: the signed 10-bit height in 64-unit steps.
constexpr std::uint16_t kHeight128 = 2;
constexpr std::uint16_t kHeight192 = 3;
constexpr std::uint16_t kHeightMinus64 = 0x3FF;

struct Heights {
  field::TerrainRow rows[field::kTerrainGridSide]{};

  [[nodiscard]] field::TerrainGrid Grid() const {
    return field::TerrainGrid{
        std::span<const field::TerrainRow, field::kTerrainGridSide>(rows)};
  }
};

// Around tile (10, 10): direction 0's neighbour is 192 high, direction 2's
// 128, direction 4's -64 and direction 6's 192.
Heights FilterHeights() {
  Heights heights;
  heights.rows[9][10] = kHeight192;
  heights.rows[10][9] = kHeight128;
  heights.rows[11][10] = kHeightMinus64;
  heights.rows[10][11] = kHeight192;
  return heights;
}

struct FakeMoveWorld final : ai::HeightFilteredMoveWorld {
  std::optional<std::uint8_t> cell = std::uint8_t{0x05};
  mutable std::vector<std::pair<int, int>> cell_queries;
  std::vector<std::tuple<int, int, std::uint8_t>> writes;
  Heights heights = FilterHeights();
  bool has_terrain = true;
  std::optional<ai::DirectionalMoveOutcome> outcome =
      ai::DirectionalMoveOutcome{.movement_result = static_cast<int>(
                                     ai::CommandIntent::kMoveDirection),
                                 .heading = 3};
  std::vector<int> angles;
  std::vector<std::uint8_t> cells_during_move;

  [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(
      int x, int z) const override {
    cell_queries.emplace_back(x, z);
    return cell;
  }
  void SetTerrainRegionCell(int x, int z, std::uint8_t value) override {
    writes.emplace_back(x, z, value);
    cell = value;
  }
  [[nodiscard]] ai::NavigationDirectionCodes DirectionCodes() const override {
    return kCodes;
  }
  [[nodiscard]] field::TerrainGrid Terrain() const override {
    return has_terrain ? heights.Grid() : field::TerrainGrid{};
  }
  [[nodiscard]] std::optional<ai::DirectionalMoveOutcome> MoveInDirection(
      int angle) override {
    angles.push_back(angle);
    cells_during_move.push_back(cell.value_or(0xEE));
    return outcome;
  }
};

ai::HeightFilteredMover Mover() {
  return {.tile = Tile(10, 10),
          .x = Centre(10),
          .y = 128,
          .z = Centre(10),
          .move_x = Centre(12),
          .move_z = Centre(10)};
}

ai::HeightFilteredMove Walk(std::uint16_t heading) {
  return {.walk_heading = heading};
}
}  // namespace

TEST_CASE("Height filtering clears directions to higher neighbours",
          "[ai][height-filter]") {
  const auto heights = FilterHeights();
  const auto filter = [&](std::uint8_t mask, int maximum) {
    return ai::FilterDirectionsByGroundHeight(mask, Tile(10, 10), maximum,
                                              kCodes, heights.Grid());
  };
  CHECK(filter(0x15, 128) == std::uint8_t{0x14});
  CHECK(filter(0x15, 200) == std::uint8_t{0x15});
  // Heights compare signed: -64 is above -100.
  CHECK(filter(0x15, -100) == std::uint8_t{0x00});
  CHECK(filter(0x15, -64) == std::uint8_t{0x10});
  // Only set directions are tested; direction 6's neighbour is 192 high.
  CHECK(filter(0xFF, 128) == std::uint8_t{0xBE});
  CHECK(filter(0x00, -1000) == std::uint8_t{0x00});
}

TEST_CASE("Height filtering refuses set directions outside the terrain",
          "[ai][height-filter]") {
  const auto heights = FilterHeights();
  CHECK_FALSE(ai::FilterDirectionsByGroundHeight(0x04, Tile(0, 10), 0, kCodes,
                                                 heights.Grid()));
  CHECK(ai::FilterDirectionsByGroundHeight(0x40, Tile(0, 10), 0, kCodes,
                                           heights.Grid()) ==
        std::uint8_t{0x40});
  CHECK_FALSE(ai::FilterDirectionsByGroundHeight(0x20, Tile(31, 31), 0, kCodes,
                                                 heights.Grid()));
  CHECK_FALSE(ai::FilterDirectionsByGroundHeight(0x01, Tile(10, 10), 0, kCodes,
                                                 field::TerrainGrid{}));
  CHECK(ai::FilterDirectionsByGroundHeight(0x00, Tile(10, 10), 0, kCodes,
                                           field::TerrainGrid{}) ==
        std::uint8_t{0x00});
}

TEST_CASE("Height-filtered moves hide high directions only during the move",
          "[ai][height-filter]") {
  FakeMoveWorld world;
  CHECK(ai::MoveWithHeightFilteredDirections(world, Mover()) == Walk(3 << 9));
  CHECK(world.cell_queries == std::vector<std::pair<int, int>>{{10, 10}});
  CHECK(world.cells_during_move == std::vector<std::uint8_t>{0x04});
  CHECK(world.writes == std::vector<std::tuple<int, int, std::uint8_t>>{
                            {10, 10, 0x04}, {10, 10, 0x05}});
  CHECK(world.angles == std::vector<int>{kTowardEast});
  CHECK(world.cell == std::uint8_t{0x05});

  auto mover = Mover();
  mover.move_x = Centre(10);
  mover.move_z = Centre(9);
  CHECK(ai::MoveWithHeightFilteredDirections(world, mover));
  mover.move_z = Centre(11);
  CHECK(ai::MoveWithHeightFilteredDirections(world, mover));
  CHECK(world.angles == std::vector<int>{kTowardEast, 0, 2048});
}

TEST_CASE("Height-filtered moves walk after move-direction or heading results",
          "[ai][height-filter]") {
  FakeMoveWorld world;
  const auto result = [&](int movement_result, std::uint16_t heading) {
    world.outcome = ai::DirectionalMoveOutcome{
        .movement_result = movement_result, .heading = heading};
    return ai::MoveWithHeightFilteredDirections(world, Mover());
  };
  CHECK(result(5, 3) == Walk(1536));
  // The heading scales as a halfword.
  CHECK(result(5, 0x81) == Walk(0x0200));
  CHECK(result(7, 1234) == Walk(1234));
  // Only the result's low five bits select a walk; the heading is then
  // unscaled.
  CHECK(result(37, 1234) == Walk(1234));
  CHECK(result(39, 1234) == Walk(1234));
  CHECK(result(4, 1234) == ai::HeightFilteredMove{});
  CHECK(result(6, 1234) == ai::HeightFilteredMove{});
  CHECK(result(32, 1234) == ai::HeightFilteredMove{});
  CHECK(world.cell == std::uint8_t{0x05});
}

TEST_CASE("Height-filtered moves refuse missing regions or terrain",
          "[ai][height-filter]") {
  FakeMoveWorld world;
  SECTION("regions") { world.cell.reset(); }
  SECTION("terrain") { world.has_terrain = false; }
  CHECK_FALSE(ai::MoveWithHeightFilteredDirections(world, Mover()));
  CHECK(world.writes.empty());
  CHECK(world.angles.empty());
}

TEST_CASE("Height-filtered moves restore the cell when movement fails",
          "[ai][height-filter]") {
  FakeMoveWorld world;
  world.outcome.reset();
  CHECK_FALSE(ai::MoveWithHeightFilteredDirections(world, Mover()));
  CHECK(world.cell == std::uint8_t{0x05});
  CHECK(world.writes.size() == 2);
  CHECK(world.angles.size() == 1);
}

TEST_CASE("Height-filtered moves keep unset directions outside the terrain",
          "[ai][height-filter]") {
  FakeMoveWorld world;
  world.cell = std::uint8_t{0x40};
  auto mover = Mover();
  mover.tile = Tile(0, 10);
  CHECK(ai::MoveWithHeightFilteredDirections(world, mover));
  CHECK(world.cells_during_move == std::vector<std::uint8_t>{0x40});
}

namespace {
constexpr ai::WanderRoomBounds kRoomBounds{.max_x = 31, .max_z = 31};

class FakeWanderWorld final : public ai::WanderWorld {
 public:
  using Edge = std::tuple<std::uint32_t, std::uint32_t, int>;
  using Step = std::pair<std::uint32_t, int>;
  using Dispatch = std::pair<int, int>;
  struct Projection {
    std::uint32_t source = 0;
    int angle = 0;
    std::uint32_t distance = 0;
    friend bool operator==(const Projection&, const Projection&) = default;
  };

  FakeWanderWorld() {
    for (auto& row : flags) row.fill(1);
  }

  bool room_data = true;
  bool has_regions = true;
  std::array<std::array<std::uint8_t, 32>, 32> regions{};  // [z][x]
  bool has_flags = true;
  std::array<std::array<std::uint16_t, 32>, 32> flags{};  // [z][x]
  std::optional<ai::WanderRoomBounds> bounds = kRoomBounds;
  std::vector<int> draws;
  std::vector<int> ranges;
  std::set<Step> blocked_steps;
  std::vector<Step> step_queries;
  bool traversable = false;
  std::vector<Edge> edges;
  std::array<bool, 8> fly_open{};
  std::array<bool, 8> walk_open{};
  std::vector<int> fly_queries;
  std::vector<int> walk_queries;
  int cache_clears = 0;
  std::vector<Projection> projections;
  std::vector<Dispatch> dispatches;
  int recoveries = 0;

  [[nodiscard]] ai::NavigationDirectionCodes DirectionCodes() const override {
    return kCodes;
  }
  [[nodiscard]] bool HasRoomData() const override { return room_data; }
  [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(
      int x, int z) const override {
    if (!has_regions || !InGrid(x, z)) return std::nullopt;
    return regions.at(At(z)).at(At(x));
  }
  [[nodiscard]] std::optional<std::uint16_t> TileFlags(int x,
                                                       int z) const override {
    if (!has_flags || !InGrid(x, z)) return std::nullopt;
    return flags.at(At(z)).at(At(x));
  }
  [[nodiscard]] std::optional<ai::WanderRoomBounds> RoomBounds()
      const override {
    return bounds;
  }
  int NextRandomMasked(int range) override {
    ranges.push_back(range);
    return draws.at(ranges.size() - 1);
  }
  [[nodiscard]] bool IsAdjacentStepBlocked(std::uint32_t tile,
                                           int direction) override {
    step_queries.emplace_back(tile, direction);
    return blocked_steps.contains({tile, direction});
  }
  [[nodiscard]] bool CanTraverseEdge(ai::WanderActor& /*actor*/,
                                     std::uint32_t from, std::uint32_t to,
                                     int direction) override {
    edges.emplace_back(from, to, direction);
    return traversable;
  }
  [[nodiscard]] bool CanFlyInDirection(ai::WanderActor& /*actor*/,
                                       int direction) override {
    fly_queries.push_back(direction);
    return fly_open.at(At(direction));
  }
  [[nodiscard]] bool CanWalkInDirection(ai::WanderActor& /*actor*/,
                                        int direction) override {
    walk_queries.push_back(direction);
    return walk_open.at(At(direction));
  }
  void ClearDirectionCache() override { ++cache_clears; }
  void ProjectClearRadialDestination(ai::WanderActor& /*actor*/,
                                     std::uint32_t source, int angle,
                                     std::uint32_t distance) override {
    projections.push_back(
        {.source = source, .angle = angle, .distance = distance});
  }
  void DispatchLocomotion(ai::WanderActor& /*actor*/, int kind,
                          int angle) override {
    dispatches.emplace_back(kind, angle);
  }
  void RecoverMovement(ai::WanderActor& /*actor*/) override { ++recoveries; }
};

ai::WanderActor Wanderer() {
  return {.tile = Tile(10, 10),
          .x = Centre(10),
          .z = Centre(10),
          .move_distance = 300,
          .phase = 1,
          .heading = -1,
          .cooldown = 5,
          .rest_period = 5,
          .move_period = 10,
          .rate = 3,
          .goal = Tile(1, 1)};
}

// A wanderer partway through its chain at tile (10, 10).
ai::WanderActor Chaining(std::int16_t heading) {
  auto actor = Wanderer();
  actor.goal = Tile(10, 10);
  actor.count = 1;
  actor.heading = heading;
  return actor;
}

std::vector<int> Ranges(std::initializer_list<int> ranges) { return ranges; }

// A tick that recovers once leaves the actor stuck, clears the direction
// cache once and dispatches nothing.
void CheckRecoveredOnce(const FakeWanderWorld& world,
                        const ai::WanderActor& actor) {
  CHECK(world.recoveries == 1);
  CHECK(actor.is_stuck == 1);
  CHECK(world.cache_clears == 1);
  CHECK(world.dispatches.empty());
}

// A narrow wanderer walking its chain dispatches one walk without
// recovering.
void CheckWalkedToGoal(const FakeWanderWorld& world,
                       const ai::WanderActor& actor) {
  CHECK(world.dispatches == std::vector<FakeWanderWorld::Dispatch>{{0, 0}});
  CHECK(world.recoveries == 0);
  CHECK(actor.height_filter == 1);
}
}  // namespace

TEST_CASE("Wander direction steps decode the codes and refuse other indices",
          "[ai][wander]") {
  CHECK(ai::WanderDirectionStep(kCodes, 0) == ai::NavigationStep{0, -1});
  CHECK(ai::WanderDirectionStep(kCodes, 3) == ai::NavigationStep{-1, 1});
  CHECK(ai::WanderDirectionStep(kCodes, 6) == ai::NavigationStep{1, 0});
  CHECK(ai::WanderDirectionStep(kCodes, 7) == ai::NavigationStep{1, -1});
  CHECK_FALSE(ai::WanderDirectionStep(kCodes, -1));
  CHECK_FALSE(ai::WanderDirectionStep(kCodes, 8));
}

TEST_CASE("Random wander headings jitter half a direction either side",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {0, 511, 256, 511};
  CHECK(ai::RandomWanderHeading(world, 0) == -256);
  CHECK(ai::RandomWanderHeading(world, 0) == 255);
  CHECK(ai::RandomWanderHeading(world, 2) == 1024);
  CHECK(ai::RandomWanderHeading(world, 7) == 3839);
  CHECK(world.ranges == Ranges({512, 512, 512, 512}));
}

TEST_CASE("Starting a wander walk clears the chain and recovers",
          "[ai][wander]") {
  FakeWanderWorld world;
  auto actor = Chaining(5);
  actor.phase = 0;
  actor.count = 3;
  actor.cooldown = 2;
  ai::StartWanderWalk(world, actor);
  CHECK(actor.heading == -1);
  CHECK(actor.count == 0);
  CHECK(actor.cooldown == 60);
  CHECK(actor.phase == 1);
  CHECK(world.recoveries == 1);
}

TEST_CASE("Wander chains start at the tile and step to a drawn neighbour",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {6};
  auto actor = Wanderer();
  actor.count = 0;
  CHECK(ai::ChooseNextWanderTile(world, actor, Tile(10, 10), 1) ==
        WanderTileChoice::kExtended);
  // 6 % 4 picks the third even direction, 4.
  CHECK(actor.goal == Tile(10, 11));
  CHECK(actor.heading == 4);
  CHECK(actor.count == 1);
  CHECK(world.ranges == Ranges({kChoiceRange}));
}

TEST_CASE("Later wander steps start from the goal and complete the chain",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  actor.goal = Tile(10, 11);
  actor.heading = 4;
  actor.count = 1;
  CHECK(ai::ChooseNextWanderTile(world, actor, Tile(20, 20), 1) ==
        WanderTileChoice::kComplete);
  CHECK(actor.goal == Tile(9, 11));
  CHECK(actor.heading == 2);
  CHECK(actor.count == 2);
  CHECK(world.ranges == Ranges({kChoiceRange}));
  CHECK(world.step_queries.empty());
  CHECK(world.edges.empty());
}

TEST_CASE("Wander chains skip blocked, other-area and no-wander neighbours",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.regions[10][10] = 0x01;
  world.flags[10][9] = 2;
  world.flags[11][10] = 0x21;
  world.draws = {3};
  auto actor = Chaining(-1);
  CHECK(ai::ChooseNextWanderTile(world, actor, 0, 5) ==
        WanderTileChoice::kExtended);
  CHECK(actor.goal == Tile(11, 10));
  CHECK(actor.heading == 6);
  // A blocked neighbour is offered to the edge test with its blocked tag.
  CHECK(world.edges == std::vector<FakeWanderWorld::Edge>{
                           {Tile(10, 10), Tile(10, 9, 0x5A000100), 0}});

  world.traversable = true;
  world.draws = {0};
  world.ranges.clear();
  actor = Chaining(-1);
  CHECK(ai::ChooseNextWanderTile(world, actor, 0, 5) ==
        WanderTileChoice::kExtended);
  CHECK(actor.goal == Tile(10, 9));
  CHECK(actor.heading == 0);
}

TEST_CASE("Stuck wanderers cross areas but still skip no-wander tiles",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.flags[10][9] = 2;
  world.flags[9][10] = 0x21;
  world.flags[11][10] = 0x21;
  world.draws = {0};
  auto actor = Chaining(-1);
  actor.is_stuck = 1;
  CHECK(ai::ChooseNextWanderTile(world, actor, 0, 5) ==
        WanderTileChoice::kExtended);
  CHECK(actor.goal == Tile(9, 10));
}

TEST_CASE("Rooms without route data test each adjacent step", "[ai][wander]") {
  FakeWanderWorld world;
  world.room_data = false;
  world.has_regions = false;
  world.blocked_steps = {
      {Tile(10, 10), 0}, {Tile(10, 10), 2}, {Tile(10, 10), 4}};
  world.draws = {7};
  auto actor = Chaining(-1);
  CHECK(ai::ChooseNextWanderTile(world, actor, 0, 5) ==
        WanderTileChoice::kExtended);
  CHECK(actor.goal == Tile(11, 10));
  CHECK(world.step_queries ==
        std::vector<FakeWanderWorld::Step>{{Tile(10, 10), 0},
                                           {Tile(10, 10), 2},
                                           {Tile(10, 10), 4},
                                           {Tile(10, 10), 6}});
  CHECK(world.edges.size() == 3);
}

TEST_CASE("Projecting wanderers check only the goal's own no-wander flag",
          "[ai][wander]") {
  FakeWanderWorld world;
  for (auto& row : world.regions) row.fill(0xFF);
  world.flags[9][10] = 0x21;
  world.flags[10][9] = 0x21;
  world.flags[11][10] = 0x21;
  world.flags[10][11] = 0x21;
  world.draws = {2};
  auto actor = Chaining(-1);
  actor.movement_flags = 8;
  CHECK(ai::ChooseNextWanderTile(world, actor, 0, 5) ==
        WanderTileChoice::kExtended);
  CHECK(actor.goal == Tile(10, 11));
  CHECK(world.edges.empty());

  world.flags[10][10] = 0x21;
  actor = Chaining(-1);
  actor.movement_flags = 8;
  CHECK(ai::ChooseNextWanderTile(world, actor, 0, 5) ==
        WanderTileChoice::kBlocked);
  CHECK(actor.goal == Tile(10, 10));
  CHECK(world.ranges.size() == 1);
}

TEST_CASE(
    "Wander draws avoid turning straight back once after a positive "
    "heading",
    "[ai][wander]") {
  FakeWanderWorld world;
  auto actor = Chaining(2);
  SECTION("the redraw may turn back again") {
    world.draws = {3, 3};
    CHECK(ai::ChooseNextWanderTile(world, actor, 0, 5));
    CHECK(actor.goal == Tile(11, 10));
  }
  SECTION("the redraw replaces the turn back") {
    world.draws = {3, 0};
    CHECK(ai::ChooseNextWanderTile(world, actor, 0, 5));
    CHECK(actor.goal == Tile(10, 9));
    CHECK(actor.heading == 0);
  }
  CHECK(world.ranges == Ranges({kChoiceRange, kChoiceRange}));
}

TEST_CASE("Heading 0 and fresh wander chains never redraw a turn back",
          "[ai][wander]") {
  const int heading = GENERATE(0, -1);
  FakeWanderWorld world;
  world.draws = {2};
  auto actor = Chaining(static_cast<std::int16_t>(heading));
  CHECK(ai::ChooseNextWanderTile(world, actor, 0, 5));
  CHECK(actor.goal == Tile(10, 11));
  CHECK(world.ranges.size() == 1);
}

TEST_CASE("Zero ranges and enclosed goals end the wander chain",
          "[ai][wander]") {
  FakeWanderWorld world;
  auto actor = Wanderer();
  actor.count = 0;
  CHECK(ai::ChooseNextWanderTile(world, actor, Tile(10, 10), 0) ==
        WanderTileChoice::kBlocked);
  CHECK(actor.goal == Tile(1, 1));

  world.flags[9][10] = 0x21;
  world.flags[10][9] = 0x21;
  world.flags[11][10] = 0x21;
  world.flags[10][11] = 0x21;
  CHECK(ai::ChooseNextWanderTile(world, actor, Tile(10, 10), 3) ==
        WanderTileChoice::kBlocked);
  CHECK(actor.goal == Tile(10, 10));
  CHECK(actor.count == 0);
  CHECK(actor.heading == -1);
  CHECK(world.ranges.empty());
}

TEST_CASE("The wander chain count is a byte", "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {0};
  auto actor = Chaining(-1);
  actor.count = 255;
  CHECK(ai::ChooseNextWanderTile(world, actor, 0, 1) ==
        WanderTileChoice::kExtended);
  CHECK(actor.count == 0);
}

TEST_CASE("Wander tile choices refuse goals and neighbours outside the maps",
          "[ai][wander]") {
  FakeWanderWorld world;
  auto actor = Chaining(-1);
  SECTION("regions") { world.has_regions = false; }
  SECTION("flags") { world.has_flags = false; }
  SECTION("goal") { actor.goal = Tile(40, 10); }
  SECTION("neighbour") { actor.goal = Tile(0, 10); }
  CHECK_FALSE(ai::ChooseNextWanderTile(world, actor, 0, 5));
  CHECK(world.ranges.empty());
}

TEST_CASE("Held wanderers wait out the hold before drawing", "[ai][wander]") {
  FakeWanderWorld world;
  auto actor = Wanderer();
  actor.behavior_mode = 1;
  actor.timer = 29;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
  CHECK(actor.timer == 30);
  CHECK(world.ranges.empty());
  CHECK(actor.is_stuck == 0);
  CHECK(world.cache_clears == 0);
}

TEST_CASE("A high draw after the hold holds the wanderer again",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {44};
  auto actor = Wanderer();
  actor.behavior_mode = 1;
  actor.timer = 30;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
  CHECK(actor.timer == 0);
  CHECK(actor.behavior_mode == 1);
  CHECK(actor.is_stuck == 0);
  CHECK(world.ranges == Ranges({kTimerRange}));
}

TEST_CASE("A low draw after the hold releases the wanderer", "[ai][wander]") {
  FakeWanderWorld world;
  // The released actor's timer then passes both periods.
  world.draws = {43, 0};
  auto actor = Wanderer();
  actor.behavior_mode = 1;
  actor.timer = 30;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
  CHECK(actor.behavior_mode == 2);
  CHECK(actor.timer == 10);
  CHECK(world.ranges == Ranges({kTimerRange, kTimerRange}));
  CheckRecoveredOnce(world, actor);
}

TEST_CASE("Wander timers switch periods on a low draw", "[ai][wander]") {
  struct Tick {
    std::uint16_t timer = 0;
    std::vector<int> draws;
    std::uint16_t next_timer = 0;
    std::vector<int> ranges;
    // Phase 3 is unknown and starts over unless the tick recovers first.
    std::uint16_t phase = 0;
  };
  // The move period is 10 ticks and the rest 5.
  const auto tick = GENERATE(
      // Moving on.
      Tick{5, {1}, 6, {8}, 0},
      // The move period ends on a high draw.
      Tick{9, {44}, 10, {kTimerRange}, 3},
      // A low draw skips the rest.
      Tick{9, {43, 1}, 0, {kTimerRange, 8}, 0},
      // Resting.
      Tick{11, {}, 12, {}, 3},
      // The rest ends on a high draw.
      Tick{14, {44, 1}, 0, {kTimerRange, 8}, 0},
      // A low draw at the rest's end rests again.
      Tick{14, {43}, 10, {kTimerRange}, 3});
  FakeWanderWorld world;
  world.draws = tick.draws;
  auto actor = Wanderer();
  actor.phase = 3;
  actor.timer = tick.timer;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
  CHECK(actor.timer == tick.next_timer);
  CHECK(world.ranges == tick.ranges);
  CHECK(actor.phase == tick.phase);
  CheckRecoveredOnce(world, actor);
}

TEST_CASE("A zero rate draw restarts a narrow wander", "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {0};
  auto actor = Chaining(4);
  actor.phase = 2;
  CHECK(ai::UpdateWanderMovement(world, actor, Tile(10, 10), 1));
  CHECK(actor.height_filter == 1);
  CHECK(actor.phase == 1);
  CHECK(actor.heading == -1);
  CHECK(actor.cooldown == 60);
  CHECK(world.recoveries == 1);
  CHECK(world.ranges == Ranges({8}));
}

TEST_CASE("Narrow wanders extend their tile chain", "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1, 6};
  auto actor = Wanderer();
  CHECK(ai::UpdateWanderMovement(world, actor, Tile(10, 10), 1));
  CHECK(actor.phase == 1);
  CHECK(actor.goal == Tile(10, 11));
  CHECK(actor.count == 1);
  CHECK(world.recoveries == 1);
  CHECK(world.ranges == Ranges({8, kChoiceRange}));
}

TEST_CASE("Narrow wanders complete their tile chain", "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1, 1};
  auto actor = Wanderer();
  actor.goal = Tile(10, 11);
  actor.heading = 4;
  actor.count = 1;
  // From (10, 11) after heading 4, direction 2 avoids the turn-back redraw.
  CHECK(ai::UpdateWanderMovement(world, actor, Tile(10, 10), 1));
  CHECK(actor.phase == 2);
  CHECK(actor.goal == Tile(9, 11));
  CHECK(actor.destination == Tile(9, 11));
  CHECK(world.recoveries == 1);
  CHECK(world.dispatches.empty());
}

TEST_CASE("Blocked narrow wander chains start over", "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  CHECK(ai::UpdateWanderMovement(world, actor, Tile(10, 10), 0));
  CHECK(actor.phase == 0);
  CHECK(world.recoveries == 1);
}

TEST_CASE("Wanderers walk toward the chain's goal", "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  actor.phase = 2;
  actor.goal = Tile(12, 10);
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
  CHECK(actor.destination == Tile(12, 10));
  CHECK(actor.cooldown == 60);
  CHECK(actor.phase == 2);
  CHECK(world.projections.empty());
  CheckWalkedToGoal(world, actor);
}

TEST_CASE("Wanderers staying on a tile count down and restart at zero",
          "[ai][wander]") {
  const int cooldown = GENERATE(1, 5);
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  actor.phase = 2;
  actor.goal = Tile(12, 10);
  actor.stayed_on_tile = 1;
  actor.cooldown = static_cast<std::uint16_t>(cooldown);
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
  CHECK(actor.cooldown == cooldown - 1);
  CHECK(actor.phase == (cooldown == 1 ? 0 : 2));
  CheckWalkedToGoal(world, actor);
}

TEST_CASE("Wanderers arrive when only the goal's coordinates match",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  actor.phase = 2;
  actor.goal = Tile(10, 10, 0x11002200);
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
  CHECK(actor.phase == 0);
  CheckWalkedToGoal(world, actor);
}

TEST_CASE("Projecting wanderers project toward the chain's goal",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  actor.phase = 2;
  actor.goal = Tile(12, 10);
  actor.movement_flags = 8;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
  CHECK(actor.saved_destination == Tile(12, 10));
  CHECK(world.projections ==
        std::vector<FakeWanderWorld::Projection>{
            {.source = Tile(10, 10), .angle = kTowardEast, .distance = 300}});
  CheckWalkedToGoal(world, actor);
}

namespace {
// A wide wanderer's first tick with only direction 2 open to walking.
struct FirstWideTick {
  FakeWanderWorld world;
  ai::WanderActor actor = Wanderer();
  bool updated = false;

  FirstWideTick() {
    world.walk_open[2] = true;
    world.draws = {100, 1, 256};
    actor.phase = 0;
    updated = ai::UpdateWanderMovement(world, actor, 0, 63);
  }
};
}  // namespace

TEST_CASE("Wide wanders draw a heading on their first tick", "[ai][wander]") {
  const FirstWideTick tick;
  CHECK(tick.updated);
  // No rate draw before the first tick's restart.
  CHECK(tick.world.ranges == Ranges({kChoiceRange, 4, 512}));
  CHECK(tick.actor.range_squared == 165 * 165);
  CHECK(tick.actor.phase == 1);
  CHECK(tick.actor.heading == 1024);
  CHECK(tick.actor.cooldown == 60);
}

TEST_CASE("Wide wanders walk their first heading without recovering",
          "[ai][wander]") {
  const FirstWideTick tick;
  CHECK(tick.world.walk_queries == std::vector<int>{2});
  CHECK(tick.world.dispatches ==
        std::vector<FakeWanderWorld::Dispatch>{{1, 1024}});
  CHECK(tick.world.recoveries == 0);
  CHECK(tick.actor.height_filter == 0);
  CHECK(tick.actor.is_stuck == 1);
}

TEST_CASE("Wide wanders take the next open even direction", "[ai][wander]") {
  FakeWanderWorld world;
  world.walk_open[4] = true;
  world.draws = {0, 3, 0};
  auto actor = Wanderer();
  actor.phase = 0;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 100));
  CHECK(world.walk_queries == std::vector<int>{6, 0, 2, 4});
  CHECK(actor.heading == 1792);
  CHECK(actor.range_squared == 128 * 128);
}

TEST_CASE("Wide wanders without an open direction take any heading",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {0, 1, 3000};
  auto actor = Wanderer();
  actor.phase = 0;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 100));
  CHECK(world.walk_queries == std::vector<int>{2, 4, 6, 0});
  CHECK(actor.heading == 3000);
}

TEST_CASE("Wide-wandering fliers test flight", "[ai][wander]") {
  FakeWanderWorld world;
  world.fly_open[2] = true;
  world.draws = {0, 1, 0};
  auto actor = Wanderer();
  actor.phase = 0;
  actor.flies = true;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 100));
  CHECK(world.fly_queries == std::vector<int>{2});
  CHECK(world.walk_queries.empty());
  CHECK(actor.heading == 768);
}

TEST_CASE("Wide wanders keep their heading until a restart is due",
          "[ai][wander]") {
  const int stayed = GENERATE(0, 1);
  FakeWanderWorld world;
  world.walk_open[0] = true;
  world.draws = {1};
  auto actor = Wanderer();
  actor.heading = 1024;
  actor.stayed_on_tile = static_cast<std::uint8_t>(stayed);
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 63));
  CHECK(actor.heading == 1024);
  // Leaving the tile resets the cooldown.
  CHECK(actor.cooldown == (stayed != 0 ? 4 : 60));
  CHECK(world.ranges == Ranges({8}));
  CHECK(world.dispatches.size() == 1);
}

TEST_CASE("Wide wanders draw a new heading on a zero rate draw",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.walk_open[0] = true;
  world.draws = {0, 0, 0, 0};
  auto actor = Wanderer();
  actor.heading = 1024;
  actor.stayed_on_tile = 1;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 63));
  CHECK(actor.heading == -256);
  CHECK(actor.cooldown == 59);
  CHECK(world.dispatches.size() == 1);
}

TEST_CASE("Wide wanders draw a new heading when the cooldown expires",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.walk_open[0] = true;
  world.draws = {1, 0, 0, 511};
  auto actor = Wanderer();
  actor.heading = 1024;
  actor.stayed_on_tile = 1;
  actor.cooldown = 0;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 63));
  CHECK(actor.heading == 255);
  CHECK(world.ranges == Ranges({8, kChoiceRange, 4, 512}));
  CHECK(world.dispatches.size() == 1);
}

TEST_CASE("Wide wanders end the cooldown at the room bounds or a barrier",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  // Direction 2 steps to (9, 10), then direction 4 to (10, 11).
  actor.heading = 1024;
  actor.stayed_on_tile = 1;
  std::uint16_t cooldown = 0;
  SECTION("open") { cooldown = 4; }
  SECTION("below the minimum x") {
    auto bounds = kRoomBounds;
    bounds.min_x = 10;
    world.bounds = bounds;
  }
  SECTION("a barrier") { world.flags[10][9] = 0x801; }
  SECTION("above the maximum z") {
    auto bounds = kRoomBounds;
    bounds.max_z = 10;
    world.bounds = bounds;
  }
  SECTION("a second barrier") { world.flags[11][10] = 0x801; }
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 63));
  CHECK(actor.cooldown == cooldown);
  CHECK(world.dispatches == std::vector<FakeWanderWorld::Dispatch>{{1, 1024}});
}

TEST_CASE(
    "Negative wide headings check direction 7, not the byte before the "
    "codes",
    "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  actor.stayed_on_tile = 1;
  std::int16_t heading = 0;
  std::uint16_t cooldown = 0;
  SECTION("direction 7's tile is a barrier") {
    heading = -256;
    world.flags[9][11] = 0x801;
  }
  SECTION("direction 1's tile is a barrier") {
    heading = -1;
    world.flags[9][9] = 0x801;
  }
  SECTION("other tiles are barriers") {
    heading = -256;
    world.flags[10][9] = 0x801;
    world.flags[11][10] = 0x801;
    cooldown = 4;
  }
  actor.heading = heading;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 63));
  CHECK(actor.cooldown == cooldown);
}

TEST_CASE("Projecting wide wanders project along their heading",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  actor.heading = 1024;
  actor.movement_flags = 8;
  CHECK(ai::UpdateWanderMovement(world, actor, 0, 63));
  CHECK(world.projections ==
        std::vector<FakeWanderWorld::Projection>{
            {.source = Tile(10, 10), .angle = 1024, .distance = 300}});
}

TEST_CASE("Wander rate draws use the rate's low five bits", "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  actor.phase = 3;
  SECTION("wrapped") {
    actor.rate = 35;
    CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
    CHECK(world.ranges == Ranges({8}));
  }
  SECTION("the sign bit") {
    actor.rate = 31;
    CHECK(ai::UpdateWanderMovement(world, actor, 0, 3));
    CHECK(world.ranges == Ranges({INT_MIN}));
  }
}

TEST_CASE("Wander movement refuses missing room bounds or tile flags",
          "[ai][wander]") {
  FakeWanderWorld world;
  world.draws = {1};
  auto actor = Wanderer();
  actor.heading = 1024;
  std::uint32_t range = 63;
  SECTION("bounds") { world.bounds.reset(); }
  SECTION("flags") { world.has_flags = false; }
  SECTION("chain") {
    world.has_regions = false;
    actor.goal = Tile(10, 10);
    actor.count = 1;
    range = 3;
  }
  CHECK_FALSE(ai::UpdateWanderMovement(world, actor, 0, range));
  CHECK(world.dispatches.empty());
}

namespace {
struct LiveFrameFixture {
  Battle_Ai_FrameContext saved = *Battle_Ai_FrameState();
  u_short flags[32][32]{};
  u_short heights[32][32]{};
  std::array<u_char, std::size_t{32} * 32U> regions{};
  int room_data = 0;

  LiveFrameFixture() {
    auto& frame = *Battle_Ai_FrameState();
    for (auto& row : flags) {
      for (auto& cell : row) cell = 1;
    }
    std::copy(kCodes.begin(), kCodes.end(), std::begin(frame.directions));
    frame.tileFlags = flags;
    frame.heightMap = heights;
    frame.terrainRegions = regions.data();
    frame.terrain_region_count = regions.size();
    frame.roomBounds = 32U | (32U << 8);
    frame.roomData = &room_data;
  }
  ~LiveFrameFixture() { *Battle_Ai_FrameState() = saved; }
  LiveFrameFixture(const LiveFrameFixture&) = delete;
  LiveFrameFixture& operator=(const LiveFrameFixture&) = delete;
};
}  // namespace

TEST_CASE("Live height filtering reads the frame's codes and terrain",
          "[ai][height-filter][height-filter-live]") {
  LiveFrameFixture fixture;
  fixture.heights[9][10] = kHeight192;
  fixture.heights[10][9] = kHeight128;
  CHECK(ai::FilterLiveDirectionsByGroundHeight(0x05, Tile(10, 10), 128) ==
        0x04);
  CHECK(ai::FilterLiveDirectionsByGroundHeight(0x05, Tile(10, 10), 127) ==
        0x00);
}

TEST_CASE("Live wander choices write the chain back to the AI state",
          "[ai][wander][wander-live]") {
  LiveFrameFixture fixture;
  fixture.flags[9][10] = 0x21;
  fixture.flags[10][9] = 0x21;
  fixture.flags[11][10] = 0x21;
  Battle_Ai_ActorAiState state{};
  auto* const view = reinterpret_cast<Battle_Ai_WanderView*>(&state);
  view->goal.word = Tile(1, 1);
  view->wanderHeading = -1;
  view->x = 0x1234;
  CHECK(ai::ChooseNextLiveWanderTile(view, Tile(10, 10), 3, 0) == 0);
  CHECK(view->goal.word == Tile(11, 10));
  CHECK(view->wanderHeading == 6);
  CHECK(view->count == 1);
  CHECK(view->x == 0x1234);
  CHECK(ai::ChooseNextLiveWanderTile(view, Tile(10, 10), 0, 0) == -1);
}

TEST_CASE("Live held wanderers only count the hold",
          "[ai][wander][wander-live]") {
  LiveFrameFixture fixture;
  Battle_Ai_WanderRecordView record{};
  Battle_Ai_ActorAiState state{};
  auto* const view = reinterpret_cast<Battle_Ai_WanderView*>(&state);
  view->model = &record;
  view->behaviorMode = 1;
  view->wanderTimer = 5;
  Battle_Ai_FrameState()->walk_valid = 1;
  ai::UpdateLiveWanderMovement(view, Tile(10, 10), 3);
  CHECK(view->wanderTimer == 6);
  CHECK(view->isStuck == 0);
  CHECK(Battle_Ai_FrameState()->walk_valid == 1);
}

TEST_CASE("Height filtering and wander keep their original module entries",
          "[ai][height-filter][wander][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [symbols](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800E1850U) ==
        reinterpret_cast<void*>(&ai::FilterLiveDirectionsByGroundHeight));
  CHECK(resolve(0x800E1908U) ==
        reinterpret_cast<void*>(&ai::MoveLiveWithHeightFilteredDirections));
  CHECK(resolve(0x800E1BB8U) ==
        reinterpret_cast<void*>(&ai::ChooseNextLiveWanderTile));
  CHECK(resolve(0x800E1EACU) ==
        reinterpret_cast<void*>(&ai::UpdateLiveWanderMovement));
}
