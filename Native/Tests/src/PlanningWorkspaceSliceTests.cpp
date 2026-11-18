// Hand-maintained tests for the typed AI plan rebuild slice (#247, #255).
#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/PlanningWorkspace.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Content/ModuleData.h"

namespace {
using VS::Battle::Ai::ActionPlanningRoom;
using VS::Battle::Ai::PlanningActor;
using VS::Battle::Ai::PlanningRequest;
namespace ai = VS::Battle::Ai;

ai::ActionCandidate PlanCandidate(int score, unsigned threshold = 0) {
  ai::ActionCandidate candidate(
      {0, 0xE0000000, 0x81234567, 0xF8000000, 0xFF400000});
  candidate.SetScore(score);
  candidate.SetSlot(2, 1);
  candidate.SetAction(static_cast<unsigned>(score) & 255);
  candidate.SetTargetId(static_cast<unsigned>(score) & 15);
  candidate.SetThreshold(threshold);
  candidate.SetWeight(17);
  return candidate;
}

class RecordingActorPlan final : public ai::ActorActionPlanHost,
                                 public ai::CandidateList {
 public:
  int count = 5;
  std::uint8_t actor_id = 1;
  bool snapshot = true, clear_ok = true, allow_fallback = false;
  std::optional<ai::ActionPlanTiming> timing = ai::ActionPlanTiming{11, 350};
  std::array<ai::ActionCandidate, 16> candidates{};
  std::array<std::size_t, 16> order{};
  std::array<std::vector<ai::ActionCandidate>, 3> results;
  std::array<std::array<ai::ActionCandidate, 16>, 3> passes{};
  std::array<int, 3> saved{};
  std::vector<ai::ActionCandidate> fallback;
  int fallback_count = 0, emitted_fallback_count = -1;
  int enumerations = 0, sorts = 0, restores = 0, resets = 0;
  int promoted = 0, movement = 0, mode = 0, draw = 0;
  int invalid_count = -1;
  bool invalidate_count = false, missing_timing = false;
  bool missing_rank = false, missing_pass = false, fail_store = false;
  bool fail_exclusion = false;
  std::vector<int> advances;
  std::vector<unsigned> thresholds;
  std::vector<std::pair<unsigned, unsigned>> released;
  std::vector<std::string> log;
  bool arena = false, plan_saved = false;
  std::uint8_t progress = 4, saved_count = 0;
  std::optional<ai::ActionPlanCommand> command = ai::ActionPlanCommand{24, 4};
  std::optional<std::uint16_t> subtype = 172;
  std::function<void()> on_advance, on_random, on_readiness, on_movement;
  std::function<void()> on_progress, on_set_progress;

  RecordingActorPlan() {
    for (std::size_t index = 0; index < order.size(); ++index) {
      order[index] = index;
    }
    Fill(0, 12);
    Fill(1, 6);
    Fill(2, 7);
  }
  void Fill(std::size_t pass, int size) {
    results[pass].clear();
    for (int index = 0; index < size; ++index) {
      results[pass].push_back(
          PlanCandidate(static_cast<int>(pass) * 100 + index + 1));
    }
  }
  ai::CandidateList& Candidates() override { return *this; }
  int Count() const override { return count; }
  void Store(std::size_t slot, const ai::ActionCandidate& candidate) override {
    candidates[slot] = candidate;
    order[slot] = slot;
  }
  void SetCount(int next) override { count = next; }
  bool ExcludeTargetAction(unsigned, unsigned) override {
    throw std::logic_error("a plan copy must not append an exclusion");
  }
  void ResetFallbackCache() override {
    log.emplace_back("cache");
    fallback_count = 0;
  }
  bool ClearConfiguredExclusions() override {
    log.emplace_back("exclusions");
    return clear_ok;
  }
  std::uint8_t ActorId() const override { return actor_id; }
  bool SnapshotStats() override {
    log.emplace_back("snapshot");
    return snapshot;
  }
  void RestoreStats() noexcept override { ++restores; }
  void ResetReadiness() noexcept override { ++resets; }
  std::optional<ai::ActionPlanTiming> Timing() const override {
    return missing_timing ? std::nullopt : timing;
  }
  void AdvanceSimulation(int step) override {
    advances.push_back(step);
    if (on_advance) on_advance();
  }
  void Enumerate(std::uint16_t threshold) override {
    thresholds.push_back(threshold);
    const auto& source = results[static_cast<std::size_t>(enumerations++)];
    count = static_cast<int>(source.size());
    for (std::size_t index = 0; index < source.size(); ++index) {
      Store(index, source[index]);
    }
    fallback_count = emitted_fallback_count < 0
                         ? static_cast<int>(fallback.size())
                         : emitted_fallback_count;
    if (invalidate_count) count = invalid_count;
  }
  void Sort() override { ++sorts; }
  std::optional<ai::ActionCandidate> Ranked(std::size_t rank) const override {
    if (missing_rank) return std::nullopt;
    return candidates[order[rank]];
  }
  bool StorePass(std::size_t pass, std::size_t rank,
                 const ai::ActionCandidate& candidate) override {
    if (fail_store) return false;
    passes[pass][rank] = candidate;
    ++saved[pass];
    return true;
  }
  std::optional<ai::ActionCandidate> Pass(std::size_t pass,
                                          std::size_t rank) const override {
    if (missing_pass) return std::nullopt;
    return passes[pass][rank];
  }
  bool ClearExclusion(unsigned target, unsigned action) override {
    released.emplace_back(target, action);
    return !fail_exclusion;
  }
  bool AllowsFallback() const override { return allow_fallback; }
  int FallbackCount() const override { return fallback_count; }
  std::optional<ai::ActionCandidate> Fallback(std::size_t rank) const override {
    return fallback[rank];
  }
  void Promote(int size) override {
    log.emplace_back("promote");
    promoted = size;
    REQUIRE(restores == 1);
    REQUIRE(resets == 2);
  }
  bool SetRankedThreshold(std::size_t rank, std::uint16_t threshold) override {
    candidates[order[rank]].SetThreshold(threshold);
    return true;
  }
  int Random() override {
    if (on_random) on_random();
    return draw;
  }
  void RefreshReadiness() override {
    log.emplace_back("readiness");
    if (on_readiness) on_readiness();
  }
  void SetPlanMode(std::uint8_t value) override { mode = value; }
  void SelectMovement() override {
    ++movement;
    if (on_movement) on_movement();
  }
  bool BeginArenaAdjustment() override {
    log.emplace_back("arena");
    return arena;
  }
  std::uint8_t ArenaProgress() const override {
    if (on_progress) on_progress();
    return progress;
  }
  std::optional<ai::ActionPlanCommand> ArenaCommand() const override {
    return command;
  }
  void SetArenaProgress(std::uint8_t next) override {
    progress = next;
    if (on_set_progress) on_set_progress();
  }
  bool ArenaPlanSaved() const override { return plan_saved; }
  std::optional<std::uint16_t> ArenaActorSubtype() const override {
    return subtype;
  }
  void SetSavedPlanCount(std::uint8_t size) override { saved_count = size; }
};

class RecordingRoom final : public ActionPlanningRoom {
 public:
  std::vector<PlanningActor> actors{{0, 1, 40}, {3, 7, 25}};
  std::set<int> plans_using_room_delay;
  std::set<int> suppressed_indicators;
  int defeat_on_relationship_slot = -1;
  std::vector<std::string> log;

