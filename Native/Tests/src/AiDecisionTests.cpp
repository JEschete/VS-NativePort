#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Random.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/ActorStateHelpers.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Battle/Targeting.h"
#include "Field/Gimmicks.h"
}

namespace {
struct DecisionFixture {
  decltype(Battle_Ai_roomAiState) saved_room = Battle_Ai_roomAiState;
  decltype(Battle_Ai_fallbackCandidates) saved_fallback =
      Battle_Ai_fallbackCandidates;
  int saved_count = Battle_Ai_fallbackCandidateCount;
  int saved_mask = Battle_Ai_unusedPlanCounter;
  Battle_ActionDefinition saved_action = Battle_actionDefinitions[42];
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  std::array<Actors_ActorRecord*, 17> saved_records =
      std::to_array(Actors_actorRecords);
  std::array<Field_GimmickInstance*, 16> saved_gimmicks =
      std::to_array(Field_gimmickInstances);
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  Battle_Ai_RoomAiState room{};
  Battle_Ai_ActorAiState state{};
  std::array<Battle_Actor, 3> actors{};
  std::array<Battle_ActorStats, 3> stats{};
  std::array<Actors_ActorRecord, 3> records{};
  Field_GimmickInstance gimmick{};
  Battle_Ai_ActionCandidate candidate{};
  std::array<Battle_Ai_CandidateEntryView, 16> fallback{};

  DecisionFixture() {
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_records.size(), nullptr);
    std::fill_n(Field_gimmickInstances, saved_gimmicks.size(), nullptr);
    for (std::size_t index = 0; index < actors.size(); ++index) {
      const int id = index == 0 ? 0 : static_cast<int>(index + 1);
      actors[index].id = id;
      actors[index].stats = &stats[index];
      actors[index].next =
          index + 1 < actors.size() ? &actors[index + 1] : nullptr;
      Battle_actors[id] = &actors[index];
      Actors_actorRecords[id] = &records[index];
    }
    Battle_Ai_roomAiState = &room;
    Battle_Ai_fallbackCandidates = fallback.data();
    Battle_Ai_fallbackCandidateCount = Battle_Ai_unusedPlanCounter = 0;
    state.actorId = 2;
    state.distancesSquared[3] = 25;
    state.relations[3] =
        std::bit_cast<Battle_Ai_RelationWord>(uint32_t{0x800000C4});
    room.readinessLevel = 5;
    candidate.targetId = 3;
    candidate.action = 42;
    candidate.weight = 40;
    candidate.threshold = 5;
    candidate.distanceSquared = 25;
  }

  ~DecisionFixture() {
    Battle_Ai_roomAiState = saved_room;
    Battle_Ai_fallbackCandidates = saved_fallback;
    Battle_Ai_fallbackCandidateCount = saved_count;
    Battle_Ai_unusedPlanCounter = saved_mask;
    Battle_actionDefinitions[42] = saved_action;
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_records.begin(), saved_records.end(), Actors_actorRecords);
    std::copy(saved_gimmicks.begin(), saved_gimmicks.end(),
              Field_gimmickInstances);
    *Battle_Ai_FrameState() = saved_frame;
  }
};
}  // namespace

TEST_CASE(
    "AI cooldown gates combine busy state, target presence and both timers",
    "[ai][decisions]") {
  const auto busy = GENERATE(0, 1);
  const auto target_flag = GENERATE(0, 1);
  const auto has_gimmick = GENERATE(0, 1);
  const auto timer = GENERATE(0, 1, 30);
  const uint16_t gate = GENERATE(0, 1, 65535);
  DecisionFixture fixture;
  fixture.state.usesRoomActionDelay = static_cast<u_char>(busy);
  fixture.state.minActionDelay = static_cast<u_char>(timer);
  std::memcpy(fixture.state.reserved90, &gate, sizeof(gate));
  fixture.candidate.opaque10.opaque0Bit4 = target_flag;
  Field_gimmickInstances[3] = has_gimmick != 0 ? &fixture.gimmick : nullptr;
  const bool required = busy != 0 || (target_flag != 0 && has_gimmick == 0);
  CHECK(Battle_Ai_DoesActionRequireCooldown(&fixture.state,
                                            &fixture.candidate) == required);
  CHECK(Battle_Ai_IsActionCooldownReady(&fixture.state, &fixture.candidate) ==
        (gate == 0 && (!required || timer == 0)));
}

