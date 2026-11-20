#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorPerceptionLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/ReactionDelay.hpp"
#include "Battle/Damage.hpp"
#include "Battle/TargetingLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Random.hpp"
#include "Field/Gimmicks.hpp"
#include "Legacy/Psx.hpp"

using VS::Battle::ActionDefinition;

namespace {
struct DecisionFixture {
  decltype(VS::Battle::Ai::roomAiState) saved_room =
      VS::Battle::Ai::roomAiState;
  decltype(VS::Battle::Ai::fallbackCandidates) saved_fallback =
      VS::Battle::Ai::fallbackCandidates;
  int saved_count = VS::Battle::Ai::fallbackCandidateCount;
  int saved_mask = VS::Battle::Ai::fallbackSlotMask;
  ActionDefinition saved_action = VS::Battle::actionDefinitions[42];
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  std::array<VS::Actors::ActorRecord*, 17> saved_records =
      VS::Actors::actorRecords;
  std::array<VS::Field::GimmickInstance*, 16> saved_gimmicks =
      VS::Field::gimmickInstances;
  VS::Battle::Ai::FrameContext saved_frame = VS::Battle::Ai::LiveFrameContext();
  VS::Battle::Ai::RoomAiState room{};
  VS::Battle::Ai::ActorAiState state{};
  std::array<VS::Battle::Actor, 3> actors{};
  std::array<VS::Battle::ActorStats, 3> stats{};
  std::array<VS::Actors::ActorRecord, 3> records{};
  VS::Field::GimmickInstance gimmick{};
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  std::array<VS::Battle::Ai::CandidateEntryView, 16> fallback{};

  DecisionFixture() {
    std::fill_n(VS::Battle::actors.begin(), saved_actors.size(), nullptr);
    std::fill_n(VS::Actors::actorRecords.begin(), saved_records.size(),
                nullptr);
    std::fill_n(VS::Field::gimmickInstances.begin(), saved_gimmicks.size(),
                nullptr);
    for (std::size_t index = 0; index < actors.size(); ++index) {
      const int id = index == 0 ? 0 : static_cast<int>(index + 1);
      actors[index].id = id;
      actors[index].stats = &stats[index];
      actors[index].next =
          index + 1 < actors.size() ? &actors[index + 1] : nullptr;
      VS::Battle::actors[id] = &actors[index];
      VS::Actors::actorRecords[id] = &records[index];
    }
    VS::Battle::Ai::roomAiState = &room;
    VS::Battle::Ai::fallbackCandidates = fallback.data();
    VS::Battle::Ai::fallbackCandidateCount = VS::Battle::Ai::fallbackSlotMask =
        0;
    state.actor_id = 2;
    state.distances_squared[3] = 25;
    state.relations[3] =
        std::bit_cast<VS::Battle::Ai::RelationWord>(uint32_t{0x800000C4});
    room.readiness_level = 5;
    candidate.target_id = 3;
    candidate.action = 42;
    candidate.weight = 40;
    candidate.threshold = 5;
    candidate.distance_squared = 25;
  }

