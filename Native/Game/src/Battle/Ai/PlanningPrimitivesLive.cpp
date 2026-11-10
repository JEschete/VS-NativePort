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
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "Actors/ActorStateLive.hpp"
#include "Battle/Ai/ConditionSnapshotBinding.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.h"
#include "Core/Memory/HeapLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Core/Random.h"
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
constexpr int kAimedSimulationMode = 3;
constexpr auto kTargetOffset = offsetof(Battle_Ai_ReactionRecord, opaque4C);
PlanningStatsSnapshot planning_stats_snapshot;
static_assert(std::extent_v<decltype(Battle_ActionRequest::targets)> ==
              kTargetPreviewCapacity);
static_assert(kTargetOffset == offsetof(Battle_ActionRequest, targets));
static_assert(sizeof(Battle_Ai_EngagementActor) == sizeof(Battle_HitResult));

static_assert(sizeof(Battle_Ai_CandidateEntryView) ==
              sizeof(ActionCandidate::Words));
static_assert(offsetof(Battle_Ai_CandidateReadinessView, candidateCount) ==
              offsetof(Battle_Ai_ActorAiState, candidateCount));
static_assert(offsetof(Battle_Ai_CandidateReadinessView, candidateOrder) ==
              offsetof(Battle_Ai_ActorAiState, candidateOrder));
static_assert(offsetof(Battle_Ai_CandidateReadinessView, actionStatus) ==
              offsetof(Battle_Ai_ActorAiState, actionStatus));
static_assert(offsetof(Battle_Ai_CandidateReadinessView, relations) ==
              offsetof(Battle_Ai_ActorAiState, relations));

bool ReadablePlanningBytes(const void* record, std::size_t bytes,
                           std::size_t alignment) {
  if (record == nullptr ||
      reinterpret_cast<std::uintptr_t>(record) % alignment != 0) {
    return false;
  }
  const auto allocation = Core::Memory::LivePayloadContaining(record);
  if (!allocation) {
    return allocation.error() == Core::Memory::HeapError::kOutsideArena;
  }
  const auto offset = reinterpret_cast<std::uintptr_t>(record) -
                      reinterpret_cast<std::uintptr_t>(allocation->data());
  return offset <= allocation->size() && bytes <= allocation->size() - offset;
}

template <class Record>
bool ReadablePlanningRecord(const Record* record) {
  return ReadablePlanningBytes(record, sizeof(Record), alignof(Record));
}

bool OverlappingPlanningBytes(const void* left, std::size_t left_bytes,
                              const void* right, std::size_t right_bytes) {
  const auto left_start = reinterpret_cast<std::uintptr_t>(left);
  const auto right_start = reinterpret_cast<std::uintptr_t>(right);
  return left_start <= right_start ? right_start - left_start < left_bytes
                                   : left_start - right_start < right_bytes;
}

SimulatedActorEffect SimulationEffectOf(
    const Battle_Ai_EngagementActor& entry) {
  return {.actor_id = entry.opaque0,
          .ignored = entry.opaque40 != 0,
          .hp = {entry.opaque4, static_cast<std::uint8_t>(entry.opaque1CBit0)},
          .mp = {entry.opaque6, static_cast<std::uint8_t>(entry.opaque1CBit2)},
          .weapon_dp = {entry.opaque38,
                        static_cast<std::uint8_t>(entry.opaque1EBit2)},
          .shield_dp = {entry.opaque3C,
                        static_cast<std::uint8_t>(entry.opaque1EBit6)},
          .statuses_set = entry.opaque14,
          .statuses_clear = entry.opaque18,
          .hit_threshold = entry.opaque2};
}

struct SimulationSnapshot {
  const Battle_Ai_ReactionRecord* source = nullptr;
  Battle_Ai_ReactionRecord header{};
  std::size_t bytes = 0;
  std::vector<SimulatedActorEffect> targets;
};

