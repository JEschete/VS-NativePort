#include <array>
#include <cstddef>
#include <exception>
#include <functional>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Boot/Session.hpp"
#include "Platform/GameFiber.hpp"

namespace {
using VS::Boot::Mode;
using VS::Boot::SessionRefusal;
using VS::Boot::SessionState;
using Calls = std::vector<std::string>;

struct Refused {
  SessionRefusal refusal;
};

class RecordingHost final : public VS::Boot::SessionHost {
 public:
  Calls calls;

  void InvalidateSceneUi() override { calls.emplace_back("ui"); }
  void ResetGameplayInspection() override { calls.emplace_back("inspect"); }
  void ResetNativeSession() override { calls.emplace_back("save"); }
  void AbandonInterruptedWork() override { calls.emplace_back("abandon"); }
  void EnterMode(Mode mode) override {
    calls.emplace_back(std::string("enter-") + VS::Boot::ModeName(mode));
  }
  [[noreturn]] void Refuse(SessionRefusal refusal) override {
    calls.emplace_back("refuse");
    throw Refused{refusal};
  }
};

// Each mode's run: the mode and how many times it ran before.
using Script = std::function<std::optional<Mode>(Mode, int)>;

class ScriptedModules final : public VS::Boot::ModeModules {
 public:
  explicit ScriptedModules(Script script) : script_(std::move(script)) {}
  VS::Boot::ModeModule& For(Mode mode) override {
    return modules_.at(static_cast<std::size_t>(mode));
  }

 private:
  class Module final : public VS::Boot::ModeModule {
   public:
    Module(const Script& script, Mode mode) : script_(script), mode_(mode) {}
    std::optional<Mode> Run() override { return script_(mode_, runs_++); }

   private:
    const Script& script_;
    Mode mode_;
    int runs_ = 0;
  };

  Script script_;
  std::array<Module, 4> modules_{
      Module{script_, Mode::kBoot}, Module{script_, Mode::kTitle},
      Module{script_, Mode::kGameplay}, Module{script_, Mode::kEnding}};
};

// A mode's local whose destructor reports the unwind.
class Frame {
 public:
  Frame(Calls& calls, std::string name)
      : calls_(calls), name_(std::move(name)) {}
  ~Frame() {
    try {
      calls_.emplace_back("unwind-" + name_);
    } catch (...) {
      std::terminate();
    }
  }
  Frame(const Frame&) = delete;
  Frame& operator=(const Frame&) = delete;
  Frame(Frame&&) = delete;
  Frame& operator=(Frame&&) = delete;

 private:
  Calls& calls_;
  std::string name_;
};

template <typename Jump>
SessionRefusal RefusedWith(Jump jump) {
  try {
    jump();
  } catch (const Refused& refused) {
    return refused.refusal;
  } catch (const VS::Boot::SessionTransfer&) {
    FAIL("the jump transferred");
  }
  FAIL("the jump returned");
  return SessionRefusal::kNoSession;
}

Calls SessionBegin() { return {"ui", "inspect"}; }
Calls SessionEnd() { return {"ui", "save", "inspect"}; }

Calls Session(std::initializer_list<std::string> body) {
  Calls calls = SessionBegin();
  calls.insert(calls.end(), body);
  const Calls end = SessionEnd();
  calls.insert(calls.end(), end.begin(), end.end());
  return calls;
}

class RecordingBootHost final : public VS::Boot::BootHost {
 public:
  Calls calls;
  void InitializePlatformServices() override { calls.emplace_back("platform"); }
};

class RecordingTitleHost final : public VS::Boot::TitleSessionHost {
 public:
  Calls calls;
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
};

// The session a game fiber runs; the fiber entry has no parameters.
struct FiberSession {
  SessionState state;
  RecordingHost host;
};
FiberSession* fiberSession = nullptr;

void RunFiberSession() {
  ScriptedModules modules([](Mode mode, int) -> std::optional<Mode> {
    if (mode != Mode::kGameplay) return Mode::kGameplay;
    const Frame frame(fiberSession->host.calls, "gameplay");
    while (true) VS::Platform::GameFiber::YieldToHost();
  });
  if (!VS::Boot::RunSession(fiberSession->state, fiberSession->host, modules,
                            Mode::kBoot)) {
    fiberSession->host.calls.emplace_back("not-started");
  }
}

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
  void WaitTicks(int count) override {
    calls.emplace_back("wait-" + std::to_string(count));
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
  void JumpToEnding() override { calls.emplace_back("jump-ending"); }
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
  void ConfigureFieldDisplay(
      const VS::Boot::GameplayOverlayDefaults& defaults) override {
    calls.emplace_back("display");
    display_defaults = defaults;
  }
};
}  // namespace

