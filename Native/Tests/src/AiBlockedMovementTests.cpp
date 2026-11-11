#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/BlockedMovement.hpp"
#include "Battle/Ai/BlockedMovementLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/ScalarMath.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
}

namespace {
namespace ai = VS::Battle::Ai;

// Direction i steps (0, -1), (-1, -1), (-1, 0), (-1, 1), (0, 1), (1, 1),
// (1, 0) and (1, -1).
constexpr ai::NavigationDirectionCodes DirectionCodes() {
  return {0x30, 0x33, 0x03, 0x13, 0x10, 0x11, 0x01, 0x31};
}
constexpr ai::NavigationDirectionCodes kCodes = DirectionCodes();
// Bytes 1 and 3 of a tile word, which carry per-actor metadata.
constexpr std::uint32_t kTag = 0x5A000700;

constexpr std::uint32_t Tile(int x, int z, std::uint32_t other = 0) {
  return other | (static_cast<std::uint32_t>(z) << 16) |
         static_cast<std::uint32_t>(x);
}

constexpr std::int16_t Centre(int tile) {
  return static_cast<std::int16_t>((tile * 128) + 64);
}

TEST_CASE("The nearest axis direction rounds past the quadrant midpoint",
          "[ai][blocked-movement]") {
  STATIC_CHECK(ai::NearestAxisDirection(0) == 0);
  STATIC_CHECK(ai::NearestAxisDirection(0x200) == 0);
  STATIC_CHECK(ai::NearestAxisDirection(0x201) == 2);
  STATIC_CHECK(ai::NearestAxisDirection(0x5FF) == 2);
  STATIC_CHECK(ai::NearestAxisDirection(0x601) == 4);
  STATIC_CHECK(ai::NearestAxisDirection(0xC00) == 6);
  STATIC_CHECK(ai::NearestAxisDirection(0xE01) == 0);
  STATIC_CHECK(ai::NearestAxisDirection(0x1C00) == 6);
}

struct EdgeCall {
  std::uint32_t from = 0;
  std::uint32_t to = 0;
  int direction = 0;
  friend bool operator==(const EdgeCall&, const EdgeCall&) = default;
};

struct GapCall {
  std::uint32_t tile = 0;
  int direction = 0;
  std::uint32_t from = 0;
  friend bool operator==(const GapCall&, const GapCall&) = default;
};

struct FakeAdjacentWorld final : ai::AdjacentDestinationWorld {
  std::deque<std::optional<int>> edges;
  std::deque<std::optional<int>> occupancy;
  std::deque<std::optional<std::uint32_t>> gaps;
  std::vector<EdgeCall> edge_calls;
  std::vector<std::uint32_t> occupancy_calls;
  std::vector<GapCall> gap_calls;

  [[nodiscard]] ai::NavigationDirectionCodes DirectionCodes() const override {
    return kCodes;
  }
  [[nodiscard]] std::optional<int> CanTraverseTileEdge(std::uint32_t from,
                                                       std::uint32_t to,
                                                       int direction) override {
    edge_calls.push_back({from, to, direction});
    return Next(edges);
  }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      std::uint32_t tile) override {
    occupancy_calls.push_back(tile);
    return Next(occupancy);
  }
  [[nodiscard]] std::optional<std::uint32_t> FindGapLandingTile(
      std::uint32_t tile, int direction, std::uint32_t from) override {
    gap_calls.push_back({tile, direction, from});
    return Next(gaps);
  }

 private:
  template <typename T>
  static std::optional<T> Next(std::deque<std::optional<T>>& answers) {
    REQUIRE_FALSE(answers.empty());
    const auto answer = answers.front();
    answers.pop_front();
    return answer;
  }
};

// On tile (10, 10) with metadata in bytes 1 and 3.
constexpr ai::AdjacentMover AdjacentMover() {
  return {.movement_disabled = false, .tile = Tile(10, 10, kTag)};
}
constexpr ai::AdjacentMover kMover = AdjacentMover();
// One probe toward +z; the direction's upper half is ignored.
constexpr std::uint32_t kTowardPlusZ = 0x10004;
constexpr std::uint32_t kStatusOne = 0x01000000;