std::optional<SimulationSnapshot> ReadSimulationSnapshot() {
  const auto* workspace = Battle_Ai_planningWorkspace;
  if (!ReadablePlanningRecord(workspace)) return std::nullopt;
  SimulationSnapshot result;
  result.source = workspace;
  std::memcpy(&result.header, workspace, sizeof(result.header));
  const auto count = static_cast<std::size_t>(result.header.opaque4A);
  result.bytes = kTargetOffset + count * sizeof(Battle_Ai_EngagementActor);
  if (count > kTargetPreviewCapacity ||
      !ReadablePlanningBytes(workspace, result.bytes,
                             alignof(Battle_Ai_ReactionRecord))) {
    return std::nullopt;
  }
  result.targets.reserve(count);
  const auto* source = reinterpret_cast<const std::byte*>(workspace);
  for (std::size_t index = 0; index < count; ++index) {
    Battle_Ai_EngagementActor entry{};
    std::memcpy(&entry, source + kTargetOffset + index * sizeof(entry),
                sizeof(entry));
    result.targets.push_back(SimulationEffectOf(entry));
  }
  return result;
}

bool RefusePlanning(const char* message) {
  Core_Debugf("ai", "%s", message);
  return false;
}

std::optional<std::span<const PlanningActorStats>> ReadPlanningStatsRows(
    std::array<PlanningActorStats, kBattleActorSlotCount>& rows) {
  std::size_t count = 0;
  for (const auto* actor = Battle_actors[0]; actor != nullptr;
       actor = actor->next) {
    if (count >= rows.size() || !ReadablePlanningRecord(actor) ||
        actor->id < 0 || actor->id >= kBattleActorSlotCount ||
        !ReadablePlanningRecord(actor->stats)) {
      return std::nullopt;
    }
    auto bytes = std::as_writable_bytes(
        std::span<Battle_ActorStats, 1>(actor->stats, 1));
    if (OverlappingPlanningBytes(bytes.data(), bytes.size(),
                                 planning_stats_snapshot.bytes.data(),
                                 planning_stats_snapshot.bytes.size())) {
      return std::nullopt;
    }
    rows[count++] = {.slot = static_cast<std::size_t>(actor->id),
                     .bytes = bytes};
  }
  return std::span<const PlanningActorStats>(rows).first(count);
}

ActorCondition SimulationConditionOf(const Battle_Ai_ActionTiming& row) {
  return {row.opaque4,  row.opaque6,  row.opaque8,  row.opaqueA, row.opaqueC,
          row.opaque10, row.opaque12, row.opaque14, row.opaque16};
}

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

const Battle_Ai_ActionTiming& ConditionRowOf(const Battle_Ai_ActionTiming* rows,
                                             int id) {
  Require(rows != nullptr && id >= 0 && id < kBattleActorSlotCount,
          "AI actor condition requires a row in the planner's table");
  const Battle_Ai_ActionTiming& row = rows[id];
  Require(ReadablePlanningRecord(&row), "AI actor condition row is unreadable");
  return row;
}

ActionCandidate CandidateOf(const Battle_Ai_ActionCandidate* candidate) {
  Require(candidate != nullptr, "AI planning requires a candidate");
  return ActionCandidate(std::bit_cast<ActionCandidate::Words>(*candidate));
}

Battle_Ai_ActionCandidate RecordOf(const ActionCandidate& candidate) {
  return std::bit_cast<Battle_Ai_ActionCandidate>(candidate.RawWords());
}

class LiveCandidateSimulation final : public CandidateSimulationHost {
 public:
  LiveCandidateSimulation(
      Battle_Ai_ActorAiState& state,
      std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows)
      : state_(state), rows_(rows) {}

  std::optional<TargetPoint> TargetPosition(unsigned target) const override {
    if (target >= std::size(Actors_actorRecords)) return std::nullopt;
    const auto* actor = Actors_actorRecords[target];
    if (!ReadablePlanningRecord(actor)) return std::nullopt;
    const auto& position = actor->instance.position;
    return TargetPoint{position.vx, position.vy, position.vz};
  }

  std::optional<bool> PreviewLimb(const ActionCandidate& candidate) override {
    if (!Ready(candidate)) return std::nullopt;
    return Battle_PreviewActionAgainstLimb(
               static_cast<int>(candidate.Action()), state_.actorId,
               static_cast<int>(candidate.TargetId()),
               static_cast<int>(candidate.TargetLimb()),
               Battle_Ai_planningWorkspace) != 0;
  }

