// Hand-maintained (tools/port/owned.csv).
// Live binding for the battle AI candidate list and planning checks
// (PlanningPrimitives.cpp).
#include "Battle/Ai/PlanningPrimitivesLive.hpp"

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

extern "C" {
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
static_assert(sizeof(Battle_Ai_ActionCandidate) ==
              sizeof(ActionCandidate::Words));
static_assert(sizeof(Battle_ActorStats) == kActorStatsRecordBytes);
static_assert(offsetof(Battle_ActorStats, currentHp) == kSnapshotVitals.offset);
static_assert(offsetof(Battle_ActorStats, movementClass) ==
              kSnapshotMovement.offset);
static_assert(offsetof(Battle_ActorStats, limbs) == kSnapshotLimbs.offset);
static_assert(sizeof(Battle_ActorStats::limbs) == kSnapshotLimbs.size);
static_assert(offsetof(Battle_ActorStats, statuses) ==
              kSnapshotStatuses.offset);
static_assert(std::size(Battle_actionDefinitions) == kActionDefinitionCount);
static_assert(std::size(Battle_Ai_actorAiStates) == kCandidateTargetSlots);
static_assert(std::extent_v<decltype(Battle_Ai_ActorAiState::candidates)> ==
              kCandidateCapacity);
static_assert(
    std::extent_v<decltype(Battle_Ai_ActionPlanView::candidateOrder)> ==
    kCandidateCapacity);
constexpr int kRestoreOperation = 2;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

ActionCandidate CandidateOf(const Battle_Ai_ActionCandidate* candidate) {
  Require(candidate != nullptr, "AI planning requires a candidate");
  return ActionCandidate(std::bit_cast<ActionCandidate::Words>(*candidate));
}

Battle_Ai_ActionCandidate RecordOf(const ActionCandidate& candidate) {
  return std::bit_cast<Battle_Ai_ActionCandidate>(candidate.RawWords());
}

const Battle_ActorStats& StatsOf(const Battle_Ai_ActorAiState& state) {
  Require(state.actor != nullptr && state.actor->opaque3C != nullptr,
          "AI planning requires the actor's stats");
  return *state.actor->opaque3C;
}

std::uint16_t Half(int value) { return static_cast<std::uint16_t>(value); }

ActorResources ResourcesOf(const Battle_ActorStats& stats) {
  return {.hp = Half(stats.currentHp),
          .max_hp = Half(stats.maxHp),
          .mp = Half(stats.currentMp),
          .max_mp = Half(stats.maxMp),
          .weapon_pp = Half(stats.weapon.currentPp),
          .statuses = static_cast<std::uint32_t>(stats.statuses)};
}

class LiveTargetMemory final : public TargetMemoryHost {
 public:
  explicit LiveTargetMemory(Battle_Ai_ActorAiState& state) : state_(state) {}

  [[nodiscard]] int CandidateCount() const override {
    return state_.candidateCount;
  }
  [[nodiscard]] bool MemoryDisabled() const override {
    return state_.settingsConstant19 != 0;
  }
  [[nodiscard]] std::optional<unsigned> FirstRankedTarget() const override {
    const Battle_Ai_ActionCandidate* first = state_.candidateOrder[0];
    if (first == nullptr) return std::nullopt;
    return first->targetId;
  }
  [[nodiscard]] std::uint32_t OwnTile() const override {
    return state_.tile.word;
  }
  [[nodiscard]] std::optional<std::uint32_t> TargetTile(
      unsigned target) const override {
    if (Actors_actorRecords[target] == nullptr) return std::nullopt;
    Actors_ActorLocationQuery placement{};
    if (Actors_ActorPlacement(static_cast<int>(target), &placement) != 0) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(placement.location.packedTile.word);
  }
  void Remember(const TargetMemory& memory) override {
    state_.previousTile = static_cast<int>(memory.previous_tile);
    state_.rememberedTargetTile = memory.remembered_target_tile;
    state_.targetMemoryFrames = memory.frames;
  }

 private:
  Battle_Ai_ActorAiState& state_;
};

class LiveCandidateList final : public CandidateList {
 public:
  explicit LiveCandidateList(Battle_Ai_ActorAiState& state) : state_(state) {}

  [[nodiscard]] int Count() const override { return state_.candidateCount; }
  void Store(std::size_t slot, const ActionCandidate& candidate) override {
    Battle_Ai_ActionCandidate& stored = std::span(state_.candidates)[slot];
    stored = RecordOf(candidate);
    std::span(state_.candidateOrder)[slot] = &stored;
  }
  void SetCount(int count) override { state_.candidateCount = count; }
  [[nodiscard]] bool ExcludeTargetAction(unsigned target,
                                         unsigned action) override {
    if (Battle_Ai_candidateExclusions == nullptr) return false;
    Battle_Ai_candidateExclusions[target][action] = 1;
    return true;
  }

 private:
  Battle_Ai_ActorAiState& state_;
};

class LiveTargeting final : public ActionTargetingHost {
 public:
  explicit LiveTargeting(const Battle_Ai_ActorAiState& state) : state_(state) {}

  [[nodiscard]] int ActorId() const override { return state_.actorId; }
  [[nodiscard]] unsigned Classification(int actor_id) const override {
    return state_.relations[actor_id].classification;
  }
  [[nodiscard]] std::optional<ActorResourceConditions> PeerConditions(
      int actor_id) const override {
    const Battle_Ai_ActorAiState* peer = Battle_Ai_actorAiStates[actor_id];
    if (peer == nullptr) return std::nullopt;
    return ActorResourceConditions{
        .status_restricted = (peer->resourceStatusRestricted & 1u) != 0,
        .low_mp = (peer->lowMp & 1u) != 0,
        .low_hp = (peer->lowHp & 1u) != 0};
  }
  [[nodiscard]] std::uint16_t ActionAiFlags(unsigned action) const override {
    return std::bit_cast<std::uint16_t>(
        Battle_actionDefinitions[action].aiFlags);
  }
  [[nodiscard]] bool SlotExcludesSelf(int row, int column) const override {
    return state_.actionSlots[row][column].opaque0Bit1 != 0;
  }

 private:
  const Battle_Ai_ActorAiState& state_;
};

class LiveActionUsability final : public ActionUsabilityHost {
 public:
  LiveActionUsability(Battle_Ai_ActorAiState& state,
                      const Battle_ActorStats& stats)
      : state_(state), stats_(stats) {}

  [[nodiscard]] unsigned SlotAction(int row, int column) const override {
    return stats_.limbActions[row][column].actionId;
  }
  [[nodiscard]] ActionCost Action(unsigned action_id) const override {
    const Battle_ActionDefinition& action = Battle_actionDefinitions[action_id];
    return {.type = action.type, .cost = action.cost};
  }
  [[nodiscard]] std::int16_t LimbHp(int row) const override {
    return stats_.limbs[row].hp;
  }
  [[nodiscard]] ActorResources Resources() const override {
    return ResourcesOf(stats_);
  }
  void SetSlotUsable(int row, int column, bool usable) override {
    state_.actionSlots[row][column].opaque0Bit7 = usable ? 1u : 0u;
  }

 private:
  Battle_Ai_ActorAiState& state_;
  const Battle_ActorStats& stats_;
};
}  // namespace

