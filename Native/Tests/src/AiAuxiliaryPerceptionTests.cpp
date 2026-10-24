#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Field/Gimmicks.h"
}

namespace {
static_assert(offsetof(Actors_ModelInstance, position) ==
              offsetof(Field_GimmickInstance, positionX));
static_assert(offsetof(Battle_Ai_PerceivedObjectView, position) ==
              offsetof(Actors_ModelInstance, position));

struct AuxiliaryPerceptionFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Field_gimmickInstances)) saved_gimmicks =
      std::to_array(Field_gimmickInstances);
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  std::array<Actors_ActorRecord, 2> records{};
  std::array<Battle_Actor, 3> actors{};
  Battle_Ai_ActorAiState owner{};
  Battle_Ai_ActorAiState target{};
  Field_GimmickInstance auxiliary{};
  u_short flags[32][32]{};

  explicit AuxiliaryPerceptionFixture(bool auxiliary_target,
                                      bool target_ai = true) {
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_records.size(), nullptr);
    std::fill_n(Field_gimmickInstances, saved_gimmicks.size(), nullptr);
    std::fill_n(Battle_Ai_actorAiStates, saved_states.size(), nullptr);
    auto& frame = *Battle_Ai_FrameState();
    frame = {};
    frame.tileFlags = flags;
    frame.actors[0] = Battle_Ai_actorAiStates[0] = &owner;
    if (target_ai) frame.actors[2] = Battle_Ai_actorAiStates[2] = &target;
    records[0].instance.position = {16, 0, 16, 0};
    records[1].instance.position = {16, 0, 112, 0};
    Actors_actorRecords[0] = &records[0];
    Actors_actorRecords[2] = &records[1];
    actors[0].id = 0;
    actors[0].model = &records[0].instance;
    actors[0].next = &actors[1];
    actors[1].id = 2;
    actors[1].model = &records[1].instance;
    Battle_actors[0] = &actors[0];
    Battle_actors[2] = &actors[1];
    owner.actorId = 0;
    View().model =
        reinterpret_cast<Battle_Ai_PerceivedObjectView*>(&records[0]);
    View().position = records[0].instance.position;
    View().pointHeight = -32;
    View().nearDistanceSquared = View().farDistanceSquared = 768 * 768;
    View().viewCosine = -4096;
    TargetView().pointHeight = -96;
    TargetView().bodyHeight = 128;
    TargetView().radius = 16;
    if (auxiliary_target) {
      SetAuxiliary(2, records[1].instance.position);
      actors[1].model = reinterpret_cast<Actors_ModelInstance*>(&auxiliary);
      actors[1].kind = 0x10;
      actors[1].modelId = 0xFE;
      Actors_actorRecords[2] = nullptr;
    }
  }

  ~AuxiliaryPerceptionFixture() {
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_records.begin(), saved_records.end(), Actors_actorRecords);
    std::copy(saved_gimmicks.begin(), saved_gimmicks.end(),
              Field_gimmickInstances);
    std::copy(saved_states.begin(), saved_states.end(),
              Battle_Ai_actorAiStates);
    *Battle_Ai_FrameState() = saved_frame;
    Core_Gte_Reset();
  }

  Battle_Ai_PerceptionView& View() {
    return *reinterpret_cast<Battle_Ai_PerceptionView*>(&owner);
  }
  Battle_Ai_PerceptionView& TargetView() {
    return *reinterpret_cast<Battle_Ai_PerceptionView*>(&target);
  }
  void SetAuxiliary(int id, SVECTOR position) {
    auxiliary.slot = static_cast<u_char>(id);
    auxiliary.positionX = position.vx;
    auxiliary.positionY = position.vy;
    auxiliary.positionZ = position.vz;
    Field_gimmickInstances[id] = &auxiliary;
  }
  void AddAuxiliaryBlocker(bool blocking) {
    SetAuxiliary(3, {static_cast<std::int16_t>(blocking ? 16 : 112), 0, 64, 0});
    actors[2].id = 3;
    actors[2].kind = 0x10;
    actors[2].model = reinterpret_cast<Actors_ModelInstance*>(&auxiliary);
    actors[1].next = &actors[2];
    Battle_actors[3] = &actors[2];
    View().distancesSquared[3] = blocking ? 48 * 48 : 96 * 96 + 48 * 48;
  }
  void Update() { Battle_Ai_UpdateTargetPerception(&View(), 2); }
};
}  // namespace

TEST_CASE(
    "Perception uses valid auxiliary target models without actor-table aliases",
    "[ai][perception][auxiliary-target]") {
  for (bool with_ai : {false, true}) {
    AuxiliaryPerceptionFixture fixture(true, with_ai);
    REQUIRE(Actors_actorRecords[2] == nullptr);
    REQUIRE(Battle_actors[2]->model != nullptr);
    fixture.Update();
    CHECK(fixture.View().distancesSquared[2] == (with_ai ? 13312 : 10240));
    CHECK(Field_gimmickInstances[2] == &fixture.auxiliary);
    CHECK((Battle_Ai_actorAiStates[2] != nullptr) == with_ai);
  }
}

TEST_CASE(
    "Perception retains ordinary actor model distance and height controls",
    "[ai][perception][auxiliary-target]") {
  AuxiliaryPerceptionFixture fixture(false);
  fixture.Update();
  CHECK(fixture.View().distancesSquared[2] == 13312);
  CHECK(Actors_actorRecords[2] == &fixture.records[1]);
}

TEST_CASE("Perception evaluates valid auxiliary occluders through their owners",
          "[ai][perception][auxiliary-occluder]") {
  for (bool blocking : {false, true}) {
    AuxiliaryPerceptionFixture fixture(false);
    fixture.AddAuxiliaryBlocker(blocking);
    REQUIRE(Actors_actorRecords[3] == nullptr);
    fixture.Update();
    CHECK(fixture.View().relations[2].bits.blocked == blocking);
    CHECK(Battle_actors[3] == &fixture.actors[2]);
  }
}
