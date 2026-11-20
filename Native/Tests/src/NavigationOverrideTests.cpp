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

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/Movement.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickResourcesLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Legacy/Psx.hpp"
#include "Platform/CrashHandler.hpp"

using VS::Core::Gte::SVECTOR;

static_assert(
    std::is_same_v<decltype(&VS::Battle::Ai::HasLiveActorNavigationState),
                   int (*)(int)>);
static_assert(
    std::is_same_v<decltype(&VS::Battle::Ai::LiveActorNavigationOverrideStatus),
                   int (*)(int)>);
static_assert(std::is_same_v<
              decltype(&VS::Battle::Ai::CopyLiveActorNavigationOverrideState),
              void (*)(int, VS::Battle::Ai::ActorAiState*)>);
static_assert(std::is_same_v<decltype(&VS::Battle::Ai::AddLiveAiTileOccupant),
                             void (*)(const VS::Field::GimmickInstance*)>);
static_assert(
    std::is_same_v<decltype(&VS::Battle::Ai::RemoveLiveAiTileOccupant),
                   void (*)(const VS::Field::GimmickInstance*)>);
static_assert(offsetof(VS::Field::GimmickPhysicsView, tile_x) ==
              offsetof(VS::Field::GimmickInstance, tile_x));
static_assert(offsetof(VS::Field::GimmickPhysicsView, tile_z) ==
              offsetof(VS::Field::GimmickInstance, tile_z));

namespace {
struct NavigationOverrideFixture {
  decltype(VS::Battle::Ai::actorAiStates) saved_states =
      VS::Battle::Ai::actorAiStates;
  decltype(VS::Actors::actorRecords) saved_models = VS::Actors::actorRecords;
  decltype(VS::Battle::Ai::tileFlagMap) saved_grid =
      VS::Battle::Ai::tileFlagMap;
  std::uint16_t grid[32][32]{};
  VS::Battle::Ai::ActorAiState source{};
  VS::Battle::Ai::ActorAiState route{};
  VS::Battle::Ai::TilePosition tile{};
  VS::Actors::ActorRecord model{};

