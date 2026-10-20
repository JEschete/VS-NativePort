#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/MovementBehavior.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/Movement.h"
}

namespace {
struct MovementFixture {
  decltype(Battle_Ai_patrolRoutes) saved_routes = Battle_Ai_patrolRoutes;
  std::uint16_t routes[4][8]{};
  Battle_Ai_ActorAiState state{};

  MovementFixture() {
    Battle_Ai_patrolRoutes = routes;
    state.navigationFlags = 0xFFFFFFFF;
    state.currentMovementMode = 7;
    std::fill_n(state.movementModeCounters, 13, 0x55);
  }

  ~MovementFixture() { Battle_Ai_patrolRoutes = saved_routes; }

  std::array<std::byte, sizeof(Battle_Ai_ActorAiState)> Bytes() const {
    std::array<std::byte, sizeof(Battle_Ai_ActorAiState)> result;
    std::memcpy(result.data(), &state, result.size());
    return result;
  }
};
}  // namespace

TEST_CASE("Movement aliases preserve native pointer and patrol view layout",
          "[ai][movement-behavior]") {
  CHECK(offsetof(Battle_Ai_ActorAiState, requestedMovementMode) ==
        offsetof(Battle_Ai_ActorAiState, reserved89));
  CHECK(offsetof(Battle_Ai_ActorAiState, currentMovementMode) ==
        offsetof(Battle_Ai_ActorAiState, reserved89) + 1);
  CHECK(offsetof(Battle_Ai_ActorAiState, navigationFlags) ==
        offsetof(Battle_Ai_ActorAiState, nativePointer18C) + sizeof(void*));
  CHECK(offsetof(Battle_Ai_ActorAiState, patrolIndex) ==
        offsetof(Battle_Ai_PatrolView, index));
  CHECK(offsetof(Battle_Ai_ActorAiState, patrolRow) ==
        offsetof(Battle_Ai_PatrolView, row));
  CHECK(offsetof(Battle_Ai_ActorAiState, patrolCooldown) ==
        offsetof(Battle_Ai_PatrolView, opaque104));
  CHECK(offsetof(Battle_Ai_ActorAiState, movementModeCounters) ==
        offsetof(Battle_Ai_ActorAiState, reserved474));
}

TEST_CASE("Movement cooldown clears only the original navigation mask",
          "[ai][movement-behavior]") {
  const auto flags = GENERATE(0u, 0xFFFFFFFFu, 0x12345678u, 0xFE1u);
  const int enabled = GENERATE(0, 1, -1, -128, 127);
  MovementFixture fixture;
  fixture.state.navigationFlags = flags;
  fixture.state.movementEnabled = static_cast<signed char>(enabled);
  Battle_Ai_RefreshAiMovementCooldown(
      reinterpret_cast<Battle_Ai_TerrainMoveView*>(&fixture.state));
  CHECK(fixture.state.navigationFlags ==
        ((flags & 0xFFFFF01Eu) | (enabled != 0 ? 0x3C0u : 0u)));
}

TEST_CASE("Movement behavior changes reset only their own counter byte",
          "[ai][movement-behavior]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6);
  MovementFixture fixture;
  constexpr std::array<int, 7> kResults{-1, 3, -1, 6, -1, 1, -1};
  CHECK(Battle_Ai_SetMovementBehavior(&fixture.state, mode) == kResults[mode]);
  CHECK(fixture.state.requestedMovementMode == mode);
  CHECK(fixture.state.currentMovementMode == mode);
  for (int index = 0; index < 13; ++index) {
    CHECK(fixture.state.movementModeCounters[index] ==
          (index == mode * 2 ? 0 : 0x55));
  }
  CHECK(fixture.state.navigationFlags == 0xFFFFF01Eu);
}

