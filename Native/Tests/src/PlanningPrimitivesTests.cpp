#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Battle/Ai/PlanningPrimitives.hpp"

namespace {
namespace ai = VS::Battle::Ai;
using ai::CandidateOption;
using Words = ai::ActionCandidate::Words;
using SlotUpdate = std::tuple<int, int, bool>;

class FakeTargetMemory final : public ai::TargetMemoryHost {
 public:
  [[nodiscard]] int CandidateCount() const override { return count; }
  [[nodiscard]] bool MemoryDisabled() const override { return disabled; }
  [[nodiscard]] std::optional<unsigned> FirstRankedTarget() const override {
    return first_target;
  }
  [[nodiscard]] std::uint32_t OwnTile() const override { return own_tile; }
  [[nodiscard]] std::optional<std::uint32_t> TargetTile(
      unsigned target) const override {
    queried.push_back(target);
    return target_tile;
  }
  void Remember(const ai::TargetMemory& memory) override {
    remembered = memory;
  }

  int count = 2;
  bool disabled = false;
  std::optional<unsigned> first_target = 15;
  std::uint32_t own_tile = 0xF4ABCDEF;
  std::optional<std::uint32_t> target_tile = 0xA1B2C3D4;
  std::optional<ai::TargetMemory> remembered;
  mutable std::vector<unsigned> queried;
};

class FakeCandidateReadiness final : public ai::CandidateReadinessHost {
 public:
  FakeCandidateReadiness() {
    affects.fill(true);
    for (std::size_t index = 0; index < order.size(); ++index) {
      order[index] = index;
      candidates[index].SetWeight(1);
      candidates[index].SetThreshold(100);
      candidates[index].SetSlot(1, 1);
      candidates[index].SetTargetId(static_cast<unsigned>(index));
    }
  }
  int Count() const override { return count; }
  std::optional<std::size_t> RankedHandle(std::size_t rank) const override {
    return rank < order.size() ? std::optional(order[rank]) : std::nullopt;
  }
  std::optional<ai::ActionCandidate> Candidate(
      std::size_t handle) const override {
    return handle < candidates.size() ? std::optional(candidates[handle])
                                      : std::nullopt;
  }
  std::uint16_t ReadinessLevel() const override { return level; }
  std::optional<bool> CanAffect(std::size_t handle, unsigned target) override {
    targeting.emplace_back(handle, target);
    if (slot_after_targeting) {
      candidates[handle].SetSlot(slot_after_targeting->first,
                                 slot_after_targeting->second);
    }
    if (target_after_targeting) {
      candidates[handle].SetTargetId(*target_after_targeting);
    }
    if (count_after_targeting) count = *count_after_targeting;
    return affects.at(handle);
  }
  std::optional<bool> RefreshUsability(unsigned row, unsigned column) override {
    usability.emplace_back(row, column);
    return usable;
  }
  bool ClearWeight(std::size_t handle) override {
    if (!accept_stores) return false;
    candidates.at(handle).SetWeight(0);
    return true;
  }
  bool SetFirstRank(std::size_t handle) override {
    if (!accept_stores) return false;
    order[0] = handle;
    return true;
  }
  bool ClearCount() override {
    if (!accept_stores) return false;
    count = 0;
    return true;
  }
  bool SelectMovementMode() override {
    ++movement_selections;
    if (clear_after_selection) count = 0;
    return accept_stores;
  }
  std::optional<bool> HasHostileActor() const override { return hostile; }
  bool StoreActionStatus(std::uint8_t value) override {
    if (!accept_stores) return false;
    status = value;
    return true;
  }

  int count = 3;
  std::uint16_t level = 100;
  std::array<ai::ActionCandidate, ai::kCandidateCapacity> candidates{};
  std::array<std::size_t, ai::kCandidateCapacity> order{};
  std::array<bool, ai::kCandidateCapacity> affects{};
  bool usable = true;
  bool accept_stores = true;
  bool clear_after_selection = false;
  int movement_selections = 0;
  std::optional<int> count_after_targeting;
  std::optional<std::pair<unsigned, unsigned>> slot_after_targeting;
  std::optional<unsigned> target_after_targeting;
  std::optional<bool> hostile = false;
  std::optional<std::uint8_t> status;
  std::vector<std::pair<std::size_t, unsigned>> targeting;
  std::vector<std::pair<unsigned, unsigned>> usability;
};

class FakeCandidateList final : public ai::CandidateList {
 public:
  [[nodiscard]] int Count() const override { return count; }
  void Store(std::size_t slot, const ai::ActionCandidate& candidate) override {
    stored.at(slot) = candidate;
    ++stores;
  }
  void SetCount(int value) override { count = value; }
  [[nodiscard]] bool ExcludeTargetAction(unsigned target,
                                         unsigned action) override {
    if (!allocated) return false;
    exclusions.emplace_back(target, action);
    return true;
  }

  int count = 0;
  bool allocated = true;
  int stores = 0;
  std::array<ai::ActionCandidate, ai::kCandidateCapacity> stored{};
  std::vector<std::pair<unsigned, unsigned>> exclusions;
};

class FakeTargeting final : public ai::ActionTargetingHost {
 public:
  [[nodiscard]] int ActorId() const override { return actor_id; }
  [[nodiscard]] unsigned Classification(int id) const override {
    return classifications.at(static_cast<std::size_t>(id));
  }
  [[nodiscard]] std::optional<ai::ActorResourceConditions> PeerConditions(
      int id) const override {
    if (!present.at(static_cast<std::size_t>(id))) return std::nullopt;
    return conditions;
  }
  [[nodiscard]] std::uint16_t ActionAiFlags(
      unsigned /*action*/) const override {
    return flags;
  }
  [[nodiscard]] bool SlotExcludesSelf(int /*row*/,
                                      int /*column*/) const override {
    return excludes_self;
  }

