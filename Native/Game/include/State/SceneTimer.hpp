#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "State/Clock.hpp"
#include "State/ScriptFlags.hpp"

// The scene timer that puzzle rooms and timed events run (decomp: D_80050468,
// SLUS_010.40 main.h:422 D_80050468_t) and the game's tick speed (decomp:
// vs_gametime_tickspeed). Mode 0 is stopped, 1 counts up to one hour, and any
// other mode counts down to zero. Each update mirrors the time into script
// flags 0xA0-0xA3 for the event scripts and the HUD.
namespace VS::State {
inline constexpr std::uint8_t kSceneTimerStopped = 0;
inline constexpr std::uint8_t kSceneTimerCountsUp = 1;
inline constexpr int kSceneTimerFramesFlag = 0xA0;
inline constexpr int kSceneTimerSecondsFlag = 0xA1;
inline constexpr int kSceneTimerMinutesFlag = 0xA2;
inline constexpr int kSceneTimerHoursFlag = 0xA3;
// The frames a tick advances: 2 normally, 4 while slowed.
inline constexpr int kNormalTickSpeed = 2;
inline constexpr int kSlowedTickSpeed = 4;
// The room trigger request an expired countdown evaluates, and its result
// when a trigger fired.
inline constexpr int kSceneTimerExpiryTrigger = 16;
inline constexpr int kExpiryTriggerFired = 1;

using SceneTime = ClockTime;

struct SceneTimer {
  std::uint8_t mode = kSceneTimerStopped;
  // The mode a pause saved, restored on resume.
  std::uint8_t paused_mode = kSceneTimerStopped;
  // Set by the script that starts the timer; the timer does not read it.
  std::uint16_t reset_scope = 0;
  SceneTime time{};

  friend bool operator==(const SceneTimer&, const SceneTimer&) = default;
};

// The HUD and the random source the timer drives.
class SceneTimerHost {
 public:
  SceneTimerHost() = default;
  SceneTimerHost(const SceneTimerHost&) = delete;
  SceneTimerHost& operator=(const SceneTimerHost&) = delete;
  SceneTimerHost(SceneTimerHost&&) = delete;
  SceneTimerHost& operator=(SceneTimerHost&&) = delete;
  virtual ~SceneTimerHost() = default;
  virtual void ShowTimer() = 0;
  virtual void HideTimer() = 0;
  // A 15-bit draw from the BIOS random sequence.
  virtual int Random() = 0;
};

// The countdown time a start packs as frames, seconds, minutes and hours from
// the low byte up, with the original's signed C division.
[[nodiscard]] SceneTime UnpackSceneTime(int packed_time) noexcept;

// decomp: BATTLE.PRG func_8007BBD8 (146C.c:8511). Only a counting-down
// timer takes the packed time; the time is mirrored and the HUD shown.
void StartSceneTimer(SceneTimer& timer, int mode, int reset_scope,
                     int packed_time, ScriptFlags& flags, SceneTimerHost& host);
// decomp: BATTLE.PRG func_8007BCCC (146C.c:8534). Stops and clears the time,
// keeping the paused mode and scope.
void ResetSceneTimer(SceneTimer& timer, ScriptFlags& flags,
                     SceneTimerHost& host);
// decomp: BATTLE.PRG func_8007BD3C (146C.c:8548). A pause saves and stops
// the mode; a resume restores the saved mode. A second pause saves the
// stopped mode.
void SetSceneTimerPaused(SceneTimer& timer, bool paused, SceneTimerHost& host);
// decomp: BATTLE.PRG func_8007C36C (146C.c:8740): the speed after a
// request; only kNormalTickSpeed and kSlowedTickSpeed are taken.
[[nodiscard]] int SelectTickSpeed(int current, int requested) noexcept;
// decomp: BATTLE.PRG func_800882F4 (146C.c:14499), up to its trigger check.
// Advances a running timer by `tick_speed` frames and mirrors it. Returns
// true when its frames are then 0 (a countdown at or past its end, or
// counting up at each whole second): the caller then evaluates the room's
// kSceneTimerExpiryTrigger triggers, after storing the timer, since a fired
// event may restart or reset it. Counting up stops at one hour. A countdown
// whose frames never land on 0 (odd frames at an even speed) wraps without
// ending, as the original did. The frames mirror shows hundredths of a
// second with a random jitter of up to 3.
[[nodiscard]] bool AdvanceSceneTimer(SceneTimer& timer, int tick_speed,
                                     ScriptFlags& flags, SceneTimerHost& host);
}  // namespace VS::State
