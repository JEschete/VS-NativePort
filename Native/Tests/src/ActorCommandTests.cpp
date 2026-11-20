#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.hpp"
#include "Actors/Movement.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/RoomAiMap.hpp"
#include "Battle/Ai/RoomAiMapLive.hpp"
#include "Battle/Damage.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Legacy/Psx.hpp"

namespace {
// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the access below.
template <typename T>
const T& Required(const std::optional<T>& value) {
  REQUIRE(value);
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

struct CommandFixture {
  VS::Battle::Ai::FrameContext saved_frame = VS::Battle::Ai::LiveFrameContext();
  decltype(VS::Battle::Ai::tileFlagMap) saved_flags =
      VS::Battle::Ai::tileFlagMap;
  VS::Battle::Actor* saved_actor = VS::Battle::actors[2];
  VS::Actors::ActorRecord* saved_target = VS::Actors::actorRecords[3];
  VS::Battle::Actor actor{};
  VS::Actors::ModelInstance model{};
  VS::Actors::ActorRecord target{};
  VS::Battle::Ai::ActorAiState state{};
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  std::uint16_t heights[32][32]{};
  std::uint16_t flags[32][32]{};

  CommandFixture() {
    VS::Battle::actors[2] = &actor;
    VS::Actors::actorRecords[3] = &target;
    actor.id = state.actor_id = 2;
    actor.model = &model;
    actor.command_arg0 = 0x13572468;
    actor.command_arg1.word = 0x22334455;
    actor.command_arg2 = 0x66778899;
    state.actor = reinterpret_cast<VS::Battle::Ai::EngagementData*>(&actor);
    state.movement_target = 3;
    state.position.vx = state.position.vz = 320;
    state.tile.word = 0x00020002;
    state.next_tile.word = state.destination.word = 0x00030003;
    state.radius = 16;
    state.locomotion_flags = 0x42;
    state.walk_step = 8;
    state.run_step = 12;
    state.opaque_13a = 32;
    state.facing_delay = 7;
    state.flight_height = 16;
    state.move_x = 1234;
    state.move_z = 2345;
    state.move_y = 3456;
    state.next_position.pad = 0x5555;
    const uint16_t threshold = 64;
    std::memcpy(state.step_height_bytes.data(), &threshold, sizeof(threshold));
    state.current_candidate = &candidate;
    candidate.row = 2;
    candidate.column = 1;
    candidate.target_id = 3;
    candidate.target_limb = 4;
    candidate.height_offset = 13;
    state.action_slots[2][1].kind = 1;
    target.instance.position = {100, 200, 300, 0};
    auto* frame = &VS::Battle::Ai::LiveFrameContext();
    frame->roomData = nullptr;
    frame->heightMap = heights;
    frame->tileFlags = flags;
    const std::array<std::uint8_t, 8> directions{0x30, 0x33, 0x03, 0x13,
                                                 0x10, 0x11, 0x01, 0x31};
    std::copy(directions.begin(), directions.end(), frame->directions);
    VS::Battle::Ai::tileFlagMap = flags;
  }

  ~CommandFixture() {
    VS::Battle::Ai::LiveFrameContext() = saved_frame;
    VS::Battle::Ai::tileFlagMap = saved_flags;
    VS::Battle::actors[2] = saved_actor;
    VS::Actors::actorRecords[3] = saved_target;
  }
};
}  // namespace

TEST_CASE("AI dispatch covers all eighteen actor command intents",
          "[ai][actor-commands]") {
  const auto intent =
      GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
  const std::array<int, 18> opcodes{2,  1,  20, 3,  4,  9,  9,  9, 19,
                                    16, 17, 21, 22, 23, 11, 24, 6, 8};
  CommandFixture fixture;
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, intent);
  CHECK(fixture.actor.command_state == 0x80);
  CHECK(fixture.actor.command_opcode == opcodes[intent]);
  CHECK(fixture.actor.command_target == 3);
  CHECK(VS::Battle::Ai::LiveFrameContext().movementResult == intent);
}