  int actor_id = 1;
  std::array<unsigned, ai::kCandidateTargetSlots> classifications{};
  std::array<bool, ai::kCandidateTargetSlots> present{};
  ai::ActorResourceConditions conditions;
  std::uint16_t flags = 0;
  bool excludes_self = false;
};

class FakeUsability final : public ai::ActionUsabilityHost {
 public:
  [[nodiscard]] unsigned SlotAction(int /*row*/,
                                    int /*column*/) const override {
    return action_id;
  }
  [[nodiscard]] ai::ActionCost Action(unsigned id) const override {
    actions.push_back(id);
    return cost;
  }
  [[nodiscard]] std::int16_t LimbHp(int /*row*/) const override {
    return limb_hp;
  }
  [[nodiscard]] ai::ActorResources Resources() const override {
    return resources;
  }
  void SetSlotUsable(int row, int column, bool usable) override {
    updates.emplace_back(row, column, usable);
  }

  unsigned action_id = 42;
  ai::ActionCost cost{.type = 0, .cost = 10};
  std::int16_t limb_hp = 2;
  ai::ActorResources resources{.hp = 100,
                               .max_hp = 100,
                               .mp = 100,
                               .max_mp = 100,
                               .weapon_pp = 100,
                               .statuses = 0};
  mutable std::vector<unsigned> actions;
  std::vector<SlotUpdate> updates;
};

ai::ActionCandidate SlotCandidate() {
  ai::ActionCandidate candidate;
  candidate.SetSlot(2, 1);
  candidate.SetAction(42);
  return candidate;
}

std::vector<ai::RankedCandidate> Ranking(std::span<const int> scores) {
  std::vector<ai::RankedCandidate> ranking;
  for (std::size_t index = 0; index < scores.size(); ++index) {
    ranking.push_back(
        {.score = scores[index], .action = 0, .weight = 0, .handle = index});
  }
  return ranking;
}

std::vector<std::size_t> Handles(std::span<const ai::RankedCandidate> ranking) {
  std::vector<std::size_t> handles;
  for (const ai::RankedCandidate& entry : ranking) {
    handles.push_back(entry.handle);
  }
  return handles;
}

std::array<bool, 3> Restrictions(
    const ai::ActorResourceConditions& conditions) {
  return {conditions.status_restricted, conditions.low_mp, conditions.low_hp};
}

bool CopiedStatByte(std::size_t offset) {
  return (offset >= 0x18 && offset < 0x20) ||
         (offset >= 0x30 && offset < 0x34) ||
         (offset >= 0x398 && offset < 0x8C0) ||
         (offset >= 0x948 && offset < 0x94C);
}
}  // namespace

TEST_CASE("Simulated results apply original action type costs before performer",
          "[ai][planning][simulation-results-core]") {
  for (unsigned type = 0; type < 8; ++type) {
    std::array<ai::ActorCondition, 2> rows{};
    rows[0] = {.hp = 50,
               .max_hp = 100,
               .mp = 40,
               .max_mp = 100,
               .weapon_dp = 30,
               .weapon_max_dp = 100,
               .shield_dp = 20,
               .shield_max_dp = 100};
    ai::SimulatedActorEffect performer;
    performer.hp = {.delta = 7, .operation = 2};
    performer.mp = {.delta = 7, .operation = 2};
    performer.weapon_dp = {.delta = 7, .operation = 2};
    REQUIRE(ai::ApplySimulatedActionResults(rows, {type, 55}, performer, {}));
    CHECK(rows[0].hp == (type == 3 ? 7 : 57));
    CHECK(rows[0].mp == (type == 1 ? 7 : 47));
    CHECK(rows[0].weapon_dp == (type == 6 ? 7 : 37));
    CHECK(rows[0].shield_dp == 20);
    CHECK(rows[1] == ai::ActorCondition{});
  }
}

TEST_CASE(
    "Simulated results preserve ordered repeated actors and status clears",
    "[ai][planning][simulation-results-core]") {
  std::array<ai::ActorCondition, 1> rows{
      {{.hp = 50, .max_hp = 100, .statuses = 0x80000000}}};
  ai::SimulatedActorEffect performer;
  performer.hp.delta = 60;
  performer.statuses_set = 3;
  performer.statuses_clear = 1;
  std::array<ai::SimulatedActorEffect, 2> targets{};
  targets[0].hp = {.delta = 30, .operation = 2};
  targets[0].statuses_set = 5;
  targets[0].statuses_clear = 4;
  targets[1].hp.delta = 10;
  targets[1].statuses_clear = 2;
  REQUIRE(ai::ApplySimulatedActionResults(rows, {}, performer, targets));
  CHECK(rows[0].hp == 20);
  CHECK(rows[0].statuses == 0x80000001);
}

