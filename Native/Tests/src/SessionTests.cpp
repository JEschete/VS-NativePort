#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Boot/Session.hpp"

namespace {
using VS::Boot::SessionDestination;
using VS::Boot::SessionRefusal;
using VS::Boot::SessionState;
using VS::Boot::SessionTransitionBarrier;

struct Transferred {
  SessionDestination destination;
};
struct Refused {
  SessionRefusal refusal;
};

// The core has no extern "C" frames, so the test host can leave Transfer and
// Refuse with exceptions instead of the live boundary jump.
class RecordingHost final : public VS::Boot::SessionHost {
 public:
  std::vector<std::string> calls;

  void InvalidateSceneUi() override { calls.emplace_back("ui"); }
  void ResetGameplayInspection() override { calls.emplace_back("inspect"); }
  void ResetNativeSession() override { calls.emplace_back("save"); }
  [[noreturn]] void Transfer(SessionDestination destination) override {
    calls.emplace_back("transfer");
    throw Transferred{destination};
  }
  [[noreturn]] void Refuse(SessionRefusal refusal) override {
    calls.emplace_back("refuse");
    throw Refused{refusal};
  }
};

template <typename Jump>
SessionDestination TransferredTo(Jump jump) {
  try {
    jump();
  } catch (const Transferred& transferred) {
    return transferred.destination;
  }
  FAIL("the jump returned");
  return SessionDestination::kTitle;
}

template <typename Jump>
SessionRefusal RefusedWith(Jump jump) {
  try {
    jump();
  } catch (const Refused& refused) {
    return refused.refusal;
  } catch (const Transferred&) {
    FAIL("the jump transferred");
  }
  FAIL("the jump returned");
  return SessionRefusal::kNoSession;
}

class RecordingTitleHost final : public VS::Boot::TitleSessionHost {
 public:
  std::vector<std::string> calls;
  int entry_mode = -1;

