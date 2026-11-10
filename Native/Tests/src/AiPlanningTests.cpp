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

#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.h"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Damage.h"
#include "Battle/StatusEffects.h"
#include "Core/Random.h"
#include "Field/Gimmicks.h"
}

namespace {
struct PlanningFixture {
  decltype(Battle_Ai_candidateExclusions) saved_exclusions =
      Battle_Ai_candidateExclusions;
  std::array<Battle_Ai_ActorAiState*, 16> saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  Battle_ActionDefinition saved_action = Battle_actionDefinitions[42];
  Battle_Ai_ActorAiState state{};
  Battle_Ai_ActorAiState peer{};
  Battle_Ai_EngagementData actor{};
  Battle_ActorStats stats{};
  u_char exclusions[16][256]{};

  PlanningFixture() {
    Battle_Ai_candidateExclusions = exclusions;
    std::fill_n(Battle_Ai_actorAiStates, 16, nullptr);
    Battle_Ai_actorAiStates[1] = &state;
    Battle_Ai_actorAiStates[3] = &peer;
    Battle_actionDefinitions[42] = {};
    actor.opaque3C = &stats;
    state.actor = &actor;
    state.stats = &stats;
    state.actorId = 1;
    stats.currentHp = stats.maxHp = 100;
    stats.currentMp = stats.maxMp = 100;
    stats.weapon.currentPp = 100;
    stats.limbs[2].hp = 2;
    stats.limbActions[2][1].actionId = 42;
  }

  ~PlanningFixture() {
    Battle_Ai_candidateExclusions = saved_exclusions;
    std::copy(saved_states.begin(), saved_states.end(),
              Battle_Ai_actorAiStates);
    Battle_actionDefinitions[42] = saved_action;
  }
};

struct PlanningAdvanceFixture : PlanningFixture {
  Battle_Ai_RoomAiState* saved_room = Battle_Ai_roomAiState;
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  std::array<u_int, 5> saved_masks{};
  Battle_StatusEffectParams saved_poison = Battle_statusEffectParams[14];
  Battle_StatusEffectParams saved_regeneration = Battle_statusEffectParams[17];
  Battle_Ai_RoomAiState room{};
  Battle_Actor battle_actor{};

  PlanningAdvanceFixture() {
    std::copy_n(Battle_Ai_kTimedStatusFlags, saved_masks.size(),
                saved_masks.begin());
    std::fill_n(Battle_Ai_kTimedStatusFlags, saved_masks.size(), 0);
    Battle_statusEffectParams[14].tickInterval = 1;
    Battle_statusEffectParams[17].tickInterval = 1;
    Battle_Ai_roomAiState = &room;
    std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
    battle_actor.id = 1;
    battle_actor.stats = &stats;
    Battle_actors[0] = &battle_actor;
    for (auto& row : stats.limbActions) {
      for (auto& slot : row) slot.actionId = 42;
    }
  }
  ~PlanningAdvanceFixture() {
    Battle_Ai_roomAiState = saved_room;
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_masks.begin(), saved_masks.end(),
              Battle_Ai_kTimedStatusFlags);
    Battle_statusEffectParams[14] = saved_poison;
    Battle_statusEffectParams[17] = saved_regeneration;
  }
};

struct PlanningStatsFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  Battle_ActorStats* saved_snapshot = Battle_Ai_planningStatsSnapshot;
  std::array<std::byte, 65536> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Battle_Actor first{}, second{};
  Battle_ActorStats first_stats{}, second_stats{};

  PlanningStatsFixture() {
    VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
    std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
    first.id = 1;
    first.stats = &first_stats;
    first.next = &second;
    second.id = 15;
    second.stats = &second_stats;
    Battle_actors[0] = &first;
    first_stats.currentHp = 100;
    second_stats.currentHp = 50;
  }
  ~PlanningStatsFixture() {
    VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    Battle_Ai_planningStatsSnapshot = saved_snapshot;
  }
};

struct SimulationResultsFixture : PlanningFixture {
  std::array<std::byte, 32768> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Battle_Ai_ReactionRecord* saved_workspace = Battle_Ai_planningWorkspace;
  std::array<Battle_Ai_ActionTiming, VS::Battle::Ai::kBattleActorSlotCount>
      rows{};
  Battle_ActionRequest* request = nullptr;
  Battle_Ai_ReactionRecord* workspace = nullptr;
  SimulationResultsFixture() {
    request = static_cast<Battle_ActionRequest*>(
        VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(Battle_ActionRequest)));
    if (request == nullptr) throw std::bad_alloc();
    std::memset(request, 0, sizeof(*request));
    workspace = reinterpret_cast<Battle_Ai_ReactionRecord*>(request);
    Battle_Ai_planningWorkspace = workspace;
    workspace->opaque0 = 42;
    for (std::size_t index = 0; index < rows.size(); ++index) {
      rows[index].opaque0Bit0 = static_cast<unsigned>(index);
      rows[index].reserved0Bit4 = 0xABCDE;
      rows[index].opaque4 = 50;
      rows[index].opaque6 = 100;
      rows[index].opaque8 = 40;
      rows[index].opaqueA = 100;
      rows[index].opaque10 = 30;
      rows[index].opaque12 = 100;
      rows[index].opaque14 = 20;
      rows[index].opaque16 = 100;
      rows[index].opaqueC = 0x80000000;
    }
  }
  ~SimulationResultsFixture() { Battle_Ai_planningWorkspace = saved_workspace; }
};

struct ReadinessFixture : PlanningFixture {
  Battle_Ai_RoomAiState* saved_room = Battle_Ai_roomAiState;
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  Battle_Ai_RoomAiState room{};
  Battle_Actor hostile{};
  ReadinessFixture() {
    Battle_Ai_roomAiState = &room;
    std::fill_n(Battle_actors, std::size(Battle_actors), nullptr);
    room.readinessLevel = 100;
    state.candidateCount = 1;
    for (std::size_t index = 0; index < std::size(state.candidates); ++index) {
      state.candidates[index].row = 2;
      state.candidates[index].column = 1;
      state.candidates[index].action = 42;
      state.candidates[index].threshold = 131;
      state.candidates[index].targetId = 3;
      state.candidates[index].weight = 17;
      state.candidateOrder[index] = &state.candidates[index];
    }
  }
  ~ReadinessFixture() {
    Battle_Ai_roomAiState = saved_room;
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
  }
};

struct EnumerationFixture : ReadinessFixture {
  decltype(Battle_Ai_fallbackCandidates) saved_fallback =
      Battle_Ai_fallbackCandidates;
  int saved_count = Battle_Ai_fallbackCandidateCount;
  int saved_mask = Battle_Ai_unusedPlanCounter;
  unsigned saved_random = Core_Random_State();
  std::array<Battle_Ai_CandidateEntryView, 24> fallback{};
  Battle_Actor battle_actor{};
  Battle_ActorStats target_stats{};

  EnumerationFixture() {
    Battle_Ai_fallbackCandidates = fallback.data();
    Battle_Ai_fallbackCandidateCount = 0;
    Battle_Ai_unusedPlanCounter = 0;
    Core_Random_Seed(1);
    state.candidateCount = 0;
    state.settingsModeLow = 1;
    state.pointHeight = -4321;
    state.relations[1].classification = 2;
    target_stats = stats;
    target_stats.limbActions[2][1].actionId = 0;
    target_stats.limbs[2].maxHp = -7;
    actor.opaque3C = &target_stats;
    battle_actor.id = 1;
    battle_actor.stats = &target_stats;
    Battle_actors[0] = Battle_actors[1] = &battle_actor;
    Battle_actionDefinitions[42].aiFlags =
        std::bit_cast<Battle_ActionAiFlags>(std::uint16_t{0x150});
    auto& slot = state.actionSlots[2][1];
    slot.opaque0Bit7 = 1;
    slot.kind = 1;
    slot.radius = 0x1234;
    slot.rangeSquared = 0xFFFFFFFFu;
  }
  ~EnumerationFixture() {
    Battle_Ai_fallbackCandidates = saved_fallback;
    Battle_Ai_fallbackCandidateCount = saved_count;
    Battle_Ai_unusedPlanCounter = saved_mask;
    Core_Random_Seed(saved_random);
  }
};

struct ActorPlanFixture : EnumerationFixture {
  decltype(Battle_Ai_planningPassCandidates) saved_passes =
      Battle_Ai_planningPassCandidates;
  Battle_Ai_ArenaOrbitState* saved_orbit = Battle_Ai_arenaOrbit;
  Battle_ActorStats* saved_snapshot = Battle_Ai_planningStatsSnapshot;
  int saved_progress = VS::State::LiveScriptFlag(0xA6);
  std::array<std::byte, 65536> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Battle_Ai_CandidateEntryView pass_candidates[3][16]{};
  Battle_Ai_ArenaOrbitState orbit{};