TEST_CASE("Simulated results restore only operation two and keep signed deltas",
          "[ai][planning][simulation-results-core]") {
  for (std::uint8_t operation = 0; operation < 4; ++operation) {
    std::array<ai::ActorCondition, 1> rows{{{.hp = 50,
                                             .max_hp = 100,
                                             .mp = 40,
                                             .max_mp = 100,
                                             .weapon_dp = 30,
                                             .weapon_max_dp = 100,
                                             .shield_dp = 20,
                                             .shield_max_dp = 100}}};
    ai::SimulatedActorEffect performer;
    performer.hp = {-10, operation};
    performer.mp = {10, operation};
    performer.weapon_dp = {-10, operation};
    performer.shield_dp = {10, operation};
    REQUIRE(ai::ApplySimulatedActionResults(rows, {}, performer, {}));
    CHECK(rows[0].hp == (operation == 2 ? 40 : 60));
    CHECK(rows[0].mp == (operation == 2 ? 50 : 30));
    CHECK(rows[0].weapon_dp == (operation == 2 ? 20 : 40));
    CHECK(rows[0].shield_dp == (operation == 2 ? 30 : 10));
  }
}

TEST_CASE(
    "Simulated results ignore unavailable performers and targets entirely",
    "[ai][planning][simulation-results-core]") {
  std::array<ai::ActorCondition, 1> rows{{{.hp = 7, .max_hp = 10}}};
  const auto before = rows;
  ai::SimulatedActorEffect performer{.actor_id = 255, .ignored = true};
  const std::array targets{performer};
  REQUIRE(ai::ApplySimulatedActionResults(rows, {1, 999}, performer, targets));
  CHECK(rows == before);
}

TEST_CASE("Simulated results refuse malformed active rows before mutation",
          "[ai][planning][simulation-results-core]") {
  std::array<ai::ActorCondition, 1> rows{{{.hp = 7, .max_hp = 10}}};
  const auto before = rows;
  ai::SimulatedActorEffect performer;
  performer.hp.delta = 2;
  std::array<ai::SimulatedActorEffect, 1> targets{{{.actor_id = 1}}};
  CHECK_FALSE(
      ai::ApplySimulatedActionResults(rows, {3, 5}, performer, targets));
  CHECK(rows == before);
  targets[0].ignored = true;
  CHECK_FALSE(
      ai::ApplySimulatedActionResults(rows, {3, 256}, performer, targets));
  CHECK(rows == before);
}

TEST_CASE("Condition status score applies each status bit's signed weight",
          "[ai][planning][condition-terms-core]") {
  // The 32 signed bytes at 0x80069BD0 in BATTLE.PRG.
  constexpr std::array<int, 32> kWeights = {
      -51, -39, -39, -39, -26, -51, 50, -13, 12,  -13, 12,
      25,  -51, -26, -45, -26, -39, 76, 19,  -13, 12,  12,
      12,  12,  12,  12,  12,  12,  12, 0,   0,   0};
  const unsigned bit = GENERATE(range(0u, 32u));
  const ai::ActorCondition condition{
      .hp = 100, .max_hp = 100, .statuses = std::uint32_t{1} << bit};
  CHECK(ai::ActorStatusConditionScore(condition, true) == kWeights[bit]);
}

TEST_CASE(
    "Condition status score counts the action cost status only when "
    "the AI enables it",
    "[ai][planning][condition-terms-core]") {
  const std::uint32_t others = (1u << 6) | (1u << 11);
  ai::ActorCondition condition{
      .hp = 100, .max_hp = 100, .statuses = (1u << 12) | others};
  CHECK(ai::ActorStatusConditionScore(condition, true) == 50 + 25 - 51);
  CHECK(ai::ActorStatusConditionScore(condition, false) == 50 + 25);
  condition.statuses = 1u << 12;
  CHECK(ai::ActorStatusConditionScore(condition, false) == 0);
}

TEST_CASE(
    "Condition status score penalizes zero HP and HP under 44/128 of "
    "the maximum",
    "[ai][planning][condition-terms-core]") {
  CHECK(ai::ActorStatusConditionScore({.hp = 0, .max_hp = 100}, true) ==
        -128 - 84);
  CHECK(ai::ActorStatusConditionScore({.hp = 0, .max_hp = 0}, true) == -128);
  CHECK(ai::ActorStatusConditionScore({.hp = 34, .max_hp = 100}, true) == -84);
  CHECK(ai::ActorStatusConditionScore({.hp = 35, .max_hp = 100}, true) == 0);
  // 11 * 128 == 32 * 44: an exact match is not low.
  CHECK(ai::ActorStatusConditionScore({.hp = 11, .max_hp = 32}, true) == 0);
  CHECK(ai::ActorStatusConditionScore({.hp = 10, .max_hp = 32}, true) == -84);
}

TEST_CASE("Condition status score compares HP as signed unnarrowed products",
          "[ai][planning][condition-terms-core]") {
  // Negative HP is low but is not the zero-HP case.
  CHECK(ai::ActorStatusConditionScore({.hp = -1, .max_hp = 100}, true) == -84);
  CHECK(ai::ActorStatusConditionScore({.hp = 5, .max_hp = -100}, true) == 0);
  CHECK(ai::ActorStatusConditionScore({.hp = -30, .max_hp = -100}, true) == 0);
  CHECK(ai::ActorStatusConditionScore({.hp = -35, .max_hp = -100}, true) ==
        -84);
  CHECK(ai::ActorStatusConditionScore({.hp = 32767, .max_hp = 32767}, true) ==
        0);
  CHECK(ai::ActorStatusConditionScore({.hp = -32768, .max_hp = 32767}, true) ==
        -84);
}

TEST_CASE("Condition ratios scale each current value to 128ths of its maximum",
          "[ai][planning][condition-terms-core]") {
  const ai::ActorCondition condition{.hp = 50,
                                     .max_hp = 100,
                                     .mp = 25,
                                     .max_mp = 100,
                                     .weapon_dp = 10,
                                     .weapon_max_dp = 40,
                                     .shield_dp = 30,
                                     .shield_max_dp = 60};
  CHECK(ai::ActorHpRatio(condition) == 64);
  CHECK(ai::ActorMpRatio(condition) == 32);
  CHECK(ai::ActorEquipmentDpRatio(condition) == 32 + 64);
}

