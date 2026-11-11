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

#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Battle/Ai/TargetApproach.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using AiState = Battle_Ai_ActorAiState;
using Approach = Battle_Ai_TargetApproachView;
using Decision = Battle_Ai_MovementDecisionView;

static_assert(std::extent_v<decltype(AiState::relations)> ==
              kTargetApproachActorSlots);
static_assert(std::extent_v<decltype(AiState::distancesSquared)> ==
              kTargetApproachActorSlots);
static_assert(std::size(Actors_actorRecords) >= kTargetApproachActorSlots);
static_assert(sizeof(Battle_Ai_RelationWord) == sizeof(std::uint32_t));
static_assert(sizeof(Battle_Ai_ApproachRelationBits) == sizeof(std::uint32_t));

// The approach view names the actor state's unnamed bytes; both alias it.
static_assert(offsetof(Approach, opaque0) == offsetof(AiState, reserved0));
static_assert(offsetof(Approach, opaque1) == offsetof(AiState, opaque1));
static_assert(offsetof(Approach, opaque3) == offsetof(AiState, opaque3));
static_assert(offsetof(Approach, opaque4) == offsetof(AiState, opaque4));
static_assert(offsetof(Approach, opaque9) == offsetof(AiState, opaque9));
static_assert(offsetof(Approach, opaqueD) == offsetof(AiState, opaqueD));
static_assert(offsetof(Approach, settingsBit13) ==
              offsetof(AiState, reserved24));
static_assert(offsetof(Approach, settingsByte0Bit7) ==
              offsetof(AiState, reserved33));
static_assert(offsetof(Approach, tile) == offsetof(AiState, tile));
static_assert(offsetof(Approach, savedDestination) ==
              offsetof(AiState, savedDestination));
static_assert(offsetof(Approach, x) == offsetof(AiState, position));
static_assert(offsetof(Approach, actor) == offsetof(AiState, actor));
static_assert(offsetof(Approach, model) == offsetof(AiState, model));
static_assert(offsetof(Approach, actorId) == offsetof(AiState, actorId));
static_assert(offsetof(Approach, opaqueE7) == offsetof(AiState, opaqueE7));
static_assert(offsetof(Approach, opaqueE8) == offsetof(AiState, opaqueE8));
static_assert(offsetof(Approach, rangeMode) == offsetof(AiState, opaqueE9));
static_assert(offsetof(Approach, targetX) == offsetof(AiState, targetX));
static_assert(offsetof(Approach, movementTarget) ==
              offsetof(AiState, movementTarget));
static_assert(offsetof(Approach, movementFlags) ==
              offsetof(AiState, movementFlags));
