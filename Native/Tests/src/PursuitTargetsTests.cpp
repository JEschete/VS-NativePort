#include <algorithm>
#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/MovementBehavior.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Battle/Actor.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Damage.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

namespace {
struct PursuitFixture {
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  decltype(Field_roomData.movingPlatforms) saved_platforms =
      Field_roomData.movingPlatforms;
  decltype(Battle_Ai_routeGates) saved_gates = Battle_Ai_routeGates;
  std::array<Actors_ActorRecord, 2> models{};
  std::array<Battle_Actor, 2> actors{};
  std::array<Battle_Ai_ActorAiState, 2> states{};
  std::array<Field_MpdMovingPlatform, 4> platforms{};
  std::array<Battle_Ai_RouteGateEnd, 16> gates{};

  PursuitFixture() {
    std::fill_n(Actors_actorRecords, saved_models.size(), nullptr);
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Battle_Ai_FrameState()->actors, kAiFrameActorSlots, nullptr);
    for (unsigned index = 0; index < states.size(); ++index) {
      Actors_actorRecords[index] = &models[index];
      Battle_actors[index] = &actors[index];
      Battle_Ai_FrameState()->actors[index] = &states[index];
      actors[index].id = static_cast<int>(index);
      states[index].actorId = static_cast<u_char>(index);
      states[index].actor =
          reinterpret_cast<Battle_Ai_EngagementData*>(&actors[index]);
      states[index].model =
          reinterpret_cast<Battle_Ai_AiModelView*>(&models[index]);
    }
    Field_roomData.movingPlatforms = platforms.data();
    Battle_Ai_routeGates = gates.data();
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
    Approach().targetId = 1;
  }

  ~PursuitFixture() {
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    *Battle_Ai_FrameState() = saved_frame;
    Field_roomData.movingPlatforms = saved_platforms;
    Battle_Ai_routeGates = saved_gates;
  }

  Battle_Ai_TargetApproachView& Approach() {
    return *reinterpret_cast<Battle_Ai_TargetApproachView*>(&states[0]);
  }

  void UseVisibleInactiveTarget() {
    Approach().relations[1].opaque0Bit1 = 1;
    Approach().relations[1].visible = 1;
  }

  void SetPlatform(unsigned group) {
    platforms[group].currentWaypoint = 3;
    platforms[group].waypoints[3].vx = 4 << 19;
    platforms[group].waypoints[3].vz = 6 << 19;
    gates[group * 4 + 3].tileX = 4;
    gates[group * 4 + 3].tileZ = 6;
    gates[group * 4 + 2].tileX = static_cast<u_char>(12 + group);
    gates[group * 4 + 2].tileZ = static_cast<u_char>(9 + group);
  }
};
}  // namespace

TEST_CASE("Pursuit refresh reads signed position halfwords in subtile units",
          "[ai][pursuit-targets]") {
  const bool planned = GENERATE(false, true);
  PursuitFixture fixture;
  fixture.models[1].instance.movementMode = planned ? 1 : 0;
  fixture.Approach().reservedF5[0] = 0x55;
  fixture.Approach().reservedF7[0] = 0x66;
  Battle_Ai_RefreshAiTargetTile(&fixture.states[0]);
  CHECK(fixture.Approach().targetTileX == (planned ? 254 : 19));
  CHECK(fixture.Approach().targetTileZ == (planned ? 3 : 253));
  CHECK(fixture.Approach().reservedF5[0] == 0x55);
  CHECK(fixture.Approach().reservedF7[0] == 0x66);
}

TEST_CASE(
    "Pursuit refresh preserves coordinates when the cached target is gone",
    "[ai][pursuit-targets]") {
  PursuitFixture fixture;
  fixture.Approach().targetTileX = 17;
  fixture.Approach().targetTileZ = 18;
  Battle_Ai_FrameState()->actors[1] = nullptr;
  Actors_actorRecords[1] = nullptr;
  Battle_Ai_RefreshAiTargetTile(&fixture.states[0]);
  CHECK(fixture.Approach().targetTileX == 17);
  CHECK(fixture.Approach().targetTileZ == 18);
}