  ~DecisionFixture() {
    VS::Battle::Ai::roomAiState = saved_room;
    VS::Battle::Ai::fallbackCandidates = saved_fallback;
    VS::Battle::Ai::fallbackCandidateCount = saved_count;
    VS::Battle::Ai::fallbackSlotMask = saved_mask;
    VS::Battle::actionDefinitions[42] = saved_action;
    std::copy(saved_actors.begin(), saved_actors.end(),
              VS::Battle::actors.begin());
    std::copy(saved_records.begin(), saved_records.end(),
              VS::Actors::actorRecords.begin());
    std::copy(saved_gimmicks.begin(), saved_gimmicks.end(),
              VS::Field::gimmickInstances.begin());
    VS::Battle::Ai::LiveFrameContext() = saved_frame;
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
  fixture.state.temporary_ashley_alliance = static_cast<std::uint8_t>(busy);
  fixture.state.min_action_delay = static_cast<std::uint8_t>(timer);
  std::memcpy(fixture.state.reserved90.data(), &gate, sizeof(gate));
  fixture.candidate.options.opaque0_bit4 = target_flag;
  VS::Field::gimmickInstances[3] =
      has_gimmick != 0 ? &fixture.gimmick : nullptr;
  const bool required = busy != 0 || (target_flag != 0 && has_gimmick == 0);
  CHECK(VS::Battle::Ai::DoesLiveActionRequireCooldown(
            &fixture.state, &fixture.candidate) == required);
  CHECK(VS::Battle::Ai::IsLiveActionCooldownReady(&fixture.state,
                                                  &fixture.candidate) ==
        (gate == 0 && (!required || timer == 0)));
}

TEST_CASE("AI reaction-delay reset updates only existing non-player states",
          "[ai][decisions]") {
  const auto saved_states = VS::Battle::Ai::actorAiStates;
  std::array<VS::Battle::Ai::ActorAiState, 16> states{};
  std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved_states.size(),
              nullptr);
  for (std::size_t id = 0; id < states.size(); ++id) {
    states[id].min_action_delay = static_cast<std::uint8_t>(id + 1);
    VS::Battle::Ai::actorAiStates[id] = &states[id];
  }
  VS::Battle::Ai::actorAiStates[3] = nullptr;

  VS::Battle::Ai::ResetActorAiReactionDelay(
      VS::Battle::Ai::BattleActorReactionDelays(), 0);
  CHECK(states[0].min_action_delay == 1);
  for (int id = 1; id < 16; ++id) {
    VS::Battle::Ai::ResetActorAiReactionDelay(
        VS::Battle::Ai::BattleActorReactionDelays(), id);
    if (id == 3) {
      CHECK(states[id].min_action_delay == id + 1);
    } else {
      CHECK(states[id].min_action_delay == 30);
    }
  }

