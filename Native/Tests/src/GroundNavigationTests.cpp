#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/GroundNavigation.h"
#include "Battle/Ai/MovementBehavior.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FlightTerrain.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Damage.h"
}

namespace {
unsigned Tile(int x, int z) {
  return static_cast<unsigned>(x) | (static_cast<unsigned>(z) << 16);
}

struct GroundFixture {
  Battle_Ai_FrameContext saved = *Battle_Ai_FrameState();
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(Battle_Ai_roomAiState) saved_room = Battle_Ai_roomAiState;
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  unsigned saved_lengths = Content_battleAiHesitationLengths[0];
  Battle_Ai_RoomAiState room{};
  std::array<Actors_ActorRecord, 2> models{};
  std::array<Battle_Actor, 2> actors{};
  std::array<Battle_Ai_ActorAiState, 2> states{};
  u_short heights[32][32]{};
  u_short flags[32][32]{};
  std::array<u_char, 1024> regions{};

  GroundFixture() {
    auto& frame = *Battle_Ai_FrameState();
    frame.heightMap = heights;
    frame.tileFlags = flags;
    frame.roomBounds = 0x2020;
    frame.roomData = nullptr;
    frame.terrainRegions = regions.data();
    frame.terrain_region_count = regions.size();
    frame.room = &room;
    Battle_Ai_roomAiState = &room;
    Battle_Ai_tileFlagMap = flags;
    Content_battleAiHesitationLengths[0] = 0x01184614;
    frame.randomState = 40;
    frame.lastBudgetTick = 1;
    const std::array<u_char, 8> directions{0x30, 0x33, 0x03, 0x13,
                                           0x10, 0x11, 0x01, 0x31};
    std::copy(directions.begin(), directions.end(), frame.directions);
    ClearCache();
    std::fill_n(frame.actors, kAiFrameActorSlots, nullptr);
    std::fill_n(Actors_actorRecords, saved_models.size(), nullptr);
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Battle_Ai_actorAiStates, saved_states.size(), nullptr);
    for (unsigned index = 0; index < states.size(); ++index) {
      frame.actors[index] = &states[index];
      Battle_Ai_actorAiStates[index] = &states[index];
      Actors_actorRecords[index] = &models[index];
      Battle_actors[index] = &actors[index];
      actors[index].id = static_cast<int>(index);
      states[index].actorId = static_cast<u_char>(index);
      states[index].actor =
          reinterpret_cast<Battle_Ai_EngagementData*>(&actors[index]);
      states[index].model =
          reinterpret_cast<Battle_Ai_AiModelView*>(&models[index]);
      states[index].collision_radius = 32;
      states[index].bodyHeight = 128;
      states[index].maxRange = 32;
      states[index].reservedB2 = 3;
      states[index].locomotionFlags = 0x42;
      states[index].walkStep = 32;
      states[index].runStep = 64;
      const std::uint16_t step = 128;
      std::memcpy(states[index].step_height_bytes, &step, sizeof(step));
    }
    actors[0].next = &actors[1];
    states[0].tile.word = Tile(8, 8);
    states[0].previousTile = -1;
    states[0].position.vx = states[0].position.vz = 1088;
    states[0].fine_x = states[0].fine_z = 17;
    models[0].instance.position = states[0].position;
    states[0].movementTarget = 16;
    models[1].instance.position.vx = models[1].instance.position.vz = 3000;
  }

  ~GroundFixture() {
    *Battle_Ai_FrameState() = saved;
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_states.begin(), saved_states.end(),
              Battle_Ai_actorAiStates);
    Battle_Ai_roomAiState = saved_room;
    Battle_Ai_tileFlagMap = saved_flags;
    Content_battleAiHesitationLengths[0] = saved_lengths;
  }

  void ClearCache() {
    std::fill_n(Battle_Ai_FrameState()->directionCache, kAiDirectionCacheWords,
                0);
  }

  int Occupancy(unsigned tile) {
    return Battle_Ai_DestinationOccupancy(
        reinterpret_cast<Battle_Ai_MoveTargetView*>(&states[0]), tile);
  }

  int Blocker(int direction) {
    return Battle_Ai_DirectionalBlockingActor(
        reinterpret_cast<Battle_Ai_MovementPathView*>(&states[0]), direction);
  }

  int Walk(int direction) {
    return Battle_Ai_CanWalkInDirection(
        reinterpret_cast<Battle_Ai_WanderView*>(&states[0]), direction);
  }

  int Step(unsigned to, int direction) {
    return Battle_Ai_CanTraverseTileEdge(
        reinterpret_cast<Battle_Ai_MoveTargetView*>(&states[0]),
        states[0].tile.word, to, direction);
  }

  unsigned Gap(unsigned tile, int direction) {
    return Battle_Ai_FindGapLandingTile(
        reinterpret_cast<Battle_Ai_MoveTargetView*>(&states[0]), tile,
        direction, states[0].tile.word);
  }
};
}  // namespace

