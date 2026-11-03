#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Battle/Ai/MovementBehavior.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/AiState.h"
}

namespace {
namespace ai = VS::Battle::Ai;
using Position = VS::Core::Math::Position3;

constexpr std::uint32_t Tile(unsigned x, unsigned layer, unsigned z) {
  return x | (layer << 8) | (z << 16);
}

constexpr std::uint16_t Waypoint(unsigned x, unsigned z) {
  return static_cast<std::uint16_t>(0x8000u | x | (z << 5));
}

ai::ActorRoster Roster(std::initializer_list<std::uint8_t> ids) {
  ai::ActorRoster roster;
  for (const std::uint8_t id : ids) roster.ids.at(roster.count++) = id;
  return roster;
}

struct HostCall {
  std::string name;
  int argument = 0;
  std::uint8_t current_mode = 0;
  std::uint32_t destination = 0;
};

class FakeMovementHost final : public ai::MovementHost {
 public:
  FakeMovementHost() { frame_actors.fill(true); }

  [[nodiscard]] std::span<const std::uint16_t> PatrolRoutes() const override {
    return routes;
  }
  [[nodiscard]] bool HasFrameActor(int actor_id) const override {
    return frame_actors.at(static_cast<std::size_t>(actor_id));
  }
  [[nodiscard]] std::optional<ai::TargetPlacement> Placement(
      int actor_id) const override {
    return placements.at(static_cast<std::size_t>(actor_id));
  }
  [[nodiscard]] std::optional<ai::RouteGate> PlatformRouteGate(
      int group) const override {
    gate_groups.push_back(group);
    return gates.at(static_cast<std::size_t>(group));
  }
  [[nodiscard]] std::optional<ai::ActorRoster> BattleActors() const override {
    return roster;
  }
  [[nodiscard]] std::int32_t TileHeight(std::uint32_t tile) const override {
    height_tiles.push_back(tile);
    return height;
  }
  [[nodiscard]] bool RunsPatrolCombatBehavior() const override {
    return patrol_combat;
  }
  [[nodiscard]] std::uint8_t ActionMovementMask() const override {
    return movement_mask;
  }
  [[nodiscard]] std::optional<std::uint32_t> BestCandidateFlags()
      const override {
    ++best_reads;
    return best_flags;
  }
  int UpdateTargetApproach(ai::MovementState& state, int option) override {
    Record("approach", option, state);
    return approach_result;
  }
  void UpdateWander(ai::MovementState& state, std::uint32_t tile,
                    std::uint32_t range) override {
    Record("wander", 0, state);
    wander_tile = tile;
    wander_range = range;
  }
  void UpdateWaypointPatrol(ai::MovementState& state) override {
    Record("patrol", 0, state);
  }
  int UpdateTargetFacing(ai::MovementState& state, int target) override {
    Record("facing", target, state);
    return facing_result;
  }
  void BeginMovementTowardPoint(ai::MovementState& state, int kind) override {
    Record("toward", kind, state);
  }
  void IssueCommand(ai::MovementState& state, int command) override {
    Record("command", command, state);
  }
  void DispatchLocomotion(ai::MovementState& state) override {
    Record("locomotion", 0, state);
  }
  void FindNearestActiveWaypoint(ai::MovementState& state) override {
    Record("waypoint", 0, state);
  }

  std::vector<std::uint16_t> routes;
  std::array<bool, ai::kMovementActorSlots> frame_actors{};
  std::array<std::optional<ai::TargetPlacement>, ai::kMovementActorSlots>
      placements{};
  std::array<std::optional<ai::RouteGate>, ai::kPlatformGroupCount> gates{};
  std::optional<ai::ActorRoster> roster = ai::ActorRoster{};
  std::int32_t height = 0;
  bool patrol_combat = false;
  std::uint8_t movement_mask = 0;
  std::optional<std::uint32_t> best_flags;
  int approach_result = 0;
  int facing_result = 0;
  std::uint32_t wander_tile = 0;
  std::uint32_t wander_range = 0;
  mutable std::vector<int> gate_groups;
  mutable std::vector<std::uint32_t> height_tiles;
  mutable int best_reads = 0;
  std::vector<HostCall> calls;

 private:
  void Record(const char* name, int argument, const ai::MovementState& state) {
    calls.push_back({name, argument, state.current_mode, state.destination});
  }
};

class FakeRelationshipHost final : public ai::RelationshipChangeHost {
 public:
  [[nodiscard]] std::optional<ai::ActorRoster> BattleActors() const override {
    return roster;
  }
  [[nodiscard]] bool HasAiState(int actor_id) const override {
    return states.at(static_cast<std::size_t>(actor_id));
  }
  void ApplyActionReaction(int actor_id) override {
    calls.emplace_back("react", actor_id);
  }
  void PropagateRelationships(int actor_id) override {
    calls.emplace_back("propagate", actor_id);
  }

