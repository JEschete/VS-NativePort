#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/AreaRouteLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/FlightTerrain.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/WaypointsLive.hpp"
#include "Battle/Damage.h"
#include "Legacy/Psx.h"

namespace {
unsigned Tile(int x, int z) {
  return static_cast<unsigned>(x) | (static_cast<unsigned>(z) << 16);
}

struct GroundFixture {
  VS::Battle::Ai::FrameContext saved = VS::Battle::Ai::LiveFrameContext();
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(VS::Battle::Ai::actorAiStates) saved_states =
      VS::Battle::Ai::actorAiStates;
  decltype(VS::Battle::Ai::roomAiState) saved_room =
      VS::Battle::Ai::roomAiState;
  decltype(VS::Battle::Ai::tileFlagMap) saved_flags =
      VS::Battle::Ai::tileFlagMap;
  unsigned saved_lengths = VS::Content::battleAiHesitationLengths[0];
  VS::Battle::Ai::RoomAiState room{};
  std::array<Actors_ActorRecord, 2> models{};
  std::array<Battle_Actor, 2> actors{};
  std::array<VS::Battle::Ai::ActorAiState, 2> states{};
  u_short heights[32][32]{};
  u_short flags[32][32]{};
  std::array<u_char, 1024> regions{};

  GroundFixture() {
    auto& frame = VS::Battle::Ai::LiveFrameContext();
    std::memset(room.engaged_mask_bytes.data(), 0,
                sizeof room.engaged_mask_bytes);
    room.engaged_count = 0;
    frame.heightMap = heights;
    frame.tileFlags = flags;
    frame.roomBounds = 0x2020;
    frame.roomData = nullptr;
    frame.terrainRegions = regions.data();
    frame.terrain_region_count = regions.size();
    frame.room = &room;
    VS::Battle::Ai::roomAiState = &room;
    VS::Battle::Ai::tileFlagMap = flags;
    VS::Content::battleAiHesitationLengths[0] = 0x01184614;
    frame.randomState = 40;
    frame.lastBudgetTick = 1;
    const std::array<u_char, 8> directions{0x30, 0x33, 0x03, 0x13,
                                           0x10, 0x11, 0x01, 0x31};
    std::copy(directions.begin(), directions.end(), frame.directions);
    ClearCache();
    std::fill_n(frame.actors, VS::Battle::Ai::kFrameActorSlots, nullptr);
    std::fill_n(Actors_actorRecords, saved_models.size(), nullptr);
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved_states.size(),
                nullptr);
    for (unsigned index = 0; index < states.size(); ++index) {
      frame.actors[index] = &states[index];
      VS::Battle::Ai::actorAiStates[index] = &states[index];
      Actors_actorRecords[index] = &models[index];
      Battle_actors[index] = &actors[index];
      actors[index].id = static_cast<int>(index);
      states[index].actor_id = static_cast<u_char>(index);
      states[index].actor =
          reinterpret_cast<VS::Battle::Ai::EngagementData*>(&actors[index]);
      states[index].model = reinterpret_cast<VS::Battle::Ai::ModelStatePrefix*>(
          &models[index].instance);
      states[index].collision_radius = 32;
      states[index].body_height = 128;
      states[index].max_range = 32;
      states[index].reserved_b2 = 3;
      states[index].locomotion_flags = 0x42;
      states[index].walk_step = 32;
      states[index].run_step = 64;
      const std::uint16_t step = 128;
      std::memcpy(states[index].step_height_bytes.data(), &step, sizeof(step));
    }
    actors[0].next = &actors[1];
    states[0].tile.word = Tile(8, 8);
    states[0].previous_tile = -1;
    states[0].position.vx = states[0].position.vz = 1088;
    states[0].fine_x = states[0].fine_z = 17;
    models[0].instance.position = states[0].position;
    states[0].movement_target = 16;
    models[1].instance.position.vx = models[1].instance.position.vz = 3000;
  }

  ~GroundFixture() {
    VS::Battle::Ai::LiveFrameContext() = saved;
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_states.begin(), saved_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
    VS::Battle::Ai::roomAiState = saved_room;
    VS::Battle::Ai::tileFlagMap = saved_flags;
    VS::Content::battleAiHesitationLengths[0] = saved_lengths;
  }

  void ClearCache() {
    auto& frame = VS::Battle::Ai::LiveFrameContext();
    frame.ClearDirectionTests();
    std::fill(std::begin(frame.blocker_actor), std::end(frame.blocker_actor),
              0);
  }

