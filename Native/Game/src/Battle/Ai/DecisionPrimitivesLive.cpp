// Hand-maintained (tools/port/owned.csv).
// Live binding for the battle AI decision gates (DecisionPrimitives.cpp).
#include "Battle/Ai/DecisionPrimitivesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Damage.h"
#include "Core/BiosRand.h"
#include "Core/Diagnostics.h"
#include "Field/Gimmicks.h"
}

namespace VS::Battle::Ai {
namespace {
static_assert(sizeof(Battle_Ai_ActionCandidate) ==
              sizeof(ActionCandidate::Words));
static_assert(sizeof(Battle_Ai_CandidateEntryView) ==
              sizeof(ActionCandidate::Words));
static_assert(std::size(Field_gimmickInstances) == kCandidateTargetSlots);
static_assert(std::size(Battle_actionDefinitions) == kActionDefinitionCount);
static_assert(
    std::extent_v<decltype(Battle_Ai_ActorAiState::distancesSquared)> ==
    kCandidateTargetSlots);
// Movement passes its target approach view, which aliases the AI state.
static_assert(offsetof(Battle_Ai_TargetApproachView, distancesSquared) ==
              offsetof(Battle_Ai_ActorAiState, distancesSquared));
constexpr std::uint32_t kRelationReady = 0x80000000;
constexpr std::uint32_t kRelationInSight = 1u << 2;
constexpr int kSteppingLocomotion = 1;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

const Battle_Ai_ActorAiState& StateOf(const Battle_Ai_ActorAiState* state) {
  Require(state != nullptr, "AI decision gate requires an actor state");
  return *state;
}

ActionCandidate CandidateOf(const Battle_Ai_ActionCandidate* candidate) {
  Require(candidate != nullptr, "AI decision gate requires a candidate");
  return ActionCandidate(std::bit_cast<ActionCandidate::Words>(*candidate));
}

ActionCooldown CooldownOf(const Battle_Ai_ActorAiState& state) {
  return {.uses_room_action_delay = state.usesRoomActionDelay != 0,
          .action_delay = state.actionDelay,
          .minimum_action_delay = state.minActionDelay};
}

bool HasGimmick(unsigned target) {
  return Field_gimmickInstances[target] != nullptr;
}

std::int16_t SignedHalf(int value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

// The battle actor list as the gimmick proximity check reads it.
class LiveActorProximity {
 public:
  explicit LiveActorProximity(const Battle_Ai_ActorAiState& state) {
    for (const Battle_Actor* actor = Battle_actors[0]; actor != nullptr;
         actor = actor->next) {
      Require(count_ < actors_.size() && actor->id >= 0 &&
                  actor->id < kCandidateTargetSlots,
              "AI proximity check requires a bounded battle actor list");
      const auto id = static_cast<std::size_t>(actor->id);
      ActorProximity& entry = actors_.at(count_++);
      entry.classification = state.relations[id].classification;
      if (const Actors_ActorRecord* record = Actors_actorRecords[id]) {
        entry.tile = GridTile{.x = record->instance.currentTileX,
                              .z = record->instance.currentTileZ};
      }
    }
  }

  [[nodiscard]] std::span<const ActorProximity> Actors() const {
    return std::span(actors_).first(count_);
  }

 private:
  std::array<ActorProximity, kCandidateTargetSlots> actors_{};
  std::size_t count_ = 0;
};

std::optional<bool> HostileNear(const Battle_Ai_ActorAiState& state,
                                unsigned target) {
  const Field_GimmickInstance* gimmick = Field_gimmickInstances[target];
  Require(gimmick != nullptr, "AI proximity check requires a target gimmick");
  const LiveActorProximity actors(state);
  return HasNearbyEnemy(
      GridTile{.x = static_cast<std::uint8_t>(gimmick->tileX),
               .z = static_cast<std::uint8_t>(gimmick->tileZ)},
      actors.Actors());
}

class LiveExecutionHost final : public ActionExecutionHost {
 public:
  explicit LiveExecutionHost(const Battle_Ai_ActorAiState& state)
      : state_(state) {}

  [[nodiscard]] std::uint16_t ReadinessLevel() const override {
    Require(Battle_Ai_roomAiState != nullptr,
            "AI execution check requires room state");
    return Battle_Ai_roomAiState->readinessLevel;
  }
  [[nodiscard]] ActionCooldown Cooldown() const override {
    return CooldownOf(state_);
  }
  [[nodiscard]] bool TargetIsGimmick(unsigned target) const override {
    return HasGimmick(target);
  }
  [[nodiscard]] std::int32_t DistanceSquared(unsigned target) const override {
    return std::bit_cast<std::int32_t>(state_.distancesSquared[target]);
  }
  [[nodiscard]] TargetRelation Relation(unsigned target) const override {
    const auto bits = std::bit_cast<std::uint32_t>(state_.relations[target]);
    return {.ready = (bits & kRelationReady) != 0,
            .in_sight = (bits & kRelationInSight) != 0,
            .obstructed = state_.relations[target].blocked != 0};
  }
  [[nodiscard]] std::optional<bool> HostileNearGimmick(
      unsigned target) const override {
    return HostileNear(state_, target);
  }
  [[nodiscard]] std::optional<bool> AshleyMoving() const override {
    const Actors_ActorRecord* ashley = Actors_actorRecords[0];
    if (ashley == nullptr) return std::nullopt;
    return ashley->instance.movementMode != 0 ||
           ashley->instance.locomotionState == kSteppingLocomotion;
  }

 private:
  const Battle_Ai_ActorAiState& state_;
};

const Battle_Actor& TargetActor(int action, int actor_id) {
  Require(action >= 0 && static_cast<unsigned>(action) < kActionDefinitionCount,
          "AI targeting flag requires an action in the action table");
  Require(actor_id >= 0 &&
              static_cast<std::size_t>(actor_id) < std::size(Battle_actors),
          "AI targeting flag requires an ID in the battle actor table");
  const Battle_Actor* actor = Battle_actors[actor_id];
  Require(actor != nullptr && actor->stats != nullptr,
          "AI targeting flag requires the actor's stats");
  return *actor;
}

std::uint16_t ActionAiFlags(unsigned action) {
  return std::bit_cast<std::uint16_t>(Battle_actionDefinitions[action].aiFlags);
}

class LiveFallbackCache final : public FallbackCandidateCache {
 public:
  [[nodiscard]] std::uint32_t SlotMask() const override {
    return std::bit_cast<std::uint32_t>(Battle_Ai_unusedPlanCounter);
  }
  void SetSlotMask(std::uint32_t mask) override {
    Battle_Ai_unusedPlanCounter = std::bit_cast<int>(mask);
  }
  [[nodiscard]] int Count() const override {
    return Battle_Ai_fallbackCandidateCount;
  }
  [[nodiscard]] bool Store(std::size_t index,
                           const ActionCandidate& candidate) override {
    if (Battle_Ai_fallbackCandidates == nullptr) return false;
    Battle_Ai_fallbackCandidates[index] =
        std::bit_cast<Battle_Ai_CandidateEntryView>(candidate.RawWords());
    return true;
  }
  void SetCount(int count) override {
    Battle_Ai_fallbackCandidateCount = count;
  }
};

class LiveCandidateRandom final : public CandidateRandom {
 public:
  [[nodiscard]] int Next() override { return Core_Random_Rand(); }
};
}  // namespace

bool DoesLiveActionRequireCooldown(const Battle_Ai_ActorAiState* state,
                                   const Battle_Ai_ActionCandidate* candidate) {
  const ActionCandidate typed = CandidateOf(candidate);
  return DoesActionRequireCooldown(CooldownOf(StateOf(state)), typed,
                                   HasGimmick(typed.TargetId()));
}

bool IsLiveActionCooldownReady(const Battle_Ai_ActorAiState* state,
                               const Battle_Ai_ActionCandidate* candidate) {
  const ActionCandidate typed = CandidateOf(candidate);
  return IsActionCooldownReady(CooldownOf(StateOf(state)), typed,
                               HasGimmick(typed.TargetId()));
}

bool HasLiveNearbyEnemy(const Battle_Ai_ActorAiState* state, int target) {
  Require(target >= 0 && target < kCandidateTargetSlots,
          "AI proximity check requires a target in the gimmick table");
  const std::optional<bool> hostile_near =
      HostileNear(StateOf(state), static_cast<unsigned>(target));
  Require(hostile_near.has_value(),
          "AI proximity check requires each hostile actor's model");
  return *hostile_near;
}

bool CanExecuteLiveActionCandidate(const Battle_Ai_ActorAiState* state,
                                   const Battle_Ai_ActionCandidate* candidate) {
  const LiveExecutionHost host(StateOf(state));
  const std::optional<bool> executable =
      CanExecuteActionCandidate(host, CandidateOf(candidate));
  Require(executable.has_value(),
          "AI execution check requires Ashley's and each hostile's model");
  return *executable;
}

int ScoreLiveActorCondition(int status, int hp, int mp, int dp) {
  const std::optional<std::int32_t> score =
      ScoreActorCondition({.status = status,
                           .hp = SignedHalf(hp),
                           .mp = SignedHalf(mp),
                           .dp = SignedHalf(dp)});
  Require(score.has_value(), "AI condition score overflows its signed range");
  return *score;
}

int IsLiveActionHarmfulToActor(int action, int actor_id) {
  const Battle_Actor& actor = TargetActor(action, actor_id);
  const auto id = static_cast<unsigned>(action);
  return static_cast<int>(
      IsActionHarmfulToActor(id, ActionAiFlags(id), actor.stats->enemyClass));
}

int IsLiveActionBeneficialToActor(int action, int actor_id) {
  const Battle_Actor& actor = TargetActor(action, actor_id);
  const auto id = static_cast<unsigned>(action);
  return static_cast<int>(IsActionBeneficialToActor(id, ActionAiFlags(id),
                                                    actor.stats->enemyClass));
}

void CacheLiveFallbackActionCandidate(
    const Battle_Ai_ActionCandidate* candidate,
    const Battle_ActionDefinition* action) {
  Require(action != nullptr, "AI fallback cache requires an action");
  LiveFallbackCache cache;
  Require(CacheFallbackActionCandidate(cache, CandidateOf(candidate),
                                       action->aiFlags.reserved0Bit6 != 0)
              .has_value(),
          "AI fallback cache requires a valid slot, count and storage");
}

void PromoteLiveRandomizedActionCandidate(Battle_Ai_ActionPlanView* state,
                                          int count) {
  LiveCandidateRanking ranking(state, count);
  LiveCandidateRandom random;
  Require(
      PromoteRandomizedActionCandidate(ranking.Ranking(), random).has_value(),
      "AI candidate promotion cannot rank its candidates");
  ranking.Apply();
}

int NextLiveActorRandomMasked(int range) {
  Battle_Ai_FrameContext* frame = Battle_Ai_FrameState();
  Require(frame != nullptr, "AI random draw requires the frame state");
  ActorRandomState random{.accumulator = frame->randomState,
                          .step = frame->lastBudgetTick};
  const std::optional<int> value = NextActorRandomMasked(random, range);
  Require(value.has_value(),
          "AI random draw requires a positive range and a bounded sum");
  frame->randomState = random.accumulator;
  return *value;
}

bool IsLiveAiTargetBeyondNearRadius(const void* view, int target,
                                    int /*range*/) {
  Require(view != nullptr && target >= 0 && target < kCandidateTargetSlots,
          "AI near-target query requires a state and a valid target");
  const auto& state = *static_cast<const Battle_Ai_ActorAiState*>(view);
  return IsAiTargetBeyondNearRadius(
      state.distancesSquared[static_cast<std::size_t>(target)]);
}

bool CanApproachLiveEngagedTarget(const Battle_Ai_ActorAiState* state,
                                  const Battle_Ai_ActionCandidate* candidate) {
  const Battle_Ai_ActorAiState& actor = StateOf(state);
  const ActionCandidate typed = CandidateOf(candidate);
  std::optional<EngagementRoster> roster;
  if (Battle_Ai_roomAiState != nullptr) {
    const Battle_Ai_RoomAiState& room = *Battle_Ai_roomAiState;
    roster = EngagementRoster{.engaged_mask = EngagedMask(room),
                              .engaged_count = room.engagedCount,
                              .limit = room.engagementLimit,
                              .approach_moves = room.approachMoves};
  }
  const std::optional<bool> approach = CanApproachEngagedTarget(
      actor.actorId,
      {.engaged_frames = actor.engagementFrames,
       .cooldown_frames = actor.engagementCooldownFrames,
       .grace_frames = actor.reengagementGraceFrames},
      roster.has_value() ? &*roster : nullptr,
      actor.relations[typed.TargetId()].active != 0);
  Require(approach.has_value(),
          "AI approach gate requires a valid actor and room state");
  return *approach;
}
}  // namespace VS::Battle::Ai
