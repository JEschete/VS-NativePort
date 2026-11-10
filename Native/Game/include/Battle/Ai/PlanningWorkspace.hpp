#ifndef NATIVE_GAME_INCLUDE_BATTLE_AI_PLANNINGWORKSPACE_HPP_
#define NATIVE_GAME_INCLUDE_BATTLE_AI_PLANNINGWORKSPACE_HPP_

#include <optional>
#include <vector>

#include "Battle/Ai/PlanningPrimitives.hpp"

namespace VS::Battle::Ai {

inline constexpr std::size_t kActionPlanPassCount = 3;

struct ActionPlanTiming {
  std::uint8_t minimum_delay = 0;
  std::uint16_t recovery_timer = 0;
};

struct ActionPlanCommand {
  std::uint8_t opcode = 0;
  std::int32_t argument = 0;
};

class ActorActionPlanHost {
 public:
  virtual ~ActorActionPlanHost() = default;
  virtual CandidateList& Candidates() = 0;
  virtual void ResetFallbackCache() = 0;
  virtual bool ClearConfiguredExclusions() = 0;
  virtual std::uint8_t ActorId() const = 0;
  virtual bool SnapshotStats() = 0;
  virtual void RestoreStats() noexcept = 0;
  virtual void ResetReadiness() noexcept = 0;
  virtual std::optional<ActionPlanTiming> Timing() const = 0;
  virtual void AdvanceSimulation(int step) = 0;
  virtual void Enumerate(std::uint16_t threshold) = 0;
  virtual void Sort() = 0;
  virtual std::optional<ActionCandidate> Ranked(std::size_t rank) const = 0;
  virtual bool StorePass(std::size_t pass, std::size_t rank,
                         const ActionCandidate& candidate) = 0;
  virtual std::optional<ActionCandidate> Pass(std::size_t pass,
                                              std::size_t rank) const = 0;
  virtual bool ClearExclusion(unsigned target, unsigned action) = 0;
  virtual bool AllowsFallback() const = 0;
  virtual int FallbackCount() const = 0;
  virtual std::optional<ActionCandidate> Fallback(std::size_t rank) const = 0;
  virtual void Promote(int count) = 0;
  virtual bool SetRankedThreshold(std::size_t rank,
                                  std::uint16_t threshold) = 0;
  virtual int Random() = 0;
  virtual void RefreshReadiness() = 0;
  virtual void SetPlanMode(std::uint8_t mode) = 0;
  virtual void SelectMovement() = 0;
  // Captures the command actor only when an arena adjustment is active.
  virtual bool BeginArenaAdjustment() = 0;
  virtual std::uint8_t ArenaProgress() const = 0;
  virtual std::optional<ActionPlanCommand> ArenaCommand() const = 0;
  virtual void SetArenaProgress(std::uint8_t progress) = 0;
  virtual bool ArenaPlanSaved() const = 0;
  virtual std::optional<std::uint16_t> ArenaActorSubtype() const = 0;
  virtual void SetSavedPlanCount(std::uint8_t count) = 0;
};

// func_800DE3E4, BATTLE/BATTLE.PRG/74088.c:1319; 459 original instructions.
bool BuildActorActionPlan(ActorActionPlanHost& host);

// The queued action the plans react to (func_800DEEA4_t at
// BATTLE/BATTLE.PRG/74088.c:388): its primary target's actor and that target's
// targets-position word.
struct PlanningRequest {
  int primary_target = 0;
  int targets_position = 0;
};

// One battle actor as the planner sees it on a walk of the live actor list.
// `slot` indexes the actor AI state table; `actor_id` is the AI state's own id.
struct PlanningActor {
  int slot = 0;
  int actor_id = 0;
  int hp = 0;
};

inline constexpr int kAwarenessIndicatorFrames = 128;

class ActionPlanningRoom {
 public:
  ActionPlanningRoom() = default;
  ActionPlanningRoom(const ActionPlanningRoom&) = delete;
  ActionPlanningRoom& operator=(const ActionPlanningRoom&) = delete;
  ActionPlanningRoom(ActionPlanningRoom&&) = delete;
  ActionPlanningRoom& operator=(ActionPlanningRoom&&) = delete;
  virtual ~ActionPlanningRoom() = default;

  virtual void ResetReadiness() = 0;
  virtual void ReconcileCompletedPlan() = 0;
  virtual void ApplyRelationshipChanges() = 0;
  // Walks the live actor list again; each planning pass reads a fresh walk.
  [[nodiscard]] virtual std::vector<PlanningActor> Actors() const = 0;
  // Clears the per-plan scratch, refreshes vertical movement and selects the
  // fallback action.
  virtual void PrepareActorPlan(const PlanningActor& actor) = 0;
  virtual void ClearReengagementGrace(const PlanningActor& actor) = 0;
  virtual void SetDefeated(const PlanningActor& actor, bool defeated) = 0;
  // Returns whether the rebuilt plan uses the room action delay.
  [[nodiscard]] virtual bool BuildActorPlan(const PlanningActor& actor) = 0;
  [[nodiscard]] virtual bool SuppressesAwarenessIndicator(
      const PlanningActor& actor) const = 0;
  virtual void ResetAwarenessIndicator(const PlanningActor& actor) = 0;
  virtual void SetAwarenessIndicator(const PlanningActor& actor,
                                     int frames) = 0;
};

// func_800DEC88, BATTLE/BATTLE.PRG/74088.c:1547.
void RebuildActorActionPlans(ActionPlanningRoom& room,
                             const std::optional<PlanningRequest>& request);

}  // namespace VS::Battle::Ai

#endif  // NATIVE_GAME_INCLUDE_BATTLE_AI_PLANNINGWORKSPACE_HPP_
