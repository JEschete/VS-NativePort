// Hand-maintained (tools/port/owned.csv).
// Live binding for AI perception (ActorPerception.cpp).
#include "Battle/Ai/ActorPerceptionLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <vector>

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorPerception.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Core/FrameTiming.hpp"
#include "Core/Math/Distance.hpp"
#include "Field/TerrainHeight.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/PlanningPrimitivesLive.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Field/Gimmicks.h"
}

namespace VS::Battle::Ai {
namespace {
using AiState = Battle_Ai_ActorAiState;
using ObserverView = Battle_Ai_PerceptionView;
using UpdateView = Battle_Ai_PerceptionUpdateView;

constexpr int kBattleActorSlots = static_cast<int>(std::size(Battle_actors));
constexpr std::uint16_t kMarkedTileFlag = 0x2000;
constexpr std::uint8_t kCommandFlagBits = 3;
constexpr int kByteShift = 8;
constexpr int kHalfShift = 16;
constexpr int kTopByteShift = 24;
constexpr std::uint32_t kHalfMask = 0xFFFF;

static_assert(std::size(Battle_Ai_actorAiStates) == kPerceptionSlots);
static_assert(std::size(Field_gimmickInstances) == kPerceptionSlots);
static_assert(kAiFrameActorSlots == kPerceptionSlots);
static_assert(std::size(Actors_actorRecords) == kPerceptionRecordSlots);

// Both views are the actor's AI state.
static_assert(offsetof(ObserverView, tile) == offsetof(AiState, tile));
static_assert(offsetof(ObserverView, position) == offsetof(AiState, position));
static_assert(offsetof(ObserverView, model) == offsetof(AiState, model));
static_assert(offsetof(ObserverView, pointHeight) ==
              offsetof(AiState, pointHeight));
static_assert(offsetof(ObserverView, actorId) == offsetof(AiState, actorId));
static_assert(offsetof(ObserverView, radius) == offsetof(AiState, radius));
static_assert(offsetof(ObserverView, bodyHeight) ==
              offsetof(AiState, bodyHeight));
static_assert(offsetof(ObserverView, relations) ==
              offsetof(AiState, relations));
static_assert(offsetof(ObserverView, distancesSquared) ==
              offsetof(AiState, distancesSquared));
static_assert(offsetof(UpdateView, opaque8) == offsetof(AiState, opaque8));
static_assert(offsetof(UpdateView, opaqueE) == offsetof(AiState, opaqueE));
static_assert(offsetof(UpdateView, opaque18) == offsetof(AiState, opaque18));
static_assert(offsetof(UpdateView, opaque1C) == offsetof(AiState, opaque1C));
static_assert(offsetof(UpdateView, tile) == offsetof(AiState, tile));
static_assert(offsetof(UpdateView, previousTile) ==
              offsetof(AiState, previousTile));
static_assert(offsetof(UpdateView, opaque3C) ==
              offsetof(AiState, rememberedTargetTile));
static_assert(offsetof(UpdateView, opaque40) ==
              offsetof(AiState, targetMemoryFrames));
static_assert(offsetof(UpdateView, opaque4C) == offsetof(AiState, position));
static_assert(offsetof(UpdateView, actor) == offsetof(AiState, actor));
static_assert(offsetof(UpdateView, model) == offsetof(AiState, model));
static_assert(offsetof(UpdateView, statusFlags) ==
              offsetof(AiState, actionDelay));
static_assert(offsetof(UpdateView, detourStep) ==
              offsetof(AiState, detourStep));
static_assert(offsetof(UpdateView, opaqueFB) == offsetof(AiState, opaqueFB));
static_assert(offsetof(UpdateView, awarenessFramesRemaining) ==
              offsetof(AiState, awarenessFramesRemaining));
static_assert(offsetof(UpdateView, actionStatus) ==
              offsetof(AiState, actionStatus));
static_assert(offsetof(UpdateView, awarenessMarker) ==
              offsetof(AiState, awarenessMarker));
static_assert(offsetof(UpdateView, opaque19A) == offsetof(AiState, opaque19A));
static_assert(offsetof(UpdateView, relations) == offsetof(AiState, relations));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

std::uint32_t CurrentTileWord(const Actors_ModelInstance& instance) {
  return static_cast<std::uint32_t>(instance.currentTileX) |
         (static_cast<std::uint32_t>(instance.currentTileLayer) << kByteShift) |
         (static_cast<std::uint32_t>(instance.currentTileZ) << kHalfShift) |
         (static_cast<std::uint32_t>(instance.reserved5F) << kTopByteShift);
}

std::uint32_t PreviousTileWord(const Actors_ModelInstance& instance) {
  return static_cast<std::uint32_t>(instance.previousTileX) |
         (static_cast<std::uint32_t>(instance.previousTileLayer)
          << kByteShift) |
         (static_cast<std::uint32_t>(instance.previousTileZ) << kHalfShift) |
         (static_cast<std::uint32_t>(instance.reserved63) << kTopByteShift);
}

// The battle actor list in order; aborts on a list longer than the table.
template <typename Visit>
void WalkBattleActors(Visit visit) {
  int walked = 0;
  for (Battle_Actor* actor = Battle_actors[0]; actor != nullptr;
       actor = actor->next) {
    Require(++walked <= kBattleActorSlots,
            "AI perception found a battle actor list longer than its table");
    visit(*actor);
  }
}

PerceptionShape ShapeOf(const AiState& state) {
  return PerceptionShape{.body_height = state.bodyHeight,
                         .point_height = state.pointHeight,
                         .radius = state.radius};
}

PerceivingObserver LoadObserver(const ObserverView& view) {
  PerceivingObserver observer;
  observer.decays_hostility = view.settingsBit23 != 0;
  observer.ignores_body_line = view.settingsBit15 != 0;
  observer.tile = view.tile.word;
  observer.position = Core::Math::PositionOf(view.position);
  observer.point_height = view.pointHeight;
  observer.actor_id = view.actorId;
  observer.near_distance_squared = view.nearDistanceSquared;
  observer.view_cosine = view.viewCosine;
  observer.far_distance_squared = view.farDistanceSquared;
  observer.body_height = view.bodyHeight;
  observer.phase = view.phase;
  for (int slot = 0; slot < kPerceptionSlots; ++slot) {
    observer.relations.at(slot) =
        DecodePerceivedRelation(view.relations[slot].word);
    observer.distances_squared.at(slot) = view.distancesSquared[slot];
  }
  return observer;
}

// The settings, tile, position and sizes are only read.
void StoreObserver(const PerceivingObserver& observer, ObserverView& view) {
  view.phase = observer.phase;
  for (int slot = 0; slot < kPerceptionSlots; ++slot) {
    view.relations[slot].word =
        EncodePerceivedRelation(observer.relations.at(slot));
    view.distancesSquared[slot] = observer.distances_squared.at(slot);
  }
}

// The battle actors' models, the AI state table, the frame context's actors
// and tile flags, the gimmicks and the room terrain.
class LiveTargetPerceptionWorld final : public TargetPerceptionWorld {
 public:
  explicit LiveTargetPerceptionWorld(const ObserverView& view) : view_(view) {}