  std::optional<bool> PreviewAimed(const ActionCandidate& candidate,
                                   const TargetPoint& source,
                                   const TargetPoint& target) override {
    if (!Ready(candidate)) return std::nullopt;
    SVECTOR source_position{source.x, source.y, source.z, source.pad};
    SVECTOR target_position{target.x, target.y, target.z, target.pad};
    return Battle_PreviewActionAgainstActor(
               static_cast<int>(candidate.Action()), state_.actorId,
               static_cast<int>(candidate.TargetId()),
               static_cast<int>(candidate.TargetLimb()), &source_position,
               &target_position,
               reinterpret_cast<Battle_ActionRequest*>(
                   Battle_Ai_planningWorkspace)) != 0;
  }

  bool ApplyResults() override {
    return ApplyLiveSimulatedActionResults(rows_);
  }

 private:
  bool Ready(const ActionCandidate& candidate) const {
    if (state_.actorId >= std::size(Battle_actors) ||
        candidate.TargetId() >= std::size(Battle_actors) ||
        !ReadablePlanningBytes(Battle_Ai_planningWorkspace,
                               sizeof(Battle_ActionRequest),
                               alignof(Battle_ActionRequest))) {
      return false;
    }
    const auto* source = Battle_actors[state_.actorId];
    const auto* target = Battle_actors[candidate.TargetId()];
    return ReadablePlanningRecord(source) && ReadablePlanningRecord(target) &&
           ReadablePlanningRecord(source->stats) &&
           ReadablePlanningRecord(target->stats);
  }

  Battle_Ai_ActorAiState& state_;
  std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows_;
};

class LiveCandidateEvaluation final : public CandidateEvaluationHost {
 public:
  LiveCandidateEvaluation(Battle_Ai_ActorAiState& state,
                          Battle_Ai_ActionCandidate& candidate)
      : state_(state), candidate_(candidate) {}

  int Random() override { return Core_Random_Rand(); }
  bool Capture(ActionCandidate& candidate) override {
    candidate_ = RecordOf(candidate);
    CaptureBattleActorConditions(rows_);
    return true;
  }
  bool SpecialMode() const override { return state_.settingsModeLow != 0; }
  bool Simulate(ActionCandidate& candidate,
                CandidateSimulationMode mode) override {
    candidate_ = RecordOf(candidate);
    if (!SimulateLiveActionCandidate(&state_, rows_, &candidate_, mode)) {
      return false;
    }
    candidate = CandidateOf(&candidate_);
    return true;
  }
  std::optional<std::span<const SimulatedActorEffect>> Hits() override {
    auto snapshot = ReadSimulationSnapshot();
    if (!snapshot) return std::nullopt;
    hits_ = std::move(snapshot->targets);
    return std::span<const SimulatedActorEffect>(hits_);
  }
  std::optional<std::span<const CandidateCondition>> Conditions() override {
    std::size_t count = 0;
    for (const auto* actor = Battle_actors[0]; actor != nullptr;
         actor = actor->next) {
      if (count >= conditions_.size() || !ReadablePlanningRecord(actor) ||
          actor->id >= kBattleActorSlotCount) {
        return std::nullopt;
      }
      auto& condition = conditions_[count++];
      condition.classification = state_.relations[actor->id].classification;
      if (condition.classification == 1) continue;
      const auto& row = rows_[actor->id];
      const auto* ai_state = Battle_Ai_actorAiStates[row.opaque0Bit0];
      if (!ReadablePlanningRecord(ai_state)) return std::nullopt;
      condition.condition = SimulationConditionOf(row);
      condition.action_cost_enabled = ai_state->actionCostEnabled != 0;
    }
    return std::span<const CandidateCondition>(conditions_).first(count);
  }

