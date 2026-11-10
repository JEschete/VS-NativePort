#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
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

class FakeCandidateSimulation final : public ai::CandidateSimulationHost {
 public:
  std::optional<VS::Battle::TargetPoint> TargetPosition(
      unsigned target) const override {
    events.emplace_back("position");
    requested_target = target;
    return position;
  }
  std::optional<bool> PreviewLimb(
      const ai::ActionCandidate& candidate) override {
    events.emplace_back("limb");
    preview = candidate;
    return available;
  }
  std::optional<bool> PreviewAimed(
      const ai::ActionCandidate& candidate,
      const VS::Battle::TargetPoint& source,
      const VS::Battle::TargetPoint& target) override {
    events.emplace_back("aimed");
    preview = candidate;
    source_position = source;
    target_position = target;
    return available;
  }
  bool ApplyResults() override {
    events.emplace_back("apply");
    return apply;
  }

  std::optional<VS::Battle::TargetPoint> position;
  std::optional<bool> available = true;
  bool apply = true;
  mutable unsigned requested_target = 0;
  mutable std::vector<std::string> events;
  ai::ActionCandidate preview;
  VS::Battle::TargetPoint source_position{};
  VS::Battle::TargetPoint target_position{};
};

class FakeCandidateEvaluation final : public ai::CandidateEvaluationHost {
 public:
  int Random() override {
    events.emplace_back("random");
    return random;
  }
  bool Capture(ai::ActionCandidate& candidate) override {
    events.emplace_back("capture");
    captured = candidate;
    return capture;
  }
  bool SpecialMode() const override { return special; }
  bool Simulate(ai::ActionCandidate&,
                ai::CandidateSimulationMode mode) override {
    events.emplace_back("simulate");
    simulation_mode = mode;
    return simulate;
  }
  std::optional<std::span<const ai::SimulatedActorEffect>> Hits() override {
    events.emplace_back("hits");
    if (!have_hits) return std::nullopt;
    return std::span<const ai::SimulatedActorEffect>(hits);
  }
  std::optional<std::span<const ai::CandidateCondition>> Conditions() override {
    events.emplace_back("conditions");
    if (!have_conditions) return std::nullopt;
    return std::span<const ai::CandidateCondition>(conditions);
  }

  int random = 0;
  bool capture = true;
  bool special = false;
  bool simulate = true;
  bool have_hits = true;
  bool have_conditions = true;
  ai::ActionCandidate captured;
  std::optional<ai::CandidateSimulationMode> simulation_mode;
  std::vector<std::string> events;
  std::vector<ai::SimulatedActorEffect> hits;
  std::vector<ai::CandidateCondition> conditions;
};

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

class FakeFallbackSelection final : public ai::FallbackActionHost {
 public:
  std::optional<ai::FallbackActionSlot> RefreshSlot(int row,
                                                    int column) override {
    calls.emplace_back(row, column);
    if (visible_choice != nullptr) {
      observed.push_back(visible_choice->RawWords());
    }
    const int index = row * ai::kLimbActionColumns + column;
    if (index == failed_slot) return std::nullopt;
    return slots[static_cast<std::size_t>(index)];
  }
  bool IgnoresSight(unsigned action) const override {
    definition_reads.push_back(action);
    return ignores_sight;
  }

  std::array<ai::FallbackActionSlot,
             static_cast<std::size_t>(ai::kLimbActionRows) *
                 ai::kLimbActionColumns>
      slots{};
  std::vector<std::pair<int, int>> calls;
  mutable std::vector<unsigned> definition_reads;
  const ai::ActionCandidate* visible_choice = nullptr;
  std::vector<Words> observed;
  int failed_slot = -1;
  bool ignores_sight = false;
};

Words FallbackWords(Words words, unsigned row, unsigned column, unsigned action,
                    std::uint32_t range, bool ignores_sight) {
  words[1] = (words[1] & ~0x1FFFu) | row | (column << 3) | (action << 5);
  words[2] = range;
  words[3] = (words[3] & ~0x07F80000u) | 0x04000000u;
  words[4] = (words[4] & ~15u) | (ignores_sight ? 4u : 0u);
  return words;
}

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