  [[nodiscard]] std::optional<PerceivedModel> Model(int target) const override {
    const Battle_Actor* actor = Battle_actors[target];
    if (actor == nullptr || actor->model == nullptr) return std::nullopt;
    const Actors_ModelInstance& model = *actor->model;
    return PerceivedModel{.position = Core::Math::PositionOf(model.position),
                          .tile = CurrentTileWord(model),
                          .last_tile = PreviousTileWord(model),
                          .locomoting = model.locomotionState != 0};
  }
  [[nodiscard]] std::optional<ObserverPose> ObserverModel() const override {
    const auto* model =
        reinterpret_cast<const Actors_ModelInstance*>(view_.model);
    if (model == nullptr) return std::nullopt;
    return ObserverPose{.position = Core::Math::PositionOf(model->position),
                        .facing = model->facing,
                        .look_yaw = static_cast<std::int16_t>(model->lookYaw)};
  }
  [[nodiscard]] std::optional<PerceptionShape> AiStateShape(
      int target) const override {
    const AiState* state = Battle_Ai_actorAiStates[target];
    if (state == nullptr) return std::nullopt;
    return ShapeOf(*state);
  }
  [[nodiscard]] std::optional<PerceptionShape> FrameShape(
      int target) const override {
    const AiState* state = Battle_Ai_FrameState()->actors[target];
    if (state == nullptr) return std::nullopt;
    return ShapeOf(*state);
  }
  [[nodiscard]] bool HasGimmick(int target) const override {
    return Field_gimmickInstances[target] != nullptr;
  }
  [[nodiscard]] std::optional<bool> IsMarkedTile(
      Core::Math::TileCoordinate tile) const override {
    const Battle_Ai_FrameContext& frame = *Battle_Ai_FrameState();
    if (frame.tileFlags == nullptr || tile.x >= kAiFrameGridSide ||
        tile.z >= kAiFrameGridSide) {
      return std::nullopt;
    }
    return (frame.tileFlags[tile.z][tile.x] & kMarkedTileFlag) != 0;
  }
  [[nodiscard]] bool IsTerrainSegmentBlocked(Core::Math::Position3 from,
                                             Core::Math::Position3 to,
                                             int from_height,
                                             int to_height) const override {
    return Field::IsTerrainSegmentBlocked(CurrentTerrain(), from, to,
                                          from_height, to_height);
  }
  [[nodiscard]] std::vector<ListedPerceptionActor> ListedActors()
      const override {
    std::vector<ListedPerceptionActor> listed;
    WalkBattleActors([&](const Battle_Actor& actor) {
      ListedPerceptionActor entry{.id = actor.id, .position = std::nullopt};
      if (actor.model != nullptr) {
        entry.position = Core::Math::PositionOf(actor.model->position);
      }
      listed.push_back(entry);
    });
    return listed;
  }

