#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Legacy/Psx.hpp"

namespace {
struct MovementFixture {
  decltype(VS::Battle::Ai::patrolRoutes) saved_routes =
      VS::Battle::Ai::patrolRoutes;
  std::uint16_t routes[4][8]{};
  VS::Battle::Ai::ActorAiState state{};

  MovementFixture() {
    VS::Battle::Ai::patrolRoutes = routes;
    state.navigation_flags = 0xFFFFFFFF;
    state.current_movement_mode = 7;
    std::fill_n(state.movement_mode_counters.data(), 13, 0x55);
  }

  ~MovementFixture() { VS::Battle::Ai::patrolRoutes = saved_routes; }

  std::array<std::byte, sizeof(VS::Battle::Ai::ActorAiState)> Bytes() const {
    std::array<std::byte, sizeof(VS::Battle::Ai::ActorAiState)> result;
    std::memcpy(result.data(), &state, result.size());
    return result;
  }
};
}  // namespace

TEST_CASE("Movement aliases preserve native pointer and patrol view layout",
          "[ai][movement-behavior]") {
  CHECK(offsetof(VS::Battle::Ai::ActorAiState, requested_movement_mode) ==
        offsetof(VS::Battle::Ai::ActorAiState, reserved89));
  CHECK(offsetof(VS::Battle::Ai::ActorAiState, current_movement_mode) ==
        offsetof(VS::Battle::Ai::ActorAiState, reserved89) + 1);
  CHECK(offsetof(VS::Battle::Ai::ActorAiState, navigation_flags) ==
        offsetof(VS::Battle::Ai::ActorAiState, native_pointer_18c) +
            sizeof(void*));
  CHECK(offsetof(VS::Battle::Ai::ActorAiState, patrol_index) ==
        offsetof(VS::Battle::Ai::PatrolView, index));
  CHECK(offsetof(VS::Battle::Ai::ActorAiState, patrol_row) ==
        offsetof(VS::Battle::Ai::PatrolView, row));
  CHECK(offsetof(VS::Battle::Ai::ActorAiState, patrol_cooldown) ==
        offsetof(VS::Battle::Ai::PatrolView, cooldown));
  CHECK(offsetof(VS::Battle::Ai::ActorAiState, movement_mode_counters) ==
        offsetof(VS::Battle::Ai::ActorAiState, reserved474));
}

TEST_CASE("Movement cooldown clears only the original navigation mask",
          "[ai][movement-behavior]") {
  const auto flags = GENERATE(0u, 0xFFFFFFFFu, 0x12345678u, 0xFE1u);
  const int enabled = GENERATE(0, 1, -1, -128, 127);
  MovementFixture fixture;
  fixture.state.navigation_flags = flags;
  fixture.state.movement_enabled = static_cast<signed char>(enabled);
  VS::Battle::Ai::RefreshLiveAiMovementCooldown(
      reinterpret_cast<VS::Battle::Ai::TerrainMoveView*>(&fixture.state));
  CHECK(fixture.state.navigation_flags ==
        ((flags & 0xFFFFF01Eu) | (enabled != 0 ? 0x3C0u : 0u)));
}

TEST_CASE("Movement behavior changes reset only their own counter byte",
          "[ai][movement-behavior]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6);
  MovementFixture fixture;
  constexpr std::array<int, 7> kResults{-1, 3, -1, 6, -1, 1, -1};
  CHECK(VS::Battle::Ai::SetLiveMovementBehavior(&fixture.state, mode) ==
        kResults[mode]);
  CHECK(fixture.state.requested_movement_mode == mode);
  CHECK(fixture.state.current_movement_mode == mode);
  for (int index = 0; index < 13; ++index) {
    CHECK(fixture.state.movement_mode_counters[index] ==
          (index == mode * 2 ? 0 : 0x55));
  }
  CHECK(fixture.state.navigation_flags == 0xFFFFF01Eu);
}