  std::copy(saved_states.begin(), saved_states.end(),
            VS::Battle::Ai::actorAiStates.begin());
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
  CHECK(VS::Battle::Ai::ScoreLiveActorCondition(
            example[0], example[1], example[2], example[3]) == example[4]);
}

TEST_CASE("AI target action flags preserve the special class-two inversion",
          "[ai][decisions]") {
  const uint16_t flags = GENERATE(0, 0x4000, 0x8000, 0xC000);
  const auto actor_class = GENERATE(0, 2, 7);
  const auto action_id = GENERATE(42, 96);
  DecisionFixture fixture;
  const auto previous = VS::Battle::actionDefinitions[action_id].ai_flags;
  VS::Battle::actionDefinitions[action_id].ai_flags =
      std::bit_cast<VS::Battle::ActionAiFlags>(flags);
  fixture.stats[2].enemy_class = actor_class;
  const bool special = action_id == 96 && actor_class == 2;
  CHECK(VS::Battle::Ai::IsLiveActionHarmfulToActor(action_id, 3) ==
        (special ? ((flags >> 15) ^ 1) : ((flags >> 15) & 1)));
  CHECK(VS::Battle::Ai::IsLiveActionBeneficialToActor(action_id, 3) ==
        (special ? ((flags >> 14) ^ 1) : ((flags >> 14) & 1)));
  VS::Battle::actionDefinitions[action_id].ai_flags = previous;
}

TEST_CASE(
    "AI fallback caching deduplicates slots and marks slots beyond capacity",
    "[ai][decisions]") {
  DecisionFixture fixture;
  ActionDefinition action{};
  action.ai_flags.reserved0_bit6 = 1;
  for (unsigned index = 0; index < 24; ++index) {
    fixture.candidate.row = index / 4;
    fixture.candidate.column = index % 4;
    fixture.candidate.opaque0 = static_cast<int>(index);
    VS::Battle::Ai::CacheLiveFallbackActionCandidate(&fixture.candidate,
                                                     &action);
    REQUIRE(VS::Battle::Ai::fallbackCandidateCount ==
            static_cast<int>(std::min(index + 1, 16u)));
    if (index < 16) {
      CHECK(std::memcmp(&fixture.fallback[index], &fixture.candidate,
                        sizeof(fixture.candidate)) == 0);
    }
  }
  CHECK(VS::Battle::Ai::fallbackSlotMask == 0xFFFFFF);
  fixture.candidate.row = fixture.candidate.column = 0;
  fixture.candidate.opaque0 = 999;
  VS::Battle::Ai::CacheLiveFallbackActionCandidate(&fixture.candidate, &action);
  CHECK(fixture.fallback[0].score == 0);
}

TEST_CASE("AI fallback caching ignores actions without the cache flag",
          "[ai][decisions]") {
  DecisionFixture fixture;
  ActionDefinition action{};
  VS::Battle::Ai::CacheLiveFallbackActionCandidate(&fixture.candidate, &action);
  CHECK(VS::Battle::Ai::fallbackCandidateCount == 0);
  CHECK(VS::Battle::Ai::fallbackSlotMask == 0);
}

TEST_CASE(
    "AI randomized promotion consumes skipped draws and caps success weight",
    "[ai][decisions]") {
  const auto weight = GENERATE(1, 2, 255);
  VS::Battle::Ai::ActionPlanView plan{};
  plan.candidate_count = 4;
  for (unsigned index = 0; index < 4; ++index) {
    plan.candidates[index].score = 100 - static_cast<int>(index) * 10;
    plan.candidates[index].action = index == 0 ? 0 : 42;
    plan.candidates[index].weight = weight;
    plan.candidate_order[index] = &plan.candidates[index];
  }
  VS::Core::SeedBiosRand(1);
  VS::Battle::Ai::PromoteLiveRandomizedActionCandidate(&plan, 4);
  const bool promoted = weight > 1;
  CHECK(plan.candidate_order[0] == &plan.candidates[promoted ? 2 : 0]);
  CHECK(plan.candidate_order[1] == &plan.candidates[promoted ? 0 : 1]);
  VS::Core::Rand15 expected(1);
  for (int index = 0; index < (promoted ? 3 : 4); ++index) expected.Next();
  CHECK(VS::Core::NextBiosRand() == expected.Next());
  VS::Core::SeedBiosRand(1);
}

TEST_CASE("AI actor RNG sign-extends the old halfword before adding its step",
          "[ai][decisions]") {
  DecisionFixture fixture;
  auto* frame = &VS::Battle::Ai::LiveFrameContext();
  frame->randomState = 0x7FFF;
  frame->lastBudgetTick = 2;
  CHECK(VS::Battle::Ai::NextLiveActorRandomMasked(128) == 1);
  CHECK(frame->randomState == 0x8001);
  CHECK(VS::Battle::Ai::NextLiveActorRandomMasked(128) == 3);
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
      fixture.room.readiness_level = 4;
      break;
    case 3:
      fixture.state.action_delay = 1;
      break;
    case 4:
      fixture.candidate.distance_squared = 24;
      break;
    case 5:
      fixture.state.relations[3] =
          std::bit_cast<VS::Battle::Ai::RelationWord>(uint32_t{0xC4});
      break;
    case 6:
    case 7:
      fixture.candidate.options.opaque0_bit1 = 1;
      fixture.candidate.options.opaque0_bit6 = condition == 6 ? 6 : 5;
      break;
    default:
      break;
  }
  CHECK(VS::Battle::Ai::CanExecuteLiveActionCandidate(&fixture.state,
                                                      &fixture.candidate) ==
        (condition == 0 || condition == 7));
}

