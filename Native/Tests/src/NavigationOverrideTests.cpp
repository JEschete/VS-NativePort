#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"
#include "Platform/CrashHandler.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Movement.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

static_assert(
    std::is_same_v<decltype(&Battle_Ai_HasActorNavigationState), int (*)(int)>);
static_assert(std::is_same_v<decltype(&Battle_Ai_ActorNavigationOverrideStatus),
                             int (*)(int)>);
static_assert(
    std::is_same_v<decltype(&Battle_Ai_CopyActorNavigationOverrideState),
                   void (*)(int, Battle_Ai_ActorAiState*)>);
static_assert(std::is_same_v<decltype(&Battle_Ai_AddAiTileOccupant),
                             void (*)(const Field_GimmickInstance*)>);
static_assert(std::is_same_v<decltype(&Battle_Ai_RemoveAiTileOccupant),
                             void (*)(const Field_GimmickInstance*)>);
static_assert(offsetof(Field_GimmickPhysicsView, tileX) ==
              offsetof(Field_GimmickInstance, tileX));
static_assert(offsetof(Field_GimmickPhysicsView, tileZ) ==
              offsetof(Field_GimmickInstance, tileZ));

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

struct GimmickOccupancyFixture : NavigationOverrideFixture {
  decltype(Battle_Ai_specialTerrainHeights) saved_heights =
      Battle_Ai_specialTerrainHeights;
  decltype(std::to_array(Field_gimmickInstances)) saved_gimmicks =
      std::to_array(Field_gimmickInstances);
  u_short heights[32][32]{};
  Field_GimmickInstance gimmick{};
  Field_GimmickInstance sibling{};

  GimmickOccupancyFixture() {
    Battle_Ai_specialTerrainHeights = heights;
    std::fill_n(Field_gimmickInstances, saved_gimmicks.size(), nullptr);
    gimmick.tileX = 13;
    gimmick.tileZ = 7;
    gimmick.boneCount = 1;
    gimmick.objectInfo.objectId = 1;
    sibling.objectInfo.objectId = 1;
    Field_gimmickInstances[0] = &gimmick;
    Field_gimmickInstances[1] = &sibling;
  }

  ~GimmickOccupancyFixture() {
    Battle_Ai_specialTerrainHeights = saved_heights;
    std::ranges::copy(saved_gimmicks, Field_gimmickInstances);
  }
};

struct RouteAvailabilityFixture {
  void* saved_auxiliary = Battle_Ai_roomHasWaypoints;
  Battle_Ai_RouteGateEnd* saved_gates = Battle_Ai_routeGates;
  Field_RoomRecord saved_room = Field_roomData;
  std::array<u_char, 13> saved_bounce{};
  alignas(u_int) std::array<u_char, 0x498> auxiliary{};
  std::array<Field_MpdMovingPlatform, 13> platforms{};

  RouteAvailabilityFixture() {
    std::ranges::copy(
        std::span(Field_platformBounceFrames, saved_bounce.size()),
        saved_bounce.begin());
    std::fill_n(Field_platformBounceFrames, saved_bounce.size(), 0);
    Field_roomData = {};
    Field_roomData.section6 = auxiliary.data();
    Field_roomData.header.section6Length = auxiliary.size();
    Field_roomData.movingPlatforms = platforms.data();
    Field_roomData.header.movingPlatformsLength = sizeof platforms;
    Battle_Ai_roomHasWaypoints = auxiliary.data();
    Battle_Ai_routeGates =
        reinterpret_cast<Battle_Ai_RouteGateEnd*>(auxiliary.data() + 0x454);
    SetEnd(0, 3, 7);
  }

  ~RouteAvailabilityFixture() {
    std::ranges::copy(saved_bounce, Field_platformBounceFrames);
    Field_roomData = saved_room;
    Battle_Ai_roomHasWaypoints = saved_auxiliary;
    Battle_Ai_routeGates = saved_gates;
  }

  void SetEnd(std::size_t pair, unsigned x, unsigned z) {
    Battle_Ai_RouteGateEnd end{};
    end.tileX = static_cast<u_short>(x);
    end.tileZ = static_cast<u_short>(z);
    std::memcpy(auxiliary.data() + 0x454 + pair * 4, &end, sizeof end);
  }

  void SetVisible(unsigned mask) {
    for (std::size_t index = 0; index < platforms.size(); ++index) {
      platforms[index].isHidden = (mask & (1u << index)) == 0;
    }
  }