TEST_CASE(
    "AI direct commands preserve the required payload and untouched halves",
    "[ai][actor-commands]") {
  const auto intent = GENERATE(3, 8, 9, 10, 16);
  CommandFixture fixture;
  fixture.state.heading = 0x2345;
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, intent);
  if (intent == 3) {
    CHECK(fixture.actor.command_arg0 == 0x00070345);
  } else if (intent == 8) {
    CHECK(fixture.actor.command_arg0 == 1234);
    CHECK(fixture.actor.command_arg1.word == 2345);
    CHECK(fixture.actor.command_arg2 == 3456);
  } else if (intent == 16) {
    CHECK(fixture.actor.command_arg0 == 0x00202345);
  } else {
    CHECK(fixture.actor.command_arg0 == (3456 << 16 | 1234));
    CHECK(fixture.actor.command_arg1.word == (0x22330000 | 2345));
    CHECK(fixture.actor.command_arg2 == 0x66778899);
  }
}

TEST_CASE("AI turning keeps strict rate boundaries and upper-body selection",
          "[ai][actor-commands]") {
  const auto target = GENERATE(0, 31, 32, 64, 4095);
  const auto upper_body = GENERATE(0, 1);
  CommandFixture fixture;
  fixture.state.angle = static_cast<int16_t>(target);
  fixture.state.has_tagged_bone3 = static_cast<char>(upper_body);
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, 4);
  const bool reached = target == 0 || target == 31 || target == 4095;
  CHECK(fixture.actor.command_opcode == (reached ? 4 : (upper_body ? 7 : 3)));
  CHECK((static_cast<uint32_t>(fixture.actor.command_arg0) >> 16) == 32);
  CHECK((fixture.actor.command_arg0 & 0xFFFF) == (reached ? target : 32));
}

TEST_CASE(
    "AI tile commands stage native positions and reserve only their target",
    "[ai][actor-commands]") {
  CommandFixture fixture;
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, 13);
  CHECK(fixture.state.next_position.vx == 448);
  CHECK(fixture.state.next_position.vy == 0);
  CHECK(fixture.state.next_position.vz == 448);
  CHECK(fixture.state.next_position.pad == 0x5555);
  CHECK(fixture.actor.command_arg0 == 0x00030003);
  CHECK(fixture.flags[3][3] == 0x80);
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, 12);
  CHECK(fixture.actor.command_arg0 == 0x001001C0);
  CHECK((fixture.actor.command_arg1.word & 0xFFFF) == 448);
}

TEST_CASE(
    "AI step commands choose walking, jumping and dropping from height flags",
    "[ai][actor-commands]") {
  const auto flags = GENERATE(0, 8, 0x30, 0xC0);
  CommandFixture fixture;
  fixture.heights[3][3] = 1;
  const uint16_t threshold = 10;
  std::memcpy(fixture.state.step_height_bytes.data(), &threshold,
              sizeof(threshold));
  fixture.state.movement_flags = static_cast<std::uint16_t>(flags);
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, 14);
  const auto expected =
      flags == 8 ? 23 : (flags == 0x30 ? 18 : (flags == 0xC0 ? 14 : 11));
  CHECK(fixture.actor.command_opcode == expected);
  CHECK(fixture.state.next_position.vy == 64);
  CHECK(fixture.flags[3][3] == 0x80);
}

TEST_CASE(
    "AI attacks encode limb targets or positional targets without guest "
    "pointers",
    "[ai][actor-commands]") {
  const auto kind = GENERATE(0, 1, 2, 3);
  CommandFixture fixture;
  fixture.state.action_slots[2][1].kind = kind;
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, 15);
  CHECK(fixture.actor.command_opcode == 24);
  CHECK(fixture.actor.command_arg0 == 0x00010002);
  if (kind == 1 || kind == 2) {
    CHECK(fixture.actor.command_arg1.word == 0x00040003);
    CHECK(fixture.actor.command_arg2 == 0x66778899);
  } else {
    CHECK(fixture.actor.command_arg1.word == (187 << 16 | 100));
    CHECK(fixture.actor.command_arg2 == 0x6677012C);
  }
}