  std::optional<ai::ActorRoster> roster = ai::ActorRoster{};
  std::array<bool, ai::kMovementActorSlots> states{};
  std::vector<std::pair<std::string, int>> calls;
};

// A state whose every field holds a distinctive value.
ai::MovementState SampleState() {
  ai::MovementState state;
  state.actor_id = 1;
  state.tile = Tile(10, 0, 10);
  state.position = Position{.x = 100, .y = -20, .z = 300};
  state.movement_enabled = true;
  state.movement_flags = 0;
  state.requested_mode = 0;
  state.current_mode = 0;
  state.mode_counters.fill(0x5A);
  state.navigation_flags = 0xFFFFFFFFu;
  state.pursuit = {1, 2, 3, 4, 5, 6, 7, 8};
  state.approach_range = 0x1234;
  state.cached_destination = 0x0BADF00Du;
  state.target_half_tile = 0xAABBCCDDu;
  state.wander_phase = 0x1111;
  state.wander_cooldown = 0x2222;
  state.wander_center_radius = 3;
  state.wander_center = Tile(4, 1, 5);
  state.patrol = {.index = 7, .row = 0, .cooldown = 9};
  state.previous_facing_tile = 0x01020304u;
  state.facing_pending = 1;
  state.flee_tile = Tile(6, 0, 7);
  state.flee_timer = 0;
  state.arrive_when_near = 0;
  state.destination = 0x44332211u;
  state.move_x = -1;
  state.move_z = -2;
  state.plan_mode = 0x77;
  return state;
}

bool SamePursuit(const ai::PursuitProgress& a, const ai::PursuitProgress& b) {
  return a.opaque_e4 == b.opaque_e4 && a.target_id == b.target_id &&
         a.timeout == b.timeout && a.opaque_e7 == b.opaque_e7 &&
         a.opaque_e8 == b.opaque_e8 && a.range_mode == b.range_mode &&
         a.previous_option == b.previous_option &&
         a.previous_target_id == b.previous_target_id;
}

void CheckSameModes(const ai::MovementState& a, const ai::MovementState& b) {
  CHECK(a.requested_mode == b.requested_mode);
  CHECK(a.current_mode == b.current_mode);
  CHECK(a.mode_counters == b.mode_counters);
  CHECK(a.navigation_flags == b.navigation_flags);
  CHECK(SamePursuit(a.pursuit, b.pursuit));
  CHECK(a.approach_range == b.approach_range);
  CHECK(a.cached_destination == b.cached_destination);
}

void CheckSameProgress(const ai::MovementState& a, const ai::MovementState& b) {
  CHECK(a.target_half_tile == b.target_half_tile);
  CHECK(a.wander_phase == b.wander_phase);
  CHECK(a.wander_cooldown == b.wander_cooldown);
  CHECK(a.patrol.index == b.patrol.index);
  CHECK(a.patrol.cooldown == b.patrol.cooldown);
  CHECK(a.previous_facing_tile == b.previous_facing_tile);
  CHECK(a.facing_pending == b.facing_pending);
}

void CheckSameTarget(const ai::MovementState& a, const ai::MovementState& b) {
  CHECK(a.flee_tile == b.flee_tile);
  CHECK(a.flee_timer == b.flee_timer);
  CHECK(a.arrive_when_near == b.arrive_when_near);
  CHECK(a.destination == b.destination);
  CHECK(a.move_x == b.move_x);
  CHECK(a.move_z == b.move_z);
  CHECK(a.plan_mode == b.plan_mode);
}

void CheckSameMovement(const ai::MovementState& a, const ai::MovementState& b) {
  CheckSameModes(a, b);
  CheckSameProgress(a, b);
  CheckSameTarget(a, b);
}

// SampleState() after entering `mode` from idle with movement enabled.
ai::MovementState EnteredState(std::uint8_t mode) {
  ai::MovementState state = SampleState();
  state.requested_mode = mode;
  state.current_mode = mode;
  state.mode_counters.at(std::size_t{mode} * 2) = 0;
  state.navigation_flags = 0xFFFFF3DEu;
  return state;
}

void CheckOnlyCall(const FakeMovementHost& host, const HostCall& expected) {
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].name == expected.name);
  CHECK(host.calls[0].argument == expected.argument);
  CHECK(host.calls[0].current_mode == expected.current_mode);
  CHECK(host.calls[0].destination == expected.destination);
}

ai::TargetPlacement GroundPlacement(std::int16_t x, std::int16_t z) {
  return ai::TargetPlacement{.x = x,
                             .z = z,
                             .planned_x = 1280,
                             .planned_z = 1344,
                             .current_tile = Tile(3, 0, 4),
                             .planned_tile = Tile(5, 0, 6)};
}

// The value of an optional a test has established must be present.
template <class Optional>
auto& Engaged(Optional& optional) {
  if (!optional.has_value()) throw std::runtime_error("expected a value");
  return *optional;
}
}  // namespace

TEST_CASE("Movement cooldown rewrites only the original navigation bits",
          "[ai][movement]") {
  const std::uint32_t flags =
      GENERATE(0u, 0xFE1u, 0x3C0u, 0xFFFFFFFFu, 0x12345678u);
  const std::uint32_t kept = flags & 0xFFFFF01Eu;
  CHECK(ai::RefreshAiMovementCooldown(flags, false) == kept);
  CHECK(ai::RefreshAiMovementCooldown(flags, true) == (kept | 0x3C0u));
  CHECK(ai::RefreshAiMovementCooldown(0xFFFFFFFFu, false) == 0xFFFFF01Eu);
  CHECK(ai::RefreshAiMovementCooldown(0xFFFFFFFFu, true) == 0xFFFFF3DEu);
}

TEST_CASE("Movement behaviour refuses modes outside the behaviour table",
          "[ai][movement]") {
  const int mode = GENERATE(-1, 7, 255, std::numeric_limits<int>::min());
  ai::MovementState state = SampleState();
  const ai::MovementState before = state;
  CHECK_FALSE(ai::SetMovementBehavior(state, {}, mode).has_value());
  CheckSameMovement(state, before);
}

