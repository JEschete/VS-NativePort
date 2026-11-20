#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorPerceptionLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Damage.h"
#include "Field/Gimmicks.hpp"
#include "Legacy/Psx.h"

namespace {
static_assert(offsetof(Actors_ModelInstance, position) ==
              offsetof(VS::Field::GimmickInstance, position_x));
static_assert(offsetof(VS::Battle::Ai::PerceivedObjectView, position) ==
              offsetof(Actors_ModelInstance, position));

struct AuxiliaryPerceptionFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  decltype(VS::Field::gimmickInstances) saved_gimmicks =
      VS::Field::gimmickInstances;
  decltype(VS::Battle::Ai::actorAiStates) saved_states =
      VS::Battle::Ai::actorAiStates;
  VS::Battle::Ai::FrameContext saved_frame = VS::Battle::Ai::LiveFrameContext();
  std::array<Actors_ActorRecord, 2> records{};
  std::array<Battle_Actor, 3> actors{};
  VS::Battle::Ai::ActorAiState owner{};
  VS::Battle::Ai::ActorAiState target{};
  VS::Field::GimmickInstance auxiliary{};
  u_short flags[32][32]{};

  explicit AuxiliaryPerceptionFixture(bool auxiliary_target,
                                      bool target_ai = true) {
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_records.size(), nullptr);
    std::fill_n(VS::Field::gimmickInstances.begin(), saved_gimmicks.size(),
                nullptr);
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved_states.size(),
                nullptr);
    auto& frame = VS::Battle::Ai::LiveFrameContext();
    frame = {};
    frame.tileFlags = flags;
    frame.actors[0] = VS::Battle::Ai::actorAiStates[0] = &owner;
    if (target_ai) frame.actors[2] = VS::Battle::Ai::actorAiStates[2] = &target;
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
    owner.actor_id = 0;
    View().model =
        reinterpret_cast<VS::Battle::Ai::PerceivedObjectView*>(&records[0]);
    View().position = records[0].instance.position;
    View().point_height = -32;
    View().near_distance_squared = View().far_distance_squared = 768 * 768;
    View().view_cosine = -4096;
    TargetView().point_height = -96;
    TargetView().body_height = 128;
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
              VS::Field::gimmickInstances.begin());
    std::copy(saved_states.begin(), saved_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
    VS::Battle::Ai::LiveFrameContext() = saved_frame;
    Core_Gte_Reset();
  }

  VS::Battle::Ai::PerceptionView& View() {
    return *reinterpret_cast<VS::Battle::Ai::PerceptionView*>(&owner);
  }
  VS::Battle::Ai::PerceptionView& TargetView() {
    return *reinterpret_cast<VS::Battle::Ai::PerceptionView*>(&target);
  }
  void SetAuxiliary(int id, SVECTOR position) {
    auxiliary.slot = static_cast<u_char>(id);
    auxiliary.position_x = position.vx;
    auxiliary.position_y = position.vy;
    auxiliary.position_z = position.vz;
    VS::Field::gimmickInstances[id] = &auxiliary;
  }
  void AddAuxiliaryBlocker(bool blocking) {
    SetAuxiliary(3, {static_cast<std::int16_t>(blocking ? 16 : 112), 0, 64, 0});
    actors[2].id = 3;
    actors[2].kind = 0x10;
    actors[2].model = reinterpret_cast<Actors_ModelInstance*>(&auxiliary);
    actors[1].next = &actors[2];
    Battle_actors[3] = &actors[2];
    View().distances_squared[3] = blocking ? 48 * 48 : 96 * 96 + 48 * 48;
  }
  void Update() { VS::Battle::Ai::UpdateLiveTargetPerception(&View(), 2); }
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
    CHECK(fixture.View().distances_squared[2] == (with_ai ? 13312 : 10240));
    CHECK(VS::Field::gimmickInstances[2] == &fixture.auxiliary);
    CHECK((VS::Battle::Ai::actorAiStates[2] != nullptr) == with_ai);
  }
}

TEST_CASE(
    "Perception retains ordinary actor model distance and height controls",
    "[ai][perception][auxiliary-target]") {
  AuxiliaryPerceptionFixture fixture(false);
  fixture.Update();
  CHECK(fixture.View().distances_squared[2] == 13312);
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
