#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>

#include "Boot/Session.hpp"

// The live session: the production modes, scene UI, save session and gameplay
// inspection state behind Boot/Session.hpp.
namespace VS::Boot {
using SessionEntry = int (*)();

// Runs a session of the production modes from boot until the game loop
// returns. Must run on the game fiber.
void RunLiveGame();
// Runs a session whose title and gameplay modes are `title` and `battle`,
// from the title, and returns the value of the entry that ended it; -1 if a
// session is already running or an entry is missing. Transitions run as in
// the game (the ending is the production one). Must run on the game fiber.
int RunLiveSessionLoop(SessionEntry title, SessionEntry battle);
// The running session's mode; nullopt with no session running.
[[nodiscard]] std::optional<Mode> LiveSessionMode();
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
// Returns the session's native state to its cold-boot form: the staged resume,
// the file menu and the event, emitter and sphere workspaces. Every session
// boundary reaches it through ResetNativeSession.
void ResetLiveSessionState();
// After the old frame producer stops, drain menu owners and abandon
// frame bindings/packets before heap replacement. Refused cleanup is fatal.
void ResetLiveSessionHeap();
void InitializeGameplayOverlayLive(int start_state);
// Runs the gameplay loop over the live field, battle and render state; it
// is left only by a session transition.
void RunGameplayLoopLive();
void SetSoftResetEnabledLive(int enabled);
}  // namespace VS::Boot
