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
#include "Actors/ActorCommandsLiveState.h"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/MotionRequestsLive.hpp"
#include "Actors/MovementCommand.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/TargetingLive.hpp"
#include "Core/Runtime.hpp"
#include "Field/CameraLive.hpp"
#include "Field/ChestInteractionsLive.h"

extern "C" {
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/TargetingState.h"
#include "Core/Diagnostics.h"
#include "Effects/ActionEffects.h"
#include "Field/CameraLiveState.h"
#include "Field/Doors.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/TransitionsLiveState.h"
#include "Input/ControllerLiveState.h"

// decomp: D_800F1A50 (D_800F1A50@BATTLE/BATTLE.PRG)
int Actors_ashleyMoveInput[kActorsAshleyMoveInputWords];
}

namespace VS::Actors {
namespace {
// Actors_ashleyMoveInput words the movement input writes.
constexpr std::size_t kMoveInputLateral = 1;
constexpr std::size_t kMoveInputForward = 3;
// Input_ControllerPortState.controllerType of an analog controller.
constexpr std::uint8_t kAnalogController = 7;
// func_8001B200 code a waiting command with an unknown opcode reports.
constexpr int kUnknownWaitingOpcodeDiagnostic = 0xC1;
constexpr std::uint8_t kNoPreviewSelected = 0xFF;

static_assert(std::size(Actors_ashleyMoveInput) > kMoveInputForward);

CommandActorGait GaitOf(const Battle_ActorStats& stats) {
  // The stats bytes are unsigned (the original builds with unsigned char).
  return {.walk_speed = static_cast<std::uint8_t>(stats.walkSpeed),
          .gait_class = static_cast<std::uint8_t>(stats.gaitClass),
          .run_speed = static_cast<std::uint8_t>(stats.runSpeed)};
}

PackedTile TileOf(const Actors_TileTarget& tile) {
  return {.x = static_cast<std::uint8_t>(tile.tileX),
          .layer = static_cast<std::uint8_t>(tile.tileLayer),
          .z = static_cast<std::uint8_t>(tile.tileZ),
          .spare = static_cast<std::uint8_t>(tile.facing)};
}

SVECTOR NativeVector(const MotionVector& value) {
  return SVECTOR{.vx = value.x, .vy = value.y, .vz = value.z, .pad = value.w};
}

// func_800A1108 for any actor slot; a slot with no actor or gimmick leaves
// the placement zeroed (the original left the caller's buffer unwritten).
CommandPlacement PlacementOf(int actor_id) {
  Actors_ActorLocation location{};
  if (LiveActorPlacement(actor_id, &location) != 0) {
    Core_Debugf("actors.commands", "Placement of empty actor slot %d",
                actor_id);
  }
  return {.tile = TileOf(location.packedTile.target),
          .position = {.x = location.position.vx,
                       .y = location.position.vy,
                       .z = location.position.vz,
                       .w = location.position.pad}};
}

class LiveCommandHost final : public ActorCommandHost {
 public:
  [[nodiscard]] std::optional<ActorCommand> Command(
      int actor_id) const override {
    const Battle_Actor* actor = Actor(actor_id);
    if (actor == nullptr) {
      return std::nullopt;
    }
    return ActorCommand{.state = actor->commandState,
                        .opcode = actor->commandOpcode,
                        .target = actor->commandTarget,
                        .flags = actor->commandFlags,
                        .args = {actor->commandArg0, actor->commandArg1.word,
                                 actor->commandArg2}};
  }
  void SetCommandState(int actor_id, std::uint8_t state) override {
    Required(actor_id).commandState = state;
  }
  void SetCommandFlags(int actor_id, std::uint8_t flags) override {
    Required(actor_id).commandFlags = flags;
  }
  void SetCommandArgs(int actor_id, const CommandArgs& args) override {
    Battle_Actor& actor = Required(actor_id);
    actor.commandArg0 = args[0];
    actor.commandArg1.word = args[1];
    actor.commandArg2 = args[2];
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
    return static_cast<int>(Field_controlMode);
  }
  [[nodiscard]] CommandPlacement Placement(int actor_id) override {
    return PlacementOf(actor_id);
  }

