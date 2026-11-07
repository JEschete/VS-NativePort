// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Actors/ActorCommandsLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Actors/MovementCommand.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/TargetingLive.hpp"
#include "Boot/GameplayLoop.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/FrameTiming.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Event/RoomTriggerEntryLive.h"
#include "Event/ScriptVmLive.hpp"
#include "Event/TriggersLive.hpp"
#include "Field/BackgroundLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Field/GameModeLive.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/InteractionsLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Input/ControllerLive.hpp"
#include "Items/LootLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/GimImagesLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "Render/TexturePagesLive.hpp"

extern "C" {
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/EncounterHooks.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/Rewards.h"
#include "Battle/Sphere.h"
#include "Battle/StatusEffects.h"
#include "Battle/TargetingState.h"
#include "Boot/GameLoop.h"
#include "Core/Gte.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Field/CameraLiveState.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Input.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/TransitionsLiveState.h"
#include "Input/ControllerLiveState.h"
#include "Items/TreasureChests.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"
}

namespace VS::Boot {
namespace {
// Ordering-table layout set up by InitializeGameplayOverlay (146C.c:7558).
constexpr int kOrderingTableLength = 0x822;
constexpr std::ptrdiff_t kOverlayOrderingTableOffset = 2;
constexpr std::ptrdiff_t kFieldOrderingTableOffset = 18;
constexpr std::ptrdiff_t kMaskBitSlot = 0x820;
constexpr std::ptrdiff_t kPresentSlot = 0x821;
constexpr std::int32_t kGeometryOffsetX = 160;
constexpr std::int32_t kGeometryOffsetY = 112;
constexpr int kControllerSkippedFrame = -1;

class LiveGameplayLoopHost final : public GameplayLoopHost {
 public:
  void InitializeGameplayOverlay() override {
    InitializeGameplayOverlayLive(Boot_gameEntryMode);
  }
  void FinishPendingZoneLoad() override {
    VS::Field::LiveFinishZoneLoad(
        Field_pendingDestinationDoor.destinationZoneId);
  }
  bool FinishPendingRoomLoad() override {
    return VS::Field::LiveFinishRoomLoad(
        Field_pendingDestinationDoor.destinationRoomId);
  }
  void UnloadCurrentRoom() override { VS::Field::LiveUnloadCurrentRoom(); }
  void ReleaseZoneData() override { VS::Field::LiveReleaseZoneData(); }
  void ResetToTitle() override { ResetToTitleLive(); }

  [[nodiscard]] int GameEntryMode() const override {
    return Boot_gameEntryMode;
  }
  [[nodiscard]] FieldControlMode ControlMode() const override {
    return static_cast<FieldControlMode>(Field_controlMode);
  }
  void SetControlMode(FieldControlMode mode) override {
    Field_controlMode = static_cast<u_int>(mode);
  }
  [[nodiscard]] std::int32_t ActionFlowMode() const override {
    return static_cast<std::int32_t>(Battle_actionFlowMode);
  }
  [[nodiscard]] ScreenTransitionStep TransitionStep() const override {
    return static_cast<ScreenTransitionStep>(Field_screenTransitionStep);
  }
  [[nodiscard]] int TransitionAlpha() const override {
    return Field_screenTransitionAlpha;
  }
  [[nodiscard]] bool IsMenuRequestQueued() const override {
    return Field_isMenuRequestQueued != 0;
  }
  void SetMenuRequestQueued(bool queued) override {
    Field_isMenuRequestQueued = queued ? 1 : 0;
  }
  [[nodiscard]] std::uint32_t ButtonsPressed() const override {
    return Input_buttonsPressed.allButtons;
  }
  [[nodiscard]] std::uint32_t PreviousButtons() const override {
    return static_cast<std::uint32_t>(Input_previousButtons);
  }
  [[nodiscard]] std::uint32_t ActorStateFlags(int slot) override {
    return VS::Actors::LiveActorStateFlags(slot);
  }
  [[nodiscard]] bool HasBattleActor(int slot) const override {
    return BattleActor(slot) != nullptr;
  }
  [[nodiscard]] std::int32_t ActorInteractionFlags(int slot) const override {
    const Battle_Actor* actor = BattleActor(slot);
    return actor != nullptr ? actor->interactionFlags : 0;
  }
  [[nodiscard]] int AshleyActionRecoveryTimer() const override {
    return Battle_characterState->stats->actionRecoveryTimer;
  }
  [[nodiscard]] std::uint32_t AshleyStatuses() const override {
    return static_cast<std::uint32_t>(Battle_characterState->stats->statuses);
  }
  [[nodiscard]] bool IsAshleyWeaponDrawn() const override {
    return (Battle_characterState->weaponDrawn & 1) != 0;
  }
  [[nodiscard]] int CameraYawDelta() const override {
    return Field_sphericalCamera.delta.yaw;
  }
  [[nodiscard]] int CameraDeltaMode() const override {
    return Field_sphericalCamera.delta.mode;
  }
  [[nodiscard]] std::uint32_t EffectBusyFlags() const override {
    return static_cast<std::uint32_t>(Effects_effectBusyFlags);
  }
  [[nodiscard]] bool IsFieldRenderingEnabled() const override {
    return VS::Field::LiveRoomRenderState().field_rendering_enabled;
  }
  [[nodiscard]] int TickSpeed() const override { return State_tickSpeed; }

