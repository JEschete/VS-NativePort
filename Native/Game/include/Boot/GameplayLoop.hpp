#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Field/GameMode.hpp"
#include "Field/Transitions.hpp"

// The gameplay frame loop: BATTLE.PRG vs_battle_exec (146C.c:7558). The core
// decides, each frame, which field or battle mode runs and in which order; the
// host performs every effect, so modes can be replaced or extended without
// reaching overlay memory, ordering tables or actor tables directly.
namespace VS::Boot {
// The field control modes (Field_controlMode) and screen transition steps
// (Field_screenTransitionStep) are the field rules' own (Field/GameMode.hpp,
// Field/Transitions.hpp).
using FieldControlMode = Field::FieldControlMode;
using ScreenTransitionStep = Field::TransitionStep;

// Battle_actionFlowMode while the control mode is kBattleAction.
enum class BattleActionFlow : std::int32_t {
  kIdle = 0,
  kInitialTargetSelection = 1,
  kAreaSelection = 2,
  kFinalTargetSelection = 3,
  kCameraZoom = 4,
  kWaitEffectReady = 5,
  kStartReadyAnimation = 6,
  kAbilityTiming = 7,
  kFollowupEffects = 8,
  kWaitResolution = 9,
  kWaitTargetRecovery = 10,
  kFinishSequence = 11,
  kLootResults = 12,
  kAbilityUnlockDelay = 13,
  kAbilityUnlockResults = 14,
};

// Battle_actionFlowMode while the control mode is kFirstPersonView.
enum class FirstPersonViewFlow : std::int32_t {
  kEntering = 0,
  kActive = 1,
  kExiting = 2,
};

// Battle_actionFlowMode while a trap or chest interaction runs.
enum class InteractionFlow : std::int32_t {
  kCameraFocus = 1,
  kTrapEffectOrChestOpening = 2,
  kChestContents = 3,
  kReturnCamera = 4,
};

// Battle_actionFlowMode in kCutscene: actors still moving into place.
inline constexpr std::int32_t kCutscenePreparingFlow = 1;
// Battle_actionFlowMode in kActionMenu: the menu is open.
inline constexpr std::int32_t kActionMenuOpenFlow = 1;

// VS::Actors::LiveActorStateFlags bits the loop tests (ActorFlags.hpp).
namespace actor_state_flag {
using Bits = std::uint32_t;
inline constexpr Bits kAnimationLocked = 0x2000;
inline constexpr Bits kMoving = 0x4000;
inline constexpr Bits kSupportLayerMask = 0x70000;
inline constexpr Bits kActionPose = 0x80000;
inline constexpr Bits kLocomotion = 0x100000;
inline constexpr Bits kMotionLocked = 0x200000;
inline constexpr Bits kWalking = 0x20000000;

// Posed or in locomotion: no first-person view or weapon drawing.
inline constexpr Bits kPosedOrLocomoting = kActionPose | kLocomotion;
inline constexpr Bits kSupportOrMoving = kSupportLayerMask | kMoving;
inline constexpr Bits kLockedOrPosed = kMotionLocked | kPosedOrLocomoting;
// Movement, support, pose or motion lock refuses a Cross interaction (the
// original's 0x3F4000 mask).
inline constexpr Bits kInteractionBlocking = kSupportOrMoving | kLockedOrPosed;
// Still settling before a prepared cutscene may start.
inline constexpr Bits kCutsceneUnsettled = kWalking | kMoving;
}  // namespace actor_state_flag

// Battle actor status bit (146C.h status list).
inline constexpr std::uint32_t kParalysisStatus = 0x2000;

// Event_EvaluateRoomEventTriggers requests and results.
inline constexpr std::int32_t kFieldEventTriggerRequest = 0x20;
inline constexpr std::int32_t kEventPlaybackStatusRequest = 0xFF;
inline constexpr std::int32_t kEventTriggerBegins = 1;
inline constexpr std::int32_t kEventPlaybackIdle = 0;

// ExecuteLiveQueuedActorCommands and UpdateLiveRoomActorVitals results.
enum class RoomActorOutcome : std::int32_t {
  kNone = 0,
  kBeginCutscene = 1,
  kOpenEnemyLoot = 2,
};

// Battle_recoveryRewardMode: no reward, so recovery waits for post-action
// movement (VS::Battle::LiveBeginPendingAbilityReward).
inline constexpr int kNoRecoveryReward = 0;

inline constexpr int kAshleySlot = 0;
inline constexpr int kBattleActorSlotCount = 16;
inline constexpr int kBasicAttackAction = 1;
inline constexpr std::int32_t kEventInteractionActor = 4;
inline constexpr int kInteractionReachHorizontal = 0xA0;
inline constexpr int kInteractionReachVertical = 0x80;
inline constexpr int kActionMenuConfirmed = 2;
inline constexpr std::uint32_t kStandaloneEffectBusyMask = 3;
inline constexpr int kFieldSoundChannelGroup = 0x7E;
inline constexpr int kAttackStartSound = 9;
inline constexpr int kActionRefusedSound = 0xC;

// Persistent loop state. The original keeps skipCameraUpdate in one stack
// slot for the whole function: transition frames leave it untouched, and it
// is read before any write on the first such frame (#286). Here it persists
// the same way but starts false.
struct GameplayLoopState {
  bool skip_camera_update = false;
};

class GameplayLoopHost {
 public:
  GameplayLoopHost() = default;
  GameplayLoopHost(const GameplayLoopHost&) = delete;
  GameplayLoopHost& operator=(const GameplayLoopHost&) = delete;
  GameplayLoopHost(GameplayLoopHost&&) = delete;
  GameplayLoopHost& operator=(GameplayLoopHost&&) = delete;
  virtual ~GameplayLoopHost() = default;