  NavigationOverrideFixture() {
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), 16, nullptr);
    VS::Battle::Ai::tileFlagMap = grid;
    source.native_pointer_18c = &route;
    route.native_pointer64 = &tile;
  }

  ~NavigationOverrideFixture() {
    std::copy(saved_states.begin(), saved_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
    std::copy(saved_models.begin(), saved_models.end(),
              VS::Actors::actorRecords.begin());
    VS::Battle::Ai::tileFlagMap = saved_grid;
  }

  static void CheckCellValue(std::uint16_t actual, std::uint16_t expected) {
    CHECK(actual == expected);
  }

  void CheckOtherCells(int x, int z, std::uint16_t expected) const {
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
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  decltype(VS::Battle::actorMarkers) saved_markers = VS::Battle::actorMarkers;
  VS::Battle::Ai::RoomAiState* saved_room = VS::Battle::Ai::roomAiState;
  VS::Battle::Ai::FrameContext saved_frame = VS::Battle::Ai::LiveFrameContext();
  decltype(VS::Battle::Ai::specialTerrainHeights) saved_heights =
      VS::Battle::Ai::specialTerrainHeights;
  void* saved_auxiliary = VS::Battle::Ai::roomAuxiliaryData;
  int saved_movement_random = VS::Battle::Ai::savedMovementRandomState;
  std::array<VS::Battle::Ai::DirectionDelta, 8> saved_directions =
      VS::Battle::Ai::directionDeltas;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 3072> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<VS::Battle::Actor, 2> actors{};
  VS::Actors::ActorRecord ashley_model{};
  VS::Battle::Ai::RoomAiState room{};
  std::uint16_t heights[32][32]{};

  ScriptedOverrideFixture() {
    std::fill_n(VS::Battle::actors.begin(), saved_actors.size(), nullptr);
    VS::Battle::actors[0] = &actors[0];
    VS::Battle::actors[1] = &actors[1];
    actors[0].next = &actors[1];
    actors[0].command_state = 0xC0;
    actors[0].model = &ashley_model.instance;
    ashley_model.instance.position = {3000, -64, 3000, 0};
    ashley_model.occupancy_radius = 16;
    ashley_model.occupancy_height = 128;
    VS::Actors::actorRecords[0] = &ashley_model;
    actors[1].id = 1;
    actors[1].model = &model.instance;
    model.instance.slot = 1;
    model.instance.position = {1088, -64, 1088, 0x55AA};
    model.instance.current_tile_x = model.instance.current_tile_z = 8;
    VS::Actors::actorRecords[1] = &model;
    source.native_pointer_18c = nullptr;
    source.actor_id = 1;
    source.actor =
        reinterpret_cast<VS::Battle::Ai::EngagementData*>(&actors[1]);
    source.model =
        reinterpret_cast<VS::Battle::Ai::ModelStatePrefix*>(&model.instance);
    source.native_pointer64 = &model.instance.current_tile_x;
    source.radius = 16;
    source.walk_step = 32;
    source.run_step = 64;
    source.locomotion_flags = 0x42;
    VS::Battle::Ai::actorAiStates[1] = &source;
    VS::Battle::Ai::roomAiState = &room;
    VS::Battle::Ai::StoreEngagedMask(room, 0);
    room.engaged_count = 0;
    VS::Battle::Ai::roomAuxiliaryData = nullptr;
    VS::Battle::Ai::specialTerrainHeights = heights;
    VS::Battle::Ai::savedMovementRandomState = 0x1234;
    room.max_tile_x = room.min_tile_x = 31;
    const std::uint32_t dimensions = 0x2020;
    std::memcpy(room.reserved4.data(), &dimensions, sizeof dimensions);
    const std::array<std::uint8_t, 8> directions{0x30, 0x33, 0x03, 0x13,
                                                 0x10, 0x11, 0x01, 0x31};
    std::memcpy(VS::Battle::Ai::directionDeltas.data(), directions.data(),
                directions.size());
  }

  ~ScriptedOverrideFixture() {
    if (source.native_pointer_18c != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(source.native_pointer_18c);
    }
    std::ranges::copy(saved_actors, VS::Battle::actors.begin());
    std::ranges::copy(saved_markers, VS::Battle::actorMarkers.begin());
    std::ranges::copy(saved_directions,
                      VS::Battle::Ai::directionDeltas.begin());
    VS::Battle::Ai::roomAiState = saved_room;
    VS::Battle::Ai::roomAuxiliaryData = saved_auxiliary;
    VS::Battle::Ai::specialTerrainHeights = saved_heights;
    VS::Battle::Ai::savedMovementRandomState = saved_movement_random;
    VS::Battle::Ai::LiveFrameContext() = saved_frame;
  }

  VS::Battle::Ai::ActorAiState* ActiveRoute() {
    return static_cast<VS::Battle::Ai::ActorAiState*>(
        source.native_pointer_18c);
  }

  void AllocateCopiedRoute(unsigned target) {
    auto* record = static_cast<VS::Battle::Ai::ActorAiState*>(
        VS::Core::Memory::AllocateLiveFromHighEnd(
            sizeof(VS::Battle::Ai::ActorAiState)));
    REQUIRE(record != nullptr);
    std::construct_at(record);
    source.native_pointer_18c = record;
    VS::Battle::Ai::CopyLiveActorNavigationOverrideState(1, record);
    record->scripted_target_xz = target;
  }

  void CheckPreparedRoute() {
    const auto* record = ActiveRoute();
    REQUIRE(record != nullptr);
    CHECK(record->movement_wait == 65534);
    CHECK(record->navigation_flags == 0xABCDFFDD);
    CHECK(record->opaque_19a == 0);
    CHECK(record->ground_area == 0xB);
    CHECK(record->reserved154_area == 0xB);
  }

  void CheckPreparedFlags() {
    const auto* record = ActiveRoute();
    REQUIRE(record != nullptr);
    CHECK(record->ignore_floor_mask == 1);
    CHECK(record->opaque1 == 0);
    CHECK(record->is_stuck == 0);
    CHECK(record->opaque3 == 0);
    CHECK(record->movement_target == 16);
  }

  void CheckEngagementReleased() const {
    CHECK(room.engaged_head == nullptr);
    CHECK(room.engaged_tail == nullptr);
    CHECK(room.engaged_count == 0);
    CHECK(VS::Battle::actorMarkers[1] == 0);
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
  decltype(VS::Battle::Ai::specialTerrainHeights) saved_heights =
      VS::Battle::Ai::specialTerrainHeights;
  decltype(VS::Field::gimmickInstances) saved_gimmicks =
      VS::Field::gimmickInstances;
  std::uint16_t heights[32][32]{};
  VS::Field::GimmickInstance gimmick{};
  VS::Field::GimmickInstance sibling{};

  GimmickOccupancyFixture() {
    VS::Battle::Ai::specialTerrainHeights = heights;
    std::fill_n(VS::Field::gimmickInstances.begin(), saved_gimmicks.size(),
                nullptr);
    gimmick.tile_x = 13;
    gimmick.tile_z = 7;
    gimmick.bone_count = 1;
    gimmick.object_info.object_id = 1;
    sibling.object_info.object_id = 1;
    VS::Field::gimmickInstances[0] = &gimmick;
    VS::Field::gimmickInstances[1] = &sibling;
  }

  ~GimmickOccupancyFixture() {
    VS::Battle::Ai::specialTerrainHeights = saved_heights;
    std::ranges::copy(saved_gimmicks, VS::Field::gimmickInstances.begin());
  }
};

struct RouteAvailabilityFixture {
  void* saved_auxiliary = VS::Battle::Ai::roomAuxiliaryData;
  VS::Battle::Ai::RouteGateEndRecord* saved_gates = VS::Battle::Ai::routeGates;
  VS::Field::RoomDataRecord saved_room = VS::Field::roomData;
  std::array<std::uint8_t, 13> saved_bounce{};
  alignas(std::uint32_t) std::array<std::uint8_t, 0x498> auxiliary{};
  std::array<VS::Field::MpdMovingPlatform, 13> platforms{};

  RouteAvailabilityFixture() {
    std::ranges::copy(
        std::span(VS::Field::platformBounceFrames.data(), saved_bounce.size()),
        saved_bounce.begin());
    std::fill_n(VS::Field::platformBounceFrames.begin(), saved_bounce.size(),
                0);
    VS::Field::roomData = {};
    VS::Field::roomData.section6 = auxiliary.data();
    VS::Field::roomData.header.section6_length = auxiliary.size();
    VS::Field::roomData.moving_platforms = platforms.data();
    VS::Field::roomData.header.moving_platforms_length = sizeof platforms;
    VS::Battle::Ai::roomAuxiliaryData = auxiliary.data();
    VS::Battle::Ai::routeGates =
        reinterpret_cast<VS::Battle::Ai::RouteGateEndRecord*>(auxiliary.data() +
                                                              0x454);
    SetEnd(0, 3, 7);
  }

  ~RouteAvailabilityFixture() {
    std::ranges::copy(saved_bounce, VS::Field::platformBounceFrames.begin());
    VS::Field::roomData = saved_room;
    VS::Battle::Ai::roomAuxiliaryData = saved_auxiliary;
    VS::Battle::Ai::routeGates = saved_gates;
  }

  void SetEnd(std::size_t pair, unsigned x, unsigned z) {
    VS::Battle::Ai::RouteGateEndRecord end{};
    end.tile_x = static_cast<std::uint16_t>(x);
    end.tile_z = static_cast<std::uint16_t>(z);
    std::memcpy(auxiliary.data() + 0x454 + pair * 4, &end, sizeof end);
  }

  void SetVisible(unsigned mask) {
    for (std::size_t index = 0; index < platforms.size(); ++index) {
      platforms[index].is_hidden = (mask & (1u << index)) == 0;
    }
  }

  void SetWord(std::size_t index, std::uint32_t word) {
    std::memcpy(auxiliary.data() + index * sizeof(word), &word, sizeof(word));
  }

  std::uint32_t Word(std::size_t index) const {
    std::uint32_t word;
    std::memcpy(&word, auxiliary.data() + index * sizeof(word), sizeof(word));
    return word;
  }
};
auto Snapshot(const VS::Battle::Ai::ActorAiState& state) {
  std::array<std::byte, sizeof(state)> bytes{};
  std::memcpy(bytes.data(), &state, bytes.size());
  return bytes;
}

void SeedCopyState(VS::Battle::Ai::ActorAiState& state) {
  state.reserved0 = 0x12;
  state.movement_enabled = -1;
  state.settings_bit14 = 0xAB;
  state.actor_id = 99;
  state.reserved_9c[19] = 0x34;
  state.step_height_bytes[0] = 0xA5;
  state.step_height_bytes[1] = 0xA5;
  state.reserved_b2 = 0xA5;
  state.max_range = 0xCD;
  state.destination.word = 0xFEDCBA98;
  state.navigation_flags = 0xFFFFFFFF;
  state.opaque_19a = 0xAB;
  state.reserved_19b[0] = 0x5A;
  state.candidate_count = 16;
  state.candidates[15].target_id = 7;
  state.candidate_order[15] = &state.candidates[15];
  state.current_candidate = &state.candidates[0];
  state.reaction_pending = 0xEF;
}

void SetExpectedOverrideFields(VS::Battle::Ai::ActorAiState& state,
                               int actor_id) {
  state.actor_id = static_cast<std::uint8_t>(actor_id);
  state.step_height_bytes[0] = 0x40;
  state.step_height_bytes[1] = 1;
  state.reserved_b2 = 3;
  state.navigation_flags = 0;
  state.opaque_19a = 0;
  state.settings_bit14 = 3;
  state.movement_enabled = 1;
}
}  // namespace

TEST_CASE(
    "Navigation presence and override status preserve missing-state gates",
    "[ai][navigation][override]") {
  const bool grid = GENERATE(false, true);
  const bool state = GENERATE(false, true);
  NavigationOverrideFixture fixture;
  if (!grid) VS::Battle::Ai::tileFlagMap = nullptr;
  fixture.source.native_pointer_18c = nullptr;
  for (int actor_id = 0; actor_id < 16; ++actor_id) {
    VS::Battle::Ai::actorAiStates[actor_id] = state ? &fixture.source : nullptr;
    CHECK(VS::Battle::Ai::HasLiveActorNavigationState(actor_id) ==
          (grid && state));
    CHECK(VS::Battle::Ai::LiveActorNavigationOverrideStatus(actor_id) == 2);
  }
}

TEST_CASE("Navigation override status compares only horizontal target bytes",
          "[ai][navigation][override]") {
  const unsigned ignored = GENERATE(0u, 0x12u, 0xFFu);
  const int actor_id = GENERATE(0, 7, 15);
  NavigationOverrideFixture fixture;
  VS::Battle::Ai::actorAiStates[actor_id] = &fixture.source;
  fixture.tile.word = 0xABCD12EF;
  fixture.route.destination.word =
      (ignored << 24) | 0x00CD00EF | (ignored << 8);
  CHECK(VS::Battle::Ai::LiveActorNavigationOverrideStatus(actor_id) == 0);
  fixture.route.destination.word ^= 1;
  CHECK(VS::Battle::Ai::LiveActorNavigationOverrideStatus(actor_id) == 1);
  fixture.route.destination.word ^= 0x10001;
  CHECK(VS::Battle::Ai::LiveActorNavigationOverrideStatus(actor_id) == 1);
}

TEST_CASE(
    "Navigation override copy preserves full native state and borrowed "
    "pointers",
    "[ai][navigation][override]") {
  const int actor_id = GENERATE(0, 7, 15);
  const bool self = GENERATE(false, true);
  NavigationOverrideFixture fixture;
  SeedCopyState(fixture.source);
  fixture.source.native_pointer60 = &fixture.source;
  fixture.source.native_pointer64 = &fixture.tile;
  fixture.source.native_pointer68 = &fixture.route;
  fixture.source.next_engaged = &fixture.route;
  const auto before = Snapshot(fixture.source);
  auto expected = fixture.source;
  SetExpectedOverrideFields(expected, actor_id);
  struct {
    std::uint64_t before = 0x123456789ABCDEF0;
    VS::Battle::Ai::ActorAiState state{};
    std::uint64_t after = 0xFEDCBA9876543210;
  } guarded;
  auto* output = self ? &fixture.source : &guarded.state;
  VS::Battle::Ai::actorAiStates[actor_id] = &fixture.source;
  VS::Battle::Ai::tileFlagMap = nullptr;
  VS::Battle::Ai::CopyLiveActorNavigationOverrideState(actor_id, output);
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
  VS::Battle::Ai::actorAiStates[4] = &fixture.source;
  SeedCopyState(fixture.source);
  VS::Battle::Ai::CopyLiveActorNavigationOverrideState(4, &fixture.route);
  fixture.route.navigation_flags = 0xFFFFFFFF;
  fixture.route.opaque_19a = 0xFF;
  fixture.source.destination.word = 0x12345678;
  fixture.source.candidate_count = 2;
  auto expected = fixture.source;
  SetExpectedOverrideFields(expected, 4);
  VS::Battle::Ai::CopyLiveActorNavigationOverrideState(4, &fixture.route);
  CHECK(Snapshot(fixture.route) == Snapshot(expected));
}

TEST_CASE("Dynamic navigation marking preserves every source flag word",
          "[ai][navigation][tile-marker]") {
  const int x = GENERATE(0, 31);
  const int z = GENERATE(0, 31);
  NavigationOverrideFixture fixture;
  constexpr std::uint16_t kCanary = 0xA15A;
  for (auto& row : fixture.grid) std::ranges::fill(row, kCanary);
  for (unsigned word = 0; word < 65536; ++word) {
    fixture.grid[z][x] = static_cast<std::uint16_t>(word);
    VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(x, z);
    REQUIRE(fixture.grid[z][x] == (word | 0x400u));
    VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(x, z);
    CHECK(fixture.grid[z][x] == (word | 0x400u));
  }
  fixture.CheckOtherCells(x, z, kCanary);
}

TEST_CASE("Dynamic navigation marking reaches every native grid cell",
          "[ai][navigation][tile-marker]") {
  NavigationOverrideFixture fixture;
  for (int z = 0; z < 32; ++z) {
    for (int x = 0; x < 32; ++x) {
      VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(x, z);
      CHECK(fixture.grid[z][x] == 0x400);
    }
  }
}

TEST_CASE("Dynamic navigation marking follows grid availability and rebinding",
          "[ai][navigation][tile-marker]") {
  NavigationOverrideFixture fixture;
  fixture.grid[2][1] = 3;
  VS::Battle::Ai::tileFlagMap = nullptr;
  VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(-1, -1);
  VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(
      std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
  CHECK(fixture.grid[2][1] == 3);
  VS::Battle::Ai::tileFlagMap = fixture.grid;
  VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(1, 2);
  CHECK(fixture.grid[2][1] == 0x403);
  std::uint16_t second[32][32]{};
  second[2][1] = 0xA0;
  VS::Battle::Ai::tileFlagMap = second;
  VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(1, 2);
  CHECK(second[2][1] == 0x4A0);
  CHECK(fixture.grid[2][1] == 0x403);
}

TEST_CASE(
    "Scripted path completion forwards the actor and clears only reached "
    "bypass",
    "[ai][navigation][override]") {
  const int status = GENERATE(0, 1, 2);
  NavigationOverrideFixture fixture;
  VS::Battle::Ai::actorAiStates[7] = &fixture.source;
  VS::Actors::actorRecords[7] = &fixture.model;
  fixture.model.bypass_actor_collision = 1;
  fixture.model.ignored_by_collision = 1;
  fixture.tile.word = 0xABCD12EF;
  fixture.route.destination.word = fixture.tile.word ^ (status == 1 ? 1u : 0u);
  if (status == 2) fixture.source.native_pointer_18c = nullptr;
  CHECK(VS::Actors::IsLiveScriptedActorPathFinished(7) == (status != 1));
  CHECK(VS::Field::LiveActorCollisionBypass(7) == (status == 0 ? 1 : 3));
}

TEST_CASE("Gimmick occupancy adjusts the height grid instead of obstacle flags",
          "[ai][navigation][gimmick-occupancy][occupancy-red]") {
  GimmickOccupancyFixture fixture;
  fixture.heights[7][13] = 5;
  fixture.grid[7][13] = 0xA15A;
  VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
  CHECK(fixture.heights[7][13] == 0x405);
  CHECK(fixture.grid[7][13] == 0xA15A);
  VS::Battle::Ai::RemoveLiveAiTileOccupant(&fixture.gimmick);
  CHECK(fixture.heights[7][13] == 5);
  CHECK(fixture.grid[7][13] == 0xA15A);
}

TEST_CASE("Gimmick occupancy preserves source halfword wrap for every value",
          "[ai][navigation][gimmick-occupancy]") {
  GimmickOccupancyFixture fixture;
  for (unsigned word = 0; word < 65536; ++word) {
    fixture.heights[7][13] = static_cast<std::uint16_t>(word);
    VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
    REQUIRE(fixture.heights[7][13] == static_cast<std::uint16_t>(word + 0x400));
    VS::Battle::Ai::RemoveLiveAiTileOccupant(&fixture.gimmick);
    CHECK(fixture.heights[7][13] == word);
    VS::Battle::Ai::RemoveLiveAiTileOccupant(&fixture.gimmick);
    CHECK(fixture.heights[7][13] == static_cast<std::uint16_t>(word - 0x400));
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
      const auto index = offset / sizeof(std::uint16_t);
      fixture.gimmick.tile_x = static_cast<char>(x);
      fixture.gimmick.tile_z = 3;
      fixture.gimmick.tile_layer = static_cast<char>(layer);
      fixture.gimmick.reserved5f = static_cast<char>(0xFF);
      auto& cell = fixture.heights[index / 32][index % 32];
      cell = 0xA15A;
      VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
      CHECK(cell == 0xA55A);
      VS::Battle::Ai::RemoveLiveAiTileOccupant(&fixture.gimmick);
      CHECK(cell == 0xA15A);
    }
  }
}

TEST_CASE("Gimmick occupancy follows room map absence and rebinding",
          "[ai][navigation][gimmick-occupancy]") {
  GimmickOccupancyFixture fixture;
  VS::Battle::Ai::tileFlagMap = nullptr;
  VS::Battle::Ai::specialTerrainHeights = nullptr;
  VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
  VS::Battle::Ai::RemoveLiveAiTileOccupant(&fixture.gimmick);
  VS::Battle::Ai::tileFlagMap = fixture.grid;
  VS::Battle::Ai::specialTerrainHeights = fixture.heights;
  VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
  std::uint16_t second[32][32]{};
  VS::Battle::Ai::specialTerrainHeights = second;
  VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
  VS::Battle::Ai::RemoveLiveAiTileOccupant(&fixture.gimmick);
  CHECK(second[7][13] == 0);
  CHECK(fixture.heights[7][13] == 0x400);
}

TEST_CASE("Gimmick occupancy reaches every cell of the bounded native map",
          "[ai][navigation][gimmick-occupancy]") {
  GimmickOccupancyFixture fixture;
  for (int z = 0; z < 32; ++z) {
    for (int x = 0; x < 32; ++x) {
      fixture.gimmick.tile_x = static_cast<char>(x);
      fixture.gimmick.tile_z = static_cast<char>(z);
      fixture.gimmick.tile_layer = 3;
      fixture.heights[z][x] = 0x123;
      VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
      REQUIRE(fixture.heights[z][x] == 0x523);
      VS::Battle::Ai::RemoveLiveAiTileOccupant(&fixture.gimmick);
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
    VS::Field::gimmickInstances[0] = &fixture.gimmick;
    VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
    CHECK(fixture.heights[7][13] == 0x488);
    CHECK(VS::Field::ReleaseLiveGimmickInstance(0) == 0);
    CHECK(VS::Field::gimmickInstances[0] == nullptr);
    CHECK(fixture.heights[7][13] == 0x88);
    CHECK(VS::Field::gimmickInstances[1] == &fixture.sibling);
  }
}

TEST_CASE("Original gimmick occupancy entries resolve to native functions",
          "[ai][navigation][gimmick-occupancy][occupancy-binding]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E6898) ==
        reinterpret_cast<void*>(&VS::Battle::Ai::AddLiveAiTileOccupant));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E68A0) ==
        reinterpret_cast<void*>(&VS::Battle::Ai::RemoveLiveAiTileOccupant));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E6EB0) ==
        reinterpret_cast<void*>(
            &VS::Battle::Ai::ReleaseLiveActorAiContactOccupancy));
}

