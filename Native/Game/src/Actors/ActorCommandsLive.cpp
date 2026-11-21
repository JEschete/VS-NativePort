// Hand-maintained (tools/port/owned.csv).
// Live binding for the queued actor commands and Ashley's movement input
// (ActorCommands.cpp).
#include "Actors/ActorCommandsLive.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Actors/ActorCommands.hpp"
#include "Actors/ActorCommandsLiveState.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/Location.hpp"
#include "Actors/MotionRequestsLive.hpp"
#include "Actors/MovementCommand.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionAnimation.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/TargetingLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Runtime.hpp"
#include "Effects/ActionEffectCallbacksLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/ChestInteractionsLive.hpp"
#include "Field/Doors.hpp"
#include "Field/Interactions.hpp"
#include "Field/Mpd.hpp"
#include "Field/TransitionsLiveState.hpp"
#include "Input/Actions.hpp"
#include "Input/ControllerLiveState.hpp"

using VS::Core::Gte::SVECTOR;

// decomp: D_800F1A50 (D_800F1A50@BATTLE/BATTLE.PRG)
int VS::Actors::ashleyMoveInput[VS::Actors::kAshleyMoveInputWords];

namespace VS::Actors {
namespace {
// VS::Actors::ashleyMoveInput words the movement input writes.
constexpr std::size_t kMoveInputLateral = 1;
constexpr std::size_t kMoveInputForward = 3;
// VS::Input::LegacyControllerPort::controller_type of an analog controller.
constexpr std::uint8_t kAnalogController = 7;
// func_8001B200 code a waiting command with an unknown opcode reports.
constexpr int kUnknownWaitingOpcodeDiagnostic = 0xC1;
constexpr std::uint8_t kNoPreviewSelected = 0xFF;

static_assert(std::size(ashleyMoveInput) > kMoveInputForward);

CommandActorGait GaitOf(const VS::Battle::ActorStats& stats) {
  // The stats bytes are unsigned (the original builds with unsigned char).
  return {.walk_speed = static_cast<std::uint8_t>(stats.walk_speed),
          .gait_class = static_cast<std::uint8_t>(stats.gait_class),
          .run_speed = static_cast<std::uint8_t>(stats.run_speed)};
}

PackedTile TileOf(const VS::Actors::TileTarget& tile) {
  return {.x = static_cast<std::uint8_t>(tile.tile_x),
          .layer = static_cast<std::uint8_t>(tile.tile_layer),
          .z = static_cast<std::uint8_t>(tile.tile_z),
          .spare = static_cast<std::uint8_t>(tile.facing)};
}

SVECTOR NativeVector(const MotionVector& value) {
  return SVECTOR{.vx = value.x, .vy = value.y, .vz = value.z, .pad = value.w};
}

// func_800A1108 for any actor slot; a slot with no actor or gimmick leaves
// the placement zeroed (the original left the caller's buffer unwritten).
CommandPlacement PlacementOf(int actor_id) {
  VS::Actors::ActorLocation location{};
  if (LiveActorPlacement(actor_id, &location) != 0) {
    VS::Core::Debugf("actors.commands", "Placement of empty actor slot %d",
                     actor_id);
  }
  return {.tile = TileOf(location.packed_tile.target),
          .position = {.x = location.position.vx,
                       .y = location.position.vy,
                       .z = location.position.vz,
                       .w = location.position.pad}};
}

class LiveCommandHost final : public ActorCommandHost {
 public:
  [[nodiscard]] std::optional<ActorCommand> Command(
      int actor_id) const override {
    const VS::Battle::Actor* actor = Actor(actor_id);
    if (actor == nullptr) {
      return std::nullopt;
    }
    return ActorCommand{.state = actor->command_state,
                        .opcode = actor->command_opcode,
                        .target = actor->command_target,
                        .flags = actor->command_flags,
                        .args = {actor->command_arg0, actor->command_arg1.word,
                                 actor->command_arg2}};
  }
  void SetCommandState(int actor_id, std::uint8_t state) override {
    Required(actor_id).command_state = state;
  }
  void SetCommandFlags(int actor_id, std::uint8_t flags) override {
    Required(actor_id).command_flags = flags;
  }
  void SetCommandArgs(int actor_id, const CommandArgs& args) override {
    VS::Battle::Actor& actor = Required(actor_id);
    actor.command_arg0 = args[0];
    actor.command_arg1.word = args[1];
    actor.command_arg2 = args[2];
  }
  [[nodiscard]] unsigned Kind(int actor_id) const override {
    return Required(actor_id).kind;
  }
  [[nodiscard]] CommandActorGait Gait(int actor_id) const override {
    return GaitOf(*Required(actor_id).stats);
  }
  [[nodiscard]] std::uint32_t StateFlags(int actor_id) const override {
    return LiveActorStateFlags(actor_id);
  }
  [[nodiscard]] int ControlMode() const override {
    return static_cast<int>(VS::Field::controlMode);
  }
  [[nodiscard]] CommandPlacement Placement(int actor_id) override {
    return PlacementOf(actor_id);
  }