  int Occupancy(unsigned tile) {
    return VS::Battle::Ai::LiveDestinationOccupancy(
        reinterpret_cast<VS::Battle::Ai::MoveTargetView*>(&states[0]), tile);
  }

  int Blocker(int direction) {
    return VS::Battle::Ai::LiveDirectionalBlockingActor(
        reinterpret_cast<VS::Battle::Ai::MovementPathView*>(&states[0]),
        direction);
  }

  int Walk(int direction) {
    return VS::Battle::Ai::CanLiveActorWalkInDirection(
        reinterpret_cast<VS::Battle::Ai::WanderView*>(&states[0]), direction);
  }

  int Step(unsigned to, int direction) {
    return VS::Battle::Ai::CanTraverseLiveTileEdge(
        reinterpret_cast<VS::Battle::Ai::MoveTargetView*>(&states[0]),
        states[0].tile.word, to, direction);
  }

  unsigned Gap(unsigned tile, int direction) {
    return VS::Battle::Ai::FindLiveGapLandingTile(
        reinterpret_cast<VS::Battle::Ai::MoveTargetView*>(&states[0]), tile,
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
  fixture.states[0].previous_tile = static_cast<int>(0xAA080009);
  CHECK(fixture.Occupancy(Tile(9, 8)) == 0);
  fixture.states[0].previous_tile = -1;
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
  fixture.room.min_tile_x = 0;
  fixture.room.max_tile_x = 31;
  fixture.room.min_tile_z = 0;
  fixture.room.max_tile_z = 31;

  auto& state = fixture.states[1];
  state.tile.word = Tile(8, 13) | 0x40000000u;
  state.previous_tile = -1;
  fixture.heights[13][9] = 1023;
  CHECK(VS::Battle::Ai::LowestReachableGroundHeight(
            VS::Battle::Ai::ActorFlightNeighborhood(state), 6 << 9) == -64);

  state.tile.word = Tile(31, 13) | 0x40000000u;
  fixture.ClearCache();
  CHECK(VS::Battle::Ai::LowestReachableGroundHeight(
            VS::Battle::Ai::ActorFlightNeighborhood(state), 6 << 9) == 0);
}

TEST_CASE("Flight terrain helpers preserve sampled heights and room ceiling",
          "[ai][flight][terrain]") {
  GroundFixture fixture;
  fixture.heights[13][9] = 1023;
  CHECK(VS::Battle::Ai::MinimumAiGroundHeightAlongSegment(
            VS::Battle::Ai::CurrentTerrain(), 8 * 128 + 64, 13 * 128 + 64,
            10 * 128 + 64, 13 * 128 + 64) == -64);

  fixture.room.flight_ceiling = 512;
  auto& runtime = VS::Battle::Ai::BattleRoomAiRuntime();
  CHECK(VS::Battle::Ai::RoomNavigationCeilingHeight(runtime) == 512);
  VS::Battle::Ai::roomAiState = nullptr;
  CHECK(VS::Battle::Ai::RoomNavigationCeilingHeight(runtime) == 0);
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
  fixture.states[1].next_position.vx = 1216;
  fixture.states[1].next_position.vz = 1088;
  CHECK(fixture.Occupancy(Tile(9, 8)) == -1);
  VS::Battle::Ai::LiveFrameContext().actors[1] = nullptr;
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
  CHECK(VS::Battle::Ai::LiveFrameContext().blocker_valid == 0x40);
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
  CHECK((VS::Battle::Ai::LiveFrameContext().walk_valid & (1u << direction)) !=
        0);
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
  fixture.states[0].settings_bit14 = 1;
  CHECK(fixture.Walk(6) == 0);
  fixture.states[0].reserved154_area = 1;
  fixture.ClearCache();
  CHECK(fixture.Walk(6) == 1);
  fixture.flags[8][9] = 0;
  VS::Battle::Ai::LiveFrameContext().roomData = fixture.regions.data();
  fixture.heights[8][9] = 0x400;
  fixture.ClearCache();
  CHECK(fixture.Walk(6) == 0);
}

TEST_CASE("Tile steps enforce configured gaps, slopes and movement disable",
          "[ai][ground-query]") {
  GroundFixture fixture;
  CHECK(fixture.Step(Tile(9, 8), 6) == 1);
  fixture.states[0].movement_disabled = 1;
  CHECK(fixture.Step(Tile(9, 8), 6) == 0);
  fixture.states[0].movement_disabled = 0;
  fixture.states[0].opaque3 = 1;
  fixture.heights[8][9] = 3;
  fixture.ClearCache();
  CHECK(fixture.Step(Tile(9, 8), 6) == 0);
  fixture.states[0].movement_flags = 8;
  fixture.ClearCache();
  CHECK(fixture.Step(Tile(9, 8), 6) == 1);
}

TEST_CASE("Tile steps use typed target positions for the approach radius",
          "[ai][ground-query]") {
  GroundFixture fixture;
  auto& approach = *reinterpret_cast<VS::Battle::Ai::TargetApproachView*>(
      &fixture.states[0]);
  approach.target_id = 1;
  approach.range = 160;
  fixture.states[0].opaque5 = 1;
  fixture.states[0].requested_movement_mode = 1;
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
  fixture.states[0].navigation_flags = 0xAA155555;
  fixture.states[0].destination.word = Tile(31, 8);
  VS::Battle::Ai::SetLiveDetourBoundary(&fixture.states[0]);
  CHECK(fixture.states[0].navigation_flags ==
        ((0xAA155555u & 0x3FFFFFu) | (32u << 22)));
}

TEST_CASE("Detour progress detects five repeated tiles and preserves RNG order",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].movement_enabled = 1;
  const auto tile = fixture.states[0].tile.word;
  std::memcpy(fixture.states[0].detour_tile_bytes.data(), &tile, sizeof(tile));
  fixture.states[0].detour_repeats = 4;
  fixture.states[0].movement_wait = 15;
  CHECK(VS::Battle::Ai::RefreshLiveDetourProgress(&fixture.states[0],
                                                  0x12345678) ==
        ((0x12345678u & 0xFFFFF01Fu) | (126u << 5)));
  CHECK(fixture.states[0].opaque_19a == 120);
  CHECK(fixture.states[0].detour_repeats == 0);
  CHECK(fixture.states[0].movement_wait == 0);
  CHECK(VS::Battle::Ai::LiveFrameContext().randomState == 40);
}

TEST_CASE("Non-enabled detour progress draws once and retains the short plan",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  VS::Battle::Ai::LiveFrameContext().randomState = 3;
  const unsigned result =
      VS::Battle::Ai::RefreshLiveDetourProgress(&fixture.states[0], 0);
  CHECK(((result >> 5) & 127) == 31);
  CHECK(VS::Battle::Ai::LiveFrameContext().randomState == 4);
}

TEST_CASE("Cardinal passability visits exactly four ordered callbacks",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].candidate_count = 0;
  auto callback = [](VS::Battle::Ai::MovementPathView* view,
                     int direction) -> int {
    auto& state = *reinterpret_cast<VS::Battle::Ai::ActorAiState*>(view);
    state.candidate_count = state.candidate_count * 10 + direction + 1;
    return direction == 0 || direction == 4;
  };
  CHECK(VS::Battle::Ai::LiveCardinalPassabilityMask(
            reinterpret_cast<VS::Battle::Ai::MovementPathView*>(
                &fixture.states[0]),
            callback) == 10);
  CHECK(fixture.states[0].candidate_count == 1357);
}