TEST_CASE(
    "Ground occupancy rejects reservations, room edges and exact cell masks",
    "[ai][ground-query]") {
  GroundFixture fixture;
  CHECK(fixture.Occupancy(Tile(9, 8)) == 1);
  CHECK(fixture.Occupancy(Tile(32, 8)) == 0);
  CHECK(fixture.Occupancy(Tile(8, 32)) == 0);
  fixture.states[0].previousTile = static_cast<int>(0xAA080009);
  CHECK(fixture.Occupancy(Tile(9, 8)) == 0);
  fixture.states[0].previousTile = -1;
  for (unsigned bit = 0; bit < 16; ++bit) {
    fixture.flags[8][9] = static_cast<u_short>(1u << bit);
    CAPTURE(bit);
    CHECK(fixture.Occupancy(Tile(9, 8)) ==
          ((0x80B0u & (1u << bit)) != 0 ? 0 : 1));
  }
  fixture.states[0].terrain_reject_mask = 2;
  fixture.flags[8][9] = 0x200;
  CHECK(fixture.Occupancy(Tile(9, 8)) == 0);
}

TEST_CASE("Flight ground queries use tile coordinates and reject room edges",
          "[ai][flight][terrain]") {
  GroundFixture fixture;
  fixture.room.minTileX = 0;
  fixture.room.maxTileX = 31;
  fixture.room.minTileZ = 0;
  fixture.room.maxTileZ = 31;

  auto& state = fixture.states[1];
  state.tile.word = Tile(8, 13) | 0x40000000u;
  state.previousTile = -1;
  fixture.heights[13][9] = 1023;
  CHECK(Battle_Ai_LowestReachableGroundHeight(&state, 6 << 9) == -64);

  state.tile.word = Tile(31, 13) | 0x40000000u;
  fixture.ClearCache();
  CHECK(Battle_Ai_LowestReachableGroundHeight(&state, 6 << 9) == 0);
}

TEST_CASE("Flight terrain helpers preserve sampled heights and room ceiling",
          "[ai][flight][terrain]") {
  GroundFixture fixture;
  fixture.heights[13][9] = 1023;
  CHECK(Battle_Ai_MinimumAiGroundHeightAlongSegment(
            8 * 128 + 64, 13 * 128 + 64, 10 * 128 + 64, 13 * 128 + 64) == -64);

  fixture.room.flightCeiling = 512;
  CHECK(Battle_Ai_RoomNavigationCeilingHeight() == 512);
  Battle_Ai_roomAiState = nullptr;
  CHECK(Battle_Ai_RoomNavigationCeilingHeight() == 0);
}

TEST_CASE(
    "Ground occupancy uses inclusive actor bounds and vertical separation",
    "[ai][ground-query]") {
  const int offset = GENERATE(-65, -64, 0, 64, 65);
  const int height = GENERATE(-129, -128, 0, 128, 129);
  GroundFixture fixture;
  fixture.models[1].instance.position.vx =
      static_cast<std::int16_t>(1216 + offset);
  fixture.models[1].instance.position.vz = 1088;
  fixture.models[1].instance.position.vy = static_cast<std::int16_t>(height);
  CHECK(fixture.Occupancy(Tile(9, 8)) ==
        (offset >= -64 && offset <= 64 && height >= -128 && height <= 128 ? -1
                                                                          : 1));
}

