// Hand-maintained (tools/port/owned.csv).
#include "Boot/Session.hpp"

namespace VS::Boot {
namespace {
// Checked before any state changes, so a refused request leaves the session
// exactly as it was.
void CheckTransition(const SessionState& state, SessionHost& host) {
  if (!state.active) host.Refuse(SessionRefusal::kNoSession);
  if (state.barrier_depth > 0) host.Refuse(SessionRefusal::kInsideBarrier);
}

[[noreturn]] void RequestSession(SessionHost& host,
                                 SessionDestination destination) {
  host.InvalidateSceneUi();
  host.ResetGameplayInspection();
  host.Transfer(destination);
}
}  // namespace

bool BeginSession(SessionState& state, SessionHost& host,
                  bool entries_present) {
  if (state.active || !entries_present) return false;
  host.InvalidateSceneUi();
  state.active = true;
  host.ResetGameplayInspection();
  return true;
}

SessionDestination EntryFor(int boundary_value) {
  return boundary_value == static_cast<int>(SessionDestination::kGameLoop)
             ? SessionDestination::kGameLoop
             : SessionDestination::kTitle;
}

void EndSession(SessionState& state, SessionHost& host) {
  host.InvalidateSceneUi();
  host.ResetNativeSession();
  state.active = false;
  host.ResetGameplayInspection();
}

SessionTransitionBarrier::SessionTransitionBarrier(SessionState& state)
    : state_(state) {
  ++state_.barrier_depth;
}

SessionTransitionBarrier::~SessionTransitionBarrier() {
  --state_.barrier_depth;
}

void JumpToTitle(SessionState& state, SessionHost& host) {
  CheckTransition(state, host);
  host.ResetNativeSession();
  RequestSession(host, SessionDestination::kTitle);
}

void JumpToGameLoop(SessionState& state, SessionHost& host) {
  CheckTransition(state, host);
  RequestSession(host, SessionDestination::kGameLoop);
}

int RunTitleAndBattle(TitleSessionHost& host) {
  host.InitializeTitleSession();
  host.LoadTitleOverlay();
  host.SetGameEntryMode(host.RunTitle());
  host.ClearPendingClearSave();
  host.ShowLoadingScreen();
  host.LoadBattleOverlays();
  return host.RunLoadedGame();
}

void RunGame(GameHost& host) {
  host.InitializePlatformServices();
  host.RunSessions();
}

void DelayAfterOverlayLoad() {}
}  // namespace VS::Boot