  void ReportUnknownWaitingOpcode(int actor_id, int opcode) override {
    Core_Debugf("actors.commands", "Actor %d waits on unknown opcode %d",
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
    Effects_TintActionActorRed(actor_id, 0);
  }
  void RestoreTint() override { Effects_RestoreActionActorTint(); }
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
    const Battle_ActorStats& stats = *Required(actor_id).stats;
    if (limb >= std::size(stats.limbActions) ||
        slot >= std::size(stats.limbActions[0])) {
      Core_Debugf("actors.commands",
                  "Actor %d limb action [%u][%u] is outside its table",
                  actor_id, limb, slot);
      return std::nullopt;
    }
    return stats.limbActions[limb][slot].actionId;
  }
  bool BeginActionSelection(int actor_id, std::uint16_t action) override {
    VS::Battle::LiveBeginActorActionSelection(Required(actor_id).id, action);
    return Battle_actionState->selectedPreview == kNoPreviewSelected;
  }

 private:
  static Battle_Actor* Actor(int actor_id) {
    if (actor_id < 0 ||
        static_cast<std::size_t>(actor_id) >= std::size(Battle_actors)) {
      return nullptr;
    }
    return Battle_actors[actor_id];
  }
  static Battle_Actor& Required(int actor_id) {
    Battle_Actor* actor = Actor(actor_id);
    if (actor == nullptr) {
      Core_Debugf("actors.commands", "Command requires battle actor %d",
                  actor_id);
      std::abort();
    }
    return *actor;
  }
};

class LiveAshleyHost final : public AshleyMovementHost {
 public:
  void ConstrainCameraToPlayer() override {
    Actors_ActorLocation location{};
    static_cast<void>(LiveActorPlacement(0, &location));
    VS::Field::LiveUpdateCameraTileConstraint(0, &location.packedTile.target);
  }
  [[nodiscard]] CommandActorGait PlayerGait() const override {
    return GaitOf(*Battle_characterState->stats);
  }
  [[nodiscard]] std::uint32_t PlayerStateFlags() const override {
    return LiveActorStateFlags(0);
  }
  [[nodiscard]] unsigned ScreenTransitionStep() const override {
    return Field_screenTransitionStep;
  }
  [[nodiscard]] std::optional<int> DoorEntryElevation() const override {
    if (Field_pendingDestinationDoor.reserved06Bit15 == 0) {
      return std::nullopt;
    }
    return static_cast<int>(Field_pendingDestinationDoor.entryElevation);
  }
  void UpdateCameraOrbit() override { VS::Field::LiveUpdateCameraOrbitInput(); }
  [[nodiscard]] AshleyControls Controls() const override {
    const Input_ControllerPortState& port = Input_controllerPorts[0];
    const auto held = static_cast<std::uint32_t>(Input_previousButtons);
    const auto has = [held](std::uint32_t button) {
      return (held & button) != 0;
    };
    AshleyControls controls{
        .stick = std::nullopt,
        .held = {.up = has(PADLup),
                 .down = has(PADLdown),
                 .left = has(PADLleft),
                 .right = has(PADLright)},
        .walk_held = has(PADR2),
        .jump_pressed = (Input_buttonsPressed.allButtons & PADRleft) != 0};
    if (static_cast<std::uint8_t>(port.controllerType) == kAnalogController) {
      // The stick bytes are unsigned (the original builds with unsigned
      // char).
      controls.stick =
          AshleyStick{.x = static_cast<std::uint8_t>(port.rightStickX),
                      .y = static_cast<std::uint8_t>(port.rightStickY)};
    }
    return controls;
  }
  [[nodiscard]] int CameraDistance() const override {
    return Field_sphericalCamera.values.distance;
  }
  void SetCameraDistance(int distance) override {
    Field_sphericalCamera.values.distance = distance;
  }
  [[nodiscard]] int CameraYaw() const override {
    return Field_sphericalCamera.values.yaw;
  }
  void SetMoveInput(AshleyMoveInput input) override {
    Actors_ashleyMoveInput[kMoveInputLateral] = input.lateral;
    Actors_ashleyMoveInput[kMoveInputForward] = input.forward;
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