TEST_CASE(
    "Condition ratios are zero without a maximum and skip absent "
    "equipment",
    "[ai][planning][condition-terms-core]") {
  ai::ActorCondition condition{.hp = 50,
                               .mp = 50,
                               .weapon_dp = 10,
                               .weapon_max_dp = 0,
                               .shield_dp = 30,
                               .shield_max_dp = 60};
  CHECK(ai::ActorHpRatio(condition) == 0);
  CHECK(ai::ActorMpRatio(condition) == 0);
  CHECK(ai::ActorEquipmentDpRatio(condition) == 64);
  condition.weapon_max_dp = 40;
  condition.shield_max_dp = 0;
  CHECK(ai::ActorEquipmentDpRatio(condition) == 32);
  condition.weapon_max_dp = 0;
  CHECK(ai::ActorEquipmentDpRatio(condition) == 0);
}

TEST_CASE("Condition ratios truncate toward zero and keep results past 16 bits",
          "[ai][planning][condition-terms-core]") {
  ai::ActorCondition condition{.hp = 1, .max_hp = 3, .mp = -1, .max_mp = 3};
  CHECK(ai::ActorHpRatio(condition) == 42);
  CHECK(ai::ActorMpRatio(condition) == -42);
  condition.hp = -1;
  condition.max_hp = -3;
  CHECK(ai::ActorHpRatio(condition) == 42);
  condition.hp = 32767;
  condition.max_hp = 1;
  CHECK(ai::ActorHpRatio(condition) == 4194176);
  condition.hp = -32768;
  CHECK(ai::ActorHpRatio(condition) == -4194304);
  condition.max_hp = -1;
  CHECK(ai::ActorHpRatio(condition) == 4194304);
  condition.weapon_dp = condition.shield_dp = 32767;
  condition.weapon_max_dp = condition.shield_max_dp = 1;
  CHECK(ai::ActorEquipmentDpRatio(condition) == 2 * 4194176);
}

TEST_CASE("Candidate readiness skips zero and future weights before callbacks",
          "[ai][planning][candidate-readiness-core]") {
  FakeCandidateReadiness host;
  host.candidates[0].SetWeight(0);
  host.candidates[1].SetThreshold(131);
  CHECK(ai::RefreshActionCandidateReadiness(host) == true);
  CHECK(host.targeting ==
        std::vector<std::pair<std::size_t, unsigned>>{{2, 2}});
  CHECK(host.order[0] == 1);
  CHECK(host.order[1] == 1);
  CHECK(host.order[2] == 2);
  CHECK(host.count == 3);
  CHECK(host.status == 1);
}

TEST_CASE(
    "Candidate readiness rereads callback fields and preserves other bits",
    "[ai][planning][candidate-readiness-core]") {
  FakeCandidateReadiness host;
  host.count = 1;
  host.candidates[0] = ai::ActionCandidate(
      Words{0x11223344, 0xF0000000, 0x55AA55AA, 0xF807FFFF, 0x5A5AA5A5});
  host.candidates[0].SetSlot(1, 2);
  host.candidates[0].SetThreshold(100);
  host.candidates[0].SetTargetId(15);
  host.candidates[0].SetWeight(255);
  auto expected = host.candidates[0];
  expected.SetSlot(5, 3);
  expected.SetTargetId(9);
  expected.SetWeight(0);
  host.slot_after_targeting = std::pair{5u, 3u};
  host.target_after_targeting = 9;
  host.usable = false;
  CHECK(ai::RefreshActionCandidateReadiness(host) == true);
  CHECK(host.targeting ==
        std::vector<std::pair<std::size_t, unsigned>>{{0, 15}});
  CHECK(host.usability == std::vector<std::pair<unsigned, unsigned>>{{5, 3}});
  CHECK(host.candidates[0].RawWords() == expected.RawWords());
  CHECK(host.count == 0);
  CHECK(host.status == 3);
}

TEST_CASE(
    "Candidate readiness promotes first survivor without compacting ranks",
    "[ai][planning][candidate-readiness-core]") {
  FakeCandidateReadiness host;
  host.candidates[0].SetWeight(0);
  host.candidates[1].SetWeight(0);
  CHECK(ai::RefreshActionCandidateReadiness(host) == true);
  CHECK(host.order[0] == 2);
  CHECK(host.order[1] == 1);
  CHECK(host.order[2] == 2);
  CHECK(host.count == 3);
  CHECK(host.movement_selections == 1);
  CHECK(host.status == 0);
}

TEST_CASE("Candidate readiness rereads count after targeting and movement",
          "[ai][planning][candidate-readiness-core]") {
  FakeCandidateReadiness host;
  host.count_after_targeting = 1;
  CHECK(ai::RefreshActionCandidateReadiness(host) == false);
  CHECK(host.targeting.size() == 1);
  host = FakeCandidateReadiness{};
  host.candidates[0].SetWeight(0);
  host.clear_after_selection = true;
  host.hostile = true;
  CHECK(ai::RefreshActionCandidateReadiness(host) == true);
  CHECK(host.count == 0);
  CHECK(host.status == 2);
}