  bool PollControllers() override {
    return VS::Input::UpdateLiveControllers() != kControllerSkippedFrame;
  }
  void PollEncounterStop() override { Boot_EncounterPollStop(); }
  void BeginFrame() override {
    ordering_table_ =
        static_cast<u_long*>(Render_legacyOrderingTables[Render_frameBuf]);
    Render_legacyCurrentOrderingTable = ordering_table_;
    Render_frameScratch.overlayOrderingTable =
        ordering_table_ + kOverlayOrderingTableOffset;
    Render_frameScratch.orderingTable =
        ordering_table_ + kFieldOrderingTableOffset;
    ClearOTagR(ordering_table_, kOrderingTableLength);
    Render_frameScratch.primitiveCursor =
        Render_legacyPrimitiveBuffers[Render_frameBuf];
    AddPrim(ordering_table_, &Render_legacyMaskBitOffPackets[Render_frameBuf]);
  }
  void UpdateMovingPlatforms() override {
    static_cast<void>(Field_UpdateMovingPlatforms());
  }
  void CaptureAshleyPlacement() override {
    // Left unchanged when Ashley has no record, as the original's stack slot.
    static_cast<void>(
        VS::Actors::LiveActorPlacement(kAshleySlot, &ashley_placement_));
  }

  std::int32_t EvaluateRoomEventTriggers(std::int32_t request) override {
    return Event_EvaluateRoomEventTriggersEntry(request, 0, nullptr, 0);
  }
  bool EvaluateActorRoomEventTrigger(int slot) override {
    return VS::Event::EvaluateLiveActorRoomEventTrigger(slot);
  }
  bool CheckAshleyTileTriggers() override {
    return VS::Field::CheckAshleyTileTriggersLive(kAshleySlot,
                                                  ashley_placement_.location);
  }
  bool TryTriggerFacingTileEvent(int slot) override {
    return Field_TryTriggerFacingTileEvent(slot) != 0;
  }
  void BeginQueuedCutscene() override { Event::BeginLiveQueuedCutscene(); }
  void StartPreparedCutscene() override { Event::StartLivePreparedCutscene(); }
  void EndFieldCutscene() override { Event::EndLiveFieldCutscene(); }
  void UpdateEventPlayback() override {
    static_cast<void>(Event::UpdateLiveEventPlayback());
  }