  void InitializeTitleSession() override { calls.emplace_back("init"); }
  void LoadTitleOverlay() override { calls.emplace_back("overlay"); }
  int RunTitle() override {
    calls.emplace_back("title");
    return 3;
  }
  void SetGameEntryMode(int mode) override {
    calls.emplace_back("mode");
    entry_mode = mode;
  }
  void ClearPendingClearSave() override { calls.emplace_back("clear"); }
  void ShowLoadingScreen() override { calls.emplace_back("loading"); }
  void LoadBattleOverlays() override { calls.emplace_back("battle"); }
  int RunLoadedGame() override {
    calls.emplace_back("game");
    return 9;
  }
};

class RecordingGameHost final : public VS::Boot::GameHost {
 public:
  std::vector<std::string> calls;
  void InitializePlatformServices() override { calls.emplace_back("platform"); }
  void RunSessions() override { calls.emplace_back("sessions"); }
};

const char* ProgramName(VS::Boot::OverlayProgram program) {
  switch (program) {
    case VS::Boot::OverlayProgram::kBattle:
      return "battle";
    case VS::Boot::OverlayProgram::kBattleInitialization:
      return "battle-init";
    case VS::Boot::OverlayProgram::kTitle:
      return "title";
    case VS::Boot::OverlayProgram::kEnding:
      return "ending";
  }
  return "unknown";
}

class RecordingLifecycleHost final : public VS::Boot::OverlayLoadHost,
                                     public VS::Boot::TransitionHost,
                                     public VS::Boot::PlatformServicesHost,
                                     public VS::Boot::TitleSessionInitHost {
 public:
  std::vector<std::string> calls;

  void Load(VS::Boot::OverlayRequest request) override {
    calls.emplace_back(std::string("load-") + ProgramName(request.program) +
                       "-" + std::to_string(static_cast<int>(request.slot)));
  }
  void ResetNativeSession() override { calls.emplace_back("reset-session"); }
  void HideDisplay() override { calls.emplace_back("hide"); }
  void StopControllerMotors() override { calls.emplace_back("stop-motors"); }
  void RefreshControllers() override { calls.emplace_back("refresh-input"); }
  void ShutdownAudio() override { calls.emplace_back("shutdown-audio"); }
  void WaitForVerticalSync(int count) override {
    calls.emplace_back("vsync-" + std::to_string(count));
  }
  void StopAllMusic() override { calls.emplace_back("stop-music"); }
  void StopSceneSoundEffects() override {
    calls.emplace_back("stop-scene-sfx");
  }
  void InvalidateSavedMusicId() override {
    calls.emplace_back("invalidate-music");
  }
  void ClearMusicBankIds() override { calls.emplace_back("clear-music-banks"); }
  void ShowLoadingScreen() override { calls.emplace_back("loading"); }
  void SetGameEntryMode(int mode) override {
    calls.emplace_back("entry-" + std::to_string(mode));
  }
  void SetGameplayActive(bool active) override {
    calls.emplace_back(active ? "gameplay-on" : "gameplay-off");
  }
  void CaptureAshleySnapshot() override {
    calls.emplace_back("capture-ashley");
  }
  void RunStaffRoll() override { calls.emplace_back("staff-roll"); }
  void SetClearSavePending(bool pending) override {
    calls.emplace_back(pending ? "clear-save-on" : "clear-save-off");
  }
  void JumpToTitle() override { calls.emplace_back("jump-title"); }
  void JumpToGameLoop() override { calls.emplace_back("jump-game"); }
  void SelectNtscVideoMode() override { calls.emplace_back("ntsc"); }
  void InitializeMemoryCard() override { calls.emplace_back("init-card"); }
  void StartMemoryCard() override { calls.emplace_back("start-card"); }
  void InitializeControllers() override {
    calls.emplace_back("init-controllers");
  }
  void StartControllerCommunication() override {
    calls.emplace_back("start-comms");
  }
  void UnlockAnalogModeSwitch() override {
    calls.emplace_back("unlock-analog");
  }
  void ResetVibrationPatterns() override {
    calls.emplace_back("reset-vibration");
  }
  void SetReverbEnabled(bool enabled) override {
    calls.emplace_back(enabled ? "reverb-on" : "reverb-off");
  }
  void InitializeDiscDrive() override { calls.emplace_back("init-cd"); }
  void InitializeRandomShuffleTable() override {
    calls.emplace_back("init-random");
  }
  void StoreSoftResetEnabled(bool enabled) override {
    calls.emplace_back(enabled ? "soft-reset-on" : "soft-reset-off");
  }
  void ClearPendingClearSave() override { calls.emplace_back("clear-save"); }
  void ClearFramebuffer() override { calls.emplace_back("clear-framebuffer"); }
  void EnableReverb() override { calls.emplace_back("reverb-on"); }
  void RegisterVSyncCallback() override {
    calls.emplace_back("vsync-callback");
  }
  void ResetGameHeap() override { calls.emplace_back("reset-heap"); }
  void InvalidateAiBindings() override { calls.emplace_back("invalidate-ai"); }
  void InitializeDiscQueue() override { calls.emplace_back("init-queue"); }
  void ResetDiscDrive() override { calls.emplace_back("reset-cd"); }
  void InitializeMenuSoundBanks() override {
    calls.emplace_back("init-menu-audio");
  }
  void ResetButtonHeldFrames() override {
    calls.emplace_back("reset-held-frames");
  }
  void ResetPlayTime() override { calls.emplace_back("reset-play-time"); }
  void ClearAshleySnapshotPending() override {
    calls.emplace_back("clear-ashley-pending");
  }
};

class RecordingGameplayOverlayHost final
    : public VS::Boot::GameplayOverlayHost {
 public:
  std::vector<std::string> calls;
  VS::Boot::GameplayOverlayDefaults ordering_defaults;
  VS::Boot::GameplayOverlayDefaults gameplay_defaults;
  VS::Boot::GameplayOverlayDefaults display_defaults;
  VS::Core::Math::Vec3 room_actor_scale{};
  std::size_t actor_slot_count = 0;
  bool field_rendering_enabled = false;

  void ResetAudioSlots() override { calls.emplace_back("audio"); }
  void ResetGameHeap() override { calls.emplace_back("heap"); }
  void InitializeOrderingTables(
      const VS::Boot::GameplayOverlayDefaults& defaults) override {
    calls.emplace_back("ordering-tables");
    ordering_defaults = defaults;
  }
  void InitializePaletteAnimation() override {
    calls.emplace_back("palette-animation");
  }
  void InitializeRoomTransitionPrimitives() override {
    calls.emplace_back("transition-primitives");
  }
  void InitializeFramebufferEffect() override {
    calls.emplace_back("framebuffer-effect");
  }
  void InitializeScriptOpcodeTable() override {
    calls.emplace_back("script-opcodes");
  }
  void InitializeEffectRuntime() override {
    calls.emplace_back("effect-runtime");
  }
  void InitializeRoomRuntimeState() override {
    calls.emplace_back("room-runtime");
  }
  void InitializeActorModelResources() override {
    calls.emplace_back("actor-model-resources");
  }
  void InitializeTexturePages() override {
    calls.emplace_back("texture-pages");
  }
  void ResetOverlayWorkspaceOwner() override {
    calls.emplace_back("workspace-owner");
  }
  void LoadSystemResources() override {
    calls.emplace_back("system-resources");
  }
  void InitializeActorAiHook() override { calls.emplace_back("actor-ai"); }
  void ResetVibrationPatterns() override { calls.emplace_back("vibration"); }
  void ApplyLearnedActionUnlocks() override {
    calls.emplace_back("action-unlocks");
  }
  void ApplyGameplayDefaults(
      const VS::Boot::GameplayOverlayDefaults& defaults) override {
    calls.emplace_back("gameplay-defaults");
    gameplay_defaults = defaults;
  }
  void SetRoomActorScale(const VS::Core::Math::Vec3& scale) override {
    calls.emplace_back("room-actor-scale");
    room_actor_scale = scale;
  }
  void ClearActorSlots(std::size_t slot_count) override {
    calls.emplace_back("actor-slots");
    actor_slot_count = slot_count;
  }
  void SpawnAshleyWithEquipment() override {
    calls.emplace_back("spawn-ashley");
  }
  void AllocateBattleActionState() override {
    calls.emplace_back("action-state");
  }
  void SetFieldRenderingEnabled(bool enabled) override {
    calls.emplace_back("field-rendering");
    field_rendering_enabled = enabled;
  }
  void ResetGameplayResetWordTwo() override {
    calls.emplace_back("reset-word-two");
  }
  void ConfigureFieldDisplay(
      const VS::Boot::GameplayOverlayDefaults& defaults) override {
    calls.emplace_back("display");
    display_defaults = defaults;
  }
};
}  // namespace