TEST_CASE("Ground occupancy reads planned positions and native non-AI volumes",
          "[ai][ground-query]") {
  GroundFixture fixture;
  fixture.models[1].instance.movementMode = 1;
  fixture.states[1].nextPosition.vx = 1216;
  fixture.states[1].nextPosition.vz = 1088;
  CHECK(fixture.Occupancy(Tile(9, 8)) == -1);
  Battle_Ai_FrameState()->actors[1] = nullptr;
  CHECK(fixture.Occupancy(Tile(9, 8)) == 1);
  fixture.models[1].instance.position.vx = 1264;
  fixture.models[1].instance.position.vz = 1088;
  fixture.models[1].occupancyRadius = 16;
  fixture.models[1].occupancyHeight = 128;
  CHECK(fixture.Occupancy(Tile(9, 8)) == -1);
  fixture.models[1].instance.position.vx = 1265;
  CHECK(fixture.Occupancy(Tile(9, 8)) == 1);
}

TEST_CASE("Directional actor blockers are forward-only and cached by direction",
          "[ai][ground-query]") {
  GroundFixture fixture;
  fixture.models[1].instance.position.vx = 1216;
  fixture.models[1].instance.position.vz = 1088;
  CHECK(fixture.Blocker(6) == 2);
  CHECK(Battle_Ai_FrameState()->blocker_valid == 0x40);
  fixture.models[1].instance.position.vx = 3000;
  CHECK(fixture.Blocker(6) == 2);
  fixture.ClearCache();
  CHECK(fixture.Blocker(6) == 0);
  fixture.models[1].instance.position.vx = 1119;
  fixture.ClearCache();
  CHECK(fixture.Blocker(6) == 0);
}

TEST_CASE("Ground walking covers every direction and closes room boundaries",
          "[ai][ground-query]") {
  const int direction = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  GroundFixture fixture;
  CHECK(fixture.Walk(direction) == 1);
  CHECK((Battle_Ai_FrameState()->walk_valid & (1u << direction)) != 0);
  fixture.states[0].tile.word = Tile(0, 8);
  fixture.states[0].position.vx = 32;
  fixture.states[0].fine_x = 0;
  fixture.ClearCache();
  CHECK(fixture.Walk(2) == 0);
}

TEST_CASE("Ground walk flags honor floor override, areas, layers and caching",
          "[ai][ground-query]") {
  GroundFixture fixture;
  fixture.flags[8][9] = 0x10;
  CHECK(fixture.Walk(6) == 0);
  fixture.flags[8][9] = 0;
  CHECK(fixture.Walk(6) == 0);
  fixture.ClearCache();
  CHECK(fixture.Walk(6) == 1);
  fixture.flags[8][9] = 0x10;
  fixture.states[0].ignore_floor_mask = 1;
  fixture.ClearCache();
  CHECK(fixture.Walk(6) == 1);
}

TEST_CASE("Ground walk respects area exceptions and higher terrain layers",
          "[ai][ground-query]") {
  GroundFixture fixture;
  fixture.flags[8][9] = 1;
  fixture.states[0].settingsBit14 = 1;
  CHECK(fixture.Walk(6) == 0);
  fixture.states[0].reserved154Area = 1;
  fixture.ClearCache();
  CHECK(fixture.Walk(6) == 1);
  fixture.flags[8][9] = 0;
  Battle_Ai_FrameState()->roomData = fixture.regions.data();
  fixture.heights[8][9] = 0x400;
  fixture.ClearCache();
  CHECK(fixture.Walk(6) == 0);
}

TEST_CASE("Tile steps enforce configured gaps, slopes and movement disable",
          "[ai][ground-query]") {
  GroundFixture fixture;
  CHECK(fixture.Step(Tile(9, 8), 6) == 1);
  fixture.states[0].movementDisabled = 1;
  CHECK(fixture.Step(Tile(9, 8), 6) == 0);
  fixture.states[0].movementDisabled = 0;
  fixture.states[0].opaque3 = 1;
  fixture.heights[8][9] = 3;
  fixture.ClearCache();
  CHECK(fixture.Step(Tile(9, 8), 6) == 0);
  fixture.states[0].movementFlags = 8;
  fixture.ClearCache();
  CHECK(fixture.Step(Tile(9, 8), 6) == 1);
}