TEST_CASE("Candidate readiness status preserves unsigned threshold margin",
          "[ai][planning][candidate-readiness-core]") {
  FakeCandidateReadiness host;
  host.count = 1;
  for (std::uint32_t threshold = 0; threshold <= UINT16_MAX; ++threshold) {
    host.candidates[0].SetThreshold(threshold);
    CHECK(ai::RefreshActionCandidateReadiness(host) == false);
    CHECK(host.status == static_cast<std::uint8_t>(130 < threshold));
    host.targeting.clear();
    host.usability.clear();
  }
  host.level = UINT16_MAX;
  host.candidates[0].SetThreshold(UINT16_MAX);
  CHECK(ai::RefreshActionCandidateReadiness(host) == false);
  CHECK(host.status == 0);
}

TEST_CASE("Candidate readiness empty and malformed states never invent a rank",
          "[ai][planning][candidate-readiness-core]") {
  FakeCandidateReadiness host;
  host.count = 0;
  CHECK(ai::RefreshActionCandidateReadiness(host) == false);
  CHECK(host.status == 3);
  host.hostile = true;
  CHECK(ai::RefreshActionCandidateReadiness(host) == false);
  CHECK(host.status == 2);
}

TEST_CASE("Candidate readiness malformed ranks refuse before state stores",
          "[ai][planning][candidate-readiness-core]") {
  FakeCandidateReadiness host;
  for (const int count : {-1, 17}) {
    host.count = count;
    host.status.reset();
    CHECK_FALSE(ai::RefreshActionCandidateReadiness(host));
    CHECK_FALSE(host.status);
  }
  host.count = 1;
  host.order[0] = ai::kCandidateCapacity;
  CHECK_FALSE(ai::RefreshActionCandidateReadiness(host));
  host.order[0] = 0;
  host.affects[0] = false;
  host.accept_stores = false;
  CHECK_FALSE(ai::RefreshActionCandidateReadiness(host));
  CHECK(host.candidates[0].Weight() == 1);
}

TEST_CASE("Action candidates pack each field into the original record words",
          "[ai][planning]") {
  ai::ActionCandidate candidate;
  candidate.SetScore(-5);
  candidate.SetSlot(5, 3);
  candidate.SetAction(0xFF);
  candidate.SetThreshold(0xFFFF);
  candidate.SetDistanceSquared(-1);
  candidate.SetTargetId(15);
  candidate.SetWeight(0xFF);
  candidate.SetRange(0xFFFF);
  for (const CandidateOption option :
       {CandidateOption::kMinimumRange, CandidateOption::kIgnoresSight,
        CandidateOption::kIgnoresObstruction, CandidateOption::kHarmful,
        CandidateOption::kBeneficial}) {
    candidate.SetOption(option, true);
  }
  CHECK(candidate.RawWords() ==
        Words{0xFFFFFFFB, 0x1FFFFFFD, 0xFFFFFFFF, 0x07F8000F, 0x003FFFFE});
  CHECK(std::array<std::int32_t, 2>{candidate.Score(),
                                    candidate.DistanceSquared()} ==
        std::array<std::int32_t, 2>{-5, -1});
  CHECK(std::array<unsigned, 7>{candidate.Row(), candidate.Column(),
                                candidate.Action(), candidate.Threshold(),
                                candidate.TargetId(), candidate.Weight(),
                                candidate.Range()} ==
        std::array<unsigned, 7>{5, 3, 0xFF, 0xFFFF, 15, 0xFF, 0xFFFF});
  CHECK(candidate.Option(CandidateOption::kBeneficial));
}

TEST_CASE(
    "Action candidate setters truncate to the field width and keep other bits",
    "[ai][planning]") {
  ai::ActionCandidate candidate(Words{0, 0, 0, 0, 0});
  candidate.SetSlot(8, 4);
  candidate.SetAction(0x1FF);
  candidate.SetThreshold(0x10000);
  candidate.SetTargetId(16);
  candidate.SetWeight(0x100);
  candidate.SetRange(0x10000);
  CHECK(candidate.Row() == 0);
  CHECK(candidate.Column() == 0);
  CHECK(candidate.Action() == 0xFF);
  CHECK(candidate.Threshold() == 0);
  CHECK(candidate.TargetId() == 0);
  CHECK(candidate.Weight() == 0);
  CHECK(candidate.Range() == 0);
  ai::ActionCandidate full(Words{~0u, ~0u, ~0u, ~0u, ~0u});
  full.SetAction(0);
  full.SetOption(CandidateOption::kHarmful, false);
  full.SetWeight(0);
  CHECK(full.RawWords() ==
        Words{~0u, ~(0xFFu << 5), ~0u, ~(0xFFu << 19), ~(1u << 4)});
}

TEST_CASE("Candidate copies carry every word including unnamed bits",
          "[ai][planning]") {
  const ai::ActionCandidate source(
      Words{0x80000001, 0xE0000000, 0x12345678, 0xF8700000, 0xFFC00001});
  ai::ActionCandidate destination(Words{1, 2, 3, 4, 5});
  ai::CopyActionCandidate(destination, source);
  CHECK(destination.RawWords() == source.RawWords());
  ai::CopyActionCandidate(destination, destination);
  CHECK(destination.RawWords() == source.RawWords());
}

TEST_CASE("Simulated stat deltas restore or spend within zero and the maximum",
          "[ai][planning]") {
  const bool restores = GENERATE(false, true);
  const int sign = restores ? 1 : -1;
  CHECK(ai::ApplySimulatedStatDelta(50, 100, 20, restores) == 50 + (20 * sign));
  CHECK(ai::ApplySimulatedStatDelta(50, 100, 1000, restores) ==
        (restores ? 100 : 0));
  CHECK(ai::ApplySimulatedStatDelta(50, 100, -20, restores) ==
        50 - (20 * sign));
  CHECK(ai::ApplySimulatedStatDelta(-5, 100, 0, restores) == 0);
  CHECK(ai::ApplySimulatedStatDelta(32767, 32767, sign, restores) == 32767);
}