TEST_CASE("Repeated movement requests preserve every other state byte",
          "[ai][movement-behavior]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6);
  MovementFixture fixture;
  fixture.state.current_movement_mode = static_cast<std::uint8_t>(mode);
  fixture.state.requested_movement_mode = static_cast<std::uint8_t>(mode);
  const auto expected = fixture.Bytes();
  fixture.state.requested_movement_mode = 99;
  CHECK(VS::Battle::Ai::SetLiveMovementBehavior(&fixture.state, mode) == mode);
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("Pursuit behavior clears six approach bytes but not adjacent options",
          "[ai][movement-behavior]") {
  MovementFixture fixture;
  auto& approach =
      *reinterpret_cast<VS::Battle::Ai::TargetApproachView*>(&fixture.state);
  approach.chasing_lost_target = approach.target_id = approach.timeout = 9;
  approach.recover_in_range = approach.walked_close = approach.range_mode = 8;
  approach.previous_option = 7;
  approach.previous_target_id = 6;
  CHECK(VS::Battle::Ai::SetLiveMovementBehavior(&fixture.state, 1) == 3);
  CHECK(approach.chasing_lost_target == 0);
  CHECK(approach.target_id == 0);
  CHECK(approach.timeout == 0);
  CHECK(approach.recover_in_range == 0);
  CHECK(approach.walked_close == 0);
  CHECK(approach.range_mode == 0);
  CHECK(approach.previous_option == 7);
  CHECK(approach.previous_target_id == 6);
}

TEST_CASE("Wander behavior retains neighboring movement state",
          "[ai][movement-behavior]") {
  MovementFixture fixture;
  fixture.state.wander_phase = 1;
  fixture.state.wander_timer = 2;
  fixture.state.wander_cooldown = 3;
  fixture.state.previous_facing_tile.word = 4;
  fixture.state.facing_pending = 5;
  CHECK(VS::Battle::Ai::SetLiveMovementBehavior(&fixture.state, 3) == 6);
  CHECK(fixture.state.wander_phase == 0);
  CHECK(fixture.state.wander_cooldown == 0);
  CHECK(fixture.state.wander_timer == 2);
  CHECK(fixture.state.previous_facing_tile.word == 4);
  CHECK(fixture.state.facing_pending == 5);
}

TEST_CASE("Facing behavior retains neighboring movement state",
          "[ai][movement-behavior]") {
  MovementFixture fixture;
  fixture.state.previous_facing_tile.word = 4;
  fixture.state.facing_pending = 5;
  fixture.state.wander_timer = 2;
  CHECK(VS::Battle::Ai::SetLiveMovementBehavior(&fixture.state, 6) == -1);
  CHECK(fixture.state.previous_facing_tile.word == -1);
  CHECK(fixture.state.facing_pending == 0);
  CHECK(fixture.state.wander_timer == 2);
}

TEST_CASE(
    "Patrol entry uses enabled Manhattan-nearest waypoints and first ties",
    "[ai][movement-behavior]") {
  MovementFixture fixture;
  fixture.state.patrol_row = 3;
  fixture.state.patrol_index = 7;
  fixture.state.patrol_cooldown = 12;
  fixture.state.tile.tile.tile_x = 4;
  fixture.state.tile.tile.tile_z = 5;
  const std::array<std::uint16_t, 8> entries{0,
                                             0x8000 | 4 | (8 << 5),
                                             0x8000 | 7 | (5 << 5),
                                             4 | (5 << 5),
                                             0x8000 | 31 | (31 << 5),
                                             0,
                                             0,
                                             0};
  std::copy(entries.begin(), entries.end(), fixture.routes[3]);
  CHECK(VS::Battle::Ai::SetLiveMovementBehavior(&fixture.state, 5) == 1);
  CHECK(fixture.state.patrol_index == 1);
  CHECK(fixture.state.patrol_cooldown == 0);
}

TEST_CASE("Patrol entry without enabled waypoints retains its selected index",
          "[ai][movement-behavior]") {
  MovementFixture fixture;
  fixture.state.patrol_index = 6;
  fixture.state.patrol_cooldown = 15;
  CHECK(VS::Battle::Ai::SetLiveMovementBehavior(&fixture.state, 5) == 1);
  CHECK(fixture.state.patrol_index == 6);
  CHECK(fixture.state.patrol_cooldown == 0);
}