TEST_CASE("Repeating the current movement behaviour only records the request",
          "[ai][movement]") {
  const int mode = GENERATE(range(0, 7));
  ai::MovementState state = SampleState();
  state.current_mode = static_cast<std::uint8_t>(mode);
  state.requested_mode = 0xEE;
  // The patrol row is not validated when patrol is already running.
  state.patrol.row = 200;
  ai::MovementState expected = state;
  expected.requested_mode = static_cast<std::uint8_t>(mode);
  const std::optional<int> result = ai::SetMovementBehavior(state, {}, mode);
  REQUIRE(result.has_value());
  CHECK(*result == mode);
  CheckSameMovement(state, expected);
}

TEST_CASE("Entering a movement behaviour resets its counter and flags",
          "[ai][movement]") {
  const auto [mode, expected] = GENERATE(table<int, int>(
      {{0, -1}, {1, 3}, {2, -1}, {3, 6}, {4, -1}, {5, 1}, {6, -1}}));
  const bool enabled = GENERATE(true, false);
  ai::MovementState state = SampleState();
  state.current_mode = static_cast<std::uint8_t>((mode + 1) % 7);
  state.movement_enabled = enabled;
  const std::vector<std::uint16_t> routes(ai::kPatrolWaypointsPerRow,
                                          Waypoint(10, 10));
  const std::optional<int> result =
      ai::SetMovementBehavior(state, routes, mode);
  REQUIRE(result.has_value());
  CHECK(*result == expected);
  CHECK(state.requested_mode == mode);
  CHECK(state.current_mode == mode);
  for (std::size_t index = 0; index < state.mode_counters.size(); ++index) {
    const auto reset = static_cast<std::size_t>(mode) * 2;
    CHECK(state.mode_counters.at(index) == (index == reset ? 0 : 0x5A));
  }
  CHECK(state.navigation_flags == (enabled ? 0xFFFFF3DEu : 0xFFFFF01Eu));
}

TEST_CASE("Pursuit entry clears six approach bytes and keeps the last target",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  const ai::MovementState before = state;
  REQUIRE(ai::SetMovementBehavior(state, {}, 1) == 3);
  CHECK(SamePursuit(state.pursuit, {0, 0, 0, 0, 0, 0, 7, 8}));
  CHECK(state.approach_range == before.approach_range);
  CHECK(state.wander_phase == before.wander_phase);
  CHECK(state.previous_facing_tile == before.previous_facing_tile);
}

TEST_CASE("Wander entry resets only its own progress", "[ai][movement]") {
  ai::MovementState state = SampleState();
  ai::MovementState expected = EnteredState(3);
  expected.wander_phase = 0;
  expected.wander_cooldown = 0;
  REQUIRE(ai::SetMovementBehavior(state, {}, 3) == 6);
  CheckSameMovement(state, expected);
}

TEST_CASE("Facing entry resets only its own progress", "[ai][movement]") {
  ai::MovementState state = SampleState();
  ai::MovementState expected = EnteredState(6);
  expected.previous_facing_tile = 0xFFFFFFFFu;
  expected.facing_pending = 0;
  REQUIRE(ai::SetMovementBehavior(state, {}, 6) == -1);
  CheckSameMovement(state, expected);
}

TEST_CASE("Patrol entry picks the first nearest enabled waypoint",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  state.patrol.row = 1;
  std::vector<std::uint16_t> routes(ai::kPatrolWaypointsPerRow * 2,
                                    Waypoint(10, 10));
  // Row 1: a disabled exact match, then distances 3, 2, 2 and 3.
  const std::array<std::uint16_t, ai::kPatrolWaypointsPerRow> row{
      static_cast<std::uint16_t>(10u | (10u << 5)),
      Waypoint(13, 10),
      Waypoint(10, 12),
      Waypoint(8, 10),
      0x7FFF,
      Waypoint(10, 13),
      0,
      0};
  std::copy(row.begin(), row.end(), routes.begin() + 8);
  REQUIRE(ai::SetMovementBehavior(state, routes, 5) == 1);
  CHECK(state.patrol.index == 2);
  CHECK(state.patrol.cooldown == 0);

  // Bits 10-14 are not part of the waypoint's tile.
  state = SampleState();
  routes.assign(ai::kPatrolWaypointsPerRow, 0);
  routes.at(0) = Waypoint(10, 12);
  routes.at(1) = static_cast<std::uint16_t>(Waypoint(10, 11) | 0x7C00u);
  REQUIRE(ai::SetMovementBehavior(state, routes, 5) == 1);
  CHECK(state.patrol.index == 1);
}

TEST_CASE("Core patrol entry without an enabled waypoint keeps its index",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  const std::vector<std::uint16_t> routes(ai::kPatrolWaypointsPerRow, 0x7FFF);
  REQUIRE(ai::SetMovementBehavior(state, routes, 5) == 1);
  CHECK(state.patrol.index == 7);
  CHECK(state.patrol.cooldown == 0);
  CHECK(state.current_mode == 5);
}

TEST_CASE("Patrol entry refuses a row outside the room's routes",
          "[ai][movement]") {
  const auto [row, rows] =
      GENERATE(table<std::uint8_t, std::size_t>({{2, 2}, {0, 0}, {255, 2}}));
  ai::MovementState state = SampleState();
  state.patrol.row = row;
  const ai::MovementState before = state;
  const std::vector<std::uint16_t> routes(rows * ai::kPatrolWaypointsPerRow,
                                          Waypoint(1, 1));
  CHECK_FALSE(ai::SetMovementBehavior(state, routes, 5).has_value());
  CheckSameMovement(state, before);

  FakeMovementHost host;
  host.routes = routes;
  CHECK_FALSE(ai::PatrolWaypoints(state, host));
  CHECK(host.calls.empty());
  CheckSameMovement(state, before);
}