 private:
  Battle_Ai_ActorAiState& state_;
  Battle_Ai_ActionCandidate& candidate_;
  std::array<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows_{};
  std::array<CandidateCondition, kBattleActorSlotCount> conditions_{};
  std::vector<SimulatedActorEffect> hits_;
};

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
    if (VS::Actors::LiveActorPlacement(static_cast<int>(target), &placement) !=
        0) {
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
class LiveFallbackActionSelection final : public FallbackActionHost {
 public:
  LiveFallbackActionSelection(Battle_Ai_ActorAiState& state,
                              const Battle_ActorStats& stats)
      : state_(state), stats_(stats) {}

  std::optional<FallbackActionSlot> RefreshSlot(int row, int column) override {
    const unsigned action = stats_.limbActions[row][column].actionId;
    if (!ReadablePlanningRecord(state_.actor) ||
        !ReadablePlanningRecord(state_.actor->opaque3C)) {
      return std::nullopt;
    }
    LiveActionUsability usability(state_, *state_.actor->opaque3C);
    const auto usable = RefreshAiActionUsability(usability, row, column);
    if (!usable) return std::nullopt;
    const auto& slot = state_.actionSlots[row][column];
    const bool eligible = *usable && slot.special != 0;
    return FallbackActionSlot{.action = action,
                              .range_squared = eligible ? slot.rangeSquared : 0,
                              .eligible = eligible};
  }
  bool IgnoresSight(unsigned action) const override {
    return Battle_actionDefinitions[action].aiFlags.opaque1Bit4 != 0;
  }

 private:
  Battle_Ai_ActorAiState& state_;
  const Battle_ActorStats& stats_;
};

class LiveCandidateReadiness final : public CandidateReadinessHost {
 public:
  explicit LiveCandidateReadiness(Battle_Ai_ActorAiState& state)
      : state_(state), room_(Battle_Ai_roomAiState) {
    for (std::size_t index = 0; index < std::size(Battle_Ai_actorAiStates);
         ++index) {
      if (Battle_Ai_actorAiStates[index] == &state_) slot_ = index;
    }
  }
  int Count() const override { return Valid() ? state_.candidateCount : -1; }
  std::optional<std::size_t> RankedHandle(std::size_t rank) const override {
    if (!Valid() || rank >= std::size(state_.candidateOrder)) {
      return std::nullopt;
    }
    const auto* candidate = state_.candidateOrder[rank];
    return ReadablePlanningRecord(candidate)
               ? std::optional(reinterpret_cast<std::size_t>(candidate))
               : std::nullopt;
  }
  std::optional<ActionCandidate> Candidate(std::size_t handle) const override {
    const auto* record = Record(handle);
    return record != nullptr ? std::optional(CandidateOf(record))
                             : std::nullopt;
  }
  std::uint16_t ReadinessLevel() const override {
    return Valid() && ReadablePlanningRecord(room_) ? room_->readinessLevel : 0;
  }
  std::optional<bool> CanAffect(std::size_t handle, unsigned target) override {
    const auto* record = Record(handle);
    if (record == nullptr) return std::nullopt;
    const LiveTargeting host(state_);
    const auto result = CanAiActionAffectActor(host, CandidateOf(record),
                                               static_cast<int>(target));
    return Valid() ? result : std::nullopt;
  }
  std::optional<bool> RefreshUsability(unsigned row, unsigned column) override {
    if (!Valid() || !ReadablePlanningRecord(state_.actor) ||
        !ReadablePlanningRecord(state_.actor->opaque3C)) {
      return std::nullopt;
    }
    LiveActionUsability host(state_, *state_.actor->opaque3C);
    const auto result = RefreshAiActionUsability(host, static_cast<int>(row),
                                                 static_cast<int>(column));
    return Valid() ? result : std::nullopt;
  }
  bool ClearWeight(std::size_t handle) override {
    auto* record = Record(handle);
    if (record == nullptr) return false;
    record->weight = 0;
    return true;
  }
  bool SetFirstRank(std::size_t handle) override {
    auto* record = Record(handle);
    if (record == nullptr) return false;
    state_.candidateOrder[0] = record;
    return true;
  }
  bool ClearCount() override {
    if (!Valid()) return false;
    state_.candidateCount = 0;
    return true;
  }
  bool SelectMovementMode() override {
    if (!Valid() || !ReadablePlanningRecord(state_.candidateOrder[0])) {
      return false;
    }
    SelectLiveActionMovementMode(
        reinterpret_cast<Battle_Ai_CandidateReadinessView*>(&state_));
    return Valid();
  }
  std::optional<bool> HasHostileActor() const override {
    constexpr unsigned kHostile = 3;
    if (!Valid()) return std::nullopt;
    std::array<const Battle_Actor*, kCandidateTargetSlots> visited{};
    std::size_t count = 0;
    for (const auto* actor = Battle_actors[0]; actor != nullptr;
         actor = actor->next) {
      if (!ReadablePlanningRecord(actor) || actor->id < 0 ||
          actor->id >= kCandidateTargetSlots || count == visited.size() ||
          std::find(visited.begin(), visited.end(), actor) != visited.end()) {
        return std::nullopt;
      }
      visited[count++] = actor;
      if (state_.relations[actor->id].classification == kHostile) return true;
    }
    return false;
  }
  bool StoreActionStatus(std::uint8_t status) override {
    if (!Valid()) return false;
    state_.actionStatus = status;
    return true;
  }

 private:
  bool Valid() const {
    return ReadablePlanningRecord(&state_) && room_ == Battle_Ai_roomAiState &&
           (!slot_ || Battle_Ai_actorAiStates[*slot_] == &state_);
  }
  Battle_Ai_ActionCandidate* Record(std::size_t handle) const {
    auto* record = reinterpret_cast<Battle_Ai_ActionCandidate*>(handle);
    return Valid() && ReadablePlanningRecord(record) ? record : nullptr;
  }
  Battle_Ai_ActorAiState& state_;
  Battle_Ai_RoomAiState* room_;
  std::optional<std::size_t> slot_;
};
}  // namespace