LiveCandidateRanking::LiveCandidateRanking(Battle_Ai_ActionPlanView* state,
                                           int count) {
  Require(state != nullptr, "AI candidate ranking requires a plan view");
  Require(count >= 0 && count <= state->candidateCount &&
              state->candidateCount <=
                  static_cast<int>(std::size(state->candidateOrder)),
          "AI candidate ranking count is outside the candidate list");
  entries_ =
      std::span(state->candidateOrder).first(static_cast<std::size_t>(count));
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    const Battle_Ai_CandidateEntryView* entry = entries_[index];
    Require(entry != nullptr, "AI candidate ranking contains a null entry");
    ranking_.at(index) = {.score = entry->reserved0,
                          .action = entry->action,
                          .weight = entry->weight,
                          .handle = index};
  }
}

std::span<RankedCandidate> LiveCandidateRanking::Ranking() {
  return std::span(ranking_).first(entries_.size());
}

void LiveCandidateRanking::Apply() {
  std::array<Battle_Ai_CandidateEntryView*, kCandidateCapacity> order{};
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    const std::size_t handle = ranking_.at(index).handle;
    Require(handle < entries_.size(),
            "AI candidate ranking refers past its entries");
    order.at(index) = entries_[handle];
  }
  std::ranges::copy(std::span(order).first(entries_.size()), entries_.begin());
}

