// Hand-maintained (tools/port/owned.csv).
// Live binding for the session policy (Session.cpp).
#include "Boot/SessionLive.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "Audio/MusicLive.hpp"
#include "Audio/SampleBanksLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Battle/Ai/RoomState.hpp"
#include "Battle/EncounterHooks.hpp"
#include "Battle/SphereGeometryLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Boot/GameLoop.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.hpp"
#include "Content/DiscQueue.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Overlays.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/Particles.hpp"
#include "Ending/EndingLive.hpp"
#include "Event/EventCameraLensLive.hpp"
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
#include "Platform/LogicalTicks.hpp"
#include "Render/Display.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "Save/NativeResume.hpp"
#include "Save/SnapshotLive.hpp"
#include "State/Ashley.hpp"
#include "State/GameClockLive.hpp"
#include "State/PlayTime.hpp"
#include "Title/TitleMenuLive.hpp"

using VS::Content::disc::Locate;

using VS::Legacy::ClearImage;
using VS::Legacy::InitCARD;
using VS::Legacy::RECT;
using VS::Legacy::SetDispMask;
using VS::Legacy::StartCARD;

namespace VS::Boot {
namespace {
SessionState liveState;

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
  void ResetNativeSession() override { ResetLiveSessionState(); }

  // The script VM's current invocation and source uses are bookkeeping, not
  // scopes, so the frames a transfer unwound leave them behind.
  void AbandonInterruptedWork() override { Event::AbandonLiveScriptUses(); }
  void EnterMode(Mode mode) override {
    VS::Core::Debugf("boot", "session mode: %s", ModeName(mode));
  }

  [[noreturn]] void Refuse(SessionRefusal refusal) override {
    VS::Core::Errorf("boot", "native session transition refused: %s",
                     refusal == SessionRefusal::kNoSession
                         ? "no session is running"
                         : "unknown refusal");
    std::abort();
  }
};

LiveSessionHost liveHost;
SoftResetState liveSoftResetState;

class LiveLifecycleHost final : public OverlayLoadHost,
                                public TransitionHost,
                                public PlatformServicesHost,
                                public TitleSessionInitHost {
 public:
  void Load(OverlayRequest request) override {
    VS::Content::DiscFile file{};
    switch (request.program) {
      case OverlayProgram::kBattle:
        file.lba = Locate("BATTLE/BATTLE.PRG").lba;
        file.size = Locate("BATTLE/BATTLE.PRG").size;
        break;
      case OverlayProgram::kBattleInitialization:
        file.lba = Locate("BATTLE/INITBTL.PRG").lba;
        file.size = Locate("BATTLE/INITBTL.PRG").size;
        break;
      case OverlayProgram::kTitle:
        file.lba = Locate("TITLE/TITLE.PRG").lba;
        file.size = Locate("TITLE/TITLE.PRG").size;
        break;
      case OverlayProgram::kEnding:
        file.lba = Locate("ENDING/ENDING.PRG").lba;
        file.size = Locate("ENDING/ENDING.PRG").size;
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
      VS::State::UpdateLivePlayTimeAndLoading();
    }
    VS::Content::ReleaseLiveDiscRequest(slot);
  }

  void ResetNativeSession() override { ResetLiveSessionState(); }
  void HideDisplay() override { SetDispMask(0); }
  void StopControllerMotors() override {
    VS::Input::SetLiveMotorCommand(0, 0, 0);
    VS::Input::SetLiveMotorCommand(0, 1, 0);
  }
  void RefreshControllers() override {
    VS::Input::UpdateLiveControllerConnection(kFirstPadPortId);
    VS::Input::UpdateLiveControllerConnection(kSecondPadPortId);
  }
  void ShutdownAudio() override { VS::Audio::Playback::LiveShutdown(); }
  void WaitTicks(int count) override { VS::Platform::WaitTicks(count); }
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
  void JumpToEnding() override { VS::Boot::JumpToEnding(liveState, liveHost); }

  void InitializeMemoryCard() override { InitCARD(0); }
  void StartMemoryCard() override { StartCARD(); }
  void InitializeControllers() override {
    VS::Input::ResetLivePadLinks();
    VS::Input::ConfigureLiveControllerActuators(kFirstPadPortId);
    VS::Input::ConfigureLiveControllerActuators(kSecondPadPortId);
  }
  // libpad began polling the ports here; the host loop submits them each
  // logical tick instead (VS::Input::SubmitLivePad).
  void StartControllerCommunication() override {}
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

LiveLifecycleHost liveLifecycleHost;

// The rest of vs_main_execTitle (main.c:1198): the game loop, then the
// teardown once it returns.
void RunLoadedGame() {
  VS::Boot::ResetGameplayInspection();
  VS::Boot::isGameplayActive = 1;
  RunGameplayLoopLive();
  VS::Boot::ResetGameplayInspection();
  SetDispMask(0);
  VS::Audio::LiveStopAndFreeCurrentMusic();
}

class LiveBootHost final : public BootHost {
 public:
  void InitializePlatformServices() override {
    InitializePlatformServicesLive();
  }
};

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
};

class BootModule final : public ModeModule {
 public:
  std::optional<Mode> Run() override {
    LiveBootHost host;
    return RunBootMode(host);
  }
};

class TitleModule final : public ModeModule {
 public:
  std::optional<Mode> Run() override {
    LiveTitleSessionHost host;
    return RunTitleMode(host);
  }
};

class GameplayModule final : public ModeModule {
 public:
  std::optional<Mode> Run() override {
    RunLoadedGame();
    return std::nullopt;
  }
};

class EndingModule final : public ModeModule {
 public:
  std::optional<Mode> Run() override {
    RunEndingMode(liveLifecycleHost, liveLifecycleHost);
    // Not reached: the ending leaves by its jump to the title.
    return std::nullopt;
  }
};

class ProductionModules final : public ModeModules {
 public:
  ModeModule& For(Mode mode) override {
    switch (mode) {
      case Mode::kBoot:
        return boot_;
      case Mode::kTitle:
        return title_;
      case Mode::kGameplay:
        return gameplay_;
      case Mode::kEnding:
        return ending_;
    }
    VS::Core::Errorf("boot", "unknown session mode: %d",
                     static_cast<int>(mode));
    std::abort();
  }

