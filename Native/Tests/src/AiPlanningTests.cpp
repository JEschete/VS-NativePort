#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/PlanningPrimitives.h"
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
  Battle_Ai_RememberAiActionTarget(&fixture.state);
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("AI target memory leaves empty and negative candidate sets untouched",
          "[ai][planning][target-memory]") {
  TargetMemoryFixture fixture(0);
  fixture.state.candidateCount = GENERATE(-1, 0);
  fixture.state.candidateOrder[0] = nullptr;
  const auto expected = fixture.Bytes();
  Battle_Ai_RememberAiActionTarget(&fixture.state);
  CHECK(fixture.Bytes() == expected);
}

TEST_CASE("AI target memory preserves the original nonzero settings guard",
          "[ai][planning][target-memory]") {
  TargetMemoryFixture fixture(15);
  fixture.state.settingsConstant19 = static_cast<char>(GENERATE(1, 255));
  fixture.state.candidateCount = GENERATE(1, 16);
  fixture.state.candidateOrder[0] = nullptr;
  const auto expected = fixture.Bytes();
  Battle_Ai_RememberAiActionTarget(&fixture.state);
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
  Battle_Ai_CopyActionCandidate(reinterpret_cast<Battle_Ai_ActionCandidate*>(
                                    bytes.data() + destination_offset),
                                reinterpret_cast<Battle_Ai_ActionCandidate*>(
                                    bytes.data() + source_offset));
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
    Battle_Ai_AppendActionCandidate(&fixture.state, &candidate);
    REQUIRE(fixture.state.candidateCount == index + 1);
    CHECK(fixture.state.candidateOrder[index] ==
          &fixture.state.candidates[index]);
    CHECK(std::memcmp(&fixture.state.candidates[index], &candidate,
                      sizeof(candidate)) == 0);
  }
  CHECK(fixture.exclusions[15][255] == 1);
  candidate.targetId = 9;
  candidate.action = 81;
  Battle_Ai_AppendActionCandidate(&fixture.state, &candidate);
  CHECK(fixture.state.candidateCount == 16);
  CHECK(fixture.exclusions[9][81] == 0);
}

TEST_CASE("AI rejects zero-weight candidates without side effects",
          "[ai][planning]") {
  PlanningFixture fixture;
  Battle_Ai_ActionCandidate candidate{};
  candidate.targetId = 3;
  candidate.action = 42;
  Battle_Ai_AppendActionCandidate(&fixture.state, &candidate);
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
  Battle_Ai_SortActionCandidates(&plan);
  for (std::size_t index = 0; index < expected.size(); ++index) {
    CHECK(plan.candidateOrder[index] == &plan.candidates[expected[index]]);
  }
}

TEST_CASE("AI simulated stat deltas clamp add and subtract at real bounds",
          "[ai][planning]") {
  const int operation = GENERATE(0, 1, 2, 3);
  int16_t current = 50;
  int16_t maximum = 100;
  Battle_Ai_ApplySimulatedStatDelta(&current, &maximum, 20, operation);
  CHECK(current == (operation == 2 ? 70 : 30));
  Battle_Ai_ApplySimulatedStatDelta(&current, &maximum, 1000, operation);
  CHECK(current == (operation == 2 ? 100 : 0));
  CHECK(maximum == 100);
}