  ActorPlanFixture() {
    VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
    Battle_Ai_planningPassCandidates = pass_candidates;
    Battle_Ai_arenaOrbit = nullptr;
    state.actionSlots[2][1].opaque0Bit7 = 0;
    std::memset(exclusions, 1, sizeof(exclusions));
  }
  ~ActorPlanFixture() {
    VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
    Battle_Ai_planningStatsSnapshot = saved_snapshot;
    Battle_Ai_planningPassCandidates = saved_passes;
    Battle_Ai_arenaOrbit = saved_orbit;
    VS::State::SetLiveScriptFlag(0xA6, saved_progress);
  }
};

struct TargetMemoryFixture {
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  Battle_Ai_ActorAiState state{};
  Battle_Ai_ActionCandidate best{};
  Battle_Ai_ActionCandidate alternate{};
  Actors_ActorRecord model{};
  Actors_ActorRecord alternate_model{};

  explicit TargetMemoryFixture(unsigned target) {
    best.targetId = target;
    alternate.targetId = (target + 1) & 15;
    state.candidateCount = 2;
    state.candidateOrder[0] = &best;
    state.candidateOrder[1] = &alternate;
    state.previousTile = 0x12345678;
    std::memset(state.reserved3C, 0xA5, sizeof(state.reserved3C));
    Actors_actorRecords[target] = &model;
    Actors_actorRecords[alternate.targetId] = &alternate_model;
    const std::array<u_char, 4> tile{0xD4, 0xC3, 0xB2, 0xA1};
    std::memcpy(&model.instance.currentTileX, tile.data(), tile.size());
  }

  ~TargetMemoryFixture() {
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
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
  std::array<Battle_Ai_ActorAiState*, 16> saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Field_gimmickInstances)) saved_objects =
      std::to_array(Field_gimmickInstances);
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  Battle_Ai_ActorAiState state{};
  Battle_Actor actor{};
  Battle_Ai_ActionCandidate first{};
  Battle_Ai_ActionCandidate second{};
  Actors_ActorRecord model{};
  Field_GimmickInstance object{};
  Battle_ActionRequest request{};

  static constexpr int kRememberedTile = 5 << 24;
  static constexpr int kUntouchedTile = 0x12345678;

  CompletedPlanFixture() {
    std::fill_n(Battle_Ai_actorAiStates, 16, nullptr);
    std::fill_n(Field_gimmickInstances, 16, nullptr);
    Battle_Ai_actorAiStates[2] = &state;
    state.actor = reinterpret_cast<Battle_Ai_EngagementData*>(&actor);
    state.previousTile = kUntouchedTile;
    actor.commandOpcode = 24;
    actor.commandArg0 = (1 << 16) | 3;
    first.row = 3;
    first.column = 1;
    first.action = 42;
    first.targetId = 5;
    second = first;
    second.action = 43;
    second.targetId = 6;
    state.candidateCount = 2;
    state.candidateOrder[0] = &first;
    state.candidateOrder[1] = &second;
    Actors_actorRecords[5] = &model;
    request.actionId = 42;
    request.primaryTarget.target.targetActor = 2;
    request.targetCount = 1;
    request.targets[0].target.targetActor = 4;
    Field_gimmickInstances[4] = &object;
  }

  ~CompletedPlanFixture() {
    std::copy(saved_states.begin(), saved_states.end(),
              Battle_Ai_actorAiStates);
    std::copy(saved_objects.begin(), saved_objects.end(),
              Field_gimmickInstances);
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
  }

  void Reconcile() {
    Battle_Ai_ReconcileCompletedActionPlanEntry(
        reinterpret_cast<Battle_Ai_ReactionRecord*>(&request));
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
  std::memcpy(expected.data() + offsetof(Battle_Ai_ActorAiState, previousTile),
              &origin, sizeof(origin));
  auto* memory = expected.data() + offsetof(Battle_Ai_ActorAiState, reserved3C);
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
      std::bit_cast<std::array<std::byte, sizeof(Battle_ActorStats)>>(
          fixture.first_stats);
  const auto refused = VS::Core::Memory::RefusedLiveHeapRequestCount();
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  auto* storage = Battle_Ai_planningStatsSnapshot;
  REQUIRE(storage != nullptr);
  CHECK(VS::Core::Memory::LivePayloadCapacity(storage).value() >=
        VS::Battle::Ai::kPlanningStatsTableBytes);
  std::memset(&fixture.first_stats, 0x75, sizeof(fixture.first_stats));
  fixture.second_stats.currentHp = 1;
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  const auto restored =
      std::bit_cast<std::array<std::byte, sizeof(Battle_ActorStats)>>(
          fixture.first_stats);
  for (std::size_t offset = 0; offset < restored.size(); ++offset) {
    CHECK(restored[offset] ==
          (CopiedStatByte(offset) ? original[offset] : std::byte{0x75}));
  }
  CHECK(fixture.second_stats.currentHp == 50);
  CHECK(Battle_Ai_planningStatsSnapshot == nullptr);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(storage).has_value());
  CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == refused);
}

TEST_CASE(
    "Native fallback selection refreshes all slots and preserves packed fields",
    "[ai][planning][fallback-selection-live]") {
  const bool ignores_sight = GENERATE(false, true);
  PlanningFixture fixture;
  using Words = VS::Battle::Ai::ActionCandidate::Words;
  fixture.state.currentChoice = std::bit_cast<Battle_Ai_ActionCandidate>(
      Words{0xDEADBEEF, 0xFFFFFFFF, 0xABCDEF01, 0xFFFFFFFF, 0xFFFFFFFF});
  Battle_actionDefinitions[42].aiFlags.opaque1Bit4 = ignores_sight ? 1u : 0u;
  for (int row = 0; row < 6; ++row) {
    fixture.stats.limbs[row].hp = 2;
    for (int column = 0; column < 4; ++column) {
      fixture.stats.limbActions[row][column].actionId = 42;
      fixture.state.actionSlots[row][column].special = 1;
      fixture.state.actionSlots[row][column].rangeSquared =
          static_cast<unsigned>(row * 4 + column + 1);
    }
  }
  auto expected = fixture.state.currentChoice;
  expected.row = 5;
  expected.column = 3;
  expected.action = 42;
  expected.distanceSquared = 24;
  expected.weight = 128;
  expected.opaque10.opaque0Bit0 = 0;
  expected.opaque10.opaque0Bit1 = 0;
  expected.opaque10.opaque0Bit2 = ignores_sight ? 1u : 0u;
  expected.opaque10.opaque0Bit3 = 0;
  VS::Battle::Ai::SelectLiveFallbackAction(&fixture.state);
  CHECK(std::memcmp(&fixture.state.currentChoice, &expected,
                    sizeof(expected)) == 0);
  for (const auto& row : fixture.state.actionSlots) {
    for (const auto& slot : row) CHECK(slot.opaque0Bit7 == 1);
  }
}

TEST_CASE(
    "Native fallback selection clears only the action when no slot qualifies",
    "[ai][planning][fallback-selection-live]") {
  PlanningFixture fixture;
  using Words = VS::Battle::Ai::ActionCandidate::Words;
  fixture.state.currentChoice = std::bit_cast<Battle_Ai_ActionCandidate>(
      Words{0xDEADBEEF, 0xFFFFFFFF, 0xABCDEF01, 0xFFFFFFFF, 0xFFFFFFFF});
  for (auto& row : fixture.state.actionSlots) {
    for (auto& slot : row) slot.opaque0Bit7 = 1;
  }
  for (auto& row : fixture.stats.limbActions) {
    for (auto& slot : row) slot.actionId = 42;
  }
  auto expected = fixture.state.currentChoice;
  expected.action = 0;
  VS::Battle::Ai::SelectLiveFallbackAction(&fixture.state);
  CHECK(std::memcmp(&fixture.state.currentChoice, &expected,
                    sizeof(expected)) == 0);
  for (int row = 0; row < 6; ++row) {
    for (int column = 0; column < 4; ++column) {
      CHECK(fixture.state.actionSlots[row][column].opaque0Bit7 ==
            static_cast<unsigned>(row == 2));
    }
  }
}