bool ApplyLiveSimulatedActionResults(
    std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows) {
  if (!ReadablePlanningBytes(rows.data(), rows.size_bytes(),
                             alignof(Battle_Ai_ActionTiming))) {
    return false;
  }
  const auto snapshot = ReadSimulationSnapshot();
  if (!snapshot) return false;
  const auto& header = snapshot->header;
  if (OverlappingPlanningBytes(rows.data(), rows.size_bytes(), snapshot->source,
                               snapshot->bytes)) {
    return false;
  }
  ActionCost cost{};
  if (header.opaque4.opaque40 == 0) {
    if (header.opaque0 >= std::size(Battle_actionDefinitions)) return false;
    const auto& action = Battle_actionDefinitions[header.opaque0];
    cost = {.type = action.type, .cost = action.cost};
  }
  std::array<ActorCondition, kBattleActorSlotCount> conditions{};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    conditions[index] = SimulationConditionOf(rows[index]);
  }
  if (!ApplySimulatedActionResults(conditions, cost,
                                   SimulationEffectOf(header.opaque4),
                                   snapshot->targets) ||
      snapshot->source != Battle_Ai_planningWorkspace) {
    return false;
  }
  for (std::size_t index = 0; index < rows.size(); ++index) {
    rows[index].opaque4 = conditions[index].hp;
    rows[index].opaque8 = conditions[index].mp;
    rows[index].opaqueC = conditions[index].statuses;
    rows[index].opaque10 = conditions[index].weapon_dp;
    rows[index].opaque14 = conditions[index].shield_dp;
  }
  return true;
}

bool SimulateLiveActionCandidate(
    Battle_Ai_ActorAiState* state,
    std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows,
    const Battle_Ai_ActionCandidate* candidate, CandidateSimulationMode mode) {
  if (!ReadablePlanningRecord(state) || !ReadablePlanningRecord(candidate) ||
      !ReadablePlanningBytes(rows.data(), rows.size_bytes(),
                             alignof(Battle_Ai_ActionTiming))) {
    return RefusePlanning(
        "AI simulation requires readable state candidate and rows");
  }
  if (!ReadablePlanningBytes(Battle_Ai_planningWorkspace,
                             sizeof(Battle_ActionRequest),
                             alignof(Battle_ActionRequest)) ||
      OverlappingPlanningBytes(rows.data(), rows.size_bytes(),
                               Battle_Ai_planningWorkspace,
                               sizeof(Battle_ActionRequest)) ||
      OverlappingPlanningBytes(state, sizeof(*state),
                               Battle_Ai_planningWorkspace,
                               sizeof(Battle_ActionRequest)) ||
      OverlappingPlanningBytes(candidate, sizeof(*candidate),
                               Battle_Ai_planningWorkspace,
                               sizeof(Battle_ActionRequest))) {
    return RefusePlanning(
        "AI simulation requires a separate complete result workspace");
  }
  LiveCandidateSimulation host(*state, rows);
  return SimulateActionCandidate(host, CandidateOf(candidate), mode);
}