TEST_CASE("AI execution applies visibility and obstruction option alternatives",
          "[ai][decisions]") {
  const auto visible = GENERATE(0, 1);
  const auto override_visibility = GENERATE(0, 1);
  const auto blocked = GENERATE(0, 1);
  const auto override_obstruction = GENERATE(0, 1);
  DecisionFixture fixture;
  fixture.state.relations[3] = std::bit_cast<VS::Battle::Ai::RelationWord>(
      uint32_t{0x800000C0} | (static_cast<uint32_t>(visible) << 2));
  fixture.state.relations[3].blocked = blocked;
  fixture.candidate.options.opaque0_bit2 = override_visibility;
  fixture.candidate.options.opaque0_bit3 = override_obstruction;
  CHECK(VS::Battle::Ai::CanExecuteLiveActionCandidate(&fixture.state,
                                                      &fixture.candidate) ==
        ((visible != 0 || override_visibility != 0) &&
         (blocked == 0 || override_obstruction != 0)));
}

TEST_CASE("AI execution observes Ashley's exact movement and locomotion bits",
          "[ai][decisions]") {
  const auto movement = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const auto locomotion = GENERATE(0, 1, 2, 3);
  DecisionFixture fixture;
  fixture.records[0].instance.movement_mode = movement;
  fixture.records[0].instance.locomotion_state = locomotion;
  CHECK(VS::Battle::Ai::CanExecuteLiveActionCandidate(&fixture.state,
                                                      &fixture.candidate) ==
        (movement == 0 && locomotion != 1));
}

TEST_CASE(
    "AI target-gimmick proximity checks native actor nodes and ignores height",
    "[ai][decisions]") {
  const auto x_offset = GENERATE(0, 1, 2);
  const auto z_offset = GENERATE(0, 1, 2);
  DecisionFixture fixture;
  VS::Field::gimmickInstances[3] = &fixture.gimmick;
  fixture.gimmick.tile_x = fixture.gimmick.tile_z = 10;
  fixture.gimmick.tile_layer = 3;
  fixture.records[0].instance.current_tile_x = 10 + x_offset;
  fixture.records[0].instance.current_tile_z = 10 + z_offset;
  fixture.records[0].instance.current_tile_layer = 200;
  fixture.state.relations[0].classification = 3;
  const bool outside = x_offset * x_offset + z_offset * z_offset >= 3;
  CHECK(VS::Battle::Ai::HasLiveNearbyEnemy(&fixture.state, 3) == !outside);
  CHECK(VS::Battle::Ai::CanExecuteLiveActionCandidate(
            &fixture.state, &fixture.candidate) == !outside);
}

TEST_CASE(
    "AI target-gimmick proximity reads unsigned tile bytes and classifications",
    "[ai][decisions]") {
  const auto relation = GENERATE(0, 1, 2, 3);
  DecisionFixture fixture;
  VS::Field::gimmickInstances[3] = &fixture.gimmick;
  fixture.gimmick.tile_x = fixture.gimmick.tile_z =
      std::bit_cast<char>(uint8_t{255});
  fixture.records[0].instance.current_tile_x =
      fixture.records[0].instance.current_tile_z = 255;
  fixture.state.relations[0].classification = relation;
  CHECK(VS::Battle::Ai::HasLiveNearbyEnemy(&fixture.state, 3) ==
        (relation >= 3));
}

TEST_CASE("AI randomized promotion handles empty and full zero-action lists",
          "[ai][decisions]") {
  const auto count = GENERATE(0, 1, 16);
  VS::Battle::Ai::ActionPlanView plan{};
  plan.candidate_count = count;
  for (int index = 0; index < count; ++index) {
    plan.candidate_order[index] = &plan.candidates[index];
  }
  const auto order = plan.candidate_order;
  VS::Core::SeedBiosRand(1234);
  VS::Battle::Ai::PromoteLiveRandomizedActionCandidate(&plan, count);
  CHECK(plan.candidate_order == order);
  VS::Core::Rand15 expected(1234);
  for (int index = 0; index < count; ++index) expected.Next();
  CHECK(VS::Core::NextBiosRand() == expected.Next());
  VS::Core::SeedBiosRand(1);
}