TEST_CASE(
    "Native fallback selection keeps selection and usability stats distinct",
    "[ai][planning][fallback-selection-live]") {
  PlanningFixture fixture;
  Battle_ActorStats selection_stats = fixture.stats;
  fixture.state.stats = &selection_stats;
  selection_stats.limbActions[2][1].actionId = 0;
  fixture.state.actionSlots[2][1].special = 1;
  fixture.state.actionSlots[2][1].rangeSquared = 9;
  VS::Battle::Ai::SelectLiveFallbackAction(&fixture.state);
  CHECK(fixture.state.currentChoice.action == 0);
  CHECK(fixture.state.currentChoice.row == 2);
  CHECK(fixture.state.currentChoice.column == 1);
  CHECK(fixture.state.currentChoice.distanceSquared == 9);
  CHECK(fixture.state.currentChoice.weight == 128);
  CHECK(fixture.state.currentChoice.opaque10.opaque0Bit2 ==
        Battle_actionDefinitions[0].aiFlags.opaque1Bit4);
  CHECK(fixture.state.actionSlots[2][1].opaque0Bit7 == 1);
}

TEST_CASE(
    "Native fallback usability reads actor stats rather than selection stats",
    "[ai][planning][fallback-selection-live]") {
  PlanningFixture fixture;
  Battle_ActorStats selection_stats = fixture.stats;
  fixture.state.stats = &selection_stats;
  fixture.stats.limbs[2].hp = 1;
  fixture.state.actionSlots[2][1].special = 1;
  fixture.state.actionSlots[2][1].rangeSquared = 11;
  fixture.state.currentChoice.action = 42;
  fixture.state.currentChoice.distanceSquared = 9;
  VS::Battle::Ai::SelectLiveFallbackAction(&fixture.state);
  CHECK(fixture.state.currentChoice.action == 0);
  CHECK(fixture.state.currentChoice.distanceSquared == 9);
  CHECK(fixture.state.actionSlots[2][1].opaque0Bit7 == 0);
}

TEST_CASE("Native planning simulation updates only the original stat fields",
          "[ai][planning][simulation-advance-live]") {
  PlanningAdvanceFixture fixture;
  fixture.room.readinessLevel = 2880;
  fixture.stats.statuses = 0x11;
  Battle_Ai_kTimedStatusFlags[0] = 1;
  for (auto& limb : fixture.stats.limbs) {
    limb.hp = 0;
    limb.maxHp = 10;
  }
  fixture.state.resourceStatusRestricted = 1;
  fixture.state.lowMp = fixture.state.lowHp = 1;
  auto expected = fixture.stats;
  expected.statuses = 0;
  for (auto& limb : expected.limbs) limb.hp = 2;
  VS::Battle::Ai::AdvanceLivePlanningSimulation(1);
  CHECK(fixture.room.readinessLevel == 2881);
  CHECK(std::memcmp(&fixture.stats, &expected, sizeof(expected)) == 0);
  CHECK(fixture.state.resourceStatusRestricted == 0);
  CHECK(fixture.state.lowMp == 0);
  CHECK(fixture.state.lowHp == 0);
  for (const auto& row : fixture.state.actionSlots) {
    for (const auto& slot : row) CHECK(slot.opaque0Bit7 == 1);
  }
}

TEST_CASE(
    "Native planning simulation preserves distinct callback stats sources",
    "[ai][planning][simulation-advance-live]") {
  PlanningAdvanceFixture fixture;
  Battle_ActorStats simulated = fixture.stats;
  simulated.currentHp = 0;
  fixture.battle_actor.stats = &simulated;
  fixture.state.stats = nullptr;
  for (auto& limb : fixture.stats.limbs) limb.hp = 0;
  for (auto& row : fixture.state.actionSlots) {
    for (auto& slot : row) slot.opaque0Bit7 = 1;
  }
  VS::Battle::Ai::AdvanceLivePlanningSimulation(480);
  CHECK(simulated.currentHp == 1);
  CHECK(fixture.stats.currentHp == 100);
  CHECK(fixture.state.lowHp == 0);
  for (const auto& row : fixture.state.actionSlots) {
    for (const auto& slot : row) CHECK(slot.opaque0Bit7 == 0);
  }
}

TEST_CASE("Native planning simulation advances an empty actor list",
          "[ai][planning][simulation-advance-live]") {
  PlanningAdvanceFixture fixture;
  Battle_actors[0] = nullptr;
  fixture.room.readinessLevel = 65535;
  Battle_Ai_AdvancePlanningSimulationEntry(1);
  CHECK(fixture.room.readinessLevel == 0);
}

TEST_CASE("Native planning simulation refreshes each actor by its own slot",
          "[ai][planning][simulation-advance-live]") {
  PlanningAdvanceFixture fixture;
  Battle_ActorStats second_stats = fixture.stats;
  Battle_Actor second{};
  Battle_Ai_EngagementData second_actor{};
  second.id = 3;
  second.stats = &second_stats;
  second_actor.opaque3C = &second_stats;
  fixture.peer.actor = &second_actor;
  fixture.battle_actor.next = &second;
  fixture.stats.currentHp = 0;
  for (auto& limb : fixture.stats.limbs) limb.hp = 0;
  for (auto& limb : second_stats.limbs) limb.hp = 2;
  VS::Battle::Ai::AdvanceLivePlanningSimulation(480);
  CHECK(fixture.stats.currentHp == 1);
  CHECK(second_stats.currentHp == 100);
  CHECK(fixture.state.lowHp == 1);
  CHECK(fixture.peer.lowHp == 0);
  for (int row = 0; row < 6; ++row) {
    for (int column = 0; column < 4; ++column) {
      CHECK(fixture.state.actionSlots[row][column].opaque0Bit7 == 0);
      CHECK(fixture.peer.actionSlots[row][column].opaque0Bit7 == 1);
    }
  }
}

TEST_CASE("Planning simulation retains its original module address",
          "[ai][planning][simulation-advance-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800DE030) ==
      reinterpret_cast<const void*>(&Battle_Ai_AdvancePlanningSimulationEntry));
}

TEST_CASE("Fallback selection retains its original module address",
          "[ai][planning][fallback-selection-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DD604) ==
        reinterpret_cast<const void*>(&Battle_Ai_SelectFallbackActionEntry));
}

TEST_CASE(
    "Native planning stats refuse reentry and allow a fresh capture after "
    "release",
    "[ai][planning][stats-lifecycle-live]") {
  PlanningStatsFixture fixture;
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  auto* storage = Battle_Ai_planningStatsSnapshot;
  fixture.first_stats.currentHp = 7;
  CHECK_FALSE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  CHECK(Battle_Ai_planningStatsSnapshot == storage);
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  CHECK(fixture.first_stats.currentHp == 100);
  fixture.first_stats.currentHp = 80;
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  fixture.first_stats.currentHp = 3;
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  CHECK(fixture.first_stats.currentHp == 80);
  CHECK(Battle_Ai_planningStatsSnapshot == nullptr);
}

TEST_CASE("Native planning stats handle empty and shortened actor lists",
          "[ai][planning][stats-lifecycle-live]") {
  PlanningStatsFixture fixture;
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  fixture.first.next = nullptr;
  fixture.first_stats.currentHp = fixture.second_stats.currentHp = 1;
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  CHECK(fixture.first_stats.currentHp == 100);
  CHECK(fixture.second_stats.currentHp == 1);
  Battle_actors[0] = nullptr;
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  CHECK(Battle_Ai_planningStatsSnapshot == nullptr);
}

TEST_CASE(
    "Native planning stats refuse allocation failure before reading actors",
    "[ai][planning][stats-lifecycle-live]") {
  PlanningStatsFixture fixture;
  std::array<std::byte, 256> tiny{};
  const VS::Core::Memory::ScopedLiveGameHeap heap(tiny);
  Battle_actors[0] = reinterpret_cast<Battle_Actor*>(std::uintptr_t{1});
  CHECK_FALSE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  CHECK(Battle_Ai_planningStatsSnapshot == nullptr);
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
    fixture.second.stats = static_cast<Battle_ActorStats*>(next_storage);
  }
  const auto refused = VS::Core::Memory::RefusedLiveHeapRequestCount();
  CHECK_FALSE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  CHECK(Battle_Ai_planningStatsSnapshot == nullptr);
  CHECK(fixture.first_stats.currentHp == 100);
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
  Battle_Ai_ActionCandidate candidate{};
  candidate.targetId = 3;
  candidate.action = 42;
  candidate.threshold = 54321;
  candidate.opaque0 = -123456;
  fixture.workspace->opaque4A = 4;
  fixture.workspace->opaque4C[0].opaque0 = 3;
  fixture.workspace->opaque4C[0].opaque40 = 1;
  fixture.workspace->opaque4C[0].opaque2 = 128;
  fixture.workspace->opaque4C[1].opaque0 = 2;
  fixture.workspace->opaque4C[1].opaque2 = 128;
  fixture.workspace->opaque4C[2].opaque0 = 3;
  fixture.workspace->opaque4C[3].opaque0 = 3;
  fixture.workspace->opaque4C[3].opaque2 = 99;
  for (unsigned threshold = 0; threshold < 256; ++threshold) {
    fixture.workspace->opaque4C[2].opaque2 =
        static_cast<unsigned char>(threshold);
    auto expected = candidate;
    expected.weight = (threshold + threshold / 4 + threshold / 32) & 255;
    REQUIRE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(&candidate));
    CHECK(std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(candidate) ==
          std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(expected));
  }
  fixture.workspace->opaque4A = 0;
  Battle_Ai_UpdateCandidateHitWeightEntry(&candidate);
  CHECK(candidate.weight == 0);
}

