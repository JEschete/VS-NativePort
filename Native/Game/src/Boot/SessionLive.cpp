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
#include "Audio/VoiceOutputLive.hpp"
#include "Battle/Ai/RoomState.hpp"
#include "Battle/EncounterHooks.hpp"
#include "Battle/TargetingState.hpp"
#include "Boot/GameLoop.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Overlays.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Runtime.hpp"
#include "Ending/EndingLive.hpp"
#include "Event/ScriptDialogCommandsLive.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Controllers.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/BattleAbilitiesTerminalLive.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Loot/LootTerminalLive.hpp"
#include "Menu/MagicCostsLive.hpp"
#include "Menu/MagicMenusLive.hpp"
#include "Menu/MapTerminalLive.hpp"
#include "Menu/Results/GameClearLifecycleLive.hpp"
#include "Menu/Results/TimeAttackTerminalLive.hpp"
#include "Menu/SceneNotificationsLive.hpp"
#include "Menu/Score/EncyclopediaPreviewLive.hpp"
#include "Menu/Score/GazetteStatisticsLive.hpp"
#include "Menu/Score/GazetteTerminalLive.hpp"
#include "Menu/Score/GazetteTimesLive.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLive.hpp"
#include "Menu/TextBox.hpp"
#include "Render/Display.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "Save/NativeResume.hpp"
#include "Save/SnapshotLive.hpp"
#include "State/Ashley.hpp"
#include "State/GameClockLive.hpp"
#include "State/PlayTime.hpp"
#include "Title/TitleMenuLive.hpp"

using VS::Legacy::ClearImage;
using VS::Legacy::InitCARD;
using VS::Legacy::kModeNtsc;
using VS::Legacy::PadInitDirect;
using VS::Legacy::PadStartCom;
using VS::Legacy::RECT;
using VS::Legacy::SetDispMask;
using VS::Legacy::SetVideoMode;
using VS::Legacy::StartCARD;
using VS::Legacy::VSync;
using VS::Legacy::VSyncCallback;

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

void CleanupMenuOwners() {
  if (!Menu::map::ResetMapTerminalColdState()) {
    VS::Core::Errorf("boot", "map cleanup refused before session reset");
    std::abort();
  }
  if (!Menu::CleanupLiveSceneNotificationOwnedState()) {
    VS::Core::Errorf("boot",
                     "scene-notification cleanup refused before session reset");
    std::abort();
  }
  if (!Menu::Score::CleanupLiveEnemyPreviewOwnedState()) {
    VS::Core::Errorf("boot",
                     "enemy-preview cleanup refused before session reset");
    std::abort();
  }
  if (!Menu::Score::ResetLiveGazetteTerminalModuleState()) {
    VS::Core::Errorf("boot", "Gazette cleanup refused before session reset");
    std::abort();
  }
  if (!Menu::Results::CleanupLiveGameClearOwnedState()) {
    VS::Core::Errorf("boot", "game-clear cleanup refused before session reset");
    std::abort();
  }
  if (!Menu::Results::CleanupLiveTimeAttackOwnedState()) {
    VS::Core::Errorf("boot",
                     "time-attack cleanup refused before session reset");
    std::abort();
  }
}

