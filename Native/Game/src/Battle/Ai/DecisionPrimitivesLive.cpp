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

#include "Actors/ActorRecord.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/RandomLive.hpp"
#include "Field/Gimmicks.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Battle::Ai {
namespace {
static_assert(sizeof(VS::Battle::Ai::ActionCandidateRecord) ==
              sizeof(ActionCandidate::Words));
static_assert(sizeof(VS::Battle::Ai::CandidateEntryView) ==
              sizeof(ActionCandidate::Words));
static_assert(std::size(VS::Field::gimmickInstances) == kCandidateTargetSlots);
static_assert(std::size(VS::Battle::actionDefinitions) ==
              kActionDefinitionCount);
static_assert(std::tuple_size_v<
                  decltype(VS::Battle::Ai::ActorAiState::distances_squared)> ==
              kCandidateTargetSlots);
// Movement passes its target approach view, which aliases the AI state.
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, distances_squared) ==
              offsetof(VS::Battle::Ai::ActorAiState, distances_squared));
constexpr std::uint32_t kRelationReady = 0x80000000;
constexpr std::uint32_t kRelationInSight = 1u << 2;
constexpr int kSteppingLocomotion = 1;

const VS::Battle::Ai::ActorAiState& StateOf(
    const VS::Battle::Ai::ActorAiState* state) {
  Require(state != nullptr, "AI decision gate requires an actor state");
  return *state;
}

ActionCandidate CandidateOf(
    const VS::Battle::Ai::ActionCandidateRecord* candidate) {
  Require(candidate != nullptr, "AI decision gate requires a candidate");
  return ActionCandidate(std::bit_cast<ActionCandidate::Words>(*candidate));
}

ActionCooldown CooldownOf(const VS::Battle::Ai::ActorAiState& state) {
  return {.uses_room_action_delay = state.temporary_ashley_alliance != 0,
          .action_delay = state.action_delay,
          .minimum_action_delay = state.min_action_delay};
}

bool HasGimmick(unsigned target) {
  return VS::Field::gimmickInstances[target] != nullptr;
}

