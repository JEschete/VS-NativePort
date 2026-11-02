#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// Native title and gameplay sessions. The original resets the stack pointer to
// a saved checkpoint to abandon the current call stack (overlay.c:20-21). The
// native session loop keeps that as a nonlocal transition to the session
// boundary, which on this toolchain runs no destructors in the abandoned
// frames (#266). The policy below decides whether a transition may happen;
// the host performs it, so the core reaches no global state.
namespace VS::Boot {
enum class SessionDestination : std::uint8_t { kTitle = 1, kGameLoop = 2 };

enum class SessionRefusal : std::uint8_t {
  // A transition requested with no session loop running.
  kNoSession,
  // A transition requested while a scope that binds globals to stack storage
  // is live; jumping would leave those globals pointing at a dead frame.
  kInsideBarrier,
};

struct SessionState {
  bool active = false;
  int barrier_depth = 0;
};

class SessionHost {
 public:
  virtual ~SessionHost() = default;
  // Drops text-box and notification borrows of the scene heap.
  virtual void InvalidateSceneUi() = 0;
  virtual void ResetGameplayInspection() = 0;
  virtual void ResetNativeSession() = 0;
  // Returns to the session boundary with `destination`; never returns.
  [[noreturn]] virtual void Transfer(SessionDestination destination) = 0;
  // Reports the refused transition and stops; never returns.
  [[noreturn]] virtual void Refuse(SessionRefusal refusal) = 0;
};

// Starts a session; false if one is running or an entry is missing.
[[nodiscard]] bool BeginSession(SessionState& state, SessionHost& host,
                                bool entries_present);
// The entry for the value the session boundary resumed with (0 on entry).
[[nodiscard]] SessionDestination EntryFor(int boundary_value);
void EndSession(SessionState& state, SessionHost& host);

// RAII barrier: a transition requested while one is held is refused.
class SessionTransitionBarrier {
 public:
  explicit SessionTransitionBarrier(SessionState& state);
  ~SessionTransitionBarrier();
  SessionTransitionBarrier(const SessionTransitionBarrier&) = delete;
  SessionTransitionBarrier& operator=(const SessionTransitionBarrier&) = delete;
  SessionTransitionBarrier(SessionTransitionBarrier&&) = delete;
  SessionTransitionBarrier& operator=(SessionTransitionBarrier&&) = delete;

 private:
  SessionState& state_;
};

// SLUS_010.40 vs_overlay_jumpToTitle (overlay.c:20): resets the native save
// session, then returns to the title entry.
[[noreturn]] void JumpToTitle(SessionState& state, SessionHost& host);
// SLUS_010.40 vs_overlay_jumpToBattle (overlay.c:21).
[[noreturn]] void JumpToGameLoop(SessionState& state, SessionHost& host);

class TitleSessionHost {
 public:
  virtual ~TitleSessionHost() = default;
  virtual void InitializeTitleSession() = 0;
  virtual void LoadTitleOverlay() = 0;
  // Runs the title module; returns the game entry mode it chose.
  virtual int RunTitle() = 0;
  virtual void SetGameEntryMode(int mode) = 0;
  virtual void ClearPendingClearSave() = 0;
  virtual void ShowLoadingScreen() = 0;
  virtual void LoadBattleOverlays() = 0;
  virtual int RunLoadedGame() = 0;
};

// SLUS_010.40 vs_main_execTitle (main.c:8344).
int RunTitleAndBattle(TitleSessionHost& host);

class GameHost {
 public:
  virtual ~GameHost() = default;
  virtual void InitializePlatformServices() = 0;
  virtual void RunSessions() = 0;
};

// SLUS_010.40 vs_main_exec (main.c:8361).
void RunGame(GameHost& host);

// SLUS_010.40 vs_overlay_wait (overlay.c:23): the original spins 4096 NOPs
// for the instruction cache after an overlay load; native code needs none.
void DelayAfterOverlayLoad();
}  // namespace VS::Boot