TEST_CASE("Tile steps use typed target positions for the approach radius",
          "[ai][ground-query]") {
  GroundFixture fixture;
  auto& approach =
      *reinterpret_cast<Battle_Ai_TargetApproachView*>(&fixture.states[0]);
  approach.targetId = 1;
  approach.range = 160;
  fixture.states[0].opaque5 = 1;
  fixture.states[0].requestedMovementMode = 1;
  fixture.states[0].destination.word = Tile(10, 8);
  fixture.models[1].instance.position.vx = 1216;
  fixture.models[1].instance.position.vz = 1088;
  fixture.states[1].collision_radius = 0;
  CHECK(fixture.Step(Tile(9, 8), 6) == -1);
}

TEST_CASE("Gap landing retains the strict step threshold and packed result",
          "[ai][ground-query]") {
  const int holes = GENERATE(1, 2, 3);
  GroundFixture fixture;
  for (int index = 1; index <= holes; ++index) {
    fixture.flags[8][8 + index] = 0x20;
  }
  const auto result = fixture.Gap(Tile(9, 8), 6);
  CHECK((result & 0xFFFFFFu) == Tile(9 + holes, 8));
  CHECK((result >> 24) == (holes < 3 ? 1 : 0));
}

TEST_CASE("Gap scans stop safely at a room edge without wrapping",
          "[ai][ground-query]") {
  GroundFixture fixture;
  for (int x = 9; x < 32; ++x) fixture.flags[8][x] = 0x20;
  CHECK(fixture.Gap(Tile(9, 8), 6) == 0);
}

TEST_CASE(
    "Detour boundaries preserve low flags and original coordinate overlap",
    "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].navigationFlags = 0xAA155555;
  fixture.states[0].destination.word = Tile(31, 8);
  Battle_Ai_SetDetourBoundary(&fixture.states[0]);
  CHECK(fixture.states[0].navigationFlags ==
        ((0xAA155555u & 0x3FFFFFu) | (32u << 22)));
}

TEST_CASE("Detour progress detects five repeated tiles and preserves RNG order",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].movementEnabled = 1;
  const auto tile = fixture.states[0].tile.word;
  std::memcpy(fixture.states[0].detour_tile_bytes, &tile, sizeof(tile));
  fixture.states[0].detour_repeats = 4;
  fixture.states[0].movement_wait = 15;
  CHECK(Battle_Ai_RefreshDetourProgress(&fixture.states[0], 0x12345678) ==
        ((0x12345678u & 0xFFFFF01Fu) | (126u << 5)));
  CHECK(fixture.states[0].opaque19A == 120);
  CHECK(fixture.states[0].detour_repeats == 0);
  CHECK(fixture.states[0].movement_wait == 0);
  CHECK(Battle_Ai_FrameState()->randomState == 40);
}

TEST_CASE("Non-enabled detour progress draws once and retains the short plan",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  Battle_Ai_FrameState()->randomState = 3;
  const unsigned result =
      Battle_Ai_RefreshDetourProgress(&fixture.states[0], 0);
  CHECK(((result >> 5) & 127) == 31);
  CHECK(Battle_Ai_FrameState()->randomState == 4);
}

TEST_CASE("Cardinal passability visits exactly four ordered callbacks",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].candidateCount = 0;
  auto callback = [](Battle_Ai_MovementPathView* view, int direction) -> int {
    auto& state = *reinterpret_cast<Battle_Ai_ActorAiState*>(view);
    state.candidateCount = state.candidateCount * 10 + direction + 1;
    return direction == 0 || direction == 4;
  };
  CHECK(Battle_Ai_CardinalPassabilityMask(
            reinterpret_cast<Battle_Ai_MovementPathView*>(&fixture.states[0]),
            callback) == 10);
  CHECK(fixture.states[0].candidateCount == 1357);
}

TEST_CASE("Existing detour initialization does not consume random draws",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].opaque1 = 1;
  auto passable = [](Battle_Ai_MovementPathView*, int) { return 1; };
  CHECK(Battle_Ai_InitializeObstacleDetour(&fixture.states[0], 0x20,
                                           passable) == (0x20u | (15u << 18)));
  CHECK(fixture.states[0].detour_run == 1);
  CHECK(Battle_Ai_FrameState()->randomState == 40);
}

