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

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/Location.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionPreviewLive.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/ConditionSnapshotBinding.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/PlanningWorkspace.hpp"
#include "Battle/Damage.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/TargetingLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/Gimmicks.hpp"
#include "State/ScriptFlagsLive.hpp"

using VS::Core::Gte::SVECTOR;

using VS::Battle::ActionDefinition;
using VS::Battle::ActionRequestRecord;

namespace VS::Battle::Ai {
namespace {
static_assert(sizeof(VS::Battle::Ai::ActionCandidateRecord) ==
              sizeof(ActionCandidate::Words));
static_assert(sizeof(VS::Battle::ActorStats) == kActorStatsRecordBytes);
static_assert(offsetof(VS::Battle::Ai::EngagementData, opaque9) ==
              offsetof(VS::Battle::Actor, command_opcode));
static_assert(offsetof(VS::Battle::Ai::EngagementData, opaque_3c) ==
              offsetof(VS::Battle::Actor, stats));
static_assert(offsetof(VS::Battle::ActorStats, current_hp) ==
              kSnapshotVitals.offset);
static_assert(offsetof(VS::Battle::ActorStats, movement_class) ==
              kSnapshotMovement.offset);
static_assert(offsetof(VS::Battle::ActorStats, limbs) == kSnapshotLimbs.offset);
static_assert(sizeof(VS::Battle::ActorStats::limbs) == kSnapshotLimbs.size);
static_assert(offsetof(VS::Battle::ActorStats, statuses) ==
              kSnapshotStatuses.offset);
static_assert(std::size(VS::Battle::actionDefinitions) ==
              kActionDefinitionCount);
static_assert(std::size(VS::Battle::Ai::actorAiStates) ==
              kCandidateTargetSlots);
static_assert(
    std::tuple_size_v<decltype(VS::Battle::Ai::ActorAiState::candidates)> ==
    kCandidateCapacity);
static_assert(std::tuple_size_v<
                  decltype(VS::Battle::Ai::ActionPlanView::candidate_order)> ==
              kCandidateCapacity);
constexpr int kRestoreOperation = 2;
constexpr int kAimedSimulationMode = 3;
constexpr std::size_t kPoisonStatusIndex = 14;
constexpr std::size_t kRegenerationStatusIndex = 17;
constexpr auto kTargetOffset = offsetof(ReactionRecord, opaque_4c);
PlanningStatsSnapshot liveStatsSnapshot;
static_assert(std::tuple_size_v<decltype(ActionRequestRecord::targets)> ==
              kTargetPreviewCapacity);
static_assert(kTargetOffset == offsetof(ActionRequestRecord, targets));
static_assert(sizeof(VS::Battle::Ai::EngagementActor) ==
              sizeof(VS::Battle::HitResult));
// func_800DEB10 reads the completed request through func_800DEEA4_t and the
// performer's command argument as two halves (lhu +0x0C, lhu +0x0E).
static_assert(offsetof(VS::Battle::Ai::ReactionRecord, opaque4) ==
              offsetof(ActionRequestRecord, primary_target));
static_assert(offsetof(VS::Battle::Ai::ReactionRecord, opaque_4a) ==
              offsetof(ActionRequestRecord, target_count));
static_assert(offsetof(VS::Battle::Ai::EngagementActor, opaque40) ==
              offsetof(VS::Battle::HitResult, targets_position));
static_assert(offsetof(VS::Battle::Ai::EngagementData, opaque_c) ==
              offsetof(VS::Battle::Actor, command_arg0));
static_assert(offsetof(VS::Battle::Ai::EngagementData, opaque_e) ==
              offsetof(VS::Battle::Actor, command_arg0) +
                  sizeof(std::uint16_t));
static_assert(std::size(VS::Field::gimmickInstances) == kCandidateTargetSlots);

static_assert(sizeof(VS::Battle::Ai::CandidateEntryView) ==
              sizeof(ActionCandidate::Words));
static_assert(offsetof(VS::Battle::Ai::CandidateReadinessView,
                       candidate_count) ==
              offsetof(VS::Battle::Ai::ActorAiState, candidate_count));
static_assert(offsetof(VS::Battle::Ai::CandidateReadinessView,
                       candidate_order) ==
              offsetof(VS::Battle::Ai::ActorAiState, candidate_order));
static_assert(offsetof(VS::Battle::Ai::CandidateReadinessView, action_status) ==
              offsetof(VS::Battle::Ai::ActorAiState, action_status));
static_assert(offsetof(VS::Battle::Ai::CandidateReadinessView, relations) ==
              offsetof(VS::Battle::Ai::ActorAiState, relations));

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
    const VS::Battle::Ai::EngagementActor& entry) {
  return {
      .actor_id = entry.opaque0,
      .ignored = entry.opaque40 != 0,
      .hp = {entry.opaque4, static_cast<std::uint8_t>(entry.opaque_1c_bit0)},
      .mp = {entry.opaque6, static_cast<std::uint8_t>(entry.opaque_1c_bit2)},
      .weapon_dp = {entry.opaque38,
                    static_cast<std::uint8_t>(entry.opaque_1e_bit2)},
      .shield_dp = {entry.opaque_3c,
                    static_cast<std::uint8_t>(entry.opaque_1e_bit6)},
      .statuses_set = entry.opaque14,
      .statuses_clear = entry.opaque18,
      .hit_threshold = entry.opaque2};
}

struct SimulationSnapshot {
  const VS::Battle::Ai::ReactionRecord* source = nullptr;
  VS::Battle::Ai::ReactionRecord header{};
  std::size_t bytes = 0;
  std::vector<SimulatedActorEffect> targets;
};

std::optional<SimulationSnapshot> ReadSimulationSnapshot() {
  const auto* workspace = VS::Battle::Ai::planningWorkspace;
  if (!ReadablePlanningRecord(workspace)) return std::nullopt;
  SimulationSnapshot result;
  result.source = workspace;
  std::memcpy(&result.header, workspace, sizeof(result.header));
  const auto count = static_cast<std::size_t>(result.header.opaque_4a);
  result.bytes =
      kTargetOffset + count * sizeof(VS::Battle::Ai::EngagementActor);
  if (count > kTargetPreviewCapacity ||
      !ReadablePlanningBytes(workspace, result.bytes,
                             alignof(VS::Battle::Ai::ReactionRecord))) {
    return std::nullopt;
  }
  result.targets.reserve(count);
  const auto* source = reinterpret_cast<const std::byte*>(workspace);
  for (std::size_t index = 0; index < count; ++index) {
    VS::Battle::Ai::EngagementActor entry{};
    std::memcpy(&entry, source + kTargetOffset + index * sizeof(entry),
                sizeof(entry));
    result.targets.push_back(SimulationEffectOf(entry));
  }
  return result;
}

bool RefusePlanning(const char* message) {
  VS::Core::Debugf("ai", "%s", message);
  return false;
}

std::optional<std::span<const PlanningActorStats>> ReadPlanningStatsRows(
    std::array<PlanningActorStats, kBattleActorSlotCount>& rows) {
  std::size_t count = 0;
  for (const auto* actor = VS::Battle::actors[0]; actor != nullptr;
       actor = actor->next) {
    if (count >= rows.size() || !ReadablePlanningRecord(actor) ||
        actor->id < 0 || actor->id >= kBattleActorSlotCount ||
        !ReadablePlanningRecord(actor->stats)) {
      return std::nullopt;
    }
    auto bytes = std::as_writable_bytes(
        std::span<VS::Battle::ActorStats, 1>(actor->stats, 1));
    if (OverlappingPlanningBytes(bytes.data(), bytes.size(),
                                 liveStatsSnapshot.bytes.data(),
                                 liveStatsSnapshot.bytes.size())) {
      return std::nullopt;
    }
    rows[count++] = {.slot = static_cast<std::size_t>(actor->id),
                     .bytes = bytes};
  }
  return std::span<const PlanningActorStats>(rows).first(count);
}

ActorCondition SimulationConditionOf(const VS::Battle::Ai::ActionTiming& row) {
  return {row.opaque4,  row.opaque6,  row.opaque8,  row.opaque_a, row.opaque_c,
          row.opaque10, row.opaque12, row.opaque14, row.opaque16};
}

const VS::Battle::Ai::ActionTiming& ConditionRowOf(
    const VS::Battle::Ai::ActionTiming* rows, int id) {
  Require(rows != nullptr && id >= 0 && id < kBattleActorSlotCount,
          "AI actor condition requires a row in the planner's table");
  const VS::Battle::Ai::ActionTiming& row = rows[id];
  Require(ReadablePlanningRecord(&row), "AI actor condition row is unreadable");
  return row;
}

ActionCandidate CandidateOf(
    const VS::Battle::Ai::ActionCandidateRecord* candidate) {
  Require(candidate != nullptr, "AI planning requires a candidate");
  return ActionCandidate(std::bit_cast<ActionCandidate::Words>(*candidate));
}

VS::Battle::Ai::ActionCandidateRecord RecordOf(
    const ActionCandidate& candidate) {
  return std::bit_cast<VS::Battle::Ai::ActionCandidateRecord>(
      candidate.RawWords());
}

class LiveCandidateSimulation final : public CandidateSimulationHost {
 public:
  LiveCandidateSimulation(
      VS::Battle::Ai::ActorAiState& state,
      std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows)
      : state_(state), rows_(rows) {}