TEST_CASE("Waypoint patrol enters patrol before updating it",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  host.routes.assign(ai::kPatrolWaypointsPerRow, Waypoint(12, 10));
  REQUIRE(ai::PatrolWaypoints(state, host));
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].name == "patrol");
  CHECK(host.calls[0].current_mode == 5);
  CHECK(state.patrol.index == 0);
}

TEST_CASE("Target tile refresh refuses targets it cannot place",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  state.pursuit.target_id = GENERATE(std::uint8_t{16}, std::uint8_t{200});
  CHECK_FALSE(ai::RefreshAiTargetTile(state, host));
  CHECK(state.target_half_tile == 0xAABBCCDDu);

  state.pursuit.target_id = 4;
  host.frame_actors.at(4) = false;
  CHECK(ai::RefreshAiTargetTile(state, host));
  CHECK(state.target_half_tile == 0xAABBCCDDu);

  host.frame_actors.at(4) = true;
  CHECK_FALSE(ai::RefreshAiTargetTile(state, host));
  CHECK(state.target_half_tile == 0xAABBCCDDu);
}

TEST_CASE("Target tile refresh halves a ground target's position",
          "[ai][movement]") {
  const unsigned layer = GENERATE(0u, 1u);
  ai::MovementState state = SampleState();
  state.pursuit.target_id = 4;
  FakeMovementHost host;
  ai::TargetPlacement placement = GroundPlacement(640, -64);
  placement.current_tile = Tile(3, layer, 4);
  host.placements.at(4) = placement;
  REQUIRE(ai::RefreshAiTargetTile(state, host));
  // X 640 >> 6 is 10; Z -64 shifts arithmetically to 0xFF. Bytes 1 and 3
  // keep their values.
  CHECK(state.target_half_tile == 0xAAFFCC0Au);
  CHECK(host.gate_groups.empty());
}

TEST_CASE("Target tile refresh doubles a platform's route gate",
          "[ai][movement]") {
  const unsigned layer = GENERATE(2u, 3u, 4u, 5u);
  ai::MovementState state = SampleState();
  state.pursuit.target_id = 4;
  FakeMovementHost host;
  ai::TargetPlacement placement = GroundPlacement(640, 640);
  placement.current_tile = Tile(3, layer, 4);
  host.placements.at(4) = placement;
  host.gates.at(layer - 2) = ai::RouteGate{.x = 7, .z = 200};
  REQUIRE(ai::RefreshAiTargetTile(state, host));
  // The doubled Z (400) keeps only its low byte, as the byte store does.
  CHECK(state.target_half_tile == 0xAA90CC0Eu);
  CHECK(host.gate_groups == std::vector<int>{static_cast<int>(layer) - 2});

  host.gates.at(layer - 2).reset();
  state.target_half_tile = 0;
  CHECK_FALSE(ai::RefreshAiTargetTile(state, host));
  CHECK(state.target_half_tile == 0);
}

TEST_CASE("Target tile refresh refuses unknown platform layers",
          "[ai][movement]") {
  const auto [planned, layer] = GENERATE(
      table<bool, unsigned>({{false, 6}, {false, 255}, {true, 1}, {true, 6}}));
  ai::MovementState state = SampleState();
  state.pursuit.target_id = 4;
  FakeMovementHost host;
  host.gates.fill(ai::RouteGate{.x = 1, .z = 1});
  ai::TargetPlacement placement = GroundPlacement(640, 640);
  placement.planned_move = planned;
  (planned ? placement.planned_tile : placement.current_tile) =
      Tile(3, layer, 4);
  host.placements.at(4) = placement;
  CHECK_FALSE(ai::RefreshAiTargetTile(state, host));
  CHECK(state.target_half_tile == 0xAABBCCDDu);
  CHECK(host.gate_groups.empty());
}

TEST_CASE("Target tile refresh follows a target's planned step",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  state.pursuit.target_id = 4;
  FakeMovementHost host;
  ai::TargetPlacement placement = GroundPlacement(640, 640);
  placement.planned_move = true;
  // The current tile's platform layer is ignored while a step is planned.
  placement.current_tile = Tile(3, 2, 4);
  host.placements.at(4) = placement;
  REQUIRE(ai::RefreshAiTargetTile(state, host));
  CHECK(state.target_half_tile == 0xAA15CC14u);
  CHECK(host.gate_groups.empty());

  placement.planned_tile = Tile(5, 3, 6);
  host.placements.at(4) = placement;
  host.gates.at(1) = ai::RouteGate{.x = 2, .z = 3};
  REQUIRE(ai::RefreshAiTargetTile(state, host));
  CHECK(state.target_half_tile == 0xAA06CC04u);
  CHECK(host.gate_groups == std::vector<int>{1});
}