class LiveSessionHost final : public SessionHost {
 public:
  // Native sessions can interrupt original event text. Drop UI borrows before
  // title/battle initialization replaces the game heap (SLUS main.c:8119,
  // 8344).
  void InvalidateSceneUi() override {
    CleanupMenuOwners();
    Event::ReleaseLiveScriptDialogText();
    std::memset(VS::Menu::textBoxes, 0, sizeof VS::Menu::textBoxes);
    std::memset(&VS::Menu::textBoxSelector, 0,
                sizeof VS::Menu::textBoxSelector);
    std::memset(&VS::Battle::stringContext, 0,
                sizeof VS::Battle::stringContext);
    std::memset(VS::Event::isTextBoxAwaitingClose, 0,
                sizeof VS::Event::isTextBoxAwaitingClose);
    VS::Menu::sceneNotificationBuffer = nullptr;
    VS::Menu::sceneNotificationTimer = 0;
    VS::Menu::sceneNotificationTimed = 0;
    Menu::BattleAbilities::ResetLiveAbilityTerminalColdState();
    Menu::Loot::ResetLootTerminalSession();
    Menu::Inventory::ResetInventoryTerminalSession();
    Menu::Magic::ResetMagicMenusLiveState();
    Menu::Magic::ResetTeleportMenuLiveState();
    Menu::Status::ResetLiveLimbControllersModuleData();
    Menu::Status::ResetLiveTerminalControllersModuleData();
    Menu::Results::ResetLiveGameClearModuleData();
    Menu::Results::ResetLiveTimeAttackModuleData();
    Menu::Score::ResetLiveGazetteStatisticsStatics();
    Menu::Score::ResetLiveTimeTrialStatics();
    Event::ResetLiveScriptRuntime();
  }
  void ResetGameplayInspection() override {
    VS::Boot::ResetGameplayInspection();
  }
  void ResetNativeSession() override { VS::Game::Save::ResetSession(); }

  [[noreturn]] void Transfer(SessionDestination destination) override {
    VS::Core::Debugf(
        "boot", "native session transition: %s",
        destination == SessionDestination::kTitle ? "title" : "battle");
    Event::AbandonLiveScriptUses();
    // NOLINTNEXTLINE(cert-err52-cpp): the session boundary leaves the copied
    // call graph by jump, not exception (#267); the barrier guards the frames.
    std::longjmp(session_boundary, static_cast<int>(destination));
  }

  [[noreturn]] void Refuse(SessionRefusal refusal) override {
    VS::Core::Errorf("boot", "native session transition refused: %s",
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
    VS::Content::DiscFile file{};
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
        VS::Core::Errorf("boot", "unsupported overlay program: %d",
                         static_cast<int>(request.program));
        std::abort();
    }

    const std::uint8_t slot_index = static_cast<std::uint8_t>(request.slot);
    if (slot_index > 1) {
      VS::Core::Errorf("boot", "unsupported overlay slot: %u", slot_index);
      std::abort();
    }
    VS::Content::DiscQueueSlot* slot =
        VS::Content::AllocateLiveDiscRequest(&file);
    if (slot != nullptr) {
      VS::Content::EnqueueLivePriorityDiscRequest(
          slot, VS::Core::kOverlaySlots[slot_index]);
    } else {
      slot = VS::Content::CompleteLiveRefusedDiscRequest(
          &file, VS::Core::kOverlaySlots[slot_index]);
    }
    while (slot->state != VS::Content::DiscRequestState::kLoaded) {
      VS::State::UpdateLivePlayTimeAndLoading(0);
    }
    VS::Content::ReleaseLiveDiscRequest(slot);
  }

