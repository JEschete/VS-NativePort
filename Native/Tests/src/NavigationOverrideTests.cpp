#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Movement.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Field/Collision.h"
}

static_assert(
    std::is_same_v<decltype(&Battle_Ai_HasActorNavigationState), int (*)(int)>);
static_assert(std::is_same_v<decltype(&Battle_Ai_ActorNavigationOverrideStatus),
                             int (*)(int)>);
static_assert(
    std::is_same_v<decltype(&Battle_Ai_CopyActorNavigationOverrideState),
                   void (*)(int, Battle_Ai_ActorAiState*)>);

namespace {
struct NavigationOverrideFixture {
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  decltype(Battle_Ai_tileFlagMap) saved_grid = Battle_Ai_tileFlagMap;
  u_short grid[32][32]{};
  Battle_Ai_ActorAiState source{};
  Battle_Ai_ActorAiState route{};
  Battle_Ai_TilePosition tile{};
  Actors_ActorRecord model{};

  NavigationOverrideFixture() {
    std::fill_n(Battle_Ai_actorAiStates, 16, nullptr);
    Battle_Ai_tileFlagMap = grid;
    source.nativePointer18C = &route;
    route.nativePointer64 = &tile;
  }

  ~NavigationOverrideFixture() {
    std::copy(saved_states.begin(), saved_states.end(),
              Battle_Ai_actorAiStates);
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
    Battle_Ai_tileFlagMap = saved_grid;
  }

  static void CheckCellValue(u_short actual, u_short expected) {
    CHECK(actual == expected);
  }

  void CheckOtherCells(int x, int z, u_short expected) const {
    for (int row = 0; row < 32; ++row) {
      for (int column = 0; column < 32; ++column) {
        if (row != z || column != x) {
          CheckCellValue(grid[row][column], expected);
        }
      }
    }
  }
};

auto Snapshot(const Battle_Ai_ActorAiState& state) {
  std::array<std::byte, sizeof(state)> bytes{};
  std::memcpy(bytes.data(), &state, bytes.size());
  return bytes;
}

void SeedCopyState(Battle_Ai_ActorAiState& state) {
  state.reserved0 = 0x12;
  state.movementEnabled = -1;
  state.settingsBit14 = 0xAB;
  state.actorId = 99;
  state.reserved9C[19] = 0x34;
  state.step_height_bytes[0] = 0xA5;
  state.step_height_bytes[1] = 0xA5;
  state.reservedB2 = 0xA5;
  state.maxRange = 0xCD;
  state.destination.word = 0xFEDCBA98;
  state.navigationFlags = 0xFFFFFFFF;
  state.opaque19A = 0xAB;
  state.reserved19B[0] = 0x5A;
  state.candidateCount = 16;
  state.candidates[15].targetId = 7;
  state.candidateOrder[15] = &state.candidates[15];
  state.currentCandidate = &state.candidates[0];
  state.reactionPending = 0xEF;
}

void SetExpectedOverrideFields(Battle_Ai_ActorAiState& state, int actor_id) {
  state.actorId = static_cast<u_char>(actor_id);
  state.step_height_bytes[0] = 0x40;
  state.step_height_bytes[1] = 1;
  state.reservedB2 = 3;
  state.navigationFlags = 0;
  state.opaque19A = 0;
  state.settingsBit14 = 3;
  state.movementEnabled = 1;
}
}  // namespace

TEST_CASE(
    "Navigation presence and override status preserve missing-state gates",
    "[ai][navigation][override]") {
  const bool grid = GENERATE(false, true);
  const bool state = GENERATE(false, true);
  NavigationOverrideFixture fixture;
  if (!grid) Battle_Ai_tileFlagMap = nullptr;
  fixture.source.nativePointer18C = nullptr;
  for (int actor_id = 0; actor_id < 16; ++actor_id) {
    Battle_Ai_actorAiStates[actor_id] = state ? &fixture.source : nullptr;
    CHECK(Battle_Ai_HasActorNavigationState(actor_id) == (grid && state));
    CHECK(Battle_Ai_ActorNavigationOverrideStatus(actor_id) == 2);
  }
}

TEST_CASE("Navigation override status compares only horizontal target bytes",
          "[ai][navigation][override]") {
  const unsigned ignored = GENERATE(0u, 0x12u, 0xFFu);
  const int actor_id = GENERATE(0, 7, 15);
  NavigationOverrideFixture fixture;
  Battle_Ai_actorAiStates[actor_id] = &fixture.source;
  fixture.tile.word = 0xABCD12EF;
  fixture.route.destination.word =
      (ignored << 24) | 0x00CD00EF | (ignored << 8);
  CHECK(Battle_Ai_ActorNavigationOverrideStatus(actor_id) == 0);
  fixture.route.destination.word ^= 1;
  CHECK(Battle_Ai_ActorNavigationOverrideStatus(actor_id) == 1);
  fixture.route.destination.word ^= 0x10001;
  CHECK(Battle_Ai_ActorNavigationOverrideStatus(actor_id) == 1);
}