  std::optional<TargetPoint> TargetPosition(unsigned target) const override {
    if (target >= std::size(VS::Actors::actorRecords)) return std::nullopt;
    const auto* actor = VS::Actors::actorRecords[target];
    if (!ReadablePlanningRecord(actor)) return std::nullopt;
    const auto& position = actor->instance.position;
    return TargetPoint{position.vx, position.vy, position.vz};
  }

  std::optional<bool> PreviewLimb(const ActionCandidate& candidate) override {
    if (!Ready(candidate)) return std::nullopt;
    return PreviewLiveActionAgainstLimb(
               static_cast<int>(candidate.Action()), state_.actor_id,
               static_cast<int>(candidate.TargetId()),
               static_cast<int>(candidate.TargetLimb()),
               VS::Battle::Ai::planningWorkspace) != 0;
  }

  std::optional<bool> PreviewAimed(const ActionCandidate& candidate,
                                   const TargetPoint& source,
                                   const TargetPoint& target) override {
    if (!Ready(candidate)) return std::nullopt;
    SVECTOR source_position{source.x, source.y, source.z, source.pad};
    SVECTOR target_position{target.x, target.y, target.z, target.pad};
    return PreviewLiveActionAgainstActor(
               static_cast<int>(candidate.Action()), state_.actor_id,
               static_cast<int>(candidate.TargetId()),
               static_cast<int>(candidate.TargetLimb()), &source_position,
               &target_position,
               reinterpret_cast<ActionRequestRecord*>(
                   VS::Battle::Ai::planningWorkspace)) != 0;
  }

  bool ApplyResults() override {
    return ApplyLiveSimulatedActionResults(rows_);
  }

 private:
  bool Ready(const ActionCandidate& candidate) const {
    if (state_.actor_id >= std::size(VS::Battle::actors) ||
        candidate.TargetId() >= std::size(VS::Battle::actors) ||
        !ReadablePlanningBytes(VS::Battle::Ai::planningWorkspace,
                               sizeof(ActionRequestRecord),
                               alignof(ActionRequestRecord))) {
      return false;
    }
    const auto* source = VS::Battle::actors[state_.actor_id];
    const auto* target = VS::Battle::actors[candidate.TargetId()];
    return ReadablePlanningRecord(source) && ReadablePlanningRecord(target) &&
           ReadablePlanningRecord(source->stats) &&
           ReadablePlanningRecord(target->stats);
  }

  VS::Battle::Ai::ActorAiState& state_;
  std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows_;
};

class LiveCandidateEvaluation final : public CandidateEvaluationHost {
 public:
  LiveCandidateEvaluation(VS::Battle::Ai::ActorAiState& state,
                          VS::Battle::Ai::ActionCandidateRecord& candidate)
      : state_(state), candidate_(candidate) {}

  int Random() override { return VS::Core::NextBiosRand(); }
  bool Capture(ActionCandidate& candidate) override {
    candidate_ = RecordOf(candidate);
    CaptureBattleActorConditions(rows_);
    return true;
  }
  bool SpecialMode() const override { return state_.settings_mode_low != 0; }
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
    for (const auto* actor = VS::Battle::actors[0]; actor != nullptr;
         actor = actor->next) {
      if (count >= conditions_.size() || !ReadablePlanningRecord(actor) ||
          actor->id >= kBattleActorSlotCount) {
        return std::nullopt;
      }
      auto& condition = conditions_[count++];
      condition.classification = state_.relations[actor->id].classification;
      if (condition.classification == 1) continue;
      const auto& row = rows_[actor->id];
      const auto* ai_state = VS::Battle::Ai::actorAiStates[row.opaque0_bit0];
      if (!ReadablePlanningRecord(ai_state)) return std::nullopt;
      condition.condition = SimulationConditionOf(row);
      condition.action_cost_enabled = ai_state->action_cost_enabled != 0;
    }
    return std::span<const CandidateCondition>(conditions_).first(count);
  }

 private:
  VS::Battle::Ai::ActorAiState& state_;
  VS::Battle::Ai::ActionCandidateRecord& candidate_;
  std::array<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows_{};
  std::array<CandidateCondition, kBattleActorSlotCount> conditions_{};
  std::vector<SimulatedActorEffect> hits_;
};

const VS::Battle::ActorStats& StatsOf(
    const VS::Battle::Ai::ActorAiState& state) {
  Require(state.actor != nullptr && state.actor->opaque_3c != nullptr,
          "AI planning requires the actor's stats");
  return *state.actor->opaque_3c;
}

std::uint16_t Half(int value) { return static_cast<std::uint16_t>(value); }

ActorResources ResourcesOf(const VS::Battle::ActorStats& stats) {
  return {.hp = Half(stats.current_hp),
          .max_hp = Half(stats.max_hp),
          .mp = Half(stats.current_mp),
          .max_mp = Half(stats.max_mp),
          .weapon_pp = Half(stats.weapon.current_pp),
          .statuses = static_cast<std::uint32_t>(stats.statuses)};
}

class LiveTargetMemory final : public TargetMemoryHost {
 public:
  explicit LiveTargetMemory(VS::Battle::Ai::ActorAiState& state)
      : state_(state) {}

  [[nodiscard]] int CandidateCount() const override {
    return state_.candidate_count;
  }
  [[nodiscard]] bool MemoryDisabled() const override {
    return state_.settings_constant19 != 0;
  }
  [[nodiscard]] std::optional<unsigned> FirstRankedTarget() const override {
    const VS::Battle::Ai::ActionCandidateRecord* first =
        state_.candidate_order[0];
    if (first == nullptr) return std::nullopt;
    return first->target_id;
  }
  [[nodiscard]] std::uint32_t OwnTile() const override {
    return state_.tile.word;
  }
  [[nodiscard]] std::optional<std::uint32_t> TargetTile(
      unsigned target) const override {
    if (VS::Actors::actorRecords[target] == nullptr) return std::nullopt;
    VS::Actors::ActorLocationQuery placement{};
    if (VS::Actors::LiveActorPlacement(static_cast<int>(target), &placement) !=
        0) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(placement.location.packed_tile.word);
  }
  void Remember(const TargetMemory& memory) override {
    state_.previous_tile = static_cast<int>(memory.previous_tile);
    state_.remembered_target_tile = memory.remembered_target_tile;
    state_.target_memory_frames = memory.frames;
  }
  void SetPreviousTile(std::uint32_t tile) override {
    state_.previous_tile = static_cast<int>(tile);
  }

 private:
  VS::Battle::Ai::ActorAiState& state_;
};

class LiveCandidateList final : public CandidateList {
 public:
  explicit LiveCandidateList(VS::Battle::Ai::ActorAiState& state)
      : state_(state) {}

