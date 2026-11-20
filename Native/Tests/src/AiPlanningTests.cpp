#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/BlockedMovementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/StatusEffects.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Legacy/Psx.hpp"
#include "State/ScriptFlagsLive.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::ActionRequestRecord;

namespace {
struct PlanningFixture {
  decltype(VS::Battle::Ai::candidateExclusions) saved_exclusions =
      VS::Battle::Ai::candidateExclusions;
  std::array<VS::Battle::Ai::ActorAiState*, 16> saved_states =
      VS::Battle::Ai::actorAiStates;
  ActionDefinition saved_action = VS::Battle::actionDefinitions[42];
  VS::Battle::Ai::ActorAiState state{};
  VS::Battle::Ai::ActorAiState peer{};
  VS::Battle::Ai::EngagementData actor{};
  VS::Battle::ActorStats stats{};
  std::uint8_t exclusions[16][256]{};

  PlanningFixture() {
    VS::Battle::Ai::candidateExclusions = exclusions;
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), 16, nullptr);
    VS::Battle::Ai::actorAiStates[1] = &state;
    VS::Battle::Ai::actorAiStates[3] = &peer;
    VS::Battle::actionDefinitions[42] = {};
    actor.stats = &stats;
    state.actor = &actor;
    state.stats = &stats;
    state.actor_id = 1;
    stats.current_hp = stats.max_hp = 100;
    stats.current_mp = stats.max_mp = 100;
    stats.weapon.current_pp = 100;
    stats.limbs[2].hp = 2;
    stats.limb_actions[2][1].action_id = 42;
  }

  ~PlanningFixture() {
    VS::Battle::Ai::candidateExclusions = saved_exclusions;
    std::copy(saved_states.begin(), saved_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
    VS::Battle::actionDefinitions[42] = saved_action;
  }
};

struct PlanningAdvanceFixture : PlanningFixture {
  VS::Battle::Ai::RoomAiState* saved_room = VS::Battle::Ai::roomAiState;
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  std::array<std::uint32_t, 5> saved_masks{};
  VS::Battle::StatusEffectParams saved_poison =
      VS::Battle::statusEffectParams[14];
  VS::Battle::StatusEffectParams saved_regeneration =
      VS::Battle::statusEffectParams[17];
  VS::Battle::Ai::RoomAiState room{};
  VS::Battle::Actor battle_actor{};

  PlanningAdvanceFixture() {
    std::copy_n(VS::Battle::Ai::kTimedStatusFlags.begin(), saved_masks.size(),
                saved_masks.begin());
    std::fill_n(VS::Battle::Ai::kTimedStatusFlags.begin(), saved_masks.size(),
                0);
    VS::Battle::statusEffectParams[14].tick_interval = 1;
    VS::Battle::statusEffectParams[17].tick_interval = 1;
    room.allows_non_spell_actions = 0;
    room.engaged_mask_bytes[0] = 0;
    room.engaged_mask_bytes[1] = 0;
    room.engaged_count = 0;
    VS::Battle::Ai::roomAiState = &room;
    std::fill(std::begin(VS::Battle::actors), std::end(VS::Battle::actors),
              nullptr);
    battle_actor.id = 1;
    battle_actor.stats = &stats;
    VS::Battle::actors[0] = &battle_actor;
    for (auto& row : stats.limb_actions) {
      for (auto& slot : row) slot.action_id = 42;
    }
  }
  ~PlanningAdvanceFixture() {
    VS::Battle::Ai::roomAiState = saved_room;
    std::copy(saved_actors.begin(), saved_actors.end(),
              VS::Battle::actors.begin());
    std::copy(saved_masks.begin(), saved_masks.end(),
              VS::Battle::Ai::kTimedStatusFlags.begin());
    VS::Battle::statusEffectParams[14] = saved_poison;
    VS::Battle::statusEffectParams[17] = saved_regeneration;
  }
};

struct PlanningStatsFixture {
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  VS::Battle::ActorStats* saved_snapshot =
      VS::Battle::Ai::planningStatsSnapshot;
  std::array<std::byte, 65536> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  VS::Battle::Actor first{}, second{};
  VS::Battle::ActorStats first_stats{}, second_stats{};

  PlanningStatsFixture() {
    VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
    std::fill(std::begin(VS::Battle::actors), std::end(VS::Battle::actors),
              nullptr);
    first.id = 1;
    first.stats = &first_stats;
    first.next = &second;
    second.id = 15;
    second.stats = &second_stats;
    VS::Battle::actors[0] = &first;
    first_stats.current_hp = 100;
    second_stats.current_hp = 50;
  }
  ~PlanningStatsFixture() {
    VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
    std::copy(saved_actors.begin(), saved_actors.end(),
              VS::Battle::actors.begin());
    VS::Battle::Ai::planningStatsSnapshot = saved_snapshot;
  }
};

struct SimulationResultsFixture : PlanningFixture {
  std::array<std::byte, 32768> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  VS::Battle::Ai::ReactionRecord* saved_workspace =
      VS::Battle::Ai::planningWorkspace;
  std::array<VS::Battle::Ai::ActionTiming,
             VS::Battle::Ai::kBattleActorSlotCount>
      rows{};
  ActionRequestRecord* request = nullptr;
  VS::Battle::Ai::ReactionRecord* workspace = nullptr;
  SimulationResultsFixture() {
    request = static_cast<ActionRequestRecord*>(
        VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(ActionRequestRecord)));
    if (request == nullptr) throw std::bad_alloc();
    std::memset(request, 0, sizeof(*request));
    workspace = reinterpret_cast<VS::Battle::Ai::ReactionRecord*>(request);
    VS::Battle::Ai::planningWorkspace = workspace;
    workspace->action_id = 42;
    for (std::size_t index = 0; index < rows.size(); ++index) {
      rows[index].actor_id = static_cast<unsigned>(index);
      rows[index].reserved0_bit4 = 0xABCDE;
      rows[index].hp = 50;
      rows[index].max_hp = 100;
      rows[index].mp = 40;
      rows[index].max_mp = 100;
      rows[index].weapon_dp = 30;
      rows[index].weapon_max_dp = 100;
      rows[index].shield_dp = 20;
      rows[index].shield_max_dp = 100;
      rows[index].statuses = 0x80000000;
    }
  }
  ~SimulationResultsFixture() {
    VS::Battle::Ai::planningWorkspace = saved_workspace;
  }
};

struct ReadinessFixture : PlanningFixture {
  VS::Battle::Ai::RoomAiState* saved_room = VS::Battle::Ai::roomAiState;
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  VS::Battle::Ai::RoomAiState room{};
  VS::Battle::Actor hostile{};
  ReadinessFixture() {
    VS::Battle::Ai::roomAiState = &room;
    std::fill_n(VS::Battle::actors.begin(), std::size(VS::Battle::actors),
                nullptr);
    room.readiness_level = 100;
    state.candidate_count = 1;
    for (std::size_t index = 0; index < std::size(state.candidates); ++index) {
      state.candidates[index].row = 2;
      state.candidates[index].column = 1;
      state.candidates[index].action = 42;
      state.candidates[index].threshold = 131;
      state.candidates[index].target_id = 3;
      state.candidates[index].weight = 17;
      state.candidate_order[index] = &state.candidates[index];
    }
  }
  ~ReadinessFixture() {
    VS::Battle::Ai::roomAiState = saved_room;
    std::copy(saved_actors.begin(), saved_actors.end(),
              VS::Battle::actors.begin());
  }
};

struct EnumerationFixture : ReadinessFixture {
  decltype(VS::Battle::Ai::fallbackCandidates) saved_fallback =
      VS::Battle::Ai::fallbackCandidates;
  int saved_count = VS::Battle::Ai::fallbackCandidateCount;
  int saved_mask = VS::Battle::Ai::fallbackSlotMask;
  unsigned saved_random = VS::Core::BiosRand().State();
  std::array<VS::Battle::Ai::CandidateEntryView, 24> fallback{};
  VS::Battle::Actor battle_actor{};
  VS::Battle::ActorStats target_stats{};

  EnumerationFixture() {
    VS::Battle::Ai::fallbackCandidates = fallback.data();
    VS::Battle::Ai::fallbackCandidateCount = 0;
    VS::Battle::Ai::fallbackSlotMask = 0;
    VS::Core::SeedBiosRand(1);
    state.candidate_count = 0;
    state.scores_special_actions = 1;
    state.point_height = -4321;
    state.relations[1].classification = 2;
    target_stats = stats;
    target_stats.limb_actions[2][1].action_id = 0;
    target_stats.limbs[2].max_hp = -7;
    actor.stats = &target_stats;
    battle_actor.id = 1;
    battle_actor.stats = &target_stats;
    VS::Battle::actors[0] = VS::Battle::actors[1] = &battle_actor;
    VS::Battle::actionDefinitions[42].ai_flags =
        std::bit_cast<VS::Battle::ActionAiFlags>(std::uint16_t{0x150});
    auto& slot = state.action_slots[2][1];
    slot.usable = 1;
    slot.kind = 1;
    slot.radius = 0x1234;
    slot.range_squared = 0xFFFFFFFFu;
  }
  ~EnumerationFixture() {
    VS::Battle::Ai::fallbackCandidates = saved_fallback;
    VS::Battle::Ai::fallbackCandidateCount = saved_count;
    VS::Battle::Ai::fallbackSlotMask = saved_mask;
    VS::Core::SeedBiosRand(saved_random);
  }
};

struct ActorPlanFixture : EnumerationFixture {
  decltype(VS::Battle::Ai::planningPassCandidates) saved_passes =
      VS::Battle::Ai::planningPassCandidates;
  VS::Battle::Ai::ArenaOrbitState* saved_orbit = VS::Battle::Ai::arenaOrbit;
  VS::Battle::ActorStats* saved_snapshot =
      VS::Battle::Ai::planningStatsSnapshot;
  int saved_progress = VS::State::LiveScriptFlag(0xA6);
  std::array<std::byte, 65536> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  VS::Battle::Ai::CandidateEntryView pass_candidates[3][16]{};
  VS::Battle::Ai::ArenaOrbitState orbit{};

  ActorPlanFixture() {
    VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
    VS::Battle::Ai::planningPassCandidates = pass_candidates;
    VS::Battle::Ai::arenaOrbit = nullptr;
    state.action_slots[2][1].usable = 0;
    std::memset(exclusions, 1, sizeof(exclusions));
  }
  ~ActorPlanFixture() {
    VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
    VS::Battle::Ai::planningStatsSnapshot = saved_snapshot;
    VS::Battle::Ai::planningPassCandidates = saved_passes;
    VS::Battle::Ai::arenaOrbit = saved_orbit;
    VS::State::SetLiveScriptFlag(0xA6, saved_progress);
  }
};