std::int16_t SignedHalf(int value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

// The battle actor list as the gimmick proximity check reads it.
class LiveActorProximity {
 public:
  explicit LiveActorProximity(const VS::Battle::Ai::ActorAiState& state) {
    for (const VS::Battle::Actor* actor = VS::Battle::actors[0];
         actor != nullptr; actor = actor->next) {
      Require(count_ < actors_.size() && actor->id >= 0 &&
                  actor->id < kCandidateTargetSlots,
              "AI proximity check requires a bounded battle actor list");
      const auto id = static_cast<std::size_t>(actor->id);
      ActorProximity& entry = actors_.at(count_++);
      entry.classification = state.relations[id].classification;
      if (const VS::Actors::ActorRecord* record =
              VS::Actors::actorRecords[id]) {
        entry.tile = GridTile{.x = record->instance.current_tile_x,
                              .z = record->instance.current_tile_z};
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

std::optional<bool> HostileNear(const VS::Battle::Ai::ActorAiState& state,
                                unsigned target) {
  const VS::Field::GimmickInstance* gimmick =
      VS::Field::gimmickInstances[target];
  Require(gimmick != nullptr, "AI proximity check requires a target gimmick");
  const LiveActorProximity actors(state);
  return HasNearbyEnemy(
      GridTile{.x = static_cast<std::uint8_t>(gimmick->tile_x),
               .z = static_cast<std::uint8_t>(gimmick->tile_z)},
      actors.Actors());
}

class LiveExecutionHost final : public ActionExecutionHost {
 public:
  explicit LiveExecutionHost(const VS::Battle::Ai::ActorAiState& state)
      : state_(state) {}

  [[nodiscard]] std::uint16_t ReadinessLevel() const override {
    Require(VS::Battle::Ai::roomAiState != nullptr,
            "AI execution check requires room state");
    return VS::Battle::Ai::roomAiState->readiness_level;
  }
  [[nodiscard]] ActionCooldown Cooldown() const override {
    return CooldownOf(state_);
  }
  [[nodiscard]] bool TargetIsGimmick(unsigned target) const override {
    return HasGimmick(target);
  }
  [[nodiscard]] std::int32_t DistanceSquared(unsigned target) const override {
    return std::bit_cast<std::int32_t>(state_.distances_squared[target]);
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
    const VS::Actors::ActorRecord* ashley = VS::Actors::actorRecords[0];
    if (ashley == nullptr) return std::nullopt;
    return ashley->instance.movement_mode != 0 ||
           ashley->instance.locomotion_state == kSteppingLocomotion;
  }

 private:
  const VS::Battle::Ai::ActorAiState& state_;
};

const VS::Battle::Actor& TargetActor(int action, int actor_id) {
  Require(action >= 0 && static_cast<unsigned>(action) < kActionDefinitionCount,
          "AI targeting flag requires an action in the action table");
  Require(actor_id >= 0 && static_cast<std::size_t>(actor_id) <
                               std::size(VS::Battle::actors),
          "AI targeting flag requires an ID in the battle actor table");
  const VS::Battle::Actor* actor = VS::Battle::actors[actor_id];
  Require(actor != nullptr && actor->stats != nullptr,
          "AI targeting flag requires the actor's stats");
  return *actor;
}

std::uint16_t ActionAiFlags(unsigned action) {
  return std::bit_cast<std::uint16_t>(
      VS::Battle::actionDefinitions[action].ai_flags);
}

class LiveFallbackCache final : public FallbackCandidateCache {
 public:
  [[nodiscard]] std::uint32_t SlotMask() const override {
    return std::bit_cast<std::uint32_t>(VS::Battle::Ai::fallbackSlotMask);
  }
  void SetSlotMask(std::uint32_t mask) override {
    VS::Battle::Ai::fallbackSlotMask = std::bit_cast<int>(mask);
  }
  [[nodiscard]] int Count() const override {
    return VS::Battle::Ai::fallbackCandidateCount;
  }
  [[nodiscard]] bool Store(std::size_t index,
                           const ActionCandidate& candidate) override {
    if (VS::Battle::Ai::fallbackCandidates == nullptr) return false;
    VS::Battle::Ai::fallbackCandidates[index] =
        std::bit_cast<VS::Battle::Ai::CandidateEntryView>(candidate.RawWords());
    return true;
  }
  void SetCount(int count) override {
    VS::Battle::Ai::fallbackCandidateCount = count;
  }
};

class LiveCandidateRandom final : public CandidateRandom {
 public:
  [[nodiscard]] int Next() override { return VS::Core::NextBiosRand(); }
};
}  // namespace

bool DoesLiveActionRequireCooldown(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate) {
  const ActionCandidate typed = CandidateOf(candidate);
  return DoesActionRequireCooldown(CooldownOf(StateOf(state)), typed,
                                   HasGimmick(typed.TargetId()));
}

bool IsLiveActionCooldownReady(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate) {
  const ActionCandidate typed = CandidateOf(candidate);
  return IsActionCooldownReady(CooldownOf(StateOf(state)), typed,
                               HasGimmick(typed.TargetId()));
}

bool HasLiveNearbyEnemy(const VS::Battle::Ai::ActorAiState* state, int target) {
  Require(target >= 0 && target < kCandidateTargetSlots,
          "AI proximity check requires a target in the gimmick table");
  const std::optional<bool> hostile_near =
      HostileNear(StateOf(state), static_cast<unsigned>(target));
  Require(hostile_near.has_value(),
          "AI proximity check requires each hostile actor's model");
  return *hostile_near;
}

bool CanExecuteLiveActionCandidate(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate) {
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
  const VS::Battle::Actor& actor = TargetActor(action, actor_id);
  const auto id = static_cast<unsigned>(action);
  return static_cast<int>(
      IsActionHarmfulToActor(id, ActionAiFlags(id), actor.stats->enemy_class));
}

int IsLiveActionBeneficialToActor(int action, int actor_id) {
  const VS::Battle::Actor& actor = TargetActor(action, actor_id);
  const auto id = static_cast<unsigned>(action);
  return static_cast<int>(IsActionBeneficialToActor(id, ActionAiFlags(id),
                                                    actor.stats->enemy_class));
}

void CacheLiveFallbackActionCandidate(
    const VS::Battle::Ai::ActionCandidateRecord* candidate,
    const ActionDefinition* action) {
  Require(action != nullptr, "AI fallback cache requires an action");
  LiveFallbackCache cache;
  Require(CacheFallbackActionCandidate(cache, CandidateOf(candidate),
                                       action->ai_flags.reserved0_bit6 != 0)
              .has_value(),
          "AI fallback cache requires a valid slot, count and storage");
}

void PromoteLiveRandomizedActionCandidate(VS::Battle::Ai::ActionPlanView* state,
                                          int count) {
  LiveCandidateRanking ranking(state, count);
  LiveCandidateRandom random;
  Require(
      PromoteRandomizedActionCandidate(ranking.Ranking(), random).has_value(),
      "AI candidate promotion cannot rank its candidates");
  ranking.Apply();
}

int NextLiveActorRandomMasked(int range) {
  FrameContext* frame = &LiveFrameContext();
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
  const auto& state = *static_cast<const VS::Battle::Ai::ActorAiState*>(view);
  return IsAiTargetBeyondNearRadius(
      state.distances_squared[static_cast<std::size_t>(target)]);
}

bool CanApproachLiveEngagedTarget(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate) {
  const VS::Battle::Ai::ActorAiState& actor = StateOf(state);
  const ActionCandidate typed = CandidateOf(candidate);
  std::optional<EngagementRoster> roster;
  if (VS::Battle::Ai::roomAiState != nullptr) {
    const VS::Battle::Ai::RoomAiState& room = *VS::Battle::Ai::roomAiState;
    roster = EngagementRoster{.engaged_mask = EngagedMask(room),
                              .engaged_count = room.engaged_count,
                              .limit = room.engagement_limit,
                              .approach_moves = room.approach_moves};
  }
  const std::optional<bool> approach = CanApproachEngagedTarget(
      actor.actor_id,
      {.engaged_frames = actor.engagement_frames,
       .cooldown_frames = actor.engagement_cooldown_frames,
       .grace_frames = actor.reengagement_grace_frames},
      roster.has_value() ? &*roster : nullptr,
      actor.relations[typed.TargetId()].active != 0);
  Require(approach.has_value(),
          "AI approach gate requires a valid actor and room state");
  return *approach;
}
}  // namespace VS::Battle::Ai