TEST_CASE("Adjacent destinations return a free neighbour",
          "[ai][blocked-movement]") {
  FakeAdjacentWorld world;
  world.edges = {1};
  world.occupancy = {1};
  CHECK(ai::FindAdjacentMovementDestination(
            world, kMover, 3, 3, kTowardPlusZ) == Tile(10, 11, kStatusOne));
  CHECK(world.edge_calls ==
        std::vector<EdgeCall>{{kMover.tile, Tile(10, 11, kStatusOne), 4}});
  CHECK(world.occupancy_calls ==
        std::vector<std::uint32_t>{Tile(10, 11, kStatusOne)});
  CHECK(world.gap_calls.empty());
}

TEST_CASE("Adjacent destinations block on a held tile or a radius edge",
          "[ai][blocked-movement]") {
  FakeAdjacentWorld world;
  const std::uint32_t blocked = Tile(10, 11, 0x02000000);
  SECTION("another actor holds the neighbour") {
    world.edges = {1};
    world.occupancy = {-1};
    CHECK(ai::FindAdjacentMovementDestination(world, kMover, 3, 3,
                                              kTowardPlusZ) == blocked);
  }
  SECTION("the edge lies inside the approach radius") {
    world.edges = {-1};
    CHECK(ai::FindAdjacentMovementDestination(world, kMover, 3, 3,
                                              kTowardPlusZ) == blocked);
    CHECK(world.occupancy_calls.empty());
  }
}

TEST_CASE("Adjacent destinations return 0 for a blocked neighbour",
          "[ai][blocked-movement]") {
  FakeAdjacentWorld world;
  world.edges = {1};
  world.occupancy = {0};
  CHECK(ai::FindAdjacentMovementDestination(world, kMover, 3, 3,
                                            kTowardPlusZ) == 0U);
}

TEST_CASE("Adjacent destinations take a gap landing's status",
          "[ai][blocked-movement]") {
  FakeAdjacentWorld world;
  world.edges = {0};
  SECTION("a found landing clears the tag byte") {
    world.gaps = {0x010C770AU};
    CHECK(ai::FindAdjacentMovementDestination(world, kMover, 3, 3,
                                              kTowardPlusZ) == 0x010C000AU);
    CHECK(world.gap_calls ==
          std::vector<GapCall>{{Tile(10, 11, kStatusOne), 4, kMover.tile}});
  }
  SECTION("another status blocks") {
    world.gaps = {0x030C770AU};
    CHECK(ai::FindAdjacentMovementDestination(world, kMover, 3, 3,
                                              kTowardPlusZ) == 0x020C000AU);
  }
  SECTION("status 0 with one direction returns 0") {
    world.gaps = {0x000C770AU};
    CHECK(ai::FindAdjacentMovementDestination(world, kMover, 3, 3,
                                              kTowardPlusZ) == 0U);
  }
}

// Angle 0x400 is nearest axis direction 2 (-x).
constexpr std::uint32_t kNearMinusX = 0x400;

TEST_CASE("Adjacent destinations try the nearest axis, then first or second",
          "[ai][blocked-movement]") {
  FakeAdjacentWorld world;
  world.edges = {0, 0};
  world.gaps = {0U, 0U};
  SECTION("first differs from the axis") {
    CHECK(ai::FindAdjacentMovementDestination(world, kMover, 3, 1,
                                              kNearMinusX) == 0U);
    REQUIRE(world.edge_calls.size() == 2);
    CHECK(world.edge_calls[0].direction == 2);
    CHECK(world.edge_calls[1].direction == 3);
  }
  SECTION("first is the axis") {
    CHECK(ai::FindAdjacentMovementDestination(world, kMover, 2, 0x10001,
                                              kNearMinusX) == 0U);
    REQUIRE(world.edge_calls.size() == 2);
    CHECK(world.edge_calls[0].direction == 2);
    CHECK(world.edge_calls[1].direction == 1);
  }
}

TEST_CASE("A second-direction neighbour keeps the failed landing's status",
          "[ai][blocked-movement]") {
  FakeAdjacentWorld world;
  world.edges = {0, 1};
  world.gaps = {0x00143309U};
  world.occupancy = {1};
  // Direction 3 steps to (9, 11); the landing's status and tag stay.
  CHECK(ai::FindAdjacentMovementDestination(world, kMover, 3, 1, kNearMinusX) ==
        Tile(9, 11));
  CHECK(world.edge_calls[1].to == Tile(9, 11, 0x3300));
}