struct TargetMemoryFixture {
  decltype(VS::Actors::actorRecords) saved_models = VS::Actors::actorRecords;
  VS::Battle::Ai::ActorAiState state{};
  VS::Battle::Ai::ActionCandidateRecord best{};
  VS::Battle::Ai::ActionCandidateRecord alternate{};
  VS::Actors::ActorRecord model{};
  VS::Actors::ActorRecord alternate_model{};

  explicit TargetMemoryFixture(unsigned target) {
    best.target_id = target;
    alternate.target_id = (target + 1) & 15;
    state.candidate_count = 2;
    state.candidate_order[0] = &best;
    state.candidate_order[1] = &alternate;
    state.previous_tile = 0x12345678;
    std::memset(state.reserved_3c.data(), 0xA5, sizeof(state.reserved_3c));
    VS::Actors::actorRecords[target] = &model;
    VS::Actors::actorRecords[alternate.target_id] = &alternate_model;
    const std::array<std::uint8_t, 4> tile{0xD4, 0xC3, 0xB2, 0xA1};
    std::memcpy(&model.instance.current_tile_x, tile.data(), tile.size());
  }

  ~TargetMemoryFixture() {
    std::copy(saved_models.begin(), saved_models.end(),
              VS::Actors::actorRecords.begin());
  }

  auto Bytes() const {
    std::array<std::byte, sizeof(state)> bytes{};
    std::memcpy(bytes.data(), &state, bytes.size());
    return bytes;
  }
};

bool CopiedStatByte(std::size_t offset) {
  return (offset >= 0x18 && offset < 0x20) ||
         (offset >= 0x30 && offset < 0x34) ||
         (offset >= 0x398 && offset < 0x8C0) ||
         (offset >= 0x948 && offset < 0x94C);
}

// A completed action 42 by performer 2, whose plan ranks the matching limb
// slot (row 3, column 1) against target 5.
struct CompletedPlanFixture {
  std::array<VS::Battle::Ai::ActorAiState*, 16> saved_states =
      VS::Battle::Ai::actorAiStates;
  decltype(VS::Field::gimmickInstances) saved_objects =
      VS::Field::gimmickInstances;
  decltype(VS::Actors::actorRecords) saved_models = VS::Actors::actorRecords;
  VS::Battle::Ai::ActorAiState state{};
  VS::Battle::Actor actor{};
  VS::Battle::Ai::ActionCandidateRecord first{};
  VS::Battle::Ai::ActionCandidateRecord second{};
  VS::Actors::ActorRecord model{};
  VS::Field::GimmickInstance object{};
  ActionRequestRecord request{};

  static constexpr int kRememberedTile = 5 << 24;
  static constexpr int kUntouchedTile = 0x12345678;

  CompletedPlanFixture() {
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), 16, nullptr);
    std::fill_n(VS::Field::gimmickInstances.begin(), 16, nullptr);
    VS::Battle::Ai::actorAiStates[2] = &state;
    state.actor = reinterpret_cast<VS::Battle::Ai::EngagementData*>(&actor);
    state.previous_tile = kUntouchedTile;
    actor.command_opcode = 24;
    actor.command_arg0 = (1 << 16) | 3;
    first.row = 3;
    first.column = 1;
    first.action = 42;
    first.target_id = 5;
    second = first;
    second.action = 43;
    second.target_id = 6;
    state.candidate_count = 2;
    state.candidate_order[0] = &first;
    state.candidate_order[1] = &second;
    VS::Actors::actorRecords[5] = &model;
    request.action_id = 42;
    request.primary_target.target.target_actor = 2;
    request.target_count = 1;
    request.targets[0].target.target_actor = 4;
    VS::Field::gimmickInstances[4] = &object;
  }

  ~CompletedPlanFixture() {
    std::copy(saved_states.begin(), saved_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
    std::copy(saved_objects.begin(), saved_objects.end(),
              VS::Field::gimmickInstances.begin());
    std::copy(saved_models.begin(), saved_models.end(),
              VS::Actors::actorRecords.begin());
  }

  void Reconcile() {
    VS::Battle::Ai::ReconcileCompletedActionPlanEntry(
        reinterpret_cast<VS::Battle::Ai::ReactionRecord*>(&request));
  }

  auto Bytes() const {
    std::array<std::byte, sizeof(state)> bytes{};
    std::memcpy(bytes.data(), &state, bytes.size());
    return bytes;
  }
};
}  // namespace

TEST_CASE(
    "AI remembers the first ranked target with the original tile and lifetime",
    "[ai][planning][target-memory]") {
  const unsigned target = GENERATE(0u, 1u, 15u);
  const unsigned tile = GENERATE(0xF4ABCDEFu, 0xFFFFFFFFu, 0x00430201u);
  TargetMemoryFixture fixture(target);
  fixture.state.tile.word = tile;
  auto expected = fixture.Bytes();
  const unsigned origin = (target << 24) | (tile & 0xFFFFFFu);
  const unsigned destination = 0xA1B2C3D4u;
  const std::uint16_t frames = 900;
  std::memcpy(
      expected.data() + offsetof(VS::Battle::Ai::ActorAiState, previous_tile),
      &origin, sizeof(origin));
  auto* memory =
      expected.data() + offsetof(VS::Battle::Ai::ActorAiState, reserved_3c);
  std::memcpy(memory, &destination, sizeof(destination));
  std::memcpy(memory + sizeof(destination), &frames, sizeof(frames));
  VS::Battle::Ai::RememberLiveAiActionTarget(&fixture.state);
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE(
    "Native planning stats restore only original fields and release storage",
    "[ai][planning][stats-lifecycle-live]") {
  PlanningStatsFixture fixture;
  const auto original =
      std::bit_cast<std::array<std::byte, sizeof(VS::Battle::ActorStats)>>(
          fixture.first_stats);
  const auto refused = VS::Core::Memory::RefusedLiveHeapRequestCount();
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  auto* storage = VS::Battle::Ai::planningStatsSnapshot;
  REQUIRE(storage != nullptr);
  CHECK(VS::Core::Memory::LivePayloadCapacity(storage).value() >=
        VS::Battle::Ai::kPlanningStatsTableBytes);
  std::memset(&fixture.first_stats, 0x75, sizeof(fixture.first_stats));
  fixture.second_stats.current_hp = 1;
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  const auto restored =
      std::bit_cast<std::array<std::byte, sizeof(VS::Battle::ActorStats)>>(
          fixture.first_stats);
  for (std::size_t offset = 0; offset < restored.size(); ++offset) {
    CHECK(restored[offset] ==
          (CopiedStatByte(offset) ? original[offset] : std::byte{0x75}));
  }
  CHECK(fixture.second_stats.current_hp == 50);
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(storage).has_value());
  CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == refused);
}

TEST_CASE(
    "Native fallback selection refreshes all slots and preserves packed fields",
    "[ai][planning][fallback-selection-live]") {
  const bool ignores_sight = GENERATE(false, true);
  PlanningFixture fixture;
  using Words = VS::Battle::Ai::ActionCandidate::Words;
  fixture.state.current_choice =
      std::bit_cast<VS::Battle::Ai::ActionCandidateRecord>(
          Words{0xDEADBEEF, 0xFFFFFFFF, 0xABCDEF01, 0xFFFFFFFF, 0xFFFFFFFF});
  VS::Battle::actionDefinitions[42].ai_flags.ignores_sight =
      ignores_sight ? 1u : 0u;
  for (int row = 0; row < 6; ++row) {
    fixture.stats.limbs[row].hp = 2;
    for (int column = 0; column < 4; ++column) {
      fixture.stats.limb_actions[row][column].action_id = 42;
      fixture.state.action_slots[row][column].special = 1;
      fixture.state.action_slots[row][column].range_squared =
          static_cast<unsigned>(row * 4 + column + 1);
    }
  }
  auto expected = fixture.state.current_choice;
  expected.row = 5;
  expected.column = 3;
  expected.action = 42;
  expected.distance_squared = 24;
  expected.weight = 128;
  expected.options.opaque0_bit0 = 0;
  expected.options.opaque0_bit1 = 0;
  expected.options.opaque0_bit2 = ignores_sight ? 1u : 0u;
  expected.options.opaque0_bit3 = 0;
  VS::Battle::Ai::SelectLiveFallbackAction(&fixture.state);
  CHECK(std::memcmp(&fixture.state.current_choice, &expected,
                    sizeof(expected)) == 0);
  for (const auto& row : fixture.state.action_slots) {
    for (const auto& slot : row) CHECK(slot.usable == 1);
  }
}

TEST_CASE(
    "Native fallback selection clears only the action when no slot qualifies",
    "[ai][planning][fallback-selection-live]") {
  PlanningFixture fixture;
  using Words = VS::Battle::Ai::ActionCandidate::Words;
  fixture.state.current_choice =
      std::bit_cast<VS::Battle::Ai::ActionCandidateRecord>(
          Words{0xDEADBEEF, 0xFFFFFFFF, 0xABCDEF01, 0xFFFFFFFF, 0xFFFFFFFF});
  for (auto& row : fixture.state.action_slots) {
    for (auto& slot : row) slot.usable = 1;
  }
  for (auto& row : fixture.stats.limb_actions) {
    for (auto& slot : row) slot.action_id = 42;
  }
  auto expected = fixture.state.current_choice;
  expected.action = 0;
  VS::Battle::Ai::SelectLiveFallbackAction(&fixture.state);
  CHECK(std::memcmp(&fixture.state.current_choice, &expected,
                    sizeof(expected)) == 0);
  for (int row = 0; row < 6; ++row) {
    for (int column = 0; column < 4; ++column) {
      CHECK(fixture.state.action_slots[row][column].usable ==
            static_cast<unsigned>(row == 2));
    }
  }
}

TEST_CASE(
    "Native fallback selection keeps selection and usability stats distinct",
    "[ai][planning][fallback-selection-live]") {
  PlanningFixture fixture;
  VS::Battle::ActorStats selection_stats = fixture.stats;
  fixture.state.stats = &selection_stats;
  selection_stats.limb_actions[2][1].action_id = 0;
  fixture.state.action_slots[2][1].special = 1;
  fixture.state.action_slots[2][1].range_squared = 9;
  VS::Battle::Ai::SelectLiveFallbackAction(&fixture.state);
  CHECK(fixture.state.current_choice.action == 0);
  CHECK(fixture.state.current_choice.row == 2);
  CHECK(fixture.state.current_choice.column == 1);
  CHECK(fixture.state.current_choice.distance_squared == 9);
  CHECK(fixture.state.current_choice.weight == 128);
  CHECK(fixture.state.current_choice.options.opaque0_bit2 ==
        VS::Battle::actionDefinitions[0].ai_flags.ignores_sight);
  CHECK(fixture.state.action_slots[2][1].usable == 1);
}