TEST_CASE(
    "Native candidate hit weighting refuses unreadable or oversized tails",
    "[ai][planning][candidate-evaluation-live]") {
  SimulationResultsFixture fixture;
  Battle_Ai_ActionCandidate candidate{};
  candidate.weight = 99;
  const auto before =
      std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(candidate);
  CHECK_FALSE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(nullptr));
  fixture.workspace->opaque4A = 256;
  CHECK_FALSE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(&candidate));
  auto* short_workspace = static_cast<Battle_Ai_ReactionRecord*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(
          sizeof(Battle_Ai_ReactionRecord)));
  REQUIRE(short_workspace != nullptr);
  std::memset(short_workspace, 0, sizeof(*short_workspace));
  short_workspace->opaque4A = 1;
  Battle_Ai_planningWorkspace = short_workspace;
  CHECK_FALSE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(&candidate));
  Battle_Ai_planningWorkspace = nullptr;
  CHECK_FALSE(VS::Battle::Ai::UpdateLiveCandidateHitWeight(&candidate));
  CHECK(std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(candidate) ==
        before);
}

TEST_CASE(
    "Native candidate evaluation owns its snapshot and preserves packed fields",
    "[ai][planning][candidate-evaluation-live]") {
  ReadinessFixture fixture;
  Battle_Actor ally{};
  Battle_Actor enemy{};
  Battle_ActorStats enemy_stats = fixture.stats;
  enemy_stats.currentHp = 50;
  ally.id = 1;
  ally.stats = &fixture.stats;
  ally.next = &enemy;
  enemy.id = 3;
  enemy.stats = &enemy_stats;
  Battle_actors[0] = &ally;
  fixture.state.relations[1].classification = 2;
  fixture.state.relations[3].classification = 3;
  Battle_Ai_ActionCandidate candidate = fixture.state.candidates[0];
  auto expected = candidate;
  expected.opaque0 = 256 * 128 + 128;
  expected.weight = 128;
  std::array<std::byte, 16 * sizeof(Battle_Ai_ActionTiming)> scratch{};
  auto* scratch_rows = static_cast<std::byte*>(Core_ScratchBase()) + 0x18C;
  std::memcpy(scratch.data(), scratch_rows, scratch.size());
  REQUIRE(VS::Battle::Ai::EvaluateLiveActionCandidate(&fixture.state,
                                                      &candidate, 0, nullptr));
  CHECK(std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(candidate) ==
        std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(expected));
  CHECK(std::memcmp(scratch.data(), scratch_rows, scratch.size()) == 0);
  std::memcpy(scratch_rows, scratch.data(), scratch.size());
  fixture.state.settingsModeLow = 1;
  Battle_actionDefinitions[42].aiFlags.opaque0Bit4 = 1;
  Battle_Ai_EvaluateActionCandidateEntry(&fixture.state, &candidate, 3,
                                         &Battle_actionDefinitions[42]);
  CHECK(candidate.opaque0 == 257 * 128 + 40);
  CHECK(candidate.weight == 40);
}

TEST_CASE("Native candidate bindings refuse missing state before simulation",
          "[ai][planning][candidate-evaluation-live]") {
  SimulationResultsFixture fixture;
  Battle_Ai_ActionCandidate candidate{};
  CHECK_FALSE(VS::Battle::Ai::EvaluateLiveActionCandidate(nullptr, &candidate,
                                                          0, nullptr));
  CHECK_FALSE(VS::Battle::Ai::EvaluateLiveActionCandidate(&fixture.state,
                                                          nullptr, 0, nullptr));
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      nullptr, fixture.rows, &candidate,
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
  fixture.state.actorId = 255;
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      &fixture.state, fixture.rows, &candidate,
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
}

TEST_CASE("Native candidate simulation refuses aliased inputs before writes",
          "[ai][planning][candidate-evaluation-live]") {
  SimulationResultsFixture fixture;
  Battle_Ai_ActionCandidate candidate{};
  const auto before = *fixture.request;
  const auto rows_before = fixture.rows;
  const std::span<Battle_Ai_ActionTiming, VS::Battle::Ai::kBattleActorSlotCount>
      aliased_rows(reinterpret_cast<Battle_Ai_ActionTiming*>(fixture.workspace),
                   VS::Battle::Ai::kBattleActorSlotCount);
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      &fixture.state, aliased_rows, &candidate,
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      reinterpret_cast<Battle_Ai_ActorAiState*>(fixture.workspace),
      fixture.rows, &candidate,
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
  CHECK_FALSE(VS::Battle::Ai::SimulateLiveActionCandidate(
      &fixture.state, fixture.rows,
      reinterpret_cast<Battle_Ai_ActionCandidate*>(fixture.workspace),
      VS::Battle::Ai::CandidateSimulationMode::kLimb));
  CHECK(std::memcmp(&before, fixture.request, sizeof(before)) == 0);
  CHECK(std::memcmp(rows_before.data(), fixture.rows.data(),
                    sizeof(rows_before)) == 0);
}

TEST_CASE("Candidate planning functions retain all original module addresses",
          "[ai][planning][candidate-evaluation-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DCAA0) ==
        reinterpret_cast<const void*>(&Battle_Ai_SimulateActionCandidateEntry));
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800DCBD8) ==
      reinterpret_cast<const void*>(&Battle_Ai_UpdateCandidateHitWeightEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DCC94) ==
        reinterpret_cast<const void*>(&Battle_Ai_EvaluateActionCandidateEntry));
}

TEST_CASE(
    "Native enumeration preserves special-action fields and distinct stats "
    "sources",
    "[ai][planning][candidate-enumeration-live]") {
  EnumerationFixture fixture;
  const auto kind = GENERATE(1u, 2u, 3u);
  fixture.state.actionSlots[2][1].kind = kind;
  Battle_Ai_EnumerateActionCandidatesEntry(&fixture.state, -1);
  REQUIRE(fixture.state.candidateCount == 1);
  const auto& selected = fixture.state.candidates[0];
  CHECK(selected.row == 2);
  CHECK(selected.column == 1);
  CHECK(selected.action == 42);
  CHECK(selected.targetId == 1);
  CHECK(selected.threshold == 0xFFFF);
  CHECK(selected.weight == 40);
  CHECK(selected.targetLimb == (kind == 2 ? 2 : 0));
  CHECK(selected.opaqueCBit7 == (kind == 3 ? 225 : 0));
}

TEST_CASE("Native enumeration caches and ranks without its old scratch records",
          "[ai][planning][candidate-enumeration-live]") {
  EnumerationFixture fixture;
  fixture.state.actionSlots[2][1].kind = GENERATE(1u, 2u, 3u);
  std::array<std::byte, 4 * sizeof(Battle_Ai_ActionCandidate)> scratch{};
  const auto* scratch_rows =
      static_cast<const std::byte*>(Core_ScratchBase()) + 0x30C;
  std::memcpy(scratch.data(), scratch_rows, scratch.size());
  Battle_Ai_EnumerateActionCandidatesEntry(&fixture.state, -1);
  REQUIRE(fixture.state.candidateCount == 1);
  REQUIRE(Battle_Ai_fallbackCandidateCount == 1);
  const auto& selected = fixture.state.candidates[0];
  CHECK(fixture.state.candidateOrder[0] == &selected);
  CHECK(fixture.exclusions[1][42] == 1);
  CHECK(Battle_Ai_unusedPlanCounter == (1 << 9));
  CHECK(std::memcmp(&fixture.fallback[0], &selected, sizeof(selected)) == 0);
  CHECK(std::memcmp(scratch.data(), scratch_rows, scratch.size()) == 0);
}