TEST_CASE("Gimmick occupancy rejects an unaligned source halfword",
          "[.occupancy-unaligned]") {
  VS::Platform::InstallCrashHandler();
  GimmickOccupancyFixture fixture;
  fixture.gimmick.tile_layer = 4;
  VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
  FAIL("Unaligned occupancy placement was accepted");
}

TEST_CASE("Gimmick occupancy rejects the first byte beyond its map",
          "[.occupancy-bounds]") {
  VS::Platform::InstallCrashHandler();
  GimmickOccupancyFixture fixture;
  fixture.gimmick.tile_x = fixture.gimmick.tile_z = 31;
  fixture.gimmick.tile_layer = 8;
  VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
  FAIL("Out-of-bounds occupancy placement was accepted");
}

TEST_CASE("Gimmick occupancy requires the active native height map",
          "[.occupancy-height-map]") {
  VS::Platform::InstallCrashHandler();
  GimmickOccupancyFixture fixture;
  VS::Battle::Ai::specialTerrainHeights = nullptr;
  VS::Battle::Ai::AddLiveAiTileOccupant(&fixture.gimmick);
  FAIL("Missing active occupancy map was accepted");
}

TEST_CASE("Route availability connects both endpoints to visible platforms",
          "[ai][route-availability][route-availability-red]") {
  RouteAvailabilityFixture fixture;
  fixture.SetVisible(1);
  constexpr std::uint32_t kFirst = 0x400 | (3u << 5) | 7;
  constexpr std::uint32_t kSecond = 0x04000000 | (3u << 21) | (7u << 16);
  fixture.SetWord(1, kFirst);
  fixture.SetWord(2, kSecond);
  fixture.SetWord(3, kFirst | kSecond);
  VS::Battle::Ai::UpdateLiveRouteGateAvailability();
  CHECK(fixture.Word(1) == (kFirst | 0x80000000u));
  CHECK(fixture.Word(2) == (kSecond | 0x8000));
  CHECK(fixture.Word(3) == (kFirst | kSecond));
}

