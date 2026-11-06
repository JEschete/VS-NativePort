// Hand-maintained (tools/port/owned.csv).
// Live binding for the session policy (Session.cpp).
#include "Boot/SessionLive.hpp"

#include <algorithm>
#include <csetjmp>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Audio/MusicLive.hpp"
#include "Audio/SampleBanksLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Runtime.hpp"
#include "Ending/EndingLive.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Input/ControllerLive.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Save/SnapshotLive.hpp"
#include "State/GameClockLive.hpp"
#include "Title/TitleMenuLive.hpp"

extern "C" {
#include "Audio/SequencerOutput.h"
#include "Battle/Ai/RoomState.h"
#include "Battle/EncounterHooks.h"
#include "Battle/TargetingState.h"
#include "Boot/GameLoop.h"
#include "Content/DiscLayout.h"
#include "Core/Diagnostics.h"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Event/ScriptVm.h"
#include "Input/ControllerLiveState.h"
#include "Input/Controllers.h"
#include "Legacy/Psx.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Save/NativeResume.h"
#include "State/Ashley.h"
}

namespace VS::Boot {
namespace {
// The boundary is the one place a transition lands; the frames it abandons
// must hold no object whose destructor matters (#266), which the transition
// barrier enforces for the scopes known to need it.
std::jmp_buf session_boundary;
SessionState live_state;

// libpad port IDs (ControllerLive.hpp).
constexpr int kFirstPadPortId = 0x00;
constexpr int kSecondPadPortId = 0x10;
// The whole of VRAM, cleared at boot.
constexpr std::int16_t kVramWidth = 1024;
constexpr std::int16_t kVramHeight = 512;

class LiveSessionHost final : public SessionHost {
 public:
  // Native sessions can interrupt original event text. Drop UI borrows before
  // title/battle initialization replaces the game heap (SLUS main.c:8119,
  // 8344).
  void InvalidateSceneUi() override {
    std::memset(Menu_textBoxes, 0, sizeof Menu_textBoxes);
    std::memset(&Menu_textBoxSelector, 0, sizeof Menu_textBoxSelector);
    std::memset(&Battle_stringContext, 0, sizeof Battle_stringContext);
    std::memset(Event_isTextBoxAwaitingClose, 0,
                sizeof Event_isTextBoxAwaitingClose);
    Menu_sceneNotificationBuffer = nullptr;
    Menu_sceneNotificationTimer = 0;
    Menu_sceneNotificationTimed = 0;
    Event::ResetLiveScriptRuntime();
  }
  void ResetGameplayInspection() override { Boot_ResetGameplayInspection(); }
  void ResetNativeSession() override { Save_NativeResetSession(); }

  [[noreturn]] void Transfer(SessionDestination destination) override {
    Core_Debugf("boot", "native session transition: %s",
                destination == SessionDestination::kTitle ? "title" : "battle");
    Event::AbandonLiveScriptUses();
    // NOLINTNEXTLINE(cert-err52-cpp): the copied call graph is extern "C" and
    // cannot carry an exception (#267); the barrier guards the frames.
    std::longjmp(session_boundary, static_cast<int>(destination));
  }

