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
