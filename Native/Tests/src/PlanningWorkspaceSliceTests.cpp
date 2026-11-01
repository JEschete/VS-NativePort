// Hand-maintained tests for the typed AI plan rebuild slice (#247, #255).
#include <algorithm>
#include <cstring>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/PlanningWorkspace.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Content/ModuleData.h"

namespace {
using VS::Battle::Ai::ActionPlanningRoom;
using VS::Battle::Ai::PlanningActor;
using VS::Battle::Ai::PlanningRequest;

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

}  // namespace

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