TEST_CASE("session begins and ends in the original order", "[session]") {
  SessionState state;
  RecordingHost host;
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  CHECK(state.active);
  CHECK(host.calls == std::vector<std::string>{"ui", "inspect"});

  host.calls.clear();
  VS::Boot::EndSession(state, host);
  CHECK_FALSE(state.active);
  CHECK(host.calls == std::vector<std::string>{"ui", "save", "inspect"});
}

TEST_CASE("session refuses to begin twice or without entries", "[session]") {
  SessionState state;
  RecordingHost host;
  CHECK_FALSE(VS::Boot::BeginSession(state, host, false));
  CHECK_FALSE(state.active);
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  host.calls.clear();
  CHECK_FALSE(VS::Boot::BeginSession(state, host, true));
  CHECK(host.calls.empty());
}

TEST_CASE("session boundary values select the entry", "[session]") {
  CHECK(VS::Boot::EntryFor(0) == SessionDestination::kTitle);
  CHECK(VS::Boot::EntryFor(1) == SessionDestination::kTitle);
  CHECK(VS::Boot::EntryFor(2) == SessionDestination::kGameLoop);
}

TEST_CASE("title transition resets the save session first", "[session]") {
  SessionState state;
  RecordingHost host;
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  host.calls.clear();
  CHECK(TransferredTo([&] { VS::Boot::JumpToTitle(state, host); }) ==
        SessionDestination::kTitle);
  CHECK(host.calls ==
        std::vector<std::string>{"save", "ui", "inspect", "transfer"});
}

