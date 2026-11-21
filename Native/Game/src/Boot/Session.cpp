// Hand-maintained (tools/port/owned.csv).
#include "Boot/Session.hpp"

namespace VS::Boot {
namespace {
constexpr int kSoftResetWaitTicks = 10;

// The session's extent: ends it on every exit from RunSession, including an
// exception or a fiber cancellation unwinding through it.
class SessionScope {
 public:
  SessionScope(SessionState& state, SessionHost& host)
      : state_(state), host_(host) {
    host_.InvalidateSceneUi();
    state_.active = true;
    host_.ResetGameplayInspection();
  }
  ~SessionScope() {
    host_.InvalidateSceneUi();
    host_.ResetNativeSession();
    state_.active = false;
    host_.ResetGameplayInspection();
  }
  SessionScope(const SessionScope&) = delete;
  SessionScope& operator=(const SessionScope&) = delete;
  SessionScope(SessionScope&&) = delete;
  SessionScope& operator=(SessionScope&&) = delete;

 private:
  SessionState& state_;
  SessionHost& host_;
};

// The native state a transition resets: the title starts a new save session,
// and the title and the game loop each replace the scene heap the UI borrows.
// Runs after the frames the transition left have unwound, so none of their
// destructors can undo it.
void PrepareDestination(SessionHost& host, Mode destination) {
  switch (destination) {
    case Mode::kTitle:
      host.ResetNativeSession();
      host.InvalidateSceneUi();
      host.ResetGameplayInspection();
      break;
    case Mode::kGameplay:
      host.InvalidateSceneUi();
      host.ResetGameplayInspection();
      break;
    case Mode::kBoot:
    case Mode::kEnding:
      break;
  }
}

// Checked before anything changes, so a refused request leaves the session
// exactly as it was.
[[noreturn]] void Transfer(const SessionState& state, SessionHost& host,
                           Mode destination) {
  if (!state.active) host.Refuse(SessionRefusal::kNoSession);
  throw SessionTransfer{destination};
}
}  // namespace

const char* ModeName(Mode mode) noexcept {
  switch (mode) {
    case Mode::kBoot:
      return "boot";
    case Mode::kTitle:
      return "title";
    case Mode::kGameplay:
      return "gameplay";
    case Mode::kEnding:
      return "ending";
  }
  return "?";
}

bool RunSession(SessionState& state, SessionHost& host, ModeModules& modules,
                Mode first) {
  if (state.active) return false;
  const SessionScope session(state, host);
  std::optional<Mode> next = first;
  while (next) {
    const Mode mode = *next;
    state.mode = mode;
    host.EnterMode(mode);
    std::optional<Mode> transfer;
    try {
      next = modules.For(mode).Run();
    } catch (const SessionTransfer& request) {
      transfer = request.destination;
    }
    // Outside the handler: the destination runs with no exception in flight.
    if (transfer) {
      host.AbandonInterruptedWork();
      PrepareDestination(host, *transfer);
      next = transfer;
    }
  }
  return true;
}

void JumpToTitle(const SessionState& state, SessionHost& host) {
  Transfer(state, host, Mode::kTitle);
}

void JumpToGameLoop(const SessionState& state, SessionHost& host) {
  Transfer(state, host, Mode::kGameplay);
}

void JumpToEnding(const SessionState& state, SessionHost& host) {
  Transfer(state, host, Mode::kEnding);
}

Mode RunBootMode(BootHost& host) {
  host.InitializePlatformServices();
  return Mode::kTitle;
}

Mode RunTitleMode(TitleSessionHost& host) {
  host.InitializeTitleSession();
  host.LoadTitleOverlay();
  host.SetGameEntryMode(host.RunTitle());
  host.ClearPendingClearSave();
  host.ShowLoadingScreen();
  host.LoadBattleOverlays();
  return Mode::kGameplay;
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
  host.WaitTicks(kSoftResetWaitTicks);
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

void RunEndingAndReturnToTitle(TransitionHost& host) {
  host.SetGameplayActive(false);
  host.HideDisplay();
  host.StopControllerMotors();
  host.RefreshControllers();
  host.StopAllMusic();
  host.StopSceneSoundEffects();
  host.CaptureAshleySnapshot();
  host.JumpToEnding();
}

void RunEndingMode(TransitionHost& host, OverlayLoadHost& overlays) {
  LoadEndingOverlay(overlays);
  host.RunStaffRoll();
  host.HideDisplay();
  host.StopControllerMotors();
  host.RefreshControllers();
  host.ShutdownAudio();
  host.SetClearSavePending(true);
  host.JumpToTitle();
}

// _sysInit first selected the NTSC video mode (SetVideoMode(MODE_NTSC),
// main.c:1138); the native frame clock is always the NTSC tick
// (Core::FrameClock).
void InitializePlatformServices(SoftResetState& reset_state,
                                PlatformServicesHost& host) {
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

// _sysReinit also installed an empty vertical-blank callback
// (VSyncCallback(vSyncVoidCallback), main.c:1176); the native tick runs no
// callback.
void InitializeTitleSession(TitleSessionInitHost& host) {
  host.ResetNativeSession();
  host.ClearFramebuffer();
  host.ShowLoadingScreen();
  host.EnableReverb();
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
  // The original also cleared the write-only D_800F18F4 (18.c:605).
  host.ConfigureFieldDisplay(defaults);
}
}  // namespace VS::Boot