  void ResetNativeSession() override { VS::Game::Save::ResetSession(); }
  void HideDisplay() override { SetDispMask(0); }
  void StopControllerMotors() override {
    VS::Input::SetLiveMotorCommand(0, 0, 0);
    VS::Input::SetLiveMotorCommand(0, 1, 0);
  }
  void RefreshControllers() override {
    VS::Input::UpdateLiveControllerConnection(
        kFirstPadPortId, VS::Input::legacyPadResponseBytes[0]);
    VS::Input::UpdateLiveControllerConnection(
        kSecondPadPortId, VS::Input::legacyPadResponseBytes[1]);
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
  void SetGameEntryMode(int mode) override { VS::Boot::gameEntryMode = mode; }
  void SetGameplayActive(bool active) override {
    VS::Boot::isGameplayActive = active ? 1 : 0;
  }
  void CaptureAshleySnapshot() override {
    VS::Save::CaptureLiveAshleySnapshot();
  }
  void RunStaffRoll() override { VS::Ending::LiveRunStaffRoll(); }
  void SetClearSavePending(bool pending) override {
    VS::Boot::isClearSavePending = pending ? 1 : 0;
  }
  void JumpToTitle() override { JumpToLiveTitle(); }
  void JumpToGameLoop() override { JumpToLiveGameLoop(); }

  void SelectNtscVideoMode() override { SetVideoMode(kModeNtsc); }
  void InitializeMemoryCard() override { InitCARD(0); }
  void StartMemoryCard() override { StartCARD(); }
  void InitializeControllers() override {
    PadInitDirect(VS::Input::legacyPadResponseBytes[0].data(),
                  VS::Input::legacyPadResponseBytes[1].data());
    VS::Input::ConfigureLiveControllerActuators(
        kFirstPadPortId, VS::Input::legacyPadResponseBytes[0]);
    VS::Input::ConfigureLiveControllerActuators(
        kSecondPadPortId, VS::Input::legacyPadResponseBytes[1]);
  }
  void StartControllerCommunication() override { PadStartCom(); }
  void UnlockAnalogModeSwitch() override {
    VS::Input::UnlockLiveAnalogModeSwitch();
  }
  void ResetVibrationPatterns() override {
    VS::Input::ResetLiveVibrationPatterns();
  }
  void SetReverbEnabled(bool enabled) override {
    VS::Audio::Driver::EnableOutputReverb(enabled ? 1 : 0);
  }
  void InitializeDiscDrive() override { VS::Legacy::DsInit(); }
  void InitializeRandomShuffleTable() override {
    VS::Core::InitializeLiveRandomShuffleTable();
  }
  void StoreSoftResetEnabled(bool enabled) override {
    VS::Boot::isSoftResetEnabled = enabled ? 1 : 0;
  }
  void ClearPendingClearSave() override { VS::Boot::isClearSavePending = 0; }

  void ClearFramebuffer() override {
    RECT rect{.x = 0, .y = 0, .w = kVramWidth, .h = kVramHeight};
    ClearImage(&rect, 0, 0, 0);
  }
  void EnableReverb() override { VS::Audio::Driver::EnableOutputReverb(1); }
  void RegisterVSyncCallback() override { VSyncCallback(VS::Core::IgnoreCall); }
  void ResetGameHeap() override { ResetLiveSessionHeap(); }
  void InvalidateAiBindings() override {
    VS::Battle::Ai::InvalidateSessionBindings();
  }
  void InitializeDiscQueue() override {
    VS::Content::InitializeLiveDiscQueue();
  }
  void ResetDiscDrive() override { VS::Content::ResetLiveDiscDrive(); }
  void InitializeMenuSoundBanks() override {
    VS::Audio::LiveInitializeMenuSoundBanks();
  }
  void ResetButtonHeldFrames() override {
    std::ranges::fill(VS::Input::buttonHeldFrames, 0U);
  }
  void ResetPlayTime() override { VS::State::playTime = {}; }
  void ClearAshleySnapshotPending() override {
    VS::Save::ClearLiveAshleySnapshotPending();
  }
};

LiveLifecycleHost live_lifecycle_host;

int RunLoadedGame() {
  VS::Boot::ResetGameplayInspection();
  VS::Boot::isGameplayActive = 1;
  RunGameplayLoopLive();
  VS::Boot::ResetGameplayInspection();
  SetDispMask(0);
  VS::Audio::LiveStopAndFreeCurrentMusic();
  return 0;
}

class LiveTitleSessionHost final : public TitleSessionHost {
 public:
  void InitializeTitleSession() override {
    VS::Core::Debugf("boot", "title session initialization begin");
    InitializeTitleSessionLive();
    VS::Core::Debugf("boot", "title session initialization complete");
  }
  void LoadTitleOverlay() override {
    LoadTitleOverlayLive();
    VS::Core::Debugf("boot", "title module ready");
  }
  int RunTitle() override { return VS::Title::RunLive(); }
  void SetGameEntryMode(int mode) override { VS::Boot::gameEntryMode = mode; }
  void ClearPendingClearSave() override { VS::Boot::isClearSavePending = 0; }
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

void ResetLiveSessionHeap() {
  CleanupMenuOwners();
  if (!Render::AbandonLiveFrameBeforeHeapReset()) {
    VS::Core::Errorf("boot", "frame cleanup refused before heap replacement");
    std::abort();
  }
  Core::Memory::ResetLiveGameHeap();
}

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
  live_soft_reset_state.enabled = VS::Boot::isSoftResetEnabled != 0;
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