  [[nodiscard]] int Count() const override { return state_.candidate_count; }
  void Store(std::size_t slot, const ActionCandidate& candidate) override {
    VS::Battle::Ai::ActionCandidateRecord& stored =
        std::span(state_.candidates)[slot];
    stored = RecordOf(candidate);
    std::span(state_.candidate_order)[slot] = &stored;
  }
  void SetCount(int count) override { state_.candidate_count = count; }
  [[nodiscard]] bool ExcludeTargetAction(unsigned target,
                                         unsigned action) override {
    if (VS::Battle::Ai::candidateExclusions == nullptr) return false;
    VS::Battle::Ai::candidateExclusions[target][action] = 1;
    return true;
  }

 private:
  VS::Battle::Ai::ActorAiState& state_;
};

class LiveActorActionPlan final : public ActorActionPlanHost {
 public:
  explicit LiveActorActionPlan(VS::Battle::Ai::ActorAiState& state)
      : state_(state), initial_stats_(state.stats), list_(state) {}

  CandidateList& Candidates() override { return list_; }
  void ResetFallbackCache() override {
    VS::Battle::Ai::fallbackCandidateCount = 0;
    VS::Battle::Ai::unusedPlanCounter = 0;
  }
  bool ClearConfiguredExclusions() override {
    for (int row = 0; row < kLimbActionRows; ++row) {
      for (int column = 0; column < kLimbActionColumns; ++column) {
        std::size_t visited = 0;
        for (auto* actor = VS::Battle::actors[0]; actor != nullptr;
             actor = actor->next) {
          if (++visited > kBattleActorSlotCount ||
              !ReadablePlanningRecord(actor) || actor->id < 0 ||
              actor->id >= kBattleActorSlotCount ||
              !ReadablePlanningRecord(initial_stats_)) {
            return false;
          }
          const auto action =
              initial_stats_->limb_actions[row][column].action_id;
          if (!ClearExclusion(static_cast<unsigned>(actor->id), action)) {
            return false;
          }
        }
      }
    }
    return true;
  }
  std::uint8_t ActorId() const override { return state_.actor_id; }
  bool SnapshotStats() override { return SnapshotLivePlanningActorStats(); }
  void RestoreStats() noexcept override { RestoreLivePlanningActorStats(); }
  void ResetReadiness() noexcept override {
    Require(ReadablePlanningRecord(VS::Battle::Ai::roomAiState),
            "Action plan requires a complete readiness clock");
    VS::Battle::Ai::roomAiState->readiness_level = 0;
  }
  std::optional<ActionPlanTiming> Timing() const override {
    if (!ReadablePlanningRecord(state_.stats)) return std::nullopt;
    return ActionPlanTiming{state_.min_action_delay,
                            state_.stats->action_recovery_timer};
  }
  void AdvanceSimulation(int step) override {
    AdvanceLivePlanningSimulation(step);
  }
  void Enumerate(std::uint16_t threshold) override {
    EnumerateLiveActionCandidates(&state_, threshold);
  }
  void Sort() override { SortLiveActionCandidates(PlanView()); }
  std::optional<ActionCandidate> Ranked(std::size_t rank) const override {
    const auto* record = RankedRecord(rank);
    return record ? std::optional(CandidateOf(record)) : std::nullopt;
  }
  bool StorePass(std::size_t pass, std::size_t rank,
                 const ActionCandidate& candidate) override {
    auto* record = PassRecord(pass, rank);
    if (record == nullptr) return false;
    *record =
        std::bit_cast<VS::Battle::Ai::CandidateEntryView>(candidate.RawWords());
    return true;
  }
  std::optional<ActionCandidate> Pass(std::size_t pass,
                                      std::size_t rank) const override {
    const auto* record = PassRecord(pass, rank);
    if (record == nullptr) return std::nullopt;
    return ActionCandidate(std::bit_cast<ActionCandidate::Words>(*record));
  }
  bool ClearExclusion(unsigned target, unsigned action) override {
    if (target >= kCandidateTargetSlots || action >= kActionDefinitionCount ||
        !ReadablePlanningBytes(
            VS::Battle::Ai::candidateExclusions,
            std::size_t{kCandidateTargetSlots} * kActionDefinitionCount,
            alignof(std::uint8_t))) {
      return false;
    }
    VS::Battle::Ai::candidateExclusions[target][action] = 0;
    return true;
  }
  bool AllowsFallback() const override {
    return state_.allow_fallback_action != 0;
  }
  int FallbackCount() const override {
    return VS::Battle::Ai::fallbackCandidateCount;
  }
  std::optional<ActionCandidate> Fallback(std::size_t rank) const override {
    if (rank >= kCandidateCapacity ||
        !ReadablePlanningBytes(
            VS::Battle::Ai::fallbackCandidates,
            sizeof(VS::Battle::Ai::CandidateEntryView) * kCandidateCapacity,
            alignof(VS::Battle::Ai::CandidateEntryView))) {
      return std::nullopt;
    }
    return ActionCandidate(std::bit_cast<ActionCandidate::Words>(
        VS::Battle::Ai::fallbackCandidates[rank]));
  }
  void Promote(int count) override {
    PromoteLiveRandomizedActionCandidate(PlanView(), count);
  }
  bool SetRankedThreshold(std::size_t rank, std::uint16_t threshold) override {
    auto* record = RankedRecord(rank);
    if (record == nullptr) return false;
    record->threshold = threshold;
    return true;
  }
  int Random() override { return VS::Core::NextBiosRand(); }
  void RefreshReadiness() override {
    Require(RefreshLiveActionCandidateReadiness(&state_).has_value(),
            "Action plan requires valid candidate readiness");
  }
  void SetPlanMode(std::uint8_t mode) override { state_.plan_mode = mode; }
  void SelectMovement() override {
    SelectLiveActionMovementMode(
        reinterpret_cast<VS::Battle::Ai::CandidateReadinessView*>(&state_));
  }
  bool BeginArenaAdjustment() override {
    if (VS::Battle::Ai::arenaOrbit == nullptr) return false;
    arena_actor_ = reinterpret_cast<const VS::Battle::Actor*>(state_.actor);
    return true;
  }
  std::uint8_t ArenaProgress() const override {
    return static_cast<std::uint8_t>(State::LiveScriptFlag(kArenaProgressFlag));
  }
  std::optional<ActionPlanCommand> ArenaCommand() const override {
    if (!ReadablePlanningRecord(arena_actor_)) return std::nullopt;
    return ActionPlanCommand{arena_actor_->command_opcode,
                             arena_actor_->command_arg0};
  }
  void SetArenaProgress(std::uint8_t progress) override {
    State::SetLiveScriptFlag(kArenaProgressFlag, progress);
  }
  bool ArenaPlanSaved() const override {
    Require(ReadablePlanningRecord(VS::Battle::Ai::arenaOrbit),
            "Action plan requires its current arena state");
    return VS::Battle::Ai::arenaOrbit->plan_saved != 0;
  }
  std::optional<std::uint16_t> ArenaActorSubtype() const override {
    if (!ReadablePlanningRecord(arena_actor_)) return std::nullopt;
    return arena_actor_->sub_type;
  }
  void SetSavedPlanCount(std::uint8_t count) override {
    Require(ReadablePlanningRecord(VS::Battle::Ai::arenaOrbit),
            "Action plan requires writable arena state");
    VS::Battle::Ai::arenaOrbit->saved_plan_count = count;
  }

 private:
  static constexpr int kArenaProgressFlag = 0xA6;
  VS::Battle::Ai::ActionPlanView* PlanView() {
    return reinterpret_cast<VS::Battle::Ai::ActionPlanView*>(&state_);
  }
  VS::Battle::Ai::ActionCandidateRecord* RankedRecord(std::size_t rank) const {
    if (rank >= std::size(state_.candidate_order)) return nullptr;
    auto* record = std::span(state_.candidate_order)[rank];
    return ReadablePlanningRecord(record) ? record : nullptr;
  }
  VS::Battle::Ai::CandidateEntryView* PassRecord(std::size_t pass,
                                                 std::size_t rank) const {
    constexpr auto kPassBytes =
        sizeof(*VS::Battle::Ai::planningPassCandidates) * kActionPlanPassCount;
    if (pass >= kActionPlanPassCount || rank >= kCandidateCapacity ||
        !ReadablePlanningBytes(VS::Battle::Ai::planningPassCandidates,
                               kPassBytes,
                               alignof(VS::Battle::Ai::CandidateEntryView)) ||
        OverlappingPlanningBytes(VS::Battle::Ai::planningPassCandidates,
                                 kPassBytes, &state_, sizeof(state_))) {
      return nullptr;
    }
    return &VS::Battle::Ai::planningPassCandidates[pass][rank];
  }