  void ResetReadiness() override { log.emplace_back("readiness"); }
  void ReconcileCompletedPlan() override { log.emplace_back("reconcile"); }
  void ApplyRelationshipChanges() override {
    log.emplace_back("relationships");
    for (auto& actor : actors) {
      if (actor.slot == defeat_on_relationship_slot) {
        actor.hp = 0;
      }
    }
  }
  [[nodiscard]] std::vector<PlanningActor> Actors() const override {
    return actors;
  }
  void PrepareActorPlan(const PlanningActor& actor) override {
    log.push_back("prepare " + std::to_string(actor.slot));
  }
  void ClearReengagementGrace(const PlanningActor& actor) override {
    log.push_back("grace " + std::to_string(actor.slot));
  }
  void SetDefeated(const PlanningActor& actor, bool defeated) override {
    log.push_back("defeated " + std::to_string(actor.slot) + " " +
                  (defeated ? "1" : "0"));
  }
  [[nodiscard]] bool BuildActorPlan(const PlanningActor& actor) override {
    log.push_back("build " + std::to_string(actor.slot));
    return plans_using_room_delay.contains(actor.slot);
  }
  [[nodiscard]] bool SuppressesAwarenessIndicator(
      const PlanningActor& actor) const override {
    return suppressed_indicators.contains(actor.slot);
  }
  void ResetAwarenessIndicator(const PlanningActor& actor) override {
    log.push_back("reset awareness " + std::to_string(actor.slot));
  }
  void SetAwarenessIndicator(const PlanningActor& actor, int frames) override {
    log.push_back("set awareness " + std::to_string(actor.actor_id) + " " +
                  std::to_string(frames));
  }