TEST_CASE(
    "Native fallback usability reads actor stats rather than selection stats",
    "[ai][planning][fallback-selection-live]") {
  PlanningFixture fixture;
  VS::Battle::ActorStats selection_stats = fixture.stats;
  fixture.state.stats = &selection_stats;
  fixture.stats.limbs[2].hp = 1;
  fixture.state.action_slots[2][1].special = 1;
  fixture.state.action_slots[2][1].range_squared = 11;
  fixture.state.current_choice.action = 42;
  fixture.state.current_choice.distance_squared = 9;
  VS::Battle::Ai::SelectLiveFallbackAction(&fixture.state);
  CHECK(fixture.state.current_choice.action == 0);
  CHECK(fixture.state.current_choice.distance_squared == 9);
  CHECK(fixture.state.action_slots[2][1].usable == 0);
}

TEST_CASE("Native planning simulation updates only the original stat fields",
          "[ai][planning][simulation-advance-live]") {
  PlanningAdvanceFixture fixture;
  fixture.room.readiness_level = 2880;
  fixture.stats.statuses = 0x11;
  VS::Battle::Ai::kTimedStatusFlags[0] = 1;
  for (auto& limb : fixture.stats.limbs) {
    limb.hp = 0;
    limb.max_hp = 10;
  }
  fixture.state.resource_status_restricted = 1;
  fixture.state.low_mp = fixture.state.low_hp = 1;
  auto expected = fixture.stats;
  expected.statuses = 0;
  for (auto& limb : expected.limbs) limb.hp = 2;
  VS::Battle::Ai::AdvanceLivePlanningSimulation(1);
  CHECK(fixture.room.readiness_level == 2881);
  CHECK(std::memcmp(&fixture.stats, &expected, sizeof(expected)) == 0);
  CHECK(fixture.state.resource_status_restricted == 0);
  CHECK(fixture.state.low_mp == 0);
  CHECK(fixture.state.low_hp == 0);
  for (const auto& row : fixture.state.action_slots) {
    for (const auto& slot : row) CHECK(slot.usable == 1);
  }
}

TEST_CASE(
    "Native planning simulation preserves distinct callback stats sources",
    "[ai][planning][simulation-advance-live]") {
  PlanningAdvanceFixture fixture;
  VS::Battle::ActorStats simulated = fixture.stats;
  simulated.current_hp = 0;
  fixture.battle_actor.stats = &simulated;
  fixture.state.stats = nullptr;
  for (auto& limb : fixture.stats.limbs) limb.hp = 0;
  for (auto& row : fixture.state.action_slots) {
    for (auto& slot : row) slot.usable = 1;
  }
  VS::Battle::Ai::AdvanceLivePlanningSimulation(480);
  CHECK(simulated.current_hp == 1);
  CHECK(fixture.stats.current_hp == 100);
  CHECK(fixture.state.low_hp == 0);
  for (const auto& row : fixture.state.action_slots) {
    for (const auto& slot : row) CHECK(slot.usable == 0);
  }
}

TEST_CASE("Native planning simulation advances an empty actor list",
          "[ai][planning][simulation-advance-live]") {
  PlanningAdvanceFixture fixture;
  VS::Battle::actors[0] = nullptr;
  fixture.room.readiness_level = 65535;
  VS::Battle::Ai::AdvancePlanningSimulationEntry(1);
  CHECK(fixture.room.readiness_level == 0);
}

TEST_CASE("Native planning simulation refreshes each actor by its own slot",
          "[ai][planning][simulation-advance-live]") {
  PlanningAdvanceFixture fixture;
  VS::Battle::ActorStats second_stats = fixture.stats;
  VS::Battle::Actor second{};
  VS::Battle::Ai::EngagementData second_actor{};
  second.id = 3;
  second.stats = &second_stats;
  second_actor.stats = &second_stats;
  fixture.peer.actor = &second_actor;
  fixture.battle_actor.next = &second;
  fixture.stats.current_hp = 0;
  for (auto& limb : fixture.stats.limbs) limb.hp = 0;
  for (auto& limb : second_stats.limbs) limb.hp = 2;
  VS::Battle::Ai::AdvanceLivePlanningSimulation(480);
  CHECK(fixture.stats.current_hp == 1);
  CHECK(second_stats.current_hp == 100);
  CHECK(fixture.state.low_hp == 1);
  CHECK(fixture.peer.low_hp == 0);
  for (int row = 0; row < 6; ++row) {
    for (int column = 0; column < 4; ++column) {
      CHECK(fixture.state.action_slots[row][column].usable == 0);
      CHECK(fixture.peer.action_slots[row][column].usable == 1);
    }
  }
}

TEST_CASE("Planning simulation retains its original module address",
          "[ai][planning][simulation-advance-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DE030) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::AdvancePlanningSimulationEntry));
}

TEST_CASE("Fallback selection retains its original module address",
          "[ai][planning][fallback-selection-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DD604) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::SelectFallbackActionEntry));
}

TEST_CASE(
    "Native planning stats refuse reentry and allow a fresh capture after "
    "release",
    "[ai][planning][stats-lifecycle-live]") {
  PlanningStatsFixture fixture;
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  auto* storage = VS::Battle::Ai::planningStatsSnapshot;
  fixture.first_stats.current_hp = 7;
  CHECK_FALSE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  CHECK(VS::Battle::Ai::planningStatsSnapshot == storage);
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  CHECK(fixture.first_stats.current_hp == 100);
  fixture.first_stats.current_hp = 80;
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  fixture.first_stats.current_hp = 3;
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  CHECK(fixture.first_stats.current_hp == 80);
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
}

TEST_CASE("Native planning stats handle empty and shortened actor lists",
          "[ai][planning][stats-lifecycle-live]") {
  PlanningStatsFixture fixture;
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  fixture.first.next = nullptr;
  fixture.first_stats.current_hp = fixture.second_stats.current_hp = 1;
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  CHECK(fixture.first_stats.current_hp == 100);
  CHECK(fixture.second_stats.current_hp == 1);
  VS::Battle::actors[0] = nullptr;
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
}

TEST_CASE(
    "Native planning stats refuse allocation failure before reading actors",
    "[ai][planning][stats-lifecycle-live]") {
  PlanningStatsFixture fixture;
  std::array<std::byte, 256> tiny{};
  const VS::Core::Memory::ScopedLiveGameHeap heap(tiny);
  VS::Battle::actors[0] =
      reinterpret_cast<VS::Battle::Actor*>(std::uintptr_t{1});
  CHECK_FALSE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
}

TEST_CASE(
    "Native planning stats refuse invalid and aliased actor rows without leaks",
    "[ai][planning][stats-lifecycle-live]") {
  const int failure = GENERATE(0, 1, 2, 3, 4);
  PlanningStatsFixture fixture;
  if (failure == 0) fixture.second.id = 16;
  if (failure == 1) fixture.second.stats = nullptr;
  if (failure == 2) fixture.second.next = &fixture.first;
  if (failure == 4) fixture.second.id = -1;
  if (failure == 3) {
    void* next_storage = VS::Core::Memory::AllocateLiveFromHighEnd(
        VS::Battle::Ai::kPlanningStatsTableBytes);
    REQUIRE(next_storage != nullptr);
    VS::Core::Memory::FreeLiveFromHighEnd(next_storage);
    fixture.second.stats = static_cast<VS::Battle::ActorStats*>(next_storage);
  }
  const auto refused = VS::Core::Memory::RefusedLiveHeapRequestCount();
  CHECK_FALSE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
  CHECK(fixture.first_stats.current_hp == 100);
  CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == refused);
  fixture.second.id = 15;
  fixture.second.stats = &fixture.second_stats;
  fixture.second.next = nullptr;
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  VS::Battle::Ai::RestoreLivePlanningActorStats();
}

TEST_CASE(
    "Native candidate hit weighting decodes the complete ordered result tail",
    "[ai][planning][candidate-evaluation-live]") {
  SimulationResultsFixture fixture;
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  candidate.target_id = 3;
  candidate.action = 42;
  candidate.threshold = 54321;
  candidate.opaque0 = -123456;
  fixture.workspace->target_count = 4;
  fixture.workspace->targets[0].target_actor = 3;
  fixture.workspace->targets[0].targets_position = 1;
  fixture.workspace->targets[0].hit_threshold = 128;
  fixture.workspace->targets[1].target_actor = 2;
  fixture.workspace->targets[1].hit_threshold = 128;
  fixture.workspace->targets[2].target_actor = 3;
  fixture.workspace->targets[3].target_actor = 3;
  fixture.workspace->targets[3].hit_threshold = 99;
  for (unsigned threshold = 0; threshold < 256; ++threshold) {
    fixture.workspace->targets[2].hit_threshold =
        static_cast<unsigned char>(threshold);
    auto expected = candidate;
    expected.weight = (threshold + threshold / 4 + threshold / 32) & 255;
    REQUIRE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(&candidate));
    CHECK(std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(candidate) ==
          std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(expected));
  }
  fixture.workspace->target_count = 0;
  VS::Battle::Ai::UpdateCandidateHitWeightEntry(&candidate);
  CHECK(candidate.weight == 0);
}

TEST_CASE(
    "Native candidate hit weighting refuses unreadable or oversized tails",
    "[ai][planning][candidate-evaluation-live]") {
  SimulationResultsFixture fixture;
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  candidate.weight = 99;
  const auto before =
      std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(candidate);
  CHECK_FALSE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(nullptr));
  fixture.workspace->target_count = 256;
  CHECK_FALSE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(&candidate));
  auto* short_workspace = static_cast<VS::Battle::Ai::ReactionRecord*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(
          sizeof(VS::Battle::Ai::ReactionRecord)));
  REQUIRE(short_workspace != nullptr);
  std::memset(short_workspace, 0, sizeof(*short_workspace));
  short_workspace->target_count = 1;
  VS::Battle::Ai::planningWorkspace = short_workspace;
  CHECK_FALSE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(&candidate));
  VS::Battle::Ai::planningWorkspace = nullptr;
  CHECK_FALSE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(&candidate));
  CHECK(std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(candidate) ==
        before);
}