  void SetWord(std::size_t index, u_int word) {
    std::memcpy(auxiliary.data() + index * sizeof(word), &word, sizeof(word));
  }

  u_int Word(std::size_t index) const {
    u_int word;
    std::memcpy(&word, auxiliary.data() + index * sizeof(word), sizeof(word));
    return word;
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

TEST_CASE("Gimmick occupancy adjusts the height grid instead of obstacle flags",
          "[ai][navigation][gimmick-occupancy][occupancy-red]") {
  GimmickOccupancyFixture fixture;
  fixture.heights[7][13] = 5;
  fixture.grid[7][13] = 0xA15A;
  Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
  CHECK(fixture.heights[7][13] == 0x405);
  CHECK(fixture.grid[7][13] == 0xA15A);
  Battle_Ai_RemoveAiTileOccupant(&fixture.gimmick);
  CHECK(fixture.heights[7][13] == 5);
  CHECK(fixture.grid[7][13] == 0xA15A);
}

TEST_CASE("Gimmick occupancy preserves source halfword wrap for every value",
          "[ai][navigation][gimmick-occupancy]") {
  GimmickOccupancyFixture fixture;
  for (unsigned word = 0; word < 65536; ++word) {
    fixture.heights[7][13] = static_cast<u_short>(word);
    Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
    REQUIRE(fixture.heights[7][13] == static_cast<u_short>(word + 0x400));
    Battle_Ai_RemoveAiTileOccupant(&fixture.gimmick);
    CHECK(fixture.heights[7][13] == word);
    Battle_Ai_RemoveAiTileOccupant(&fixture.gimmick);
    CHECK(fixture.heights[7][13] == static_cast<u_short>(word - 0x400));
  }
}

TEST_CASE("Gimmick placement retains original layer bits and native row carry",
          "[ai][navigation][gimmick-occupancy]") {
  GimmickOccupancyFixture fixture;
  for (unsigned layer = 0; layer < 256; ++layer) {
    if ((layer >> 2) & 1) continue;
    for (unsigned x = 0; x < 256; ++x) {
      const unsigned placement = 0xFF030000u | (layer << 8) | x;
      const unsigned offset = (placement & 255) * 2 + ((placement << 8) >> 18);
      const auto index = offset / sizeof(u_short);
      fixture.gimmick.tileX = static_cast<char>(x);
      fixture.gimmick.tileZ = 3;
      fixture.gimmick.tileLayer = static_cast<char>(layer);
      fixture.gimmick.reserved5F = static_cast<char>(0xFF);
      auto& cell = fixture.heights[index / 32][index % 32];
      cell = 0xA15A;
      Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
      CHECK(cell == 0xA55A);
      Battle_Ai_RemoveAiTileOccupant(&fixture.gimmick);
      CHECK(cell == 0xA15A);
    }
  }
}

TEST_CASE("Gimmick occupancy follows room map absence and rebinding",
          "[ai][navigation][gimmick-occupancy]") {
  GimmickOccupancyFixture fixture;
  Battle_Ai_tileFlagMap = nullptr;
  Battle_Ai_specialTerrainHeights = nullptr;
  Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
  Battle_Ai_RemoveAiTileOccupant(&fixture.gimmick);
  Battle_Ai_tileFlagMap = fixture.grid;
  Battle_Ai_specialTerrainHeights = fixture.heights;
  Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
  u_short second[32][32]{};
  Battle_Ai_specialTerrainHeights = second;
  Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
  Battle_Ai_RemoveAiTileOccupant(&fixture.gimmick);
  CHECK(second[7][13] == 0);
  CHECK(fixture.heights[7][13] == 0x400);
}

TEST_CASE("Gimmick occupancy reaches every cell of the bounded native map",
          "[ai][navigation][gimmick-occupancy]") {
  GimmickOccupancyFixture fixture;
  for (int z = 0; z < 32; ++z) {
    for (int x = 0; x < 32; ++x) {
      fixture.gimmick.tileX = static_cast<char>(x);
      fixture.gimmick.tileZ = static_cast<char>(z);
      fixture.gimmick.tileLayer = 3;
      fixture.heights[z][x] = 0x123;
      Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
      REQUIRE(fixture.heights[z][x] == 0x523);
      Battle_Ai_RemoveAiTileOccupant(&fixture.gimmick);
      CHECK(fixture.heights[z][x] == 0x123);
    }
  }
  for (const auto& row : fixture.heights) {
    for (const auto cell : row) CHECK(cell == 0x123);
  }
}

TEST_CASE("Production gimmick release balances one occupancy contribution",
          "[ai][navigation][gimmick-occupancy][occupancy-release]") {
  GimmickOccupancyFixture fixture;
  fixture.heights[7][13] = 0x88;
  for (int cycle = 0; cycle < 2; ++cycle) {
    Field_gimmickInstances[0] = &fixture.gimmick;
    Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
    CHECK(fixture.heights[7][13] == 0x488);
    CHECK(Field_ReleaseGimmickInstance(0) == 0);
    CHECK(Field_gimmickInstances[0] == nullptr);
    CHECK(fixture.heights[7][13] == 0x88);
    CHECK(Field_gimmickInstances[1] == &fixture.sibling);
  }
}

TEST_CASE("Original gimmick occupancy entries resolve to native functions",
          "[ai][navigation][gimmick-occupancy][occupancy-binding]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E6898) ==
        reinterpret_cast<void*>(&Battle_Ai_AddAiTileOccupant));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E68A0) ==
        reinterpret_cast<void*>(&Battle_Ai_RemoveAiTileOccupant));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E6EB0) ==
        reinterpret_cast<void*>(&Battle_Ai_ReleaseActorAiContactOccupancy));
}