TEST_CASE("Hesitation only draws for a fresh non-poll request",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  CHECK(Battle_Ai_UpdateHesitationTurn(&fixture.states[0], 0, 3, 1) == 0);
  CHECK(Battle_Ai_FrameState()->randomState == 40);
  CHECK(Battle_Ai_UpdateHesitationTurn(&fixture.states[0], 0, 1, 1) == 0);
  CHECK(Battle_Ai_FrameState()->randomState == 41);
  CHECK(Battle_Ai_UpdateHesitationTurn(&fixture.states[0], 128, 1, 6) == 1);
  CHECK(fixture.states[0].movementModeCounters[12] == 1);
  CHECK(fixture.states[0].reactionPending == 69);
}

TEST_CASE("Hesitation phase thresholds execute the actual timed-turn consumer",
          "[ai][ground-motion]") {
  const int count = GENERATE(1, 2, 17, 18, 52, 53, 70);
  GroundFixture fixture;
  fixture.states[0].movementModeCounters[2] = 1;
  fixture.states[0].movementModeCounters[3] = static_cast<u_char>(count);
  CHECK(Battle_Ai_UpdateHesitationTurn(&fixture.states[0], 128, 1, 1) ==
        (count >= 2));
  CHECK(fixture.states[0].movementModeCounters[3] == count - 1);
  CHECK(fixture.states[0].heading == (count >= 53   ? 0x320
                                      : count >= 18 ? -0x320
                                                    : 0));
  CHECK(fixture.actors[0].commandOpcode == 6);
  CHECK(fixture.states[0].opaque13A == 64);
}

TEST_CASE("Vertical gait selection retains source priorities and output widths",
          "[ai][ground-motion]") {
  const int running = GENERATE(0, 1);
  GroundFixture fixture;
  fixture.states[0].opaque1 = static_cast<u_char>(running);
  fixture.states[0].locomotionFlags = 0x3010;
  fixture.states[0].walkStep = 17;
  fixture.states[0].runStep = 93;
  int value = -1, code = -1;
  CHECK(Battle_Ai_SelectActorVerticalMovementStep(&fixture.states[0], &value,
                                                  &code) == 1);
  CHECK(code == (running ? 10 : 9));
  CHECK(value == (running ? 93 : 17));
}

TEST_CASE("Room contact flags preserve all original shift/sign cases",
          "[ai][ground-motion]") {
  const unsigned bit = GENERATE(0u, 1u, 5u, 7u, 8u, 30u, 31u);
  GroundFixture fixture;
  fixture.room.reservedEPrefix[0] = 0x55;
  const unsigned mask = 1u << bit;
  Battle_Ai_SetAiContactOccupancy(static_cast<int>(bit));
  CHECK(fixture.room.reservedEPrefix[0] ==
        static_cast<u_char>(bit == 31 ? 0 : 0x55u | mask));
  fixture.room.reservedEPrefix[0] = 0x55;
  Battle_Ai_ClearAiContactOccupancy(static_cast<int>(bit));
  CHECK(fixture.room.reservedEPrefix[0] ==
        static_cast<u_char>(bit == 31 ? 0xFF : 0x55u & ~mask));
}

TEST_CASE(
    "Free ground dispatch emits a real walk packet toward a clear heading",
    "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].destination.word = Tile(10, 8);
  Battle_Ai_DispatchAiLocomotionMode(&fixture.states[0], 1, 3072);
  CHECK(fixture.states[0].isStuck == 1);
  CHECK(fixture.actors[0].commandOpcode == 9);
  CHECK(fixture.actors[0].commandState == 0x80);
}

TEST_CASE("Gate release precedes movement when the actor is off its link tile",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].route_gate = 3;
  fixture.room.reservedEPrefix[0] = 0xFF;
  Battle_Ai_MoveActorTowardDestinationTile(&fixture.states[0], 0);
  CHECK(fixture.states[0].route_gate == 0);
  CHECK(fixture.room.reservedEPrefix[0] == 0xF7);
  unsigned exit;
  std::memcpy(&exit, fixture.states[0].route_exit_bytes, sizeof(exit));
  CHECK(exit == 0xFFFFFFFFu);
  CHECK(fixture.actors[0].commandOpcode == 0);
}

