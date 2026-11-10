#include "Battle/Ai/PlanningWorkspace.hpp"

#include <algorithm>
#include <array>

extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
constexpr std::array<int, kActionPlanPassCount> kPlanDelays{0, 455, 2900};
constexpr std::array<int, kActionPlanPassCount> kPlanQuotas{10, 3, 3};
constexpr unsigned kRecoveryDelayScale = 35;
constexpr unsigned kFallbackWeight = 128;
constexpr int kThresholdSpacing = 80;
constexpr unsigned kThresholdRandomMask = 7;
constexpr std::uint8_t kIdlePlanMode = 7;
constexpr std::uint8_t kArenaSetupProgress = 4;
constexpr std::uint8_t kArenaReadyProgress = 5;
constexpr std::uint8_t kActionCommandOpcode = 24;
constexpr unsigned kFirstArenaActorSubtype = 172;
constexpr int kPlanCapacity = static_cast<int>(kCandidateCapacity);

bool RefusePlan(const char* reason) {
  Core_Debugf("ai", "%s", reason);
  return false;
}

bool ValidPlanCount(int count) { return count >= 0 && count <= kPlanCapacity; }

class ProjectedStatsScope {
 public:
  explicit ProjectedStatsScope(ActorActionPlanHost& host) : host_(host) {}
  ProjectedStatsScope(const ProjectedStatsScope&) = delete;
  ProjectedStatsScope& operator=(const ProjectedStatsScope&) = delete;
  ProjectedStatsScope(ProjectedStatsScope&&) = delete;
  ProjectedStatsScope& operator=(ProjectedStatsScope&&) = delete;
  ~ProjectedStatsScope() {
    host_.RestoreStats();
    host_.ResetReadiness();
  }

 private:
  ActorActionPlanHost& host_;
};

bool SelectPlanningPass(ActorActionPlanHost& host, std::size_t pass,
                        std::array<int, kActionPlanPassCount>& selected) {
  auto& list = host.Candidates();
  const int count = list.Count();
  if (!ValidPlanCount(count)) {
    return RefusePlan("Action-plan pass exceeds its candidate capacity");
  }
  const int quota =
      pass == 1 ? std::max(kPlanCapacity - selected[0] - kPlanQuotas[2], 0)
                : kPlanQuotas[pass];
  selected[pass] = std::min(count, quota);
  for (int rank = 0; rank < selected[pass]; ++rank) {
    const auto candidate = host.Ranked(static_cast<std::size_t>(rank));
    if (!candidate ||
        !host.StorePass(pass, static_cast<std::size_t>(rank), *candidate)) {
      return RefusePlan(
          "Action-plan pass requires ranked and saved candidates");
    }
  }
  for (int rank = selected[pass]; rank < list.Count(); ++rank) {
    if (!ValidPlanCount(list.Count())) {
      return RefusePlan("Action-plan rejection exceeds its candidate capacity");
    }
    const auto candidate = host.Ranked(static_cast<std::size_t>(rank));
    if (!candidate ||
        !host.ClearExclusion(candidate->TargetId(), candidate->Action())) {
      return RefusePlan("Action-plan rejection requires a valid exclusion");
    }
  }
  if (!ValidPlanCount(list.Count())) {
    return RefusePlan("Action-plan rejection left an invalid candidate count");
  }
  list.SetCount(0);
  return true;
}

bool MergePlanningPasses(
    ActorActionPlanHost& host,
    const std::array<int, kActionPlanPassCount>& selected) {
  auto& list = host.Candidates();
  for (std::size_t pass = 0; pass < selected.size(); ++pass) {
    for (int rank = 0; rank < selected[pass]; ++rank) {
      const int count = list.Count();
      if (!ValidPlanCount(count) || count == kCandidateCapacity) {
        return RefusePlan("Action-plan merge exceeds its candidate capacity");
      }
      const auto candidate = host.Pass(pass, static_cast<std::size_t>(rank));
      if (!candidate) {
        return RefusePlan("Action-plan merge requires a saved candidate");
      }
      list.Store(static_cast<std::size_t>(count), *candidate);
      list.SetCount(count + 1);
      if (count + 1 == kCandidateCapacity) return true;
    }
  }
  if (list.Count() != 0 || !host.AllowsFallback()) return true;
  for (int rank = 0; rank < host.FallbackCount(); ++rank) {
    if (!ValidPlanCount(host.FallbackCount())) {
      return RefusePlan("Action-plan fallback exceeds its candidate capacity");
    }
    auto candidate = host.Fallback(static_cast<std::size_t>(rank));
    if (!candidate) {
      return RefusePlan("Action-plan fallback requires a cached candidate");
    }
    candidate->SetWeight(kFallbackWeight);
    list.Store(static_cast<std::size_t>(rank), *candidate);
  }
  const int count = host.FallbackCount();
  if (!ValidPlanCount(count)) {
    return RefusePlan("Action-plan fallback requires a bounded count");
  }
  list.SetCount(count);
  host.Sort();
  return true;
}

bool BuildProjectedPlan(ActorActionPlanHost& host,
                        std::array<int, kActionPlanPassCount>& selected) {
  ProjectedStatsScope restore(host);
  const auto timing = host.Timing();
  if (!timing) return RefusePlan("Action plan requires readable timing");
  const int minimum = static_cast<int>(std::max<unsigned>(
      timing->minimum_delay, timing->recovery_timer / kRecoveryDelayScale));
  host.ResetReadiness();
  for (std::size_t pass = 0; pass < selected.size(); ++pass) {
    const int delay = std::max(kPlanDelays[pass], minimum);
    host.AdvanceSimulation(delay);
    host.Enumerate(static_cast<std::uint16_t>(delay));
    host.Sort();
    if (!SelectPlanningPass(host, pass, selected)) return false;
  }
  return MergePlanningPasses(host, selected);
}