 private:
  const ObserverView& view_;
};

bool TryUpdateLiveTargetPerception(ObserverView& view, int target) {
  LiveTargetPerceptionWorld world(view);
  PerceivingObserver observer = LoadObserver(view);
  const bool updated = UpdateTargetPerception(world, observer, target);
  StoreObserver(observer, view);
  return updated;
}

Core::Math::Position3 UnpackPosition(std::uint32_t xy, std::uint32_t z) {
  return Core::Math::Position3{.x = static_cast<std::int16_t>(xy & kHalfMask),
                               .y = static_cast<std::int16_t>(xy >> kHalfShift),
                               .z = static_cast<std::int16_t>(z & kHalfMask)};
}

PerceivingActor Load(const Battle_Actor& battle, const UpdateView& view) {
  PerceivingActor actor;
  actor.battle = PerceivingBattleActor{
      .id = static_cast<std::uint8_t>(battle.id),
      .command_state = battle.commandState,
      .command_opcode = battle.commandOpcode,
      .command_flags =
          static_cast<std::uint8_t>(battle.commandFlags & kCommandFlagBits),
      .action_recovery_timer = battle.stats->actionRecoveryTimer};
  actor.stayed_on_tile = view.opaque8;
  actor.alert = view.opaqueE;
  actor.perception_skipped = view.opaque14;
  actor.alerted = view.opaque18;
  actor.holds_stall_tile = view.opaque1C;
  actor.partial_perception = view.opaque1D;
  actor.alert_rule = view.settingsBits28;
  actor.tile = view.tile;
  actor.previous_tile = view.previousTile.word;
  actor.remembered_target_tile = view.opaque3C;
  actor.target_memory_frames = view.opaque40;
  actor.detour_step_frames = view.opaque45;
  actor.stall_frames = view.opaque46;
  const auto xy = static_cast<std::uint32_t>(view.opaque4C);
  const auto z = static_cast<std::uint32_t>(view.opaque50);
  actor.position = UnpackPosition(xy, z);
  actor.position_pad = static_cast<std::int16_t>(z >> kHalfShift);
  for (std::size_t word = 0; word < actor.history.x_words.size(); ++word) {
    actor.history.x_words.at(word) =
        static_cast<std::uint32_t>(view.historyX.words[word]);
    actor.history.z_words.at(word) =
        static_cast<std::uint32_t>(view.historyZ.words[word]);
  }
  actor.history.mask = view.historyMask;
  actor.history_index = view.historyIndex;
  actor.action_delay = view.statusFlags;
  actor.detour_step = view.detourStep;
  actor.detour_target = view.opaqueFB;
  actor.flee_timer = view.opaque114;
  actor.fine_tile = view.opaque120;
  actor.observed_command = view.opaque124;
  actor.awareness_frames_remaining = view.awarenessFramesRemaining;
  actor.action_status = view.actionStatus;
  actor.awareness_marker = view.awarenessMarker;
  actor.movement_wait = view.opaque156;
  actor.has_navigation_override = view.opaque18C != nullptr;
  actor.navigation_counter = static_cast<std::uint8_t>(view.opaque190);
  actor.recovery_frames = view.opaque19A;
  for (int slot = 0; slot < kPerceptionSlots; ++slot) {
    actor.relations.at(slot) = DecodePerceivedRelation(view.relations[slot]);
  }
  return actor;
}

std::uint32_t PackHalves(std::int16_t low, std::int16_t high) {
  return static_cast<std::uint32_t>(static_cast<std::uint16_t>(low)) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high))
          << kHalfShift);
}