TEST_CASE(
    "AI weapon range reads the canonical actor stats instead of its model",
    "[ai][decisions]") {
  REQUIRE(offsetof(VS::Battle::Ai::ActionRangeView, stats) ==
          offsetof(VS::Battle::Ai::ActorAiState, stats));
  DecisionFixture fixture;
  fixture.state.stats = &fixture.stats[1];
  fixture.stats[1].current_range_y = 4;
  fixture.stats[1].current_range_z = 6;
  fixture.stats[1].current_attack_shape = 1;
  fixture.stats[1].current_attack_shape_angle = 0;
  VS::Battle::actionDefinitions[42].range_x = 0xFF;
  CHECK(VS::Battle::LiveCalculateActionRangeSquared(
            reinterpret_cast<VS::Battle::Ai::ActionRangeView*>(&fixture.state),
            42) == 25600);
}

namespace {
struct PerceptionFixture : DecisionFixture {
  decltype(VS::Battle::Ai::actorAiStates) saved_states =
      VS::Battle::Ai::actorAiStates;
  VS::Battle::Ai::ActorAiState player{};
  std::uint16_t flags[32][32]{};

  PerceptionFixture() {
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved_states.size(),
                nullptr);
    auto& frame = VS::Battle::Ai::LiveFrameContext();
    std::fill_n(frame.actors, VS::Battle::Ai::kFrameActorSlots, nullptr);
    frame.actors[0] = VS::Battle::Ai::actorAiStates[0] = &player;
    frame.actors[2] = VS::Battle::Ai::actorAiStates[2] = &state;
    frame.tileFlags = flags;
    actors[0].model = &records[0].instance;
    actors[1].model = &records[1].instance;
    actors[1].next = nullptr;
    state.model = reinterpret_cast<VS::Battle::Ai::ModelStatePrefix*>(
        &records[1].instance);
    state.position = records[1].instance.position = {64, 0, 96, 0};
    records[0].instance.position = {64, 0, 32, 0};
    View().near_distance_squared = View().far_distance_squared = 768 * 768;
    View().view_cosine = 3850;
    state.relations[0].classification = 3;
    candidate.target_id = 0;
    candidate.distance_squared = 192 * 192;
  }

  ~PerceptionFixture() {
    std::copy(saved_states.begin(), saved_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
  }

  VS::Battle::Ai::PerceptionView& View() {
    return *reinterpret_cast<VS::Battle::Ai::PerceptionView*>(&state);
  }

  VS::Battle::Ai::TargetRelationWord Relation() const {
    return std::bit_cast<VS::Battle::Ai::TargetRelationWord>(
        state.relations[0]);
  }

  void Update() { VS::Battle::Ai::UpdateLiveTargetPerception(&View(), 0); }

  int CanExecute() {
    return VS::Battle::Ai::CanExecuteLiveActionCandidate(&state, &candidate);
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
  CHECK(fixture.state.distances_squared[0] == 64 * 64);
  CHECK(fixture.Relation().bits.in_view == in_view);
  CHECK(fixture.Relation().bits.active == in_view);
  CHECK(fixture.Relation().bits.visible == 1);
  CHECK(fixture.Relation().bits.on_marked_tile == in_view);
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
  fixture.state.relations[0] =
      std::bit_cast<VS::Battle::Ai::RelationWord>(relation);
  fixture.Update();
  CHECK((std::array<int, 5>{
             fixture.Relation().bits.in_view, fixture.Relation().bits.timer,
             fixture.Relation().bits.active,
             fixture.Relation().bits.on_marked_tile,
             fixture.CanExecute()} == std::array<int, 5>{0, 0, 1, 1, 1}));
  fixture.Update();
  CHECK((std::array<int, 4>{
             fixture.Relation().bits.active, fixture.Relation().bits.visible,
             fixture.Relation().bits.on_marked_tile, fixture.CanExecute()} ==
         std::array<int, 4>{0, 1, marked, marked}));
}