TEST_CASE(
    "Simulated stat deltas store a negative maximum and refuse an "
    "overflowing difference",
    "[ai][planning]") {
  constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
  constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
  CHECK(ai::ApplySimulatedStatDelta(5, -3, 0, true) == -3);
  CHECK(ai::ApplySimulatedStatDelta(5, -3, 10, false) == 0);
  CHECK(ai::ApplySimulatedStatDelta(1, 100, kMax, false) == 0);
  CHECK(ai::ApplySimulatedStatDelta(-1, 100, kMax, true) == 100);
  CHECK_FALSE(ai::ApplySimulatedStatDelta(1, 100, kMax, true).has_value());
  CHECK_FALSE(ai::ApplySimulatedStatDelta(0, 100, kMin, false).has_value());
  // The original evaluates the subtraction even when it restores.
  CHECK_FALSE(ai::ApplySimulatedStatDelta(0, 100, kMin, true).has_value());
  CHECK_FALSE(ai::ApplySimulatedStatDelta(-2, 100, kMax, true).has_value());
}

TEST_CASE(
    "Candidate append stores weighted candidates and excludes their target "
    "and action",
    "[ai][planning]") {
  FakeCandidateList list;
  ai::ActionCandidate candidate;
  candidate.SetTargetId(15);
  candidate.SetAction(255);
  candidate.SetWeight(23);
  std::vector<std::optional<bool>> appended;
  std::vector<int> stored_scores;
  for (int index = 0; index < 16; ++index) {
    candidate.SetScore(index - 5);
    appended.push_back(ai::AppendActionCandidate(list, candidate));
  }
  for (const ai::ActionCandidate& entry : list.stored) {
    stored_scores.push_back(entry.Score());
  }
  CHECK(appended == std::vector<std::optional<bool>>(16, true));
  CHECK(list.count == 16);
  CHECK(stored_scores.front() == -5);
  CHECK(stored_scores.back() == 10);
  CHECK(list.stored.back().TargetId() == 15);
  CHECK(list.exclusions ==
        std::vector<std::pair<unsigned, unsigned>>(16, {15, 255}));
}

TEST_CASE("Candidate append skips a full list and a zero-weight candidate",
          "[ai][planning]") {
  FakeCandidateList list;
  list.count = 16;
  ai::ActionCandidate candidate;
  candidate.SetTargetId(9);
  candidate.SetWeight(1);
  CHECK(ai::AppendActionCandidate(list, candidate) ==
        std::optional<bool>{false});
  CHECK(list.count == 16);
  list.count = 0;
  candidate.SetWeight(0);
  CHECK(ai::AppendActionCandidate(list, candidate) ==
        std::optional<bool>{false});
  CHECK(list.count == 0);
  CHECK(list.stores == 0);
  CHECK(list.exclusions.empty());
}

TEST_CASE(
    "Candidate append refuses counts outside the list and a missing "
    "exclusion table",
    "[ai][planning]") {
  const int refusal = GENERATE(0, 1, 2);
  FakeCandidateList list;
  ai::ActionCandidate candidate;
  candidate.SetWeight(1);
  if (refusal == 0) list.count = -1;
  if (refusal == 1) list.count = 17;
  if (refusal == 2) list.allocated = false;
  const int count = list.count;
  CHECK_FALSE(ai::AppendActionCandidate(list, candidate).has_value());
  CHECK(list.count == count);
  CHECK(list.stores == 0);
  CHECK(list.exclusions.empty());
}

TEST_CASE("Candidate sort orders descending scores with the original ties",
          "[ai][planning]") {
  const std::array<int, 6> scores{2, -3, 2, 9, 9, 0};
  auto ranking = Ranking(scores);
  CHECK(ai::SortActionCandidates(ranking));
  CHECK(Handles(ranking) == std::vector<std::size_t>{4, 3, 2, 0, 5, 1});
  const std::array<int, 3> extremes{std::numeric_limits<int>::min(), 0,
                                    std::numeric_limits<int>::max()};
  auto signed_ranking = Ranking(extremes);
  CHECK(ai::SortActionCandidates(signed_ranking));
  CHECK(Handles(signed_ranking) == std::vector<std::size_t>{2, 1, 0});
  std::vector<ai::RankedCandidate> empty;
  CHECK(ai::SortActionCandidates(empty));
}

TEST_CASE("Candidate sort refuses rankings longer than the candidate list",
          "[ai][planning]") {
  std::array<int, ai::kCandidateCapacity + 1> scores{};
  for (std::size_t index = 0; index < scores.size(); ++index) {
    scores.at(index) = static_cast<int>(index);
  }
  auto ranking = Ranking(scores);
  CHECK_FALSE(ai::SortActionCandidates(ranking));
  CHECK(ranking.front().handle == 0);
  CHECK(ranking.back().handle == ai::kCandidateCapacity);
  auto full = Ranking(std::span(scores).first(ai::kCandidateCapacity));
  CHECK(ai::SortActionCandidates(full));
  CHECK(full.front().handle == ai::kCandidateCapacity - 1);
}