// The battle actor's command state, opcode and recovery timer, the alert
// rule, remembered tile, flee timer, awareness, status, navigation override
// and relations are only read.
void Store(const PerceivingActor& actor, Battle_Actor& battle,
           UpdateView& view) {
  battle.commandFlags =
      static_cast<u_char>((battle.commandFlags & ~kCommandFlagBits) |
                          (actor.battle.command_flags & kCommandFlagBits));
  view.opaque8 = actor.stayed_on_tile;
  view.opaqueE = actor.alert;
  view.opaque14 = actor.perception_skipped;
  view.opaque18 = actor.alerted;
  view.opaque1C = actor.holds_stall_tile;
  view.opaque1D = actor.partial_perception;
  view.tile = actor.tile;
  view.previousTile.word = actor.previous_tile;
  view.opaque40 = actor.target_memory_frames;
  view.opaque45 = actor.detour_step_frames;
  view.opaque46 = actor.stall_frames;
  view.opaque4C =
      static_cast<int>(PackHalves(actor.position.x, actor.position.y));
  view.opaque50 =
      static_cast<int>(PackHalves(actor.position.z, actor.position_pad));
  for (std::size_t word = 0; word < actor.history.x_words.size(); ++word) {
    view.historyX.words[word] =
        static_cast<int>(actor.history.x_words.at(word));
    view.historyZ.words[word] =
        static_cast<int>(actor.history.z_words.at(word));
  }
  view.historyMask = actor.history.mask;
  view.historyIndex = actor.history_index;
  view.statusFlags = actor.action_delay;
  view.detourStep = actor.detour_step;
  view.opaqueFB = actor.detour_target;
  view.opaque120 = actor.fine_tile;
  view.opaque124 = actor.observed_command;
  view.opaque156 = actor.movement_wait;
  view.opaque190 = actor.navigation_counter;
  view.opaque19A = actor.recovery_frames;
}