 private:
  BootModule boot_;
  TitleModule title_;
  GameplayModule gameplay_;
  EndingModule ending_;
};

// A mode that runs one entry function and ends the session with its value.
class EntryModule final : public ModeModule {
 public:
  EntryModule(SessionEntry entry, int& result)
      : entry_(entry), result_(result) {}
  std::optional<Mode> Run() override {
    result_ = entry_();
    return std::nullopt;
  }

 private:
  SessionEntry entry_;
  int& result_;
};

// The production modes with the title and the game loop replaced by entry
// functions.
class EntryModules final : public ModeModules {
 public:
  EntryModules(SessionEntry title, SessionEntry battle)
      : title_(title, result_), gameplay_(battle, result_) {}
  ModeModule& For(Mode mode) override {
    if (mode == Mode::kTitle) return title_;
    if (mode == Mode::kGameplay) return gameplay_;
    return production_.For(mode);
  }
  int Result() const { return result_; }

 private:
  int result_ = 0;
  EntryModule title_;
  EntryModule gameplay_;
  ProductionModules production_;
};
}  // namespace

// The workspaces below replace the PS1 scratchpad (#45), which outlived a soft
// reset only because nothing cleared it; the title overlay overwrote it anyway,
// so a new session matches the first boot instead of inheriting its values.
void ResetLiveSessionState() {
  VS::Game::Save::ResetSession();
  Event::ResetLiveEventCameraScratch();
  Effects::ResetEmitterWorkspace();
  Battle::ResetLiveSphereWorkspace();
}

void ResetLiveSessionHeap() {
  CleanupMenuOwners();
  if (!Render::AbandonLiveFrameBeforeHeapReset()) {
    VS::Core::Errorf("boot", "frame cleanup refused before heap replacement");
    std::abort();
  }
  Core::Memory::ResetLiveGameHeap();
}

void RunLiveGame() {
  ProductionModules modules;
  if (!RunSession(liveState, liveHost, modules, Mode::kBoot)) {
    VS::Core::Errorf("boot", "a native session is already running");
  }
}

int RunLiveSessionLoop(SessionEntry title, SessionEntry battle) {
  if (title == nullptr || battle == nullptr) return -1;
  EntryModules modules(title, battle);
  if (!RunSession(liveState, liveHost, modules, Mode::kTitle)) return -1;
  return modules.Result();
}

std::optional<Mode> LiveSessionMode() {
  if (!liveState.active) return std::nullopt;
  return liveState.mode;
}

void LoadBattleOverlaysLive() { LoadBattleOverlays(liveLifecycleHost); }

void LoadTitleOverlayLive() { LoadTitleOverlay(liveLifecycleHost); }

void LoadEndingOverlayLive() { LoadEndingOverlay(liveLifecycleHost); }

void WaitForOverlayReadyLive() { WaitForOverlayReady(); }

void ResetToTitleLive() { ResetToTitle(liveLifecycleHost); }

void EnterBattleLive() { EnterBattle(liveLifecycleHost, liveLifecycleHost); }

void RunEndingAndReturnToTitleLive() {
  RunEndingAndReturnToTitle(liveLifecycleHost);
}

void InitializePlatformServicesLive() {
  liveSoftResetState.enabled = VS::Boot::isSoftResetEnabled != 0;
  InitializePlatformServices(liveSoftResetState, liveLifecycleHost);
}

void InitializeTitleSessionLive() { InitializeTitleSession(liveLifecycleHost); }

void SetSoftResetEnabledLive(int enabled) {
  SetSoftResetEnabled(liveSoftResetState, enabled != 0);
  liveLifecycleHost.StoreSoftResetEnabled(liveSoftResetState.enabled);
}

void JumpToLiveTitle() { JumpToTitle(liveState, liveHost); }

void JumpToLiveGameLoop() { JumpToGameLoop(liveState, liveHost); }
}  // namespace VS::Boot
