// Hand-maintained (tools/port/owned.csv).
// Live binding for the scene timer (State/SceneTimer.hpp): the timer record,
// the tick speed, the script flags, the puzzle timer HUD, the BIOS random
// sequence and the room triggers.
#include "State/SceneTimerLive.hpp"

#include <cstdint>

#include "State/SceneTimer.hpp"
#include "State/ScriptFlags.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Battle/Hud.h"
#include "Core/BiosRand.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Event/Triggers.h"
#include "State/SceneTimer.h"
#include "State/Timers.h"

// decomp: D_80050468 (D_80050468@SLUS_010.40)
State_SceneTimer State_sceneTimer;

// decomp: vs_gametime_tickspeed (vs_gametime_tickspeed@SLUS_010.40)
int State_tickSpeed;
}

namespace VS::State {
namespace {
class LiveSceneTimerHost final : public SceneTimerHost {
 public:
  void ShowTimer() override { Battle_ShowPuzzleTimer(); }
  void HideTimer() override { Battle_HidePuzzleTimer(); }
  // Core_Random_Rand keeps the RNG trace of the BIOS sequence.
  int Random() override { return Core_Random_Rand(); }
};
}  // namespace

SceneTimer LiveSceneTimer() noexcept {
  const State_SceneTimer& record = State_sceneTimer;
  return SceneTimer{.mode = static_cast<std::uint8_t>(record.mode),
                    .paused_mode = static_cast<std::uint8_t>(record.pausedMode),
                    .reset_scope = record.resetScope,
                    .time = SceneTime{.frames = record.frames,
                                      .seconds = record.seconds,
                                      .minutes = record.minutes,
                                      .hours = record.hours}};
}

void StoreLiveSceneTimer(const SceneTimer& timer) noexcept {
  State_SceneTimer& record = State_sceneTimer;
  record.mode = static_cast<char>(timer.mode);
  record.pausedMode = static_cast<char>(timer.paused_mode);
  record.resetScope = timer.reset_scope;
  record.frames = timer.time.frames;
  record.seconds = timer.time.seconds;
  record.minutes = timer.time.minutes;
  record.hours = timer.time.hours;
}

void StartLiveSceneTimer(int mode, int reset_scope, int packed_time) {
  SceneTimer timer = LiveSceneTimer();
  ScriptFlags flags = LiveScriptFlags();
  LiveSceneTimerHost host;
  StartSceneTimer(timer, mode, reset_scope, packed_time, flags, host);
  StoreLiveSceneTimer(timer);
}

void ResetLiveSceneTimer() {
  SceneTimer timer = LiveSceneTimer();
  ScriptFlags flags = LiveScriptFlags();
  LiveSceneTimerHost host;
  ResetSceneTimer(timer, flags, host);
  StoreLiveSceneTimer(timer);
}

void SetLiveSceneTimerPaused(int paused) {
  SceneTimer timer = LiveSceneTimer();
  LiveSceneTimerHost host;
  SetSceneTimerPaused(timer, paused != 0, host);
  StoreLiveSceneTimer(timer);
}

int SetLiveTickSpeed(int requested_tick_speed) {
  State_tickSpeed = SelectTickSpeed(State_tickSpeed, requested_tick_speed);
  return State_tickSpeed;
}

int UpdateLiveSceneTimer() {
  SceneTimer timer = LiveSceneTimer();
  ScriptFlags flags = LiveScriptFlags();
  LiveSceneTimerHost host;
  const bool expiry_due =
      AdvanceSceneTimer(timer, State_tickSpeed, flags, host);
  StoreLiveSceneTimer(timer);
  if (!expiry_due) return 0;
  return Event_EvaluateRoomEventTriggersEntry(kSceneTimerExpiryTrigger, 0,
                                              nullptr, 0) == kExpiryTriggerFired
             ? 1
             : 0;
}
}  // namespace VS::State
