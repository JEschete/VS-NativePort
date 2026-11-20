// Hand-maintained (tools/port/owned.csv).
// Live binding for the scene timer (State/SceneTimer.hpp): the timer,
// the tick speed, the script flags, the puzzle timer HUD, the BIOS random
// sequence and the room triggers.
#include "State/SceneTimerLive.hpp"

#include "Battle/Hud.hpp"
#include "Battle/HudStateLive.hpp"
#include "Core/RandomLive.hpp"
#include "Event/RoomTriggerEntryLive.hpp"
#include "Event/Triggers.hpp"
#include "State/SceneTimer.hpp"
#include "State/ScriptFlags.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/Timers.hpp"

// decomp: D_80050468 (D_80050468@SLUS_010.40)
VS::State::SceneTimer VS::State::sceneTimer;

// decomp: vs_gametime_tickspeed (vs_gametime_tickspeed@SLUS_010.40)
int VS::State::tickSpeed;

namespace VS::State {
namespace {
class LiveSceneTimerHost final : public SceneTimerHost {
 public:
  void ShowTimer() override { VS::Battle::ShowLivePuzzleTimer(); }
  void HideTimer() override { VS::Battle::HideLivePuzzleTimer(); }
  // VS::Core::NextBiosRand keeps the RNG trace of the BIOS sequence.
  int Random() override { return VS::Core::NextBiosRand(); }
};
}  // namespace

void StartLiveSceneTimer(int mode, int reset_scope, int packed_time) {
  ScriptFlags flags = LiveScriptFlags();
  LiveSceneTimerHost host;
  StartSceneTimer(sceneTimer, mode, reset_scope, packed_time, flags, host);
}

void ResetLiveSceneTimer() {
  ScriptFlags flags = LiveScriptFlags();
  LiveSceneTimerHost host;
  ResetSceneTimer(sceneTimer, flags, host);
}

void SetLiveSceneTimerPaused(int paused) {
  LiveSceneTimerHost host;
  SetSceneTimerPaused(sceneTimer, paused != 0, host);
}

int SetLiveTickSpeed(int requested_tick_speed) {
  tickSpeed = SelectTickSpeed(tickSpeed, requested_tick_speed);
  return tickSpeed;
}

int UpdateLiveSceneTimer() {
  ScriptFlags flags = LiveScriptFlags();
  LiveSceneTimerHost host;
  const bool expiry_due = AdvanceSceneTimer(sceneTimer, tickSpeed, flags, host);
  if (!expiry_due) return 0;
  return VS::Event::EvaluateRoomEventTriggersEntry(
             kSceneTimerExpiryTrigger, 0, nullptr, 0) == kExpiryTriggerFired
             ? 1
             : 0;
}
}  // namespace VS::State