TEST_CASE("Tracking refresh runs only while pursuit is requested",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  state.pursuit.target_id = 200;
  FakeMovementHost host;
  state.requested_mode = GENERATE(std::uint8_t{0}, std::uint8_t{2},
                                  std::uint8_t{6}, std::uint8_t{255});
  CHECK(ai::RefreshAiTargetTileWhenTracking(state, host));
  CHECK(state.target_half_tile == 0xAABBCCDDu);

  state.requested_mode = 1;
  CHECK_FALSE(ai::RefreshAiTargetTileWhenTracking(state, host));
  state.pursuit.target_id = 4;
  host.placements.at(4) = GroundPlacement(640, 640);
  CHECK(ai::RefreshAiTargetTileWhenTracking(state, host));
  CHECK(state.target_half_tile == 0xAA0ACC0Au);
}

TEST_CASE("Approach refuses targets outside the slots or without a model",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  const ai::MovementState before = state;
  FakeMovementHost host;
  const int target = GENERATE(-1, 16, 255);
  CHECK_FALSE(ai::ApproachActor(state, host, {target, 5, 0}).has_value());
  CheckSameMovement(state, before);

  CHECK_FALSE(ai::ApproachActor(state, host, {4, 5, 0}).has_value());
  CheckSameMovement(state, before);
  CHECK(host.calls.empty());
}

TEST_CASE("Approach without a frame actor pursues every target but Ashley",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  const ai::MovementState before = state;
  FakeMovementHost host;
  host.frame_actors.fill(false);
  CHECK(ai::ApproachActor(state, host, {0, 5, 1}) == 0);
  CheckSameMovement(state, before);

  CHECK(ai::ApproachActor(state, host, {4, 5, 1}) == 3);
  CHECK(state.current_mode == 1);
  CHECK(SamePursuit(state.pursuit, {0, 0, 0, 0, 0, 0, 7, 8}));
  CHECK(state.approach_range == before.approach_range);
  CHECK(state.destination == before.destination);
  CHECK(host.calls.empty());
}

TEST_CASE("Approach walks to a ground target's tile", "[ai][movement]") {
  ai::MovementState state = SampleState();
  state.pursuit.target_id = 4;
  ai::MovementState expected = state;
  expected.destination = Tile(3, 0, 4);
  expected.approach_range = 9;
  FakeMovementHost host;
  host.placements.at(4) = GroundPlacement(640, 640);
  host.approach_result = 0;
  CHECK(ai::ApproachActor(state, host, {4, 9, 2}) == 0);
  CheckSameMovement(state, expected);
  CheckOnlyCall(host, {"approach", 2, 0, Tile(3, 0, 4)});
}

TEST_CASE("A moving approach enters pursuit, then keeps its progress",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  state.pursuit.target_id = 4;
  FakeMovementHost host;
  host.placements.at(4) = GroundPlacement(640, 640);
  host.approach_result = 1;
  // Pursuit entry clears the target ID the approach just stored, as the
  // original's does.
  CHECK(ai::ApproachActor(state, host, {4, 9, 2}) == 3);
  CHECK(state.current_mode == 1);
  CHECK(SamePursuit(state.pursuit, {0, 0, 0, 0, 0, 0, 7, 8}));

  // Already pursuing the same target, the approach keeps its progress.
  state.pursuit.target_id = 4;
  state.pursuit.timeout = 0x33;
  CHECK(ai::ApproachActor(state, host, {4, 9, 2}) == 1);
  CHECK(SamePursuit(state.pursuit, {0, 4, 0x33, 0, 0, 0, 7, 8}));
}

TEST_CASE("Approach clears all pursuit progress when the target changes",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  state.current_mode = 1;
  FakeMovementHost host;
  host.placements.at(9) = GroundPlacement(640, 640);
  host.approach_result = 1;
  CHECK(ai::ApproachActor(state, host, {9, -4, 0}) == 1);
  CHECK(SamePursuit(state.pursuit, {0, 9, 0, 0, 0, 0, 0, 0}));
  CHECK(state.approach_range == -4);
}

TEST_CASE("Approach steers to a planned step or its route gate",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  ai::TargetPlacement placement = GroundPlacement(640, 640);
  placement.planned_move = true;
  placement.support_layer = 3;
  host.placements.at(4) = placement;
  REQUIRE(ai::ApproachActor(state, host, {4, 5, 0}) == 0);
  CHECK(state.destination == Tile(5, 0, 6));
  CHECK(host.gate_groups.empty());

  placement.planned_tile = 0xEECC0201u;  // layer 2 is platform group 0
  host.placements.at(4) = placement;
  host.gates.at(0) = ai::RouteGate{.x = 0x21, .z = 0x30};
  REQUIRE(ai::ApproachActor(state, host, {4, 5, 0}) == 0);
  CHECK(state.destination == 0xEE300221u);
  CHECK(state.cached_destination == 0xEE300221u);
  // Both doubled coordinates are OR-combined into the whole word.
  CHECK(state.target_half_tile == ((0x21u << 1) | (0x30u << 1)));
  CHECK(host.gate_groups == std::vector<int>{0});
}

TEST_CASE("A busy approach follows the platform the target stands on",
          "[ai][movement]") {
  const std::uint16_t busy =
      GENERATE(std::uint16_t{0x08}, std::uint16_t{0x10}, std::uint16_t{0x20});
  ai::MovementState state = SampleState();
  state.movement_flags = busy;
  FakeMovementHost host;
  ai::TargetPlacement placement = GroundPlacement(640, 640);
  placement.planned_move = true;
  placement.support_layer = 3;
  host.placements.at(4) = placement;
  host.gates.at(1) = ai::RouteGate{.x = 0x12, .z = 0x34};
  REQUIRE(ai::ApproachActor(state, host, {4, 5, 0}) == 0);
  // The previous destination keeps its layer and top byte.
  CHECK(state.destination == 0x44342212u);
  CHECK(state.cached_destination == 0x44342212u);
  CHECK(host.gate_groups == std::vector<int>{1});
}