TEST_CASE("Existing detour initialization does not consume random draws",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].opaque1 = 1;
  auto passable = [](VS::Battle::Ai::MovementPathView*, int) { return 1; };
  CHECK(VS::Battle::Ai::InitializeLiveObstacleDetour(
            &fixture.states[0], 0x20, passable) == (0x20u | (15u << 18)));
  CHECK(fixture.states[0].detour_run == 1);
  CHECK(VS::Battle::Ai::LiveFrameContext().randomState == 40);
}

TEST_CASE("Hesitation only draws for a fresh non-poll request",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  CHECK(VS::Battle::Ai::UpdateLiveHesitationTurn(&fixture.states[0], 0, 3, 1) ==
        0);
  CHECK(VS::Battle::Ai::LiveFrameContext().randomState == 40);
  CHECK(VS::Battle::Ai::UpdateLiveHesitationTurn(&fixture.states[0], 0, 1, 1) ==
        0);
  CHECK(VS::Battle::Ai::LiveFrameContext().randomState == 41);
  CHECK(VS::Battle::Ai::UpdateLiveHesitationTurn(&fixture.states[0], 128, 1,
                                                 6) == 1);
  CHECK(fixture.states[0].movement_mode_counters[12] == 1);
  CHECK(fixture.states[0].reaction_pending == 69);
}