  VS::Battle::Ai::ActorAiState& state_;
  const VS::Battle::ActorStats* initial_stats_;
  LiveCandidateList list_;
  const VS::Battle::Actor* arena_actor_ = nullptr;
};

class LiveCompletedActionPlan final : public CompletedActionPlanHost {
 public:
  explicit LiveCompletedActionPlan(const ActionRequestRecord* request)
      : request_(request) {}

  std::optional<bool> SelectPerformer(unsigned slot) override {
    if (slot >= std::size(VS::Battle::Ai::actorAiStates)) return std::nullopt;
    auto* state = std::span(VS::Battle::Ai::actorAiStates)[slot];
    if (state == nullptr) return false;
    if (!ReadablePlanningRecord(state)) return std::nullopt;
    state_ = state;
    return true;
  }
  std::optional<ActionPlanCommand> PerformerCommand() const override {
    const auto* actor =
        reinterpret_cast<const VS::Battle::Actor*>(State().actor);
    if (!ReadablePlanningRecord(actor)) return std::nullopt;
    return ActionPlanCommand{actor->command_opcode, actor->command_arg0};
  }
  int CandidateCount() const override { return State().candidate_count; }
  std::optional<ActionCandidate> Ranked(std::size_t rank) const override {
    if (rank >= std::size(State().candidate_order)) return std::nullopt;
    const auto* record = std::span(State().candidate_order)[rank];
    if (!ReadablePlanningRecord(record)) return std::nullopt;
    return CandidateOf(record);
  }
  void SetDetourTarget(std::uint8_t target) override {
    State().opaque_fb = target;
  }
  int TargetCount() const override {
    return request_ == nullptr ? 0 : request_->target_count;
  }
  std::optional<CompletedActionTarget> Target(
      std::size_t index) const override {
    if (request_ == nullptr || index >= std::size(request_->targets) ||
        !ReadablePlanningBytes(
            request_,
            kTargetOffset + (index + 1) * sizeof(VS::Battle::HitResult),
            alignof(ActionRequestRecord))) {
      return std::nullopt;
    }
    const auto& target = std::span(request_->targets)[index];
    return CompletedActionTarget{
        .slot = target.target.target_actor,
        .targets_position = target.targets_position != 0};
  }
  std::optional<CompletedTargetObject> TargetObject(
      unsigned slot) const override {
    if (slot >= std::size(VS::Field::gimmickInstances)) return std::nullopt;
    const auto* object = std::span(VS::Field::gimmickInstances)[slot];
    if (object == nullptr) return CompletedTargetObject{};
    if (!Field::IsLiveGimmickPrefixReadable(object)) return std::nullopt;
    return CompletedTargetObject{.present = true,
                                 .kind = object->object_info.object_id};
  }
  void ForgetTarget() override { ForgetLiveAiActionTarget(&State()); }
  void RememberTarget() override { RememberLiveAiActionTarget(&State()); }

 private:
  VS::Battle::Ai::ActorAiState& State() const {
    Require(state_ != nullptr,
            "Completed action reconciliation requires a selected performer");
    return *state_;
  }

  const ActionRequestRecord* request_;
  VS::Battle::Ai::ActorAiState* state_ = nullptr;
};

class LiveTargeting final : public ActionTargetingHost {
 public:
  explicit LiveTargeting(const VS::Battle::Ai::ActorAiState& state)
      : state_(state) {}

  [[nodiscard]] int ActorId() const override { return state_.actor_id; }
  [[nodiscard]] unsigned Classification(int actor_id) const override {
    return state_.relations[actor_id].classification;
  }
  [[nodiscard]] std::optional<ActorResourceConditions> PeerConditions(
      int actor_id) const override {
    const VS::Battle::Ai::ActorAiState* peer =
        VS::Battle::Ai::actorAiStates[actor_id];
    if (peer == nullptr) return std::nullopt;
    return ActorResourceConditions{
        .status_restricted = (peer->resource_status_restricted & 1u) != 0,
        .low_mp = (peer->low_mp & 1u) != 0,
        .low_hp = (peer->low_hp & 1u) != 0};
  }
  [[nodiscard]] std::uint16_t ActionAiFlags(unsigned action) const override {
    return std::bit_cast<std::uint16_t>(
        VS::Battle::actionDefinitions[action].ai_flags);
  }
  [[nodiscard]] bool SlotExcludesSelf(int row, int column) const override {
    return state_.action_slots[row][column].opaque0_bit1 != 0;
  }

 private:
  const VS::Battle::Ai::ActorAiState& state_;
};

class LiveActionUsability final : public ActionUsabilityHost {
 public:
  LiveActionUsability(VS::Battle::Ai::ActorAiState& state,
                      const VS::Battle::ActorStats& stats)
      : state_(state), stats_(stats) {}

  [[nodiscard]] unsigned SlotAction(int row, int column) const override {
    return stats_.limb_actions[row][column].action_id;
  }
  [[nodiscard]] ActionCost Action(unsigned action_id) const override {
    const ActionDefinition& action = VS::Battle::actionDefinitions[action_id];
    return {.type = action.type, .cost = action.cost};
  }
  [[nodiscard]] std::int16_t LimbHp(int row) const override {
    return stats_.limbs[row].hp;
  }
  [[nodiscard]] ActorResources Resources() const override {
    return ResourcesOf(stats_);
  }
  void SetSlotUsable(int row, int column, bool usable) override {
    state_.action_slots[row][column].opaque0_bit7 = usable ? 1u : 0u;
  }

 private:
  VS::Battle::Ai::ActorAiState& state_;
  const VS::Battle::ActorStats& stats_;
};

class LiveActionMetadata final : public ActionMetadataHost {
 public:
  explicit LiveActionMetadata(VS::Battle::Ai::ActorAiState& state)
      : state_(state) {}

  [[nodiscard]] unsigned ActorId() const override { return state_.actor_id; }
  [[nodiscard]] unsigned SlotAction(int row, int column) const override {
    return Stats().limb_actions[row][column].action_id;
  }
  [[nodiscard]] MetadataAction Action(unsigned action_id) const override {
    const ActionDefinition& action = VS::Battle::actionDefinitions[action_id];
    return {.type = action.type,
            .category = action.category,
            .cost = action.cost,
            .radial_area = action.ai_flags.radial_area != 0};
  }
  [[nodiscard]] std::uint32_t RangeSquared(unsigned action_id) override {
    return static_cast<std::uint32_t>(LiveCalculateActorActionRangeSquared(
        &Stats(), static_cast<int>(action_id)));
  }
  [[nodiscard]] std::int32_t AreaReach(unsigned action_id,
                                       std::uint32_t maximum_reach) override {
    return LiveCalculateActionAreaReach(0, static_cast<int>(action_id),
                                        maximum_reach);
  }
  [[nodiscard]] bool RoomAllowsNonSpellActions() const override {
    Require(ReadablePlanningRecord(VS::Battle::Ai::roomAiState),
            "AI action metadata requires the room AI state");
    return VS::Battle::Ai::roomAiState->allows_non_spell_actions != 0;
  }
  [[nodiscard]] ActionSlotMetadata Slot(int row, int column) const override {
    const VS::Battle::Ai::ActionSlotMetadataRecord& slot =
        state_.action_slots[row][column];
    return {.excludes_self = slot.opaque0_bit1 != 0,
            .ignores_obstruction = slot.opaque0_bit2 != 0,
            .kind = static_cast<CandidateEnumerationKind>(slot.kind),
            .long_range = slot.long_range != 0,
            .fallback_eligible = slot.special != 0,
            .radius = static_cast<std::uint16_t>(slot.radius),
            .range_squared = slot.range_squared};
  }
  // The usable bit and the reserved bits keep their values.
  void SetSlot(int row, int column, const ActionSlotMetadata& slot) override {
    VS::Battle::Ai::ActionSlotMetadataRecord& entry =
        state_.action_slots[row][column];
    entry.opaque0_bit1 = slot.excludes_self ? 1u : 0u;
    entry.opaque0_bit2 = slot.ignores_obstruction ? 1u : 0u;
    entry.kind = static_cast<unsigned>(slot.kind);
    entry.long_range = slot.long_range ? 1u : 0u;
    entry.special = slot.fallback_eligible ? 1u : 0u;
    entry.radius = slot.radius;
    entry.range_squared = slot.range_squared;
  }
  [[nodiscard]] std::uint8_t ActionCostThreshold() const override {
    return state_.action_cost_enabled;
  }
  void SetActionCostThreshold(std::uint8_t threshold) override {
    state_.action_cost_enabled = threshold;
  }