TEST_CASE(
    "Navigation override copy preserves full native state and borrowed "
    "pointers",
    "[ai][navigation][override]") {
  const int actor_id = GENERATE(0, 7, 15);
  const bool self = GENERATE(false, true);
  NavigationOverrideFixture fixture;
  SeedCopyState(fixture.source);
  fixture.source.nativePointer60 = &fixture.source;
  fixture.source.nativePointer64 = &fixture.tile;
  fixture.source.nativePointer68 = &fixture.route;
  fixture.source.nextEngaged = &fixture.route;
  const auto before = Snapshot(fixture.source);
  auto expected = fixture.source;
  SetExpectedOverrideFields(expected, actor_id);
  struct {
    std::uint64_t before = 0x123456789ABCDEF0;
    Battle_Ai_ActorAiState state{};
    std::uint64_t after = 0xFEDCBA9876543210;
  } guarded;
  auto* output = self ? &fixture.source : &guarded.state;
  Battle_Ai_actorAiStates[actor_id] = &fixture.source;
  Battle_Ai_tileFlagMap = nullptr;
  Battle_Ai_CopyActorNavigationOverrideState(actor_id, output);
  CHECK(Snapshot(*output) == Snapshot(expected));
  CHECK(guarded.before == 0x123456789ABCDEF0);
  CHECK(guarded.after == 0xFEDCBA9876543210);
  if (!self) CHECK(Snapshot(fixture.source) == before);
}

TEST_CASE(
    "Navigation override copies reset every entry rather than retaining old "
    "flags",
    "[ai][navigation][override]") {
  NavigationOverrideFixture fixture;
  Battle_Ai_actorAiStates[4] = &fixture.source;
  SeedCopyState(fixture.source);
  Battle_Ai_CopyActorNavigationOverrideState(4, &fixture.route);
  fixture.route.navigationFlags = 0xFFFFFFFF;
  fixture.route.opaque19A = 0xFF;
  fixture.source.destination.word = 0x12345678;
  fixture.source.candidateCount = 2;
  auto expected = fixture.source;
  SetExpectedOverrideFields(expected, 4);
  Battle_Ai_CopyActorNavigationOverrideState(4, &fixture.route);
  CHECK(Snapshot(fixture.route) == Snapshot(expected));
}

TEST_CASE("Dynamic navigation marking preserves every source flag word",
          "[ai][navigation][tile-marker]") {
  const int x = GENERATE(0, 31);
  const int z = GENERATE(0, 31);
  NavigationOverrideFixture fixture;
  constexpr u_short kCanary = 0xA15A;
  for (auto& row : fixture.grid) std::ranges::fill(row, kCanary);
  for (unsigned word = 0; word < 65536; ++word) {
    fixture.grid[z][x] = static_cast<u_short>(word);
    Battle_Ai_MarkAiTileDynamicObstacle(x, z);
    REQUIRE(fixture.grid[z][x] == (word | 0x400u));
    Battle_Ai_MarkAiTileDynamicObstacle(x, z);
    CHECK(fixture.grid[z][x] == (word | 0x400u));
  }
  fixture.CheckOtherCells(x, z, kCanary);
}

TEST_CASE("Dynamic navigation marking reaches every native grid cell",
          "[ai][navigation][tile-marker]") {
  NavigationOverrideFixture fixture;
  for (int z = 0; z < 32; ++z) {
    for (int x = 0; x < 32; ++x) {
      Battle_Ai_MarkAiTileDynamicObstacle(x, z);
      CHECK(fixture.grid[z][x] == 0x400);
    }
  }
}

TEST_CASE("Dynamic navigation marking follows grid availability and rebinding",
          "[ai][navigation][tile-marker]") {
  NavigationOverrideFixture fixture;
  fixture.grid[2][1] = 3;
  Battle_Ai_tileFlagMap = nullptr;
  Battle_Ai_MarkAiTileDynamicObstacle(-1, -1);
  Battle_Ai_MarkAiTileDynamicObstacle(std::numeric_limits<int>::min(),
                                      std::numeric_limits<int>::max());
  CHECK(fixture.grid[2][1] == 3);
  Battle_Ai_tileFlagMap = fixture.grid;
  Battle_Ai_MarkAiTileDynamicObstacle(1, 2);
  CHECK(fixture.grid[2][1] == 0x403);
  u_short second[32][32]{};
  second[2][1] = 0xA0;
  Battle_Ai_tileFlagMap = second;
  Battle_Ai_MarkAiTileDynamicObstacle(1, 2);
  CHECK(second[2][1] == 0x4A0);
  CHECK(fixture.grid[2][1] == 0x403);
}

TEST_CASE(
    "Scripted path completion forwards the actor and clears only reached "
    "bypass",
    "[ai][navigation][override]") {
  const int status = GENERATE(0, 1, 2);
  NavigationOverrideFixture fixture;
  Battle_Ai_actorAiStates[7] = &fixture.source;
  Actors_actorRecords[7] = &fixture.model;
  fixture.model.bypassActorCollision = 1;
  fixture.model.ignoredByCollision = 1;
  fixture.tile.word = 0xABCD12EF;
  fixture.route.destination.word = fixture.tile.word ^ (status == 1 ? 1u : 0u);
  if (status == 2) fixture.source.nativePointer18C = nullptr;
  CHECK(Actors_IsScriptedActorPathFinished(7) == (status != 1));
  CHECK(Field_ActorCollisionBypass(7) == (status == 0 ? 1 : 3));
}