TEST_CASE("Tile goals inside their radius stop through the real command API",
          "[ai][behavior-continuation]") {
  GroundFixture fixture;
  Battle_Ai_MoveToTile(
      reinterpret_cast<Battle_Ai_CandidateMovementView*>(&fixture.states[0]),
      Tile(8, 8), 64);
  CHECK(fixture.states[0].currentMovementMode == 2);
  CHECK(fixture.states[0].destination.word == Tile(8, 8));
  CHECK(fixture.actors[0].commandOpcode == 2);
}

TEST_CASE("Fleeing remembers a hostile position without changing enemy health",
          "[ai][behavior-continuation]") {
  GroundFixture fixture;
  fixture.states[0].relations[1].visible = 1;
  fixture.states[0].relations[1].classification = 3;
  CHECK(Battle_Ai_FleeVisibleEnemies(
            reinterpret_cast<Battle_Ai_CandidateMovementView*>(
                &fixture.states[0]),
            1) == -1);
  CHECK(fixture.states[0].flee_timer == 90);
  CHECK(fixture.states[0].flee_tile.word == Tile(23, 23));
  CHECK(fixture.states[0].currentMovementMode == 4);
  CHECK(fixture.states[0].requestedMovementMode == 4);
}

TEST_CASE("Flee memory expires without issuing a new movement packet",
          "[ai][behavior-continuation]") {
  GroundFixture fixture;
  fixture.states[0].flee_timer = 1;
  fixture.states[0].currentMovementMode = 4;
  CHECK(Battle_Ai_FleeVisibleEnemies(
            reinterpret_cast<Battle_Ai_CandidateMovementView*>(
                &fixture.states[0]),
            1) == 0);
  CHECK(fixture.states[0].flee_timer == 0);
  CHECK(fixture.actors[0].commandOpcode == 0);
}

TEST_CASE("Relationship queries retain list order and optional native tiles",
          "[ai][behavior-continuation]") {
  GroundFixture fixture;
  fixture.states[0].relations[1].classification = 2;
  fixture.models[1].instance.currentTileX = 9;
  fixture.models[1].instance.currentTileZ = 10;
  unsigned tile = 0;
  CHECK(Battle_Ai_FindActorByRelationship(&fixture.states[0], &tile, 2) == 1);
  CHECK(tile == Tile(9, 10));
  CHECK(Battle_Ai_FindActorByRelationship(&fixture.states[0], nullptr, 3) ==
        -1);
}

TEST_CASE(
    "Facing cosine uses native Q12 normalization and cardinal orientation",
    "[ai][behavior-continuation]") {
  SVECTOR from{};
  SVECTOR north{0, 0, -64, 0};
  SVECTOR south{0, 0, 64, 0};
  SVECTOR east{64, 0, 0, 0};
  CHECK(Battle_Ai_TargetFacingCosine(&from, &north, 0, 4096) == 4096);
  CHECK(Battle_Ai_TargetFacingCosine(&from, &south, 0, 4096) == -4096);
  CHECK(Battle_Ai_TargetFacingCosine(&from, &east, 0, 4096) == 0);
  CHECK(Battle_Ai_TargetFacingCosine(&from, &east, 3072, 4096) == 4096);
  CHECK(Battle_Ai_TargetFacingCosine(&from, &from, 0, 0) == 4096);
}

TEST_CASE("Creation-time null action contexts still propagate relationships",
          "[ai][behavior-continuation]") {
  GroundFixture fixture;
  fixture.states[1].usesRoomActionDelay = 1;
  Battle_Ai_ApplyActionRelationshipChanges(nullptr);
  CHECK(fixture.states[1].relations[0].classification == 2);
  CHECK(fixture.states[1].relations[1].classification == 2);
}

TEST_CASE(
    "Approach engagement gates retain discarded cooldown and room ordering",
    "[ai][behavior-continuation]") {
  const int branch = GENERATE(0, 1, 2, 3, 4, 5, 6);
  GroundFixture fixture;
  Battle_Ai_ActionCandidate candidate{};
  candidate.targetId = 1;
  fixture.states[0].actionDelay = 1;
  fixture.states[0].relations[1].active = branch == 2 ? 0 : 1;
  fixture.states[0].engagementFrames = branch == 0 ? 1 : 0;
  fixture.states[0].engagementCooldownFrames = branch == 1 ? 1 : 0;
  fixture.room.engagementLimit = 1;
  fixture.room.approachMoves = branch == 3 ? 2 : 0;
  fixture.room.engagedCount = branch == 6 ? 1 : 2;
  fixture.room.engaged_mask_bytes[0] = branch == 4 ? 1 : 0;
  CHECK(Battle_Ai_CanApproachEngagedTarget(&fixture.states[0], &candidate) ==
        (branch == 0 || branch == 4 || branch == 6));
}