  [[nodiscard]] int Count(const std::string& entry) const {
    int count = 0;
    for (const auto& line : log) {
      count += line == entry ? 1 : 0;
    }
    return count;
  }
};

ai::ActionCandidate CompletedCandidate(unsigned action, unsigned target) {
  auto candidate = PlanCandidate(1);
  candidate.SetSlot(2, 1);
  candidate.SetAction(action);
  candidate.SetTargetId(target);
  return candidate;
}

class RecordingCompletedPlan final : public ai::CompletedActionPlanHost {
 public:
  // Row 2 in the low half and column 1 in the high half.
  static constexpr std::int32_t kSlotArgument = (1 << 16) | 2;

  std::optional<bool> performer = true;
  std::optional<ai::ActionPlanCommand> command =
      ai::ActionPlanCommand{24, kSlotArgument};
  int count = 1;
  std::vector<std::optional<ai::ActionCandidate>> ranked{
      CompletedCandidate(5, 9)};
  std::vector<std::optional<ai::CompletedActionTarget>> targets;
  std::array<std::optional<ai::CompletedTargetObject>, 16> objects{};
  int detour = -1;
  mutable std::vector<std::string> log;

  RecordingCompletedPlan() {
    objects.fill(ai::CompletedTargetObject{.present = true, .kind = 0});
  }

  std::optional<bool> SelectPerformer(unsigned slot) override {
    log.push_back("select " + std::to_string(slot));
    return performer;
  }
  std::optional<ai::ActionPlanCommand> PerformerCommand() const override {
    log.push_back("command");
    return command;
  }
  int CandidateCount() const override { return count; }
  std::optional<ai::ActionCandidate> Ranked(std::size_t rank) const override {
    log.push_back("rank " + std::to_string(rank));
    return rank < ranked.size() ? ranked[rank] : std::nullopt;
  }
  void SetDetourTarget(std::uint8_t target) override {
    detour = target;
    log.push_back("detour " + std::to_string(target));
  }
  int TargetCount() const override { return static_cast<int>(targets.size()); }
  std::optional<ai::CompletedActionTarget> Target(
      std::size_t index) const override {
    log.push_back("target " + std::to_string(index));
    return index < targets.size() ? targets[index] : std::nullopt;
  }
  std::optional<ai::CompletedTargetObject> TargetObject(
      unsigned slot) const override {
    log.push_back("object " + std::to_string(slot));
    return slot < objects.size() ? objects[slot] : std::nullopt;
  }
  void ForgetTarget() override { log.push_back("forget"); }
  void RememberTarget() override { log.push_back("remember"); }
};

ai::CompletedAction CompletedActionOf(std::uint16_t action_id) {
  return {.action_id = action_id, .performer = 3};
}

ai::CompletedActionTarget CompletedTargetAt(std::uint8_t slot,
                                            bool targets_position = false) {
  return {.slot = slot, .targets_position = targets_position};
}

}  // namespace

TEST_CASE("Actor plan resets exclusions and counts before the zero actor exit",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  host.actor_id = 0;
  host.fallback_count = 9;
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.log == std::vector<std::string>{"cache", "exclusions"});
  CHECK(host.count == 0);
  CHECK(host.fallback_count == 0);
  CHECK(host.enumerations == 0);
  CHECK(host.restores == 0);
}