TEST_CASE("session runs its modes until one ends it", "[session]") {
  SessionState state;
  RecordingHost host;
  ScriptedModules modules([&](Mode mode, int) -> std::optional<Mode> {
    CHECK(state.active);
    CHECK(state.mode == mode);
    if (mode == Mode::kBoot) return Mode::kTitle;
    if (mode == Mode::kTitle) return Mode::kGameplay;
    return std::nullopt;
  });
  REQUIRE(VS::Boot::RunSession(state, host, modules, Mode::kBoot));
  CHECK_FALSE(state.active);
  CHECK(host.calls == Session({"enter-boot", "enter-title", "enter-gameplay"}));
}

TEST_CASE("session refuses to start inside a running session", "[session]") {
  SessionState state;
  RecordingHost host;
  ScriptedModules inner([](Mode, int) -> std::optional<Mode> {
    FAIL("the nested session ran a mode");
    return std::nullopt;
  });
  ScriptedModules outer([&](Mode, int) -> std::optional<Mode> {
    CHECK_FALSE(VS::Boot::RunSession(state, host, inner, Mode::kTitle));
    CHECK(state.active);
    return std::nullopt;
  });
  REQUIRE(VS::Boot::RunSession(state, host, outer, Mode::kGameplay));
  CHECK(host.calls == Session({"enter-gameplay"}));
}

TEST_CASE("title transfer unwinds the mode before resetting the save session",
          "[session]") {
  SessionState state;
  RecordingHost host;
  ScriptedModules modules([&](Mode mode, int runs) -> std::optional<Mode> {
    if (mode == Mode::kTitle) {
      return runs == 0 ? std::optional<Mode>(Mode::kGameplay) : std::nullopt;
    }
    const Frame frame(host.calls, "gameplay");
    VS::Boot::JumpToTitle(state, host);
  });
  REQUIRE(VS::Boot::RunSession(state, host, modules, Mode::kTitle));
  CHECK(host.calls ==
        Session({"enter-title", "enter-gameplay", "unwind-gameplay", "abandon",
                 "save", "ui", "inspect", "enter-title"}));
}

TEST_CASE("game-loop transfer keeps the save session", "[session]") {
  SessionState state;
  RecordingHost host;
  ScriptedModules modules([&](Mode, int runs) -> std::optional<Mode> {
    if (runs == 0) VS::Boot::JumpToGameLoop(state, host);
    return std::nullopt;
  });
  REQUIRE(VS::Boot::RunSession(state, host, modules, Mode::kGameplay));
  CHECK(host.calls == Session({"enter-gameplay", "abandon", "ui", "inspect",
                               "enter-gameplay"}));
}

TEST_CASE("ending transfer prepares nothing and returns by the title",
          "[session]") {
  SessionState state;
  RecordingHost host;
  ScriptedModules modules([&](Mode mode, int runs) -> std::optional<Mode> {
    if (mode == Mode::kGameplay) VS::Boot::JumpToEnding(state, host);
    if (mode == Mode::kEnding) VS::Boot::JumpToTitle(state, host);
    return runs == 0 ? std::optional<Mode>(Mode::kGameplay) : std::nullopt;
  });
  REQUIRE(VS::Boot::RunSession(state, host, modules, Mode::kTitle));
  CHECK(host.calls ==
        Session({"enter-title", "enter-gameplay", "abandon", "enter-ending",
                 "abandon", "save", "ui", "inspect", "enter-title"}));
}

TEST_CASE("repeated transfers stay in one session", "[session]") {
  SessionState state;
  RecordingHost host;
  int titles = 0;
  ScriptedModules modules([&](Mode mode, int runs) -> std::optional<Mode> {
    if (mode == Mode::kTitle) {
      ++titles;
      return Mode::kGameplay;
    }
    if (runs < 3) VS::Boot::JumpToTitle(state, host);
    return std::nullopt;
  });
  REQUIRE(VS::Boot::RunSession(state, host, modules, Mode::kTitle));
  CHECK(titles == 4);
  CHECK_FALSE(state.active);
  CHECK(Calls(host.calls.begin(), host.calls.begin() + 2) == SessionBegin());
  CHECK(Calls(host.calls.end() - 3, host.calls.end()) == SessionEnd());
}

TEST_CASE("an exception unwinds the mode and ends the session", "[session]") {
  SessionState state;
  RecordingHost host;
  ScriptedModules failing([&](Mode, int) -> std::optional<Mode> {
    const Frame frame(host.calls, "gameplay");
    throw std::runtime_error("mode failed");
  });
  CHECK_THROWS_AS(VS::Boot::RunSession(state, host, failing, Mode::kGameplay),
                  std::runtime_error);
  CHECK_FALSE(state.active);
  CHECK(host.calls == Session({"enter-gameplay", "unwind-gameplay"}));

  host.calls.clear();
  ScriptedModules next(
      [](Mode, int) -> std::optional<Mode> { return std::nullopt; });
  CHECK(VS::Boot::RunSession(state, host, next, Mode::kTitle));
  CHECK(host.calls == Session({"enter-title"}));
}