TEST_CASE(
    "Native candidate evaluation owns its snapshot and preserves packed fields",
    "[ai][planning][candidate-evaluation-live]") {
  ReadinessFixture fixture;
  VS::Battle::Actor ally{};
  VS::Battle::Actor enemy{};
  VS::Battle::ActorStats enemy_stats = fixture.stats;
  enemy_stats.current_hp = 50;
  ally.id = 1;
  ally.stats = &fixture.stats;
  ally.next = &enemy;
  enemy.id = 3;
  enemy.stats = &enemy_stats;
  VS::Battle::actors[0] = &ally;
  fixture.state.relations[1].classification = 2;
  fixture.state.relations[3].classification = 3;
  VS::Battle::Ai::ActionCandidateRecord candidate = fixture.state.candidates[0];
  auto expected = candidate;
  expected.opaque0 = 256 * 128 + 128;
  expected.weight = 128;
  std::array<std::byte, 16 * sizeof(VS::Battle::Ai::ActionTiming)> scratch{};
  auto* scratch_rows = static_cast<std::byte*>(VS::Core::ScratchBase()) + 0x18C;
  std::memcpy(scratch.data(), scratch_rows, scratch.size());
  REQUIRE(VS::Battle::Ai::EvaluateLiveActionCandidate(&fixture.state,
                                                      &candidate, 0, nullptr));
  CHECK(std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(candidate) ==
        std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(expected));
  CHECK(std::memcmp(scratch.data(), scratch_rows, scratch.size()) == 0);
  std::memcpy(scratch_rows, scratch.data(), scratch.size());
  fixture.state.scores_special_actions = 1;
  VS::Battle::actionDefinitions[42].ai_flags.special_mode_action = 1;
  VS::Battle::Ai::EvaluateActionCandidateEntry(
      &fixture.state, &candidate, 3, &VS::Battle::actionDefinitions[42]);
  CHECK(candidate.opaque0 == 257 * 128 + 40);
  CHECK(candidate.weight == 40);
}

TEST_CASE("Native candidate bindings refuse missing state before simulation",
          "[ai][planning][candidate-evaluation-live]") {
  SimulationResultsFixture fixture;
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  CHECK_FALSE(VS::Battle::Ai::EvaluateLiveActionCandidate(nullptr, &candidate,
                                                          0, nullptr));
  CHECK_FALSE(VS::Battle::Ai::EvaluateLiveActionCandidate(&fixture.state,
                                                          nullptr, 0, nullptr));
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      nullptr, fixture.rows, &candidate,
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
  fixture.state.actor_id = 255;
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      &fixture.state, fixture.rows, &candidate,
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
}

TEST_CASE("Native candidate simulation refuses aliased inputs before writes",
          "[ai][planning][candidate-evaluation-live]") {
  SimulationResultsFixture fixture;
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  const auto before = *fixture.request;
  const auto rows_before = fixture.rows;
  const std::span<VS::Battle::Ai::ActionTiming,
                  VS::Battle::Ai::kBattleActorSlotCount>
      aliased_rows(
          reinterpret_cast<VS::Battle::Ai::ActionTiming*>(fixture.workspace),
          VS::Battle::Ai::kBattleActorSlotCount);
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      &fixture.state, aliased_rows, &candidate,
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      reinterpret_cast<VS::Battle::Ai::ActorAiState*>(fixture.workspace),
      fixture.rows, &candidate,
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      &fixture.state, fixture.rows,
      reinterpret_cast<VS::Battle::Ai::ActionCandidateRecord*>(
          fixture.workspace),
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
  CHECK(std::memcmp(&before, fixture.request, sizeof(before)) == 0);
  CHECK(std::memcmp(rows_before.data(), fixture.rows.data(),
                    sizeof(rows_before)) == 0);
}

TEST_CASE("Candidate planning functions retain all original module addresses",
          "[ai][planning][candidate-evaluation-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DCAA0) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::SimulateActionCandidateEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DCBD8) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::UpdateCandidateHitWeightEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DCC94) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::EvaluateActionCandidateEntry));
}

TEST_CASE(
    "Native enumeration preserves special-action fields and distinct stats "
    "sources",
    "[ai][planning][candidate-enumeration-live]") {
  EnumerationFixture fixture;
  const auto kind = GENERATE(1u, 2u, 3u);
  fixture.state.action_slots[2][1].kind = kind;
  VS::Battle::Ai::EnumerateActionCandidatesEntry(&fixture.state, -1);
  REQUIRE(fixture.state.candidate_count == 1);
  const auto& selected = fixture.state.candidates[0];
  CHECK(selected.row == 2);
  CHECK(selected.column == 1);
  CHECK(selected.action == 42);
  CHECK(selected.target_id == 1);
  CHECK(selected.threshold == 0xFFFF);
  CHECK(selected.weight == 40);
  CHECK(selected.target_limb == (kind == 2 ? 2 : 0));
  CHECK(selected.height_offset == (kind == 3 ? 225 : 0));
}

TEST_CASE("Native enumeration caches and ranks without its old scratch records",
          "[ai][planning][candidate-enumeration-live]") {
  EnumerationFixture fixture;
  fixture.state.action_slots[2][1].kind = GENERATE(1u, 2u, 3u);
  std::array<std::byte, 4 * sizeof(VS::Battle::Ai::ActionCandidateRecord)>
      scratch{};
  const auto* scratch_rows =
      static_cast<const std::byte*>(VS::Core::ScratchBase()) + 0x30C;
  std::memcpy(scratch.data(), scratch_rows, scratch.size());
  VS::Battle::Ai::EnumerateActionCandidatesEntry(&fixture.state, -1);
  REQUIRE(fixture.state.candidate_count == 1);
  REQUIRE(VS::Battle::Ai::fallbackCandidateCount == 1);
  const auto& selected = fixture.state.candidates[0];
  CHECK(fixture.state.candidate_order[0] == &selected);
  CHECK(fixture.exclusions[1][42] == 1);
  CHECK(VS::Battle::Ai::fallbackSlotMask == (1 << 9));
  CHECK(std::memcmp(&fixture.fallback[0], &selected, sizeof(selected)) == 0);
  CHECK(std::memcmp(scratch.data(), scratch_rows, scratch.size()) == 0);
}

TEST_CASE("Native enumeration rereads exclusions on repeated calls",
          "[ai][planning][candidate-enumeration-live]") {
  EnumerationFixture fixture;
  fixture.state.action_slots[2][1].kind = GENERATE(1u, 2u, 3u);
  VS::Battle::Ai::EnumerateActionCandidatesEntry(&fixture.state, -1);
  REQUIRE(fixture.state.candidate_count == 1);
  REQUIRE(VS::Battle::Ai::fallbackCandidateCount == 1);
  REQUIRE_NOTHROW(
      VS::Battle::Ai::EnumerateLiveActionCandidates(&fixture.state, 0));
  CHECK(fixture.state.candidate_count == 1);
  CHECK(VS::Battle::Ai::fallbackCandidateCount == 1);
}

TEST_CASE(
    "Native enumeration leaves disabled slots and existing lists untouched",
    "[ai][planning][candidate-enumeration-live]") {
  EnumerationFixture fixture;
  fixture.state.action_slots[2][1].usable = 0;
  fixture.state.candidate_count = 3;
  VS::Battle::Ai::fallbackCandidateCount = 7;
  VS::Battle::Ai::candidateExclusions = nullptr;
  VS::Battle::Ai::fallbackCandidates = nullptr;
  using StateBytes = std::array<std::byte, sizeof(fixture.state)>;
  const auto before = std::bit_cast<StateBytes>(fixture.state);
  REQUIRE_NOTHROW(
      VS::Battle::Ai::EnumerateLiveActionCandidates(&fixture.state, 70000));
  CHECK(std::bit_cast<StateBytes>(fixture.state) == before);
  CHECK(VS::Battle::Ai::fallbackCandidateCount == 7);
  CHECK(VS::Battle::Ai::fallbackSlotMask == 0);
  CHECK(VS::Core::BiosRand().State() == 1);
}

TEST_CASE("Native enumeration retains its original module address",
          "[ai][planning][candidate-enumeration-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800DD918) ==
      reinterpret_cast<void*>(&VS::Battle::Ai::EnumerateActionCandidatesEntry));
}