TEST_CASE("game-loop transition keeps the save session", "[session]") {
  SessionState state;
  RecordingHost host;
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  host.calls.clear();
  CHECK(TransferredTo([&] { VS::Boot::JumpToGameLoop(state, host); }) ==
        SessionDestination::kGameLoop);
  CHECK(host.calls == std::vector<std::string>{"ui", "inspect", "transfer"});
}

TEST_CASE("transitions without a session are refused untouched", "[session]") {
  SessionState state;
  RecordingHost host;
  CHECK(RefusedWith([&] { VS::Boot::JumpToTitle(state, host); }) ==
        SessionRefusal::kNoSession);
  CHECK(RefusedWith([&] { VS::Boot::JumpToGameLoop(state, host); }) ==
        SessionRefusal::kNoSession);
  CHECK(host.calls == std::vector<std::string>{"refuse", "refuse"});
}

TEST_CASE("transitions inside a barrier are refused untouched", "[session]") {
  SessionState state;
  RecordingHost host;
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  host.calls.clear();
  {
    const SessionTransitionBarrier barrier{state};
    CHECK(RefusedWith([&] { VS::Boot::JumpToTitle(state, host); }) ==
          SessionRefusal::kInsideBarrier);
    CHECK(RefusedWith([&] { VS::Boot::JumpToGameLoop(state, host); }) ==
          SessionRefusal::kInsideBarrier);
  }
  CHECK(host.calls == std::vector<std::string>{"refuse", "refuse"});
  CHECK(state.active);
  CHECK(state.barrier_depth == 0);
  CHECK(TransferredTo([&] { VS::Boot::JumpToGameLoop(state, host); }) ==
        SessionDestination::kGameLoop);
}

TEST_CASE("session barriers nest", "[session]") {
  SessionState state;
  {
    const SessionTransitionBarrier outer{state};
    CHECK(state.barrier_depth == 1);
    {
      const SessionTransitionBarrier inner{state};
      CHECK(state.barrier_depth == 2);
    }
    CHECK(state.barrier_depth == 1);
  }
  CHECK(state.barrier_depth == 0);
}

TEST_CASE("title session runs in the original order", "[session]") {
  RecordingTitleHost host;
  CHECK(VS::Boot::RunTitleAndBattle(host) == 9);
  CHECK(host.entry_mode == 3);
  CHECK(host.calls == std::vector<std::string>{"init", "overlay", "title",
                                               "mode", "clear", "loading",
                                               "battle", "game"});
}

TEST_CASE("game initializes the platform before sessions", "[session]") {
  RecordingGameHost host;
  VS::Boot::RunGame(host);
  CHECK(host.calls == std::vector<std::string>{"platform", "sessions"});
}

TEST_CASE("overlay loading uses typed programs and original slots",
          "[session]") {
  RecordingLifecycleHost host;
  VS::Boot::LoadBattleOverlays(host);
  CHECK(host.calls ==
        std::vector<std::string>{"load-battle-0", "load-battle-init-1"});

  host.calls.clear();
  VS::Boot::LoadTitleOverlay(host);
  CHECK(host.calls == std::vector<std::string>{"load-title-0"});

  host.calls.clear();
  VS::Boot::LoadEndingOverlay(host);
  CHECK(host.calls == std::vector<std::string>{"load-ending-0"});
}