  void ReportUnknownWaitingOpcode(int actor_id, int opcode) override {
    VS::Core::Debugf("actors.commands", "Actor %d waits on unknown opcode %d",
                     actor_id, opcode);
    Core::IgnoreDiagnostic(kUnknownWaitingOpcodeDiagnostic, 0);
  }
  // Cross-slice calls (row 21 movement phase): retarget when the movement
  // core is promoted.
  void UpdateMovement(int actor_id) override {
    UpdateLiveActorMovement(actor_id);
  }
  int CheckTrapAndFallTriggers(int actor_id,
                               const CommandPlacement& placement) override {
    return static_cast<int>(VS::Field::CheckLiveActorTrapAndFallTriggers(
        actor_id, {.tile = placement.tile, .position = placement.position}));
  }

  void PlayModelSound(int actor_id, int sound) override {
    VS::Audio::LivePlayActorModelSound(Required(actor_id).model, sound);
  }
  void TintRed(int actor_id) override {
    VS::Effects::LiveTintActionActorRed(actor_id, 0);
  }
  void RestoreTint() override { VS::Effects::LiveRestoreActionActorTint(); }
  void RestoreNeutralPose(int actor_id) override {
    RestoreLiveActorNeutralPose(actor_id);
  }
  void RestoreIdlePose(int actor_id) override {
    RestoreLiveActorIdlePose(actor_id);
  }
  void SetHeadTracking(int actor_id, int target,
                       const std::optional<MotionVector>& aim,
                       int step) override {
    if (aim) {
      const SVECTOR vector = NativeVector(*aim);
      static_cast<void>(
          SetLiveActorHeadTracking(actor_id, target, &vector, step));
    } else {
      static_cast<void>(
          SetLiveActorHeadTracking(actor_id, target, nullptr, step));
    }
  }

  void TurnNavigation(int actor_id, int facing) override {
    TurnLiveNavigationActor(actor_id, facing);
  }
  void TurnAtSpeed(int actor_id, std::int16_t facing, int speed) override {
    TurnLiveActorAtSpeed(actor_id, facing, speed);
  }
  void QueueNavigationTarget(int actor_id, int direction, int distance,
                             int facing) override {
    QueueNavigationTileTarget(ActorRecordMovementRequests(), actor_id,
                              direction, distance, facing);
  }
  void BeginJump(int actor_id, CommandJump jump, PackedTile target, int speed,
                 int facing) override {
    switch (jump) {
      case CommandJump::kAirborne:
        BeginLiveAirborneTileJump(actor_id, target, speed, facing);
        break;
      case CommandJump::kNavigation:
        BeginLiveNavigationJump(actor_id, target, speed, facing);
        break;
      case CommandJump::kTile:
        BeginLiveTileJump(actor_id, target, speed, facing);
        break;
    }
  }
  void BeginTeleport(int actor_id, PackedTile target, int facing) override {
    BeginLiveTeleport(actor_id, target, facing);
  }
  void BeginTravel(int actor_id, TravelPose pose, const MotionVector& target,
                   int facing) override {
    if (pose == TravelPose::kTimed) {
      BeginLiveTimedTravel(actor_id, target, facing);
    } else {
      BeginLiveTileTravel(actor_id, target, facing);
    }
  }
  void QueueGroundOffset(int actor_id, const MotionVector& step) override {
    QueueLiveGroundOffset(actor_id, step);
  }
  void QueueMovementVector(int actor_id, const MotionVector& step,
                           int facing) override {
    QueueLiveMovementVector(actor_id, step, facing);
  }
  void QueueFlightMovement(int actor_id, const MotionVector& step, int speed,
                           int facing) override {
    QueueLiveFlightMovement(actor_id, step, speed, facing);
  }
  void FinishCombatPose(int actor_id, std::int16_t facing) override {
    Battle::FinishLiveEnemyCombatPose(actor_id, facing);
  }
  [[nodiscard]] std::optional<std::uint16_t> LimbActionId(
      int actor_id, unsigned limb, unsigned slot) const override {
    const VS::Battle::ActorStats& stats = *Required(actor_id).stats;
    if (limb >= std::size(stats.limb_actions) ||
        slot >= std::size(stats.limb_actions[0])) {
      VS::Core::Debugf("actors.commands",
                       "Actor %d limb action [%u][%u] is outside its table",
                       actor_id, limb, slot);
      return std::nullopt;
    }
    return stats.limb_actions[limb][slot].action_id;
  }
  bool BeginActionSelection(int actor_id, std::uint16_t action) override {
    VS::Battle::LiveBeginActorActionSelection(Required(actor_id).id, action);
    return VS::Battle::actionState->selected_preview == kNoPreviewSelected;
  }