bool UpdateLiveCandidateHitWeight(Battle_Ai_ActionCandidate* candidate) {
  if (!ReadablePlanningRecord(candidate)) {
    return RefusePlanning("AI hit weighting requires a readable candidate");
  }
  const auto snapshot = ReadSimulationSnapshot();
  if (!snapshot) {
    return RefusePlanning("AI hit weighting requires a complete result tail");
  }
  auto value = CandidateOf(candidate);
  UpdateCandidateHitWeight(value, snapshot->targets);
  *candidate = RecordOf(value);
  return true;
}

bool EvaluateLiveActionCandidate(Battle_Ai_ActorAiState* state,
                                 Battle_Ai_ActionCandidate* candidate,
                                 int simulation_mode,
                                 const Battle_ActionDefinition* action) {
  if (!ReadablePlanningRecord(state) || !ReadablePlanningRecord(candidate) ||
      (action != nullptr && !ReadablePlanningRecord(action))) {
    return RefusePlanning(
        "AI evaluation requires readable state candidate and action");
  }
  CandidateEvaluationOptions options{
      .rarely_chosen = action != nullptr && action->aiFlags.rarelyChosen != 0,
      .special_action = action != nullptr && action->aiFlags.opaque0Bit4 != 0};
  if (simulation_mode != 0) {
    options.simulation = simulation_mode == kAimedSimulationMode
                             ? CandidateSimulationMode::kAimed
                             : CandidateSimulationMode::kLimb;
  }
  auto value = CandidateOf(candidate);
  LiveCandidateEvaluation host(*state, *candidate);
  if (!EvaluateActionCandidate(value, options, host)) return false;
  *candidate = RecordOf(value);
  return true;
}

int LiveActorStatusConditionScore(const Battle_Ai_ActionTiming* row) {
  Require(row != nullptr && ReadablePlanningRecord(row),
          "AI actor status score requires a condition row");
  const Battle_Ai_ActorAiState* state =
      Battle_Ai_actorAiStates[row->opaque0Bit0];
  Require(state != nullptr,
          "AI actor status score requires the actor's AI state");
  return ActorStatusConditionScore(SimulationConditionOf(*row),
                                   state->actionCostEnabled != 0);
}

int LiveActorHpRatio(const Battle_Ai_ActionTiming* rows, int id) {
  return ActorHpRatio(SimulationConditionOf(ConditionRowOf(rows, id)));
}

int LiveActorMpRatio(const Battle_Ai_ActionTiming* rows, int id) {
  return ActorMpRatio(SimulationConditionOf(ConditionRowOf(rows, id)));
}

int LiveActorEquipmentDpRatio(const Battle_Ai_ActionTiming* rows, int id) {
  return ActorEquipmentDpRatio(SimulationConditionOf(ConditionRowOf(rows, id)));
}

std::optional<bool> RefreshLiveActionCandidateReadiness(
    Battle_Ai_ActorAiState* state) {
  if (!ReadablePlanningRecord(state) ||
      state->actorId >= kCandidateTargetSlots || state->candidateCount < 0 ||
      state->candidateCount > static_cast<int>(kCandidateCapacity) ||
      (state->candidateCount > 0 &&
       !ReadablePlanningRecord(Battle_Ai_roomAiState))) {
    Core_Debugf("ai", "AI readiness state/count/room is unavailable");
    return std::nullopt;
  }
  LiveCandidateReadiness host(*state);
  return RefreshActionCandidateReadiness(host);
}

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