// One listed actor's model, links and HP, the frame budget, the battle
// actor list, the actor records and the AI routines. Each call taking the
// actor stores it into the AI state and the battle actor first and reloads
// it after.
class LiveActorPerceptionWorld final : public ActorPerceptionWorld {
 public:
  LiveActorPerceptionWorld(Battle_Actor& battle, UpdateView& view)
      : battle_(battle), view_(view) {}

  [[nodiscard]] std::optional<PerceiverModel> Model() const override {
    // The AI state's model is the actor's record.
    const auto* record =
        reinterpret_cast<const Actors_ActorRecord*>(view_.model);
    if (record == nullptr) return std::nullopt;
    const Actors_ModelInstance& instance = record->instance;
    return PerceiverModel{
        .moving = instance.movementMode != 0 || instance.locomotionState != 0,
        .running = record->isRunning != 0,
        .forces_perception = instance.wasDrawn != 0,
        .tile = CurrentTileWord(instance)};
  }
  [[nodiscard]] std::optional<std::uint32_t> LinkedTile() const override {
    if (view_.tileSource == nullptr) return std::nullopt;
    return view_.tileSource->position;
  }
  [[nodiscard]] std::optional<LinkedPoint> LinkedPosition() const override {
    const Battle_Ai_PositionLinkView* link = view_.positionSource;
    if (link == nullptr) return std::nullopt;
    return LinkedPoint{
        .position = UnpackPosition(link->position, link->opaque4),
        .pad = static_cast<std::int16_t>(link->opaque4 >> kHalfShift)};
  }
  [[nodiscard]] std::optional<PerceivedHp> Hp() const override {
    const Battle_Actor* actor = view_.actor;
    if (actor == nullptr || actor->stats == nullptr) return std::nullopt;
    return PerceivedHp{
        .current = static_cast<std::uint16_t>(actor->stats->currentHp),
        .max = static_cast<std::uint16_t>(actor->stats->maxHp)};
  }
  [[nodiscard]] std::vector<int> ListedActorIds() const override {
    std::vector<int> ids;
    WalkBattleActors(
        [&](const Battle_Actor& actor) { ids.push_back(actor.id); });
    return ids;
  }
  [[nodiscard]] std::optional<std::uint32_t> RecordTile(int id) const override {
    const Actors_ActorRecord* record = Actors_actorRecords[id];
    if (record == nullptr) return std::nullopt;
    return CurrentTileWord(record->instance);
  }
  [[nodiscard]] std::int32_t FrameTickBudget() const override {
    return Battle_Ai_frameTickBudget;
  }
  [[nodiscard]] std::int32_t SecondPerceptionTarget() const override {
    return Battle_Ai_secondPerceptionTarget;
  }
  [[nodiscard]] std::int32_t ElapsedFrameTicks() override {
    return Core::ElapsedFrameCounterTicks();
  }
  void SetLastBudgetTick(std::int32_t tick) override {
    Battle_Ai_FrameState()->lastBudgetTick = tick;
  }