TEST_CASE("Approach follows platforms only when told to or busy",
          "[ai][movement]") {
  const auto [follows, flags, gated] =
      GENERATE(table<bool, std::uint16_t, bool>({{false, 0x00, false},
                                                 {false, 0x07, false},
                                                 {false, 0x40, false},
                                                 {true, 0x00, true},
                                                 {true, 0x07, true}}));
  ai::MovementState state = SampleState();
  state.follows_platform_targets = follows;
  state.movement_flags = flags;
  FakeMovementHost host;
  ai::TargetPlacement placement = GroundPlacement(640, 640);
  placement.support_layer = 5;
  host.placements.at(4) = placement;
  host.gates.at(3) = ai::RouteGate{.x = 1, .z = 2};
  REQUIRE(ai::ApproachActor(state, host, {4, 5, 0}) == 0);
  CHECK(state.destination == (gated ? 0x44022201u : Tile(3, 0, 4)));
  CHECK(host.gate_groups.size() == (gated ? 1u : 0u));
}

TEST_CASE("Approach refuses an unknown platform or a missing route gate",
          "[ai][movement]") {
  const auto [planned, layer] = GENERATE(table<bool, unsigned>(
      {{false, 1}, {false, 6}, {false, 15}, {true, 1}, {true, 7}}));
  ai::MovementState state = SampleState();
  state.follows_platform_targets = true;
  const ai::MovementState before = state;
  FakeMovementHost host;
  host.gates.fill(ai::RouteGate{.x = 1, .z = 1});
  ai::TargetPlacement placement = GroundPlacement(640, 640);
  placement.planned_move = planned;
  placement.support_layer = static_cast<std::uint8_t>(planned ? 0 : layer);
  placement.planned_tile = Tile(5, planned ? layer : 0, 6);
  host.placements.at(4) = placement;
  CHECK_FALSE(ai::ApproachActor(state, host, {4, 5, 0}).has_value());
  CheckSameMovement(state, before);

  placement.support_layer = 2;
  placement.planned_move = false;
  host.placements.at(4) = placement;
  host.gates.at(0).reset();
  CHECK_FALSE(ai::ApproachActor(state, host, {4, 5, 0}).has_value());
  CheckSameMovement(state, before);
  CHECK(host.calls.empty());
}

TEST_CASE("Wandering enters the wander mode before choosing a step",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  ai::WanderAroundTile(state, host, Tile(3, 0, 4), -2);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].name == "wander");
  CHECK(host.calls[0].current_mode == 3);
  CHECK(host.wander_tile == Tile(3, 0, 4));
  // The radius is passed on unsigned.
  CHECK(host.wander_range == 0xFFFFFFFEu);
  CHECK(state.wander_phase == 0);

  state = SampleState();
  host.calls.clear();
  ai::WanderAroundSavedCenter(state, host);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.wander_tile == Tile(4, 1, 5));
  CHECK(host.wander_range == 3u);
}

TEST_CASE("Walking to a tile stops within the squared radius",
          "[ai][movement]") {
  // The tile centre is (320, height, 448); the actor is at (3, 4, 12) from it,
  // squared distance 169.
  const auto [radius, arrives] = GENERATE(table<int, bool>({{13, false},
                                                            {14, true},
                                                            {-14, true},
                                                            {-13, false},
                                                            {0, false},
                                                            {65536, false}}));
  ai::MovementState state = SampleState();
  state.position = Position{.x = 317, .y = -104, .z = 436};
  FakeMovementHost host;
  host.height = -100;
  const std::uint32_t tile = Tile(2, 1, 3);
  ai::MoveToTile(state, host, tile, radius);
  CHECK(state.destination == tile);
  CHECK(state.current_mode == 2);
  CHECK(host.height_tiles == std::vector<std::uint32_t>{tile});
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].name == (arrives ? "command" : "locomotion"));
  CHECK(host.calls[0].argument == 0);
  CHECK(host.calls[0].current_mode == 2);
  CHECK(host.calls[0].destination == tile);
}

TEST_CASE("Flight averages visible hostiles pairwise", "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  host.roster = Roster({1, 2, 3, 5});
  state.relations.at(2) = 0xE0;
  state.relations.at(3) = 0x1E0;
  state.relations.at(5) = 0x60;  // not visible as hostile
  host.placements.at(2) = GroundPlacement(1000, 2000);
  host.placements.at(3) = GroundPlacement(3000, 600);
  ai::MovementState expected = EnteredState(4);
  expected.flee_timer = 90;
  expected.flee_tile = 0x000A000Fu;
  expected.destination = 0x000A000Fu;
  expected.arrive_when_near = 1;
  expected.move_x = 15 * 128 + 64;
  expected.move_z = 10 * 128 + 64;
  CHECK(ai::FleeVisibleEnemies(state, host, 7) == -1);
  CheckSameMovement(state, expected);
  CheckOnlyCall(host, {"toward", 7, 0, 0x000A000Fu});
}

TEST_CASE("Flight averages a third hostile with the running average",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  host.roster = Roster({1, 2, 3, 5});
  state.relations.at(2) = 0xE0;
  state.relations.at(3) = 0xE0;
  state.relations.at(5) = 0xE0;
  host.placements.at(2) = GroundPlacement(1000, 2000);
  host.placements.at(3) = GroundPlacement(3000, 600);
  host.placements.at(5) = GroundPlacement(4000, 0);
  REQUIRE(ai::FleeVisibleEnemies(state, host, 7).has_value());
  // X: (2000 + 4000) / 2 = 3000; Z: (1300 + 0) / 2 = 650.
  CHECK(state.flee_tile == Tile(3000u >> 7, 0, 650u >> 7));
}