TEST_CASE("AI reaction-delay reset updates only existing non-player states",
          "[ai][decisions]") {
  const auto saved_states = std::to_array(Battle_Ai_actorAiStates);
  std::array<Battle_Ai_ActorAiState, 16> states{};
  std::fill_n(Battle_Ai_actorAiStates, saved_states.size(), nullptr);
  for (std::size_t id = 0; id < states.size(); ++id) {
    states[id].minActionDelay = static_cast<u_char>(id + 1);
    Battle_Ai_actorAiStates[id] = &states[id];
  }
  Battle_Ai_actorAiStates[3] = nullptr;

  Battle_Ai_ResetActorAiReactionDelay(0);
  CHECK(states[0].minActionDelay == 1);
  for (int id = 1; id < 16; ++id) {
    Battle_Ai_ResetActorAiReactionDelay(id);
    if (id == 3) {
      CHECK(states[id].minActionDelay == id + 1);
    } else {
      CHECK(states[id].minActionDelay == 30);
    }
  }

  std::copy(saved_states.begin(), saved_states.end(), Battle_Ai_actorAiStates);
}

TEST_CASE("AI condition scoring uses the source's signed-halfword weights",
          "[ai][decisions]") {
  const std::array<std::array<int, 5>, 8> examples{{
      {1, 2, 3, 4, 25},
      {-1, -2, -3, -4, -25},
      {0, 65535, 0, 0, -4},
      {0, 32768, 0, 0, -131072},
      {0, 0, 65535, 0, -1},
      {0, 0, 0, 65535, -2},
      {123, 65537, 65538, 65539, 750},
      {32767, 32767, -32768, -32768, 229366},
  }};
  const auto index = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const auto& example = examples[index];
  CHECK(Battle_Ai_ScoreActorCondition(example[0], example[1], example[2],
                                      example[3]) == example[4]);
}

TEST_CASE("AI target action flags preserve the special class-two inversion",
          "[ai][decisions]") {
  const uint16_t flags = GENERATE(0, 0x4000, 0x8000, 0xC000);
  const auto actor_class = GENERATE(0, 2, 7);
  const auto action_id = GENERATE(42, 96);
  DecisionFixture fixture;
  const auto previous = Battle_actionDefinitions[action_id].aiFlags;
  Battle_actionDefinitions[action_id].aiFlags =
      std::bit_cast<Battle_ActionAiFlags>(flags);
  fixture.stats[2].enemyClass = actor_class;
  const bool special = action_id == 96 && actor_class == 2;
  CHECK(Battle_Ai_IsActionHarmfulToActor(action_id, 3) ==
        (special ? ((flags >> 15) ^ 1) : ((flags >> 15) & 1)));
  CHECK(Battle_Ai_IsActionBeneficialToActor(action_id, 3) ==
        (special ? ((flags >> 14) ^ 1) : ((flags >> 14) & 1)));
  Battle_actionDefinitions[action_id].aiFlags = previous;
}

TEST_CASE(
    "AI fallback caching deduplicates slots and marks slots beyond capacity",
    "[ai][decisions]") {
  DecisionFixture fixture;
  Battle_ActionDefinition action{};
  action.aiFlags.reserved0Bit6 = 1;
  for (unsigned index = 0; index < 24; ++index) {
    fixture.candidate.row = index / 4;
    fixture.candidate.column = index % 4;
    fixture.candidate.opaque0 = static_cast<int>(index);
    Battle_Ai_CacheFallbackActionCandidate(&fixture.candidate, &action);
    REQUIRE(Battle_Ai_fallbackCandidateCount ==
            static_cast<int>(std::min(index + 1, 16u)));
    if (index < 16) {
      CHECK(std::memcmp(&fixture.fallback[index], &fixture.candidate,
                        sizeof(fixture.candidate)) == 0);
    }
  }
  CHECK(Battle_Ai_unusedPlanCounter == 0xFFFFFF);
  fixture.candidate.row = fixture.candidate.column = 0;
  fixture.candidate.opaque0 = 999;
  Battle_Ai_CacheFallbackActionCandidate(&fixture.candidate, &action);
  CHECK(fixture.fallback[0].reserved0 == 0);
}