void SelectLiveFallbackAction(Battle_Ai_ActorAiState* state) {
  Require(ReadablePlanningRecord(state) && ReadablePlanningRecord(state->stats),
          "AI fallback selection requires a complete state and stats record");
  LiveFallbackActionSelection host(*state, *state->stats);
  auto choice = CandidateOf(&state->currentChoice);
  Require(SelectFallbackAction(choice, host),
          "AI fallback selection refused invalid input");
  state->currentChoice = RecordOf(choice);
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

bool SnapshotLivePlanningActorStats() {
  if (!planning_stats_snapshot.bytes.empty() ||
      Battle_Ai_planningStatsSnapshot != nullptr) {
    return RefusePlanning("AI stat capture is already active");
  }
  auto* storage = static_cast<std::byte*>(
      Core::Memory::AllocateLiveFromHighEnd(kPlanningStatsTableBytes));
  if (storage == nullptr) {
    return RefusePlanning("AI stat capture has no snapshot storage");
  }
  planning_stats_snapshot = {
      .bytes = std::span<std::byte>(storage, kPlanningStatsTableBytes)};
  Battle_Ai_planningStatsSnapshot =
      reinterpret_cast<Battle_ActorStats*>(storage);
  std::array<PlanningActorStats, kBattleActorSlotCount> rows{};
  const auto actors = ReadPlanningStatsRows(rows);
  if (actors && SnapshotPlanningActorStats(planning_stats_snapshot, *actors)) {
    return true;
  }
  Core::Memory::FreeLiveFromHighEnd(storage);
  InvalidateLivePlanningStatsSnapshot();
  return RefusePlanning("AI stat capture requires a valid disjoint actor list");
}

void RestoreLivePlanningActorStats() {
  const auto storage = planning_stats_snapshot.bytes;
  const auto allocation = Core::Memory::LivePayloadContaining(storage.data());
  Require(allocation.has_value() && allocation->data() == storage.data() &&
              allocation->size() >= kPlanningStatsTableBytes &&
              reinterpret_cast<std::byte*>(Battle_Ai_planningStatsSnapshot) ==
                  storage.data(),
          "AI stat restore requires its live snapshot allocation");
  std::array<PlanningActorStats, kBattleActorSlotCount> rows{};
  const auto actors = ReadPlanningStatsRows(rows);
  Require(actors.has_value() &&
              RestorePlanningActorStats(planning_stats_snapshot, *actors),
          "AI stat restore requires a valid captured actor list");
  Core::Memory::FreeLiveFromHighEnd(storage.data());
  InvalidateLivePlanningStatsSnapshot();
}

void InvalidateLivePlanningStatsSnapshot() {
  planning_stats_snapshot = {};
  Battle_Ai_planningStatsSnapshot = nullptr;
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

int Battle_Ai_RefreshActionCandidateReadinessEntry(
    Battle_Ai_CandidateReadinessView* state) {
  return VS::Battle::Ai::RefreshLiveActionCandidateReadiness(
             reinterpret_cast<Battle_Ai_ActorAiState*>(state))
      .value_or(false);
}

void Battle_Ai_ApplySimulatedActionResultsEntry(Battle_Ai_ActionTiming* rows) {
  if (rows == nullptr) return;
  static_cast<void>(VS::Battle::Ai::ApplyLiveSimulatedActionResults(
      std::span<Battle_Ai_ActionTiming, VS::Battle::Ai::kBattleActorSlotCount>(
          rows, VS::Battle::Ai::kBattleActorSlotCount)));
}

void Battle_Ai_SimulateActionCandidateEntry(
    Battle_Ai_ActorAiState* state, Battle_Ai_ActionTiming* rows,
    Battle_Ai_ActionCandidate* candidate, int mode) {
  constexpr int kAimedMode = 3;
  if (rows != nullptr &&
      VS::Battle::Ai::SimulateLiveActionCandidate(
          state,
          std::span<Battle_Ai_ActionTiming,
                    VS::Battle::Ai::kBattleActorSlotCount>(
              rows, VS::Battle::Ai::kBattleActorSlotCount),
          candidate,
          mode == kAimedMode
              ? VS::Battle::Ai::CandidateSimulationMode::kAimed
              : VS::Battle::Ai::CandidateSimulationMode::kLimb)) {
    return;
  }
  Core_Errorf("ai", "AI candidate simulation entry refused invalid input");
  std::abort();
}

void Battle_Ai_UpdateCandidateHitWeightEntry(
    Battle_Ai_ActionCandidate* candidate) {
  if (VS::Battle::Ai::UpdateLiveCandidateHitWeight(candidate)) return;
  Core_Errorf("ai", "AI candidate hit-weight entry refused invalid input");
  std::abort();
}

void Battle_Ai_EvaluateActionCandidateEntry(
    Battle_Ai_ActorAiState* state, Battle_Ai_ActionCandidate* candidate,
    int mode, Battle_ActionDefinition* action) {
  if (VS::Battle::Ai::EvaluateLiveActionCandidate(state, candidate, mode,
                                                  action)) {
    return;
  }
  Core_Errorf("ai", "AI candidate evaluation entry refused invalid input");
  std::abort();
}

void Battle_Ai_SelectFallbackActionEntry(Battle_Ai_ActorAiState* state) {
  VS::Battle::Ai::SelectLiveFallbackAction(state);
}