TEST_CASE(
    "Fallback selection refreshes every slot and keeps the first unsigned "
    "maximum",
    "[ai][planning][fallback-selection]") {
  const auto range = GENERATE(1u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu);
  const bool ignores_sight = GENERATE(false, true);
  const Words before{0xDEADBEEF, 0xABCDEF01, 0x76543210, 0xFFFFFFFF,
                     0xABCDEF0F};
  ai::ActionCandidate choice(before);
  FakeFallbackSelection host;
  host.ignores_sight = ignores_sight;
  host.slots[0] = {.action = 42, .range_squared = 1, .eligible = true};
  host.slots[7] = {.action = 43, .range_squared = range, .eligible = true};
  host.slots[8] = {.action = 44, .range_squared = range, .eligible = true};
  host.slots[23] = {.action = 45, .range_squared = 0xFFFFFFFFu};
  REQUIRE(ai::SelectFallbackAction(choice, host));
  CHECK(choice.RawWords() ==
        FallbackWords(before, range == 1 ? 0u : 1u, range == 1 ? 0u : 3u,
                      range == 1 ? 42u : 43u, range, ignores_sight));
  REQUIRE(host.calls.size() == 24);
  for (std::size_t index = 0; index < host.calls.size(); ++index) {
    CHECK(host.calls[index].first == static_cast<int>(index / 4));
    CHECK(host.calls[index].second == static_cast<int>(index % 4));
  }
  const std::vector<unsigned> reads =
      range == 1 ? std::vector<unsigned>{42} : std::vector<unsigned>{42, 43};
  CHECK(host.definition_reads == reads);
}

TEST_CASE(
    "Fallback selection without a positive eligible range clears only the "
    "action",
    "[ai][planning][fallback-selection]") {
  const bool eligible = GENERATE(false, true);
  const Words before{0x12345678, 0xFFFFFFFF, 0xABCDEF01, 0xDEADBEEF,
                     0xFFFFFFFF};
  ai::ActionCandidate choice(before);
  FakeFallbackSelection host;
  for (auto& slot : host.slots) {
    slot = {.action = 256,
            .range_squared = eligible ? 0u : 0xFFFFFFFFu,
            .eligible = eligible};
  }
  REQUIRE(ai::SelectFallbackAction(choice, host));
  auto expected = before;
  expected[1] &= ~0x1FE0u;
  CHECK(choice.RawWords() == expected);
  CHECK(host.calls.size() == 24);
  CHECK(host.definition_reads.empty());
}

TEST_CASE(
    "Fallback selection refuses missing slots and out-of-table winning actions",
    "[ai][planning][fallback-selection]") {
  const bool missing = GENERATE(false, true);
  const Words before{0x12345678, 0xFFFFFFFF, 0xABCDEF01, 0xDEADBEEF,
                     0xFFFFFFFF};
  ai::ActionCandidate choice(before);
  FakeFallbackSelection host;
  host.slots[0] = {.action = 256, .range_squared = 1, .eligible = true};
  host.failed_slot = missing ? 0 : -1;
  CHECK_FALSE(ai::SelectFallbackAction(choice, host));
  CHECK(choice.RawWords() == before);
  CHECK(host.calls.size() == 1);
  CHECK(host.definition_reads.empty());
}

TEST_CASE(
    "Fallback selection exposes each winner before refreshing the next slot",
    "[ai][planning][fallback-selection]") {
  const Words before{0xFFFFFFFF, 0x12345678, 0xABCDEF01, 0xDEADBEEF,
                     0xFFFFFFFF};
  ai::ActionCandidate choice(before);
  FakeFallbackSelection host;
  host.visible_choice = &choice;
  host.slots[0] = {.action = 42, .range_squared = 1, .eligible = true};
  host.slots[5] = {.action = 43, .range_squared = 2, .eligible = true};
  REQUIRE(ai::SelectFallbackAction(choice, host));
  REQUIRE(host.observed.size() == 24);
  CHECK(host.observed[0] == before);
  for (std::size_t index = 1; index < host.observed.size(); ++index) {
    CHECK(host.observed[index] ==
          (index <= 5 ? FallbackWords(before, 0, 0, 42, 1, false)
                      : FallbackWords(before, 1, 1, 43, 2, false)));
  }
}