TEST_CASE("Hesitation phase thresholds execute the actual timed-turn consumer",
          "[ai][ground-motion]") {
  const int count = GENERATE(1, 2, 17, 18, 52, 53, 70);
  GroundFixture fixture;
  fixture.states[0].movement_mode_counters[2] = 1;
  fixture.states[0].movement_mode_counters[3] = static_cast<u_char>(count);
  CHECK(VS::Battle::Ai::UpdateLiveHesitationTurn(&fixture.states[0], 128, 1,
                                                 1) == (count >= 2));
  CHECK(fixture.states[0].movement_mode_counters[3] == count - 1);
  CHECK(fixture.states[0].heading == (count >= 53   ? 0x320
                                      : count >= 18 ? -0x320
                                                    : 0));
  CHECK(fixture.actors[0].commandOpcode == 6);
  CHECK(fixture.states[0].opaque_13a == 64);
}

TEST_CASE("Vertical gait selection retains source priorities and output widths",
          "[ai][ground-motion]") {
  const int running = GENERATE(0, 1);
  GroundFixture fixture;
  fixture.states[0].opaque1 = static_cast<u_char>(running);
  fixture.states[0].locomotion_flags = 0x3010;
  fixture.states[0].walk_step = 17;
  fixture.states[0].run_step = 93;
  int value = -1, code = -1;
  CHECK(VS::Battle::Ai::SelectLiveActorVerticalMovementStep(
            &fixture.states[0], &value, &code) == 1);
  CHECK(code == (running ? 10 : 9));
  CHECK(value == (running ? 93 : 17));
}

TEST_CASE("Room contact flags preserve all original shift/sign cases",
          "[ai][ground-motion]") {
  const unsigned bit = GENERATE(0u, 1u, 5u, 7u, 8u, 30u, 31u);
  GroundFixture fixture;
  fixture.room.reserved_e_prefix[0] = 0x55;
  const unsigned mask = 1u << bit;
  VS::Battle::Ai::SetLiveAiContactOccupancy(static_cast<int>(bit));
  CHECK(fixture.room.reserved_e_prefix[0] ==
        static_cast<u_char>(bit == 31 ? 0 : 0x55u | mask));
  fixture.room.reserved_e_prefix[0] = 0x55;
  VS::Battle::Ai::ClearLiveAiContactOccupancy(static_cast<int>(bit));
  CHECK(fixture.room.reserved_e_prefix[0] ==
        static_cast<u_char>(bit == 31 ? 0xFF : 0x55u & ~mask));
}

TEST_CASE(
    "Free ground dispatch emits a real walk packet toward a clear heading",
    "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].destination.word = Tile(10, 8);
  VS::Battle::Ai::DispatchLiveAiLocomotionMode(&fixture.states[0], 1, 3072);
  CHECK(fixture.states[0].is_stuck == 1);
  CHECK(fixture.actors[0].commandOpcode == 9);
  CHECK(fixture.actors[0].commandState == 0x80);
}

TEST_CASE("Gate release precedes movement when the actor is off its link tile",
          "[ai][ground-motion]") {
  GroundFixture fixture;
  fixture.states[0].route_gate = 3;
  fixture.room.reserved_e_prefix[0] = 0xFF;
  VS::Battle::Ai::MoveLiveActorTowardDestinationTile(&fixture.states[0], 0);
  CHECK(fixture.states[0].route_gate == 0);
  CHECK(fixture.room.reserved_e_prefix[0] == 0xF7);
  unsigned exit;
  std::memcpy(&exit, fixture.states[0].route_exit_bytes.data(), sizeof(exit));
  CHECK(exit == 0xFFFFFFFFu);
  CHECK(fixture.actors[0].commandOpcode == 0);
}

TEST_CASE("Tile goals inside their radius stop through the real command API",
          "[ai][behavior-continuation]") {
  GroundFixture fixture;
  VS::Battle::Ai::MoveLiveActorToTile(
      reinterpret_cast<VS::Battle::Ai::CandidateMovementView*>(
          &fixture.states[0]),
      Tile(8, 8), 64);
  CHECK(fixture.states[0].current_movement_mode == 2);
  CHECK(fixture.states[0].destination.word == Tile(8, 8));
  CHECK(fixture.actors[0].commandOpcode == 2);
}