TEST_CASE("Actor plan preserves cumulative passes quotas and merge order",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.advances == std::vector<int>{11, 455, 2900});
  CHECK(host.thresholds == std::vector<unsigned>{11, 455, 2900});
  CHECK(host.saved == std::array<int, 3>{10, 3, 3});
  CHECK(host.count == 16);
  CHECK(host.promoted == 10);
  std::vector<int> scores;
  for (const auto& candidate : host.candidates) {
    scores.push_back(candidate.Score());
  }
  CHECK(scores == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 101, 102, 103,
                                   201, 202, 203});
}

TEST_CASE("Actor plan releases only rejected target action pairs",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  REQUIRE(ai::BuildActorActionPlan(host));
  std::vector<std::pair<unsigned, unsigned>> expected;
  for (const int score : {11, 12, 104, 105, 106, 204, 205, 206, 207}) {
    expected.emplace_back(static_cast<unsigned>(score) & 15, score);
  }
  CHECK(host.released == expected);
  CHECK(host.sorts == 3);
  CHECK(host.movement == 1);
}

TEST_CASE(
    "Actor plan reserves the last quota and gives spare capacity to the middle",
    "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  host.Fill(0, 2);
  host.Fill(1, 16);
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.saved == std::array<int, 3>{2, 11, 3});
  CHECK(host.count == 16);
  CHECK(host.promoted == 2);
}

TEST_CASE("Actor plan derives minimum delay from unsigned recovery ticks",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  host.timing = ai::ActionPlanTiming{255, 65535};
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.advances == std::vector<int>{1872, 1872, 2900});
  CHECK(host.thresholds == std::vector<unsigned>{1872, 1872, 2900});
}

TEST_CASE(
    "Actor plan preserves the snapshot failure repair and threshold wrapping",
    "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  host.snapshot = false;
  host.results[0] = {PlanCandidate(1, 65535), PlanCandidate(2, 0),
                     PlanCandidate(3, 6000)};
  host.draw = -1;
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.thresholds == std::vector<unsigned>{0});
  CHECK(host.advances.empty());
  CHECK(host.restores == 0);
  CHECK(host.resets == 0);
  CHECK(host.promoted == 0);
  CHECK(host.candidates[0].Threshold() == 6);
  CHECK(host.candidates[1].Threshold() == 93);
  CHECK(host.candidates[2].Threshold() == 6007);
}

TEST_CASE(
    "Actor plan uses fallback weights only after all projected passes are "
    "empty",
    "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  for (auto& result : host.results) result.clear();
  host.allow_fallback = true;
  host.fallback = {PlanCandidate(9, 100), PlanCandidate(8, 200)};
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.count == 2);
  CHECK(host.sorts == 4);
  CHECK(host.promoted == 0);
  CHECK(host.candidates[0].Weight() == 128);
  CHECK(host.candidates[1].Weight() == 128);
  CHECK(host.fallback[0].Weight() == 17);
  CHECK(host.restores == 1);
  CHECK(host.resets == 2);
}

TEST_CASE("Actor plan keeps empty plans idle when fallback is disabled",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  for (auto& result : host.results) result.clear();
  host.fallback = {PlanCandidate(9)};
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.count == 0);
  CHECK(host.mode == 7);
  CHECK(host.movement == 0);
  CHECK(host.sorts == 3);
}

TEST_CASE("Actor plan refuses exclusions without clearing the existing list",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  host.clear_ok = false;
  REQUIRE_FALSE(ai::BuildActorActionPlan(host));
  CHECK(host.count == 5);
  CHECK(host.log == std::vector<std::string>{"cache", "exclusions"});
}

TEST_CASE("Actor plan restores projections on invalid timing or pass data",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  const int failure = GENERATE(0, 1, 2, 3, 4, 5, 6);
  host.missing_timing = failure == 0;
  host.invalidate_count = failure == 1 || failure == 2;
  host.invalid_count = failure == 1 ? -1 : 17;
  host.missing_rank = failure == 3;
  host.fail_store = failure == 4;
  host.missing_pass = failure == 5;
  host.fail_exclusion = failure == 6;
  REQUIRE_FALSE(ai::BuildActorActionPlan(host));
  CHECK(host.restores == 1);
  CHECK(host.resets == (failure == 0 ? 1 : 2));
  CHECK(host.promoted == 0);
  CHECK(host.movement == 0);
}