TEST_CASE("Disabled movers have no adjacent destination",
          "[ai][blocked-movement]") {
  FakeAdjacentWorld world;
  const ai::AdjacentMover disabled{.movement_disabled = true,
                                   .tile = kMover.tile};
  CHECK(ai::FindAdjacentMovementDestination(world, disabled, 3, 3,
                                            kTowardPlusZ) == 0U);
  CHECK(world.edge_calls.empty());
}

TEST_CASE("Adjacent destinations refuse a direction outside 0..7",
          "[ai][blocked-movement]") {
  FakeAdjacentWorld world;
  SECTION("the one direction") {
    CHECK_FALSE(ai::FindAdjacentMovementDestination(world, kMover, 3, 3, 8)
                    .has_value());
    CHECK(world.edge_calls.empty());
  }
  SECTION("the second direction") {
    world.edges = {0};
    world.gaps = {0U};
    CHECK_FALSE(
        ai::FindAdjacentMovementDestination(world, kMover, 9, 1, kNearMinusX)
            .has_value());
    CHECK(world.edge_calls.size() == 1);
  }
  SECTION("a refusing world") {
    world.edges = {std::nullopt};
    CHECK_FALSE(
        ai::FindAdjacentMovementDestination(world, kMover, 3, 3, kTowardPlusZ)
            .has_value());
  }
}

TEST_CASE("Movement history stalls when full and repeating",
          "[ai][blocked-movement]") {
  ai::MovementHistory history{.x_words = {0x01020304, 0x01020304},
                              .z_words = {0x05060708, 0x05060708},
                              .mask = 0xFF};
  CHECK(ai::HasRepeatingMovementHistory(history));
  CHECK(ai::IsMovementHistoryStalled(history));
  SECTION("a missing sample") {
    history.mask = 0x7F;
    CHECK(ai::HasRepeatingMovementHistory(history));
    CHECK_FALSE(ai::IsMovementHistoryStalled(history));
  }
  SECTION("x differs") {
    history.x_words[1] ^= 1U;
    CHECK_FALSE(ai::HasRepeatingMovementHistory(history));
    CHECK_FALSE(ai::IsMovementHistoryStalled(history));
  }
  SECTION("z differs") {
    history.z_words[0] ^= 0x80000000U;
    CHECK_FALSE(ai::IsMovementHistoryStalled(history));
  }
}

TEST_CASE("The live movement history reads the perception view",
          "[ai][blocked-movement][blocked-movement-live]") {
  Battle_Ai_PerceptionUpdateView view{};
  view.historyX.words[0] = view.historyX.words[1] = -3;
  view.historyZ.words[0] = view.historyZ.words[1] = 7;
  view.historyMask = 0xFF;
  CHECK(VS::Battle::Ai::IsLiveMovementHistoryStalled(&view) == 1);
  view.historyZ.samples[7] = 1;
  CHECK(VS::Battle::Ai::IsLiveMovementHistoryStalled(&view) == 0);
}

TEST_CASE("Movement saves the frame's random state", "[ai][blocked-movement]") {
  ai::MovementRandomStates states{.frame = 0x89ABCDEF, .saved = 5};
  ai::SaveMovementRandomState(states);
  CHECK(states.frame == 0x89ABCDEFU);
  CHECK(states.saved == static_cast<std::int32_t>(0x89ABCDEFU));
}

TEST_CASE("The live random state save copies the AI frame's state",
          "[ai][blocked-movement][blocked-movement-live]") {
  auto& frame = *Battle_Ai_FrameState();
  struct Restore {
    u_int random;
    int saved;
    ~Restore() {
      Battle_Ai_FrameState()->randomState = random;
      Battle_Ai_savedMovementRandomState = saved;
    }
  } restore{frame.randomState, Battle_Ai_savedMovementRandomState};
  frame.randomState = 0xFEDCBA98U;
  VS::Battle::Ai::SaveLiveMovementRandomState();
  CHECK(Battle_Ai_savedMovementRandomState == static_cast<int>(0xFEDCBA98U));
}

enum class Event { kForget, kDispatch };

struct OccupancyCall {
  std::uint32_t tile = 0;
  std::uint8_t movement_target = 0;
  friend bool operator==(const OccupancyCall&, const OccupancyCall&) = default;
};