TEST_CASE("Flight counts the actor itself and keeps wrapped coordinates",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  state.current_mode = 4;
  FakeMovementHost host;
  host.roster = Roster({1});
  state.relations.at(1) = 0xE0;
  host.placements.at(1) = GroundPlacement(-128, 256);
  REQUIRE(ai::FleeVisibleEnemies(state, host, 0) == 4);
  // The negative X sum is shifted logically and stored unmasked.
  CHECK(state.flee_tile == 0x01FFFFFFu);
  CHECK(state.move_x == 255 * 128 + 64);
  CHECK(state.move_z == 255 * 128 + 64);
}

TEST_CASE("Flight remembers hostiles for its timer while fleeing",
          "[ai][movement]") {
  const auto [mode, timer, remaining, result] =
      GENERATE(table<std::uint8_t, std::uint16_t, std::uint16_t, int>(
          {{4, 5, 4, 4}, {4, 1, 0, 0}, {4, 0, 0, 0}, {2, 5, 4, 0}}));
  ai::MovementState state = SampleState();
  state.current_mode = mode;
  state.flee_timer = timer;
  FakeMovementHost host;
  host.roster = Roster({1, 2});
  REQUIRE(ai::FleeVisibleEnemies(state, host, 3) == result);
  CHECK(state.flee_timer == remaining);
  CHECK(state.flee_tile == Tile(6, 0, 7));
  CHECK(host.calls.size() == (result != 0 ? 1u : 0u));
  CHECK(state.destination == (result != 0 ? Tile(6, 0, 7) : 0x44332211u));
}

TEST_CASE("Flight refuses an invalid roster or a hostile without a model",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  const ai::MovementState before = state;
  FakeMovementHost host;
  host.roster.reset();
  CHECK_FALSE(ai::FleeVisibleEnemies(state, host, 0).has_value());
  host.roster = Roster({2});
  state.relations.at(2) = 0xE0;
  CHECK_FALSE(ai::FleeVisibleEnemies(state, host, 0).has_value());
  CHECK(state.flee_timer == before.flee_timer);
  CHECK(state.destination == before.destination);
  CHECK(host.calls.empty());
}

TEST_CASE("Tracking refuses an invalid or unplaced target", "[ai][movement]") {
  ai::MovementState state = SampleState();
  const ai::MovementState before = state;
  FakeMovementHost host;
  const int invalid = GENERATE(-1, 16);
  CHECK_FALSE(ai::TrackActorPosition(state, host, invalid).has_value());
  CHECK_FALSE(ai::TrackActorPosition(state, host, 6).has_value());
  CheckSameMovement(state, before);
  CHECK(host.calls.empty());
}

TEST_CASE("Tracking faces a placed target", "[ai][movement]") {
  ai::MovementState state = SampleState();
  ai::MovementState expected = state;
  expected.move_x = -300;
  expected.move_z = 500;
  expected.destination = Tile(3, 0, 4);
  FakeMovementHost host;
  host.placements.at(6) = GroundPlacement(-300, 500);
  CHECK(ai::TrackActorPosition(state, host, 6) == 0);
  CheckSameMovement(state, expected);
  CheckOnlyCall(host, {"facing", 6, 0, Tile(3, 0, 4)});
}

TEST_CASE("Tracking enters facing once the facing update turns",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  host.placements.at(6) = GroundPlacement(-300, 500);
  host.facing_result = 1;
  CHECK(ai::TrackActorPosition(state, host, 6) == -1);
  CHECK(state.current_mode == 6);
  CHECK(state.previous_facing_tile == 0xFFFFFFFFu);
  CHECK(ai::TrackActorPosition(state, host, 6) == 6);
}

TEST_CASE("Relationship search returns the first other listed actor",
          "[ai][movement]") {
  const auto [relation, actor] = GENERATE(
      table<int, int>({{0, 9}, {1, 2}, {2, 5}, {3, -1}, {4, -1}, {-1, -1}}));
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  host.roster = Roster({2, 1, 5, 7, 9});
  state.relations.at(1) = 0x80;  // the actor itself is skipped
  state.relations.at(2) = 0x40;
  state.relations.at(5) = 0xBF;
  state.relations.at(7) = 0x80;
  state.relations.at(9) = 0x3F;
  for (const int id : {2, 5, 7, 9}) {
    ai::TargetPlacement placement = GroundPlacement(0, 0);
    placement.current_tile = Tile(static_cast<unsigned>(id), 0, 1);
    host.placements.at(static_cast<std::size_t>(id)) = placement;
  }
  const std::optional<ai::RelatedActor> found =
      ai::FindActorByRelationship(state, host, relation, true);
  REQUIRE(found.has_value());
  CHECK(Engaged(found).actor_id == actor);
  CHECK(Engaged(found).tile ==
        (actor < 0 ? 0u : Tile(static_cast<unsigned>(actor), 0, 1)));
}