TEST_CASE(
    "Actor plan restores projections on excess fallback or callback exceptions",
    "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  SECTION("fallback") {
    for (auto& result : host.results) result.clear();
    host.allow_fallback = true;
    host.emitted_fallback_count = 17;
    CHECK_FALSE(ai::BuildActorActionPlan(host));
  }
  SECTION("exception") {
    host.on_advance = [] { throw std::runtime_error("preview failed"); };
    CHECK_THROWS_AS(ai::BuildActorActionPlan(host), std::runtime_error);
  }
  CHECK(host.restores == 1);
  CHECK(host.resets == 2);
}

TEST_CASE("Actor plan rereads ranking and count after each random draw",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  host.results[0] = {PlanCandidate(1, 10), PlanCandidate(2, 20)};
  host.results[1].clear();
  host.results[2].clear();
  host.draw = -1;
  host.on_random = [&] {
    host.order[0] = 1;
    host.count = 0;
  };
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.candidates[0].Threshold() == 10);
  CHECK(host.candidates[1].Threshold() == 27);
  CHECK(host.mode == 7);
  CHECK(host.movement == 0);
}

TEST_CASE("Actor plan uses the post-readiness count for movement",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  host.on_readiness = [&] { host.count = 0; };
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.mode == 7);
  CHECK(host.movement == 0);
}

TEST_CASE(
    "Actor plan observes late arena command saved state subtype and count",
    "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  host.command = ai::ActionPlanCommand{24, 0};
  host.on_movement = [&] { host.arena = true; };
  host.on_progress = [&] { host.command = ai::ActionPlanCommand{24, 4}; };
  host.on_set_progress = [&] {
    host.plan_saved = true;
    host.subtype = 173;
    host.count = 3;
  };
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.progress == 5);
  CHECK(host.saved_count == 3);
  CHECK(host.count == 0);
}

TEST_CASE("Actor plan saves only the two arena actor subtypes",
          "[ai][planning][actor-plan]") {
  RecordingActorPlan host;
  const auto subtype = GENERATE(0, 171, 172, 173, 174, 65535);
  host.arena = host.plan_saved = true;
  host.subtype = static_cast<std::uint16_t>(subtype);
  REQUIRE(ai::BuildActorActionPlan(host));
  CHECK(host.saved_count == ((subtype == 172 || subtype == 173) ? 16 : 0));
  CHECK(host.count == ((subtype == 172 || subtype == 173) ? 0 : 16));
}

TEST_CASE("Plan rebuild keeps the original pass order",
          "[combat][ai][planning]") {
  RecordingRoom room;
  room.plans_using_room_delay = {3};
  VS::Battle::Ai::RebuildActorActionPlans(room, std::nullopt);
  const std::vector<std::string> expected = {
      "readiness",     "reconcile",           "prepare 0",    "prepare 3",
      "relationships", "defeated 0 0",        "defeated 3 0", "build 0",
      "build 3",       "set awareness 7 128", "readiness"};
  CHECK(room.log == expected);
}

TEST_CASE("Only the queued primary target loses its re-engagement grace",
          "[combat][ai][planning]") {
  RecordingRoom room;
  VS::Battle::Ai::RebuildActorActionPlans(room, PlanningRequest{7, 0});
  CHECK(room.Count("grace 3") == 1);
  CHECK(room.Count("grace 0") == 0);
  const auto grace = std::find(room.log.begin(), room.log.end(), "grace 3");
  REQUIRE(grace != room.log.begin());
  CHECK(*(grace - 1) == "prepare 3");
}

TEST_CASE("A request with a targets position keeps every grace",
          "[combat][ai][planning]") {
  for (const auto& request :
       {std::optional<PlanningRequest>{}, std::optional{PlanningRequest{7, 1}},
        std::optional{PlanningRequest{9, 0}}}) {
    RecordingRoom room;
    VS::Battle::Ai::RebuildActorActionPlans(room, request);
    CHECK(room.Count("grace 0") == 0);
    CHECK(room.Count("grace 3") == 0);
  }
}