TEST_CASE("Completed action reconciliation ignores an absent performer state",
          "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  VS::Battle::Ai::actorAiStates[2] = nullptr;
  fixture.state.actor = nullptr;
  const auto expected = fixture.Bytes();
  fixture.Reconcile();
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE(
    "Completed action reconciliation ignores absent and position requests",
    "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  const auto expected = fixture.Bytes();
  VS::Battle::Ai::ReconcileCompletedActionPlanEntry(nullptr);
  CHECK(fixture.Bytes() == expected);
  fixture.request.primary_target.targets_position = 1;
  fixture.Reconcile();
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("Completed action reconciliation leaves non-action commands",
          "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  fixture.actor.command_opcode = 23;
  const auto expected = fixture.Bytes();
  fixture.Reconcile();
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE(
    "Completed action reconciliation records the detour and remembers its "
    "target",
    "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  fixture.object.object_info.object_id = GENERATE(0, 1, 3, 5, 6, 7);
  fixture.Reconcile();
  CHECK(fixture.state.detour_target == 5);
  CHECK(fixture.state.previous_tile == CompletedPlanFixture::kRememberedTile);
}

TEST_CASE(
    "Completed action reconciliation forgets for absent and kind 2 or 4 "
    "objects",
    "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  const int kind = GENERATE(-1, 2, 4);
  if (kind < 0) {
    VS::Field::gimmickInstances[4] = nullptr;
  } else {
    fixture.object.object_info.object_id = kind;
  }
  fixture.Reconcile();
  CHECK(fixture.state.detour_target == 5);
  CHECK(fixture.state.previous_tile == -1);
}

TEST_CASE("Completed action reconciliation skips position targets",
          "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  fixture.request.targets[0].target.target_actor = 200;
  fixture.request.targets[0].targets_position = 1;
  fixture.Reconcile();
  CHECK(fixture.state.detour_target == 5);
  CHECK(fixture.state.previous_tile == CompletedPlanFixture::kRememberedTile);
}

TEST_CASE(
    "Completed action reconciliation compares the full action and command "
    "halves",
    "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  SECTION("an action ID past the candidate field") {
    fixture.request.action_id = 42 + 256;
  }
  SECTION("another column") { fixture.actor.command_arg0 = 3; }
  SECTION("a wider row half") { fixture.actor.command_arg0 = (1 << 16) | 11; }
  VS::Field::gimmickInstances[4] = nullptr;
  fixture.Reconcile();
  CHECK(fixture.state.detour_target == 0);
  CHECK(fixture.state.previous_tile == CompletedPlanFixture::kRememberedTile);
}

TEST_CASE("Completed action reconciliation retains its original module address",
          "[ai][planning][completed-plan-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DEB10) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::ReconcileCompletedActionPlanEntry));
}

TEST_CASE("Native actor plan retains its original module address",
          "[ai][planning][actor-plan-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DE3E4) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::BuildActorActionPlanEntry));
}

TEST_CASE(
    "Native actor plan clears configured exclusions before the zero actor exit",
    "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  fixture.state.actor_id = 0;
  fixture.state.candidate_count = 5;
  VS::Battle::Ai::fallbackCandidateCount = 7;
  VS::Battle::Ai::fallbackSlotMask = -1;
  VS::Battle::Ai::BuildLiveActorActionPlan(&fixture.state);
  CHECK(fixture.state.candidate_count == 0);
  CHECK(VS::Battle::Ai::fallbackCandidateCount == 0);
  CHECK(VS::Battle::Ai::fallbackSlotMask == 0);
  CHECK(fixture.exclusions[1][0] == 0);
  CHECK(fixture.exclusions[1][42] == 0);
  CHECK(fixture.exclusions[1][43] == 1);
  CHECK(fixture.exclusions[3][42] == 1);
  CHECK(fixture.room.readiness_level == 100);
}

TEST_CASE(
    "Native actor plan restores projected stats and releases its snapshot",
    "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  fixture.stats.limb_actions[2][1].action_id = 0;
  using StatsBytes = std::array<std::byte, sizeof(fixture.target_stats)>;
  const auto before = std::bit_cast<StatsBytes>(fixture.target_stats);
  VS::Battle::Ai::BuildLiveActorActionPlan(&fixture.state);
  CHECK(std::bit_cast<StatsBytes>(fixture.target_stats) == before);
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
  CHECK(fixture.room.readiness_level == 0);
  CHECK(fixture.state.candidate_count == 0);
  CHECK(fixture.state.plan_mode == 7);
  CHECK(VS::Core::BiosRand().State() == 1);
}

TEST_CASE(
    "Native actor plan retains the no-snapshot path without changing the clock",
    "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  auto* reserved = VS::Core::Memory::AllocateLiveFromHighEnd(60000);
  REQUIRE(reserved != nullptr);
  VS::Battle::Ai::BuildLiveActorActionPlan(&fixture.state);
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
  CHECK(fixture.room.readiness_level == 100);
  CHECK(fixture.state.candidate_count == 0);
  CHECK(fixture.state.plan_mode == 7);
  CHECK(VS::Core::BiosRand().State() == 1);
  VS::Core::Memory::FreeLiveFromHighEnd(reserved);
}

TEST_CASE(
    "Native actor plan uses real pass storage ranking and final readiness",
    "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  const auto jitter = VS::Core::NextBiosRand() & 7;
  const auto expected_random = VS::Core::BiosRand().State();
  VS::Core::SeedBiosRand(1);
  VS::Battle::Ai::BuildActorActionPlanEntry(&fixture.state);
  REQUIRE(fixture.state.candidate_count == 1);
  CHECK(fixture.state.candidate_order[0] == &fixture.state.candidates[0]);
  CHECK(fixture.state.candidates[0].action == 42);
  CHECK(fixture.state.candidates[0].threshold == jitter);
  CHECK(fixture.exclusions[1][42] == 1);
  CHECK(VS::Core::BiosRand().State() == expected_random);
  const auto saved = std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(
      fixture.pass_candidates[0][0]);
  CHECK(VS::Battle::Ai::ActionCandidate(saved).Action() == 42);
}

TEST_CASE("Native actor plan reads the full arena command argument",
          "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  fixture.stats.limb_actions[2][1].action_id = 0;
  const int argument = GENERATE(4, 0x10004);
  VS::Battle::Ai::arenaOrbit = &fixture.orbit;
  fixture.orbit.plan_saved = 1;
  fixture.orbit.saved_plan_count = 9;
  fixture.battle_actor.command_opcode = 24;
  fixture.battle_actor.command_arg0 = argument;
  fixture.battle_actor.sub_type = 173;
  fixture.state.actor =
      reinterpret_cast<VS::Battle::Ai::EngagementData*>(&fixture.battle_actor);
  VS::State::SetLiveScriptFlag(0xA6, 4);
  VS::Battle::Ai::BuildLiveActorActionPlan(&fixture.state);
  CHECK(VS::State::LiveScriptFlag(0xA6) == (argument == 4 ? 5 : 4));
  CHECK(fixture.orbit.saved_plan_count == 0);
  CHECK(fixture.state.candidate_count == 0);
}

TEST_CASE(
    "Native simulated results preserve row metadata maxima and target order",
    "[ai][planning][simulation-results-live]") {
  SimulationResultsFixture fixture;
  VS::Battle::actionDefinitions[42].type = 3;
  VS::Battle::actionDefinitions[42].cost = 55;
  fixture.workspace->primary_target.hp = 7;
  fixture.workspace->primary_target.hp_effect = 2;
  fixture.workspace->primary_target.added_statuses = 3;
  fixture.workspace->primary_target.removed_statuses = 1;
  fixture.workspace->target_count = 2;
  fixture.workspace->targets[0].target_actor = 0;
  fixture.workspace->targets[0].hp = 30;
  fixture.workspace->targets[0].hp_effect = 2;
  fixture.workspace->targets[1].target_actor = 0;
  fixture.workspace->targets[1].hp = 10;
  fixture.workspace->targets[1].removed_statuses = 2;
  auto expected = fixture.rows;
  expected[0].hp = 27;
  expected[0].statuses = 0x80000000;
  using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
  REQUIRE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  CHECK((std::bit_cast<RowBytes>(fixture.rows) ==
         std::bit_cast<RowBytes>(expected)));
}

TEST_CASE("Native simulated results decode all signed resource operations",
          "[ai][planning][simulation-results-live]") {
  for (const unsigned type : {1u, 3u, 6u}) {
    SimulationResultsFixture fixture;
    VS::Battle::actionDefinitions[42].type = type;
    VS::Battle::actionDefinitions[42].cost = 5;
    auto& performer = fixture.workspace->primary_target;
    performer.hp = 7;
    performer.hp_effect = 2;
    performer.mp = -9;
    performer.weapon_dp_change = 9;
    performer.weapon_dp_effect = 2;
    performer.armor_dp_change = -8;
    performer.armor_dp_effect = 3;
    performer.added_statuses = 5;
    performer.removed_statuses = 1;
    fixture.workspace->target_count = 1;
    auto& target = fixture.workspace->targets[0];
    target.target_actor = 1;
    target.mp = INT16_MAX;
    target.weapon_dp_change = 5;
    target.weapon_dp_effect = 2;
    target.armor_dp_change = INT16_MIN;
    auto expected = fixture.rows;
    expected[0].hp = type == 3 ? 52 : 57;
    expected[0].mp = type == 1 ? 44 : 49;
    expected[0].weapon_dp = type == 6 ? 34 : 39;
    expected[0].shield_dp = 28;
    expected[0].statuses = 0x80000004;
    expected[1].mp = 0;
    expected[1].weapon_dp = 35;
    expected[1].shield_dp = 100;
    using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
    REQUIRE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
    CHECK((std::bit_cast<RowBytes>(fixture.rows) ==
           std::bit_cast<RowBytes>(expected)));
  }
}

TEST_CASE("Native simulated results accept the complete native target capacity",
          "[ai][planning][simulation-results-live]") {
  SimulationResultsFixture fixture;
  fixture.workspace->primary_target.targets_position = 1;
  fixture.workspace->target_count =
      static_cast<std::uint16_t>(std::size(fixture.request->targets));
  using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
  const auto before = std::bit_cast<RowBytes>(fixture.rows);
  REQUIRE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  CHECK((std::bit_cast<RowBytes>(fixture.rows) == before));
}

TEST_CASE(
    "Native simulated results ignore inactive records before action lookup",
    "[ai][planning][simulation-results-live]") {
  SimulationResultsFixture fixture;
  fixture.workspace->action_id = UINT16_MAX;
  fixture.workspace->primary_target.targets_position = -1;
  fixture.workspace->primary_target.target_actor = 255;
  fixture.workspace->target_count = 1;
  fixture.workspace->targets[0].targets_position = 1;
  fixture.workspace->targets[0].target_actor = 255;
  using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
  const auto before = std::bit_cast<RowBytes>(fixture.rows);
  VS::Battle::Ai::ApplySimulatedActionResultsEntry(fixture.rows.data());
  CHECK((std::bit_cast<RowBytes>(fixture.rows) == before));
}

TEST_CASE("Simulated results retain their original module address",
          "[ai][planning][simulation-results-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DC888) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::ApplySimulatedActionResultsEntry));
}

TEST_CASE("Native simulated results refuse short tails and invalid actor slots",
          "[ai][planning][simulation-results-live]") {
  SimulationResultsFixture fixture;
  using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
  const auto before = std::bit_cast<RowBytes>(fixture.rows);
  fixture.workspace->target_count = 256;
  CHECK_FALSE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  fixture.workspace->target_count = 0;
  fixture.workspace->primary_target.target_actor = 16;
  CHECK_FALSE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  auto* short_workspace = static_cast<VS::Battle::Ai::ReactionRecord*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(
          sizeof(VS::Battle::Ai::ReactionRecord)));
  REQUIRE(short_workspace != nullptr);
  std::memset(short_workspace, 0, sizeof(*short_workspace));
  short_workspace->target_count = 1;
  VS::Battle::Ai::planningWorkspace = short_workspace;
  CHECK_FALSE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  VS::Battle::Ai::planningWorkspace = nullptr;
  CHECK_FALSE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  CHECK((std::bit_cast<RowBytes>(fixture.rows) == before));
}

TEST_CASE("Native condition status score reads the row's own AI state",
          "[ai][planning][condition-terms-live]") {
  PlanningFixture fixture;
  std::array<VS::Battle::Ai::ActionTiming,
             VS::Battle::Ai::kBattleActorSlotCount>
      rows{};
  // Row 7 belongs to the fixture's peer in slot 3, so slot 1 is not consulted.
  VS::Battle::Ai::ActionTiming& row = rows[7];
  row.actor_id = 3;
  row.hp = 100;
  row.max_hp = 100;
  row.statuses = (1u << 6) | (1u << 12);
  fixture.state.max_spell_cost = 1;
  fixture.peer.max_spell_cost = 0;
  CHECK(VS::Battle::Ai::LiveActorStatusConditionScore(&row) == 50);
  fixture.peer.max_spell_cost = 1;
  CHECK(VS::Battle::Ai::LiveActorStatusConditionScore(&row) == 50 - 51);
  fixture.peer.max_spell_cost = 2;
  row.hp = 0;
  CHECK(VS::Battle::Ai::LiveActorStatusConditionScore(&row) ==
        50 - 51 - 128 - 84);
}

TEST_CASE("Native condition ratios read each term from the requested row",
          "[ai][planning][condition-terms-live]") {
  std::array<VS::Battle::Ai::ActionTiming,
             VS::Battle::Ai::kBattleActorSlotCount>
      rows{};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    auto& row = rows[index];
    row.actor_id = static_cast<std::uint32_t>(rows.size() - 1 - index);
    row.hp = static_cast<std::int16_t>(2 * index);
    row.max_hp = 128;
    row.mp = static_cast<std::int16_t>(3 * index);
    row.max_mp = 128;
    row.weapon_dp = static_cast<std::int16_t>(5 * index);
    row.weapon_max_dp = 128;
    row.shield_dp = static_cast<std::int16_t>(7 * index);
    row.shield_max_dp = 128;
  }
  for (int id = 0; id < VS::Battle::Ai::kBattleActorSlotCount; ++id) {
    CHECK(VS::Battle::Ai::LiveActorHpRatio(rows.data(), id) == 2 * id);
    CHECK(VS::Battle::Ai::LiveActorMpRatio(rows.data(), id) == 3 * id);
    CHECK(VS::Battle::Ai::LiveActorEquipmentDpRatio(rows.data(), id) ==
          12 * id);
  }
}