bool AdjustPlanThresholds(ActorActionPlanHost& host) {
  auto& list = host.Candidates();
  if (!ValidPlanCount(list.Count())) {
    return RefusePlan("Action-plan thresholds require a bounded count");
  }
  for (int rank = 0; rank < list.Count(); ++rank) {
    if (!ValidPlanCount(list.Count())) {
      return RefusePlan("Action-plan thresholds require a bounded count");
    }
    const auto index = static_cast<std::size_t>(rank);
    if (rank > 0) {
      const auto current = host.Ranked(index);
      const auto first = host.Ranked(0);
      if (!current || !first) {
        return RefusePlan("Action-plan spacing requires readable candidates");
      }
      if (static_cast<int>(current->Threshold()) -
                  static_cast<int>(first->Threshold()) <
              kThresholdSpacing &&
          !host.SetRankedThreshold(
              index, static_cast<std::uint16_t>(first->Threshold() +
                                                kThresholdSpacing))) {
        return RefusePlan("Action-plan spacing requires a writable candidate");
      }
    }
    const auto roll =
        static_cast<unsigned>(host.Random()) & kThresholdRandomMask;
    const auto current = host.Ranked(index);
    if (!current ||
        !host.SetRankedThreshold(
            index, static_cast<std::uint16_t>(current->Threshold() + roll))) {
      return RefusePlan("Action-plan jitter requires a writable candidate");
    }
  }
  return ValidPlanCount(list.Count()) ||
         RefusePlan("Action-plan jitter left an invalid candidate count");
}

bool AdjustArenaPlan(ActorActionPlanHost& host) {
  if (!host.BeginArenaAdjustment()) return true;
  if (host.ArenaProgress() == kArenaSetupProgress) {
    const auto command = host.ArenaCommand();
    if (!command) return RefusePlan("Arena plan requires its captured actor");
    if (command->opcode == kActionCommandOpcode &&
        command->argument == kArenaSetupProgress) {
      host.SetArenaProgress(kArenaReadyProgress);
    }
  }
  if (!host.ArenaPlanSaved()) return true;
  const auto subtype = host.ArenaActorSubtype();
  if (!subtype) return RefusePlan("Arena plan requires its captured subtype");
  if (static_cast<unsigned>(*subtype) - kFirstArenaActorSubtype < 2) {
    const int count = host.Candidates().Count();
    if (!ValidPlanCount(count)) {
      return RefusePlan("Arena plan requires a bounded saved count");
    }
    host.SetSavedPlanCount(static_cast<std::uint8_t>(count));
    host.Candidates().SetCount(0);
  }
  return true;
}
}  // namespace

// func_800DE3E4 (74088.c:1319), 800DE3E4-800DEB0C. Selection counts retain
// the #274 repair: failed snapshot allocation does not promote stack garbage.
bool BuildActorActionPlan(ActorActionPlanHost& host) {
  host.ResetFallbackCache();
  if (!host.ClearConfiguredExclusions()) {
    return RefusePlan("Action plan requires configured target exclusions");
  }
  auto& list = host.Candidates();
  list.SetCount(0);
  if (host.ActorId() == 0) return true;
  std::array<int, kActionPlanPassCount> selected{};
  if (host.SnapshotStats()) {
    if (!BuildProjectedPlan(host, selected)) return false;
  } else {
    host.Enumerate(0);
    host.Sort();
  }
  if (!ValidPlanCount(list.Count())) {
    return RefusePlan("Action plan requires a bounded candidate count");
  }
  if (list.Count() >= 2) {
    for (const int count : selected) {
      if (count != 0) {
        host.Promote(count);
        break;
      }
    }
  }
  if (!AdjustPlanThresholds(host)) return false;
  host.RefreshReadiness();
  if (!ValidPlanCount(list.Count())) {
    return RefusePlan("Action-plan readiness left an invalid candidate count");
  }
  if (list.Count() == 0) {
    host.SetPlanMode(kIdlePlanMode);
  } else {
    host.SelectMovement();
  }
  return AdjustArenaPlan(host);
}

// func_800DEC88, BATTLE/BATTLE.PRG/74088.c:1547. The original walks the
// battle actor list three times between the completed-plan reconcile and the
// relationship update; the planning storage it allocates is bound by the
// caller.
void RebuildActorActionPlans(ActionPlanningRoom& room,
                             const std::optional<PlanningRequest>& request) {
  room.ResetReadiness();
  room.ReconcileCompletedPlan();
  for (const PlanningActor& actor : room.Actors()) {
    room.PrepareActorPlan(actor);
    if (request && request->targets_position == 0 &&
        request->primary_target == actor.actor_id) {
      room.ClearReengagementGrace(actor);
    }
  }
  room.ApplyRelationshipChanges();
  for (const PlanningActor& actor : room.Actors()) {
    room.SetDefeated(actor, actor.hp == 0);
  }
  for (const PlanningActor& actor : room.Actors()) {
    if (!room.BuildActorPlan(actor)) {
      continue;
    }
    if (room.SuppressesAwarenessIndicator(actor)) {
      room.ResetAwarenessIndicator(actor);
    } else {
      room.SetAwarenessIndicator(actor, kAwarenessIndicatorFrames);
    }
  }
  room.ResetReadiness();
}

}  // namespace VS::Battle::Ai