  void UpdateAshleyMovementInput() override {
    VS::Actors::UpdateLiveAshleyMovementInput();
  }
  RoomActorOutcome ExecuteQueuedActorCommands() override {
    return static_cast<RoomActorOutcome>(
        VS::Actors::ExecuteLiveQueuedActorCommands());
  }
  RoomActorOutcome UpdateRoomActorVitals() override {
    return static_cast<RoomActorOutcome>(Battle_UpdateRoomActorVitals());
  }
  void SetRecoveryRewardMode(int recovery_reward_mode) override {
    Battle_recoveryRewardMode = recovery_reward_mode;
  }
  void OpenPendingEnemyLoot() override {
    VS::Items::OpenLivePendingEnemyLoot();
  }
  void EnterFieldExplorationMode() override {
    VS::Field::LiveEnterFieldExplorationMode();
  }
  int FindInteractableGimmickAhead(int slot) override {
    return Field_FindInteractableGimmickAhead(slot);
  }
  void BeginGimmickInteraction(int slot) override {
    static_cast<void>(Field_BeginGimmickInteraction(slot));
  }
  [[nodiscard]] bool AreGimmicksMoving() override {
    return Field_AreGimmicksMoving() != 0;
  }
  int FindNearestActorAhead(int slot, int horizontal_reach,
                            int vertical_reach) override {
    return VS::Actors::FindLiveNearestActorAhead(slot, horizontal_reach,
                                                 vertical_reach);
  }
  bool FindFacingRoomObject(int slot) override {
    facing_room_object_ = Field_FindFacingRoomObject(slot);
    return facing_room_object_ != nullptr;
  }
  bool InteractWithFacingRoomObject() override {
    return facing_room_object_ == nullptr ||
           Field_InteractWithLockedRoomObject(facing_room_object_) != 0;
  }
  bool FindFacingUnlootedChest(int slot) override {
    facing_chest_ = Field_FindFacingUnlootedChest(slot);
    return facing_chest_ != nullptr;
  }
  void BeginFacingChestInteraction(int slot) override {
    if (facing_chest_ == nullptr) {
      return;
    }
    const auto target = VS::Field::FindLiveChestTarget(facing_chest_);
    if (target) {
      static_cast<void>(VS::Field::BeginLiveChestInteraction(*target, slot));
    }
  }
  void BeginObjectInteractionWait() override {
    Field_BeginObjectInteractionWaitEntry();
  }
  void EndObjectInteractionWait() override {
    Field_EndObjectInteractionWaitEntry();
  }
  [[nodiscard]] bool IsSceneNotificationPending() override {
    return Menu_IsSceneNotificationPending() != 0;
  }
  void ReleaseSceneNotification() override { Menu_ReleaseSceneNotification(); }
  void PlayCenteredSound(int channel_group, int sound) override {
    VS::Audio::LivePlaySfxCentered(channel_group, sound);
  }

  void BeginShortcutMenu() override { VS::Field::LiveBeginShortcutMenu(); }
  void EndShortcutMenu() override { VS::Field::LiveEndShortcutMenu(); }
  void ShowShortcutHud() override { Battle_ShowShortcutHud(); }
  bool OpenShortcutMenu(int shortcut) override {
    return VS::Menu::Framework::OpenLiveShortcutMenu(shortcut) != 0;
  }
  bool OpenGameMenu() override {
    return VS::Menu::Framework::OpenLiveGameMenu(0, 0) != 0;
  }
  void BeginBattleActionMenu() override {
    VS::Field::LiveBeginBattleActionMenu(0, 0);
  }
  void UpdateMenuOverlay() override {
    VS::Menu::Framework::UpdateLiveMenuOverlay();
  }
  int SelectActionMenu() override {
    action_menu_ = VS::Menu::Framework::LiveActionSelectionState();
    Battle_actionSelection = action_menu_;
    return action_menu_->menuStatus;
  }
  void DispatchActionMenuResult() override {
    VS::Field::LiveDispatchBattleActionMenuResult(action_menu_);
  }

  void TrackActionCameraTarget(int slot) override {
    VS::Field::LiveTrackActionCameraTarget(slot);
  }
  void UpdateCameraOrbitInput() override {
    VS::Field::LiveUpdateCameraOrbitInput();
  }
  void UpdateRoomActorMovement() override {
    VS::Actors::UpdateLiveRoomActorMovement();
  }
  void SetCameraPositionFromAngles() override {
    VS::Field::LiveSetCameraPositionFromAngles(&Field_sphericalCamera.values);
  }
  void UpdateAshleyFollowCamera() override {
    static_cast<void>(VS::Field::LiveUpdateAshleyFollowCamera());
  }
  void BuildCameraView() override {
    VS::Field::LiveUpdateCameraFacingState();
    VS::Render::BuildLiveCameraViewMatrix();
    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
  }
  void BeginFirstPersonView() override {
    VS::Field::LiveBeginFirstPersonView();
  }
  void UpdateFirstPersonViewEntry() override {
    VS::Field::LiveUpdateFirstPersonViewEntry();
  }
  void CheckFirstPersonViewExit() override {
    VS::Field::LiveCheckFirstPersonViewExit();
  }
  void UpdateFirstPersonCamera() override {
    VS::Field::LiveUpdateFirstPersonCamera();
  }
  void UpdateFirstPersonViewExit() override {
    VS::Field::LiveUpdateFirstPersonViewExit();
  }
  bool TrackInteractionCameraToAshley() override {
    return Field_TrackInteractionCameraToAshley() != 0;
  }
  [[nodiscard]] bool IsCameraLookAtOnAshley() override {
    return VS::Field::LiveIsCameraLookAtOnAshley() != 0;
  }