TEST_CASE("Native condition ratios return unnarrowed signed quotients",
          "[ai][planning][condition-terms-live]") {
  std::array<VS::Battle::Ai::ActionTiming,
             VS::Battle::Ai::kBattleActorSlotCount>
      rows{};
  VS::Battle::Ai::ActionTiming& row = rows[9];
  row.hp = INT16_MIN;
  row.max_hp = 1;
  row.mp = -1;
  row.max_mp = 3;
  row.weapon_dp = INT16_MAX;
  row.weapon_max_dp = 1;
  row.shield_dp = INT16_MAX;
  row.shield_max_dp = 1;
  CHECK(VS::Battle::Ai::LiveActorHpRatio(rows.data(), 9) == -4194304);
  CHECK(VS::Battle::Ai::LiveActorMpRatio(rows.data(), 9) == -42);
  CHECK(VS::Battle::Ai::LiveActorEquipmentDpRatio(rows.data(), 9) ==
        2 * 4194176);
  CHECK(VS::Battle::Ai::LiveActorHpRatio(rows.data(), 8) == 0);
}

TEST_CASE(
    "Native candidate readiness preserves exact status byte and threshold",
    "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  auto expected = fixture.state;
  expected.action_status = 1;
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        false);
  using StateBytes = std::array<std::byte, sizeof(fixture.state)>;
  CHECK((std::bit_cast<StateBytes>(fixture.state) ==
         std::bit_cast<StateBytes>(expected)));
  fixture.room.readiness_level = 101;
  CHECK(VS::Battle::Ai::RefreshActionCandidateReadinessEntry(
            reinterpret_cast<VS::Battle::Ai::CandidateReadinessView*>(
                &fixture.state)) == 0);
  CHECK(fixture.state.action_status == 0);
}

TEST_CASE("Native candidate readiness clears only weight and exhausted count",
          "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  fixture.state.candidates[0].threshold = 100;
  fixture.state.relations[3].classification = 0;
  auto expected = fixture.state;
  expected.candidates[0].weight = 0;
  expected.candidate_count = 0;
  expected.action_status = 3;
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        true);
  using StateBytes = std::array<std::byte, sizeof(fixture.state)>;
  CHECK((std::bit_cast<StateBytes>(fixture.state) ==
         std::bit_cast<StateBytes>(expected)));
}

TEST_CASE("Native candidate readiness refreshes actual action slot usability",
          "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  auto candidate = VS::Battle::Ai::ActionCandidate(
      std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(
          fixture.state.candidates[0]));
  candidate.SetThreshold(100);
  candidate.SetOption(VS::Battle::Ai::CandidateOption::kHarmful, true);
  fixture.state.candidates[0] =
      std::bit_cast<VS::Battle::Ai::ActionCandidateRecord>(
          candidate.RawWords());
  fixture.state.relations[3].classification = 3;
  VS::Battle::actionDefinitions[42].ai_flags =
      std::bit_cast<decltype(VS::Battle::actionDefinitions[42].ai_flags)>(
          std::uint16_t{0x8000});
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        false);
  CHECK(fixture.state.candidates[0].weight == 17);
  CHECK(fixture.state.action_slots[2][1].usable == 1);
  CHECK(fixture.state.action_status == 0);
  fixture.actor.stats = nullptr;
  fixture.state.action_status = 9;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state));
  CHECK(fixture.state.candidates[0].weight == 17);
  CHECK(fixture.state.action_status == 9);
}

TEST_CASE(
    "Native candidate readiness promotes first survivor without compaction",
    "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  fixture.state.candidate_count = 3;
  fixture.state.candidates[0].weight = 0;
  fixture.state.candidates[1].weight = 0;
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        true);
  CHECK(fixture.state.candidate_order[0] == &fixture.state.candidates[2]);
  CHECK(fixture.state.candidate_order[1] == &fixture.state.candidates[1]);
  CHECK(fixture.state.candidate_order[2] == &fixture.state.candidates[2]);
  CHECK(fixture.state.candidate_count == 3);
  CHECK(fixture.state.plan_mode == 1);
  CHECK(fixture.state.action_status == 1);
}

TEST_CASE("Native candidate readiness empty status checks actual hostile links",
          "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  fixture.state.candidate_count = 0;
  fixture.hostile.id = 4;
  fixture.state.relations[4].classification = 3;
  VS::Battle::actors[0] = &fixture.hostile;
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        false);
  CHECK(fixture.state.action_status == 2);
  fixture.state.relations[4].classification = 2;
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        false);
  CHECK(fixture.state.action_status == 3);
  fixture.hostile.next = &fixture.hostile;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state));
}

TEST_CASE("Candidate readiness retains its original module address",
          "[ai][planning][candidate-readiness-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800D85D8) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::RefreshActionCandidateReadinessEntry));
}

TEST_CASE("Native candidate readiness refuses absent malformed and short state",
          "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  CHECK_FALSE(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(nullptr));
  fixture.state.candidate_count = 17;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state));
  fixture.state.candidate_count = 1;
  fixture.state.candidate_order[0] = nullptr;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state));
  std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap(arena);
  auto* state = VS::Core::Memory::AllocateLiveFromLowEnd(16);
  REQUIRE(state != nullptr);
  CHECK_FALSE(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(
      static_cast<VS::Battle::Ai::ActorAiState*>(state)));
}