TEST_CASE("Native enumeration rereads exclusions on repeated calls",
          "[ai][planning][candidate-enumeration-live]") {
  EnumerationFixture fixture;
  fixture.state.actionSlots[2][1].kind = GENERATE(1u, 2u, 3u);
  Battle_Ai_EnumerateActionCandidatesEntry(&fixture.state, -1);
  REQUIRE(fixture.state.candidateCount == 1);
  REQUIRE(Battle_Ai_fallbackCandidateCount == 1);
  REQUIRE_NOTHROW(
      VS::Battle::Ai::EnumerateLiveActionCandidates(&fixture.state, 0));
  CHECK(fixture.state.candidateCount == 1);
  CHECK(Battle_Ai_fallbackCandidateCount == 1);
}

TEST_CASE(
    "Native enumeration leaves disabled slots and existing lists untouched",
    "[ai][planning][candidate-enumeration-live]") {
  EnumerationFixture fixture;
  fixture.state.actionSlots[2][1].opaque0Bit7 = 0;
  fixture.state.candidateCount = 3;
  Battle_Ai_fallbackCandidateCount = 7;
  Battle_Ai_candidateExclusions = nullptr;
  Battle_Ai_fallbackCandidates = nullptr;
  using StateBytes = std::array<std::byte, sizeof(fixture.state)>;
  const auto before = std::bit_cast<StateBytes>(fixture.state);
  REQUIRE_NOTHROW(
      VS::Battle::Ai::EnumerateLiveActionCandidates(&fixture.state, 70000));
  CHECK(std::bit_cast<StateBytes>(fixture.state) == before);
  CHECK(Battle_Ai_fallbackCandidateCount == 7);
  CHECK(Battle_Ai_unusedPlanCounter == 0);
  CHECK(Core_Random_State() == 1);
}

TEST_CASE("Native enumeration retains its original module address",
          "[ai][planning][candidate-enumeration-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DD918) ==
        reinterpret_cast<void*>(&Battle_Ai_EnumerateActionCandidatesEntry));
}

TEST_CASE("Completed action reconciliation ignores an absent performer state",
          "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  Battle_Ai_actorAiStates[2] = nullptr;
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
  Battle_Ai_ReconcileCompletedActionPlanEntry(nullptr);
  CHECK(fixture.Bytes() == expected);
  fixture.request.primaryTarget.targetsPosition = 1;
  fixture.Reconcile();
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("Completed action reconciliation leaves non-action commands",
          "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  fixture.actor.commandOpcode = 23;
  const auto expected = fixture.Bytes();
  fixture.Reconcile();
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE(
    "Completed action reconciliation records the detour and remembers its "
    "target",
    "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  fixture.object.objectInfo.objectId = GENERATE(0, 1, 3, 5, 6, 7);
  fixture.Reconcile();
  CHECK(fixture.state.opaqueFB == 5);
  CHECK(fixture.state.previousTile == CompletedPlanFixture::kRememberedTile);
}

TEST_CASE(
    "Completed action reconciliation forgets for absent and kind 2 or 4 "
    "objects",
    "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  const int kind = GENERATE(-1, 2, 4);
  if (kind < 0) {
    Field_gimmickInstances[4] = nullptr;
  } else {
    fixture.object.objectInfo.objectId = kind;
  }
  fixture.Reconcile();
  CHECK(fixture.state.opaqueFB == 5);
  CHECK(fixture.state.previousTile == -1);
}

TEST_CASE("Completed action reconciliation skips position targets",
          "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  fixture.request.targets[0].target.targetActor = 200;
  fixture.request.targets[0].targetsPosition = 1;
  fixture.Reconcile();
  CHECK(fixture.state.opaqueFB == 5);
  CHECK(fixture.state.previousTile == CompletedPlanFixture::kRememberedTile);
}

TEST_CASE(
    "Completed action reconciliation compares the full action and command "
    "halves",
    "[ai][planning][completed-plan-live]") {
  CompletedPlanFixture fixture;
  SECTION("an action ID past the candidate field") {
    fixture.request.actionId = 42 + 256;
  }
  SECTION("another column") { fixture.actor.commandArg0 = 3; }
  SECTION("a wider row half") { fixture.actor.commandArg0 = (1 << 16) | 11; }
  Field_gimmickInstances[4] = nullptr;
  fixture.Reconcile();
  CHECK(fixture.state.opaqueFB == 0);
  CHECK(fixture.state.previousTile == CompletedPlanFixture::kRememberedTile);
}

TEST_CASE("Completed action reconciliation retains its original module address",
          "[ai][planning][completed-plan-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DEB10) ==
        reinterpret_cast<const void*>(
            &Battle_Ai_ReconcileCompletedActionPlanEntry));
}

TEST_CASE("Native actor plan retains its original module address",
          "[ai][planning][actor-plan-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DE3E4) ==
        reinterpret_cast<const void*>(&Battle_Ai_BuildActorActionPlanEntry));
}

TEST_CASE(
    "Native actor plan clears configured exclusions before the zero actor exit",
    "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  fixture.state.actorId = 0;
  fixture.state.candidateCount = 5;
  Battle_Ai_fallbackCandidateCount = 7;
  Battle_Ai_unusedPlanCounter = -1;
  VS::Battle::Ai::BuildLiveActorActionPlan(&fixture.state);
  CHECK(fixture.state.candidateCount == 0);
  CHECK(Battle_Ai_fallbackCandidateCount == 0);
  CHECK(Battle_Ai_unusedPlanCounter == 0);
  CHECK(fixture.exclusions[1][0] == 0);
  CHECK(fixture.exclusions[1][42] == 0);
  CHECK(fixture.exclusions[1][43] == 1);
  CHECK(fixture.exclusions[3][42] == 1);
  CHECK(fixture.room.readinessLevel == 100);
}

TEST_CASE(
    "Native actor plan restores projected stats and releases its snapshot",
    "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  fixture.stats.limbActions[2][1].actionId = 0;
  using StatsBytes = std::array<std::byte, sizeof(fixture.target_stats)>;
  const auto before = std::bit_cast<StatsBytes>(fixture.target_stats);
  VS::Battle::Ai::BuildLiveActorActionPlan(&fixture.state);
  CHECK(std::bit_cast<StatsBytes>(fixture.target_stats) == before);
  CHECK(Battle_Ai_planningStatsSnapshot == nullptr);
  CHECK(fixture.room.readinessLevel == 0);
  CHECK(fixture.state.candidateCount == 0);
  CHECK(fixture.state.planMode == 7);
  CHECK(Core_Random_State() == 1);
}

TEST_CASE(
    "Native actor plan retains the no-snapshot path without changing the clock",
    "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  auto* reserved = VS::Core::Memory::AllocateLiveFromHighEnd(60000);
  REQUIRE(reserved != nullptr);
  VS::Battle::Ai::BuildLiveActorActionPlan(&fixture.state);
  CHECK(Battle_Ai_planningStatsSnapshot == nullptr);
  CHECK(fixture.room.readinessLevel == 100);
  CHECK(fixture.state.candidateCount == 0);
  CHECK(fixture.state.planMode == 7);
  CHECK(Core_Random_State() == 1);
  VS::Core::Memory::FreeLiveFromHighEnd(reserved);
}

TEST_CASE(
    "Native actor plan uses real pass storage ranking and final readiness",
    "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  const auto jitter = Core_Random_Rand() & 7;
  const auto expected_random = Core_Random_State();
  Core_Random_Seed(1);
  Battle_Ai_BuildActorActionPlanEntry(&fixture.state);
  REQUIRE(fixture.state.candidateCount == 1);
  CHECK(fixture.state.candidateOrder[0] == &fixture.state.candidates[0]);
  CHECK(fixture.state.candidates[0].action == 42);
  CHECK(fixture.state.candidates[0].threshold == jitter);
  CHECK(fixture.exclusions[1][42] == 1);
  CHECK(Core_Random_State() == expected_random);
  const auto saved = std::bit_cast<VS::Battle::Ai::ActionCandidate::Words>(
      fixture.pass_candidates[0][0]);
  CHECK(VS::Battle::Ai::ActionCandidate(saved).Action() == 42);
}

TEST_CASE("Native actor plan reads the full arena command argument",
          "[ai][planning][actor-plan-live]") {
  ActorPlanFixture fixture;
  fixture.stats.limbActions[2][1].actionId = 0;
  const int argument = GENERATE(4, 0x10004);
  Battle_Ai_arenaOrbit = &fixture.orbit;
  fixture.orbit.planSaved = 1;
  fixture.orbit.savedPlanCount = 9;
  fixture.battle_actor.commandOpcode = 24;
  fixture.battle_actor.commandArg0 = argument;
  fixture.battle_actor.subType = 173;
  fixture.state.actor =
      reinterpret_cast<Battle_Ai_EngagementData*>(&fixture.battle_actor);
  VS::State::SetLiveScriptFlag(0xA6, 4);
  VS::Battle::Ai::BuildLiveActorActionPlan(&fixture.state);
  CHECK(VS::State::LiveScriptFlag(0xA6) == (argument == 4 ? 5 : 4));
  CHECK(fixture.orbit.savedPlanCount == 0);
  CHECK(fixture.state.candidateCount == 0);
}