struct FakeBlockedWorld final : ai::BlockedMovementWorld {
  bool orbit = false;
  int random = 1;
  std::deque<std::optional<int>> occupancy;
  std::optional<int> otherwise = 0;
  std::vector<int> ranges;
  std::vector<OccupancyCall> queried;
  std::vector<Event> events;
  std::vector<std::uint32_t> dispatched_destinations;

  [[nodiscard]] ai::NavigationDirectionCodes DirectionCodes() const override {
    return kCodes;
  }
  [[nodiscard]] bool HasArenaOrbit() const override { return orbit; }
  [[nodiscard]] int NextRandomMasked(int range) override {
    ranges.push_back(range);
    return random;
  }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      ai::BlockedMover& mover, std::uint32_t tile) override {
    queried.push_back({tile, mover.movement_target});
    if (occupancy.empty()) return otherwise;
    const auto answer = occupancy.front();
    occupancy.pop_front();
    return answer;
  }
  void ForgetActionTarget(ai::BlockedMover& mover) override {
    events.push_back(Event::kForget);
    mover.previous_tile = 0xFFFFFFFF;
  }
  void DispatchLocomotion(ai::BlockedMover& mover) override {
    events.push_back(Event::kDispatch);
    dispatched_destinations.push_back(mover.destination);
  }
};

constexpr std::uint8_t kDetourTarget = 3;
constexpr std::uint8_t kNoTarget = 16;

// Stuck on tile (10, 10) at its centre, walking to tile (14, 10).
ai::BlockedMover StuckMover() {
  return {.is_stuck = 0,
          .holds_stall_tile = 7,
          .tile = Tile(10, 10, kTag),
          .previous_tile = Tile(10, 10),
          .x = Centre(10),
          .z = Centre(10),
          .detour_step = 0,
          .detour_target = kDetourTarget,
          .detour_directions = {},
          .movement_target = kNoTarget,
          .destination = Tile(14, 10, kTag)};
}

TEST_CASE("Blocked movement waits out the arena orbit",
          "[ai][blocked-movement]") {
  FakeBlockedWorld world;
  world.orbit = true;
  auto mover = StuckMover();
  mover.detour_step = 3;
  const auto before = mover;
  CHECK(ai::TryRecoverBlockedMovement(world, mover) == false);
  CHECK(mover == before);
  CHECK(world.queried.empty());
}

TEST_CASE("Movers that changed tile clear their detour step",
          "[ai][blocked-movement]") {
  FakeBlockedWorld world;
  auto mover = StuckMover();
  mover.previous_tile = Tile(11, 10);
  mover.detour_step = 3;
  auto expected = mover;
  expected.detour_step = 0;
  CHECK(ai::TryRecoverBlockedMovement(world, mover) == false);
  CHECK(mover == expected);
}

TEST_CASE("Stuck movers start at the first free detour direction",
          "[ai][blocked-movement]") {
  // The destination lies along -x from the actor, angle 0xC00: lateral
  // directions 6 and 0, with 6 the nearest axis direction.
  REQUIRE((VS::Core::Math::Arctangent2(-512, 0) & 0xFFF) == 0xC00);
  FakeBlockedWorld world;
  // Tile (11, 10) is blocked and tile (10, 9) free; then the destination.
  world.occupancy = {0, 1, 1};
  auto mover = StuckMover();
  CHECK(ai::TryRecoverBlockedMovement(world, mover) == true);
  CHECK(world.ranges == std::vector<int>{2});
  CHECK(mover.is_stuck == 1);
  CHECK(mover.detour_directions == ai::DetourDirections{6, 0, 2, 4});
  CHECK(mover.detour_step == 2);
  CHECK(mover.destination == Tile(10, 9, kTag));
  CHECK(mover.movement_target == kDetourTarget);
}

TEST_CASE("A detour checks each direction and then dispatches",
          "[ai][blocked-movement]") {
  FakeBlockedWorld world;
  world.occupancy = {0, 1, 1};
  auto mover = StuckMover();
  CHECK(ai::TryRecoverBlockedMovement(world, mover) == true);
  CHECK(world.queried ==
        std::vector<OccupancyCall>{{Tile(11, 10), kNoTarget},
                                   {Tile(10, 9), kNoTarget},
                                   {Tile(10, 9, kTag), kDetourTarget}});
  CHECK(world.events == std::vector<Event>{Event::kDispatch});
  CHECK(world.dispatched_destinations ==
        std::vector<std::uint32_t>{Tile(10, 9, kTag)});
}

