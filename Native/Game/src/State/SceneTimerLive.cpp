// Hand-maintained (tools/port/owned.csv).
// Live binding for the scene timer (State/SceneTimer.hpp): the timer record,
// the tick speed, the script flags, the puzzle timer HUD, the BIOS random
// sequence and the room triggers.
#include "State/SceneTimerLive.hpp"

#include <cstdint>

#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Core/BiosRand.h"
#include "Event/RoomTriggerEntryLive.hpp"
#include "Event/Triggers.h"
#include "State/SceneTimer.h"
#include "State/SceneTimer.hpp"
#include "State/ScriptFlags.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/Timers.hpp"

// decomp: D_80050468 (D_80050468@SLUS_010.40)
State_SceneTimer VS::State::sceneTimer;

// decomp: vs_gametime_tickspeed (vs_gametime_tickspeed@SLUS_010.40)
int VS::State::tickSpeed;

namespace VS::State {
namespace {
class LiveSceneTimerHost final : public SceneTimerHost {
 public:
  void ShowTimer() override { VS::Battle::ShowLivePuzzleTimer(); }
  void HideTimer() override { VS::Battle::HideLivePuzzleTimer(); }
  // Core_Random_Rand keeps the RNG trace of the BIOS sequence.
  int Random() override { return Core_Random_Rand(); }
};
}  // namespace

SceneTimer LiveSceneTimer() noexcept {
  const State_SceneTimer& record = sceneTimer;
  return SceneTimer{.mode = static_cast<std::uint8_t>(record.mode),
                    .paused_mode = static_cast<std::uint8_t>(record.pausedMode),
                    .reset_scope = record.resetScope,
                    .time = SceneTime{.frames = record.frames,
                                      .seconds = record.seconds,
                                      .minutes = record.minutes,
                                      .hours = record.hours}};
}

void StoreLiveSceneTimer(const SceneTimer& timer) noexcept {
  State_SceneTimer& record = sceneTimer;
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
  tickSpeed = SelectTickSpeed(tickSpeed, requested_tick_speed);
  return tickSpeed;
}

int UpdateLiveSceneTimer() {
  SceneTimer timer = LiveSceneTimer();
  ScriptFlags flags = LiveScriptFlags();
  LiveSceneTimerHost host;
  const bool expiry_due = AdvanceSceneTimer(timer, tickSpeed, flags, host);
  StoreLiveSceneTimer(timer);
  if (!expiry_due) return 0;
  return VS::Event::EvaluateRoomEventTriggersEntry(
             kSceneTimerExpiryTrigger, 0, nullptr, 0) == kExpiryTriggerFired
             ? 1
             : 0;
}
}  // namespace VS::State