TEST_CASE("AI fallback caching ignores actions without the cache flag",
          "[ai][decisions]") {
  DecisionFixture fixture;
  Battle_ActionDefinition action{};
  Battle_Ai_CacheFallbackActionCandidate(&fixture.candidate, &action);
  CHECK(Battle_Ai_fallbackCandidateCount == 0);
  CHECK(Battle_Ai_unusedPlanCounter == 0);
}

TEST_CASE(
    "AI randomized promotion consumes skipped draws and caps success weight",
    "[ai][decisions]") {
  const auto weight = GENERATE(1, 2, 255);
  Battle_Ai_ActionPlanView plan{};
  plan.candidateCount = 4;
  for (unsigned index = 0; index < 4; ++index) {
    plan.candidates[index].reserved0 = 100 - static_cast<int>(index) * 10;
    plan.candidates[index].action = index == 0 ? 0 : 42;
    plan.candidates[index].weight = weight;
    plan.candidateOrder[index] = &plan.candidates[index];
  }
  Core_Random_Seed(1);
  Battle_Ai_PromoteRandomizedActionCandidate(&plan, 4);
  const bool promoted = weight > 1;
  CHECK(plan.candidateOrder[0] == &plan.candidates[promoted ? 2 : 0]);
  CHECK(plan.candidateOrder[1] == &plan.candidates[promoted ? 0 : 1]);
  VS::Core::Rand15 expected(1);
  for (int index = 0; index < (promoted ? 3 : 4); ++index) expected.Next();
  CHECK(Core_Random_Rand() == expected.Next());
  Core_Random_Seed(1);
}

TEST_CASE("AI actor RNG sign-extends the old halfword before adding its step",
          "[ai][decisions]") {
  DecisionFixture fixture;
  auto* frame = Battle_Ai_FrameState();
  frame->randomState = 0x7FFF;
  frame->lastBudgetTick = 2;
  CHECK(Battle_Ai_NextActorRandomMasked(128) == 1);
  CHECK(frame->randomState == 0x8001);
  CHECK(Battle_Ai_NextActorRandomMasked(128) == 3);
  CHECK(frame->randomState == 0xFFFF8003);
}

TEST_CASE(
    "AI execution gates reject only the failing readiness or range condition",
    "[ai][decisions]") {
  const auto condition = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  DecisionFixture fixture;
  switch (condition) {
    case 1:
      fixture.candidate.weight = 0;
      break;
    case 2:
      fixture.room.readinessLevel = 4;
      break;
    case 3:
      fixture.state.actionDelay = 1;
      break;
    case 4:
      fixture.candidate.distanceSquared = 24;
      break;
    case 5:
      fixture.state.relations[3] =
          std::bit_cast<Battle_Ai_RelationWord>(uint32_t{0xC4});
      break;
    case 6:
    case 7:
      fixture.candidate.opaque10.opaque0Bit1 = 1;
      fixture.candidate.opaque10.opaque0Bit6 = condition == 6 ? 6 : 5;
      break;
    default:
      break;
  }
  CHECK(
      Battle_Ai_CanExecuteActionCandidate(&fixture.state, &fixture.candidate) ==
      (condition == 0 || condition == 7));
}