TEST_CASE(
    "Native simulated results preserve row metadata maxima and target order",
    "[ai][planning][simulation-results-live]") {
  SimulationResultsFixture fixture;
  Battle_actionDefinitions[42].type = 3;
  Battle_actionDefinitions[42].cost = 55;
  fixture.workspace->opaque4.opaque4 = 7;
  fixture.workspace->opaque4.opaque1CBit0 = 2;
  fixture.workspace->opaque4.opaque14 = 3;
  fixture.workspace->opaque4.opaque18 = 1;
  fixture.workspace->opaque4A = 2;
  fixture.workspace->opaque4C[0].opaque0 = 0;
  fixture.workspace->opaque4C[0].opaque4 = 30;
  fixture.workspace->opaque4C[0].opaque1CBit0 = 2;
  fixture.workspace->opaque4C[1].opaque0 = 0;
  fixture.workspace->opaque4C[1].opaque4 = 10;
  fixture.workspace->opaque4C[1].opaque18 = 2;
  auto expected = fixture.rows;
  expected[0].opaque4 = 27;
  expected[0].opaqueC = 0x80000000;
  using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
  REQUIRE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  CHECK((std::bit_cast<RowBytes>(fixture.rows) ==
         std::bit_cast<RowBytes>(expected)));
}

TEST_CASE("Native simulated results decode all signed resource operations",
          "[ai][planning][simulation-results-live]") {
  for (const unsigned type : {1u, 3u, 6u}) {
    SimulationResultsFixture fixture;
    Battle_actionDefinitions[42].type = type;
    Battle_actionDefinitions[42].cost = 5;
    auto& performer = fixture.workspace->opaque4;
    performer.opaque4 = 7;
    performer.opaque1CBit0 = 2;
    performer.opaque6 = -9;
    performer.opaque38 = 9;
    performer.opaque1EBit2 = 2;
    performer.opaque3C = -8;
    performer.opaque1EBit6 = 3;
    performer.opaque14 = 5;
    performer.opaque18 = 1;
    fixture.workspace->opaque4A = 1;
    auto& target = fixture.workspace->opaque4C[0];
    target.opaque0 = 1;
    target.opaque6 = INT16_MAX;
    target.opaque38 = 5;
    target.opaque1EBit2 = 2;
    target.opaque3C = INT16_MIN;
    auto expected = fixture.rows;
    expected[0].opaque4 = type == 3 ? 52 : 57;
    expected[0].opaque8 = type == 1 ? 44 : 49;
    expected[0].opaque10 = type == 6 ? 34 : 39;
    expected[0].opaque14 = 28;
    expected[0].opaqueC = 0x80000004;
    expected[1].opaque8 = 0;
    expected[1].opaque10 = 35;
    expected[1].opaque14 = 100;
    using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
    REQUIRE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
    CHECK((std::bit_cast<RowBytes>(fixture.rows) ==
           std::bit_cast<RowBytes>(expected)));
  }
}

TEST_CASE("Native simulated results accept the complete native target capacity",
          "[ai][planning][simulation-results-live]") {
  SimulationResultsFixture fixture;
  fixture.workspace->opaque4.opaque40 = 1;
  fixture.workspace->opaque4A =
      static_cast<u_short>(std::size(fixture.request->targets));
  using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
  const auto before = std::bit_cast<RowBytes>(fixture.rows);
  REQUIRE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  CHECK((std::bit_cast<RowBytes>(fixture.rows) == before));
}

TEST_CASE(
    "Native simulated results ignore inactive records before action lookup",
    "[ai][planning][simulation-results-live]") {
  SimulationResultsFixture fixture;
  fixture.workspace->opaque0 = UINT16_MAX;
  fixture.workspace->opaque4.opaque40 = -1;
  fixture.workspace->opaque4.opaque0 = 255;
  fixture.workspace->opaque4A = 1;
  fixture.workspace->opaque4C[0].opaque40 = 1;
  fixture.workspace->opaque4C[0].opaque0 = 255;
  using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
  const auto before = std::bit_cast<RowBytes>(fixture.rows);
  Battle_Ai_ApplySimulatedActionResultsEntry(fixture.rows.data());
  CHECK((std::bit_cast<RowBytes>(fixture.rows) == before));
}

TEST_CASE("Simulated results retain their original module address",
          "[ai][planning][simulation-results-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DC888) ==
        reinterpret_cast<const void*>(
            &Battle_Ai_ApplySimulatedActionResultsEntry));
}

TEST_CASE("Native simulated results refuse short tails and invalid actor slots",
          "[ai][planning][simulation-results-live]") {
  SimulationResultsFixture fixture;
  using RowBytes = std::array<std::byte, sizeof(fixture.rows)>;
  const auto before = std::bit_cast<RowBytes>(fixture.rows);
  fixture.workspace->opaque4A = 256;
  CHECK_FALSE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  fixture.workspace->opaque4A = 0;
  fixture.workspace->opaque4.opaque0 = 16;
  CHECK_FALSE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  auto* short_workspace = static_cast<Battle_Ai_ReactionRecord*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(
          sizeof(Battle_Ai_ReactionRecord)));
  REQUIRE(short_workspace != nullptr);
  std::memset(short_workspace, 0, sizeof(*short_workspace));
  short_workspace->opaque4A = 1;
  Battle_Ai_planningWorkspace = short_workspace;
  CHECK_FALSE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  Battle_Ai_planningWorkspace = nullptr;
  CHECK_FALSE(VS::Battle::Ai::ApplyLiveSimulatedActionResults(fixture.rows));
  CHECK((std::bit_cast<RowBytes>(fixture.rows) == before));
}

TEST_CASE("Native condition status score reads the row's own AI state",
          "[ai][planning][condition-terms-live]") {
  PlanningFixture fixture;
  std::array<Battle_Ai_ActionTiming, VS::Battle::Ai::kBattleActorSlotCount>
      rows{};
  // Row 7 belongs to the fixture's peer in slot 3, so slot 1 is not consulted.
  Battle_Ai_ActionTiming& row = rows[7];
  row.opaque0Bit0 = 3;
  row.opaque4 = 100;
  row.opaque6 = 100;
  row.opaqueC = (1u << 6) | (1u << 12);
  fixture.state.actionCostEnabled = 1;
  fixture.peer.actionCostEnabled = 0;
  CHECK(VS::Battle::Ai::LiveActorStatusConditionScore(&row) == 50);
  fixture.peer.actionCostEnabled = 1;
  CHECK(VS::Battle::Ai::LiveActorStatusConditionScore(&row) == 50 - 51);
  fixture.peer.actionCostEnabled = 2;
  row.opaque4 = 0;
  CHECK(VS::Battle::Ai::LiveActorStatusConditionScore(&row) ==
        50 - 51 - 128 - 84);
}

TEST_CASE("Native condition ratios read each term from the requested row",
          "[ai][planning][condition-terms-live]") {
  std::array<Battle_Ai_ActionTiming, VS::Battle::Ai::kBattleActorSlotCount>
      rows{};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    auto& row = rows[index];
    row.opaque0Bit0 = static_cast<u_int>(rows.size() - 1 - index);
    row.opaque4 = static_cast<std::int16_t>(2 * index);
    row.opaque6 = 128;
    row.opaque8 = static_cast<std::int16_t>(3 * index);
    row.opaqueA = 128;
    row.opaque10 = static_cast<std::int16_t>(5 * index);
    row.opaque12 = 128;
    row.opaque14 = static_cast<std::int16_t>(7 * index);
    row.opaque16 = 128;
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
  std::array<Battle_Ai_ActionTiming, VS::Battle::Ai::kBattleActorSlotCount>
      rows{};
  Battle_Ai_ActionTiming& row = rows[9];
  row.opaque4 = INT16_MIN;
  row.opaque6 = 1;
  row.opaque8 = -1;
  row.opaqueA = 3;
  row.opaque10 = INT16_MAX;
  row.opaque12 = 1;
  row.opaque14 = INT16_MAX;
  row.opaque16 = 1;
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
  expected.actionStatus = 1;
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        false);
  using StateBytes = std::array<std::byte, sizeof(fixture.state)>;
  CHECK((std::bit_cast<StateBytes>(fixture.state) ==
         std::bit_cast<StateBytes>(expected)));
  fixture.room.readinessLevel = 101;
  CHECK(Battle_Ai_RefreshActionCandidateReadinessEntry(
            reinterpret_cast<Battle_Ai_CandidateReadinessView*>(
                &fixture.state)) == 0);
  CHECK(fixture.state.actionStatus == 0);
}