TEST_CASE("Near target gating preserves the actual strict hundred cutoff",
          "[ai][behavior-continuation]") {
  const unsigned distance = GENERATE(0u, 99u, 100u, 101u, 0xFFFFFFFFu);
  const int range = GENERATE(-1000, 0, 2048);
  GroundFixture fixture;
  fixture.states[0].distancesSquared[1] = distance;
  CHECK(Battle_Ai_IsAiTargetBeyondNearRadius(&fixture.states[0], 1, range) ==
        (distance > 100));
}

TEST_CASE("Actor contact release clears the reserved moving-platform bit",
          "[ai][contact-release][occupancy-red]") {
  GroundFixture fixture;
  fixture.actors[1].commandOpcode = 11;
  fixture.states[1].nextTile.word = 2u << 8;
  fixture.models[1].instance.supportLayer = 0;
  fixture.room.reservedEPrefix[0] = 0xFF;
  Battle_Ai_ReleaseActorAiContactOccupancy(1);
  CHECK(fixture.room.reservedEPrefix[0] == 0xFB);
}

namespace {
u_char ExpectedContactRelease(unsigned command, unsigned layer, unsigned bit) {
  constexpr unsigned kInitial = 0xA5;
  if (((1u << (command & 31)) & 0x4800) == 0 || bit == 0 || layer != 0) {
    return kInitial;
  }
  const auto mask = ~(1u << (bit & 31));
  return static_cast<u_char>((mask & 0x80000000u) ? kInitial & mask
                                                  : kInitial | mask);
}

void CheckActorContactBits(GroundFixture& fixture, unsigned command,
                           unsigned layer) {
  for (unsigned bit = 0; bit < 256; ++bit) {
    fixture.states[1].nextTile.word = 0xA500005Au | (bit << 8);
    fixture.room.reservedEPrefix[0] = 0xA5;
    Battle_Ai_ReleaseActorAiContactOccupancy(1);
    CHECK(fixture.room.reservedEPrefix[0] ==
          ExpectedContactRelease(command, layer, bit));
  }
}
}  // namespace

TEST_CASE(
    "Actor contact release retains every command contact and support gate",
    "[ai][contact-release]") {
  GroundFixture fixture;
  for (unsigned command = 0; command < 256; ++command) {
    fixture.actors[1].commandOpcode = static_cast<u_char>(command);
    for (unsigned layer = 0; layer < 16; ++layer) {
      fixture.models[1].instance.supportLayer = static_cast<u_char>(layer);
      fixture.models[1].instance.placementLayer = 15;
      CheckActorContactBits(fixture, command, layer);
    }
  }
}

TEST_CASE("Actor contact release preserves source no-state and no-room gates",
          "[ai][contact-release]") {
  GroundFixture fixture;
  Battle_actors[1] = nullptr;
  Battle_Ai_actorAiStates[1] = nullptr;
  fixture.room.reservedEPrefix[0] = 0xA5;
  Battle_Ai_ReleaseActorAiContactOccupancy(1);
  CHECK(fixture.room.reservedEPrefix[0] == 0xA5);
  Battle_actors[1] = &fixture.actors[1];
  Battle_Ai_actorAiStates[1] = &fixture.states[1];
  fixture.actors[1].commandOpcode = 11;
  fixture.states[1].nextTile.word = 0x200;
  Battle_Ai_roomAiState = nullptr;
  Battle_Ai_ReleaseActorAiContactOccupancy(1);
  CHECK(fixture.room.reservedEPrefix[0] == 0xA5);
  Battle_Ai_roomAiState = &fixture.room;
  Battle_Ai_ReleaseActorAiContactOccupancy(1);
  CHECK(fixture.room.reservedEPrefix[0] == 0xA1);
}