  [[noreturn]] void Refuse(SessionRefusal refusal) override {
    Core_Errorf("boot", "native session transition refused: %s",
                refusal == SessionRefusal::kNoSession
                    ? "no session is running"
                    : "a scope bound to stack storage is live");
    std::abort();
  }
};

LiveSessionHost live_host;
SoftResetState live_soft_reset_state;

class LiveLifecycleHost final : public OverlayLoadHost,
                                public TransitionHost,
                                public PlatformServicesHost,
                                public TitleSessionInitHost {
 public:
  void Load(OverlayRequest request) override {
    Content_DiscFile file{};
    switch (request.program) {
      case OverlayProgram::kBattle:
        file.lba = VS_BATTLE_PRG_LBA;
        file.size = VS_BATTLE_PRG_SIZE;
        break;
      case OverlayProgram::kBattleInitialization:
        file.lba = VS_INITBTL_PRG_LBA;
        file.size = VS_INITBTL_PRG_SIZE;
        break;
      case OverlayProgram::kTitle:
        file.lba = VS_TITLE_PRG_LBA;
        file.size = VS_TITLE_PRG_SIZE;
        break;
      case OverlayProgram::kEnding:
        file.lba = VS_ENDING_PRG_LBA;
        file.size = VS_ENDING_PRG_SIZE;
        break;
      default:
        Core_Errorf("boot", "unsupported overlay program: %d",
                    static_cast<int>(request.program));
        std::abort();
    }

    const std::uint8_t slot_index = static_cast<std::uint8_t>(request.slot);
    if (slot_index > 1) {
      Core_Errorf("boot", "unsupported overlay slot: %u", slot_index);
      std::abort();
    }
    Content_DiscQueueSlot* slot = VS::Content::AllocateLiveDiscRequest(&file);
    if (slot != nullptr) {
      VS::Content::EnqueueLivePriorityDiscRequest(
          slot, Core_overlaySlots[slot_index]);
    } else {
      slot = VS::Content::CompleteLiveRefusedDiscRequest(
          &file, Core_overlaySlots[slot_index]);
    }
    while (slot->state != Content_DiscRequestState_Loaded) {
      VS::State::UpdateLivePlayTimeAndLoading(0);
    }
    VS::Content::ReleaseLiveDiscRequest(slot);
  }

  void ResetNativeSession() override { Save_NativeResetSession(); }
  void HideDisplay() override { SetDispMask(0); }
  void StopControllerMotors() override {
    VS::Input::SetLiveMotorCommand(0, 0, 0);
    VS::Input::SetLiveMotorCommand(0, 1, 0);
  }
  void RefreshControllers() override {
    VS::Input::UpdateLiveControllerConnection(kFirstPadPortId,
                                              Input_legacyPadResponseBytes[0]);
    VS::Input::UpdateLiveControllerConnection(kSecondPadPortId,
                                              Input_legacyPadResponseBytes[1]);
  }
  void ShutdownAudio() override { VS::Audio::Playback::LiveShutdown(); }
  void WaitForVerticalSync(int count) override { VSync(count); }
  void StopAllMusic() override { Audio::Control::LiveStopAllMusic(); }
  void StopSceneSoundEffects() override {
    Audio::Control::LiveStopSceneSoundEffects();
  }
  void InvalidateSavedMusicId() override {
    VS::Audio::Playback::LiveInvalidateSavedMusicId();
  }
  void ClearMusicBankIds() override {
    VS::Audio::Samples::LiveClearMusicBankIds();
  }
  void ShowLoadingScreen() override { VS::Render::ShowLiveLoadingScreen(); }
  void SetGameEntryMode(int mode) override { Boot_gameEntryMode = mode; }
  void SetGameplayActive(bool active) override {
    Boot_isGameplayActive = active ? 1 : 0;
  }
  void CaptureAshleySnapshot() override {
    VS::Save::CaptureLiveAshleySnapshot();
  }
  void RunStaffRoll() override { VS::Ending::LiveRunStaffRoll(); }
  void SetClearSavePending(bool pending) override {
    Boot_isClearSavePending = pending ? 1 : 0;
  }
  void JumpToTitle() override { JumpToLiveTitle(); }
  void JumpToGameLoop() override { JumpToLiveGameLoop(); }

  void SelectNtscVideoMode() override { SetVideoMode(MODE_NTSC); }
  void InitializeMemoryCard() override { InitCARD(0); }
  void StartMemoryCard() override { StartCARD(); }
  void InitializeControllers() override {
    PadInitDirect(
        reinterpret_cast<unsigned char*>(Input_legacyPadResponseBytes[0]),
        reinterpret_cast<unsigned char*>(Input_legacyPadResponseBytes[1]));
    VS::Input::ConfigureLiveControllerActuators(
        kFirstPadPortId, Input_legacyPadResponseBytes[0]);
    VS::Input::ConfigureLiveControllerActuators(
        kSecondPadPortId, Input_legacyPadResponseBytes[1]);
  }
  void StartControllerCommunication() override { PadStartCom(); }
  void UnlockAnalogModeSwitch() override {
    VS::Input::UnlockLiveAnalogModeSwitch();
  }
  void ResetVibrationPatterns() override {
    VS::Input::ResetLiveVibrationPatterns();
  }
  void SetReverbEnabled(bool enabled) override {
    Audio_Native_EnableReverb(enabled ? 1 : 0);
  }
  void InitializeDiscDrive() override { DsInit(); }
  void InitializeRandomShuffleTable() override {
    VS::Core::InitializeLiveRandomShuffleTable();
  }
  void StoreSoftResetEnabled(bool enabled) override {
    Boot_isSoftResetEnabled = enabled ? 1 : 0;
  }
  void ClearPendingClearSave() override { Boot_isClearSavePending = 0; }

  void ClearFramebuffer() override {
    RECT rect{.x = 0, .y = 0, .w = kVramWidth, .h = kVramHeight};
    ClearImage(&rect, 0, 0, 0);
  }
  void EnableReverb() override { Audio_Native_EnableReverb(1); }
  void RegisterVSyncCallback() override { VSyncCallback(VS::Core::IgnoreCall); }
  void ResetGameHeap() override { VS::Core::Memory::ResetLiveGameHeap(); }
  void InvalidateAiBindings() override {
    Battle_Ai_InvalidateSessionBindings();
  }
  void InitializeDiscQueue() override {
    VS::Content::InitializeLiveDiscQueue();
  }
  void ResetDiscDrive() override { VS::Content::ResetLiveDiscDrive(); }
  void InitializeMenuSoundBanks() override {
    VS::Audio::LiveInitializeMenuSoundBanks();
  }
  void ResetButtonHeldFrames() override {
    std::ranges::fill(Input_buttonHeldFrames, 0U);
  }
  void ResetPlayTime() override { VS::State::StoreLivePlayTime({}); }
  void ClearAshleySnapshotPending() override {
    VS::Save::ClearLiveAshleySnapshotPending();
  }
};

LiveLifecycleHost live_lifecycle_host;

int RunLoadedGame() {
  Boot_ResetGameplayInspection();
  Boot_isGameplayActive = 1;
  RunGameplayLoopLive();
  Boot_ResetGameplayInspection();
  SetDispMask(0);
  VS::Audio::LiveStopAndFreeCurrentMusic();
  return 0;
}

class LiveTitleSessionHost final : public TitleSessionHost {
 public:
  void InitializeTitleSession() override {
    Core_Debugf("boot", "title session initialization begin");
    InitializeTitleSessionLive();
    Core_Debugf("boot", "title session initialization complete");
  }
  void LoadTitleOverlay() override {
    LoadTitleOverlayLive();
    Core_Debugf("boot", "title module ready");
  }
  int RunTitle() override { return VS::Title::RunLive(); }
  void SetGameEntryMode(int mode) override { Boot_gameEntryMode = mode; }
  void ClearPendingClearSave() override { Boot_isClearSavePending = 0; }
  void ShowLoadingScreen() override { Render::ShowLiveLoadingScreen(); }
  void LoadBattleOverlays() override { LoadBattleOverlaysLive(); }
  int RunLoadedGame() override { return VS::Boot::RunLoadedGame(); }
};

class LiveGameHost final : public GameHost {
 public:
  void InitializePlatformServices() override {
    InitializePlatformServicesLive();
  }
  void RunSessions() override {
    RunLiveSessionLoop(RunLiveTitleAndBattle, RunLoadedGame);
  }
};
}  // namespace

int RunLiveSessionLoop(SessionEntry title, SessionEntry battle) {
  if (!BeginSession(live_state, live_host,
                    title != nullptr && battle != nullptr)) {
    return -1;
  }
  // NOLINTNEXTLINE(cert-err52-cpp): see Transfer.
  const int boundary_value = setjmp(session_boundary);
  const int result = EntryFor(boundary_value) == SessionDestination::kGameLoop
                         ? battle()
                         : title();
  EndSession(live_state, live_host);
  return result;
}

int RunLiveTitleAndBattle() {
  // Static: the frame below the boundary must not own it (#266).
  static LiveTitleSessionHost host;
  return RunTitleAndBattle(host);
}

void RunLiveGame() {
  LiveGameHost host;
  RunGame(host);
}

void LoadBattleOverlaysLive() { LoadBattleOverlays(live_lifecycle_host); }

void LoadTitleOverlayLive() { LoadTitleOverlay(live_lifecycle_host); }

void LoadEndingOverlayLive() { LoadEndingOverlay(live_lifecycle_host); }

void WaitForOverlayReadyLive() { WaitForOverlayReady(); }

void ResetToTitleLive() { ResetToTitle(live_lifecycle_host); }

void EnterBattleLive() {
  EnterBattle(live_lifecycle_host, live_lifecycle_host);
}

void RunEndingAndReturnToTitleLive() {
  RunEndingAndReturnToTitle(live_lifecycle_host, live_lifecycle_host);
}

void InitializePlatformServicesLive() {
  live_soft_reset_state.enabled = Boot_isSoftResetEnabled != 0;
  InitializePlatformServices(live_soft_reset_state, live_lifecycle_host);
}

void InitializeTitleSessionLive() {
  InitializeTitleSession(live_lifecycle_host);
}

void SetSoftResetEnabledLive(int enabled) {
  SetSoftResetEnabled(live_soft_reset_state, enabled != 0);
  live_lifecycle_host.StoreSoftResetEnabled(live_soft_reset_state.enabled);
}

void JumpToLiveTitle() { JumpToTitle(live_state, live_host); }

void JumpToLiveGameLoop() { JumpToGameLoop(live_state, live_host); }

LiveSessionTransitionBarrier::LiveSessionTransitionBarrier()
    : barrier_(live_state) {}
}  // namespace VS::Boot