TEST_CASE("Native candidate readiness clears only weight and exhausted count",
          "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  fixture.state.candidates[0].threshold = 100;
  fixture.state.relations[3].classification = 0;
  auto expected = fixture.state;
  expected.candidates[0].weight = 0;
  expected.candidateCount = 0;
  expected.actionStatus = 3;
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
      std::bit_cast<Battle_Ai_ActionCandidate>(candidate.RawWords());
  fixture.state.relations[3].classification = 3;
  Battle_actionDefinitions[42].aiFlags =
      std::bit_cast<decltype(Battle_actionDefinitions[42].aiFlags)>(
          std::uint16_t{0x8000});
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        false);
  CHECK(fixture.state.candidates[0].weight == 17);
  CHECK(fixture.state.actionSlots[2][1].opaque0Bit7 == 1);
  CHECK(fixture.state.actionStatus == 0);
  fixture.actor.opaque3C = nullptr;
  fixture.state.actionStatus = 9;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state));
  CHECK(fixture.state.candidates[0].weight == 17);
  CHECK(fixture.state.actionStatus == 9);
}

TEST_CASE(
    "Native candidate readiness promotes first survivor without compaction",
    "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  fixture.state.candidateCount = 3;
  fixture.state.candidates[0].weight = 0;
  fixture.state.candidates[1].weight = 0;
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        true);
  CHECK(fixture.state.candidateOrder[0] == &fixture.state.candidates[2]);
  CHECK(fixture.state.candidateOrder[1] == &fixture.state.candidates[1]);
  CHECK(fixture.state.candidateOrder[2] == &fixture.state.candidates[2]);
  CHECK(fixture.state.candidateCount == 3);
  CHECK(fixture.state.planMode == 1);
  CHECK(fixture.state.actionStatus == 1);
}

TEST_CASE("Native candidate readiness empty status checks actual hostile links",
          "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  fixture.state.candidateCount = 0;
  fixture.hostile.id = 4;
  fixture.state.relations[4].classification = 3;
  Battle_actors[0] = &fixture.hostile;
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        false);
  CHECK(fixture.state.actionStatus == 2);
  fixture.state.relations[4].classification = 2;
  CHECK(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state) ==
        false);
  CHECK(fixture.state.actionStatus == 3);
  fixture.hostile.next = &fixture.hostile;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state));
}

TEST_CASE("Candidate readiness retains its original module address",
          "[ai][planning][candidate-readiness-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800D85D8) ==
        reinterpret_cast<const void*>(
            &Battle_Ai_RefreshActionCandidateReadinessEntry));
}

TEST_CASE("Native candidate readiness refuses absent malformed and short state",
          "[ai][planning][candidate-readiness-live]") {
  ReadinessFixture fixture;
  CHECK_FALSE(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(nullptr));
  fixture.state.candidateCount = 17;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state));
  fixture.state.candidateCount = 1;
  fixture.state.candidateOrder[0] = nullptr;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveActionCandidateReadiness(&fixture.state));
  std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap(arena);
  auto* state = VS::Core::Memory::AllocateLiveFromLowEnd(16);
  REQUIRE(state != nullptr);
  CHECK_FALSE(VS::Battle::Ai::RefreshLiveActionCandidateReadiness(
      static_cast<Battle_Ai_ActorAiState*>(state)));
}

TEST_CASE("AI target memory leaves empty and negative candidate sets untouched",
          "[ai][planning][target-memory]") {
  TargetMemoryFixture fixture(0);
  fixture.state.candidateCount = GENERATE(-1, 0);
  fixture.state.candidateOrder[0] = nullptr;
  const auto expected = fixture.Bytes();
  VS::Battle::Ai::RememberLiveAiActionTarget(&fixture.state);
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("AI target memory preserves the original nonzero settings guard",
          "[ai][planning][target-memory]") {
  TargetMemoryFixture fixture(15);
  fixture.state.settingsConstant19 = static_cast<char>(GENERATE(1, 255));
  fixture.state.candidateCount = GENERATE(1, 16);
  fixture.state.candidateOrder[0] = nullptr;
  const auto expected = fixture.Bytes();
  VS::Battle::Ai::RememberLiveAiActionTarget(&fixture.state);
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("AI candidates copy the complete record including overlapping input",
          "[ai][planning]") {
  const auto source_offset = GENERATE(0, 4);
  const auto destination_offset = GENERATE(0, 4, 8);
  alignas(Battle_Ai_ActionCandidate) std::array<unsigned char, 32> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<unsigned char>(index + 1);
  }
  auto expected = bytes;
  std::memmove(expected.data() + destination_offset,
               expected.data() + source_offset, 20);
  VS::Battle::Ai::CopyLiveActionCandidate(
      reinterpret_cast<Battle_Ai_ActionCandidate*>(bytes.data() +
                                                   destination_offset),
      reinterpret_cast<Battle_Ai_ActionCandidate*>(bytes.data() +
                                                   source_offset));
  CHECK(bytes == expected);
}

TEST_CASE(
    "AI append owns complete candidates and marks exact target/action pairs",
    "[ai][planning]") {
  PlanningFixture fixture;
  Battle_Ai_ActionCandidate candidate{};
  candidate.targetId = 15;
  candidate.action = 255;
  candidate.weight = 23;
  candidate.threshold = 0xCAFE;
  for (int index = 0; index < 16; ++index) {
    candidate.opaque0 = index - 5;
    VS::Battle::Ai::AppendLiveActionCandidate(&fixture.state, &candidate);
    REQUIRE(fixture.state.candidateCount == index + 1);
    CHECK(fixture.state.candidateOrder[index] ==
          &fixture.state.candidates[index]);
    CHECK(std::memcmp(&fixture.state.candidates[index], &candidate,
                      sizeof(candidate)) == 0);
  }
  CHECK(fixture.exclusions[15][255] == 1);
  candidate.targetId = 9;
  candidate.action = 81;
  VS::Battle::Ai::AppendLiveActionCandidate(&fixture.state, &candidate);
  CHECK(fixture.state.candidateCount == 16);
  CHECK(fixture.exclusions[9][81] == 0);
}

TEST_CASE("AI rejects zero-weight candidates without side effects",
          "[ai][planning]") {
  PlanningFixture fixture;
  Battle_Ai_ActionCandidate candidate{};
  candidate.targetId = 3;
  candidate.action = 42;
  VS::Battle::Ai::AppendLiveActionCandidate(&fixture.state, &candidate);
  CHECK(fixture.state.candidateCount == 0);
  CHECK(fixture.exclusions[3][42] == 0);
}

TEST_CASE(
    "AI ranking uses signed priorities and the original midpoint partition",
    "[ai][planning]") {
  Battle_Ai_ActionPlanView plan{};
  const std::array<int, 6> scores{2, -3, 2, 9, 9, 0};
  const std::array<int, 6> expected{4, 3, 2, 0, 5, 1};
  plan.candidateCount = static_cast<int>(scores.size());
  for (std::size_t index = 0; index < scores.size(); ++index) {
    plan.candidates[index].reserved0 = scores[index];
    plan.candidateOrder[index] = &plan.candidates[index];
  }
  VS::Battle::Ai::SortLiveActionCandidates(&plan);
  for (std::size_t index = 0; index < expected.size(); ++index) {
    CHECK(plan.candidateOrder[index] == &plan.candidates[expected[index]]);
  }
}

TEST_CASE(
    "AI ranking lifecycle produces, consumes, resets and repeats real plans",
    "[ai][planning][pointer-sort]") {
  PlanningFixture fixture;
  auto saved_actor = Battle_actors[0];
  Battle_Actor listed{};
  listed.id = 3;
  Battle_actors[0] = &listed;
  auto& state = fixture.state;
  auto& plan = reinterpret_cast<Battle_Ai_ActionPlanView&>(state);
  const std::array<int, 6> scores{2, -3, 2, 9, 9, 0};
  const std::array<std::size_t, 6> expected{4, 3, 2, 0, 5, 1};

  for (int entry = 0; entry < 2; ++entry) {
    CAPTURE(entry);
    for (std::size_t index = 0; index < scores.size(); ++index) {
      Battle_Ai_ActionCandidate candidate{};
      candidate.opaque0 = scores[index];
      candidate.weight = 1;
      candidate.targetId = 3;
      candidate.action = static_cast<unsigned>(40 + index);
      fixture.stats.limbActions[index][0].actionId =
          static_cast<u_short>(candidate.action);
      VS::Battle::Ai::AppendLiveActionCandidate(&state, &candidate);
      CHECK(fixture.exclusions[3][candidate.action] == 1);
    }
    REQUIRE(state.candidateCount == 6);
    VS::Battle::Ai::SortLiveActionCandidates(&plan);
    for (std::size_t index = 0; index < expected.size(); ++index) {
      CHECK(state.candidateOrder[index] == &state.candidates[expected[index]]);
    }

    // state.actorId == 0 takes the planner's real reset-only path.
    state.actorId = 0;
    VS::Battle::Ai::BuildLiveActorActionPlan(&state);
    state.actorId = 1;
    CHECK(state.candidateCount == 0);
    for (std::size_t index = 0; index < scores.size(); ++index) {
      CHECK(fixture.exclusions[3][40 + index] == 0);
    }
  }
  Battle_actors[0] = saved_actor;
}

TEST_CASE("AI ranking producer refuses zero weights and a full candidate list",
          "[ai][planning][pointer-sort]") {
  PlanningFixture fixture;
  auto& state = fixture.state;
  Battle_Ai_ActionCandidate candidate{};
  candidate.targetId = 3;
  candidate.action = 42;
  VS::Battle::Ai::AppendLiveActionCandidate(&state, &candidate);
  CHECK(state.candidateCount == 0);
  CHECK(fixture.exclusions[3][42] == 0);

  candidate.weight = 1;
  for (int index = 0; index < 16; ++index) {
    candidate.opaque0 = index;
    VS::Battle::Ai::AppendLiveActionCandidate(&state, &candidate);
  }
  REQUIRE(state.candidateCount == 16);
  const auto ordered = std::to_array(state.candidateOrder);
  candidate.opaque0 = 99;
  VS::Battle::Ai::AppendLiveActionCandidate(&state, &candidate);
  CHECK(state.candidateCount == 16);
  CHECK(std::to_array(state.candidateOrder) == ordered);
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
  Battle_ActorStats source;
  Battle_ActorStats destination;
  std::memset(&source, 0x5A, sizeof(source));
  std::memset(&destination, 0xA5, sizeof(destination));
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
  fixture.stats.limbActions[row][column].actionId = 42;
  auto& slot = fixture.state.actionSlots[row][column];
  slot = std::bit_cast<Battle_Ai_ActionSlotMetadata>(
      std::array<uint32_t, 2>{0xA5CAFE13, 0xDEADBEAF});
  auto& action = Battle_actionDefinitions[42];
  action.type = static_cast<u_char>(type);
  action.cost = 10;
  fixture.stats.currentHp = 59;
  fixture.stats.currentMp = 9;
  fixture.stats.weapon.currentPp = 9;
  CHECK(VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, row,
                                                     column) ==
        !(type == 1 || type == 3 || type == 4));
  fixture.stats.currentHp = 60;
  fixture.stats.currentMp = 10;
  fixture.stats.weapon.currentPp = 10;
  CHECK(VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, row,
                                                     column));
  CHECK(slot.opaque0Bit7 == 1);
  CHECK(std::bit_cast<std::array<uint32_t, 2>>(slot) ==
        std::array<uint32_t, 2>{0xA5CAFE93, 0xDEADBEAF});
}