 private:
  static VS::Battle::Actor* Actor(int actor_id) {
    if (actor_id < 0 ||
        static_cast<std::size_t>(actor_id) >= std::size(VS::Battle::actors)) {
      return nullptr;
    }
    return VS::Battle::actors[actor_id];
  }
  static VS::Battle::Actor& Required(int actor_id) {
    VS::Battle::Actor* actor = Actor(actor_id);
    if (actor == nullptr) {
      VS::Core::Errorf("actors.commands", "Command requires battle actor %d",
                       actor_id);
      std::abort();
    }
    return *actor;
  }
};

class LiveAshleyHost final : public AshleyMovementHost {
 public:
  void ConstrainCameraToPlayer() override {
    VS::Actors::ActorLocation location{};
    static_cast<void>(LiveActorPlacement(0, &location));
    VS::Field::LiveUpdateCameraTileConstraint(0, &location.packed_tile.target);
  }
  [[nodiscard]] CommandActorGait PlayerGait() const override {
    return GaitOf(*VS::Battle::characterState->stats);
  }
  [[nodiscard]] std::uint32_t PlayerStateFlags() const override {
    return LiveActorStateFlags(0);
  }
  [[nodiscard]] unsigned ScreenTransitionStep() const override {
    return VS::Field::screenTransitionStep;
  }
  [[nodiscard]] std::optional<int> DoorEntryFacing() const override {
    if (VS::Field::pendingDestinationDoor.walks_in == 0) {
      return std::nullopt;
    }
    return static_cast<int>(VS::Field::pendingDestinationDoor.entry_facing);
  }
  void UpdateCameraOrbit() override { VS::Field::LiveUpdateCameraOrbitInput(); }
  [[nodiscard]] AshleyControls Controls() const override {
    const VS::Input::LegacyControllerPort& port = VS::Input::controllerPorts[0];
    const auto held = static_cast<std::uint32_t>(VS::Input::previousButtons);
    const auto has = [held](std::uint32_t button) {
      return (held & button) != 0;
    };
    AshleyControls controls{
        .stick = std::nullopt,
        .held = {.up = has(VS::Input::Up),
                 .down = has(VS::Input::Down),
                 .left = has(VS::Input::Left),
                 .right = has(VS::Input::Right)},
        .walk_held = has(VS::Input::R2),
        .jump_pressed = (VS::Input::buttonsPressed & VS::Input::Square) != 0};
    if (port.controller_type == kAnalogController) {
      controls.stick =
          AshleyStick{.x = port.right_stick_x, .y = port.right_stick_y};
    }
    return controls;
  }
  [[nodiscard]] int CameraDistance() const override {
    return VS::Field::sphericalCamera.values.distance;
  }
  void SetCameraDistance(int distance) override {
    VS::Field::sphericalCamera.values.distance = distance;
  }
  [[nodiscard]] int CameraYaw() const override {
    return VS::Field::sphericalCamera.values.yaw;
  }
  void SetMoveInput(AshleyMoveInput input) override {
    ashleyMoveInput[kMoveInputLateral] = input.lateral;
    ashleyMoveInput[kMoveInputForward] = input.forward;
  }
  void SetDirectMovement(int direction, int speed) override {
    VS::Actors::SetDirectMovement(ActorRecordMovementRequests(), 0, direction,
                                  speed);
  }
  void BeginJumpMovement(int direction, int distance) override {
    BeginLiveActorJumpMovement(0, direction, distance);
  }
};
}  // namespace

void UpdateLiveAshleyMovementInput() {
  LiveAshleyHost host;
  UpdateAshleyMovementInput(host);
}

int ExecuteLiveQueuedActorCommands() {
  LiveCommandHost host;
  return ExecuteQueuedActorCommands(host);
}
}  // namespace VS::Actors