 private:
  // The player's path never reads the stats record.
  [[nodiscard]] const VS::Battle::ActorStats& Stats() const {
    Require(ReadablePlanningRecord(state_.stats),
            "AI action metadata requires the actor's stats record");
    return *state_.stats;
  }

  VS::Battle::Ai::ActorAiState& state_;
};
class LiveFallbackActionSelection final : public FallbackActionHost {
 public:
  LiveFallbackActionSelection(VS::Battle::Ai::ActorAiState& state,
                              const VS::Battle::ActorStats& stats)
      : state_(state), stats_(stats) {}

  std::optional<FallbackActionSlot> RefreshSlot(int row, int column) override {
    const unsigned action = stats_.limb_actions[row][column].action_id;
    if (!ReadablePlanningRecord(state_.actor) ||
        !ReadablePlanningRecord(state_.actor->opaque_3c)) {
      return std::nullopt;
    }
    LiveActionUsability usability(state_, *state_.actor->opaque_3c);
    const auto usable = RefreshAiActionUsability(usability, row, column);
    if (!usable) return std::nullopt;
    const auto& slot = state_.action_slots[row][column];
    const bool eligible = *usable && slot.special != 0;
    return FallbackActionSlot{
        .action = action,
        .range_squared = eligible ? slot.range_squared : 0,
        .eligible = eligible};
  }
  bool IgnoresSight(unsigned action) const override {
    return VS::Battle::actionDefinitions[action].ai_flags.opaque1_bit4 != 0;
  }

 private:
  VS::Battle::Ai::ActorAiState& state_;
  const VS::Battle::ActorStats& stats_;
};

class LivePlanningSimulation final : public PlanningSimulationHost {
 public:
  explicit LivePlanningSimulation(VS::Battle::Ai::RoomAiState& room)
      : room_(room) {
    for (auto* actor = VS::Battle::actors[0]; actor != nullptr;
         actor = actor->next) {
      Require(count_ < actors_.size() && ReadablePlanningRecord(actor) &&
                  actor->id >= 0 && actor->id < kBattleActorSlotCount,
              "Planning simulation requires a bounded valid actor list");
      actors_[count_++] = actor;
    }
  }
  std::uint16_t Readiness() const override { return room_.readiness_level; }
  void SetReadiness(std::uint16_t value) override {
    room_.readiness_level = value;
  }
  std::size_t ActorCount() const override { return count_; }
  std::optional<PlanningSimulationStats> ReadActor(
      std::size_t index) const override {
    const auto* stats = actors_[index]->stats;
    if (!ReadablePlanningRecord(stats)) return std::nullopt;
    PlanningSimulationStats result{
        .hp = {stats->current_hp, stats->max_hp},
        .mp = {stats->current_mp, stats->max_mp},
        .mobility_limbs = {stats->mobility_limb0, stats->mobility_limb1},
        .statuses = static_cast<std::uint32_t>(stats->statuses)};
    for (std::size_t limb = 0; limb < result.limbs.size(); ++limb) {
      const auto& source = std::span(stats->limbs)[limb];
      result.limbs[limb] = {source.hp, source.max_hp};
    }
    std::copy_n(std::begin(stats->status_timers), result.status_timers.size(),
                result.status_timers.begin());
    return result;
  }
  void WriteActor(std::size_t index,
                  const PlanningSimulationStats& result) override {
    auto& stats = *actors_[index]->stats;
    stats.current_hp = result.hp.value;
    stats.current_mp = result.mp.value;
    stats.statuses = std::bit_cast<std::int32_t>(result.statuses);
    for (std::size_t limb = 0; limb < result.limbs.size(); ++limb) {
      std::span(stats.limbs)[limb].hp = result.limbs[limb].value;
    }
  }
  bool RefreshRestrictions(std::size_t index) override {
    state_ = VS::Battle::Ai::actorAiStates[actors_[index]->id];
    if (!ReadablePlanningRecord(state_) ||
        !ReadablePlanningRecord(state_->actor) ||
        !ReadablePlanningRecord(state_->actor->opaque_3c)) {
      return false;
    }
    RefreshLiveAiResourceRestrictions(state_);
    return true;
  }
  bool RefreshSlot(std::size_t /*index*/, int row, int column) override {
    static_cast<void>(RefreshLiveAiActionUsability(state_, row, column));
    return true;
  }

 private:
  VS::Battle::Ai::RoomAiState& room_;
  std::array<VS::Battle::Actor*, kBattleActorSlotCount> actors_{};
  std::size_t count_ = 0;
  VS::Battle::Ai::ActorAiState* state_ = nullptr;
};

class LiveCandidateEnumeration final : public CandidateEnumerationHost {
 public:
  explicit LiveCandidateEnumeration(VS::Battle::Ai::ActorAiState& state)
      : state_(state) {}