void RememberLiveAiActionTarget(Battle_Ai_ActorAiState* state) {
  Require(state != nullptr, "AI target memory requires an actor state");
  LiveTargetMemory host(*state);
  Require(RememberAiActionTarget(host).has_value(),
          "AI target memory requires a ranked, placed target");
}

void CopyLiveActionCandidate(Battle_Ai_ActionCandidate* destination,
                             const Battle_Ai_ActionCandidate* source) {
  Require(destination != nullptr, "AI candidate copy requires a destination");
  ActionCandidate copy;
  CopyActionCandidate(copy, CandidateOf(source));
  *destination = RecordOf(copy);
}

void ApplyLiveSimulatedStatDelta(std::int16_t* value,
                                 const std::int16_t* maximum, int delta,
                                 int operation) {
  Require(value != nullptr && maximum != nullptr,
          "AI simulated stat delta requires a value and its maximum");
  const std::optional<std::int16_t> next = ApplySimulatedStatDelta(
      *value, *maximum, delta, operation == kRestoreOperation);
  Require(next.has_value(), "AI simulated stat delta overflows");
  *value = *next;
}

void AppendLiveActionCandidate(Battle_Ai_ActorAiState* state,
                               const Battle_Ai_ActionCandidate* candidate) {
  Require(state != nullptr, "AI candidate append requires an actor state");
  LiveCandidateList list(*state);
  Require(AppendActionCandidate(list, CandidateOf(candidate)).has_value(),
          "AI candidate append requires a valid count and exclusion table");
}

void SortLiveActionCandidates(Battle_Ai_ActionPlanView* state) {
  Require(state != nullptr, "AI candidate sort requires a plan view");
  LiveCandidateRanking ranking(state, state->candidateCount);
  Require(SortActionCandidates(ranking.Ranking()),
          "AI candidate sort exceeds the candidate list");
  ranking.Apply();
}

void CopyLivePlanningActorStats(const Battle_ActorStats* source,
                                Battle_ActorStats* destination) {
  Require(source != nullptr && destination != nullptr,
          "AI stat snapshot requires a source and destination");
  CopyPlanningActorStats(
      std::as_bytes(std::span<const Battle_ActorStats, 1>(source, 1)),
      std::as_writable_bytes(std::span<Battle_ActorStats, 1>(destination, 1)));
}

bool CanLiveAiActionAffectActor(const Battle_Ai_ActorAiState* state,
                                const Battle_Ai_ActionCandidate* candidate,
                                int actor_id) {
  Require(state != nullptr, "AI target eligibility requires an actor state");
  const LiveTargeting host(*state);
  const std::optional<bool> affects =
      CanAiActionAffectActor(host, CandidateOf(candidate), actor_id);
  Require(affects.has_value(),
          "AI target eligibility requires an actor and slot in their tables");
  return *affects;
}

bool RefreshLiveAiActionUsability(Battle_Ai_ActorAiState* state, int row,
                                  int column) {
  Require(state != nullptr, "AI action usability requires an actor state");
  LiveActionUsability host(*state, StatsOf(*state));
  const std::optional<bool> usable =
      RefreshAiActionUsability(host, row, column);
  Require(usable.has_value(),
          "AI action usability requires a slot and action in their tables");
  return *usable;
}

void RefreshLiveAiResourceRestrictions(Battle_Ai_ActorAiState* state) {
  Require(state != nullptr, "AI resource restrictions require a state");
  const ActorResourceConditions conditions = RefreshAiResourceRestrictions(
      ResourcesOf(StatsOf(*state)), state->actionCostEnabled);
  state->resourceStatusRestricted = conditions.status_restricted ? 1 : 0;
  state->lowMp = conditions.low_mp ? 1 : 0;
  state->lowHp = conditions.low_hp ? 1 : 0;
}
}  // namespace VS::Battle::Ai