  bool StepTrapCameraFocus() override {
    return Field_StepTrapCameraFocus() != 0;
  }
  bool WaitForChestCameraFocus() override {
    return Field_WaitForChestCameraFocus() != 0;
  }
  void DispatchTrapOrChestInteraction() override {
    Field_DispatchTrapOrChestInteractionEntry();
  }
  [[nodiscard]] bool IsActionEffectBusy() override {
    return Effects_ActionEffectBusyFlags() != 0;
  }
  void FinishTrapInteraction() override { Field_FinishTrapInteractionEntry(); }
  [[nodiscard]] bool IsChestAnimationReady() override {
    return Field_IsChestAnimationReady() != 0;
  }
  void OpenChestContents() override { Field_OpenChestContentsEntry(); }
  bool UpdateChestContentsSelection() override {
    return Field_UpdateChestContentsSelectionEntry() != 0;
  }
  void FinishChestInteraction() override {
    Field_FinishChestInteractionEntry();
  }
  void RebuildActorActionPlans() override {
    static_cast<void>(VS::Battle::Ai::RebuildLiveActorActionPlans(
        &Battle_actionState->queue[0]));
  }
  void BeginPostActionRecovery(int recovery_reward_mode) override {
    Battle_BeginPostActionRecovery(recovery_reward_mode);
  }

  [[nodiscard]] bool CanStartEffectTrack() override {
    return Effects_CanStartEffectTrack() != 0;
  }
  void StartEffectTrack() override {
    static_cast<void>(Effects_StartEffectTrack(0));
  }
  void EndAshleyStandaloneEffect() override {
    Effects_EndAshleyStandaloneEffect();
  }
  void BeginActorActionSelection(int slot, int action) override {
    VS::Battle::LiveBeginActorActionSelection(slot,
                                              static_cast<u_short>(action));
  }
  void BeginAshleyWeaponToggle() override { Battle_BeginAshleyWeaponToggle(); }
  void StopAshleyDirectMovement() override {
    VS::Actors::SetDirectMovement(VS::Actors::ActorRecordMovementRequests(),
                                  kAshleySlot, 0, 0);
  }
  [[nodiscard]] bool IsAshleyActionAnimationBusy() override {
    return VS::Actors::IsLiveActorActionAnimationBusy(kAshleySlot) != 0;
  }
  void EndAshleyWeaponToggle() override { Battle_EndAshleyWeaponToggle(); }
  void UpdateGimImages() override { VS::Render::UpdateLiveGimImages(); }

  void UpdateTargetingRiskPenalty() override {
    Battle_UpdateTargetingRiskPenalty();
  }
  void RunBattleActionFlow(BattleActionFlow flow) override {
    switch (flow) {
      case BattleActionFlow::kInitialTargetSelection:
        Battle_UpdateInitialTargetSelection();
        break;
      case BattleActionFlow::kAreaSelection:
        Battle_UpdateActionAreaSelection();
        break;
      case BattleActionFlow::kFinalTargetSelection:
        Battle_UpdateFinalTargetSelection();
        break;
      case BattleActionFlow::kCameraZoom:
        Battle_UpdateActionCameraZoom();
        break;
      case BattleActionFlow::kWaitEffectReady:
        Battle_WaitForActionEffectReady();
        break;
      case BattleActionFlow::kStartReadyAnimation:
        Battle_StartReadyActionAnimation();
        break;
      case BattleActionFlow::kAbilityTiming:
        VS::Battle::LiveUpdateBattleAbilityTiming();
        break;
      case BattleActionFlow::kFollowupEffects:
        Battle_StartFollowupActionEffects();
        break;
      case BattleActionFlow::kWaitResolution:
        Battle_WaitForActionResolution();
        break;
      case BattleActionFlow::kWaitTargetRecovery:
        Battle_WaitForTargetRecovery();
        break;
      case BattleActionFlow::kFinishSequence:
        Battle_FinishActionSequence();
        break;
      case BattleActionFlow::kLootResults:
        Battle_UpdateLootResults();
        break;
      case BattleActionFlow::kAbilityUnlockDelay:
        Battle_UpdateAbilityUnlockDelay();
        break;
      case BattleActionFlow::kAbilityUnlockResults:
        Battle_UpdateAbilityUnlockResults();
        break;
      case BattleActionFlow::kIdle:
        break;
    }
  }
  void DrawSphere() override { Battle_DrawSphere(); }

