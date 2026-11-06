// Hand-maintained (tools/port/owned.csv).
#include "Boot/Session.hpp"

namespace VS::Boot {
namespace {
// Checked before any state changes, so a refused request leaves the session
// exactly as it was.
void CheckTransition(const SessionState& state, SessionHost& host) {
  if (!state.active) host.Refuse(SessionRefusal::kNoSession);
  if (state.barrier_depth > 0) host.Refuse(SessionRefusal::kInsideBarrier);
}

[[noreturn]] void RequestSession(SessionHost& host,
                                 SessionDestination destination) {
  host.InvalidateSceneUi();
  host.ResetGameplayInspection();
  host.Transfer(destination);
}
}  // namespace

bool BeginSession(SessionState& state, SessionHost& host,
                  bool entries_present) {
  if (state.active || !entries_present) return false;
  host.InvalidateSceneUi();
  state.active = true;
  host.ResetGameplayInspection();
  return true;
}

SessionDestination EntryFor(int boundary_value) {
  return boundary_value == static_cast<int>(SessionDestination::kGameLoop)
             ? SessionDestination::kGameLoop
             : SessionDestination::kTitle;
}

void EndSession(SessionState& state, SessionHost& host) {
  host.InvalidateSceneUi();
  host.ResetNativeSession();
  state.active = false;
  host.ResetGameplayInspection();
}

SessionTransitionBarrier::SessionTransitionBarrier(SessionState& state)
    : state_(state) {
  ++state_.barrier_depth;
}

SessionTransitionBarrier::~SessionTransitionBarrier() {
  --state_.barrier_depth;
}

void JumpToTitle(SessionState& state, SessionHost& host) {
  CheckTransition(state, host);
  host.ResetNativeSession();
  RequestSession(host, SessionDestination::kTitle);
}

void JumpToGameLoop(SessionState& state, SessionHost& host) {
  CheckTransition(state, host);
  RequestSession(host, SessionDestination::kGameLoop);
}

int RunTitleAndBattle(TitleSessionHost& host) {
  host.InitializeTitleSession();
  host.LoadTitleOverlay();
  host.SetGameEntryMode(host.RunTitle());
  host.ClearPendingClearSave();
  host.ShowLoadingScreen();
  host.LoadBattleOverlays();
  return host.RunLoadedGame();
}

void RunGame(GameHost& host) {
  host.InitializePlatformServices();
  host.RunSessions();
}

void DelayAfterOverlayLoad() {}

void WaitForOverlayReady() { DelayAfterOverlayLoad(); }

void LoadBattleOverlays(OverlayLoadHost& host) {
  host.Load(
      {.program = OverlayProgram::kBattle, .slot = OverlaySlot::kPrimary});
  host.Load({.program = OverlayProgram::kBattleInitialization,
             .slot = OverlaySlot::kSecondary});
  DelayAfterOverlayLoad();
}

void LoadTitleOverlay(OverlayLoadHost& host) {
  host.Load({.program = OverlayProgram::kTitle, .slot = OverlaySlot::kPrimary});
  DelayAfterOverlayLoad();
}

void LoadEndingOverlay(OverlayLoadHost& host) {
  host.Load(
      {.program = OverlayProgram::kEnding, .slot = OverlaySlot::kPrimary});
  DelayAfterOverlayLoad();
}

void SetSoftResetEnabled(SoftResetState& state, bool enabled) noexcept {
  state.enabled = enabled;
}

void ResetToTitle(TransitionHost& host) {
  host.ResetNativeSession();
  host.HideDisplay();
  host.StopControllerMotors();
  host.RefreshControllers();
  host.ShutdownAudio();
  host.WaitForVerticalSync(10);
  host.JumpToTitle();
}

void EnterBattle(TransitionHost& host, OverlayLoadHost& overlays) {
  host.HideDisplay();
  host.StopControllerMotors();
  host.RefreshControllers();
  host.StopAllMusic();
  host.StopSceneSoundEffects();
  host.InvalidateSavedMusicId();
  host.ClearMusicBankIds();
  host.ShowLoadingScreen();
  LoadBattleOverlays(overlays);
  host.SetGameEntryMode(1);
  host.JumpToGameLoop();
}

void RunEndingAndReturnToTitle(TransitionHost& host,
                               OverlayLoadHost& overlays) {
  host.SetGameplayActive(false);
  host.HideDisplay();
  host.StopControllerMotors();
  host.RefreshControllers();
  host.StopAllMusic();
  host.StopSceneSoundEffects();
  host.CaptureAshleySnapshot();
  LoadEndingOverlay(overlays);
  host.RunStaffRoll();
  host.HideDisplay();
  host.StopControllerMotors();
  host.RefreshControllers();
  host.ShutdownAudio();
  host.SetClearSavePending(true);
  host.JumpToTitle();
}

void InitializePlatformServices(SoftResetState& reset_state,
                                PlatformServicesHost& host) {
  host.SelectNtscVideoMode();
  host.HideDisplay();
  host.InitializeMemoryCard();
  host.StartMemoryCard();
  host.InitializeControllers();
  host.StartControllerCommunication();
  host.UnlockAnalogModeSwitch();
  host.ResetVibrationPatterns();
  host.SetReverbEnabled(false);
  host.InitializeDiscDrive();
  host.InitializeRandomShuffleTable();
  SetSoftResetEnabled(reset_state, true);
  host.StoreSoftResetEnabled(reset_state.enabled);
  host.ClearPendingClearSave();
}

void InitializeTitleSession(TitleSessionInitHost& host) {
  host.ResetNativeSession();
  host.ClearFramebuffer();
  host.ShowLoadingScreen();
  host.EnableReverb();
  host.RegisterVSyncCallback();
  host.ResetGameHeap();
  host.InvalidateAiBindings();
  host.InitializeDiscQueue();
  host.ResetDiscDrive();
  host.InitializeMenuSoundBanks();
  host.ResetVibrationPatterns();
  host.SetGameplayActive(false);
  host.UnlockAnalogModeSwitch();
  host.ResetButtonHeldFrames();
  host.ResetPlayTime();
  host.ClearAshleySnapshotPending();
}

void InitializeGameplayOverlay(GameplayOverlayHost& host) {
  const GameplayOverlayDefaults defaults{};
  host.ResetAudioSlots();
  host.ResetGameHeap();
  host.InitializeOrderingTables(defaults);
  host.InitializePaletteAnimation();
  host.InitializeRoomTransitionPrimitives();
  host.InitializeFramebufferEffect();
  host.InitializeScriptOpcodeTable();
  host.InitializeEffectRuntime();
  host.InitializeRoomRuntimeState();
  host.InitializeActorModelResources();
  host.InitializeTexturePages();
  host.ResetOverlayWorkspaceOwner();
  host.LoadSystemResources();
  host.InitializeActorAiHook();
  host.ResetVibrationPatterns();
  host.ApplyLearnedActionUnlocks();
  host.ApplyGameplayDefaults(defaults);
  host.SetRoomActorScale(defaults.room_actor_scale);
  host.ClearActorSlots(defaults.actor_slot_count);
  host.SpawnAshleyWithEquipment();
  host.AllocateBattleActionState();
  host.SetFieldRenderingEnabled(defaults.field_rendering_enabled);
  host.ResetGameplayResetWordTwo();
  host.ConfigureFieldDisplay(defaults);
}
}  // namespace VS::Boot
