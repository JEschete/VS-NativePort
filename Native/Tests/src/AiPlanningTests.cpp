#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
#include "Battle/Damage.h"
#include "Core/Random.h"
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
    Battle_Ai_BuildActorActionPlan(&plan);
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