TEST_CASE("Defeat is read from the walk after relationship changes",
          "[combat][ai][planning]") {
  RecordingRoom room;
  room.defeat_on_relationship_slot = 3;
  VS::Battle::Ai::RebuildActorActionPlans(room, std::nullopt);
  CHECK(room.Count("defeated 0 0") == 1);
  CHECK(room.Count("defeated 3 1") == 1);
}

TEST_CASE("Room-delay plans reset or raise the awareness indicator",
          "[combat][ai][planning]") {
  RecordingRoom room;
  room.plans_using_room_delay = {0, 3};
  room.suppressed_indicators = {0};
  VS::Battle::Ai::RebuildActorActionPlans(room, std::nullopt);
  CHECK(room.Count("reset awareness 0") == 1);
  CHECK(room.Count("set awareness 1 128") == 0);
  CHECK(room.Count("set awareness 7 128") == 1);
  CHECK(room.Count("reset awareness 3") == 0);

  RecordingRoom quiet;
  quiet.suppressed_indicators = {0, 3};
  VS::Battle::Ai::RebuildActorActionPlans(quiet, std::nullopt);
  for (const auto& line : quiet.log) {
    CHECK(line.find("awareness") == std::string::npos);
  }
}

TEST_CASE("The live request reads the primary target, not padded offsets",
          "[combat][ai][planning]") {
  Battle_ActionRequest request;
  std::memset(&request, 0, sizeof request);
  request.primaryTarget.target.targetActor = 7;
  request.primaryTarget.riskChange = 3;
  request.primaryTarget.targetsPosition = 0;
  std::memset(&request.targets[0], 0x5A, sizeof request.targets[0]);
  PlanningRequest decoded = VS::Battle::Ai::PlanningRequestOf(request);
  CHECK(decoded.primary_target == 7);
  CHECK(decoded.targets_position == 0);

  request.primaryTarget.targetsPosition = 0x12;
  decoded = VS::Battle::Ai::PlanningRequestOf(request);
  CHECK(decoded.targets_position == 0x12);
}

TEST_CASE("The promoted plan rebuild has no original module address",
          "[combat][ai][planning]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DEC88u) == nullptr);
}

TEST_CASE("Completed plan ignores absent and position requests",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  CHECK(ai::ReconcileCompletedActionPlan(std::nullopt, host));
  auto action = CompletedActionOf(5);
  action.performer_targets_position = true;
  CHECK(ai::ReconcileCompletedActionPlan(action, host));
  CHECK(host.log.empty());
}

TEST_CASE("Completed plan returns before an absent performer's command (#336)",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  host.performer = false;
  CHECK(ai::ReconcileCompletedActionPlan(CompletedActionOf(5), host));
  CHECK(host.log == std::vector<std::string>{"select 3"});
}

TEST_CASE("Completed plan reconciles only action commands",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  host.command =
      ai::ActionPlanCommand{static_cast<std::uint8_t>(GENERATE(0, 23, 25, 255)),
                            RecordingCompletedPlan::kSlotArgument};
  CHECK(ai::ReconcileCompletedActionPlan(CompletedActionOf(5), host));
  CHECK(host.log == std::vector<std::string>{"select 3", "command"});
}

TEST_CASE("Completed plan records the match and remembers the target",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  host.targets = {CompletedTargetAt(4)};
  CHECK(ai::ReconcileCompletedActionPlan(CompletedActionOf(5), host));
  CHECK(host.detour == 9);
  CHECK(host.log == std::vector<std::string>{"select 3", "command", "rank 0",
                                             "detour 9", "target 0", "object 4",
                                             "remember"});
}

TEST_CASE("Completed plan compares the full action ID and both command halves",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  std::uint16_t action_id = 5;
  SECTION("action IDs past the candidate field") { action_id = 0x105; }
  SECTION("a wider row half") {
    host.command = ai::ActionPlanCommand{24, (1 << 16) | 0x0A};
  }
  SECTION("a wider column half") {
    host.command = ai::ActionPlanCommand{24, (5 << 16) | 2};
  }
  SECTION("another column") { host.command = ai::ActionPlanCommand{24, 2}; }
  CHECK(ai::ReconcileCompletedActionPlan(CompletedActionOf(action_id), host));
  CHECK(host.detour == -1);
  CHECK(host.log ==
        std::vector<std::string>{"select 3", "command", "rank 0", "remember"});
}