  void DrawRoomScenery() override {
    VS::Field::LiveDrawRoomBackground(nullptr);
    VS::Field::LiveDrawRoomScene();
    VS::Field::LiveAdvanceRoomAnimationClock();
    Core_Gte_SetGeomOffset(kGeometryOffsetX, kGeometryOffsetY);
  }
  void DrawRoomActors() override {
    VS::Actors::UpdateAndDrawLiveRoomActors(&Render_frameScratch.viewMatrix);
    static_cast<void>(VS::Content::UpdateLiveActorAssetQueue());
    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    Battle_UpdateAndDrawFloatingLabels();
  }
  void UpdateEffectRuntime() override {
    Effects_effectBusyFlags = Effects_UpdateEffectRuntime();
  }
  void DrawBattleHud() override { Battle_DrawBattleHud(); }
  void UpdateTextBoxes() override { Menu_UpdateTextBoxes(); }
  void UpdateTickTransitions() override {
    VS::Input::UpdateLiveVibrationPatterns();
    VS::Render::UpdateLiveBackgroundGradientTransition();
    VS::Render::UpdateLiveModelTintTransitions();
    VS::Render::UpdateLiveClutTransitions();
  }
  void DrawScreenOverlays() override {
    VS::Render::DrawLiveScreenTransition(ordering_table_);
    VS::Render::DrawLiveFramebufferEffect(ordering_table_);
  }
  void RunActorAiUpdateStages() override {
    VS::Battle::Ai::RunLiveActorAiUpdateStages();
  }
  void UpdateActorNavigationOverrides() override {
    VS::Battle::Ai::UpdateLiveActorNavigationOverrides();
  }
  void PresentFrame() override {
    DR_STP* mask_bit = &Render_legacyMaskBitOnPackets[Render_frameBuf];
    SetDrawStp(mask_bit, VS::Render::IsLiveFramebufferEffectShowing() ? 1 : 0);
    AddPrim(ordering_table_ + kMaskBitSlot, mask_bit);
    static_cast<void>(VS::Core::ElapsedFrameCounterTicks());
    static_cast<void>(
        VS::Render::PresentLiveFieldFrame(ordering_table_ + kPresentSlot));
    VS::Render::AdvanceLiveTexturePageRetirements();
  }

 private:
  [[nodiscard]] static Battle_Actor* BattleActor(int slot) {
    if (slot < 0 ||
        static_cast<std::size_t>(slot) >= std::size(Battle_actors)) {
      return nullptr;
    }
    return Battle_actors[slot];
  }

  u_long* ordering_table_ = nullptr;
  Actors_ActorLocationQuery ashley_placement_{};
  Field_MpdLockTrigger* facing_room_object_ = nullptr;
  Items_TreasureChest* facing_chest_ = nullptr;
  Menu_Framework_ActionSelection* action_menu_ = nullptr;
};
}  // namespace

// decomp: BATTLE.PRG vs_battle_exec (src/BATTLE/BATTLE.PRG/146C.c:7558).
// Session transitions leave it by a nonlocal jump, so the loop state is
// trivially destructible and fresh on each entry.
void RunGameplayLoopLive() {
  static LiveGameplayLoopHost host;
  GameplayLoopState state;
  RunGameplayLoop(state, host);
}
}  // namespace VS::Boot