static_assert(offsetof(Approach, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(Approach, moveX) == offsetof(AiState, moveX));
static_assert(offsetof(Approach, relations) == offsetof(AiState, relations));
static_assert(offsetof(Approach, distancesSquared) ==
              offsetof(AiState, distancesSquared));
// The facing view, too.
static_assert(offsetof(Decision, x) == offsetof(AiState, position));
static_assert(offsetof(Decision, model) == offsetof(AiState, model));
static_assert(offsetof(Decision, actorId) == offsetof(AiState, actorId));
static_assert(offsetof(Decision, facingDelay) ==
              offsetof(AiState, facingDelay));
static_assert(offsetof(Decision, previousFacingTile) ==
              offsetof(AiState, previousFacingTile));
static_assert(offsetof(Decision, facingPending) ==
              offsetof(AiState, facingPending));
static_assert(offsetof(Decision, opaque13A) == offsetof(AiState, opaque13A));
static_assert(offsetof(Decision, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(Decision, moveX) == offsetof(AiState, moveX));
static_assert(offsetof(Decision, targetFlags) == offsetof(AiState, relations));
static_assert(offsetof(Decision, reactionPending) ==
              offsetof(AiState, reactionPending));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

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
  TargetApproacher approacher{
      .close_approach = view.opaque0,
      .gait = view.opaque1,
      .height_filter = view.opaque3,
      .near_target_detour = view.opaque4,
      .toward_point = view.opaque9,
      .facing_locked = view.opaqueC,
      .approach_directly = view.opaqueD,
      .chases_lost_target = view.settingsBit13,
      .approaches_target_point = view.settingsByte0Bit7,
      .tile = view.tile.word,
      .saved_destination = view.savedDestination.word,
      .x = view.x,
      .z = view.z,
      .flies = model != nullptr && model->opaqueABit5 != 0,
      .model_look_yaw =
          model != nullptr ? model->opaque14 : static_cast<std::int16_t>(0),
      .model_facing =
          model != nullptr ? model->opaque26 : static_cast<std::int16_t>(0),
      .actor_id = view.actorId,
      .facing_delay = state.facingDelay,
      .range = view.range,
      .previous_range = view.previousRange,
      .chasing_lost_target = view.opaqueE4,
      .target_id = view.targetId,
      .chase_timeout = view.timeout,
      .recover_in_range = view.opaqueE7,
      .walked_close = view.opaqueE8,
      .range_mode = view.rangeMode,
      .previous_option = view.previousOption,
      .previous_target_id = view.previousTargetId,
      .chase_destination = view.cachedDestination.word,
      .target_half_tile_x = view.targetTileX,
      .target_half_tile_z = view.targetTileZ,
      .retarget_timer = view.opaqueF8,
      .previous_facing_tile = state.previousFacingTile.word,
      .facing_pending = state.facingPending,
      .turn_timer = state.opaque13A,
      .target_x = view.targetX,
      .target_z = view.targetZ,
      .movement_target = view.movementTarget,
      .movement_flags = view.movementFlags,
      .destination = view.destination.word,
      .move_x = view.moveX,
      .move_z = view.moveZ,
      .angle = static_cast<std::int16_t>(state.angle),
      .reaction_pending = state.reactionPending};
  for (std::size_t id = 0; id < approacher.relations.size(); ++id) {
    approacher.relations.at(id) =
        std::bit_cast<std::uint32_t>(state.relations[id]);
    approacher.distances_squared.at(id) = state.distancesSquared[id];
  }
  return approacher;
}

// The settings, the model and the actor's relations and distances are not
// written back.
void Store(const TargetApproacher& approacher, AiState& state) {
  Approach& view = ApproachOf(state);
  view.opaque0 = approacher.close_approach;
  view.opaque1 = approacher.gait;
  view.opaque3 = approacher.height_filter;
  view.opaque4 = approacher.near_target_detour;
  view.opaque9 = approacher.toward_point;
  view.opaqueC = approacher.facing_locked;
  view.opaqueD = approacher.approach_directly;
  view.tile.word = approacher.tile;
  view.savedDestination.word = approacher.saved_destination;
  view.x = approacher.x;
  view.z = approacher.z;
  state.facingDelay = approacher.facing_delay;
  view.range = approacher.range;
  view.previousRange = approacher.previous_range;
  view.opaqueE4 = approacher.chasing_lost_target;
  view.targetId = approacher.target_id;
  view.timeout = approacher.chase_timeout;
  view.opaqueE7 = approacher.recover_in_range;
  view.opaqueE8 = approacher.walked_close;
  view.rangeMode = approacher.range_mode;
  view.previousOption = approacher.previous_option;
  view.previousTargetId = approacher.previous_target_id;
  view.cachedDestination.word = approacher.chase_destination;
  view.targetTileX = approacher.target_half_tile_x;
  view.targetTileZ = approacher.target_half_tile_z;
  view.opaqueF8 = approacher.retarget_timer;
  state.previousFacingTile.word = approacher.previous_facing_tile;
  state.facingPending = approacher.facing_pending;
  state.opaque13A = approacher.turn_timer;
  view.targetX = approacher.target_x;
  view.targetZ = approacher.target_z;
  view.movementTarget = approacher.movement_target;
  view.movementFlags = approacher.movement_flags;
  view.destination.word = approacher.destination;
  view.moveX = approacher.move_x;
  view.moveZ = approacher.move_z;
  state.angle = approacher.angle;
  state.reactionPending = approacher.reaction_pending;
}

// The actor models, the battle actor and the actor's movement code. Each call
// taking the approacher stores it into the AI state first and reloads it
// after.
class LiveTargetApproachWorld final : public TargetApproachWorld {
 public:
  explicit LiveTargetApproachWorld(AiState& state) : state_(state) {}

  [[nodiscard]] std::optional<ApproachTargetModel> TargetModel(
      int actor_id) const override {
    if (actor_id < 0 ||
        static_cast<std::size_t>(actor_id) >= std::size(Actors_actorRecords)) {
      return std::nullopt;
    }
    const auto* record = Actors_actorRecords[actor_id];
    if (record == nullptr) return std::nullopt;
    const auto& instance = record->instance;
    return ApproachTargetModel{
        .moving = instance.movementMode != 0 || instance.locomotionState != 0,
        .x = instance.position.vx,
        .z = instance.position.vz};
  }
  [[nodiscard]] std::optional<std::uint8_t> CommandOpcode() const override {
    const auto* actor = reinterpret_cast<const Battle_Actor*>(state_.actor);
    if (actor == nullptr) return std::nullopt;
    return actor->commandOpcode;
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
      Battle_Ai_TilePosition position{};
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
    Battle_Ai_FrameState()->walk_valid = 0;
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

int UpdateLiveTargetApproachMovement(Battle_Ai_TargetApproachView* state,
                                     int option) {
  return Update(StateWithModel(state),
                "AI target approach refused its target or battle actor",
                [&](TargetApproachWorld& world, TargetApproacher& approacher) {
                  return UpdateTargetApproachMovement(world, approacher,
                                                      option);
                });
}

int UpdateLiveTargetFacing(Battle_Ai_MovementDecisionView* state, int target) {
  return Update(StateWithModel(state), "AI target facing refused its target",
                [&](TargetApproachWorld& world, TargetApproacher& approacher) {
                  return UpdateTargetFacing(world, approacher, target);
                });
}
}  // namespace VS::Battle::Ai