TEST_CASE("AI execution applies visibility and obstruction option alternatives",
          "[ai][decisions]") {
  const auto visible = GENERATE(0, 1);
  const auto override_visibility = GENERATE(0, 1);
  const auto blocked = GENERATE(0, 1);
  const auto override_obstruction = GENERATE(0, 1);
  DecisionFixture fixture;
  fixture.state.relations[3] = std::bit_cast<Battle_Ai_RelationWord>(
      uint32_t{0x800000C0} | (static_cast<uint32_t>(visible) << 2));
  fixture.state.relations[3].blocked = blocked;
  fixture.candidate.opaque10.opaque0Bit2 = override_visibility;
  fixture.candidate.opaque10.opaque0Bit3 = override_obstruction;
  CHECK(
      Battle_Ai_CanExecuteActionCandidate(&fixture.state, &fixture.candidate) ==
      ((visible != 0 || override_visibility != 0) &&
       (blocked == 0 || override_obstruction != 0)));
}

TEST_CASE("AI execution observes Ashley's exact movement and locomotion bits",
          "[ai][decisions]") {
  const auto movement = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const auto locomotion = GENERATE(0, 1, 2, 3);
  DecisionFixture fixture;
  fixture.records[0].instance.movementMode = movement;
  fixture.records[0].instance.locomotionState = locomotion;
  CHECK(
      Battle_Ai_CanExecuteActionCandidate(&fixture.state, &fixture.candidate) ==
      (movement == 0 && locomotion != 1));
}

TEST_CASE(
    "AI target-gimmick proximity checks native actor nodes and ignores height",
    "[ai][decisions]") {
  const auto x_offset = GENERATE(0, 1, 2);
  const auto z_offset = GENERATE(0, 1, 2);
  DecisionFixture fixture;
  Field_gimmickInstances[3] = &fixture.gimmick;
  fixture.gimmick.tileX = fixture.gimmick.tileZ = 10;
  fixture.gimmick.tileLayer = 3;
  fixture.records[0].instance.currentTileX = 10 + x_offset;
  fixture.records[0].instance.currentTileZ = 10 + z_offset;
  fixture.records[0].instance.currentTileLayer = 200;
  fixture.state.relations[0].classification = 3;
  const bool outside = x_offset * x_offset + z_offset * z_offset >= 3;
  CHECK(Battle_Ai_HasNearbyEnemy(&fixture.state, 3) == outside);
  CHECK(Battle_Ai_CanExecuteActionCandidate(&fixture.state,
                                            &fixture.candidate) == !outside);
}

TEST_CASE(
    "AI target-gimmick proximity reads unsigned tile bytes and classifications",
    "[ai][decisions]") {
  const auto relation = GENERATE(0, 1, 2, 3);
  DecisionFixture fixture;
  Field_gimmickInstances[3] = &fixture.gimmick;
  fixture.gimmick.tileX = fixture.gimmick.tileZ =
      std::bit_cast<char>(uint8_t{255});
  fixture.records[0].instance.currentTileX =
      fixture.records[0].instance.currentTileZ = 255;
  fixture.state.relations[0].classification = relation;
  CHECK(Battle_Ai_HasNearbyEnemy(&fixture.state, 3) == (relation < 3));
}

TEST_CASE("AI randomized promotion handles empty and full zero-action lists",
          "[ai][decisions]") {
  const auto count = GENERATE(0, 1, 16);
  Battle_Ai_ActionPlanView plan{};
  plan.candidateCount = count;
  for (int index = 0; index < count; ++index) {
    plan.candidateOrder[index] = &plan.candidates[index];
  }
  const auto order = std::to_array(plan.candidateOrder);
  Core_Random_Seed(1234);
  Battle_Ai_PromoteRandomizedActionCandidate(&plan, count);
  CHECK(std::to_array(plan.candidateOrder) == order);
  VS::Core::Rand15 expected(1234);
  for (int index = 0; index < count; ++index) expected.Next();
  CHECK(Core_Random_Rand() == expected.Next());
  Core_Random_Seed(1);
}

TEST_CASE(
    "AI weapon range reads the canonical actor stats instead of its model",
    "[ai][decisions]") {
  REQUIRE(offsetof(Battle_Ai_ActionRangeView, opaque5C) ==
          offsetof(Battle_Ai_ActorAiState, stats));
  DecisionFixture fixture;
  fixture.state.stats = &fixture.stats[1];
  fixture.stats[1].currentRangeY = 4;
  fixture.stats[1].currentRangeZ = 6;
  fixture.stats[1].currentAttackShape = 1;
  fixture.stats[1].currentAttackShapeAngle = 0;
  Battle_actionDefinitions[42].rangeX = 0xFF;
  CHECK(Battle_CalculateActionRangeSquared(
            reinterpret_cast<Battle_Ai_ActionRangeView*>(&fixture.state), 42) ==
        25600);
}