TEST_CASE(
    "Fallback selection reports a failed later refresh without hiding earlier "
    "writes",
    "[ai][planning][fallback-selection]") {
  const Words before{0xFFFFFFFF, 0x12345678, 0xABCDEF01, 0xDEADBEEF,
                     0xFFFFFFFF};
  ai::ActionCandidate choice(before);
  FakeFallbackSelection host;
  host.slots[0] = {.action = 42, .range_squared = 9, .eligible = true};
  host.failed_slot = 3;
  CHECK_FALSE(ai::SelectFallbackAction(choice, host));
  CHECK(host.calls.size() == 4);
  CHECK(choice.RawWords() == FallbackWords(before, 0, 0, 42, 9, false));
  CHECK(host.definition_reads == std::vector<unsigned>{42});
}

TEST_CASE("Planning stats capture preserves sparse slots and untouched bytes",
          "[ai][planning][stats-lifecycle]") {
  std::array<std::byte, ai::kPlanningStatsTableBytes> storage{};
  storage.fill(std::byte{0xCC});
  std::array<std::byte, ai::kActorStatsRecordBytes> first{}, second{};
  first.fill(std::byte{0x21});
  second.fill(std::byte{0x43});
  const std::array<ai::PlanningActorStats, 2> actors{
      {{.slot = 15, .bytes = first}, {.slot = 3, .bytes = second}}};
  ai::PlanningStatsSnapshot snapshot{.bytes = storage};
  REQUIRE(ai::SnapshotPlanningActorStats(snapshot, actors));
  for (std::size_t slot = 0; slot < snapshot.captured_slots.size(); ++slot) {
    CHECK(snapshot.captured_slots[slot] == (slot == 15 || slot == 3));
    for (std::size_t offset = 0; offset < first.size(); ++offset) {
      const auto expected =
          CopiedStatByte(offset) && slot == 15  ? std::byte{0x21}
          : CopiedStatByte(offset) && slot == 3 ? std::byte{0x43}
                                                : std::byte{0xCC};
      CHECK(storage[slot * first.size() + offset] == expected);
    }
  }
  first.fill(std::byte{0x75});
  second.fill(std::byte{0x75});
  const std::array<ai::PlanningActorStats, 1> remaining{
      {{.slot = 3, .bytes = first}}};
  REQUIRE(ai::RestorePlanningActorStats(snapshot, remaining));
  for (std::size_t offset = 0; offset < first.size(); ++offset) {
    CHECK(first[offset] ==
          (CopiedStatByte(offset) ? std::byte{0x43} : std::byte{0x75}));
    CHECK(second[offset] == std::byte{0x75});
  }
}

TEST_CASE("Planning stats capture keeps the last repeated slot and empty lists",
          "[ai][planning][stats-lifecycle]") {
  std::array<std::byte, ai::kPlanningStatsTableBytes> storage{};
  std::array<std::byte, ai::kActorStatsRecordBytes> first{}, second{};
  first.fill(std::byte{1});
  second.fill(std::byte{2});
  const std::array<ai::PlanningActorStats, 2> actors{
      {{.slot = 0, .bytes = first}, {.slot = 0, .bytes = second}}};
  ai::PlanningStatsSnapshot snapshot{.bytes = storage};
  REQUIRE(ai::SnapshotPlanningActorStats(snapshot, actors));
  CHECK(storage[0x18] == std::byte{2});
  REQUIRE(ai::SnapshotPlanningActorStats(snapshot, {}));
  for (const bool captured : snapshot.captured_slots) CHECK_FALSE(captured);
  CHECK(storage[0x18] == std::byte{2});
  CHECK(ai::RestorePlanningActorStats(snapshot, {}));
}