TEST_CASE(
    "AI blocked movement slides once per tile using fixed-point command "
    "payloads",
    "[ai][actor-commands]") {
  CommandFixture fixture;
  fixture.state.position.vx = 264;
  fixture.state.heading = 1024;
  fixture.heights[2][1] = 1;
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, 7);
  CHECK(fixture.actor.command_opcode == 13);
  CHECK(fixture.actor.command_arg0 == 32768);
  CHECK(fixture.actor.command_arg1.word == 192512);
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, 7);
  CHECK(fixture.actor.command_opcode == 9);
}

namespace {
struct RoomCommandFixture : CommandFixture {
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  decltype(VS::Battle::Ai::actorAiStates) saved_states =
      VS::Battle::Ai::actorAiStates;
  VS::Battle::Ai::RoomAiState* saved_room = VS::Battle::Ai::roomAiState;
  decltype(VS::Battle::Ai::specialTerrainHeights) saved_heights =
      VS::Battle::Ai::specialTerrainHeights;
  decltype(VS::Battle::Ai::roomAuxiliaryData) saved_auxiliary =
      VS::Battle::Ai::roomAuxiliaryData;
  decltype(VS::Battle::Ai::patrolRoutes) saved_routes =
      VS::Battle::Ai::patrolRoutes;
  decltype(VS::Battle::Ai::roomWaypoints) saved_waypoints =
      VS::Battle::Ai::roomWaypoints;
  decltype(VS::Battle::Ai::routeGates) saved_gates = VS::Battle::Ai::routeGates;
  int saved_random = VS::Battle::Ai::savedMovementRandomState;
  VS::Field::RoomDataRecord saved_data = VS::Field::roomData;
  decltype(VS::Battle::Ai::directionDeltas) saved_directions =
      VS::Battle::Ai::directionDeltas;
  VS::Field::MpdCollisionHeader dimensions{2, 2, 0, 4};
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 3072> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};

  RoomCommandFixture() {
    std::fill_n(VS::Battle::actors.begin(), saved_actors.size(), nullptr);
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved_states.size(),
                nullptr);
    VS::Battle::actors[2] = &actor;
    VS::Battle::Ai::roomAiState = nullptr;
    VS::Battle::Ai::tileFlagMap = nullptr;
    VS::Battle::Ai::specialTerrainHeights = nullptr;
    VS::Battle::Ai::roomAuxiliaryData = nullptr;
    VS::Battle::Ai::patrolRoutes = nullptr;
    VS::Battle::Ai::roomWaypoints = nullptr;
    VS::Battle::Ai::routeGates = nullptr;
    VS::Field::roomData = {};
    VS::Field::roomData.collision_map = &dimensions;
    VS::Battle::Ai::LiveFrameContext() = {};
    state.next_tile.word = 0x00010001;
    VS::Battle::Ai::savedMovementRandomState = 0x12345678;
  }

  ~RoomCommandFixture() {
    VS::Battle::Ai::FreeRoomAiState(VS::Battle::Ai::BattleRoomAiRuntime());
    std::copy(saved_actors.begin(), saved_actors.end(),
              VS::Battle::actors.begin());
    std::copy(saved_states.begin(), saved_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
    VS::Battle::Ai::roomAiState = saved_room;
    VS::Battle::Ai::specialTerrainHeights = saved_heights;
    VS::Battle::Ai::roomAuxiliaryData = saved_auxiliary;
    VS::Battle::Ai::patrolRoutes = saved_routes;
    VS::Battle::Ai::roomWaypoints = saved_waypoints;
    VS::Battle::Ai::routeGates = saved_gates;
    VS::Battle::Ai::savedMovementRandomState = saved_random;
    VS::Field::roomData = saved_data;
    std::copy(saved_directions.begin(), saved_directions.end(),
              VS::Battle::Ai::directionDeltas.begin());
  }
};
}  // namespace

