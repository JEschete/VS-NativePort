// Hand-maintained (tools/port/owned.csv).
// Live binding for the session policy (Session.cpp).
#include "Boot/SessionLive.hpp"

#include <csetjmp>
#include <cstdlib>
#include <cstring>

#include "Render/DisplayControlLive.hpp"

extern "C" {
#include "Audio/Music.h"
#include "Battle/EncounterHooks.h"
#include "Battle/Targeting.h"
#include "Boot/GameLoop.h"
#include "Boot/Lifecycle.h"
#include "Core/Diagnostics.h"
#include "Event/ScriptVm.h"
#include "Legacy/Psx.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Save/NativeResume.h"
#include "Title/TitleMenu.h"
}

namespace VS::Boot {
namespace {
// The boundary is the one place a transition lands; the frames it abandons
// must hold no object whose destructor matters (#266), which the transition
// barrier enforces for the scopes known to need it.
std::jmp_buf session_boundary;
SessionState live_state;

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
  }
  void ResetGameplayInspection() override { Boot_ResetGameplayInspection(); }
  void ResetNativeSession() override { Save_NativeResetSession(); }

  [[noreturn]] void Transfer(SessionDestination destination) override {
    Core_Debugf("boot", "native session transition: %s",
                destination == SessionDestination::kTitle ? "title" : "battle");
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

int RunLoadedGame() {
  Boot_ResetGameplayInspection();
  Boot_isGameplayActive = 1;
  Boot_RunGameLoop();
  Boot_ResetGameplayInspection();
  SetDispMask(0);
  Audio_StopAndFreeCurrentMusic();
  return 0;
}

class LiveTitleSessionHost final : public TitleSessionHost {
 public:
  void InitializeTitleSession() override {
    Core_Debugf("boot", "title session initialization begin");
    Boot_InitializeTitleSession();
    Core_Debugf("boot", "title session initialization complete");
  }
  void LoadTitleOverlay() override {
    Boot_LoadTitleOverlay();
    Core_Debugf("boot", "title module ready");
  }
  int RunTitle() override { return Title_Run(); }
  void SetGameEntryMode(int mode) override { Boot_gameEntryMode = mode; }
  void ClearPendingClearSave() override { Boot_isClearSavePending = 0; }
  void ShowLoadingScreen() override { Render::ShowLiveLoadingScreen(); }
  void LoadBattleOverlays() override { Boot_LoadBattleOverlays(); }
  int RunLoadedGame() override { return VS::Boot::RunLoadedGame(); }
};

class LiveGameHost final : public GameHost {
 public:
  void InitializePlatformServices() override {
    Boot_InitializePlatformServices();
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

void JumpToLiveTitle() { JumpToTitle(live_state, live_host); }

void JumpToLiveGameLoop() { JumpToGameLoop(live_state, live_host); }

LiveSessionTransitionBarrier::LiveSessionTransitionBarrier()
    : barrier_(live_state) {}
}  // namespace VS::Boot