TEST_CASE(
    "Planning stats capture validates all rows before changing its snapshot",
    "[ai][planning][stats-lifecycle]") {
  const int failure = GENERATE(0, 1, 2, 3);
  std::array<std::byte, ai::kPlanningStatsTableBytes> storage{};
  storage.fill(std::byte{0xCC});
  const auto before = storage;
  std::array<std::byte, ai::kActorStatsRecordBytes> bytes{};
  std::vector<ai::PlanningActorStats> actors{{.slot = 1, .bytes = bytes},
                                             {.slot = 2, .bytes = bytes}};
  ai::PlanningStatsSnapshot snapshot{.bytes = storage};
  snapshot.captured_slots.fill(true);
  if (failure == 0) actors.back().slot = 16;
  if (failure == 1) {
    actors.back().bytes = std::span(bytes).first(bytes.size() - 1);
  }
  if (failure == 2) {
    snapshot.bytes = std::span(storage).first(storage.size() - 1);
  }
  if (failure == 3) actors.resize(17, actors.front());
  CHECK_FALSE(ai::SnapshotPlanningActorStats(snapshot, actors));
  CHECK(storage == before);
  for (const bool captured : snapshot.captured_slots) CHECK(captured);
}

TEST_CASE(
    "Planning stats restore refuses uncaptured or malformed rows before writes",
    "[ai][planning][stats-lifecycle]") {
  const int failure = GENERATE(0, 1, 2, 3);
  std::array<std::byte, ai::kPlanningStatsTableBytes> storage{};
  std::array<std::byte, ai::kActorStatsRecordBytes> first{}, second{};
  first.fill(std::byte{0x23});
  second.fill(std::byte{0x45});
  const auto first_before = first;
  const auto second_before = second;
  std::array<ai::PlanningActorStats, 2> actors{
      {{.slot = 1, .bytes = first}, {.slot = 3, .bytes = second}}};
  ai::PlanningStatsSnapshot snapshot{.bytes = storage};
  snapshot.captured_slots.fill(true);
  if (failure == 0) actors.back().slot = 16;
  if (failure == 1) {
    actors.back().bytes = std::span(second).first(second.size() - 1);
  }
  if (failure == 2) {
    snapshot.bytes = std::span(storage).first(storage.size() - 1);
  }
  if (failure == 3) snapshot.captured_slots[3] = false;
  CHECK_FALSE(ai::RestorePlanningActorStats(snapshot, actors));
  CHECK(first == first_before);
  CHECK(second == second_before);
}

TEST_CASE("Candidate aim fields preserve every unrelated packed bit",
          "[ai][planning][candidate-evaluation]") {
  const Words words{0xDEADBEEF, 0x12345678, 0x87654321, 0xA5A5A5A5, 0xDEADC0DE};
  ai::ActionCandidate candidate(words);
  candidate.SetTargetLimb(14);
  candidate.SetHeightOffset(0x1ABC);
  auto expected = words;
  expected[3] = (expected[3] & ~0x7FFF0u) | (6u << 4) | (0xABCu << 7);
  CHECK(candidate.TargetLimb() == 6);
  CHECK(candidate.HeightOffset() == 0xABC);
  CHECK(candidate.RawWords() == expected);
}

TEST_CASE("Candidate simulation applies results only for an available preview",
          "[ai][planning][candidate-evaluation]") {
  FakeCandidateSimulation host;
  ai::ActionCandidate candidate;
  candidate.SetAction(42);
  candidate.SetTargetId(3);
  candidate.SetTargetLimb(5);
  host.available = GENERATE(false, true);
  REQUIRE(ai::SimulateActionCandidate(host, candidate,
                                      ai::CandidateSimulationMode::kLimb));
  const std::vector<std::string> expected =
      *host.available ? std::vector<std::string>{"limb", "apply"}
                      : std::vector<std::string>{"limb"};
  CHECK(host.events == expected);
  CHECK(host.preview.RawWords() == candidate.RawWords());
}

