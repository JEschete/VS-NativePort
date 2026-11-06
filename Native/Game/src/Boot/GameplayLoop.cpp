// Hand-maintained (tools/port/owned.csv).
#include "Boot/GameplayLoop.hpp"

#include <array>
#include <cstdint>

#include "Input/Pad.hpp"

namespace VS::Boot {
namespace {
using Input::Button;

constexpr int kAttractDemoEntry = 3;
constexpr int kAttractDemoAltEntry = 4;
constexpr std::uint32_t kShortcutHoldButton = Button::L2;
constexpr std::uint32_t kFirstPersonButtons = Button::R3 | Button::Start;
constexpr int kOpaqueAlpha = 255;

struct ShortcutBinding {
  std::uint32_t button;
  int shortcut;
};

// Checked in this order while L2 is held (146C.c:7558, control mode 6).
constexpr std::array<ShortcutBinding, 8> kShortcutBindings{{
    {Button::Circle, 1},
    {Button::Triangle, 2},
    {Button::Square, 3},
    {Button::Cross, 4},
    {Button::Up, 5},
    {Button::Down, 6},
    {Button::Right, 7},
    {Button::Left, 8},
}};

bool IsAttractDemoEntry(int entry_mode) {
  return entry_mode == kAttractDemoEntry || entry_mode == kAttractDemoAltEntry;
}

bool IsTransitionFrame(ScreenTransitionStep step) {
  return step == ScreenTransitionStep::kRoomExit ||
         step == ScreenTransitionStep::kZoneExit ||
         step == ScreenTransitionStep::kRoomEntryReveal;
}

bool IsPressed(const GameplayLoopHost& host, std::uint32_t buttons) {
  return (host.ButtonsPressed() & buttons) != 0;
}

// Attract demos (entry modes 3 and 4) return to title on any face, shoulder,
// Select or Start press.
bool IsDemoCancelPressed(const GameplayLoopHost& host) {
  constexpr std::uint32_t kCancelButtons =
      Button::L2 | Button::R2 | Button::L1 | Button::R1 | Button::Triangle |
      Button::Circle | Button::Cross | Button::Square | Button::Select |
      Button::Start;
  return IsPressed(host, kCancelButtons);
}

bool IsShortcutHeld(const GameplayLoopHost& host) {
  return (host.PreviousButtons() & kShortcutHoldButton) != 0;
}

bool IsAshleyReady(const GameplayLoopHost& host, std::uint32_t status) {
  return host.AshleyActionRecoveryTimer() == 0 &&
         (status & actor_state_flag::kMoving) == 0;
}

void HandleRoomActorOutcome(GameplayLoopHost& host, RoomActorOutcome outcome) {
  switch (outcome) {
    case RoomActorOutcome::kBeginCutscene:
      host.BeginQueuedCutscene();
      break;
    case RoomActorOutcome::kOpenEnemyLoot:
      host.SetRecoveryRewardMode(kNoRecoveryReward);
      host.OpenPendingEnemyLoot();
      break;
    case RoomActorOutcome::kNone:
      break;
  }
}

// The menu part of exploration; true when the frame's dispatch is finished.
bool UpdateExplorationMenus(GameplayLoopHost& host, std::uint32_t status) {
  if (IsAshleyReady(host, status) && !host.IsMenuRequestQueued() &&
      IsShortcutHeld(host)) {
    host.BeginShortcutMenu();
    return true;
  }
  if (host.IsMenuRequestQueued()) {
    if (IsAshleyReady(host, status) && host.OpenGameMenu()) {
      host.BeginBattleActionMenu();
      host.SetMenuRequestQueued(false);
      return true;
    }
  } else if (IsPressed(host, Button::Triangle) && !IsShortcutHeld(host)) {
    if (IsAshleyReady(host, status)) {
      if (host.OpenGameMenu()) {
        host.BeginBattleActionMenu();
        return true;
      }
    } else {
      host.SetMenuRequestQueued(true);
    }
  }
  return false;
}

// Cross against what Ashley faces; true when the frame's dispatch is finished.
bool UpdateExplorationInteraction(GameplayLoopHost& host,
                                  std::uint32_t status) {
  if (!IsPressed(host, Button::Cross)) {
    return false;
  }
  if (host.FindInteractableGimmickAhead(kAshleySlot) >= 0) {
    if (!host.IsAshleyWeaponDrawn()) {
      host.BeginGimmickInteraction(kAshleySlot);
      return true;
    }
    return false;
  }
  if ((status & actor_state_flag::kInteractionBlocking) != 0) {
    return false;
  }
  const int actor = host.FindNearestActorAhead(
      kAshleySlot, kInteractionReachHorizontal, kInteractionReachVertical);
  if (actor >= 0 &&
      host.ActorInteractionFlags(actor) == kEventInteractionActor &&
      host.EvaluateActorRoomEventTrigger(actor)) {
    host.BeginQueuedCutscene();
    return true;
  }
  if (host.TryTriggerFacingTileEvent(kAshleySlot)) {
    host.BeginQueuedCutscene();
    return true;
  }
  if (host.FindFacingRoomObject(kAshleySlot)) {
    if (!host.InteractWithFacingRoomObject()) {
      host.BeginObjectInteractionWait();
      return true;
    }
  } else if (host.FindFacingUnlootedChest(kAshleySlot)) {
    host.BeginFacingChestInteraction(kAshleySlot);
    return true;
  }
  return false;
}

// View and weapon commands while standing; true when dispatch is finished.
bool UpdateExplorationStance(GameplayLoopState& state, GameplayLoopHost& host,
                             std::uint32_t status) {
  if ((status & actor_state_flag::kMoving) != 0) {
    return false;
  }
  const bool posed = (status & actor_state_flag::kPosedOrLocomoting) != 0;
  if (!posed && IsPressed(host, kFirstPersonButtons)) {
    host.BeginFirstPersonView();
    return true;
  }
  if (IsPressed(host, Button::Circle)) {
    if (host.IsAshleyWeaponDrawn()) {
      if (host.AshleyActionRecoveryTimer() == 0 &&
          (host.AshleyStatuses() & kParalysisStatus) == 0) {
        host.PlayCenteredSound(kFieldSoundChannelGroup, kAttackStartSound);
        host.BeginActorActionSelection(kAshleySlot, kBasicAttackAction);
        state.skip_camera_update = true;
        return true;
      }
      host.PlayCenteredSound(kFieldSoundChannelGroup, kActionRefusedSound);
    } else if (!posed) {
      host.BeginAshleyWeaponToggle();
      return true;
    }
  }
  if (IsPressed(host, Button::Cross) && host.IsAshleyWeaponDrawn()) {
    host.BeginAshleyWeaponToggle();
    return true;
  }
  return false;
}

void UpdateExploration(GameplayLoopState& state, GameplayLoopHost& host,
                       std::uint32_t status) {
  if ((status & actor_state_flag::kAnimationLocked) != 0) {
    return;
  }
  if (host.EvaluateRoomEventTriggers(kFieldEventTriggerRequest) ==
      kEventTriggerBegins) {
    host.BeginQueuedCutscene();
    return;
  }
  if (host.CheckAshleyTileTriggers()) {
    return;
  }
  if (UpdateExplorationMenus(host, status) ||
      UpdateExplorationInteraction(host, status) ||
      UpdateExplorationStance(state, host, status)) {
    return;
  }
  host.UpdateAshleyMovementInput();
  HandleRoomActorOutcome(host, host.ExecuteQueuedActorCommands());
}

bool AreBattleActorsSettled(GameplayLoopHost& host) {
  for (int slot = 0; slot < kBattleActorSlotCount; ++slot) {
    if (host.HasBattleActor(slot) &&
        (host.ActorStateFlags(slot) & actor_state_flag::kCutsceneUnsettled) !=
            0) {
      return false;
    }
  }
  return true;
}

void UpdateCutscene(GameplayLoopHost& host) {
  constexpr int kOrbitDeltaMode = 1;
  constexpr int kOrbitDeltaAltMode = 3;
  if (host.ActionFlowMode() == kCutscenePreparingFlow) {
    host.UpdateCameraOrbitInput();
    host.UpdateRoomActorMovement();
    host.SetCameraPositionFromAngles();
    if (host.CameraYawDelta() != 0) {
      return;
    }
    const int delta_mode = host.CameraDeltaMode();
    if ((delta_mode == kOrbitDeltaMode || delta_mode == kOrbitDeltaAltMode) &&
        AreBattleActorsSettled(host)) {
      host.StartPreparedCutscene();
    }
    return;
  }
  host.UpdateGimImages();
  static_cast<void>(host.ExecuteQueuedActorCommands());
  if (host.EvaluateRoomEventTriggers(kEventPlaybackStatusRequest) ==
      kEventPlaybackIdle) {
    if (IsAttractDemoEntry(host.GameEntryMode())) {
      host.ResetToTitle();
    } else {
      host.EndFieldCutscene();
    }
  }
}

void UpdateStandaloneEffect(GameplayLoopHost& host) {
  if (host.CanStartEffectTrack()) {
    host.StartEffectTrack();
  }
  if ((host.EffectBusyFlags() & kStandaloneEffectBusyMask) == 0) {
    host.EndAshleyStandaloneEffect();
  }
}

void UpdateActionMenu(GameplayLoopHost& host) {
  if (host.ActionFlowMode() != kActionMenuOpenFlow) {
    return;
  }
  host.UpdateMenuOverlay();
  if (host.SelectActionMenu() == kActionMenuConfirmed) {
    host.DispatchActionMenuResult();
  }
}

void UpdateShortcutMenu(GameplayLoopHost& host) {
  if (!IsShortcutHeld(host)) {
    host.EndShortcutMenu();
    return;
  }
  host.ShowShortcutHud();
  for (const ShortcutBinding& binding : kShortcutBindings) {
    if (IsPressed(host, binding.button)) {
      if (host.OpenShortcutMenu(binding.shortcut)) {
        host.BeginBattleActionMenu();
      }
      return;
    }
  }
}

void UpdateFirstPersonView(GameplayLoopHost& host) {
  switch (static_cast<FirstPersonViewFlow>(host.ActionFlowMode())) {
    case FirstPersonViewFlow::kEntering:
      host.UpdateFirstPersonViewEntry();
      break;
    case FirstPersonViewFlow::kActive:
      host.CheckFirstPersonViewExit();
      host.UpdateFirstPersonCamera();
      break;
    case FirstPersonViewFlow::kExiting:
      host.UpdateFirstPersonViewExit();
      if (IsPressed(host, kFirstPersonButtons)) {
        host.BeginFirstPersonView();
      }
      break;
  }
}

void UpdateTrapInteraction(GameplayLoopHost& host) {
  switch (static_cast<InteractionFlow>(host.ActionFlowMode())) {
    case InteractionFlow::kCameraFocus:
      if (!host.StepTrapCameraFocus()) {
        host.DispatchTrapOrChestInteraction();
      }
      break;
    case InteractionFlow::kTrapEffectOrChestOpening:
      if (!host.IsActionEffectBusy()) {
        host.FinishTrapInteraction();
      }
      break;
    case InteractionFlow::kReturnCamera:
      static_cast<void>(host.TrackInteractionCameraToAshley());
      if (host.IsCameraLookAtOnAshley()) {
        host.RebuildActorActionPlans();
        host.BeginPostActionRecovery(kNoRecoveryReward);
        host.SetControlMode(FieldControlMode::kBattleAction);
      }
      break;
    case InteractionFlow::kChestContents:
      break;
  }
  host.SetCameraPositionFromAngles();
}

void UpdateChestInteraction(GameplayLoopHost& host) {
  switch (static_cast<InteractionFlow>(host.ActionFlowMode())) {
    case InteractionFlow::kCameraFocus:
      if (!host.WaitForChestCameraFocus()) {
        host.DispatchTrapOrChestInteraction();
      }
      break;
    case InteractionFlow::kTrapEffectOrChestOpening:
      if (host.IsChestAnimationReady()) {
        host.OpenChestContents();
      }
      break;
    case InteractionFlow::kChestContents:
      if (!host.UpdateChestContentsSelection()) {
        host.FinishChestInteraction();
      }
      break;
    case InteractionFlow::kReturnCamera: {
      const bool returned = host.TrackInteractionCameraToAshley();
      if (host.IsCameraLookAtOnAshley() && returned) {
        host.ReleaseSceneNotification();
        host.BeginPostActionRecovery(kNoRecoveryReward);
        host.SetControlMode(FieldControlMode::kBattleAction);
      }
      break;
    }
  }
  host.SetCameraPositionFromAngles();
}

void UpdateWeaponToggle(GameplayLoopHost& host) {
  host.StopAshleyDirectMovement();
  if (!host.IsAshleyActionAnimationBusy()) {
    host.EndAshleyWeaponToggle();
  }
}

void DispatchControlMode(GameplayLoopState& state, GameplayLoopHost& host,
                         std::uint32_t status) {
  switch (host.ControlMode()) {
    case FieldControlMode::kNone:
      break;
    case FieldControlMode::kExploration:
      UpdateExploration(state, host, status);
      break;
    case FieldControlMode::kBattleAction:
      host.TrackActionCameraTarget(kAshleySlot);
      break;
    case FieldControlMode::kCutscene:
      UpdateCutscene(host);
      break;
    case FieldControlMode::kStandaloneEffect:
      UpdateStandaloneEffect(host);
      break;
    case FieldControlMode::kActionMenu:
      UpdateActionMenu(host);
      break;
    case FieldControlMode::kShortcutMenu:
      UpdateShortcutMenu(host);
      break;
    case FieldControlMode::kFirstPersonView:
      UpdateFirstPersonView(host);
      break;
    case FieldControlMode::kTrapInteraction:
      UpdateTrapInteraction(host);
      break;
    case FieldControlMode::kChestInteraction:
      UpdateChestInteraction(host);
      break;
    case FieldControlMode::kWeaponToggle:
      UpdateWeaponToggle(host);
      break;
    case FieldControlMode::kObjectInteractionWait:
      if (!host.IsSceneNotificationPending()) {
        host.EndObjectInteractionWait();
      }
      break;
    case FieldControlMode::kGimmickMotion:
      if (!host.AreGimmicksMoving()) {
        host.EnterFieldExplorationMode();
      }
      break;
  }
}

bool IsBattleCameraHeld(std::int32_t flow) {
  return flow >= static_cast<std::int32_t>(BattleActionFlow::kAbilityTiming) &&
         flow <=
             static_cast<std::int32_t>(BattleActionFlow::kWaitTargetRecovery);
}

void UpdateFollowCamera(GameplayLoopHost& host) {
  switch (host.ControlMode()) {
    case FieldControlMode::kExploration:
    case FieldControlMode::kWeaponToggle:
    case FieldControlMode::kGimmickMotion:
      host.UpdateAshleyFollowCamera();
      break;
    case FieldControlMode::kBattleAction:
      if (!IsBattleCameraHeld(host.ActionFlowMode())) {
        host.SetCameraPositionFromAngles();
      }
      break;
    default:
      break;
  }
}

void UpdateBattleAction(GameplayLoopHost& host) {
  host.UpdateTargetingRiskPenalty();
  const std::int32_t flow = host.ActionFlowMode();
  if (flow >= static_cast<std::int32_t>(
                  BattleActionFlow::kInitialTargetSelection) &&
      flow <=
          static_cast<std::int32_t>(BattleActionFlow::kAbilityUnlockResults)) {
    host.RunBattleActionFlow(static_cast<BattleActionFlow>(flow));
  }
  const std::int32_t drawn_flow = host.ActionFlowMode();
  if (drawn_flow < static_cast<std::int32_t>(BattleActionFlow::kCameraZoom) &&
      drawn_flow != static_cast<std::int32_t>(BattleActionFlow::kIdle)) {
    host.DrawSphere();
  }
}

void UpdateCameraDependents(GameplayLoopHost& host) {
  switch (host.ControlMode()) {
    case FieldControlMode::kNone:
    case FieldControlMode::kExploration:
      if (!IsTransitionFrame(host.TransitionStep())) {
        HandleRoomActorOutcome(host, host.UpdateRoomActorVitals());
      }
      break;
    case FieldControlMode::kBattleAction:
      UpdateBattleAction(host);
      break;
    default:
      break;
  }
}

void DrawFrame(GameplayLoopHost& host) {
  if (host.IsFieldRenderingEnabled()) {
    host.DrawRoomScenery();
    if (host.IsFieldRenderingEnabled()) {
      host.DrawRoomActors();
    }
  }
  host.UpdateEffectRuntime();
  host.DrawBattleHud();
  host.UpdateTextBoxes();
  for (int tick = 0; tick < host.TickSpeed() / 2; ++tick) {
    host.UpdateTickTransitions();
  }
  host.DrawScreenOverlays();
  switch (host.ControlMode()) {
    case FieldControlMode::kExploration:
      host.RunActorAiUpdateStages();
      break;
    case FieldControlMode::kCutscene:
      host.UpdateActorNavigationOverrides();
      break;
    default:
      break;
  }
  host.PresentFrame();
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_exec (src/BATTLE/BATTLE.PRG/146C.c:7558),
// one pass of the innermost do-while.
bool RunGameplayFrame(GameplayLoopState& state, GameplayLoopHost& host) {
  if (!host.PollControllers()) {
    return false;
  }
  host.PollEncounterStop();
  host.BeginFrame();
  if (IsAttractDemoEntry(host.GameEntryMode()) && IsDemoCancelPressed(host)) {
    host.ResetToTitle();
  }
  host.UpdateMovingPlatforms();
  const std::uint32_t status = host.ActorStateFlags(kAshleySlot);
  host.CaptureAshleyPlacement();

  if (!IsTransitionFrame(host.TransitionStep())) {
    state.skip_camera_update = false;
    DispatchControlMode(state, host, status);
  } else if (host.ControlMode() == FieldControlMode::kCutscene) {
    host.UpdateGimImages();
  } else if ((status & actor_state_flag::kAnimationLocked) == 0) {
    host.UpdateAshleyMovementInput();
  }

  host.UpdateEventPlayback();
  UpdateFollowCamera(host);
  host.BuildCameraView();
  if (!state.skip_camera_update) {
    UpdateCameraDependents(host);
  }
  DrawFrame(host);
  return true;
}

bool IsRoomExitComplete(const GameplayLoopHost& host) {
  const ScreenTransitionStep step = host.TransitionStep();
  return (step == ScreenTransitionStep::kRoomExit ||
          step == ScreenTransitionStep::kZoneExit) &&
         host.TransitionAlpha() == kOpaqueAlpha;
}

bool RunGameplayRoom(GameplayLoopState& state, GameplayLoopHost& host) {
  if (!host.FinishPendingRoomLoad()) {
    host.ResetToTitle();
    return false;
  }
  host.SetMenuRequestQueued(false);
  do {
    static_cast<void>(RunGameplayFrame(state, host));
  } while (!IsRoomExitComplete(host));
  host.UnloadCurrentRoom();
  return true;
}

bool RunGameplayZone(GameplayLoopState& state, GameplayLoopHost& host) {
  host.FinishPendingZoneLoad();
  do {
    if (!RunGameplayRoom(state, host)) return false;
  } while (host.TransitionStep() != ScreenTransitionStep::kZoneExit);
  host.ReleaseZoneData();
  return true;
}

void RunGameplayLoop(GameplayLoopState& state, GameplayLoopHost& host) {
  host.SetMenuRequestQueued(false);
  host.InitializeGameplayOverlay();
  while (RunGameplayZone(state, host)) {
  }
}
}  // namespace VS::Boot