TEST_CASE("Relationship search needs a model only when the tile is wanted",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  host.roster.reset();
  CHECK_FALSE(ai::FindActorByRelationship(state, host, 1, false).has_value());

  host.roster = Roster({3});
  state.relations.at(3) = 0x40;
  CHECK_FALSE(ai::FindActorByRelationship(state, host, 1, true).has_value());
  const std::optional<ai::RelatedActor> found =
      ai::FindActorByRelationship(state, host, 1, false);
  REQUIRE(found.has_value());
  CHECK(Engaged(found).actor_id == 3);
  CHECK(Engaged(found).tile == 0u);
}

TEST_CASE("Facing cosine compares the facing with the horizontal direction",
          "[ai][movement]") {
  const Position origin{};
  CHECK(ai::TargetFacingCosine(origin, Position{.x = 64}, 0, 0) == 4096);
  CHECK(ai::TargetFacingCosine(origin, origin, 1234, 0) == 4096);
  const auto [x, y, z, angle, expected] =
      GENERATE(table<std::int16_t, std::int16_t, std::int16_t, int, int>(
          {{0, 0, -64, 0, 4096},
           {0, 900, -64, 0, 4096},
           {0, 0, 64, 0, -4096},
           {64, 0, 0, 0, 0},
           {64, 0, 0, 3072, 4096},
           {-64, 0, 0, 1024, 4096},
           {-64, 0, 0, 1024 + 4096, 4096}}));
  const Position to{.x = x, .y = y, .z = z};
  CHECK(ai::TargetFacingCosine(origin, to, angle, 64) == expected);
}

TEST_CASE("Live facing cosine matches the core", "[ai][movement]") {
  const auto [x, z, angle] = GENERATE(table<std::int16_t, std::int16_t, int>(
      {{0, -64, 0}, {64, -64, 0}, {-90, 30, 700}, {5, 5, 3000}}));
  const SVECTOR from{100, 0, 100, 0};
  const SVECTOR to{static_cast<std::int16_t>(100 + x), 50,
                   static_cast<std::int16_t>(100 + z), 0};
  const Position core_from{.x = 100, .y = 0, .z = 100};
  const Position core_to{.x = static_cast<std::int16_t>(100 + x),
                         .y = 50,
                         .z = static_cast<std::int16_t>(100 + z)};
  CHECK(ai::LiveTargetFacingCosine(&from, &to, angle, 1) ==
        ai::TargetFacingCosine(core_from, core_to, angle, 1));
}

TEST_CASE("Live movement entry stores the wander reset", "[ai][movement]") {
  auto live = std::make_unique<Battle_Ai_ActorAiState>();
  live->currentMovementMode = 0;
  live->requestedMovementMode = 0;
  live->movementEnabled = 1;
  live->navigationFlags = 0xFFFFFFFFu;
  live->wanderPhase = 9;
  live->wanderCooldown = 4;
  std::fill(std::begin(live->movementModeCounters),
            std::end(live->movementModeCounters), 0x5A);
  CHECK(ai::SetLiveMovementBehavior(live.get(), 3) == 6);
  CHECK(live->currentMovementMode == 3);
  CHECK(live->requestedMovementMode == 3);
  CHECK(live->wanderPhase == 0);
  CHECK(live->wanderCooldown == 0);
  CHECK(live->navigationFlags == 0xFFFFF3DEu);
  CHECK(live->movementModeCounters[6] == 0);
  CHECK(live->movementModeCounters[5] == 0x5A);
  CHECK(ai::SetLiveMovementBehavior(live.get(), 3) == 3);
}

TEST_CASE("Action movement selection moves in place for patrol combat",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  host.patrol_combat = true;
  host.movement_mask = 0x0F;
  REQUIRE(ai::SelectActionMovementMode(state, host));
  CHECK(state.plan_mode == 1);
  CHECK(host.best_reads == 0);

  // A zero mask cannot match, so the best candidate is not read.
  host.patrol_combat = false;
  host.movement_mask = 0;
  state.plan_mode = 0x77;
  REQUIRE(ai::SelectActionMovementMode(state, host));
  CHECK(state.plan_mode == 1);
  CHECK(host.best_reads == 0);
}

TEST_CASE("Action movement selection refuses a missing best candidate",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  host.movement_mask = 0x0C;
  CHECK_FALSE(ai::SelectActionMovementMode(state, host));
  CHECK(state.plan_mode == 0x77);
  CHECK(host.calls.empty());
}

TEST_CASE("Action movement selection walks to a waypoint for move actions",
          "[ai][movement]") {
  ai::MovementState state = SampleState();
  FakeMovementHost host;
  host.movement_mask = 0x0C;
  host.best_flags = 0xF3u;
  REQUIRE(ai::SelectActionMovementMode(state, host));
  CHECK(state.plan_mode == 1);
  CHECK(host.calls.empty());

  host.best_flags = 0x04u;
  REQUIRE(ai::SelectActionMovementMode(state, host));
  CHECK(state.plan_mode == 2);
  CheckOnlyCall(host, {"waypoint", 0, 0, 0x44332211u});
}

TEST_CASE("Action relationship changes react before propagating",
          "[ai][movement]") {
  FakeRelationshipHost host;
  host.roster.reset();
  CHECK_FALSE(ai::ApplyActionRelationshipChanges(host));
  CHECK(host.calls.empty());

  host.roster = Roster({3, 0, 7});
  host.states.at(3) = host.states.at(7) = true;
  REQUIRE(ai::ApplyActionRelationshipChanges(host));
  const std::vector<std::pair<std::string, int>> expected{
      {"react", 3}, {"react", 7}, {"propagate", 3}, {"propagate", 7}};
  CHECK(host.calls == expected);
}