TEST_CASE("Aimed candidate geometry wraps the original halfword coordinates",
          "[ai][planning][candidate-evaluation]") {
  FakeCandidateSimulation host;
  host.position = VS::Battle::TargetPoint{32767, -32760, 32760, 99};
  ai::ActionCandidate candidate;
  candidate.SetTargetId(15);
  candidate.SetHeightOffset(4095);
  candidate.SetDistanceSquared(256);
  REQUIRE(ai::SimulateActionCandidate(host, candidate,
                                      ai::CandidateSimulationMode::kAimed));
  CHECK(host.requested_target == 15);
  CHECK(host.target_position ==
        VS::Battle::TargetPoint{32767, 28681, 32760, 0});
  CHECK(host.source_position ==
        VS::Battle::TargetPoint{32767, 28681, -32760, 0});
  CHECK(host.events == std::vector<std::string>{"position", "aimed", "apply"});
}

TEST_CASE("Candidate simulation explicitly refuses missing inputs and results",
          "[ai][planning][candidate-evaluation]") {
  FakeCandidateSimulation host;
  ai::ActionCandidate candidate;
  CHECK_FALSE(ai::SimulateActionCandidate(host, candidate,
                                          ai::CandidateSimulationMode::kAimed));
  CHECK(host.events == std::vector<std::string>{"position"});
  host.events.clear();
  host.available.reset();
  CHECK_FALSE(ai::SimulateActionCandidate(host, candidate,
                                          ai::CandidateSimulationMode::kLimb));
  CHECK(host.events == std::vector<std::string>{"limb"});
  host.events.clear();
  host.available = true;
  host.apply = false;
  CHECK_FALSE(ai::SimulateActionCandidate(host, candidate,
                                          ai::CandidateSimulationMode::kLimb));
  CHECK(host.events == std::vector<std::string>{"limb", "apply"});
}

TEST_CASE("Candidate hit weights exhaust every byte threshold and target slot",
          "[ai][planning][candidate-evaluation]") {
  for (unsigned target = 0; target < 16; ++target) {
    for (unsigned threshold = 0; threshold < 256; ++threshold) {
      ai::ActionCandidate candidate(
          Words{0x12345678, 0x89ABCDEF, 0x98765432, 0xFEDCBA98, 0x10203040});
      candidate.SetTargetId(target);
      auto expected = candidate.RawWords();
      const auto weight = (threshold + threshold / 4 + threshold / 32) & 255;
      expected[3] = (expected[3] & ~0x7F80000u) | (weight << 19);
      const std::array hits{ai::SimulatedActorEffect{
          .actor_id = static_cast<std::uint8_t>(target),
          .hit_threshold = static_cast<std::uint8_t>(threshold)}};
      ai::UpdateCandidateHitWeight(candidate, hits);
      REQUIRE(candidate.RawWords() == expected);
    }
  }
}

TEST_CASE("Candidate hit weights use the first nonignored matching target",
          "[ai][planning][candidate-evaluation]") {
  ai::ActionCandidate candidate;
  candidate.SetTargetId(3);
  candidate.SetWeight(99);
  const std::array hits{
      ai::SimulatedActorEffect{
          .actor_id = 3, .ignored = true, .hit_threshold = 127},
      ai::SimulatedActorEffect{.actor_id = 2, .hit_threshold = 128},
      ai::SimulatedActorEffect{.actor_id = 3, .hit_threshold = 0},
      ai::SimulatedActorEffect{.actor_id = 3, .hit_threshold = 255}};
  ai::UpdateCandidateHitWeight(candidate, hits);
  CHECK(candidate.Weight() == 0);
  ai::UpdateCandidateHitWeight(candidate, std::span(hits).last(1));
  CHECK(candidate.Weight() == 69);
  ai::UpdateCandidateHitWeight(candidate, {});
  CHECK(candidate.Weight() == 0);
}