  std::optional<EnumerationSlot> ReadSlot(int row, int column) override {
    if (!ReadablePlanningRecord(state_.stats)) return std::nullopt;
    const auto& metadata = state_.action_slots[row][column];
    EnumerationSlot slot{
        .action = state_.stats->limb_actions[row][column].action_id,
        .usable = metadata.opaque0_bit7 != 0};
    if (!slot.usable || slot.action == 0 ||
        slot.action >= kActionDefinitionCount) {
      return slot;
    }
    const auto& action = VS::Battle::actionDefinitions[slot.action];
    slot.minimum_range =
        action.ai_flags.shaped_area != 0 || action.ai_flags.radial_area != 0;
    slot.ignores_sight = action.ai_flags.opaque1_bit4 != 0;
    slot.ignores_obstruction = metadata.opaque0_bit2 != 0;
    slot.radius = static_cast<std::uint16_t>(metadata.radius);
    slot.range_squared = metadata.range_squared;
    return slot;
  }
  void BeginTargets() override {
    head_ = VS::Battle::actors[0];
    current_ = nullptr;
    started_ = false;
    visited_ = 0;
  }
  std::optional<EnumerationTarget> NextTarget() override {
    if (!started_) {
      current_ = head_;
      started_ = true;
    } else if (current_ != nullptr) {
      Require(ReadablePlanningRecord(current_),
              "AI enumeration actor was invalidated during traversal");
      current_ = current_->next;
    }
    if (current_ == nullptr) return std::nullopt;
    Require(++visited_ <= kBattleActorSlotCount &&
                ReadablePlanningRecord(current_) && current_->id >= 0 &&
                current_->id < kBattleActorSlotCount,
            "AI enumeration requires a bounded valid actor list");
    const auto* target = VS::Battle::Ai::actorAiStates[current_->id];
    Require(ReadablePlanningRecord(target),
            "AI enumeration requires each actor's AI state");
    return EnumerationTarget{.id = static_cast<unsigned>(current_->id),
                             .dead = target->is_dead != 0};
  }
  bool Harmful(unsigned action, unsigned target) override {
    RequireTargetStats(target);
    return IsLiveActionHarmfulToActor(static_cast<int>(action),
                                      static_cast<int>(target)) != 0;
  }
  bool Beneficial(unsigned action, unsigned target) override {
    RequireTargetStats(target);
    return IsLiveActionBeneficialToActor(static_cast<int>(action),
                                         static_cast<int>(target)) != 0;
  }
  bool LongRange(int row, int column) const override {
    return state_.action_slots[row][column].long_range != 0;
  }
  bool CanAffect(const ActionCandidate& candidate, unsigned target) override {
    const auto record = RecordOf(candidate);
    return CanLiveAiActionAffectActor(&state_, &record,
                                      static_cast<int>(target));
  }
  bool Excluded(unsigned target, unsigned action) const override {
    Require(ReadablePlanningBytes(
                VS::Battle::Ai::candidateExclusions,
                std::size_t{kCandidateTargetSlots} * kActionDefinitionCount,
                alignof(std::uint8_t)),
            "AI enumeration requires a complete exclusion table");
    return VS::Battle::Ai::candidateExclusions[target][action] != 0;
  }
  CandidateEnumerationKind Kind(int row, int column) const override {
    return static_cast<CandidateEnumerationKind>(
        state_.action_slots[row][column].kind);
  }
  std::int16_t LimbMaximum(unsigned limb) const override {
    Require(ReadablePlanningRecord(current_) &&
                ReadablePlanningRecord(current_->stats) &&
                limb < static_cast<unsigned>(kLimbActionRows),
            "AI enumeration requires readable target limbs");
    return current_->stats->limbs[limb].max_hp;
  }
  std::int16_t TargetHeight() const override {
    Require(ReadablePlanningRecord(current_) && current_->id >= 0 &&
                current_->id < kBattleActorSlotCount,
            "AI enumeration requires a valid aimed actor");
    const auto* target = VS::Battle::Ai::actorAiStates[current_->id];
    Require(ReadablePlanningRecord(target),
            "AI enumeration requires the aimed actor's AI state");
    return target->point_height;
  }
  bool Evaluate(ActionCandidate& candidate, CandidateEnumerationKind kind,
                std::optional<unsigned> action) override {
    auto record = RecordOf(candidate);
    const auto* definition =
        action ? &VS::Battle::actionDefinitions[*action] : nullptr;
    if (!EvaluateLiveActionCandidate(&state_, &record, static_cast<int>(kind),
                                     definition)) {
      return false;
    }
    candidate = CandidateOf(&record);
    return true;
  }
  void Cache(const ActionCandidate& candidate, unsigned action) override {
    const auto record = RecordOf(candidate);
    CacheLiveFallbackActionCandidate(&record,
                                     &VS::Battle::actionDefinitions[action]);
  }
  void Append(const ActionCandidate& candidate) override {
    const auto record = RecordOf(candidate);
    AppendLiveActionCandidate(&state_, &record);
  }
  int Random() override { return VS::Core::NextBiosRand(); }

 private:
  void RequireTargetStats(unsigned id) const {
    const auto* actor = VS::Battle::actors[id];
    Require(
        ReadablePlanningRecord(actor) && ReadablePlanningRecord(actor->stats),
        "AI enumeration requires complete targeting actor stats");
  }

  VS::Battle::Ai::ActorAiState& state_;
  const VS::Battle::Actor* head_ = nullptr;
  const VS::Battle::Actor* current_ = nullptr;
  bool started_ = false;
  std::size_t visited_ = 0;
};

class LiveCandidateReadiness final : public CandidateReadinessHost {
 public:
  explicit LiveCandidateReadiness(VS::Battle::Ai::ActorAiState& state)
      : state_(state), room_(VS::Battle::Ai::roomAiState) {
    for (std::size_t index = 0;
         index < std::size(VS::Battle::Ai::actorAiStates); ++index) {
      if (VS::Battle::Ai::actorAiStates[index] == &state_) slot_ = index;
    }
  }
  int Count() const override { return Valid() ? state_.candidate_count : -1; }
  std::optional<std::size_t> RankedHandle(std::size_t rank) const override {
    if (!Valid() || rank >= std::size(state_.candidate_order)) {
      return std::nullopt;
    }
    const auto* candidate = state_.candidate_order[rank];
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
    return Valid() && ReadablePlanningRecord(room_) ? room_->readiness_level
                                                    : 0;
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
        !ReadablePlanningRecord(state_.actor->opaque_3c)) {
      return std::nullopt;
    }
    LiveActionUsability host(state_, *state_.actor->opaque_3c);
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
    state_.candidate_order[0] = record;
    return true;
  }
  bool ClearCount() override {
    if (!Valid()) return false;
    state_.candidate_count = 0;
    return true;
  }
  bool SelectMovementMode() override {
    if (!Valid() || !ReadablePlanningRecord(state_.candidate_order[0])) {
      return false;
    }
    SelectLiveActionMovementMode(
        reinterpret_cast<VS::Battle::Ai::CandidateReadinessView*>(&state_));
    return Valid();
  }
  std::optional<bool> HasHostileActor() const override {
    constexpr unsigned kHostile = 3;
    if (!Valid()) return std::nullopt;
    std::array<const VS::Battle::Actor*, kCandidateTargetSlots> visited{};
    std::size_t count = 0;
    for (const auto* actor = VS::Battle::actors[0]; actor != nullptr;
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
    state_.action_status = status;
    return true;
  }

 private:
  bool Valid() const {
    return ReadablePlanningRecord(&state_) &&
           room_ == VS::Battle::Ai::roomAiState &&
           (!slot_ || VS::Battle::Ai::actorAiStates[*slot_] == &state_);
  }
  VS::Battle::Ai::ActionCandidateRecord* Record(std::size_t handle) const {
    auto* record =
        reinterpret_cast<VS::Battle::Ai::ActionCandidateRecord*>(handle);
    return Valid() && ReadablePlanningRecord(record) ? record : nullptr;
  }
  VS::Battle::Ai::ActorAiState& state_;
  VS::Battle::Ai::RoomAiState* room_;
  std::optional<std::size_t> slot_;
};
}  // namespace

bool ApplyLiveSimulatedActionResults(
    std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows) {
  if (!ReadablePlanningBytes(rows.data(), rows.size_bytes(),
                             alignof(VS::Battle::Ai::ActionTiming))) {
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
    if (header.opaque0 >= std::size(VS::Battle::actionDefinitions)) {
      return false;
    }
    const auto& action = VS::Battle::actionDefinitions[header.opaque0];
    cost = {.type = action.type, .cost = action.cost};
  }
  std::array<ActorCondition, kBattleActorSlotCount> conditions{};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    conditions[index] = SimulationConditionOf(rows[index]);
  }
  if (!ApplySimulatedActionResults(conditions, cost,
                                   SimulationEffectOf(header.opaque4),
                                   snapshot->targets) ||
      snapshot->source != VS::Battle::Ai::planningWorkspace) {
    return false;
  }
  for (std::size_t index = 0; index < rows.size(); ++index) {
    rows[index].opaque4 = conditions[index].hp;
    rows[index].opaque8 = conditions[index].mp;
    rows[index].opaque_c = conditions[index].statuses;
    rows[index].opaque10 = conditions[index].weapon_dp;
    rows[index].opaque14 = conditions[index].shield_dp;
  }
  return true;
}

bool SimulateLiveActionCandidate(
    VS::Battle::Ai::ActorAiState* state,
    std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows,
    const VS::Battle::Ai::ActionCandidateRecord* candidate,
    CandidateSimulationMode mode) {
  if (!ReadablePlanningRecord(state) || !ReadablePlanningRecord(candidate) ||
      !ReadablePlanningBytes(rows.data(), rows.size_bytes(),
                             alignof(VS::Battle::Ai::ActionTiming))) {
    return RefusePlanning(
        "AI simulation requires readable state candidate and rows");
  }
  if (!ReadablePlanningBytes(VS::Battle::Ai::planningWorkspace,
                             sizeof(ActionRequestRecord),
                             alignof(ActionRequestRecord)) ||
      OverlappingPlanningBytes(rows.data(), rows.size_bytes(),
                               VS::Battle::Ai::planningWorkspace,
                               sizeof(ActionRequestRecord)) ||
      OverlappingPlanningBytes(state, sizeof(*state),
                               VS::Battle::Ai::planningWorkspace,
                               sizeof(ActionRequestRecord)) ||
      OverlappingPlanningBytes(candidate, sizeof(*candidate),
                               VS::Battle::Ai::planningWorkspace,
                               sizeof(ActionRequestRecord))) {
    return RefusePlanning(
        "AI simulation requires a separate complete result workspace");
  }
  LiveCandidateSimulation host(*state, rows);
  return SimulateActionCandidate(host, CandidateOf(candidate), mode);
}