TEST_CASE("transitions without a session are refused untouched", "[session]") {
  SessionState state;
  RecordingHost host;
  CHECK(RefusedWith([&] { VS::Boot::JumpToTitle(state, host); }) ==
        SessionRefusal::kNoSession);
  CHECK(RefusedWith([&] { VS::Boot::JumpToGameLoop(state, host); }) ==
        SessionRefusal::kNoSession);
  CHECK(RefusedWith([&] { VS::Boot::JumpToEnding(state, host); }) ==
        SessionRefusal::kNoSession);
  CHECK(host.calls == Calls{"refuse", "refuse", "refuse"});
}

TEST_CASE("cancelling the game fiber unwinds the mode and ends the session",
          "[session][fiber]") {
  FiberSession session;
  fiberSession = &session;
  {
    VS::Platform::GameFiber fiber(&RunFiberSession);
    REQUIRE(fiber.Resume());
    CHECK(session.state.active);
    CHECK(session.state.mode == Mode::kGameplay);
    CHECK(session.host.calls ==
          Calls{"ui", "inspect", "enter-boot", "enter-gameplay"});
    session.host.calls.clear();
  }
  fiberSession = nullptr;
  CHECK_FALSE(session.state.active);
  CHECK(session.host.calls ==
        Calls{"unwind-gameplay", "ui", "save", "inspect"});
}

TEST_CASE("boot initializes the platform before the title", "[session]") {
  RecordingBootHost host;
  CHECK(VS::Boot::RunBootMode(host) == Mode::kTitle);
  CHECK(host.calls == Calls{"platform"});
}

TEST_CASE("title mode runs in the original order", "[session]") {
  RecordingTitleHost host;
  CHECK(VS::Boot::RunTitleMode(host) == Mode::kGameplay);
  CHECK(host.entry_mode == 3);
  CHECK(host.calls == Calls{"init", "overlay", "title", "mode", "clear",
                            "loading", "battle"});
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
                                               "shutdown-audio", "wait-10",
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

TEST_CASE("ending leaves the game loop and returns to title after staff roll",
          "[session]") {
  RecordingLifecycleHost host;
  VS::Boot::RunEndingAndReturnToTitle(host);
  CHECK(host.calls == Calls{"gameplay-off", "hide", "stop-motors",
                            "refresh-input", "stop-music", "stop-scene-sfx",
                            "capture-ashley", "jump-ending"});

  host.calls.clear();
  VS::Boot::RunEndingMode(host, host);
  CHECK(host.calls == Calls{"load-ending-0", "staff-roll", "hide",
                            "stop-motors", "refresh-input", "shutdown-audio",
                            "clear-save-on", "jump-title"});
}

TEST_CASE("platform initialization enables reset after services start",
          "[session]") {
  RecordingLifecycleHost host;
  VS::Boot::SoftResetState state;
  VS::Boot::InitializePlatformServices(state, host);
  CHECK(state.enabled);
  CHECK(host.calls == std::vector<std::string>{
                          "hide", "init-card", "start-card", "init-controllers",
                          "start-comms", "unlock-analog", "reset-vibration",
                          "reverb-off", "init-cd", "init-random",
                          "soft-reset-on", "clear-save"});

  VS::Boot::SetSoftResetEnabled(state, false);
  CHECK_FALSE(state.enabled);
}

TEST_CASE("title session initialization resets state in order", "[session]") {
  RecordingLifecycleHost host;
  VS::Boot::InitializeTitleSession(host);
  CHECK(host.calls == std::vector<std::string>{
                          "reset-session", "clear-framebuffer", "loading",
                          "reverb-on", "reset-heap", "invalidate-ai",
                          "init-queue", "reset-cd", "init-menu-audio",
                          "reset-vibration", "gameplay-off", "unlock-analog",
                          "reset-held-frames", "reset-play-time",
                          "clear-ashley-pending"});
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
                                               "display"});
  CHECK(host.ordering_defaults == VS::Boot::GameplayOverlayDefaults{});
  CHECK(host.gameplay_defaults == VS::Boot::GameplayOverlayDefaults{});
  CHECK(host.display_defaults == VS::Boot::GameplayOverlayDefaults{});
  CHECK(host.room_actor_scale ==
        VS::Core::Math::Vec3{.x = 0x71C, .y = 0x71C, .z = 0x71C});
  CHECK(host.actor_slot_count == 16);
  CHECK(host.field_rendering_enabled);
}