TEST_CASE("Completed plan with no candidates still remembers the target",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  host.count = GENERATE(-1, 0);
  CHECK(ai::ReconcileCompletedActionPlan(CompletedActionOf(5), host));
  CHECK(host.log ==
        std::vector<std::string>{"select 3", "command", "remember"});
}

TEST_CASE("Later completed-plan matches replace the detour and rescan targets",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  host.count = 3;
  host.ranked = {CompletedCandidate(5, 4), CompletedCandidate(6, 8),
                 CompletedCandidate(5, 7)};
  host.targets = {CompletedTargetAt(1)};
  CHECK(ai::ReconcileCompletedActionPlan(CompletedActionOf(5), host));
  CHECK(host.detour == 7);
  CHECK(host.log == std::vector<std::string>{
                        "select 3", "command", "rank 0", "detour 4", "target 0",
                        "object 1", "rank 1", "rank 2", "detour 7", "target 0",
                        "object 1", "remember"});
}

TEST_CASE("Completed plan forgets for absent and kind 2 or 4 target objects",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  const int kind = GENERATE(-1, 2, 4);
  host.objects[6] = ai::CompletedTargetObject{
      .present = kind >= 0, .kind = static_cast<std::uint8_t>(kind & 7)};
  host.count = 2;
  host.ranked = {CompletedCandidate(5, 4), CompletedCandidate(5, 7)};
  host.targets = {CompletedTargetAt(200, true), CompletedTargetAt(6),
                  CompletedTargetAt(1)};
  CHECK(ai::ReconcileCompletedActionPlan(CompletedActionOf(5), host));
  CHECK(host.detour == 4);
  CHECK(host.log == std::vector<std::string>{"select 3", "command", "rank 0",
                                             "detour 4", "target 0", "target 1",
                                             "object 6", "forget"});
}

TEST_CASE("Completed plan keeps memory for other target object kinds",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  const auto kind = static_cast<std::uint8_t>(GENERATE(0, 1, 3, 5, 6, 7));
  host.objects[6] = ai::CompletedTargetObject{.present = true, .kind = kind};
  host.targets = {CompletedTargetAt(6), CompletedTargetAt(255, true)};
  CHECK(ai::ReconcileCompletedActionPlan(CompletedActionOf(5), host));
  CHECK(host.log == std::vector<std::string>{"select 3", "command", "rank 0",
                                             "detour 9", "target 0", "object 6",
                                             "target 1", "remember"});
}

TEST_CASE("Completed plan refuses records outside native storage",
          "[ai][planning][completed-plan]") {
  RecordingCompletedPlan host;
  std::vector<std::string> expected{"select 3", "command"};
  SECTION("a performer outside the state table") {
    host.performer = std::nullopt;
    expected = {"select 3"};
  }
  SECTION("a missing performer command") { host.command = std::nullopt; }
  SECTION("a candidate count past the list") {
    host.count = 17;
    host.ranked.assign(16, CompletedCandidate(6, 9));
    for (int rank = 0; rank < 16; ++rank) {
      expected.push_back("rank " + std::to_string(rank));
    }
  }
  SECTION("a missing ranked candidate") {
    host.count = 2;
    expected.insert(expected.end(), {"rank 0", "detour 9", "rank 1"});
  }
  SECTION("a target count past the request") {
    host.targets.assign(256, CompletedTargetAt(0, true));
    expected.insert(expected.end(), {"rank 0", "detour 9"});
    for (int index = 0; index < 255; ++index) {
      expected.push_back("target " + std::to_string(index));
    }
  }
  SECTION("an unreadable target") {
    host.targets = {std::nullopt};
    expected.insert(expected.end(), {"rank 0", "detour 9", "target 0"});
  }
  SECTION("a target outside the gimmick table") {
    host.targets = {CompletedTargetAt(16)};
    expected.insert(expected.end(),
                    {"rank 0", "detour 9", "target 0", "object 16"});
  }
  CHECK_FALSE(ai::ReconcileCompletedActionPlan(CompletedActionOf(5), host));
  CHECK(host.log == expected);
}