TEST_CASE("AI target memory leaves empty and negative candidate sets untouched",
          "[ai][planning][target-memory]") {
  TargetMemoryFixture fixture(0);
  fixture.state.candidate_count = GENERATE(-1, 0);
  fixture.state.candidate_order[0] = nullptr;
  const auto expected = fixture.Bytes();
  VS::Battle::Ai::RememberLiveAiActionTarget(&fixture.state);
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("AI target memory preserves the original nonzero settings guard",
          "[ai][planning][target-memory]") {
  TargetMemoryFixture fixture(15);
  fixture.state.target_memory_disabled = static_cast<char>(GENERATE(1, 255));
  fixture.state.candidate_count = GENERATE(1, 16);
  fixture.state.candidate_order[0] = nullptr;
  const auto expected = fixture.Bytes();
  VS::Battle::Ai::RememberLiveAiActionTarget(&fixture.state);
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("AI target forgetting writes only the negative previous tile",
          "[ai][planning][target-forget-live]") {
  const int fill = GENERATE(0x00, 0x5A, 0xFF);
  VS::Battle::Ai::ActorAiState state{};
  std::memset(&state, fill, sizeof(state));
  std::array<std::byte, sizeof(state)> expected{};
  std::memcpy(expected.data(), &state, expected.size());
  std::memset(
      expected.data() + offsetof(VS::Battle::Ai::ActorAiState, previous_tile),
      0xFF, sizeof(state.previous_tile));
  VS::Battle::Ai::ForgetLiveAiActionTarget(&state);
  std::array<std::byte, sizeof(state)> actual{};
  std::memcpy(actual.data(), &state, actual.size());
  CHECK(actual == expected);
  CHECK(state.previous_tile == -1);
}

TEST_CASE("AI target forgetting keeps the remembered destination and lifetime",
          "[ai][planning][target-forget-live]") {
  TargetMemoryFixture fixture(5);
  fixture.state.tile.word = 0x00430201;
  VS::Battle::Ai::RememberLiveAiActionTarget(&fixture.state);
  REQUIRE(fixture.state.previous_tile == static_cast<int>(0x05430201));
  auto expected = fixture.Bytes();
  std::memset(
      expected.data() + offsetof(VS::Battle::Ai::ActorAiState, previous_tile),
      0xFF, sizeof(fixture.state.previous_tile));
  VS::Battle::Ai::ForgetLiveAiActionTarget(&fixture.state);
  CHECK(fixture.Bytes() == expected);
  CHECK(fixture.state.previous_tile == -1);
  CHECK(fixture.state.remembered_target_tile == 0xA1B2C3D4u);
  CHECK(fixture.state.target_memory_frames == 900);
}

static_assert(offsetof(VS::Battle::Ai::BlockedRecoveryView, is_stuck) ==
              offsetof(VS::Battle::Ai::ActorAiState, is_stuck));
static_assert(offsetof(VS::Battle::Ai::BlockedRecoveryView, opaque_1c) ==
              offsetof(VS::Battle::Ai::ActorAiState, holds_stall_tile));
static_assert(offsetof(VS::Battle::Ai::BlockedRecoveryView, tile) ==
              offsetof(VS::Battle::Ai::ActorAiState, tile));
static_assert(offsetof(VS::Battle::Ai::BlockedRecoveryView, previous_tile) ==
              offsetof(VS::Battle::Ai::ActorAiState, previous_tile));
static_assert(offsetof(VS::Battle::Ai::BlockedRecoveryView, detour_step) ==
              offsetof(VS::Battle::Ai::ActorAiState, detour_step));
static_assert(sizeof(VS::Battle::Ai::BlockedRecoveryView) <=
              sizeof(VS::Battle::Ai::ActorAiState));

TEST_CASE("Blocked movement forgets its target after exhausting detours",
          "[ai][movement][target-forget-live]") {
  struct OrbitGuard {
    VS::Battle::Ai::ArenaOrbitState* saved = VS::Battle::Ai::arenaOrbit;
    ~OrbitGuard() { VS::Battle::Ai::arenaOrbit = saved; }
  } orbit_guard;
  VS::Battle::Ai::arenaOrbit = nullptr;
  VS::Battle::Ai::ActorAiState state{};
  auto* view = reinterpret_cast<VS::Battle::Ai::BlockedRecoveryView*>(&state);
  view->tile.word = 0x00120034;
  view->previous_tile.word = 0x05120034;
  view->opaque_1c = 7;
  view->detour_step = static_cast<std::uint8_t>(GENERATE(5, 255));
  std::array<std::byte, sizeof(state)> expected{};
  std::memcpy(expected.data(), &state, expected.size());
  expected[offsetof(VS::Battle::Ai::ActorAiState, is_stuck)] = std::byte{1};
  expected[offsetof(VS::Battle::Ai::ActorAiState, holds_stall_tile)] =
      std::byte{0};
  expected[offsetof(VS::Battle::Ai::ActorAiState, detour_step)] = std::byte{0};
  std::memset(
      expected.data() + offsetof(VS::Battle::Ai::ActorAiState, previous_tile),
      0xFF, sizeof(state.previous_tile));
  const int result = VS::Battle::Ai::TryRecoverLiveBlockedMovement(view);
  std::array<std::byte, sizeof(state)> actual{};
  std::memcpy(actual.data(), &state, actual.size());
  CHECK(result == 0);
  CHECK(actual == expected);
}

TEST_CASE("AI target forgetting retains its original module address",
          "[ai][planning][target-forget-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800E4CE8) ==
      reinterpret_cast<const void*>(&VS::Battle::Ai::ForgetLiveAiActionTarget));
}

namespace {
using VS::Battle::Ai::ActorAiState;
constexpr std::size_t kSlotBytes = sizeof(ActorAiState::action_slots);
using SlotBytes = std::array<std::byte, kSlotBytes>;

SlotBytes SlotBytesOf(const VS::Battle::Ai::ActorAiState& state) {
  SlotBytes bytes{};
  std::memcpy(bytes.data(), &state.action_slots, bytes.size());
  return bytes;
}

// Action 42 as a radial category-14 action reaching 96 units.
void DefineMetadataAction(unsigned type) {
  ActionDefinition& action = VS::Battle::actionDefinitions[42];
  action = {};
  action.type = type;
  action.category = 14;
  action.cost = 30;
  action.range_x = 1;
  action.range_z = 4;
  action.shape = 1;
  action.ai_flags.radial_area = 1;
  action.area_range_y = 3;
}
}  // namespace

TEST_CASE("AI action metadata writes the typed slot fields and keeps the rest",
          "[ai][planning][action-metadata-live]") {
  const int fill = GENERATE(0x00, 0xA5, 0xFF);
  const unsigned type = GENERATE(0u, 1u);
  PlanningAdvanceFixture fixture;
  fixture.room.allows_non_spell_actions = 1;
  DefineMetadataAction(type);
  std::memset(&fixture.state.action_slots, fill,
              sizeof(fixture.state.action_slots));
  fixture.state.max_spell_cost = 10;
  VS::Battle::Ai::ActorAiState expected = fixture.state;
  for (auto& row : expected.action_slots) {
    for (auto& slot : row) {
      slot.excludes_self = 1;
      slot.kind = 2;
      slot.long_range = 0;
      slot.special = type == 0 ? 1u : 0u;
      slot.radius = 96;
      slot.range_squared = 96 * 96;
    }
  }
  VS::Battle::Ai::BuildLiveAiActionMetadata(&fixture.state);
  CHECK(SlotBytesOf(fixture.state) == SlotBytesOf(expected));
  CHECK(fixture.state.max_spell_cost == (type == 1 ? 30 : 10));
  const auto& slot = fixture.state.action_slots[3][2];
  CHECK(slot.reserved0_bit0 == (fill & 1u));
  CHECK(slot.ignores_obstruction == ((fill >> 2) & 1u));
  CHECK(slot.usable == ((fill >> 7) & 1u));
  CHECK(slot.reserved3_bit0 == static_cast<unsigned>(fill));
}

TEST_CASE("AI action metadata reads weapon ranges from the actor's stats",
          "[ai][planning][action-metadata-live]") {
  PlanningAdvanceFixture fixture;
  DefineMetadataAction(0);
  VS::Battle::actionDefinitions[42].range_x = 0xFF;
  VS::Battle::actionDefinitions[42].ai_flags.radial_area = 0;
  fixture.stats.current_range_z = 4;
  fixture.stats.current_attack_shape = 1;
  VS::Battle::Ai::BuildLiveAiActionMetadata(&fixture.state);
  CHECK(fixture.state.action_slots[0][0].range_squared == 96 * 96);
  CHECK(fixture.state.action_slots[5][3].range_squared == 96 * 96);
  CHECK(fixture.state.action_slots[0][0].radius == 0);
  CHECK(fixture.state.action_slots[0][0].special == 0);
}

TEST_CASE("AI action metadata only sets the player's threshold",
          "[ai][planning][action-metadata-live]") {
  VS::Battle::Ai::ActorAiState state{};
  std::memset(&state.action_slots, 0x5A, sizeof(state.action_slots));
  state.actor_id = 0;
  state.stats = nullptr;
  const SlotBytes slots = SlotBytesOf(state);
  VS::Battle::Ai::BuildLiveAiActionMetadata(&state);
  CHECK(state.max_spell_cost == 1);
  CHECK(SlotBytesOf(state) == slots);
}

TEST_CASE("AI action metadata retains its original module address",
          "[ai][planning][action-metadata-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E5710) ==
        reinterpret_cast<const void*>(
            &VS::Battle::Ai::BuildLiveAiActionMetadata));
}

TEST_CASE("AI candidates copy the complete record including overlapping input",
          "[ai][planning]") {
  const auto source_offset = GENERATE(0, 4);
  const auto destination_offset = GENERATE(0, 4, 8);
  alignas(VS::Battle::Ai::ActionCandidateRecord) std::array<unsigned char, 32>
      bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<unsigned char>(index + 1);
  }
  auto expected = bytes;
  std::memmove(expected.data() + destination_offset,
               expected.data() + source_offset, 20);
  VS::Battle::Ai::CopyLiveActionCandidate(
      reinterpret_cast<VS::Battle::Ai::ActionCandidateRecord*>(
          bytes.data() + destination_offset),
      reinterpret_cast<VS::Battle::Ai::ActionCandidateRecord*>(bytes.data() +
                                                               source_offset));
  CHECK(bytes == expected);
}

TEST_CASE(
    "AI append owns complete candidates and marks exact target/action pairs",
    "[ai][planning]") {
  PlanningFixture fixture;
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  candidate.target_id = 15;
  candidate.action = 255;
  candidate.weight = 23;
  candidate.threshold = 0xCAFE;
  for (int index = 0; index < 16; ++index) {
    candidate.opaque0 = index - 5;
    VS::Battle::Ai::AppendLiveActionCandidate(&fixture.state, &candidate);
    REQUIRE(fixture.state.candidate_count == index + 1);
    CHECK(fixture.state.candidate_order[index] ==
          &fixture.state.candidates[index]);
    CHECK(std::memcmp(&fixture.state.candidates[index], &candidate,
                      sizeof(candidate)) == 0);
  }
  CHECK(fixture.exclusions[15][255] == 1);
  candidate.target_id = 9;
  candidate.action = 81;
  VS::Battle::Ai::AppendLiveActionCandidate(&fixture.state, &candidate);
  CHECK(fixture.state.candidate_count == 16);
  CHECK(fixture.exclusions[9][81] == 0);
}

TEST_CASE("AI rejects zero-weight candidates without side effects",
          "[ai][planning]") {
  PlanningFixture fixture;
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  candidate.target_id = 3;
  candidate.action = 42;
  VS::Battle::Ai::AppendLiveActionCandidate(&fixture.state, &candidate);
  CHECK(fixture.state.candidate_count == 0);
  CHECK(fixture.exclusions[3][42] == 0);
}

TEST_CASE(
    "AI ranking uses signed priorities and the original midpoint partition",
    "[ai][planning]") {
  VS::Battle::Ai::ActionPlanView plan{};
  const std::array<int, 6> scores{2, -3, 2, 9, 9, 0};
  const std::array<int, 6> expected{4, 3, 2, 0, 5, 1};
  plan.candidate_count = static_cast<int>(scores.size());
  for (std::size_t index = 0; index < scores.size(); ++index) {
    plan.candidates[index].score = scores[index];
    plan.candidate_order[index] = &plan.candidates[index];
  }
  VS::Battle::Ai::SortLiveActionCandidates(&plan);
  for (std::size_t index = 0; index < expected.size(); ++index) {
    CHECK(plan.candidate_order[index] == &plan.candidates[expected[index]]);
  }
}

TEST_CASE(
    "AI ranking lifecycle produces, consumes, resets and repeats real plans",
    "[ai][planning][pointer-sort]") {
  PlanningFixture fixture;
  auto saved_actor = VS::Battle::actors[0];
  VS::Battle::Actor listed{};
  listed.id = 3;
  VS::Battle::actors[0] = &listed;
  auto& state = fixture.state;
  auto& plan = reinterpret_cast<VS::Battle::Ai::ActionPlanView&>(state);
  const std::array<int, 6> scores{2, -3, 2, 9, 9, 0};
  const std::array<std::size_t, 6> expected{4, 3, 2, 0, 5, 1};

  for (int entry = 0; entry < 2; ++entry) {
    CAPTURE(entry);
    for (std::size_t index = 0; index < scores.size(); ++index) {
      VS::Battle::Ai::ActionCandidateRecord candidate{};
      candidate.opaque0 = scores[index];
      candidate.weight = 1;
      candidate.target_id = 3;
      candidate.action = static_cast<unsigned>(40 + index);
      fixture.stats.limb_actions[index][0].action_id =
          static_cast<std::uint16_t>(candidate.action);
      VS::Battle::Ai::AppendLiveActionCandidate(&state, &candidate);
      CHECK(fixture.exclusions[3][candidate.action] == 1);
    }
    REQUIRE(state.candidate_count == 6);
    VS::Battle::Ai::SortLiveActionCandidates(&plan);
    for (std::size_t index = 0; index < expected.size(); ++index) {
      CHECK(state.candidate_order[index] == &state.candidates[expected[index]]);
    }

    // state.actorId == 0 takes the planner's real reset-only path.
    state.actor_id = 0;
    VS::Battle::Ai::BuildLiveActorActionPlan(&state);
    state.actor_id = 1;
    CHECK(state.candidate_count == 0);
    for (std::size_t index = 0; index < scores.size(); ++index) {
      CHECK(fixture.exclusions[3][40 + index] == 0);
    }
  }
  VS::Battle::actors[0] = saved_actor;
}