bool UpdateLiveCandidateHitWeight(
    VS::Battle::Ai::ActionCandidateRecord* candidate) {
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

bool EvaluateLiveActionCandidate(
    VS::Battle::Ai::ActorAiState* state,
    VS::Battle::Ai::ActionCandidateRecord* candidate, int simulation_mode,
    const ActionDefinition* action) {
  if (!ReadablePlanningRecord(state) || !ReadablePlanningRecord(candidate) ||
      (action != nullptr && !ReadablePlanningRecord(action))) {
    return RefusePlanning(
        "AI evaluation requires readable state candidate and action");
  }
  CandidateEvaluationOptions options{
      .rarely_chosen = action != nullptr && action->ai_flags.rarely_chosen != 0,
      .special_action =
          action != nullptr && action->ai_flags.opaque0_bit4 != 0};
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

int LiveActorStatusConditionScore(const VS::Battle::Ai::ActionTiming* row) {
  Require(row != nullptr && ReadablePlanningRecord(row),
          "AI actor status score requires a condition row");
  const VS::Battle::Ai::ActorAiState* state =
      VS::Battle::Ai::actorAiStates[row->opaque0_bit0];
  Require(state != nullptr,
          "AI actor status score requires the actor's AI state");
  return ActorStatusConditionScore(SimulationConditionOf(*row),
                                   state->action_cost_enabled != 0);
}

int LiveActorHpRatio(const VS::Battle::Ai::ActionTiming* rows, int id) {
  return ActorHpRatio(SimulationConditionOf(ConditionRowOf(rows, id)));
}

int LiveActorMpRatio(const VS::Battle::Ai::ActionTiming* rows, int id) {
  return ActorMpRatio(SimulationConditionOf(ConditionRowOf(rows, id)));
}

int LiveActorEquipmentDpRatio(const VS::Battle::Ai::ActionTiming* rows,
                              int id) {
  return ActorEquipmentDpRatio(SimulationConditionOf(ConditionRowOf(rows, id)));
}

std::optional<bool> RefreshLiveActionCandidateReadiness(
    VS::Battle::Ai::ActorAiState* state) {
  if (!ReadablePlanningRecord(state) ||
      state->actor_id >= kCandidateTargetSlots || state->candidate_count < 0 ||
      state->candidate_count > static_cast<int>(kCandidateCapacity) ||
      (state->candidate_count > 0 &&
       !ReadablePlanningRecord(VS::Battle::Ai::roomAiState))) {
    VS::Core::Debugf("ai", "AI readiness state/count/room is unavailable");
    return std::nullopt;
  }
  LiveCandidateReadiness host(*state);
  return RefreshActionCandidateReadiness(host);
}

LiveCandidateRanking::LiveCandidateRanking(
    VS::Battle::Ai::ActionPlanView* state, int count) {
  Require(state != nullptr, "AI candidate ranking requires a plan view");
  Require(count >= 0 && count <= state->candidate_count &&
              state->candidate_count <=
                  static_cast<int>(std::size(state->candidate_order)),
          "AI candidate ranking count is outside the candidate list");
  entries_ =
      std::span(state->candidate_order).first(static_cast<std::size_t>(count));
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    const VS::Battle::Ai::CandidateEntryView* entry = entries_[index];
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
  std::array<VS::Battle::Ai::CandidateEntryView*, kCandidateCapacity> order{};
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    const std::size_t handle = ranking_.at(index).handle;
    Require(handle < entries_.size(),
            "AI candidate ranking refers past its entries");
    order.at(index) = entries_[handle];
  }
  std::ranges::copy(std::span(order).first(entries_.size()), entries_.begin());
}

void RememberLiveAiActionTarget(VS::Battle::Ai::ActorAiState* state) {
  Require(state != nullptr, "AI target memory requires an actor state");
  LiveTargetMemory host(*state);
  Require(RememberAiActionTarget(host).has_value(),
          "AI target memory requires a ranked, placed target");
}

void ForgetLiveAiActionTarget(VS::Battle::Ai::ActorAiState* state) {
  Require(state != nullptr, "AI target memory requires an actor state");
  LiveTargetMemory host(*state);
  ForgetAiActionTarget(host);
}

void SelectLiveFallbackAction(VS::Battle::Ai::ActorAiState* state) {
  Require(ReadablePlanningRecord(state) && ReadablePlanningRecord(state->stats),
          "AI fallback selection requires a complete state and stats record");
  LiveFallbackActionSelection host(*state, *state->stats);
  auto choice = CandidateOf(&state->current_choice);
  Require(SelectFallbackAction(choice, host),
          "AI fallback selection refused invalid input");
  state->current_choice = RecordOf(choice);
}

void CopyLiveActionCandidate(
    VS::Battle::Ai::ActionCandidateRecord* destination,
    const VS::Battle::Ai::ActionCandidateRecord* source) {
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

void AppendLiveActionCandidate(
    VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate) {
  Require(state != nullptr, "AI candidate append requires an actor state");
  LiveCandidateList list(*state);
  Require(AppendActionCandidate(list, CandidateOf(candidate)).has_value(),
          "AI candidate append requires a valid count and exclusion table");
}

void SortLiveActionCandidates(VS::Battle::Ai::ActionPlanView* state) {
  Require(state != nullptr, "AI candidate sort requires a plan view");
  LiveCandidateRanking ranking(state, state->candidate_count);
  Require(SortActionCandidates(ranking.Ranking()),
          "AI candidate sort exceeds the candidate list");
  ranking.Apply();
}

void CopyLivePlanningActorStats(const VS::Battle::ActorStats* source,
                                VS::Battle::ActorStats* destination) {
  Require(source != nullptr && destination != nullptr,
          "AI stat snapshot requires a source and destination");
  CopyPlanningActorStats(
      std::as_bytes(std::span<const VS::Battle::ActorStats, 1>(source, 1)),
      std::as_writable_bytes(
          std::span<VS::Battle::ActorStats, 1>(destination, 1)));
}

bool SnapshotLivePlanningActorStats() {
  if (!liveStatsSnapshot.bytes.empty() ||
      VS::Battle::Ai::planningStatsSnapshot != nullptr) {
    return RefusePlanning("AI stat capture is already active");
  }
  auto* storage = static_cast<std::byte*>(
      Core::Memory::AllocateLiveFromHighEnd(kPlanningStatsTableBytes));
  if (storage == nullptr) {
    return RefusePlanning("AI stat capture has no snapshot storage");
  }
  liveStatsSnapshot = {
      .bytes = std::span<std::byte>(storage, kPlanningStatsTableBytes)};
  VS::Battle::Ai::planningStatsSnapshot =
      reinterpret_cast<VS::Battle::ActorStats*>(storage);
  std::array<PlanningActorStats, kBattleActorSlotCount> rows{};
  const auto actors = ReadPlanningStatsRows(rows);
  if (actors && SnapshotPlanningActorStats(liveStatsSnapshot, *actors)) {
    return true;
  }
  Core::Memory::FreeLiveFromHighEnd(storage);
  InvalidateLivePlanningStatsSnapshot();
  return RefusePlanning("AI stat capture requires a valid disjoint actor list");
}

void RestoreLivePlanningActorStats() {
  const auto storage = liveStatsSnapshot.bytes;
  const auto allocation = Core::Memory::LivePayloadContaining(storage.data());
  Require(allocation.has_value() && allocation->data() == storage.data() &&
              allocation->size() >= kPlanningStatsTableBytes &&
              reinterpret_cast<std::byte*>(
                  VS::Battle::Ai::planningStatsSnapshot) == storage.data(),
          "AI stat restore requires its live snapshot allocation");
  std::array<PlanningActorStats, kBattleActorSlotCount> rows{};
  const auto actors = ReadPlanningStatsRows(rows);
  Require(actors.has_value() &&
              RestorePlanningActorStats(liveStatsSnapshot, *actors),
          "AI stat restore requires a valid captured actor list");
  Core::Memory::FreeLiveFromHighEnd(storage.data());
  InvalidateLivePlanningStatsSnapshot();
}

void BuildLiveActorActionPlan(VS::Battle::Ai::ActorAiState* state) {
  Require(ReadablePlanningRecord(state),
          "Action planning requires a complete deciding actor state");
  LiveActorActionPlan host(*state);
  Require(BuildActorActionPlan(host),
          "Actor action planning refused invalid inputs");
}

void ReconcileLiveCompletedActionPlan(const ActionRequestRecord* request) {
  std::optional<CompletedAction> action;
  if (request != nullptr) {
    Require(ReadablePlanningBytes(request, kTargetOffset,
                                  alignof(ActionRequestRecord)),
            "Completed action reconciliation requires a complete request");
    action = CompletedAction{
        .action_id = request->action_id,
        .performer = request->primary_target.target.target_actor,
        .performer_targets_position =
            request->primary_target.targets_position != 0};
  }
  LiveCompletedActionPlan host(request);
  Require(ReconcileCompletedActionPlan(action, host),
          "Completed action reconciliation refused invalid input");
}

void EnumerateLiveActionCandidates(VS::Battle::Ai::ActorAiState* state,
                                   std::int32_t readiness_threshold) {
  Require(ReadablePlanningRecord(state),
          "AI enumeration requires a complete deciding actor state");
  LiveCandidateEnumeration host(*state);
  Require(EnumerateActionCandidates(
              static_cast<std::uint16_t>(readiness_threshold), host),
          "AI candidate enumeration refused invalid planning inputs");
}

void AdvanceLivePlanningSimulation(std::int32_t time_step) {
  Require(ReadablePlanningRecord(VS::Battle::Ai::roomAiState),
          "Planning simulation requires a valid room AI state");
  LivePlanningSimulation host(*VS::Battle::Ai::roomAiState);
  PlanningSimulationRules rules{
      .poison_interval =
          VS::Battle::statusEffectParams[kPoisonStatusIndex].tick_interval,
      .regeneration_interval =
          VS::Battle::statusEffectParams[kRegenerationStatusIndex]
              .tick_interval};
  std::copy_n(std::begin(VS::Battle::Ai::kTimedStatusFlags),
              rules.timed_status_masks.size(),
              rules.timed_status_masks.begin());
  Require(AdvancePlanningSimulation(time_step, rules, host),
          "Planning simulation refused invalid actor inputs");
}

void InvalidateLivePlanningStatsSnapshot() {
  liveStatsSnapshot = {};
  VS::Battle::Ai::planningStatsSnapshot = nullptr;
}

bool CanLiveAiActionAffectActor(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate, int actor_id) {
  Require(state != nullptr, "AI target eligibility requires an actor state");
  const LiveTargeting host(*state);
  const std::optional<bool> affects =
      CanAiActionAffectActor(host, CandidateOf(candidate), actor_id);
  Require(affects.has_value(),
          "AI target eligibility requires an actor and slot in their tables");
  return *affects;
}

bool RefreshLiveAiActionUsability(VS::Battle::Ai::ActorAiState* state, int row,
                                  int column) {
  Require(state != nullptr, "AI action usability requires an actor state");
  LiveActionUsability host(*state, StatsOf(*state));
  const std::optional<bool> usable =
      RefreshAiActionUsability(host, row, column);
  Require(usable.has_value(),
          "AI action usability requires a slot and action in their tables");
  return *usable;
}

void RefreshLiveAiResourceRestrictions(VS::Battle::Ai::ActorAiState* state) {
  Require(state != nullptr, "AI resource restrictions require a state");
  const ActorResourceConditions conditions = RefreshAiResourceRestrictions(
      ResourcesOf(StatsOf(*state)), state->action_cost_enabled);
  state->resource_status_restricted = conditions.status_restricted ? 1 : 0;
  state->low_mp = conditions.low_mp ? 1 : 0;
  state->low_hp = conditions.low_hp ? 1 : 0;
}

void BuildLiveAiActionMetadata(VS::Battle::Ai::ActorAiState* state) {
  Require(ReadablePlanningRecord(state),
          "AI action metadata requires an actor state");
  LiveActionMetadata host(*state);
  Require(BuildAiActionMetadata(host),
          "AI action metadata requires slot actions in the action table");
}
}  // namespace VS::Battle::Ai

int VS::Battle::Ai::RefreshActionCandidateReadinessEntry(
    VS::Battle::Ai::CandidateReadinessView* state) {
  return VS::Battle::Ai::RefreshLiveActionCandidateReadiness(
             reinterpret_cast<VS::Battle::Ai::ActorAiState*>(state))
      .value_or(false);
}

void VS::Battle::Ai::ApplySimulatedActionResultsEntry(
    VS::Battle::Ai::ActionTiming* rows) {
  if (rows == nullptr) return;
  static_cast<void>(VS::Battle::Ai::ApplyLiveSimulatedActionResults(
      std::span<VS::Battle::Ai::ActionTiming,
                VS::Battle::Ai::kBattleActorSlotCount>(
          rows, VS::Battle::Ai::kBattleActorSlotCount)));
}

void VS::Battle::Ai::SimulateActionCandidateEntry(
    VS::Battle::Ai::ActorAiState* state, VS::Battle::Ai::ActionTiming* rows,
    VS::Battle::Ai::ActionCandidateRecord* candidate, int mode) {
  constexpr int kAimedMode = 3;
  if (rows != nullptr &&
      VS::Battle::Ai::SimulateLiveActionCandidate(
          state,
          std::span<VS::Battle::Ai::ActionTiming,
                    VS::Battle::Ai::kBattleActorSlotCount>(
              rows, VS::Battle::Ai::kBattleActorSlotCount),
          candidate,
          mode == kAimedMode
              ? VS::Battle::Ai::CandidateSimulationMode::kAimed
              : VS::Battle::Ai::CandidateSimulationMode::kLimb)) {
    return;
  }
  VS::Core::Errorf("ai", "AI candidate simulation entry refused invalid input");
  std::abort();
}

void VS::Battle::Ai::UpdateCandidateHitWeightEntry(
    VS::Battle::Ai::ActionCandidateRecord* candidate) {
  if (VS::Battle::Ai::UpdateLiveCandidateHitWeight(candidate)) return;
  VS::Core::Errorf("ai", "AI candidate hit-weight entry refused invalid input");
  std::abort();
}

void VS::Battle::Ai::EvaluateActionCandidateEntry(
    VS::Battle::Ai::ActorAiState* state,
    VS::Battle::Ai::ActionCandidateRecord* candidate, int mode,
    ActionDefinition* action) {
  if (VS::Battle::Ai::EvaluateLiveActionCandidate(state, candidate, mode,
                                                  action)) {
    return;
  }
  VS::Core::Errorf("ai", "AI candidate evaluation entry refused invalid input");
  std::abort();
}

void VS::Battle::Ai::SelectFallbackActionEntry(
    VS::Battle::Ai::ActorAiState* state) {
  VS::Battle::Ai::SelectLiveFallbackAction(state);
}

void VS::Battle::Ai::AdvancePlanningSimulationEntry(int time_step) {
  VS::Battle::Ai::AdvanceLivePlanningSimulation(time_step);
}

void VS::Battle::Ai::EnumerateActionCandidatesEntry(
    VS::Battle::Ai::ActorAiState* state, int readiness_threshold) {
  VS::Battle::Ai::EnumerateLiveActionCandidates(state, readiness_threshold);
}

void VS::Battle::Ai::BuildActorActionPlanEntry(
    VS::Battle::Ai::ActorAiState* state) {
  VS::Battle::Ai::BuildLiveActorActionPlan(state);
}

void VS::Battle::Ai::ReconcileCompletedActionPlanEntry(
    VS::Battle::Ai::ReactionRecord* completed_action) {
  VS::Battle::Ai::ReconcileLiveCompletedActionPlan(
      reinterpret_cast<const ActionRequestRecord*>(completed_action));
}
