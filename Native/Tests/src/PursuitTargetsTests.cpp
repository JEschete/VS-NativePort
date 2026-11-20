#include <algorithm>
#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Damage.h"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Legacy/Psx.h"

namespace {
struct PursuitFixture {
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  VS::Battle::Ai::FrameContext saved_frame = VS::Battle::Ai::LiveFrameContext();
  decltype(VS::Field::roomData.moving_platforms) saved_platforms =
      VS::Field::roomData.moving_platforms;
  decltype(VS::Field::roomData.header.moving_platforms_length)
      saved_platform_bytes = VS::Field::roomData.header.moving_platforms_length;
  decltype(VS::Battle::Ai::routeGates) saved_gates = VS::Battle::Ai::routeGates;
  std::array<Actors_ActorRecord, 2> models{};
  std::array<Battle_Actor, 2> actors{};
  std::array<VS::Battle::Ai::ActorAiState, 2> states{};
  std::array<VS::Field::MpdMovingPlatform, 4> platforms{};
  std::array<VS::Battle::Ai::RouteGateEndRecord, 16> gates{};

  PursuitFixture() {
    std::fill_n(Actors_actorRecords, saved_models.size(), nullptr);
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(VS::Battle::Ai::LiveFrameContext().actors,
                VS::Battle::Ai::kFrameActorSlots, nullptr);
    for (unsigned index = 0; index < states.size(); ++index) {
      Actors_actorRecords[index] = &models[index];
      Battle_actors[index] = &actors[index];
      VS::Battle::Ai::LiveFrameContext().actors[index] = &states[index];
      actors[index].id = static_cast<int>(index);
      states[index].actor_id = static_cast<u_char>(index);
      states[index].actor =
          reinterpret_cast<VS::Battle::Ai::EngagementData*>(&actors[index]);
      states[index].model = reinterpret_cast<VS::Battle::Ai::ModelStatePrefix*>(
          &models[index].instance);
    }
    VS::Field::roomData.moving_platforms = platforms.data();
    VS::Field::roomData.header.moving_platforms_length = sizeof platforms;
    VS::Battle::Ai::routeGates = gates.data();
    auto& model = models[1].instance;
    model.currentTileX = 4;
    model.currentTileZ = 6;
    model.reserved5F = 0x22;
    model.previousTileX = 9;
    model.previousTileZ = 10;
    model.reserved63 = 0x33;
    model.position.vx = 1234;
    model.position.vz = -129;
    model.supportOffset = 0x1234FFBF;
    model.reserved50 = 193;
    Approach().target_id = 1;
  }

  ~PursuitFixture() {
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    VS::Battle::Ai::LiveFrameContext() = saved_frame;
    VS::Field::roomData.moving_platforms = saved_platforms;
    VS::Field::roomData.header.moving_platforms_length = saved_platform_bytes;
    VS::Battle::Ai::routeGates = saved_gates;
  }

  VS::Battle::Ai::TargetApproachView& Approach() {
    return *reinterpret_cast<VS::Battle::Ai::TargetApproachView*>(&states[0]);
  }

  void UseVisibleInactiveTarget() {
    Approach().relations[1].opaque0_bit1 = 1;
    Approach().relations[1].visible = 1;
  }

  void SetPlatform(unsigned group) {
    platforms[group].current_waypoint = 3;
    platforms[group].waypoints[3].vx = 4 << 19;
    platforms[group].waypoints[3].vz = 6 << 19;
    gates[group * 4 + 3].tile_x = 4;
    gates[group * 4 + 3].tile_z = 6;
    gates[group * 4 + 2].tile_x = static_cast<u_char>(12 + group);
    gates[group * 4 + 2].tile_z = static_cast<u_char>(9 + group);
  }
};
}  // namespace

TEST_CASE("Pursuit refresh reads signed position halfwords in subtile units",
          "[ai][pursuit-targets]") {
  const bool planned = GENERATE(false, true);
  PursuitFixture fixture;
  fixture.models[1].instance.movementMode = planned ? 1 : 0;
  fixture.Approach().reserved_f5[0] = 0x55;
  fixture.Approach().reserved_f7[0] = 0x66;
  VS::Battle::Ai::RefreshLiveAiTargetTile(&fixture.Approach());
  CHECK(fixture.Approach().target_tile_x == (planned ? 254 : 19));
  CHECK(fixture.Approach().target_tile_z == (planned ? 3 : 253));
  CHECK(fixture.Approach().reserved_f5[0] == 0x55);
  CHECK(fixture.Approach().reserved_f7[0] == 0x66);
}

TEST_CASE(
    "Pursuit refresh preserves coordinates when the cached target is gone",
    "[ai][pursuit-targets]") {
  PursuitFixture fixture;
  fixture.Approach().target_tile_x = 17;
  fixture.Approach().target_tile_z = 18;
  VS::Battle::Ai::LiveFrameContext().actors[1] = nullptr;
  Actors_actorRecords[1] = nullptr;
  VS::Battle::Ai::RefreshLiveAiTargetTile(&fixture.Approach());
  CHECK(fixture.Approach().target_tile_x == 17);
  CHECK(fixture.Approach().target_tile_z == 18);
}