TEST_CASE(
    "Room entry prepares special heights before actor commands and reentry",
    "[ai][special-terrain][room-context][room-navigation]") {
  RoomCommandFixture fixture;
  alignas(VS::Field::MpdCollisionHeader) std::array<unsigned char, 40>
      collision{};
  const VS::Field::MpdCollisionHeader header{2, 2, 16, 4};
  std::memcpy(collision.data(), &header, sizeof header);
  std::array<VS::Field::MpdTileCell, 4> cells{};
  cells[3].is_pit = 1;
  VS::Field::roomData.collision_map =
      reinterpret_cast<VS::Field::MpdCollisionHeader*>(collision.data());
  VS::Field::roomData.header.collision_map_length = collision.size();
  VS::Field::roomData.tile_cells = cells.data();
  VS::Field::roomData.header.tile_cells_length = sizeof cells;
  constexpr std::array<int, 4> kX{0, 1, 0, -1};
  constexpr std::array<int, 4> kZ{-1, 0, 1, 0};
  for (std::size_t index = 0; index < kX.size(); ++index) {
    VS::Battle::Ai::directionDeltas[index * 2].dx =
        static_cast<signed char>(kX[index]);
    VS::Battle::Ai::directionDeltas[index * 2].dz =
        static_cast<signed char>(kZ[index]);
  }
  for (int entry = 0; entry < 2; ++entry) {
    VS::Battle::Ai::InitializeLiveRoomNavigation();
    REQUIRE(VS::Battle::Ai::specialTerrainHeights != nullptr);
    CHECK(VS::Battle::Ai::tileFlagMap[1][1] == 0x20);
    CHECK(VS::Battle::Ai::specialTerrainHeights[1][1] == 0x2002);
    CHECK(VS::Battle::Ai::specialTerrainHeights[0][0] == 2);
    CHECK(VS::Battle::Ai::LiveFrameContext().heightMap ==
          VS::Battle::Ai::specialTerrainHeights);
    VS::Battle::Ai::FreeRoomAiState(VS::Battle::Ai::BattleRoomAiRuntime());
    CHECK(VS::Battle::Ai::LiveFrameContext().heightMap == nullptr);
    CHECK(VS::Battle::Ai::specialTerrainHeights == nullptr);
  }
}

TEST_CASE("Room terrain supports hover before the first AI update",
          "[ai][actor-commands][room-context]") {
  RoomCommandFixture fixture;
  VS::Battle::Ai::InitializeRoomAiMap(VS::Battle::Ai::BattleRoomAiRuntime());
  auto* frame = &VS::Battle::Ai::LiveFrameContext();
  REQUIRE(frame->heightMap != nullptr);
  CHECK(frame->heightMap == VS::Battle::Ai::specialTerrainHeights);
  CHECK(frame->tileFlags == VS::Battle::Ai::tileFlagMap);
  CHECK(frame->room == VS::Battle::Ai::roomAiState);
  frame->heightMap[1][1] = 2;
  frame->randomState = 0x87654321;
  VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, 12);
  CHECK(fixture.actor.command_arg0 == (144 << 16 | 192));
  CHECK((fixture.actor.command_arg1.word & 0xFFFF) == 192);
  CHECK(frame->randomState == 0x87654321);
  CHECK(VS::Battle::Ai::savedMovementRandomState == 0x12345678);
}

