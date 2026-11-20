// Hand-maintained (tools/port/owned.csv).
// Live binding for AI perception (ActorPerception.cpp).
#include "Battle/Ai/ActorPerceptionLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "Actors/ActorRecord.hpp"
#include "Actors/ModelInstance.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorPerception.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/FrameTiming.hpp"
#include "Core/Math/Distance.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/TerrainHeight.hpp"

namespace VS::Battle::Ai {
namespace {
using AiState = VS::Battle::Ai::ActorAiState;
using ObserverView = VS::Battle::Ai::PerceptionView;
using UpdateView = VS::Battle::Ai::PerceptionUpdateView;

constexpr int kBattleActorSlots = static_cast<int>(std::size(Battle::actors));
constexpr std::uint16_t kMarkedTileFlag = 0x2000;
constexpr std::uint8_t kCommandFlagBits = 3;
constexpr int kByteShift = 8;
constexpr int kHalfShift = 16;
constexpr int kTopByteShift = 24;
constexpr std::uint32_t kHalfMask = 0xFFFF;

static_assert(std::size(VS::Battle::Ai::actorAiStates) == kPerceptionSlots);
static_assert(std::size(VS::Field::gimmickInstances) == kPerceptionSlots);
static_assert(kFrameActorSlots == kPerceptionSlots);
static_assert(std::size(VS::Actors::actorRecords) == kPerceptionRecordSlots);

// Both views are the actor's AI state.
static_assert(offsetof(ObserverView, tile) == offsetof(AiState, tile));
static_assert(offsetof(ObserverView, position) == offsetof(AiState, position));
static_assert(offsetof(ObserverView, model) == offsetof(AiState, model));
static_assert(offsetof(ObserverView, point_height) ==
              offsetof(AiState, point_height));
static_assert(offsetof(ObserverView, actor_id) == offsetof(AiState, actor_id));
static_assert(offsetof(ObserverView, radius) == offsetof(AiState, radius));
static_assert(offsetof(ObserverView, body_height) ==
              offsetof(AiState, body_height));
static_assert(offsetof(ObserverView, relations) ==
              offsetof(AiState, relations));
static_assert(offsetof(ObserverView, distances_squared) ==
              offsetof(AiState, distances_squared));
static_assert(offsetof(UpdateView, opaque8) == offsetof(AiState, opaque8));
static_assert(offsetof(UpdateView, opaque_e) == offsetof(AiState, opaque_e));
static_assert(offsetof(UpdateView, alerted) == offsetof(AiState, opaque18));
static_assert(offsetof(UpdateView, holds_stall_tile) ==
              offsetof(AiState, opaque_1c));
static_assert(offsetof(UpdateView, tile) == offsetof(AiState, tile));
static_assert(offsetof(UpdateView, previous_tile) ==
              offsetof(AiState, previous_tile));
static_assert(offsetof(UpdateView, remembered_target_tile) ==
              offsetof(AiState, remembered_target_tile));
static_assert(offsetof(UpdateView, target_memory_frames) ==
              offsetof(AiState, target_memory_frames));
static_assert(offsetof(UpdateView, position_xy) == offsetof(AiState, position));
static_assert(offsetof(UpdateView, actor) == offsetof(AiState, actor));
static_assert(offsetof(UpdateView, model) == offsetof(AiState, model));
static_assert(offsetof(UpdateView, status_flags) ==
              offsetof(AiState, action_delay));
static_assert(offsetof(UpdateView, detour_step) ==
              offsetof(AiState, detour_step));
static_assert(offsetof(UpdateView, opaque_fb) == offsetof(AiState, opaque_fb));
static_assert(offsetof(UpdateView, awareness_frames_remaining) ==
              offsetof(AiState, awareness_frames_remaining));
static_assert(offsetof(UpdateView, action_status) ==
              offsetof(AiState, action_status));
static_assert(offsetof(UpdateView, awareness_marker) ==
              offsetof(AiState, awareness_marker));
static_assert(offsetof(UpdateView, recovery_frames) ==
              offsetof(AiState, opaque_19a));
static_assert(offsetof(UpdateView, relations) == offsetof(AiState, relations));

std::uint32_t CurrentTileWord(const VS::Actors::ModelInstance& instance) {
  return static_cast<std::uint32_t>(instance.current_tile_x) |
         (static_cast<std::uint32_t>(instance.current_tile_layer)
          << kByteShift) |
         (static_cast<std::uint32_t>(instance.current_tile_z) << kHalfShift) |
         (static_cast<std::uint32_t>(instance.reserved5f) << kTopByteShift);
}

std::uint32_t PreviousTileWord(const VS::Actors::ModelInstance& instance) {
  return static_cast<std::uint32_t>(instance.previous_tile_x) |
         (static_cast<std::uint32_t>(instance.previous_tile_layer)
          << kByteShift) |
         (static_cast<std::uint32_t>(instance.previous_tile_z) << kHalfShift) |
         (static_cast<std::uint32_t>(instance.reserved63) << kTopByteShift);
}

// The battle actor list in order; aborts on a list longer than the table.
template <typename Visit>
void WalkBattleActors(Visit visit) {
  int walked = 0;
  for (VS::Battle::Actor* actor = VS::Battle::actors[0]; actor != nullptr;
       actor = actor->next) {
    Require(++walked <= kBattleActorSlots,
            "AI perception found a battle actor list longer than its table");
    visit(*actor);
  }
}

PerceptionShape ShapeOf(const AiState& state) {
  return PerceptionShape{.body_height = state.body_height,
                         .point_height = state.point_height,
                         .radius = state.radius};
}

PerceivingObserver LoadObserver(const ObserverView& view) {
  PerceivingObserver observer;
  observer.decays_hostility = view.decays_hostility != 0;
  observer.ignores_body_line = view.ignores_body_line != 0;
  observer.tile = view.tile.word;
  observer.position = Core::Math::PositionOf(view.position);
  observer.point_height = view.point_height;
  observer.actor_id = view.actor_id;
  observer.near_distance_squared = view.near_distance_squared;
  observer.view_cosine = view.view_cosine;
  observer.far_distance_squared = view.far_distance_squared;
  observer.body_height = view.body_height;
  observer.phase = view.phase;
  for (int slot = 0; slot < kPerceptionSlots; ++slot) {
    observer.relations.at(slot) =
        DecodePerceivedRelation(view.relations[slot].word);
    observer.distances_squared.at(slot) = view.distances_squared[slot];
  }
  return observer;
}

// The settings, tile, position and sizes are only read.
void StoreObserver(const PerceivingObserver& observer, ObserverView& view) {
  view.phase = observer.phase;
  for (int slot = 0; slot < kPerceptionSlots; ++slot) {
    view.relations[slot].word =
        EncodePerceivedRelation(observer.relations.at(slot));
    view.distances_squared[slot] = observer.distances_squared.at(slot);
  }
}

// The battle actors' models, the AI state table, the frame context's actors
// and tile flags, the gimmicks and the room terrain.
class LiveTargetPerceptionWorld final : public TargetPerceptionWorld {
 public:
  explicit LiveTargetPerceptionWorld(const ObserverView& view) : view_(view) {}