TEST_CASE(
    "Planning snapshots copy only the simulated stat ranges, also in place",
    "[ai][planning]") {
  std::array<std::byte, ai::kActorStatsRecordBytes> source{};
  std::array<std::byte, ai::kActorStatsRecordBytes> destination{};
  source.fill(std::byte{0x5A});
  destination.fill(std::byte{0xA5});
  ai::CopyPlanningActorStats(source, destination);
  for (std::size_t offset = 0; offset < destination.size(); ++offset) {
    CAPTURE(offset);
    CHECK(destination.at(offset) == std::byte{CopiedStatByte(offset)
                                                  ? std::uint8_t{0x5A}
                                                  : std::uint8_t{0xA5}});
  }
  for (std::size_t offset = 0; offset < source.size(); ++offset) {
    source.at(offset) = static_cast<std::byte>(offset & 0xFF);
  }
  const auto expected = source;
  ai::CopyPlanningActorStats(source, source);
  CHECK(source == expected);
}

TEST_CASE("Target eligibility requires a tracked allied or hostile actor",
          "[ai][planning]") {
  const unsigned classification = GENERATE(0u, 1u, 2u, 3u, 4u);
  const bool present = GENERATE(false, true);
  FakeTargeting host;
  host.classifications.at(3) = classification;
  host.present.at(3) = present;
  CHECK(ai::CanAiActionAffectActor(host, SlotCandidate(), 3) ==
        std::optional<bool>{present && classification >= 2});
}

TEST_CASE(
    "Beneficial actions target only non-hostile actors whose restrictions "
    "they address",
    "[ai][planning]") {
  const int condition = GENERATE(0, 1, 2);
  const unsigned flags = GENERATE(0u, 0x100u, 0x200u, 0x400u, 0x800u, 0xE00u);
  FakeTargeting host;
  host.classifications.at(3) = 2;
  host.present.at(3) = true;
  host.flags = static_cast<std::uint16_t>(flags);
  host.conditions = {.status_restricted = condition == 0,
                     .low_mp = condition == 1,
                     .low_hp = condition == 2};
  ai::ActionCandidate candidate = SlotCandidate();
  candidate.SetOption(CandidateOption::kBeneficial, true);
  const bool addressed =
      flags == 0x100 || flags == 0xE00 || (flags >> (9 + condition)) == 1;
  CHECK(ai::CanAiActionAffectActor(host, candidate, 3) ==
        std::optional<bool>{addressed});
  host.classifications.at(3) = 3;
  CHECK(ai::CanAiActionAffectActor(host, candidate, 3) ==
        std::optional<bool>{false});
}

TEST_CASE(
    "Harmful actions target only hostile actors and self-excluding slots skip "
    "the actor",
    "[ai][planning]") {
  FakeTargeting host;
  host.present.fill(true);
  host.classifications.fill(2);
  ai::ActionCandidate candidate = SlotCandidate();
  candidate.SetOption(CandidateOption::kHarmful, true);
  CHECK(ai::CanAiActionAffectActor(host, candidate, 3) ==
        std::optional<bool>{false});
  host.classifications.at(3) = 3;
  CHECK(ai::CanAiActionAffectActor(host, candidate, 3) ==
        std::optional<bool>{true});
  host.excludes_self = true;
  CHECK(ai::CanAiActionAffectActor(host, SlotCandidate(), 1) ==
        std::optional<bool>{false});
  CHECK(ai::CanAiActionAffectActor(host, SlotCandidate(), 2) ==
        std::optional<bool>{true});
}

TEST_CASE(
    "Target eligibility refuses actors and action slots outside their tables",
    "[ai][planning]") {
  FakeTargeting host;
  host.present.fill(true);
  host.classifications.fill(2);
  CHECK_FALSE(
      ai::CanAiActionAffectActor(host, SlotCandidate(), -1).has_value());
  CHECK_FALSE(
      ai::CanAiActionAffectActor(host, SlotCandidate(), 16).has_value());
  const unsigned row = GENERATE(6u, 7u);
  ai::ActionCandidate candidate = SlotCandidate();
  candidate.SetSlot(row, 0);
  CHECK_FALSE(ai::CanAiActionAffectActor(host, candidate, 3).has_value());
  host.classifications.at(3) = 1;
  CHECK(ai::CanAiActionAffectActor(host, candidate, 3) ==
        std::optional<bool>{false});
}

TEST_CASE(
    "Action usability needs a sound limb and an affordable MP, HP or PP cost",
    "[ai][planning]") {
  const unsigned type = GENERATE(0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u);
  const int row = GENERATE(0, 5);
  const int column = GENERATE(0, 3);
  FakeUsability host;
  host.cost.type = type;
  host.resources.hp = 59;
  host.resources.mp = 9;
  host.resources.weapon_pp = 9;
  const bool affordable = type != 1 && type != 3 && type != 4;
  CHECK(ai::RefreshAiActionUsability(host, row, column) ==
        std::optional<bool>{affordable});
  host.resources.hp = 60;
  host.resources.mp = 10;
  host.resources.weapon_pp = 10;
  CHECK(ai::RefreshAiActionUsability(host, row, column) ==
        std::optional<bool>{true});
  CHECK(host.updates == std::vector<SlotUpdate>{{row, column, affordable},
                                                {row, column, true}});
  CHECK(host.actions == std::vector<unsigned>{42, 42});
}

TEST_CASE("Action usability reads MP and PP as signed and HP as unsigned",
          "[ai][planning]") {
  FakeUsability host;
  host.cost.type = 1;
  host.resources.mp = 0xFFFF;
  CHECK(ai::RefreshAiActionUsability(host, 2, 1) == std::optional<bool>{false});
  host.cost.type = 4;
  host.resources.weapon_pp = 0xFFFF;
  CHECK(ai::RefreshAiActionUsability(host, 2, 1) == std::optional<bool>{false});
  host.cost.type = 3;
  host.resources.hp = 0xFFFF;
  CHECK(ai::RefreshAiActionUsability(host, 2, 1) == std::optional<bool>{true});
}