  // Session, zone and room lifecycle.
  virtual void InitializeGameplayOverlay() = 0;
  virtual void FinishPendingZoneLoad() = 0;
  [[nodiscard]] virtual bool FinishPendingRoomLoad() = 0;
  virtual void UnloadCurrentRoom() = 0;
  virtual void ReleaseZoneData() = 0;
  virtual void ResetToTitle() = 0;

  // Frame state queries.
  [[nodiscard]] virtual int GameEntryMode() const = 0;
  [[nodiscard]] virtual FieldControlMode ControlMode() const = 0;
  virtual void SetControlMode(FieldControlMode mode) = 0;
  [[nodiscard]] virtual std::int32_t ActionFlowMode() const = 0;
  [[nodiscard]] virtual ScreenTransitionStep TransitionStep() const = 0;
  [[nodiscard]] virtual int TransitionAlpha() const = 0;
  [[nodiscard]] virtual bool IsMenuRequestQueued() const = 0;
  virtual void SetMenuRequestQueued(bool queued) = 0;
  [[nodiscard]] virtual std::uint32_t ButtonsPressed() const = 0;
  [[nodiscard]] virtual std::uint32_t PreviousButtons() const = 0;
  [[nodiscard]] virtual std::uint32_t ActorStateFlags(int slot) = 0;
  [[nodiscard]] virtual bool HasBattleActor(int slot) const = 0;
  [[nodiscard]] virtual std::int32_t ActorInteractionFlags(int slot) const = 0;
  [[nodiscard]] virtual int AshleyActionRecoveryTimer() const = 0;
  [[nodiscard]] virtual std::uint32_t AshleyStatuses() const = 0;
  [[nodiscard]] virtual bool IsAshleyWeaponDrawn() const = 0;
  [[nodiscard]] virtual int CameraYawDelta() const = 0;
  [[nodiscard]] virtual int CameraDeltaMode() const = 0;
  [[nodiscard]] virtual std::uint32_t EffectBusyFlags() const = 0;
  [[nodiscard]] virtual bool IsFieldRenderingEnabled() const = 0;
  [[nodiscard]] virtual int TickSpeed() const = 0;

  // Frame boundary. PollControllers is false when the frame must be skipped.
  virtual bool PollControllers() = 0;
  virtual void PollEncounterStop() = 0;
  virtual void BeginFrame() = 0;
  virtual void UpdateMovingPlatforms() = 0;
  // Ashley's frame-start placement, used by the tile-trigger check.
  virtual void CaptureAshleyPlacement() = 0;

  // Events.
  virtual std::int32_t EvaluateRoomEventTriggers(std::int32_t request) = 0;
  // True when the actor's interaction event begins.
  virtual bool EvaluateActorRoomEventTrigger(int slot) = 0;
  virtual bool CheckAshleyTileTriggers() = 0;
  virtual bool TryTriggerFacingTileEvent(int slot) = 0;
  virtual void BeginQueuedCutscene() = 0;
  virtual void StartPreparedCutscene() = 0;
  virtual void EndFieldCutscene() = 0;
  virtual void UpdateEventPlayback() = 0;

  // Exploration.
  // func_8006E7F0 always returns 0, so the original's test of its result
  // never ends the exploration frame.
  virtual void UpdateAshleyMovementInput() = 0;
  virtual RoomActorOutcome ExecuteQueuedActorCommands() = 0;
  virtual RoomActorOutcome UpdateRoomActorVitals() = 0;
  virtual void SetRecoveryRewardMode(int recovery_reward_mode) = 0;
  virtual void OpenPendingEnemyLoot() = 0;
  virtual void EnterFieldExplorationMode() = 0;
  virtual int FindInteractableGimmickAhead(int slot) = 0;
  virtual void BeginGimmickInteraction(int slot) = 0;
  [[nodiscard]] virtual bool AreGimmicksMoving() = 0;
  virtual int FindNearestActorAhead(int slot, int horizontal_reach,
                                    int vertical_reach) = 0;
  // The facing room object and chest are kept by the host between the find
  // and the interaction, so the core never holds an MPD record (#287).
  virtual bool FindFacingRoomObject(int slot) = 0;
  virtual bool InteractWithFacingRoomObject() = 0;
  virtual bool FindFacingUnlootedChest(int slot) = 0;
  virtual void BeginFacingChestInteraction(int slot) = 0;
  virtual void BeginObjectInteractionWait() = 0;
  virtual void EndObjectInteractionWait() = 0;
  [[nodiscard]] virtual bool IsSceneNotificationPending() = 0;
  virtual void ReleaseSceneNotification() = 0;
  virtual void PlayCenteredSound(int channel_group, int sound) = 0;