TEST_CASE("Rare candidate rejection preserves all fields except the score",
          "[ai][planning][candidate-evaluation]") {
  const int roll = GENERATE(0, 114, 115, 127, 128, -1);
  FakeCandidateEvaluation host;
  host.random = roll;
  const Words words{123, 0xDEADBEEF, 0x98765432, 0xFEDCBA98, 0xABCDEF01};
  ai::ActionCandidate candidate(words);
  REQUIRE(
      ai::EvaluateActionCandidate(candidate, {.rarely_chosen = true}, host));
  if ((roll & 127) < 115) {
    auto expected = words;
    expected[0] = 0x80000000;
    CHECK(candidate.RawWords() == expected);
    CHECK(host.events == std::vector<std::string>{"random"});
  } else {
    CHECK(candidate.Score() == 128);
    CHECK(candidate.Weight() == 128);
    CHECK(host.events ==
          std::vector<std::string>{"random", "capture", "conditions"});
  }
}

TEST_CASE("Candidate evaluation orders capture simulation and hit weighting",
          "[ai][planning][candidate-evaluation]") {
  FakeCandidateEvaluation host;
  host.hits.push_back({.actor_id = 3, .hit_threshold = 127});
  ai::ActionCandidate candidate;
  candidate.SetScore(999);
  candidate.SetWeight(17);
  candidate.SetTargetId(3);
  REQUIRE(ai::EvaluateActionCandidate(
      candidate, {.simulation = ai::CandidateSimulationMode::kAimed}, host));
  CHECK(host.captured.Score() == 0);
  CHECK(host.captured.Weight() == 128);
  CHECK(host.simulation_mode == ai::CandidateSimulationMode::kAimed);
  CHECK(host.events ==
        std::vector<std::string>{"capture", "simulate", "hits", "conditions"});
  CHECK(candidate.Score() == 161);
  CHECK(candidate.Weight() == 161);
}

TEST_CASE(
    "Special candidate evaluation skips simulation and adds its preference",
    "[ai][planning][candidate-evaluation]") {
  FakeCandidateEvaluation host;
  host.special = true;
  ai::ActionCandidate candidate;
  REQUIRE(ai::EvaluateActionCandidate(
      candidate,
      {.special_action = true,
       .simulation = ai::CandidateSimulationMode::kAimed},
      host));
  CHECK(host.events == std::vector<std::string>{"capture", "conditions"});
  CHECK(candidate.Score() == 168);
  CHECK(candidate.Weight() == 40);
}

TEST_CASE("Candidate evaluation scores relations and narrows resource ratios",
          "[ai][planning][candidate-evaluation]") {
  FakeCandidateEvaluation host;
  host.conditions = {
      {.condition = {.hp = 100, .max_hp = 100}, .classification = 2},
      {.condition = {.hp = 50, .max_hp = 100}, .classification = 3},
      {.condition = {.hp = 100, .max_hp = 100}, .classification = 1},
      {.condition = {.hp = 25, .max_hp = 100}, .classification = 0}};
  ai::ActionCandidate candidate;
  REQUIRE(ai::EvaluateActionCandidate(candidate, {}, host));
  // Ally 512; hostile -256; ignored zero; low-HP neutral -(-504 + 128).
  CHECK(candidate.Score() == (512 - 256 + 376) * 128 + 128);
  host.conditions = {
      {.condition = {.hp = 257, .max_hp = 1}, .classification = 2}};
  REQUIRE(ai::EvaluateActionCandidate(candidate, {}, host));
  CHECK(candidate.Score() == (-32640 * 4) * 128 + 128);
}

TEST_CASE("Candidate evaluation refuses failed capture simulation or decoding",
          "[ai][planning][candidate-evaluation]") {
  const int failure = GENERATE(0, 1, 2, 3, 4);
  FakeCandidateEvaluation host;
  if (failure == 0) host.capture = false;
  if (failure == 1) host.simulate = false;
  if (failure == 2) host.have_hits = false;
  if (failure == 3) host.have_conditions = false;
  if (failure == 4) host.conditions.resize(17);
  ai::ActionCandidate candidate;
  CHECK_FALSE(ai::EvaluateActionCandidate(
      candidate, {.simulation = ai::CandidateSimulationMode::kLimb}, host));
}

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
