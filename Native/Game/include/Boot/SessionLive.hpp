#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Boot/Session.hpp"

// The live session loop: the session boundary, scene UI, save session and
// gameplay inspection state behind Boot/Session.hpp.
namespace VS::Boot {
using SessionEntry = int (*)();

// Runs `title` (or `battle` after a game-loop transition) until it returns;
// -1 if a session is already running or an entry is missing. Must run on the
// game fiber.
int RunLiveSessionLoop(SessionEntry title, SessionEntry battle);
int RunLiveTitleAndBattle();
void RunLiveGame();
[[noreturn]] void JumpToLiveTitle();
[[noreturn]] void JumpToLiveGameLoop();
void LoadBattleOverlaysLive();
void LoadTitleOverlayLive();
void LoadEndingOverlayLive();
void WaitForOverlayReadyLive();
void ResetToTitleLive();
void EnterBattleLive();
void RunEndingAndReturnToTitleLive();
void InitializePlatformServicesLive();
void InitializeTitleSessionLive();
void InitializeGameplayOverlayLive(int start_state);
// Runs the gameplay loop over the live field, battle and render state; it
// is left only by a session transition.
void RunGameplayLoopLive();
void SetSoftResetEnabledLive(int enabled);

// Held by scopes that bind globals to stack storage; a session transition
// requested meanwhile is reported and stops the process instead of leaving
// those globals pointing at an abandoned frame (#266).
class LiveSessionTransitionBarrier {
 public:
  LiveSessionTransitionBarrier();

 private:
  SessionTransitionBarrier barrier_;
};
}  // namespace VS::Boot
