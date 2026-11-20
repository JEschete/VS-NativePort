// Hand-maintained (tools/port/owned.csv).
// Live binding for the target approach (TargetApproach.cpp).
#include "Battle/Ai/TargetApproachLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Actors/ActorRecord.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Battle/Ai/TargetApproach.hpp"
#include "Core/Diagnostics.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Battle::Ai {
namespace {
using AiState = VS::Battle::Ai::ActorAiState;
using Approach = VS::Battle::Ai::TargetApproachView;
using Decision = VS::Battle::Ai::MovementDecisionView;

static_assert(std::tuple_size_v<decltype(AiState::relations)> ==
              kTargetApproachActorSlots);
static_assert(std::tuple_size_v<decltype(AiState::distances_squared)> ==
              kTargetApproachActorSlots);
static_assert(std::size(VS::Actors::actorRecords) >= kTargetApproachActorSlots);
static_assert(sizeof(VS::Battle::Ai::RelationWord) == sizeof(std::uint32_t));
static_assert(sizeof(VS::Battle::Ai::ApproachRelationBits) ==
              sizeof(std::uint32_t));

// The approach view names the actor state's unnamed bytes; both alias it.
static_assert(offsetof(Approach, close_approach) ==
              offsetof(AiState, reserved0));
static_assert(offsetof(Approach, gait) == offsetof(AiState, opaque1));
static_assert(offsetof(Approach, height_filter) == offsetof(AiState, opaque3));
static_assert(offsetof(Approach, near_target_detour) ==
              offsetof(AiState, opaque4));
static_assert(offsetof(Approach, toward_point) == offsetof(AiState, opaque9));
static_assert(offsetof(Approach, approach_directly) ==
              offsetof(AiState, opaque_d));
static_assert(offsetof(Approach, chases_lost_target) ==
              offsetof(AiState, reserved24));
static_assert(offsetof(Approach, approaches_target_point) ==
              offsetof(AiState, reserved33));
static_assert(offsetof(Approach, tile) == offsetof(AiState, tile));
static_assert(offsetof(Approach, saved_destination) ==
              offsetof(AiState, saved_destination));
static_assert(offsetof(Approach, x) == offsetof(AiState, position));
static_assert(offsetof(Approach, actor) == offsetof(AiState, actor));
static_assert(offsetof(Approach, model) == offsetof(AiState, model));
static_assert(offsetof(Approach, actor_id) == offsetof(AiState, actor_id));
static_assert(offsetof(Approach, recover_in_range) ==
              offsetof(AiState, opaque_e7));
static_assert(offsetof(Approach, walked_close) == offsetof(AiState, opaque_e8));
static_assert(offsetof(Approach, range_mode) == offsetof(AiState, opaque_e9));
static_assert(offsetof(Approach, target_x) == offsetof(AiState, target_x));
static_assert(offsetof(Approach, movement_target) ==
              offsetof(AiState, movement_target));
static_assert(offsetof(Approach, movement_flags) ==
              offsetof(AiState, movement_flags));
static_assert(offsetof(Approach, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(Approach, move_x) == offsetof(AiState, move_x));
static_assert(offsetof(Approach, relations) == offsetof(AiState, relations));
static_assert(offsetof(Approach, distances_squared) ==
              offsetof(AiState, distances_squared));
// The facing view, too.
static_assert(offsetof(Decision, x) == offsetof(AiState, position));
static_assert(offsetof(Decision, model) == offsetof(AiState, model));
static_assert(offsetof(Decision, actor_id) == offsetof(AiState, actor_id));
static_assert(offsetof(Decision, facing_delay) ==
              offsetof(AiState, facing_delay));
static_assert(offsetof(Decision, previous_facing_tile) ==
              offsetof(AiState, previous_facing_tile));
static_assert(offsetof(Decision, facing_pending) ==
              offsetof(AiState, facing_pending));
static_assert(offsetof(Decision, opaque_13a) == offsetof(AiState, opaque_13a));
static_assert(offsetof(Decision, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(Decision, move_x) == offsetof(AiState, move_x));
static_assert(offsetof(Decision, target_flags) == offsetof(AiState, relations));
static_assert(offsetof(Decision, reaction_pending) ==
              offsetof(AiState, reaction_pending));

Approach& ApproachOf(AiState& state) {
  return *reinterpret_cast<Approach*>(&state);
}

const Approach& ApproachOf(const AiState& state) {
  return *reinterpret_cast<const Approach*>(&state);
}

// The model's fields read as zero without a model; the entries require one.
TargetApproacher Load(const AiState& state) {
  const Approach& view = ApproachOf(state);
  const auto* model = state.model;
  const auto* model_record =
      model != nullptr ? reinterpret_cast<const VS::Actors::ActorRecord*>(model)
                       : nullptr;
  TargetApproacher approacher{
      .close_approach = view.close_approach,
      .gait = view.gait,
      .height_filter = view.height_filter,
      .near_target_detour = view.near_target_detour,
      .toward_point = view.toward_point,
      .facing_locked = view.facing_locked,
      .approach_directly = view.approach_directly,
      .chases_lost_target = view.chases_lost_target,
      .approaches_target_point = view.approaches_target_point,
      .tile = view.tile.word,
      .saved_destination = view.saved_destination.word,
      .x = view.x,
      .z = view.z,
      .flies = model != nullptr && model->flies != 0,
      .model_look_yaw =
          model_record != nullptr
              ? std::bit_cast<std::int16_t>(model_record->instance.look_yaw)
              : static_cast<std::int16_t>(0),
      .model_facing = model_record != nullptr ? model_record->instance.facing
                                              : static_cast<std::int16_t>(0),
      .actor_id = view.actor_id,
      .facing_delay = state.facing_delay,
      .range = view.range,
      .previous_range = view.previous_range,
      .chasing_lost_target = view.chasing_lost_target,
      .target_id = view.target_id,
      .chase_timeout = view.timeout,
      .recover_in_range = view.recover_in_range,
      .walked_close = view.walked_close,
      .range_mode = view.range_mode,
      .previous_option = view.previous_option,
      .previous_target_id = view.previous_target_id,
      .chase_destination = view.cached_destination.word,
      .target_half_tile_x = view.target_tile_x,
      .target_half_tile_z = view.target_tile_z,
      .retarget_timer = view.retarget_timer,
      .previous_facing_tile = state.previous_facing_tile.word,
      .facing_pending = state.facing_pending,
      .turn_timer = state.opaque_13a,
      .target_x = view.target_x,
      .target_z = view.target_z,
      .movement_target = view.movement_target,
      .movement_flags = view.movement_flags,
      .destination = view.destination.word,
      .move_x = view.move_x,
      .move_z = view.move_z,
      .angle = static_cast<std::int16_t>(state.angle),
      .reaction_pending = state.reaction_pending};
  for (std::size_t id = 0; id < approacher.relations.size(); ++id) {
    approacher.relations.at(id) =
        std::bit_cast<std::uint32_t>(state.relations[id]);
    approacher.distances_squared.at(id) = state.distances_squared[id];
  }
  return approacher;
}

// The settings, the model and the actor's relations and distances are not
// written back.
void Store(const TargetApproacher& approacher, AiState& state) {
  Approach& view = ApproachOf(state);
  view.close_approach = approacher.close_approach;
  view.gait = approacher.gait;
  view.height_filter = approacher.height_filter;
  view.near_target_detour = approacher.near_target_detour;
  view.toward_point = approacher.toward_point;
  view.facing_locked = approacher.facing_locked;
  view.approach_directly = approacher.approach_directly;
  view.tile.word = approacher.tile;
  view.saved_destination.word = approacher.saved_destination;
  view.x = approacher.x;
  view.z = approacher.z;
  state.facing_delay = approacher.facing_delay;
  view.range = approacher.range;
  view.previous_range = approacher.previous_range;
  view.chasing_lost_target = approacher.chasing_lost_target;
  view.target_id = approacher.target_id;
  view.timeout = approacher.chase_timeout;
  view.recover_in_range = approacher.recover_in_range;
  view.walked_close = approacher.walked_close;
  view.range_mode = approacher.range_mode;
  view.previous_option = approacher.previous_option;
  view.previous_target_id = approacher.previous_target_id;
  view.cached_destination.word = approacher.chase_destination;
  view.target_tile_x = approacher.target_half_tile_x;
  view.target_tile_z = approacher.target_half_tile_z;
  view.retarget_timer = approacher.retarget_timer;
  state.previous_facing_tile.word = approacher.previous_facing_tile;
  state.facing_pending = approacher.facing_pending;
  state.opaque_13a = approacher.turn_timer;
  view.target_x = approacher.target_x;
  view.target_z = approacher.target_z;
  view.movement_target = approacher.movement_target;
  view.movement_flags = approacher.movement_flags;
  view.destination.word = approacher.destination;
  view.move_x = approacher.move_x;
  view.move_z = approacher.move_z;
  state.angle = approacher.angle;
  state.reaction_pending = approacher.reaction_pending;
}

// The actor models, the battle actor and the actor's movement code. Each call
// taking the approacher stores it into the AI state first and reloads it
// after.
class LiveTargetApproachWorld final : public TargetApproachWorld {
 public:
  explicit LiveTargetApproachWorld(AiState& state) : state_(state) {}

  [[nodiscard]] std::optional<ApproachTargetModel> TargetModel(
      int actor_id) const override {
    if (actor_id < 0 || static_cast<std::size_t>(actor_id) >=
                            std::size(VS::Actors::actorRecords)) {
      return std::nullopt;
    }
    const auto* record = VS::Actors::actorRecords[actor_id];
    if (record == nullptr) return std::nullopt;
    const auto& instance = record->instance;
    return ApproachTargetModel{
        .moving = instance.movement_mode != 0 || instance.locomotion_state != 0,
        .x = instance.position.vx,
        .z = instance.position.vz};
  }
  [[nodiscard]] std::optional<std::uint8_t> CommandOpcode() const override {
    const auto* actor =
        reinterpret_cast<const VS::Battle::Actor*>(state_.actor);
    if (actor == nullptr) return std::nullopt;
    return actor->command_opcode;
  }
  void IssueCommand(TargetApproacher& approacher,
                    CommandIntent intent) override {
    Run(approacher,
        [&] { IssueLiveActorCommand(&state_, static_cast<int>(intent)); });
  }
  void ProjectClearRadialDestination(TargetApproacher& approacher,
                                     std::uint32_t source, int angle,
                                     std::uint32_t distance) override {
    Run(approacher, [&] {
      VS::Battle::Ai::TilePosition position{};
      position.word = source;
      ProjectLiveClearRadialDestination(&state_, position, angle, distance);
    });
  }
  void DispatchLocomotion(TargetApproacher& approacher, int kind,
                          int angle) override {
    Run(approacher,
        [&] { DispatchLiveAiLocomotionMode(&state_, kind, angle); });
  }
  void RecoverMovement(TargetApproacher& approacher) override {
    Run(approacher, [&] { RecoverLiveMovement(&state_); });
  }
  void SetTemporaryAwareness(TargetApproacher& approacher, int marker,
                             int frames) override {
    Run(approacher,
        [&] { SetLiveTemporaryAwarenessIndicator(&state_, marker, frames); });
  }
  [[nodiscard]] int HesitationTurn(TargetApproacher& approacher, int chance,
                                   int kind, int slot) override {
    int hesitating = 0;
    Run(approacher, [&] {
      hesitating = UpdateLiveHesitationTurn(&state_, chance, kind, slot);
    });
    return hesitating;
  }
  void ClearWalkTests() override {
    // The original cleared only the first cache byte, 0x1F8003D0.
    LiveFrameContext().walk_valid = 0;
  }
  void RefreshTargetTile(TargetApproacher& approacher) override {
    Run(approacher, [&] { RefreshLiveAiTargetTile(&ApproachOf(state_)); });
  }

 private:
  template <typename Call>
  void Run(TargetApproacher& approacher, Call&& call) {
    Store(approacher, state_);
    call();
    approacher = Load(state_);
  }

  AiState& state_;
};

// Runs a typed routine over the state's approacher, stores it back and aborts
// when the routine refused.
template <typename Routine>
int Update(AiState& state, const char* refusal, Routine routine) {
  LiveTargetApproachWorld world(state);
  auto approacher = Load(state);
  const std::optional<bool> result = routine(world, approacher);
  Store(approacher, state);
  Require(result.has_value(), refusal);
  return result.value_or(false) ? 1 : 0;
}

AiState& StateWithModel(void* state) {
  auto* actor = static_cast<AiState*>(state);
  Require(actor != nullptr && actor->model != nullptr,
          "AI target approach requires an AI state and a model");
  return *actor;
}
}  // namespace

int UpdateLiveTargetApproachMovement(VS::Battle::Ai::TargetApproachView* state,
                                     int option) {
  return Update(StateWithModel(state),
                "AI target approach refused its target or battle actor",
                [&](TargetApproachWorld& world, TargetApproacher& approacher) {
                  return UpdateTargetApproachMovement(world, approacher,
                                                      option);
                });
}

int UpdateLiveTargetFacing(VS::Battle::Ai::MovementDecisionView* state,
                           int target) {
  return Update(StateWithModel(state), "AI target facing refused its target",
                [&](TargetApproachWorld& world, TargetApproacher& approacher) {
                  return UpdateTargetFacing(world, approacher, target);
                });
}
}  // namespace VS::Battle::Ai