TEST_CASE("title reset preserves teardown order", "[session]") {
  RecordingLifecycleHost host;
  VS::Boot::ResetToTitle(host);
  CHECK(host.calls == std::vector<std::string>{"reset-session", "hide",
                                               "stop-motors", "refresh-input",
                                               "shutdown-audio", "vsync-10",
                                               "jump-title"});
}

TEST_CASE("battle entry loads overlays before requesting the game loop",
          "[session]") {
  RecordingLifecycleHost host;
  VS::Boot::EnterBattle(host, host);
  CHECK(host.calls == std::vector<std::string>{
                          "hide", "stop-motors", "refresh-input", "stop-music",
                          "stop-scene-sfx", "invalidate-music",
                          "clear-music-banks", "loading", "load-battle-0",
                          "load-battle-init-1", "entry-1", "jump-game"});
}

TEST_CASE("ending cleanup returns to title after staff roll", "[session]") {
  RecordingLifecycleHost host;
  VS::Boot::RunEndingAndReturnToTitle(host, host);
  CHECK(host.calls == std::vector<std::string>{
                          "gameplay-off", "hide", "stop-motors",
                          "refresh-input", "stop-music", "stop-scene-sfx",
                          "capture-ashley", "load-ending-0", "staff-roll",
                          "hide", "stop-motors", "refresh-input",
                          "shutdown-audio", "clear-save-on", "jump-title"});
}

TEST_CASE("platform initialization enables reset after services start",
          "[session]") {
  RecordingLifecycleHost host;
  VS::Boot::SoftResetState state;
  VS::Boot::InitializePlatformServices(state, host);
  CHECK(state.enabled);
  CHECK(host.calls == std::vector<std::string>{
                          "ntsc", "hide", "init-card", "start-card",
                          "init-controllers", "start-comms", "unlock-analog",
                          "reset-vibration", "reverb-off", "init-cd",
                          "init-random", "soft-reset-on", "clear-save"});

  VS::Boot::SetSoftResetEnabled(state, false);
  CHECK_FALSE(state.enabled);
}

TEST_CASE("title session initialization resets state in order", "[session]") {
  RecordingLifecycleHost host;
  VS::Boot::InitializeTitleSession(host);
  CHECK(host.calls == std::vector<std::string>{
                          "reset-session", "clear-framebuffer", "loading",
                          "reverb-on", "vsync-callback", "reset-heap",
                          "invalidate-ai", "init-queue", "reset-cd",
                          "init-menu-audio", "reset-vibration", "gameplay-off",
                          "unlock-analog", "reset-held-frames",
                          "reset-play-time", "clear-ashley-pending"});
}

TEST_CASE("gameplay overlay initialization preserves order and defaults",
          "[session]") {
  RecordingGameplayOverlayHost host;
  VS::Boot::InitializeGameplayOverlay(host);

  CHECK(host.calls == std::vector<std::string>{"audio",
                                               "heap",
                                               "ordering-tables",
                                               "palette-animation",
                                               "transition-primitives",
                                               "framebuffer-effect",
                                               "script-opcodes",
                                               "effect-runtime",
                                               "room-runtime",
                                               "actor-model-resources",
                                               "texture-pages",
                                               "workspace-owner",
                                               "system-resources",
                                               "actor-ai",
                                               "vibration",
                                               "action-unlocks",
                                               "gameplay-defaults",
                                               "room-actor-scale",
                                               "actor-slots",
                                               "spawn-ashley",
                                               "action-state",
                                               "field-rendering",
                                               "reset-word-two",
                                               "display"});
  CHECK(host.ordering_defaults == VS::Boot::GameplayOverlayDefaults{});
  CHECK(host.gameplay_defaults == VS::Boot::GameplayOverlayDefaults{});
  CHECK(host.display_defaults == VS::Boot::GameplayOverlayDefaults{});
  CHECK(host.room_actor_scale ==
        VS::Core::Math::Vec3{.x = 0x71C, .y = 0x71C, .z = 0x71C});
  CHECK(host.actor_slot_count == 16);
  CHECK(host.field_rendering_enabled);
}