TEST_CASE("Route availability preserves every platform mask and selector",
          "[ai][route-availability]") {
  RouteAvailabilityFixture fixture;
  constexpr std::uint32_t kFirst = 0x400 | (3u << 5) | 7;
  constexpr std::uint32_t kSecond = 0x04000000 | (3u << 21) | (7u << 16);
  for (unsigned mask = 0; mask < 16; ++mask) {
    fixture.SetVisible(mask);
    for (unsigned selector = 0; selector < 4; ++selector) {
      const auto first = kFirst | (selector << 13);
      const auto second = kSecond | (selector << 29);
      fixture.SetWord(1, first);
      fixture.SetWord(2, second);
      fixture.SetWord(3, first | second);
      fixture.SetWord(4, 0xA31D51A5u & ~0x04000400u);
      VS::Battle::Ai::UpdateLiveRouteGateAvailability();
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
  constexpr std::uint32_t kFirst = 0x400 | (3u << 5) | 7 | (3u << 13);
  constexpr std::uint32_t kSecond =
      0x04000000 | (3u << 21) | (7u << 16) | (3u << 29);
  fixture.SetWord(1, kFirst);
  fixture.SetWord(2, kSecond);
  VS::Battle::Ai::UpdateLiveRouteGateAvailability();
  CHECK(fixture.Word(1) == (kFirst | 0x80008000u));
  CHECK(fixture.Word(2) == (kSecond | 0x80008000u));
}

TEST_CASE(
    "Route availability updates all words and reopens after visibility changes",
    "[ai][route-availability]") {
  RouteAvailabilityFixture fixture;
  constexpr std::uint32_t kFirst = 0x400 | (3u << 5) | 7;
  for (unsigned word = 0; word < 256; ++word) fixture.SetWord(word, kFirst);
  for (unsigned mask : {1u, 0u, 1u}) {
    fixture.SetVisible(mask);
    VS::Battle::Ai::UpdateLiveRouteGateAvailability();
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
  VS::Battle::Ai::roomAuxiliaryData = nullptr;
  VS::Battle::Ai::routeGates = nullptr;
  VS::Battle::Ai::UpdateLiveRouteGateAvailability();
  CHECK(fixture.Word(1) == 0x400);
}

TEST_CASE("Route availability retains the original thirteen-point capacity",
          "[ai][route-availability]") {
  RouteAvailabilityFixture fixture;
  fixture.SetVisible(1u << 12);
  constexpr std::uint32_t kFirst = 0x400 | (3u << 5) | 7;
  fixture.SetWord(1, kFirst);
  VS::Battle::Ai::UpdateLiveRouteGateAvailability();
  CHECK(fixture.Word(1) == (kFirst | 0x80008000u));
  fixture.SetVisible(1u << 4);
  fixture.SetEnd(0, 3, 6);
  fixture.SetEnd(1, 3, 7);
  fixture.SetWord(1, kFirst | (2u << 13));
  VS::Battle::Ai::UpdateLiveRouteGateAvailability();
  CHECK(fixture.Word(1) == (kFirst | (2u << 13) | 0x80000000u));
}

TEST_CASE("Original route availability entry resolves to the native updater",
          "[ai][route-availability]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DC638) ==
        reinterpret_cast<void*>(
            &VS::Battle::Ai::UpdateLiveRouteGateAvailability));
}

TEST_CASE("Route availability rejects source stack-dependent input",
          "[.routegate-stack-input]") {
  VS::Platform::InstallCrashHandler();
  RouteAvailabilityFixture fixture;
  fixture.SetVisible(1);
  fixture.SetWord(0, 0x04000000);
  VS::Battle::Ai::UpdateLiveRouteGateAvailability();
  FAIL("Stack-register-dependent route input was accepted");
}

TEST_CASE("Route availability rejects a truncated room auxiliary section",
          "[.routegate-room-bound]") {
  VS::Platform::InstallCrashHandler();
  RouteAvailabilityFixture fixture;
  VS::Field::roomData.header.section6_length = 0x497;
  VS::Battle::Ai::UpdateLiveRouteGateAvailability();
  FAIL("Truncated room route table was accepted");
}

TEST_CASE("Route availability rejects excess moving-platform output capacity",
          "[.routegate-platform-bound]") {
  VS::Platform::InstallCrashHandler();
  RouteAvailabilityFixture fixture;
  VS::Field::roomData.header.moving_platforms_length =
      14 * sizeof(VS::Field::MpdMovingPlatform);
  VS::Battle::Ai::UpdateLiveRouteGateAvailability();
  FAIL("Moving-platform output beyond the original buffer was accepted");
}

TEST_CASE("Production scripted path request allocates a complete native route",
          "[ai][scripted-override][scripted-override-red]") {
  ScriptedOverrideFixture fixture;
  SVECTOR target{1200, 0, 1300, 0};
  VS::Actors::BeginLiveScriptedActorPath(1, &target);
  auto* route = fixture.ActiveRoute();
  REQUIRE(route != nullptr);
  const auto capacity = VS::Core::Memory::LivePayloadCapacity(route);
  REQUIRE(capacity.has_value());
  CHECK(*capacity >= sizeof(*route));
  CHECK(route->actor_id == 1);
  CHECK(route->scripted_target_xz == (1200u | (1300u << 16)));
  CHECK(route->native_pointer_18c == route);
  CHECK(route->native_pointer64 == &fixture.model.instance.current_tile_x);
  CHECK(VS::Field::LiveActorCollisionBypass(1) == 2);
}

TEST_CASE(
    "Scripted route update completes a reached point and releases storage",
    "[ai][scripted-override][scripted-override-red]") {
  ScriptedOverrideFixture fixture;
  fixture.AllocateCopiedRoute(1088u | (1088u << 16));
  VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
  CHECK(fixture.source.native_pointer_18c == nullptr);
  CHECK(fixture.actors[1].command_state == 0x80);
  CHECK(fixture.actors[1].command_opcode == 2);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE(
    "Scripted route creation unlinks engagement and preserves native aliases",
    "[ai][scripted-override]") {
  ScriptedOverrideFixture fixture;
  fixture.room.engagement_limit = 1;
  const auto saved_busy = VS::Battle::Ai::candidateEngagedThisUpdate;
  VS::Battle::Ai::candidateEngagedThisUpdate = 0;
  const auto engaged = VS::Battle::Ai::TryEngageLiveActor(&fixture.source);
  VS::Battle::Ai::candidateEngagedThisUpdate = saved_busy;
  REQUIRE(engaged);
  VS::Battle::actorMarkers[1] = static_cast<char>(0x85);
  fixture.source.current_candidate = &fixture.source.candidates[0];
  const std::uint32_t preserved = 0x11223344;
  std::memcpy(fixture.source.reserved160.data() + 4, &preserved,
              sizeof preserved);
  VS::Battle::Ai::SetLiveActorNavigationOverride(1, 1200, 1300);
  const auto* record = fixture.ActiveRoute();
  REQUIRE(record != nullptr);
  CHECK(record->current_candidate == &fixture.source.candidates[0]);
  CHECK(record->actor == fixture.source.actor);
  fixture.CheckEngagementReleased();
  std::uint32_t actual;
  std::memcpy(&actual, record->reserved160.data() + 4, sizeof actual);
  CHECK(actual == preserved);
  VS::Battle::Ai::FreeLiveActorNavigationOverride(1);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE(
    "Repeated scripted path requests retain the source base-route distinction",
    "[ai][scripted-override]") {
  ScriptedOverrideFixture fixture;
  VS::Battle::Ai::SetLiveActorNavigationOverride(1, 1200, 1300);
  auto* first = fixture.ActiveRoute();
  REQUIRE(first != nullptr);
  const auto units = VS::Core::Memory::GameHeap().FreeUnits();
  VS::Battle::Ai::SetLiveActorNavigationOverride(1, 1400, 1500);
  CHECK(fixture.ActiveRoute() == first);
  CHECK(first->scripted_target_xz == (1200u | (1300u << 16)));
  CHECK(fixture.source.scripted_target_xz == (1400u | (1500u << 16)));
  CHECK(fixture.source.settings_bit14 == 3);
  CHECK(VS::Core::Memory::GameHeap().FreeUnits() == units);
}

TEST_CASE("Scripted path packing preserves every halfword and signed high bits",
          "[ai][scripted-override]") {
  ScriptedOverrideFixture fixture;
  const auto random = VS::Core::BiosRand().State();
  for (unsigned value = 0; value < 65536; ++value) {
    VS::Battle::Ai::SetLiveActorNavigationOverride(1, static_cast<int>(value),
                                                   0x1234);
    const auto* changed = value == 0 ? fixture.ActiveRoute() : &fixture.source;
    REQUIRE(changed != nullptr);
    CHECK(changed->scripted_target_xz == (value | 0x12340000u));
    VS::Battle::Ai::SetLiveActorNavigationOverride(1, 0x5678,
                                                   static_cast<int>(value));
    CHECK(fixture.source.scripted_target_xz == (0x5678u | (value << 16)));
  }
  VS::Battle::Ai::SetLiveActorNavigationOverride(1, -1, 0x1234);
  CHECK(fixture.source.scripted_target_xz == 0xFFFFFFFFu);
  VS::Battle::Ai::SetLiveActorNavigationOverride(1, 0x1234, -1);
  CHECK(fixture.source.scripted_target_xz == 0xFFFF1234u);
  CHECK(VS::Core::BiosRand().State() == random);
}

TEST_CASE("Scripted path requests retain missing map and AI-state gates",
          "[ai][scripted-override]") {
  ScriptedOverrideFixture fixture;
  VS::Battle::actorMarkers[1] = static_cast<char>(0x85);
  VS::Battle::Ai::tileFlagMap = nullptr;
  VS::Battle::Ai::SetLiveActorNavigationOverride(1, 1200, 1300);
  CHECK(fixture.source.native_pointer_18c == nullptr);
  VS::Battle::Ai::tileFlagMap = fixture.grid;
  VS::Battle::Ai::actorAiStates[1] = nullptr;
  VS::Battle::Ai::SetLiveActorNavigationOverride(1, 1200, 1300);
  CHECK(fixture.source.native_pointer_18c == nullptr);
  CHECK(VS::Core::Memory::GameHeap().Empty());
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[1]) == 0x85);
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
    VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
    const bool reached = delta[0] * delta[0] + delta[1] * delta[1] < 1024;
    CHECK((fixture.source.native_pointer_18c == nullptr) == reached);
    if (!reached) {
      CHECK(fixture.ActiveRoute()->move_x == 1088 + delta[0]);
      CHECK(fixture.ActiveRoute()->move_z == 1088 + delta[1]);
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
  record->navigation_flags = 0xABCDFFFD;
  record->opaque_19a = record->opaque1 = record->is_stuck = record->opaque3 =
      0xA5;
  record->route_gate = 1;
  fixture.model.instance.current_tile_layer = 1;
  fixture.grid[8][8] = 0xAB;
  auto& frame = VS::Battle::Ai::LiveFrameContext();
  frame.walk_valid = frame.walk_passable = 0xFF;
  frame.blocker_valid = frame.reserved_cache = 0xFF;
  constexpr std::array<std::uint8_t, 8> kBlockers{0x78, 0x56, 0x34, 0x12,
                                                  0xD8, 0xC7, 0xB6, 0xA5};
  std::ranges::copy(kBlockers, std::begin(frame.blocker_actor));
  VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
  fixture.CheckPreparedRoute();
  fixture.CheckPreparedFlags();
  fixture.CheckCopiedCoordinates();
  CHECK(frame.walk_valid == 0);
  CHECK(frame.walk_passable == 0);
  CHECK(frame.blocker_valid == 0);
  CHECK(frame.reserved_cache == 0);
  CHECK(std::ranges::equal(kBlockers, frame.blocker_actor));
  CHECK(VS::Battle::Ai::savedMovementRandomState == 0x1234);
}

TEST_CASE("Scripted route updates skip suspended and missing-AI actors safely",
          "[ai][scripted-override]") {
  const auto flags = GENERATE(0x40, 0x80, 0xC0);
  ScriptedOverrideFixture fixture;
  fixture.AllocateCopiedRoute(1088u | (1088u << 16));
  const auto before = Snapshot(*fixture.ActiveRoute());
  fixture.actors[1].command_state = static_cast<std::uint8_t>(flags);
  VS::Battle::Ai::LiveFrameContext().lastBudgetTick = 0x1234;
  VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
  CHECK(Snapshot(*fixture.ActiveRoute()) == before);
  CHECK(VS::Battle::Ai::LiveFrameContext().lastBudgetTick == 0x1234);
  fixture.actors[1].command_state = 0;
  VS::Battle::Ai::actorAiStates[1] = nullptr;
  VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
  CHECK(Snapshot(*fixture.ActiveRoute()) == before);
  CHECK(fixture.actors[0].next == &fixture.actors[1]);
}

TEST_CASE("Non-ground scripted arrival ignores layer and status metadata",
          "[ai][scripted-override]") {
  const auto mode = GENERATE(1, 2);
  ScriptedOverrideFixture fixture;
  fixture.AllocateCopiedRoute(1200u | (1300u << 16));
  fixture.ActiveRoute()->locomotion_mode = static_cast<std::uint8_t>(mode);
  fixture.ActiveRoute()->destination.word = 0xA5080308;
  fixture.model.instance.current_tile_layer = 5;
  VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
  CHECK(fixture.source.native_pointer_18c == nullptr);
  CHECK(fixture.actors[1].command_opcode == 2);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("Scripted arrival retains signed source square-sum behavior",
          "[ai][scripted-override]") {
  const std::int16_t position = GENERATE(-1, -32768);
  ScriptedOverrideFixture fixture;
  fixture.model.instance.position.vx = fixture.model.instance.position.vz =
      position;
  fixture.AllocateCopiedRoute(0);
  VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
  CHECK(fixture.source.native_pointer_18c == nullptr);
  CHECK(fixture.actors[1].command_opcode == 2);
}

TEST_CASE(
    "Original scripted route entries resolve to native lifecycle functions",
    "[ai][scripted-override]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800E527C) ==
      reinterpret_cast<void*>(&VS::Battle::Ai::SetLiveActorNavigationOverride));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E5308) ==
        reinterpret_cast<void*>(
            &VS::Battle::Ai::UpdateLiveActorNavigationOverrides));
}

TEST_CASE("Scripted path requests reject mismatched actor ownership",
          "[.override-actor-identity]") {
  VS::Platform::InstallCrashHandler();
  ScriptedOverrideFixture fixture;
  VS::Battle::actors[1] = nullptr;
  VS::Battle::Ai::SetLiveActorNavigationOverride(1, 1200, 1300);
  FAIL("Mismatched path-request identity was accepted");
}

TEST_CASE("Active scripted routes require their native map",
          "[.override-missing-map]") {
  VS::Platform::InstallCrashHandler();
  ScriptedOverrideFixture fixture;
  fixture.AllocateCopiedRoute(1088u | (1088u << 16));
  VS::Battle::Ai::tileFlagMap = nullptr;
  VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
  FAIL("An active path without a native map was accepted");
}

TEST_CASE("Scripted navigation rejects cyclic native actor lists",
          "[.override-cyclic-list]") {
  VS::Platform::InstallCrashHandler();
  ScriptedOverrideFixture fixture;
  fixture.actors[1].command_state = 0xC0;
  fixture.actors[1].next = &fixture.actors[1];
  VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
  FAIL("A cyclic actor list was accepted");
}