TEST_CASE("AI planning snapshots copy only the original simulation stat ranges",
          "[ai][planning]") {
  Battle_ActorStats source;
  Battle_ActorStats destination;
  std::memset(&source, 0x5A, sizeof(source));
  std::memset(&destination, 0xA5, sizeof(destination));
  Battle_Ai_CopyPlanningActorStats(&source, &destination);
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
  CHECK(Battle_Ai_RefreshAiActionUsability(&fixture.state, row, column) ==
        (type == 1 || type == 3 || type == 4 ? 0 : 1));
  fixture.stats.currentHp = 60;
  fixture.stats.currentMp = 10;
  fixture.stats.weapon.currentPp = 10;
  CHECK(Battle_Ai_RefreshAiActionUsability(&fixture.state, row, column) == 1);
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
  CHECK(Battle_Ai_RefreshAiActionUsability(&fixture.state, 2, 1) ==
        (type == 1 || type == 2 ? 1 : 0));
  fixture.stats.statuses = 0;
  fixture.stats.limbs[2].hp = 1;
  CHECK(Battle_Ai_RefreshAiActionUsability(&fixture.state, 2, 1) == 0);
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
  Battle_Ai_RefreshAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.reserved10[0] == 0);
  CHECK(fixture.state.reserved10[1] == 1);
  CHECK(fixture.state.reserved10[2] == 1);
  fixture.stats.currentHp = 50;
  fixture.stats.currentMp = 50;
  fixture.stats.statuses = 0x1000;
  Battle_Ai_RefreshAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.reserved10[0] == 1);
  CHECK(fixture.state.reserved10[1] == 0);
  CHECK(fixture.state.reserved10[2] == 0);
  fixture.state.actionCostEnabled = 0;
  Battle_Ai_RefreshAiResourceRestrictions(&fixture.state);
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
  CHECK(Battle_Ai_CanAiActionAffectActor(&fixture.state, &candidate, 3) == 1);
  candidate.opaque10.opaque0Bit4 = 1;
  CHECK(Battle_Ai_CanAiActionAffectActor(&fixture.state, &candidate, 3) == 0);
  fixture.state.relations[3].classification = 3;
  CHECK(Battle_Ai_CanAiActionAffectActor(&fixture.state, &candidate, 3) == 1);
  candidate.opaque10.opaque0Bit4 = 0;
  candidate.opaque10.opaque0Bit5 = 1;
  CHECK(Battle_Ai_CanAiActionAffectActor(&fixture.state, &candidate, 3) == 0);
  fixture.state.relations[3].classification = 2;
  fixture.peer.actor = &fixture.actor;
  fixture.peer.actionCostEnabled = 10;
  fixture.stats.currentMp = 9;
  Battle_Ai_RefreshAiResourceRestrictions(&fixture.peer);
  Battle_actionDefinitions[42].aiFlags =
      std::bit_cast<Battle_ActionAiFlags>(u_short{0x400});
  CHECK(Battle_Ai_CanAiActionAffectActor(&fixture.state, &candidate, 3) == 1);
  fixture.stats.currentMp = 50;
  Battle_Ai_RefreshAiResourceRestrictions(&fixture.peer);
  CHECK(Battle_Ai_CanAiActionAffectActor(&fixture.state, &candidate, 3) == 0);
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
  Battle_Ai_SetActorVerticalMovementSteps(1, example[0], example[1]);
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
  Battle_Ai_SortActionCandidates(&plan);
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
  CHECK(Battle_Ai_RefreshAiActionUsability(&fixture.state, 2, 1) == 0);
  fixture.stats.currentMp = 10;
  fixture.stats.statuses = 0x1000;
  CHECK(Battle_Ai_RefreshAiActionUsability(&fixture.state, 2, 1) == 0);
  fixture.stats.statuses = 0;
  action.type = 4;
  fixture.stats.weapon.currentPp = 0xFFFF;
  CHECK(Battle_Ai_RefreshAiActionUsability(&fixture.state, 2, 1) == 0);
  action.type = 3;
  fixture.stats.currentHp = -1;
  CHECK(Battle_Ai_RefreshAiActionUsability(&fixture.state, 2, 1) == 1);
}

TEST_CASE("AI condition status mask rejects only the documented source bits",
          "[ai][planning]") {
  const auto bit =
      GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
               19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
  PlanningFixture fixture;
  fixture.stats.statuses = std::bit_cast<int>(uint32_t{1} << bit);
  Battle_Ai_RefreshAiResourceRestrictions(&fixture.state);
  CHECK(fixture.state.resourceStatusRestricted ==
        (((uint32_t{1} << bit) & 0xE2B0) != 0));
}