  // Menus.
  virtual void BeginShortcutMenu() = 0;
  virtual void EndShortcutMenu() = 0;
  virtual void ShowShortcutHud() = 0;
  virtual bool OpenShortcutMenu(int shortcut) = 0;
  virtual bool OpenGameMenu() = 0;
  virtual void BeginBattleActionMenu() = 0;
  virtual void UpdateMenuOverlay() = 0;
  // Publishes the action-selection menu and returns its status.
  virtual int SelectActionMenu() = 0;
  virtual void DispatchActionMenuResult() = 0;

  // Camera.
  virtual void TrackActionCameraTarget(int slot) = 0;
  virtual void UpdateCameraOrbitInput() = 0;
  virtual void UpdateRoomActorMovement() = 0;
  virtual void SetCameraPositionFromAngles() = 0;
  virtual void UpdateAshleyFollowCamera() = 0;
  virtual void BuildCameraView() = 0;
  virtual void BeginFirstPersonView() = 0;
  virtual void UpdateFirstPersonViewEntry() = 0;
  virtual void CheckFirstPersonViewExit() = 0;
  virtual void UpdateFirstPersonCamera() = 0;
  virtual void UpdateFirstPersonViewExit() = 0;
  virtual bool TrackInteractionCameraToAshley() = 0;
  [[nodiscard]] virtual bool IsCameraLookAtOnAshley() = 0;

  // Trap and chest interactions.
  virtual bool StepTrapCameraFocus() = 0;
  virtual bool WaitForChestCameraFocus() = 0;
  virtual void DispatchTrapOrChestInteraction() = 0;
  [[nodiscard]] virtual bool IsActionEffectBusy() = 0;
  virtual void FinishTrapInteraction() = 0;
  [[nodiscard]] virtual bool IsChestAnimationReady() = 0;
  virtual void OpenChestContents() = 0;
  virtual bool UpdateChestContentsSelection() = 0;
  virtual void FinishChestInteraction() = 0;
  virtual void RebuildActorActionPlans() = 0;
  virtual void BeginPostActionRecovery(int recovery_reward_mode) = 0;

  // Effects and weapon.
  [[nodiscard]] virtual bool CanStartEffectTrack() = 0;
  virtual void StartEffectTrack() = 0;
  virtual void EndAshleyStandaloneEffect() = 0;
  virtual void BeginActorActionSelection(int slot, int action) = 0;
  virtual void BeginAshleyWeaponToggle() = 0;
  virtual void StopAshleyDirectMovement() = 0;
  [[nodiscard]] virtual bool IsAshleyActionAnimationBusy() = 0;
  virtual void EndAshleyWeaponToggle() = 0;
  virtual void UpdateGimImages() = 0;

  // Battle action flow.
  virtual void UpdateTargetingRiskPenalty() = 0;
  virtual void RunBattleActionFlow(BattleActionFlow flow) = 0;
  virtual void DrawSphere() = 0;

  // Drawing and presentation.
  virtual void DrawRoomScenery() = 0;
  virtual void DrawRoomActors() = 0;
  virtual void UpdateEffectRuntime() = 0;
  virtual void DrawBattleHud() = 0;
  virtual void UpdateTextBoxes() = 0;
  virtual void UpdateTickTransitions() = 0;
  virtual void DrawScreenOverlays() = 0;
  virtual void RunActorAiUpdateStages() = 0;
  virtual void UpdateActorNavigationOverrides() = 0;
  virtual void PresentFrame() = 0;
};

// One frame of 146C.c:7558's inner loop. Returns false when the controller
// poll skipped the frame.
bool RunGameplayFrame(GameplayLoopState& state, GameplayLoopHost& host);
// True once a room or zone exit has fully faded out.
[[nodiscard]] bool IsRoomExitComplete(const GameplayLoopHost& host);
// Loads the pending room, runs frames until it is left, then unloads it.
// A refused load resets to title before any frame and returns false.
[[nodiscard]] bool RunGameplayRoom(GameplayLoopState& state,
                                   GameplayLoopHost& host);
// Loads the pending zone and runs rooms until a zone exit, then releases it.
// False propagates the room refusal; session reset owns cleanup in that case.
[[nodiscard]] bool RunGameplayZone(GameplayLoopState& state,
                                   GameplayLoopHost& host);
// The whole loop; a refused room ends it after the reset-to-title request.
void RunGameplayLoop(GameplayLoopState& state, GameplayLoopHost& host);
}  // namespace VS::Boot