namespace {
struct PerceptionFixture : DecisionFixture {
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  Battle_Ai_ActorAiState player{};
  u_short flags[32][32]{};

  PerceptionFixture() {
    std::fill_n(Battle_Ai_actorAiStates, saved_states.size(), nullptr);
    auto& frame = *Battle_Ai_FrameState();
    std::fill_n(frame.actors, kAiFrameActorSlots, nullptr);
    frame.actors[0] = Battle_Ai_actorAiStates[0] = &player;
    frame.actors[2] = Battle_Ai_actorAiStates[2] = &state;
    frame.tileFlags = flags;
    actors[0].model = &records[0].instance;
    actors[1].model = &records[1].instance;
    actors[1].next = nullptr;
    state.model = reinterpret_cast<Battle_Ai_AiModelView*>(&records[1]);
    state.position = records[1].instance.position = {64, 0, 96, 0};
    records[0].instance.position = {64, 0, 32, 0};
    View().nearDistanceSquared = View().farDistanceSquared = 768 * 768;
    View().viewCosine = 3850;
    state.relations[0].classification = 3;
    candidate.targetId = 0;
    candidate.distanceSquared = 192 * 192;
  }

  ~PerceptionFixture() {
    std::copy(saved_states.begin(), saved_states.end(),
              Battle_Ai_actorAiStates);
  }

  Battle_Ai_PerceptionView& View() {
    return *reinterpret_cast<Battle_Ai_PerceptionView*>(&state);
  }

  Battle_Ai_TargetRelation Relation() const {
    return std::bit_cast<Battle_Ai_TargetRelation>(state.relations[0]);
  }

  void Update() { Battle_Ai_UpdateTargetPerception(&View(), 0); }

  int CanExecute() {
    return Battle_Ai_CanExecuteActionCandidate(&state, &candidate);
  }
};
}  // namespace

TEST_CASE("AI perception connects the original narrow FOV to execution",
          "[ai][perception][reference]") {
  const int heading = GENERATE(0, 227, 228, 2048);
  PerceptionFixture fixture;
  fixture.records[1].instance.facing = static_cast<int16_t>(heading);
  fixture.Update();
  const bool in_view = heading <= 227;
  CHECK(fixture.state.distancesSquared[0] == 64 * 64);
  CHECK(fixture.Relation().bits.inView == in_view);
  CHECK(fixture.Relation().bits.active == in_view);
  CHECK(fixture.Relation().bits.visible == 1);
  CHECK(fixture.Relation().bits.onMarkedTile == in_view);
  CHECK(fixture.CanExecute() == in_view);
}

TEST_CASE("AI marked tiles and awareness timers retain original readiness",
          "[ai][perception][reference]") {
  const bool marked = GENERATE(false, true);
  PerceptionFixture fixture;
  fixture.records[1].instance.facing = 2048;
  fixture.flags[0][0] = marked ? 0x2000 : 0;
  auto relation = fixture.Relation();
  relation.bits.timer = 1;
  fixture.state.relations[0] = std::bit_cast<Battle_Ai_RelationWord>(relation);
  fixture.Update();
  CHECK(
      (std::array<int, 5>{
           fixture.Relation().bits.inView, fixture.Relation().bits.timer,
           fixture.Relation().bits.active, fixture.Relation().bits.onMarkedTile,
           fixture.CanExecute()} == std::array<int, 5>{0, 0, 1, 1, 1}));
  fixture.Update();
  CHECK((std::array<int, 4>{
             fixture.Relation().bits.active, fixture.Relation().bits.visible,
             fixture.Relation().bits.onMarkedTile, fixture.CanExecute()} ==
         std::array<int, 4>{0, 1, marked, marked}));
}