TEST_CASE("Conditional target refresh tests the requested movement mode",
          "[ai][pursuit-targets]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6);
  PursuitFixture fixture;
  fixture.states[0].requested_movement_mode = static_cast<u_char>(mode);
  fixture.states[0].current_movement_mode = 1;
  fixture.Approach().target_tile_x = 17;
  fixture.Approach().target_tile_z = 18;
  VS::Battle::Ai::RefreshLiveAiTargetTileWhenTracking(
      reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(
          &fixture.states[0]));
  CHECK(fixture.Approach().target_tile_x == (mode == 1 ? 19 : 17));
  CHECK(fixture.Approach().target_tile_z == (mode == 1 ? 253 : 18));
}

TEST_CASE("Pursuit refresh resolves current and planned moving-platform gates",
          "[ai][pursuit-targets]") {
  const bool planned = GENERATE(false, true);
  const unsigned group = GENERATE(0u, 3u);
  PursuitFixture fixture;
  fixture.SetPlatform(group);
  auto& model = fixture.models[1].instance;
  model.movementMode = planned ? 1 : 0;
  model.currentTileLayer = model.previousTileLayer =
      static_cast<u_char>(group + 2);
  VS::Battle::Ai::RefreshLiveAiTargetTile(&fixture.Approach());
  CHECK(fixture.Approach().target_tile_x == (12 + group) * 2);
  CHECK(fixture.Approach().target_tile_z == (9 + group) * 2);
}

TEST_CASE("Approach retains the original missing-cache target-zero distinction",
          "[ai][pursuit-targets]") {
  const int target = GENERATE(0, 1);
  PursuitFixture fixture;
  VS::Battle::Ai::LiveFrameContext().actors[target] = nullptr;
  Actors_actorRecords[target] = nullptr;
  fixture.states[0].requested_movement_mode = 3;
  fixture.states[0].current_movement_mode = 2;
  CHECK(VS::Battle::Ai::ApproachLiveActor(&fixture.states[0], target, 640, 1) ==
        (target == 0 ? 0 : 3));
  CHECK(fixture.states[0].requested_movement_mode == (target == 0 ? 3 : 1));
  CHECK(fixture.states[0].current_movement_mode == (target == 0 ? 2 : 1));
}

TEST_CASE("Approach preserves the full current or planned destination word",
          "[ai][pursuit-targets]") {
  const bool planned = GENERATE(false, true);
  PursuitFixture fixture;
  fixture.UseVisibleInactiveTarget();
  fixture.models[1].instance.movementMode = planned ? 1 : 0;
  CHECK(VS::Battle::Ai::ApproachLiveActor(&fixture.states[0], 1, 640, 1) == 0);
  CHECK(fixture.states[0].destination.word ==
        (planned ? 0x330A0009u : 0x22060004u));
  CHECK(fixture.Approach().target_id == 1);
  CHECK(fixture.Approach().range == 640);
  CHECK(fixture.Approach().previous_range == 640);
  CHECK(fixture.Approach().previous_option == 1);
  CHECK(fixture.states[0].current_movement_mode == 0);
}

TEST_CASE("Approach hands a successful real consumer into the movement mode",
          "[ai][pursuit-targets]") {
  const int mode = GENERATE(0, 1);
  PursuitFixture fixture;
  fixture.states[0].current_movement_mode = static_cast<u_char>(mode);
  CHECK(VS::Battle::Ai::ApproachLiveActor(&fixture.states[0], 0, 640, 0) ==
        (mode == 0 ? 3 : 1));
  CHECK(fixture.actors[0].commandOpcode == 2);
  CHECK(fixture.actors[0].commandState == 0x80);
  CHECK(fixture.states[0].requested_movement_mode == 1);
  CHECK(fixture.states[0].current_movement_mode == 1);
}

TEST_CASE("Approach platform selection preserves the original other tile bytes",
          "[ai][pursuit-targets]") {
  const int branch = GENERATE(0, 1, 2);
  PursuitFixture fixture;
  fixture.UseVisibleInactiveTarget();
  fixture.SetPlatform(0);
  fixture.models[1].instance.movementMode = branch == 1 ? 0 : 1;
  fixture.models[1].instance.previousTileLayer = 2;
  fixture.models[1].instance.supportLayer = 2;
  fixture.states[0].settings_bit14 = branch == 1 ? 1 : 0;
  fixture.states[0].movement_flags = branch == 2 ? 8 : 0;
  fixture.states[0].destination.word = 0xAABBCCDD;
  CHECK(VS::Battle::Ai::ApproachLiveActor(&fixture.states[0], 1, 640, 1) == 0);
  const unsigned expected = branch == 0 ? 0x3309020C : 0xAA09CC0C;
  CHECK(fixture.states[0].destination.word == expected);
  CHECK(fixture.Approach().cached_destination.word == expected);
}