TEST_CASE("Gimmick occupancy rejects an unaligned source halfword",
          "[.occupancy-unaligned]") {
  VS::Platform::InstallCrashHandler();
  GimmickOccupancyFixture fixture;
  fixture.gimmick.tileLayer = 4;
  Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
  FAIL("Unaligned occupancy placement was accepted");
}

TEST_CASE("Gimmick occupancy rejects the first byte beyond its map",
          "[.occupancy-bounds]") {
  VS::Platform::InstallCrashHandler();
  GimmickOccupancyFixture fixture;
  fixture.gimmick.tileX = fixture.gimmick.tileZ = 31;
  fixture.gimmick.tileLayer = 8;
  Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
  FAIL("Out-of-bounds occupancy placement was accepted");
}

TEST_CASE("Gimmick occupancy requires the active native height map",
          "[.occupancy-height-map]") {
  VS::Platform::InstallCrashHandler();
  GimmickOccupancyFixture fixture;
  Battle_Ai_specialTerrainHeights = nullptr;
  Battle_Ai_AddAiTileOccupant(&fixture.gimmick);
  FAIL("Missing active occupancy map was accepted");
}

TEST_CASE("Route availability connects both endpoints to visible platforms",
          "[ai][route-availability][route-availability-red]") {
  RouteAvailabilityFixture fixture;
  fixture.SetVisible(1);
  constexpr u_int kFirst = 0x400 | (3u << 5) | 7;
  constexpr u_int kSecond = 0x04000000 | (3u << 21) | (7u << 16);
  fixture.SetWord(1, kFirst);
  fixture.SetWord(2, kSecond);
  fixture.SetWord(3, kFirst | kSecond);
  Battle_Ai_UpdateRouteGateAvailability();
  CHECK(fixture.Word(1) == (kFirst | 0x80000000u));
  CHECK(fixture.Word(2) == (kSecond | 0x8000));
  CHECK(fixture.Word(3) == (kFirst | kSecond));
}

TEST_CASE("Route availability preserves every platform mask and selector",
          "[ai][route-availability]") {
  RouteAvailabilityFixture fixture;
  constexpr u_int kFirst = 0x400 | (3u << 5) | 7;
  constexpr u_int kSecond = 0x04000000 | (3u << 21) | (7u << 16);
  for (unsigned mask = 0; mask < 16; ++mask) {
    fixture.SetVisible(mask);
    for (unsigned selector = 0; selector < 4; ++selector) {
      const auto first = kFirst | (selector << 13);
      const auto second = kSecond | (selector << 29);
      fixture.SetWord(1, first);
      fixture.SetWord(2, second);
      fixture.SetWord(3, first | second);
      fixture.SetWord(4, 0xA31D51A5u & ~0x04000400u);
      Battle_Ai_UpdateRouteGateAvailability();
      const bool visible = (mask & (1u << selector)) != 0;
      CHECK(fixture.Word(1) == (first | (visible ? 0x80000000u : 0x80008000u)));
      CHECK(fixture.Word(2) == (second | (visible ? 0x8000u : 0x80008000u)));
      CHECK(fixture.Word(3) ==
            ((first | second) | (visible ? 0u : 0x80008000u)));
      CHECK(fixture.Word(4) == (0xA31D51A5u & ~0x04000400u));
    }
  }
}