TEST_CASE("AI ranking producer refuses zero weights and a full candidate list",
          "[ai][planning][pointer-sort]") {
  PlanningFixture fixture;
  auto& state = fixture.state;
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  candidate.target_id = 3;
  candidate.action = 42;
  VS::Battle::Ai::AppendLiveActionCandidate(&state, &candidate);
  CHECK(state.candidate_count == 0);
  CHECK(fixture.exclusions[3][42] == 0);

  candidate.weight = 1;
  for (int index = 0; index < 16; ++index) {
    candidate.opaque0 = index;
    VS::Battle::Ai::AppendLiveActionCandidate(&state, &candidate);
  }
  REQUIRE(state.candidate_count == 16);
  const auto ordered = state.candidate_order;
  candidate.opaque0 = 99;
  VS::Battle::Ai::AppendLiveActionCandidate(&state, &candidate);
  CHECK(state.candidate_count == 16);
  CHECK(state.candidate_order == ordered);
}

TEST_CASE("AI simulated stat deltas clamp add and subtract at real bounds",
          "[ai][planning]") {
  const int operation = GENERATE(0, 1, 2, 3);
  int16_t current = 50;
  int16_t maximum = 100;
  VS::Battle::Ai::ApplyLiveSimulatedStatDelta(&current, &maximum, 20,
                                              operation);
  CHECK(current == (operation == 2 ? 70 : 30));
  VS::Battle::Ai::ApplyLiveSimulatedStatDelta(&current, &maximum, 1000,
                                              operation);
  CHECK(current == (operation == 2 ? 100 : 0));
  CHECK(maximum == 100);
}

TEST_CASE("AI planning snapshots copy only the original simulation stat ranges",
          "[ai][planning]") {
  VS::Battle::ActorStats source;
  VS::Battle::ActorStats destination;
  std::ranges::fill(std::as_writable_bytes(std::span{&source, 1}),
                    std::byte{0x5A});
  std::ranges::fill(std::as_writable_bytes(std::span{&destination, 1}),
                    std::byte{0xA5});
  VS::Battle::Ai::CopyLivePlanningActorStats(&source, &destination);
  const auto bytes = std::as_bytes(std::span{&destination, 1});
  for (std::size_t offset = 0; offset < bytes.size(); ++offset) {
    CAPTURE(offset);
    CHECK(bytes[offset] ==
          static_cast<std::byte>(CopiedStatByte(offset) ? 0x5A : 0xA5));
  }
}

TEST_CASE("AI action usability applies resource thresholds to every limb slot",
          "[ai][planning]") {
  const int type = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const int row = GENERATE(0, 1, 2, 3, 4, 5);
  const int column = GENERATE(0, 1, 2, 3);
  PlanningFixture fixture;
  fixture.stats.limbs[row].hp = 2;
  fixture.stats.limb_actions[row][column].action_id = 42;
  auto& slot = fixture.state.action_slots[row][column];
  slot = std::bit_cast<VS::Battle::Ai::ActionSlotMetadataRecord>(
      std::array<uint32_t, 2>{0xA5CAFE13, 0xDEADBEAF});
  auto& action = VS::Battle::actionDefinitions[42];
  action.type = static_cast<std::uint8_t>(type);
  action.cost = 10;
  fixture.stats.current_hp = 59;
  fixture.stats.current_mp = 9;
  fixture.stats.weapon.current_pp = 9;
  CHECK(VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, row,
                                                     column) ==
        !(type == 1 || type == 3 || type == 4));
  fixture.stats.current_hp = 60;
  fixture.stats.current_mp = 10;
  fixture.stats.weapon.current_pp = 10;
  CHECK(VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, row,
                                                     column));
  CHECK(slot.usable == 1);
  CHECK(std::bit_cast<std::array<uint32_t, 2>>(slot) ==
        std::array<uint32_t, 2>{0xA5CAFE93, 0xDEADBEAF});
}

TEST_CASE("AI action usability applies status and damaged-limb restrictions",
          "[ai][planning]") {
  const auto type = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  PlanningFixture fixture;
  VS::Battle::actionDefinitions[42].type = static_cast<std::uint8_t>(type);
  fixture.stats.statuses = 0x2000;
  CHECK(VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1) ==
        (type == 1 || type == 2));
  fixture.stats.statuses = 0;
  fixture.stats.limbs[2].hp = 1;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1));
  CHECK(fixture.state.action_slots[2][1].usable == 0);
}

TEST_CASE(
    "AI resource conditions use unsigned halves and the actual status mask",
    "[ai][planning]") {
  PlanningFixture fixture;
  fixture.state.max_spell_cost = 10;
  fixture.stats.current_hp = 49;
  fixture.stats.current_mp = 9;
  fixture.stats.statuses = 0x90000;
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.reserved10[0] == 0);
  CHECK(fixture.state.reserved10[1] == 1);
  CHECK(fixture.state.reserved10[2] == 1);
  fixture.stats.current_hp = 50;
  fixture.stats.current_mp = 50;
  fixture.stats.statuses = 0x1000;
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.reserved10[0] == 1);
  CHECK(fixture.state.reserved10[1] == 0);
  CHECK(fixture.state.reserved10[2] == 0);
  fixture.state.max_spell_cost = 0;
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.reserved10[0] == 0);
}

TEST_CASE(
    "AI target eligibility observes relations, condition masks and self "
    "restrictions",
    "[ai][planning]") {
  PlanningFixture fixture;
  VS::Battle::Ai::ActionCandidateRecord candidate{};
  candidate.row = 2;
  candidate.column = 1;
  candidate.action = 42;
  fixture.state.relations[3].classification = 2;
  CHECK(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state, &candidate,
                                                   3));
  candidate.options.opaque0_bit4 = 1;
  CHECK_FALSE(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state,
                                                         &candidate, 3));
  fixture.state.relations[3].classification = 3;
  CHECK(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state, &candidate,
                                                   3));
  candidate.options.opaque0_bit4 = 0;
  candidate.options.opaque0_bit5 = 1;
  CHECK_FALSE(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state,
                                                         &candidate, 3));
  fixture.state.relations[3].classification = 2;
  fixture.peer.actor = &fixture.actor;
  fixture.peer.max_spell_cost = 10;
  fixture.stats.current_mp = 9;
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.peer);
  VS::Battle::actionDefinitions[42].ai_flags =
      std::bit_cast<VS::Battle::ActionAiFlags>(std::uint16_t{0x400});
  CHECK(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state, &candidate,
                                                   3));
  fixture.stats.current_mp = 50;
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.peer);
  CHECK_FALSE(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state,
                                                         &candidate, 3));
}

TEST_CASE(
    "AI movement setup stores the signed maximum step before byte packing",
    "[ai][planning]") {
  const std::array<std::array<int, 5>, 8> examples{{
      {8, 12, 8, 12, 12},
      {16, 6, 16, 6, 16},
      {4, 4, 4, 4, 4},
      {0, 255, 0, 255, 255},
      {256, 0, 0, 1, 0},
      {-1, 0, 255, 255, 0},
      {0, -1, 0, 255, 0},
      {-3, -7, 253, 255, 253},
  }};
  const auto index = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const auto& example = examples[index];
  PlanningFixture fixture;
  VS::Battle::Ai::SetLiveActorVerticalMovementSteps(1, example[0], example[1]);
  CHECK(fixture.state.walk_step == example[2]);
  CHECK(fixture.state.run_step == example[3]);
  CHECK(fixture.state.max_range == example[4]);
}

TEST_CASE("AI sorting covers empty/full lists and signed extremes without RNG",
          "[ai][planning]") {
  const auto count = GENERATE(0, 1, 2, 6, 16);
  const auto pattern = GENERATE(0, 1, 2);
  VS::Battle::Ai::ActionPlanView plan{};
  plan.candidate_count = count;
  const auto random_table = std::to_array(VS::Core::randomShuffleTable);
  const auto random_index = VS::Core::randomShuffleIndex;
  const auto interface_random = VS::Core::interfaceRandomState;
  for (int index = 0; index < count; ++index) {
    plan.candidates[index].score =
        pattern == 0 ? index : (pattern == 1 ? -index : 0);
    plan.candidate_order[index] = &plan.candidates[index];
  }
  if (count > 1 && pattern != 2) {
    plan.candidates[0].score = std::numeric_limits<int>::min();
    plan.candidates[count - 1].score = std::numeric_limits<int>::max();
  }
  const auto records = plan.candidates;
  VS::Battle::Ai::SortLiveActionCandidates(&plan);
  for (int index = 1; index < count; ++index) {
    CHECK(plan.candidate_order[index - 1]->score >=
          plan.candidate_order[index]->score);
  }
  CHECK(std::memcmp(records.data(), plan.candidates.data(),
                    sizeof(plan.candidates)) == 0);
  CHECK(std::to_array(VS::Core::randomShuffleTable) == random_table);
  CHECK(VS::Core::randomShuffleIndex == random_index);
  CHECK(VS::Core::interfaceRandomState == interface_random);
}

TEST_CASE(
    "AI resource comparisons retain signed MP/PP and unsigned HP semantics",
    "[ai][planning]") {
  PlanningFixture fixture;
  auto& action = VS::Battle::actionDefinitions[42];
  action.cost = 10;
  action.type = 1;
  fixture.stats.current_mp = -1;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1));
  fixture.stats.current_mp = 10;
  fixture.stats.statuses = 0x1000;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1));
  fixture.stats.statuses = 0;
  action.type = 4;
  fixture.stats.weapon.current_pp = 0xFFFF;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1));
  action.type = 3;
  fixture.stats.current_hp = -1;
  CHECK(VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1));
}

TEST_CASE("AI condition status mask rejects only the documented source bits",
          "[ai][planning]") {
  const auto bit =
      GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
               19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
  PlanningFixture fixture;
  fixture.stats.statuses = std::bit_cast<int>(uint32_t{1} << bit);
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.resource_status_restricted ==
        (((uint32_t{1} << bit) & 0xE2B0) != 0));
}