TEST_CASE("Repeated movement requests preserve every other state byte",
          "[ai][movement-behavior]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6);
  MovementFixture fixture;
  fixture.state.currentMovementMode = static_cast<u_char>(mode);
  fixture.state.requestedMovementMode = static_cast<u_char>(mode);
  const auto expected = fixture.Bytes();
  fixture.state.requestedMovementMode = 99;
  CHECK(Battle_Ai_SetMovementBehavior(&fixture.state, mode) == mode);
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("Pursuit behavior clears six approach bytes but not adjacent options",
          "[ai][movement-behavior]") {
  MovementFixture fixture;
  auto& approach =
      *reinterpret_cast<Battle_Ai_TargetApproachView*>(&fixture.state);
  approach.opaqueE4 = approach.targetId = approach.timeout = 9;
  approach.opaqueE7 = approach.opaqueE8 = approach.rangeMode = 8;
  approach.previousOption = 7;
  approach.previousTargetId = 6;
  CHECK(Battle_Ai_SetMovementBehavior(&fixture.state, 1) == 3);
  CHECK(approach.opaqueE4 == 0);
  CHECK(approach.targetId == 0);
  CHECK(approach.timeout == 0);
  CHECK(approach.opaqueE7 == 0);
  CHECK(approach.opaqueE8 == 0);
  CHECK(approach.rangeMode == 0);
  CHECK(approach.previousOption == 7);
  CHECK(approach.previousTargetId == 6);
}

TEST_CASE("Wander behavior retains neighboring movement state",
          "[ai][movement-behavior]") {
  MovementFixture fixture;
  fixture.state.wanderPhase = 1;
  fixture.state.wanderTimer = 2;
  fixture.state.wanderCooldown = 3;
  fixture.state.previousFacingTile.word = 4;
  fixture.state.facingPending = 5;
  CHECK(Battle_Ai_SetMovementBehavior(&fixture.state, 3) == 6);
  CHECK(fixture.state.wanderPhase == 0);
  CHECK(fixture.state.wanderCooldown == 0);
  CHECK(fixture.state.wanderTimer == 2);
  CHECK(fixture.state.previousFacingTile.word == 4);
  CHECK(fixture.state.facingPending == 5);
}

TEST_CASE("Facing behavior retains neighboring movement state",
          "[ai][movement-behavior]") {
  MovementFixture fixture;
  fixture.state.previousFacingTile.word = 4;
  fixture.state.facingPending = 5;
  fixture.state.wanderTimer = 2;
  CHECK(Battle_Ai_SetMovementBehavior(&fixture.state, 6) == -1);
  CHECK(fixture.state.previousFacingTile.word == -1);
  CHECK(fixture.state.facingPending == 0);
  CHECK(fixture.state.wanderTimer == 2);
}

TEST_CASE(
    "Patrol entry uses enabled Manhattan-nearest waypoints and first ties",
    "[ai][movement-behavior]") {
  MovementFixture fixture;
  fixture.state.patrolRow = 3;
  fixture.state.patrolIndex = 7;
  fixture.state.patrolCooldown = 12;
  fixture.state.tile.tile.tileX = 4;
  fixture.state.tile.tile.tileZ = 5;
  const std::array<std::uint16_t, 8> entries{0,
                                             0x8000 | 4 | (8 << 5),
                                             0x8000 | 7 | (5 << 5),
                                             4 | (5 << 5),
                                             0x8000 | 31 | (31 << 5),
                                             0,
                                             0,
                                             0};
  std::copy(entries.begin(), entries.end(), fixture.routes[3]);
  CHECK(Battle_Ai_SetMovementBehavior(&fixture.state, 5) == 1);
  CHECK(fixture.state.patrolIndex == 1);
  CHECK(fixture.state.patrolCooldown == 0);
}

TEST_CASE("Patrol entry without enabled waypoints retains its selected index",
          "[ai][movement-behavior]") {
  MovementFixture fixture;
  fixture.state.patrolIndex = 6;
  fixture.state.patrolCooldown = 15;
  CHECK(Battle_Ai_SetMovementBehavior(&fixture.state, 5) == 1);
  CHECK(fixture.state.patrolIndex == 6);
  CHECK(fixture.state.patrolCooldown == 0);
}