TEST_CASE("Conditional target refresh tests the requested movement mode",
          "[ai][pursuit-targets]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6);
  PursuitFixture fixture;
  fixture.states[0].requestedMovementMode = static_cast<u_char>(mode);
  fixture.states[0].currentMovementMode = 1;
  fixture.Approach().targetTileX = 17;
  fixture.Approach().targetTileZ = 18;
  Battle_Ai_RefreshAiTargetTileWhenTracking(
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&fixture.states[0]));
  CHECK(fixture.Approach().targetTileX == (mode == 1 ? 19 : 17));
  CHECK(fixture.Approach().targetTileZ == (mode == 1 ? 253 : 18));
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
  Battle_Ai_RefreshAiTargetTile(&fixture.states[0]);
  CHECK(fixture.Approach().targetTileX == (12 + group) * 2);
  CHECK(fixture.Approach().targetTileZ == (9 + group) * 2);
}

TEST_CASE("Approach retains the original missing-cache target-zero distinction",
          "[ai][pursuit-targets]") {
  const int target = GENERATE(0, 1);
  PursuitFixture fixture;
  Battle_Ai_FrameState()->actors[target] = nullptr;
  Actors_actorRecords[target] = nullptr;
  fixture.states[0].requestedMovementMode = 3;
  fixture.states[0].currentMovementMode = 2;
  CHECK(Battle_Ai_ApproachActor(&fixture.states[0], target, 640, 1) ==
        (target == 0 ? 0 : 3));
  CHECK(fixture.states[0].requestedMovementMode == (target == 0 ? 3 : 1));
  CHECK(fixture.states[0].currentMovementMode == (target == 0 ? 2 : 1));
}

TEST_CASE("Approach preserves the full current or planned destination word",
          "[ai][pursuit-targets]") {
  const bool planned = GENERATE(false, true);
  PursuitFixture fixture;
  fixture.UseVisibleInactiveTarget();
  fixture.models[1].instance.movementMode = planned ? 1 : 0;
  CHECK(Battle_Ai_ApproachActor(&fixture.states[0], 1, 640, 1) == 0);
  CHECK(fixture.states[0].destination.word ==
        (planned ? 0x330A0009u : 0x22060004u));
  CHECK(fixture.Approach().targetId == 1);
  CHECK(fixture.Approach().range == 640);
  CHECK(fixture.Approach().previousRange == 640);
  CHECK(fixture.Approach().previousOption == 1);
  CHECK(fixture.states[0].currentMovementMode == 0);
}

TEST_CASE("Approach hands a successful real consumer into the movement mode",
          "[ai][pursuit-targets]") {
  const int mode = GENERATE(0, 1);
  PursuitFixture fixture;
  fixture.states[0].currentMovementMode = static_cast<u_char>(mode);
  CHECK(Battle_Ai_ApproachActor(&fixture.states[0], 0, 640, 0) ==
        (mode == 0 ? 3 : 1));
  CHECK(fixture.actors[0].commandOpcode == 2);
  CHECK(fixture.actors[0].commandState == 0x80);
  CHECK(fixture.states[0].requestedMovementMode == 1);
  CHECK(fixture.states[0].currentMovementMode == 1);
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
  fixture.states[0].settingsBit14 = branch == 1 ? 1 : 0;
  fixture.states[0].movementFlags = branch == 2 ? 8 : 0;
  fixture.states[0].destination.word = 0xAABBCCDD;
  CHECK(Battle_Ai_ApproachActor(&fixture.states[0], 1, 640, 1) == 0);
  const unsigned expected = branch == 0 ? 0x3309020C : 0xAA09CC0C;
  CHECK(fixture.states[0].destination.word == expected);
  CHECK(fixture.Approach().cachedDestination.word == expected);
}