TEST_CASE("A zero random draw swaps the opposite detour directions",
          "[ai][blocked-movement]") {
  FakeBlockedWorld world;
  world.random = 0;
  auto mover = StuckMover();
  CHECK(ai::TryRecoverBlockedMovement(world, mover) == true);
  CHECK(mover.detour_directions == ai::DetourDirections{6, 0, 4, 2});
}

TEST_CASE("The axis-nearest lateral direction goes first",
          "[ai][blocked-movement]") {
  // The angle lies in octant 7: lateral directions 7 and 1. An odd direction
  // is never the nearest axis direction, so 1 goes first.
  constexpr unsigned kOctantShift = 9;
  constexpr unsigned kOctantMask = 7;
  REQUIRE(((VS::Core::Math::Arctangent2(-512, 512) >> kOctantShift) &
           kOctantMask) == kOctantMask);
  FakeBlockedWorld world;
  auto mover = StuckMover();
  mover.destination = Tile(14, 6, kTag);
  CHECK(ai::TryRecoverBlockedMovement(world, mover) == true);
  CHECK(mover.detour_directions == ai::DetourDirections{1, 7, 3, 5});
}

TEST_CASE("Stuck movers without a free neighbour start at step 1",
          "[ai][blocked-movement]") {
  FakeBlockedWorld world;
  auto mover = StuckMover();
  CHECK(ai::TryRecoverBlockedMovement(world, mover) == true);
  CHECK(world.queried.size() == 5);
  // Step 1 walks direction 6 to (11, 10), which is blocked.
  CHECK(world.queried.back().tile == Tile(11, 10, kTag));
  CHECK(mover.detour_step == 2);
  CHECK(world.events.empty());
}

TEST_CASE("Later detour steps walk their stored direction",
          "[ai][blocked-movement]") {
  FakeBlockedWorld world;
  auto mover = StuckMover();
  mover.detour_step = 3;
  mover.detour_directions = {6, 0, 2, 4};
  CHECK(ai::TryRecoverBlockedMovement(world, mover) == true);
  CHECK(world.ranges.empty());
  CHECK(mover.destination == Tile(9, 10, kTag));
  CHECK(mover.movement_target == kDetourTarget);
  CHECK(mover.detour_step == 4);
  CHECK(world.events.empty());
}

TEST_CASE("Exhausted detours forget the action target",
          "[ai][blocked-movement]") {
  FakeBlockedWorld world;
  auto mover = StuckMover();
  mover.detour_step = 5;
  auto expected = mover;
  expected.is_stuck = 1;
  expected.holds_stall_tile = 0;
  expected.detour_step = 0;
  expected.previous_tile = 0xFFFFFFFF;
  CHECK(ai::TryRecoverBlockedMovement(world, mover) == false);
  CHECK(mover == expected);
  CHECK(world.events == std::vector<Event>{Event::kForget});
}

TEST_CASE("Blocked movement refuses a detour direction outside 0..7",
          "[ai][blocked-movement]") {
  FakeBlockedWorld world;
  auto mover = StuckMover();
  SECTION("a stored direction") {
    mover.detour_step = 1;
    mover.detour_directions = {8, 0, 0, 0};
    CHECK_FALSE(ai::TryRecoverBlockedMovement(world, mover).has_value());
    CHECK(world.queried.empty());
  }
  SECTION("a refusing world") {
    world.occupancy = {std::nullopt};
    CHECK_FALSE(ai::TryRecoverBlockedMovement(world, mover).has_value());
  }
}

TEST_CASE("Blocked movement keeps its original module entries",
          "[ai][blocked-movement][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [symbols](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800D8400U) ==
        reinterpret_cast<void*>(&ai::FindLiveAdjacentMovementDestination));
  CHECK(resolve(0x800D9538U) ==
        reinterpret_cast<void*>(&ai::SaveLiveMovementRandomState));
  CHECK(resolve(0x800DB0BCU) ==
        reinterpret_cast<void*>(&ai::TryRecoverLiveBlockedMovement));
}
}  // namespace