TEST_CASE("AI action usability applies status and damaged-limb restrictions",
          "[ai][planning]") {
  const auto type = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  PlanningFixture fixture;
  Battle_actionDefinitions[42].type = static_cast<u_char>(type);
  fixture.stats.statuses = 0x2000;
  CHECK(VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1) ==
        (type == 1 || type == 2));
  fixture.stats.statuses = 0;
  fixture.stats.limbs[2].hp = 1;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1));
  CHECK(fixture.state.actionSlots[2][1].opaque0Bit7 == 0);
}

TEST_CASE(
    "AI resource conditions use unsigned halves and the actual status mask",
    "[ai][planning]") {
  PlanningFixture fixture;
  fixture.state.actionCostEnabled = 10;
  fixture.stats.currentHp = 49;
  fixture.stats.currentMp = 9;
  fixture.stats.statuses = 0x90000;
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.reserved10[0] == 0);
  CHECK(fixture.state.reserved10[1] == 1);
  CHECK(fixture.state.reserved10[2] == 1);
  fixture.stats.currentHp = 50;
  fixture.stats.currentMp = 50;
  fixture.stats.statuses = 0x1000;
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.reserved10[0] == 1);
  CHECK(fixture.state.reserved10[1] == 0);
  CHECK(fixture.state.reserved10[2] == 0);
  fixture.state.actionCostEnabled = 0;
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.reserved10[0] == 0);
}

TEST_CASE(
    "AI target eligibility observes relations, condition masks and self "
    "restrictions",
    "[ai][planning]") {
  PlanningFixture fixture;
  Battle_Ai_ActionCandidate candidate{};
  candidate.row = 2;
  candidate.column = 1;
  candidate.action = 42;
  fixture.state.relations[3].classification = 2;
  CHECK(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state, &candidate,
                                                   3));
  candidate.opaque10.opaque0Bit4 = 1;
  CHECK_FALSE(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state,
                                                         &candidate, 3));
  fixture.state.relations[3].classification = 3;
  CHECK(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state, &candidate,
                                                   3));
  candidate.opaque10.opaque0Bit4 = 0;
  candidate.opaque10.opaque0Bit5 = 1;
  CHECK_FALSE(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state,
                                                         &candidate, 3));
  fixture.state.relations[3].classification = 2;
  fixture.peer.actor = &fixture.actor;
  fixture.peer.actionCostEnabled = 10;
  fixture.stats.currentMp = 9;
  VS::Battle::Ai::RefreshLiveAiResourceRestrictions(&fixture.peer);
  Battle_actionDefinitions[42].aiFlags =
      std::bit_cast<Battle_ActionAiFlags>(u_short{0x400});
  CHECK(VS::Battle::Ai::CanLiveAiActionAffectActor(&fixture.state, &candidate,
                                                   3));
  fixture.stats.currentMp = 50;
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
  CHECK(fixture.state.walkStep == example[2]);
  CHECK(fixture.state.runStep == example[3]);
  CHECK(fixture.state.maxRange == example[4]);
}

TEST_CASE("AI sorting covers empty/full lists and signed extremes without RNG",
          "[ai][planning]") {
  const auto count = GENERATE(0, 1, 2, 6, 16);
  const auto pattern = GENERATE(0, 1, 2);
  Battle_Ai_ActionPlanView plan{};
  plan.candidateCount = count;
  const auto random_table = std::to_array(Core_randomShuffleTable);
  const auto random_index = Core_randomShuffleIndex;
  const auto interface_random = Core_interfaceRandomState;
  for (int index = 0; index < count; ++index) {
    plan.candidates[index].reserved0 =
        pattern == 0 ? index : (pattern == 1 ? -index : 0);
    plan.candidateOrder[index] = &plan.candidates[index];
  }
  if (count > 1 && pattern != 2) {
    plan.candidates[0].reserved0 = std::numeric_limits<int>::min();
    plan.candidates[count - 1].reserved0 = std::numeric_limits<int>::max();
  }
  const auto records = std::to_array(plan.candidates);
  VS::Battle::Ai::SortLiveActionCandidates(&plan);
  for (int index = 1; index < count; ++index) {
    CHECK(plan.candidateOrder[index - 1]->reserved0 >=
          plan.candidateOrder[index]->reserved0);
  }
  CHECK(std::memcmp(records.data(), plan.candidates, sizeof(plan.candidates)) ==
        0);
  CHECK(std::to_array(Core_randomShuffleTable) == random_table);
  CHECK(Core_randomShuffleIndex == random_index);
  CHECK(Core_interfaceRandomState == interface_random);
}

TEST_CASE(
    "AI resource comparisons retain signed MP/PP and unsigned HP semantics",
    "[ai][planning]") {
  PlanningFixture fixture;
  auto& action = Battle_actionDefinitions[42];
  action.cost = 10;
  action.type = 1;
  fixture.stats.currentMp = -1;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1));
  fixture.stats.currentMp = 10;
  fixture.stats.statuses = 0x1000;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1));
  fixture.stats.statuses = 0;
  action.type = 4;
  fixture.stats.weapon.currentPp = 0xFFFF;
  CHECK_FALSE(
      VS::Battle::Ai::RefreshLiveAiActionUsability(&fixture.state, 2, 1));
  action.type = 3;
  fixture.stats.currentHp = -1;
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
  CHECK(fixture.state.resourceStatusRestricted ==
        (((uint32_t{1} << bit) & 0xE2B0) != 0));
}