TEST_CASE("Fleeing remembers a hostile position without changing enemy health",
          "[ai][behavior-continuation]") {
  GroundFixture fixture;
  fixture.states[0].relations[1].visible = 1;
  fixture.states[0].relations[1].classification = 3;
  CHECK(VS::Battle::Ai::FleeLiveVisibleEnemies(
            reinterpret_cast<VS::Battle::Ai::CandidateMovementView*>(
                &fixture.states[0]),
            1) == -1);
  CHECK(fixture.states[0].flee_timer == 90);
  CHECK(fixture.states[0].flee_tile.word == Tile(23, 23));
  CHECK(fixture.states[0].current_movement_mode == 4);
  CHECK(fixture.states[0].requested_movement_mode == 4);
}

TEST_CASE("Flee memory expires without issuing a new movement packet",
          "[ai][behavior-continuation]") {
  GroundFixture fixture;
  fixture.states[0].flee_timer = 1;
  fixture.states[0].current_movement_mode = 4;
  CHECK(VS::Battle::Ai::FleeLiveVisibleEnemies(
            reinterpret_cast<VS::Battle::Ai::CandidateMovementView*>(
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
  CHECK(VS::Battle::Ai::FindLiveActorByRelationship(&fixture.states[0], &tile,
                                                    2) == 1);
  CHECK(tile == Tile(9, 10));
  CHECK(VS::Battle::Ai::FindLiveActorByRelationship(&fixture.states[0], nullptr,
                                                    3) == -1);
}

TEST_CASE(
    "Facing cosine uses native Q12 normalization and cardinal orientation",
    "[ai][behavior-continuation]") {
  SVECTOR from{};
  SVECTOR north{0, 0, -64, 0};
  SVECTOR south{0, 0, 64, 0};
  SVECTOR east{64, 0, 0, 0};
  CHECK(VS::Battle::Ai::LiveTargetFacingCosine(&from, &north, 0, 4096) == 4096);
  CHECK(VS::Battle::Ai::LiveTargetFacingCosine(&from, &south, 0, 4096) ==
        -4096);
  CHECK(VS::Battle::Ai::LiveTargetFacingCosine(&from, &east, 0, 4096) == 0);
  CHECK(VS::Battle::Ai::LiveTargetFacingCosine(&from, &east, 3072, 4096) ==
        4096);
  CHECK(VS::Battle::Ai::LiveTargetFacingCosine(&from, &from, 0, 0) == 4096);
}

TEST_CASE("Creation-time null action contexts still propagate relationships",
          "[ai][behavior-continuation]") {
  GroundFixture fixture;
  fixture.states[1].uses_room_action_delay = 1;
  VS::Battle::Ai::ApplyLiveActionRelationshipChanges(nullptr);
  CHECK(fixture.states[1].relations[0].classification == 2);
  CHECK(fixture.states[1].relations[1].classification == 2);
}

TEST_CASE(
    "Approach engagement gates retain discarded cooldown and room ordering",
    "[ai][behavior-continuation]") {
  const int branch = GENERATE(0, 1, 2, 3, 4, 5, 6);
  GroundFixture fixture;
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  candidate.target_id = 1;
  fixture.states[0].action_delay = 1;
  fixture.states[0].relations[1].active = branch == 2 ? 0 : 1;
  fixture.states[0].engagement_frames = branch == 0 ? 1 : 0;
  fixture.states[0].engagement_cooldown_frames = branch == 1 ? 1 : 0;
  fixture.room.engagement_limit = 1;
  fixture.room.approach_moves = branch == 3 ? 2 : 0;
  fixture.room.engaged_count = branch == 6 ? 1 : 2;
  fixture.room.engaged_mask_bytes[0] = branch == 4 ? 1 : 0;
  CHECK(VS::Battle::Ai::CanApproachLiveEngagedTarget(&fixture.states[0],
                                                     &candidate) ==
        (branch == 0 || branch == 4 || branch == 6));
}

TEST_CASE("Near target gating preserves the actual strict hundred cutoff",
          "[ai][behavior-continuation]") {
  const unsigned distance = GENERATE(0u, 99u, 100u, 101u, 0xFFFFFFFFu);
  const int range = GENERATE(-1000, 0, 2048);
  GroundFixture fixture;
  fixture.states[0].distances_squared[1] = distance;
  CHECK(VS::Battle::Ai::IsLiveAiTargetBeyondNearRadius(
            &fixture.states[0], 1, range) == (distance > 100));
}

TEST_CASE("Actor contact release clears the reserved moving-platform bit",
          "[ai][contact-release][occupancy-red]") {
  GroundFixture fixture;
  fixture.actors[1].commandOpcode = 11;
  fixture.states[1].next_tile.word = 2u << 8;
  fixture.models[1].instance.supportLayer = 0;
  fixture.room.reserved_e_prefix[0] = 0xFF;
  VS::Battle::Ai::ReleaseLiveActorAiContactOccupancy(1);
  CHECK(fixture.room.reserved_e_prefix[0] == 0xFB);
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
    fixture.states[1].next_tile.word = 0xA500005Au | (bit << 8);
    fixture.room.reserved_e_prefix[0] = 0xA5;
    VS::Battle::Ai::ReleaseLiveActorAiContactOccupancy(1);
    CHECK(fixture.room.reserved_e_prefix[0] ==
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
  VS::Battle::Ai::actorAiStates[1] = nullptr;
  fixture.room.reserved_e_prefix[0] = 0xA5;
  VS::Battle::Ai::ReleaseLiveActorAiContactOccupancy(1);
  CHECK(fixture.room.reserved_e_prefix[0] == 0xA5);
  Battle_actors[1] = &fixture.actors[1];
  VS::Battle::Ai::actorAiStates[1] = &fixture.states[1];
  fixture.actors[1].commandOpcode = 11;
  fixture.states[1].next_tile.word = 0x200;
  VS::Battle::Ai::roomAiState = nullptr;
  VS::Battle::Ai::ReleaseLiveActorAiContactOccupancy(1);
  CHECK(fixture.room.reserved_e_prefix[0] == 0xA5);
  VS::Battle::Ai::roomAiState = &fixture.room;
  VS::Battle::Ai::ReleaseLiveActorAiContactOccupancy(1);
  CHECK(fixture.room.reserved_e_prefix[0] == 0xA1);
}

TEST_CASE("Live patrols store the cursor and walk to the next waypoint",
          "[ai][waypoints][waypoints-live][ground-motion]") {
  GroundFixture fixture;
  auto* const saved_routes = VS::Battle::Ai::patrolRoutes;
  std::uint16_t routes[4][8]{};
  routes[1][0] = 0x8000 | (8 << 5) | 8;
  routes[1][1] = 0x8000 | (8 << 5) | 10;
  VS::Battle::Ai::patrolRoutes = routes;
  auto& state = fixture.states[0];
  state.patrol_row = 1;
  state.patrol_rate = 3;
  state.patrol_direction = 1;
  VS::Battle::Ai::UpdateLiveWaypointPatrol(
      reinterpret_cast<VS::Battle::Ai::PatrolView*>(&state));
  VS::Battle::Ai::patrolRoutes = saved_routes;
  CHECK(state.patrol_index == 1);
  CHECK(state.patrol_row == 1);
  CHECK(state.patrol_rate == 3);
  CHECK(state.patrol_direction == 1);
  CHECK(state.patrol_cooldown == 10);
  CHECK(state.destination.word == Tile(10, 8));
  CHECK(fixture.actors[0].commandOpcode == 9);
}

TEST_CASE("Live area movement clears one cache byte when the actor is stuck",
          "[ai][area-movement][area-route-live][ground-motion]") {
  const bool stuck = GENERATE(false, true);
  GroundFixture fixture;
  auto& state = fixture.states[0];
  fixture.flags[8][8] = 1;
  fixture.flags[8][10] = stuck ? 2 : 1;
  state.destination.word = Tile(10, 8);
  state.opaque4 = 5;
  state.move_x = 1344;
  state.move_z = 1088;
  auto& frame = VS::Battle::Ai::LiveFrameContext();
  frame.walk_valid = 0xFF;
  frame.walk_passable = 0xFF;
  frame.blocker_valid = 0xFF;
  frame.reserved_cache = 0x5A;
  VS::Battle::Ai::UpdateLiveNavigationAreaMovement(
      reinterpret_cast<VS::Battle::Ai::TerrainMoveView*>(&state));
  CHECK(state.opaque4 == 0);
  CHECK(state.opaque5 == 1);
  CHECK(frame.blocker_valid == 0xFF);
  CHECK(frame.reserved_cache == 0x5A);
  CHECK((frame.walk_valid == 0xFF) == !stuck);
  if (stuck) CHECK(state.is_stuck == 1);
  CHECK(fixture.actors[0].commandOpcode == 9);
}