  [[nodiscard]] std::optional<PerceivedModel> Model(int target) const override {
    const VS::Battle::Actor* actor = VS::Battle::actors[target];
    if (actor == nullptr || actor->model == nullptr) return std::nullopt;
    const VS::Actors::ModelInstance& model = *actor->model;
    return PerceivedModel{.position = Core::Math::PositionOf(model.position),
                          .tile = CurrentTileWord(model),
                          .last_tile = PreviousTileWord(model),
                          .locomoting = model.locomotion_state != 0};
  }
  [[nodiscard]] std::optional<ObserverPose> ObserverModel() const override {
    const auto* model =
        reinterpret_cast<const VS::Actors::ModelInstance*>(view_.model);
    if (model == nullptr) return std::nullopt;
    return ObserverPose{.position = Core::Math::PositionOf(model->position),
                        .facing = model->facing,
                        .look_yaw = static_cast<std::int16_t>(model->look_yaw)};
  }
  [[nodiscard]] std::optional<PerceptionShape> AiStateShape(
      int target) const override {
    const AiState* state = VS::Battle::Ai::actorAiStates[target];
    if (state == nullptr) return std::nullopt;
    return ShapeOf(*state);
  }
  [[nodiscard]] std::optional<PerceptionShape> FrameShape(
      int target) const override {
    const AiState* state = LiveFrameContext().actors[target];
    if (state == nullptr) return std::nullopt;
    return ShapeOf(*state);
  }
  [[nodiscard]] bool HasGimmick(int target) const override {
    return VS::Field::gimmickInstances[target] != nullptr;
  }
  [[nodiscard]] std::optional<bool> IsMarkedTile(
      Core::Math::TileCoordinate tile) const override {
    const FrameContext& frame = LiveFrameContext();
    if (frame.tileFlags == nullptr || tile.x >= kNavigationGridSide ||
        tile.z >= kNavigationGridSide) {
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
    WalkBattleActors([&](const VS::Battle::Actor& actor) {
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

PerceivingActor Load(const VS::Battle::Actor& battle, const UpdateView& view) {
  PerceivingActor actor;
  actor.battle = PerceivingBattleActor{
      .id = static_cast<std::uint8_t>(battle.id),
      .command_state = battle.command_state,
      .command_opcode = battle.command_opcode,
      .command_flags =
          static_cast<std::uint8_t>(battle.command_flags & kCommandFlagBits),
      .action_recovery_timer = battle.stats->action_recovery_timer};
  actor.stayed_on_tile = view.opaque8;
  actor.alert = view.opaque_e;
  actor.perception_skipped = view.opaque14;
  actor.alerted = view.alerted;
  actor.holds_stall_tile = view.holds_stall_tile;
  actor.partial_perception = view.opaque_1d;
  actor.alert_rule = view.alert_rule;
  actor.tile = view.tile;
  actor.previous_tile = view.previous_tile.word;
  actor.remembered_target_tile = view.remembered_target_tile;
  actor.target_memory_frames = view.target_memory_frames;
  actor.detour_step_frames = view.detour_step_frames;
  actor.stall_frames = view.stall_frames;
  const auto xy = static_cast<std::uint32_t>(view.position_xy);
  const auto z = static_cast<std::uint32_t>(view.position_z_pad);
  actor.position = UnpackPosition(xy, z);
  actor.position_pad = static_cast<std::int16_t>(z >> kHalfShift);
  for (std::size_t word = 0; word < actor.history.x_words.size(); ++word) {
    actor.history.x_words.at(word) =
        static_cast<std::uint32_t>(view.history_x.words[word]);
    actor.history.z_words.at(word) =
        static_cast<std::uint32_t>(view.history_z.words[word]);
  }
  actor.history.mask = view.history_mask;
  actor.history_index = view.history_index;
  actor.action_delay = view.status_flags;
  actor.detour_step = view.detour_step;
  actor.detour_target = view.opaque_fb;
  actor.flee_timer = view.flee_timer;
  actor.fine_tile = view.opaque120;
  actor.observed_command = view.observed_command;
  actor.awareness_frames_remaining = view.awareness_frames_remaining;
  actor.action_status = view.action_status;
  actor.awareness_marker = view.awareness_marker;
  actor.movement_wait = view.movement_wait;
  actor.has_navigation_override = view.scripted_route != nullptr;
  actor.navigation_counter = static_cast<std::uint8_t>(view.detour_steps_left);
  actor.recovery_frames = view.recovery_frames;
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
void Store(const PerceivingActor& actor, VS::Battle::Actor& battle,
           UpdateView& view) {
  battle.command_flags = static_cast<std::uint8_t>(
      (battle.command_flags & ~kCommandFlagBits) |
      (actor.battle.command_flags & kCommandFlagBits));
  view.opaque8 = actor.stayed_on_tile;
  view.opaque_e = actor.alert;
  view.opaque14 = actor.perception_skipped;
  view.alerted = actor.alerted;
  view.holds_stall_tile = actor.holds_stall_tile;
  view.opaque_1d = actor.partial_perception;
  view.tile = actor.tile;
  view.previous_tile.word = actor.previous_tile;
  view.target_memory_frames = actor.target_memory_frames;
  view.detour_step_frames = actor.detour_step_frames;
  view.stall_frames = actor.stall_frames;
  view.position_xy =
      static_cast<int>(PackHalves(actor.position.x, actor.position.y));
  view.position_z_pad =
      static_cast<int>(PackHalves(actor.position.z, actor.position_pad));
  for (std::size_t word = 0; word < actor.history.x_words.size(); ++word) {
    view.history_x.words[word] =
        static_cast<int>(actor.history.x_words.at(word));
    view.history_z.words[word] =
        static_cast<int>(actor.history.z_words.at(word));
  }
  view.history_mask = actor.history.mask;
  view.history_index = actor.history_index;
  view.status_flags = actor.action_delay;
  view.detour_step = actor.detour_step;
  view.opaque_fb = actor.detour_target;
  view.opaque120 = actor.fine_tile;
  view.observed_command = actor.observed_command;
  view.movement_wait = actor.movement_wait;
  view.detour_steps_left = actor.navigation_counter;
  view.recovery_frames = actor.recovery_frames;
}

// One listed actor's model, links and HP, the frame budget, the battle
// actor list, the actor records and the AI routines. Each call taking the
// actor stores it into the AI state and the battle actor first and reloads
// it after.
class LiveActorPerceptionWorld final : public ActorPerceptionWorld {
 public:
  LiveActorPerceptionWorld(VS::Battle::Actor& battle, UpdateView& view)
      : battle_(battle), view_(view) {}

  [[nodiscard]] std::optional<PerceiverModel> Model() const override {
    // The AI state's model is the actor's record.
    const auto* record =
        reinterpret_cast<const VS::Actors::ActorRecord*>(view_.model);
    if (record == nullptr) return std::nullopt;
    const VS::Actors::ModelInstance& instance = record->instance;
    return PerceiverModel{
        .moving = instance.movement_mode != 0 || instance.locomotion_state != 0,
        .running = record->is_running != 0,
        .forces_perception = instance.was_drawn != 0,
        .tile = CurrentTileWord(instance)};
  }
  [[nodiscard]] std::optional<std::uint32_t> LinkedTile() const override {
    if (view_.tile_source == nullptr) return std::nullopt;
    return view_.tile_source->position;
  }
  [[nodiscard]] std::optional<LinkedPoint> LinkedPosition() const override {
    const VS::Battle::Ai::PositionLinkView* link = view_.position_source;
    if (link == nullptr) return std::nullopt;
    return LinkedPoint{
        .position = UnpackPosition(link->position, link->z_and_pad),
        .pad = static_cast<std::int16_t>(link->z_and_pad >> kHalfShift)};
  }
  [[nodiscard]] std::optional<PerceivedHp> Hp() const override {
    const VS::Battle::Actor* actor = view_.actor;
    if (actor == nullptr || actor->stats == nullptr) return std::nullopt;
    return PerceivedHp{
        .current = static_cast<std::uint16_t>(actor->stats->current_hp),
        .max = static_cast<std::uint16_t>(actor->stats->max_hp)};
  }
  [[nodiscard]] std::vector<int> ListedActorIds() const override {
    std::vector<int> ids;
    WalkBattleActors(
        [&](const VS::Battle::Actor& actor) { ids.push_back(actor.id); });
    return ids;
  }
  [[nodiscard]] std::optional<std::uint32_t> RecordTile(int id) const override {
    const VS::Actors::ActorRecord* record = VS::Actors::actorRecords[id];
    if (record == nullptr) return std::nullopt;
    return CurrentTileWord(record->instance);
  }
  [[nodiscard]] std::int32_t FrameTickBudget() const override {
    return VS::Battle::Ai::frameTickBudget;
  }
  [[nodiscard]] std::int32_t SecondPerceptionTarget() const override {
    return VS::Battle::Ai::secondPerceptionTarget;
  }
  [[nodiscard]] std::int32_t ElapsedFrameTicks() override {
    return Core::ElapsedFrameCounterTicks();
  }
  void SetLastBudgetTick(std::int32_t tick) override {
    LiveFrameContext().lastBudgetTick = tick;
  }

  void Disengage(PerceivingActor& actor) override {
    Run(actor, [&] { DisengageLiveActor(State()); });
  }
  void ResetAwarenessIndicator(PerceivingActor& actor) override {
    Run(actor, [&] {
      ResetLiveActorAwarenessIndicator(
          reinterpret_cast<VS::Battle::Ai::PlanEvaluationView*>(&view_));
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
      ready = RefreshActionCandidateReadinessEntry(
          reinterpret_cast<VS::Battle::Ai::CandidateReadinessView*>(&view_));
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

  VS::Battle::Actor& battle_;
  UpdateView& view_;
};

void UpdateListedActor(VS::Battle::Actor& battle) {
  Require(battle.id >= 0 && std::cmp_less(battle.id, kFrameActorSlots),
          "AI perception requires a listed actor inside the AI state table");
  auto* view =
      reinterpret_cast<UpdateView*>(LiveFrameContext().actors[battle.id]);
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
void UpdateLiveTargetPerception(VS::Battle::Ai::PerceptionView* state,
                                int target) {
  Require(state != nullptr, "AI target perception requires an AI state");
  Require(TryUpdateLiveTargetPerception(*state, target),
          "AI target perception requires a target inside the AI state table, "
          "both models, the frame's tile flags and listed actors inside the "
          "table with models");
}

// decomp: func_800D8EF4 (src/BATTLE/BATTLE.PRG/6FC00.c:983).
void UpdateLiveActorPerception() {
  VS::Battle::Ai::RoomAiState* room = VS::Battle::Ai::roomAiState;
  Require(room != nullptr, "AI perception requires the room AI state");
  PerceptionRoom typed{
      .readiness_level = room->readiness_level,
      .approach_moves = room->approach_moves,
      .candidate_engaged = VS::Battle::Ai::candidateEngagedThisUpdate};
  BeginActorPerception(typed);
  VS::Battle::Ai::candidateEngagedThisUpdate = typed.candidate_engaged;
  room->approach_moves = typed.approach_moves;
  room->readiness_level = typed.readiness_level;
  WalkBattleActors(UpdateListedActor);
}
}  // namespace VS::Battle::Ai