TEST_CASE("Action usability applies status and damaged-limb restrictions",
          "[ai][planning]") {
  const unsigned type = GENERATE(0u, 1u, 2u, 3u, 4u);
  FakeUsability host;
  host.cost.type = type;
  host.resources.statuses = 0x2000;
  CHECK(ai::RefreshAiActionUsability(host, 2, 1) ==
        std::optional<bool>{type == 1 || type == 2});
  host.resources.statuses = 0x1000;
  CHECK(ai::RefreshAiActionUsability(host, 2, 1) ==
        std::optional<bool>{type != 1});
  host.resources.statuses = 0;
  host.limb_hp = GENERATE(std::int16_t{1}, std::int16_t{-1});
  CHECK(ai::RefreshAiActionUsability(host, 2, 1) == std::optional<bool>{false});
  CHECK_FALSE(std::get<2>(host.updates.back()));
}

TEST_CASE(
    "Action usability refuses slots and actions outside their tables without "
    "an update",
    "[ai][planning]") {
  const auto [row, column] = GENERATE(std::pair{-1, 0}, std::pair{6, 0},
                                      std::pair{0, -1}, std::pair{0, 4});
  FakeUsability host;
  CHECK_FALSE(ai::RefreshAiActionUsability(host, row, column).has_value());
  host.action_id = ai::kActionDefinitionCount;
  CHECK_FALSE(ai::RefreshAiActionUsability(host, 2, 1).has_value());
  CHECK(host.actions.empty());
  CHECK(host.updates.empty());
}

TEST_CASE("Resource restrictions flag statuses, low MP and low HP",
          "[ai][planning]") {
  ai::ActorResources resources{.hp = 49,
                               .max_hp = 100,
                               .mp = 9,
                               .max_mp = 100,
                               .weapon_pp = 0,
                               .statuses = 0x90000};
  auto conditions = ai::RefreshAiResourceRestrictions(resources, 10);
  CHECK(Restrictions(conditions) == std::array<bool, 3>{false, true, true});
  resources.hp = resources.mp = 50;
  resources.statuses = 0x1000;
  conditions = ai::RefreshAiResourceRestrictions(resources, 10);
  CHECK(Restrictions(conditions) == std::array<bool, 3>{true, false, false});
  resources.mp = 49;
  conditions = ai::RefreshAiResourceRestrictions(resources, 0);
  CHECK(Restrictions(conditions) == std::array<bool, 3>{false, false, false});
  CHECK(ai::RefreshAiResourceRestrictions(resources, 1).low_mp);
}

TEST_CASE("Resource restrictions compare unsigned halves of HP",
          "[ai][planning]") {
  ai::ActorResources resources;
  resources.hp = resources.max_hp = 0xFFFF;
  CHECK_FALSE(ai::RefreshAiResourceRestrictions(resources, 0).low_hp);
  resources.hp = 0x7FFF;
  CHECK(ai::RefreshAiResourceRestrictions(resources, 0).low_hp);
}

TEST_CASE("Resource restrictions reject only the documented status bits",
          "[ai][planning]") {
  const unsigned bit = GENERATE(range(0u, 32u));
  const std::uint32_t status = std::uint32_t{1} << bit;
  ai::ActorResources resources;
  resources.statuses = status;
  CHECK(ai::RefreshAiResourceRestrictions(resources, 0).status_restricted ==
        ((status & 0xE2B0) != 0));
  CHECK(ai::RefreshAiResourceRestrictions(resources, 1).status_restricted ==
        ((status & 0xF2B0) != 0));
}

TEST_CASE(
    "Target memory remembers the first ranked target and the actor's tile "
    "for 900 frames",
    "[ai][planning]") {
  FakeTargetMemory host;
  CHECK(ai::RememberAiActionTarget(host) == std::optional<bool>{true});
  REQUIRE(host.remembered.has_value());
  const ai::TargetMemory memory = host.remembered.value_or(ai::TargetMemory{});
  CHECK(memory.previous_tile == 0x0FABCDEF);
  CHECK(memory.remembered_target_tile == 0xA1B2C3D4);
  CHECK(memory.frames == 900);
  CHECK(host.queried == std::vector<unsigned>{15});
}

TEST_CASE("Target memory skips empty rankings and disabled memory",
          "[ai][planning]") {
  const int count = GENERATE(-1, 0, 1, 16);
  FakeTargetMemory host;
  host.count = count;
  host.disabled = count > 0;
  host.first_target.reset();
  CHECK(ai::RememberAiActionTarget(host) == std::optional<bool>{false});
  CHECK_FALSE(host.remembered.has_value());
}

TEST_CASE(
    "Target memory refuses counts past the list, an empty first entry or an "
    "unplaced target",
    "[ai][planning]") {
  const int refusal = GENERATE(0, 1, 2, 3);
  FakeTargetMemory host;
  if (refusal == 0) host.count = 17;
  if (refusal == 1) host.first_target.reset();
  if (refusal == 2) host.first_target = 16;
  if (refusal == 3) host.target_tile.reset();
  CHECK_FALSE(ai::RememberAiActionTarget(host).has_value());
  CHECK_FALSE(host.remembered.has_value());
  CHECK(host.queried.size() == (refusal == 3 ? 1u : 0u));
}