  void Disengage(PerceivingActor& actor) override {
    Run(actor, [&] { DisengageLiveActor(State()); });
  }
  void ResetAwarenessIndicator(PerceivingActor& actor) override {
    Run(actor, [&] {
      ResetLiveActorAwarenessIndicator(
          reinterpret_cast<Battle_Ai_PlanEvaluationView*>(&view_));
    });
  }
  void UnlinkEngagedActor(PerceivingActor& actor) override {
    Run(actor, [&] { UnlinkLiveEngagedActor(State()); });
  }
  void UpdateTemporaryAwareness(PerceivingActor& actor) override {
    Run(actor, [&] { UpdateLiveTemporaryAwarenessIndicator(&view_); });
  }
  void TickDecisionTimers(PerceivingActor& actor) override {
    Run(actor, [&] { TickLiveActorDecisionTimers(&view_); });
  }
  void UpdateEngagementBuildup(PerceivingActor& actor) override {
    Run(actor, [&] { UpdateLiveEngagementBuildup(State()); });
  }
  [[nodiscard]] bool UpdateTargetPerception(PerceivingActor& actor,
                                            int target) override {
    bool updated = false;
    Run(actor, [&] {
      updated = TryUpdateLiveTargetPerception(
          *reinterpret_cast<ObserverView*>(&view_), target);
    });
    return updated;
  }
  void RefreshTargetTileWhenTracking(PerceivingActor& actor) override {
    Run(actor, [&] { RefreshLiveAiTargetTileWhenTracking(&view_); });
  }
  void RecoverMovement(PerceivingActor& actor) override {
    Run(actor, [&] { RecoverLiveMovement(State()); });
  }
  void ForgetActionTarget(PerceivingActor& actor) override {
    Run(actor, [&] { ForgetLiveAiActionTarget(State()); });
  }
  void SetReactionWithDelay(PerceivingActor& actor, int reaction,
                            int delay) override {
    Run(actor,
        [&] { SetLiveActorAiReactionWithDelay(&view_, reaction, delay); });
  }
  void RefreshResourceRestrictions(PerceivingActor& actor) override {
    Run(actor, [&] { RefreshLiveAiResourceRestrictions(State()); });
  }
  [[nodiscard]] bool RefreshCandidateReadiness(
      PerceivingActor& actor) override {
    int ready = 0;
    Run(actor, [&] {
      ready = Battle_Ai_RefreshActionCandidateReadinessEntry(
          reinterpret_cast<Battle_Ai_CandidateReadinessView*>(&view_));
    });
    return ready != 0;
  }

 private:
  [[nodiscard]] AiState* State() const {
    return reinterpret_cast<AiState*>(&view_);
  }

  template <typename Call>
  void Run(PerceivingActor& actor, Call&& call) {
    Store(actor, battle_, view_);
    call();
    actor = Load(battle_, view_);
  }

  Battle_Actor& battle_;
  UpdateView& view_;
};

void UpdateListedActor(Battle_Actor& battle) {
  Require(battle.id >= 0 && battle.id < kAiFrameActorSlots,
          "AI perception requires a listed actor inside the AI state table");
  auto* view =
      reinterpret_cast<UpdateView*>(Battle_Ai_FrameState()->actors[battle.id]);
  Require(view != nullptr, "AI perception requires a listed actor's AI state");
  Require(battle.stats != nullptr,
          "AI perception requires a listed actor's stats");
  LiveActorPerceptionWorld world(battle, *view);
  PerceivingActor actor = Load(battle, *view);
  const bool updated = UpdatePerceivingActor(world, actor);
  Store(actor, battle, *view);
  Require(updated,
          "AI perception requires a model, linked tile and position, HP, "
          "listed and remembered actors inside their tables and a history "
          "index inside its samples");
}
}  // namespace

// decomp: func_800D87E8 (src/BATTLE/BATTLE.PRG/6FC00.c:679).
void UpdateLiveTargetPerception(Battle_Ai_PerceptionView* state, int target) {
  Require(state != nullptr, "AI target perception requires an AI state");
  Require(TryUpdateLiveTargetPerception(*state, target),
          "AI target perception requires a target inside the AI state table, "
          "both models, the frame's tile flags and listed actors inside the "
          "table with models");
}

// decomp: func_800D8EF4 (src/BATTLE/BATTLE.PRG/6FC00.c:983).
void UpdateLiveActorPerception() {
  Battle_Ai_RoomAiState* room = Battle_Ai_roomAiState;
  Require(room != nullptr, "AI perception requires the room AI state");
  PerceptionRoom typed{
      .readiness_level = room->readinessLevel,
      .approach_moves = room->approachMoves,
      .candidate_engaged = Battle_Ai_candidateEngagedThisUpdate};
  BeginActorPerception(typed);
  Battle_Ai_candidateEngagedThisUpdate = typed.candidate_engaged;
  room->approachMoves = typed.approach_moves;
  room->readinessLevel = typed.readiness_level;
  WalkBattleActors(UpdateListedActor);
}
}  // namespace VS::Battle::Ai