TEST_CASE("Route availability retains mutated shift operands between pairs",
          "[ai][route-availability]") {
  RouteAvailabilityFixture fixture;
  fixture.SetVisible(8);
  fixture.SetEnd(0, 3, 6);
  fixture.SetEnd(1, 3, 7);
  constexpr u_int kFirst = 0x400 | (3u << 5) | 7 | (3u << 13);
  constexpr u_int kSecond = 0x04000000 | (3u << 21) | (7u << 16) | (3u << 29);
  fixture.SetWord(1, kFirst);
  fixture.SetWord(2, kSecond);
  Battle_Ai_UpdateRouteGateAvailability();
  CHECK(fixture.Word(1) == (kFirst | 0x80008000u));
  CHECK(fixture.Word(2) == (kSecond | 0x80008000u));
}

TEST_CASE(
    "Route availability updates all words and reopens after visibility changes",
    "[ai][route-availability]") {
  RouteAvailabilityFixture fixture;
  constexpr u_int kFirst = 0x400 | (3u << 5) | 7;
  for (unsigned word = 0; word < 256; ++word) fixture.SetWord(word, kFirst);
  for (unsigned mask : {1u, 0u, 1u}) {
    fixture.SetVisible(mask);
    Battle_Ai_UpdateRouteGateAvailability();
    for (unsigned word = 0; word < 256; ++word) {
      CHECK(fixture.Word(word) ==
            (kFirst | (mask ? 0x80000000u : 0x80008000u)));
    }
  }
}

TEST_CASE("Route availability respects absent room auxiliary data",
          "[ai][route-availability]") {
  RouteAvailabilityFixture fixture;
  fixture.SetWord(1, 0x400);
  Battle_Ai_roomHasWaypoints = nullptr;
  Battle_Ai_routeGates = nullptr;
  Battle_Ai_UpdateRouteGateAvailability();
  CHECK(fixture.Word(1) == 0x400);
}

TEST_CASE("Route availability retains the original thirteen-point capacity",
          "[ai][route-availability]") {
  RouteAvailabilityFixture fixture;
  fixture.SetVisible(1u << 12);
  constexpr u_int kFirst = 0x400 | (3u << 5) | 7;
  fixture.SetWord(1, kFirst);
  Battle_Ai_UpdateRouteGateAvailability();
  CHECK(fixture.Word(1) == (kFirst | 0x80008000u));
  fixture.SetVisible(1u << 4);
  fixture.SetEnd(0, 3, 6);
  fixture.SetEnd(1, 3, 7);
  fixture.SetWord(1, kFirst | (2u << 13));
  Battle_Ai_UpdateRouteGateAvailability();
  CHECK(fixture.Word(1) == (kFirst | (2u << 13) | 0x80000000u));
}

TEST_CASE("Original route availability entry resolves to the native updater",
          "[ai][route-availability]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DC638) ==
        reinterpret_cast<void*>(&Battle_Ai_UpdateRouteGateAvailability));
}

TEST_CASE("Route availability rejects source stack-dependent input",
          "[.routegate-stack-input]") {
  VS::Platform::InstallCrashHandler();
  RouteAvailabilityFixture fixture;
  fixture.SetVisible(1);
  fixture.SetWord(0, 0x04000000);
  Battle_Ai_UpdateRouteGateAvailability();
  FAIL("Stack-register-dependent route input was accepted");
}

TEST_CASE("Route availability rejects a truncated room auxiliary section",
          "[.routegate-room-bound]") {
  VS::Platform::InstallCrashHandler();
  RouteAvailabilityFixture fixture;
  Field_roomData.header.section6Length = 0x497;
  Battle_Ai_UpdateRouteGateAvailability();
  FAIL("Truncated room route table was accepted");
}

TEST_CASE("Route availability rejects excess moving-platform output capacity",
          "[.routegate-platform-bound]") {
  VS::Platform::InstallCrashHandler();
  RouteAvailabilityFixture fixture;
  Field_roomData.header.movingPlatformsLength =
      14 * sizeof(Field_MpdMovingPlatform);
  Battle_Ai_UpdateRouteGateAvailability();
  FAIL("Moving-platform output beyond the original buffer was accepted");
}
