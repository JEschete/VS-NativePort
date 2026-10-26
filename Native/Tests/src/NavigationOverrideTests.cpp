#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
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
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Core/BiosRand.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
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

struct ScriptedOverrideFixture : NavigationOverrideFixture {
  Core_HeapBlockHeader saved_heap = Core_legacyHeapList;
  Core_HeapBlockHeader saved_secondary = Core_legacySecondaryHeapList;
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_actorMarkers)) saved_markers =
      std::to_array(Battle_actorMarkers);
  Battle_Ai_RoomAiState* saved_room = Battle_Ai_roomAiState;
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  decltype(Battle_Ai_specialTerrainHeights) saved_heights =
      Battle_Ai_specialTerrainHeights;
  void* saved_auxiliary = Battle_Ai_roomHasWaypoints;
  int saved_movement_random = Battle_Ai_savedMovementRandomState;
  std::array<Battle_Ai_DirectionDelta, 8> saved_directions =
      std::to_array(Battle_Ai_directionDeltas);
  std::array<Core_HeapBlockHeader, 2048> heap{};
  std::array<Battle_Actor, 2> actors{};
  Actors_ActorRecord ashley_model{};
  Battle_Ai_RoomAiState room{};
  u_short heights[32][32]{};
  unsigned free_units = 0;

  ScriptedOverrideFixture() {
    Core_Memory_InitializeHeap(heap.data(), sizeof heap);
    free_units = Core_legacyHeapList.next->blockSize;
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    Battle_actors[0] = &actors[0];
    Battle_actors[1] = &actors[1];
    actors[0].next = &actors[1];
    actors[0].commandState = 0xC0;
    actors[0].model = &ashley_model.instance;
    ashley_model.instance.position = {3000, -64, 3000, 0};
    ashley_model.occupancyRadius = 16;
    ashley_model.occupancyHeight = 128;
    Actors_actorRecords[0] = &ashley_model;
    actors[1].id = 1;
    actors[1].model = &model.instance;
    model.instance.slot = 1;
    model.instance.position = {1088, -64, 1088, 0x55AA};
    model.instance.currentTileX = model.instance.currentTileZ = 8;
    Actors_actorRecords[1] = &model;
    source.nativePointer18C = nullptr;
    source.actorId = 1;
    source.actor = reinterpret_cast<Battle_Ai_EngagementData*>(&actors[1]);
    source.model = reinterpret_cast<Battle_Ai_AiModelView*>(&model);
    source.nativePointer64 = &model.instance.currentTileX;
    source.radius = 16;
    source.walkStep = 32;
    source.runStep = 64;
    source.locomotionFlags = 0x42;
    Battle_Ai_actorAiStates[1] = &source;
    Battle_Ai_roomAiState = &room;
    Battle_Ai_roomHasWaypoints = nullptr;
    Battle_Ai_specialTerrainHeights = heights;
    Battle_Ai_savedMovementRandomState = 0x1234;
    room.maxTileX = room.minTileX = 31;
    const u_int dimensions = 0x2020;
    std::memcpy(room.reserved4, &dimensions, sizeof dimensions);
    const std::array<u_char, 8> directions{0x30, 0x33, 0x03, 0x13,
                                           0x10, 0x11, 0x01, 0x31};
    std::memcpy(Battle_Ai_directionDeltas, directions.data(),
                directions.size());
  }

  ~ScriptedOverrideFixture() {
    if (source.nativePointer18C != nullptr) {
      Core_Memory_FreeFromHighEnd(source.nativePointer18C);
    }
    std::ranges::copy(saved_actors, Battle_actors);
    std::ranges::copy(saved_markers, Battle_actorMarkers);
    std::ranges::copy(saved_directions, Battle_Ai_directionDeltas);
    Battle_Ai_roomAiState = saved_room;
    Battle_Ai_roomHasWaypoints = saved_auxiliary;
    Battle_Ai_specialTerrainHeights = saved_heights;
    Battle_Ai_savedMovementRandomState = saved_movement_random;
    *Battle_Ai_FrameState() = saved_frame;
    Core_legacyHeapList = saved_heap;
    Core_legacySecondaryHeapList = saved_secondary;
  }

  Battle_Ai_ActorAiState* ActiveRoute() {
    return static_cast<Battle_Ai_ActorAiState*>(source.nativePointer18C);
  }

  void AllocateCopiedRoute(unsigned target) {
    auto* record = static_cast<Battle_Ai_ActorAiState*>(
        Core_Memory_AllocateFromHighEnd(sizeof(Battle_Ai_ActorAiState)));
    REQUIRE(record != nullptr);
    std::construct_at(record);
    source.nativePointer18C = record;
    Battle_Ai_CopyActorNavigationOverrideState(1, record);
    record->scriptedTargetXZ = target;
  }

  void CheckPreparedRoute() {
    const auto* record = ActiveRoute();
    REQUIRE(record != nullptr);
    CHECK(record->movement_wait == 65534);
    CHECK(record->navigationFlags == 0xABCDFFDD);
    CHECK(record->opaque19A == 0);
    CHECK(record->ground_area == 0xB);
    CHECK(record->reserved154Area == 0xB);
  }

  void CheckPreparedFlags() {
    const auto* record = ActiveRoute();
    REQUIRE(record != nullptr);
    CHECK(record->ignore_floor_mask == 1);
    CHECK(record->opaque1 == 0);
    CHECK(record->isStuck == 0);
    CHECK(record->opaque3 == 0);
    CHECK(record->movementTarget == 16);
  }

  void CheckEngagementReleased() const {
    CHECK(room.engagedHead == nullptr);
    CHECK(room.engagedTail == nullptr);
    CHECK(room.engagedCount == 0);
    CHECK(Battle_actorMarkers[1] == 0);
  }

  void CheckCopiedCoordinates() {
    const auto* record = ActiveRoute();
    REQUIRE(record != nullptr);
    CHECK(record->tile.word == 0x00080108);
    CHECK(std::memcmp(&record->position, &model.instance.position,
                      sizeof record->position) == 0);
    CHECK(record->fine_x == 17);
    CHECK(record->fine_layer == 0);
    CHECK(record->fine_z == 17);
    CHECK(record->fine_status == 0);
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

TEST_CASE("Production scripted path request allocates a complete native route",
          "[ai][scripted-override][scripted-override-red]") {
  ScriptedOverrideFixture fixture;
  SVECTOR target{1200, 0, 1300, 0};
  Actors_BeginScriptedActorPath(1, &target);
  auto* route = fixture.ActiveRoute();
  REQUIRE(route != nullptr);
  const auto* header = static_cast<const Core_HeapBlockHeader*>(
                           fixture.source.nativePointer18C) -
                       1;
  REQUIRE(header->blockSize > 1);
  CHECK((header->blockSize - 1) * sizeof(*header) >= sizeof(*route));
  CHECK(route->actorId == 1);
  CHECK(route->scriptedTargetXZ == (1200u | (1300u << 16)));
  CHECK(route->nativePointer18C == route);
  CHECK(route->nativePointer64 == &fixture.model.instance.currentTileX);
  CHECK(Field_ActorCollisionBypass(1) == 2);
}

TEST_CASE(
    "Scripted route update completes a reached point and releases storage",
    "[ai][scripted-override][scripted-override-red]") {
  ScriptedOverrideFixture fixture;
  fixture.AllocateCopiedRoute(1088u | (1088u << 16));
  Battle_Ai_UpdateActorNavigationOverrides();
  CHECK(fixture.source.nativePointer18C == nullptr);
  CHECK(fixture.actors[1].commandState == 0x80);
  CHECK(fixture.actors[1].commandOpcode == 2);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE(
    "Scripted route creation unlinks engagement and preserves native aliases",
    "[ai][scripted-override]") {
  ScriptedOverrideFixture fixture;
  fixture.room.engagementLimit = 1;
  const auto saved_busy = Battle_Ai_candidateEngagedThisUpdate;
  Battle_Ai_candidateEngagedThisUpdate = 0;
  const auto engaged = Battle_Ai_TryEngageActor(&fixture.source);
  Battle_Ai_candidateEngagedThisUpdate = saved_busy;
  REQUIRE(engaged == 1);
  Battle_actorMarkers[1] = static_cast<char>(0x85);
  fixture.source.currentCandidate = &fixture.source.candidates[0];
  const u_int preserved = 0x11223344;
  std::memcpy(fixture.source.reserved160 + 4, &preserved, sizeof preserved);
  Battle_Ai_SetActorNavigationOverride(1, 1200, 1300);
  const auto* record = fixture.ActiveRoute();
  REQUIRE(record != nullptr);
  CHECK(record->currentCandidate == &fixture.source.candidates[0]);
  CHECK(record->actor == fixture.source.actor);
  fixture.CheckEngagementReleased();
  u_int actual;
  std::memcpy(&actual, record->reserved160 + 4, sizeof actual);
  CHECK(actual == preserved);
  Battle_Ai_FreeActorNavigationOverride(1);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE(
    "Repeated scripted path requests retain the source base-route distinction",
    "[ai][scripted-override]") {
  ScriptedOverrideFixture fixture;
  Battle_Ai_SetActorNavigationOverride(1, 1200, 1300);
  auto* first = fixture.ActiveRoute();
  REQUIRE(first != nullptr);
  const auto units = Core_legacyHeapList.next->blockSize;
  Battle_Ai_SetActorNavigationOverride(1, 1400, 1500);
  CHECK(fixture.ActiveRoute() == first);
  CHECK(first->scriptedTargetXZ == (1200u | (1300u << 16)));
  CHECK(fixture.source.scriptedTargetXZ == (1400u | (1500u << 16)));
  CHECK(fixture.source.settingsBit14 == 3);
  CHECK(Core_legacyHeapList.next->blockSize == units);
}

TEST_CASE("Scripted path packing preserves every halfword and signed high bits",
          "[ai][scripted-override]") {
  ScriptedOverrideFixture fixture;
  const auto random = Core_Random_State();
  for (unsigned value = 0; value < 65536; ++value) {
    Battle_Ai_SetActorNavigationOverride(1, static_cast<int>(value), 0x1234);
    const auto* changed = value == 0 ? fixture.ActiveRoute() : &fixture.source;
    REQUIRE(changed != nullptr);
    CHECK(changed->scriptedTargetXZ == (value | 0x12340000u));
    Battle_Ai_SetActorNavigationOverride(1, 0x5678, static_cast<int>(value));
    CHECK(fixture.source.scriptedTargetXZ == (0x5678u | (value << 16)));
  }
  Battle_Ai_SetActorNavigationOverride(1, -1, 0x1234);
  CHECK(fixture.source.scriptedTargetXZ == 0xFFFFFFFFu);
  Battle_Ai_SetActorNavigationOverride(1, 0x1234, -1);
  CHECK(fixture.source.scriptedTargetXZ == 0xFFFF1234u);
  CHECK(Core_Random_State() == random);
}

TEST_CASE("Scripted path requests retain missing map and AI-state gates",
          "[ai][scripted-override]") {
  ScriptedOverrideFixture fixture;
  Battle_actorMarkers[1] = static_cast<char>(0x85);
  Battle_Ai_tileFlagMap = nullptr;
  Battle_Ai_SetActorNavigationOverride(1, 1200, 1300);
  CHECK(fixture.source.nativePointer18C == nullptr);
  Battle_Ai_tileFlagMap = fixture.grid;
  Battle_Ai_actorAiStates[1] = nullptr;
  Battle_Ai_SetActorNavigationOverride(1, 1200, 1300);
  CHECK(fixture.source.nativePointer18C == nullptr);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[1]) == 0x85);
}

TEST_CASE("Scripted route updates preserve strict source arrival boundaries",
          "[ai][scripted-override]") {
  constexpr std::array<std::array<int, 2>, 6> kDeltas{
      {{0, 0}, {31, 0}, {32, 0}, {0, 32}, {22, 22}, {23, 23}}};
  for (const auto& delta : kDeltas) {
    ScriptedOverrideFixture fixture;
    const auto target = static_cast<unsigned>(1088 + delta[0]) |
                        (static_cast<unsigned>(1088 + delta[1]) << 16);
    fixture.AllocateCopiedRoute(target);
    Battle_Ai_UpdateActorNavigationOverrides();
    const bool reached = delta[0] * delta[0] + delta[1] * delta[1] < 1024;
    CHECK((fixture.source.nativePointer18C == nullptr) == reached);
    if (!reached) {
      CHECK(fixture.ActiveRoute()->moveX == 1088 + delta[0]);
      CHECK(fixture.ActiveRoute()->moveZ == 1088 + delta[1]);
    }
    CHECK(fixture.actors[0].next == &fixture.actors[1]);
  }
}

TEST_CASE(
    "Scripted route preparation preserves countdowns map areas and caches",
    "[ai][scripted-override]") {
  ScriptedOverrideFixture fixture;
  fixture.AllocateCopiedRoute(1200u | (1300u << 16));
  auto* record = fixture.ActiveRoute();
  record->movement_wait = 65535;
  record->navigationFlags = 0xABCDFFFD;
  record->opaque19A = record->opaque1 = record->isStuck = record->opaque3 =
      0xA5;
  record->route_gate = 1;
  fixture.model.instance.currentTileLayer = 1;
  fixture.grid[8][8] = 0xAB;
  auto& frame = *Battle_Ai_FrameState();
  frame.directionCache[0] = 0xFFFFFFFF;
  frame.directionCache[1] = 0x12345678;
  frame.directionCache[2] = 0xA5B6C7D8;
  Battle_Ai_UpdateActorNavigationOverrides();
  fixture.CheckPreparedRoute();
  fixture.CheckPreparedFlags();
  fixture.CheckCopiedCoordinates();
  CHECK(frame.directionCache[0] == 0);
  CHECK(frame.directionCache[1] == 0x12345678);
  CHECK(frame.directionCache[2] == 0xA5B6C7D8);
  CHECK(Battle_Ai_savedMovementRandomState == 0x1234);
}

TEST_CASE("Scripted route updates skip suspended and missing-AI actors safely",
          "[ai][scripted-override]") {
  const auto flags = GENERATE(0x40, 0x80, 0xC0);
  ScriptedOverrideFixture fixture;
  fixture.AllocateCopiedRoute(1088u | (1088u << 16));
  const auto before = Snapshot(*fixture.ActiveRoute());
  fixture.actors[1].commandState = static_cast<u_char>(flags);
  Battle_Ai_FrameState()->lastBudgetTick = 0x1234;
  Battle_Ai_UpdateActorNavigationOverrides();
  CHECK(Snapshot(*fixture.ActiveRoute()) == before);
  CHECK(Battle_Ai_FrameState()->lastBudgetTick == 0x1234);
  fixture.actors[1].commandState = 0;
  Battle_Ai_actorAiStates[1] = nullptr;
  Battle_Ai_UpdateActorNavigationOverrides();
  CHECK(Snapshot(*fixture.ActiveRoute()) == before);
  CHECK(fixture.actors[0].next == &fixture.actors[1]);
}

TEST_CASE("Non-ground scripted arrival ignores layer and status metadata",
          "[ai][scripted-override]") {
  const auto mode = GENERATE(1, 2);
  ScriptedOverrideFixture fixture;
  fixture.AllocateCopiedRoute(1200u | (1300u << 16));
  fixture.ActiveRoute()->locomotionMode = static_cast<u_char>(mode);
  fixture.ActiveRoute()->destination.word = 0xA5080308;
  fixture.model.instance.currentTileLayer = 5;
  Battle_Ai_UpdateActorNavigationOverrides();
  CHECK(fixture.source.nativePointer18C == nullptr);
  CHECK(fixture.actors[1].commandOpcode == 2);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE("Scripted arrival retains signed source square-sum behavior",
          "[ai][scripted-override]") {
  const std::int16_t position = GENERATE(-1, -32768);
  ScriptedOverrideFixture fixture;
  fixture.model.instance.position.vx = fixture.model.instance.position.vz =
      position;
  fixture.AllocateCopiedRoute(0);
  Battle_Ai_UpdateActorNavigationOverrides();
  CHECK(fixture.source.nativePointer18C == nullptr);
  CHECK(fixture.actors[1].commandOpcode == 2);
}

TEST_CASE(
    "Original scripted route entries resolve to native lifecycle functions",
    "[ai][scripted-override]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E527C) ==
        reinterpret_cast<void*>(&Battle_Ai_SetActorNavigationOverride));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E5308) ==
        reinterpret_cast<void*>(&Battle_Ai_UpdateActorNavigationOverrides));
}

TEST_CASE("Scripted path requests reject mismatched actor ownership",
          "[.override-actor-identity]") {
  VS::Platform::InstallCrashHandler();
  ScriptedOverrideFixture fixture;
  Battle_actors[1] = nullptr;
  Battle_Ai_SetActorNavigationOverride(1, 1200, 1300);
  FAIL("Mismatched path-request identity was accepted");
}

TEST_CASE("Active scripted routes require their native map",
          "[.override-missing-map]") {
  VS::Platform::InstallCrashHandler();
  ScriptedOverrideFixture fixture;
  fixture.AllocateCopiedRoute(1088u | (1088u << 16));
  Battle_Ai_tileFlagMap = nullptr;
  Battle_Ai_UpdateActorNavigationOverrides();
  FAIL("An active path without a native map was accepted");
}

TEST_CASE("Scripted navigation rejects cyclic native actor lists",
          "[.override-cyclic-list]") {
  VS::Platform::InstallCrashHandler();
  ScriptedOverrideFixture fixture;
  fixture.actors[1].commandState = 0xC0;
  fixture.actors[1].next = &fixture.actors[1];
  Battle_Ai_UpdateActorNavigationOverrides();
  FAIL("A cyclic actor list was accepted");
}