TEST_CASE("Room teardown retires frame pointers before another room",
          "[ai][actor-commands][room-context]") {
  RoomCommandFixture fixture;
  for (int world = 1; world <= 2; ++world) {
    VS::Battle::Ai::InitializeRoomAiMap(VS::Battle::Ai::BattleRoomAiRuntime());
    auto* frame = &VS::Battle::Ai::LiveFrameContext();
    REQUIRE(frame->heightMap == VS::Battle::Ai::specialTerrainHeights);
    REQUIRE(frame->heightMap != nullptr);
    CHECK(frame->randomState == 0x12345678);
    frame->heightMap[1][1] = static_cast<std::uint16_t>(world);
    VS::Battle::Ai::IssueLiveActorCommand(&fixture.state, 12);
    CHECK((fixture.actor.command_arg0 >> 16) == world * 64 + 16);
    frame->actors[0] = &fixture.state;
    frame->walk_valid = 1;
    VS::Battle::Ai::FreeRoomAiState(VS::Battle::Ai::BattleRoomAiRuntime());
    const std::array retired{
        frame->heightMap == nullptr, frame->tileFlags == nullptr,
        frame->room == nullptr, frame->actors[0] == nullptr,
        frame->walk_valid == 0};
    const std::array expected_retired{true, true, true, true, true};
    CHECK(retired == expected_retired);
    CHECK(VS::Battle::Ai::savedMovementRandomState == 0x12345678);
  }
}

TEST_CASE("Original cutscene rooms allow shorter AI direction payloads",
          "[ai][room-map169]") {
  RoomCommandFixture fixture;
  const int shape = GENERATE(0, 1);
  fixture.dimensions.width = shape == 0 ? 32 : 13;
  fixture.dimensions.height = shape == 0 ? 32 : 25;
  std::array<std::uint8_t, 0x498 + 308> auxiliary{};
  auxiliary[0x494] = 17;
  auxiliary[0x495] = 16;
  VS::Field::roomData.section6 = auxiliary.data();
  VS::Field::roomData.header.section6_length = auxiliary.size();
  const auto original = auxiliary;
  const auto grid = VS::Battle::Ai::BuildRoomAiMap(
      VS::Battle::Ai::BattleRoomAiRuntime().CurrentRoom());
  const VS::Battle::Ai::RoomAiGrid& map = Required(grid);
  CHECK(map.settings.max_tile_x == 17);
  CHECK(map.settings.max_tile_z == 16);
  CHECK(map.flags[31][31] == 0x8000);
  CHECK(auxiliary == original);
}

namespace {
void CheckMissingDirectionCell(int x, int z) {
  CHECK(VS::Battle::Ai::FindTerrainRegionCell(x, z) == nullptr);
}
}  // namespace

TEST_CASE(
    "AI direction access preserves mutable cells and rejects missing bytes",
    "[ai][room-map169]") {
  RoomCommandFixture fixture;
  std::array<std::uint8_t, 310> guarded{};
  guarded.front() = 0xA5;
  guarded.back() = 0x5A;
  auto& frame = VS::Battle::Ai::LiveFrameContext();
  frame.terrainRegions = guarded.data() + 1;
  frame.terrain_region_count = 308;
  frame.roomBounds = GENERATE(0x2020u, 0x190Du);
  const int width = static_cast<int>(frame.roomBounds & 0xFFu);
  std::uint8_t* cell =
      VS::Battle::Ai::FindTerrainRegionCell(307 % width, 307 / width);
  CHECK(cell == guarded.data() + 308);
  REQUIRE(cell != nullptr);
  *cell = 0xD2;
  CHECK(*VS::Battle::Ai::FindTerrainRegionCell(307 % width, 307 / width) ==
        0xD2);
  CheckMissingDirectionCell(308 % width, 308 / width);
  CheckMissingDirectionCell(-1, 0);
  CheckMissingDirectionCell(width, 0);
  CheckMissingDirectionCell(0, -1);
  CheckMissingDirectionCell(0, 32);
  frame.terrainRegions = nullptr;
  CheckMissingDirectionCell(0, 0);
  CHECK(guarded.front() == 0xA5);
  CHECK(guarded.back() == 0x5A);
}
