// Hand-maintained (tools/port/owned.csv).
// The scene timer and tick speed (State/SceneTimer.hpp).
#include "State/SceneTimer.hpp"  // NOLINT(build/include): native C++ interface.

#include <cstdint>

#include "State/Clock.hpp"
#include "State/ScriptFlags.hpp"

namespace VS::State {
namespace {
constexpr int kByteValues = 0x100;
// Counting up stops at this many hours.
constexpr std::int8_t kCountUpHourLimit = 1;
// The frames mirror: ((frames << 14) + random) * 5 >> 14, over 3.
constexpr unsigned kMirrorFixedPointBits = 14;
constexpr std::uint32_t kMirrorScale = 5;
constexpr std::uint32_t kMirrorDivisor = 3;

// The low byte of `packed_time` as the original's signed remainder; the
// rest moves down a byte with signed division.
[[nodiscard]] std::int8_t TakePackedByte(int& packed_time) noexcept {
  const auto value = static_cast<std::int8_t>(packed_time % kByteValues);
  packed_time /= kByteValues;
  return value;
}

[[nodiscard]] std::uint8_t FlagByte(std::int8_t value) noexcept {
  return static_cast<std::uint8_t>(value);
}

void MirrorWholeTime(const SceneTime& time, ScriptFlags& flags) noexcept {
  (void)flags.Set(kSceneTimerSecondsFlag, FlagByte(time.seconds));
  (void)flags.Set(kSceneTimerMinutesFlag, FlagByte(time.minutes));
  (void)flags.Set(kSceneTimerHoursFlag, FlagByte(time.hours));
}

[[nodiscard]] std::int8_t Advanced(std::int8_t value, int by) noexcept {
  return static_cast<std::int8_t>(value + by);
}

void CountUp(SceneTime& time, int tick_speed) noexcept {
  if (time.hours > 0) return;
  if (!AdvanceClock(time, tick_speed)) return;
  const std::int8_t hours = Advanced(time.hours, 1);
  time.hours = hours > 0 ? kCountUpHourLimit : hours;
}

void CountDown(SceneTime& time, int tick_speed) noexcept {
  if ((time.frames | time.seconds | time.minutes | time.hours) == 0) return;
  if (time.frames == 0) {
    time.frames = kFramesPerSecond;
    time.seconds = Advanced(time.seconds, -1);
    if (time.seconds < 0) {
      time.seconds = kSecondsPerMinute - 1;
      time.minutes = Advanced(time.minutes, -1);
      if (time.minutes < 0) {
        time.minutes = kMinutesPerHour - 1;
        time.hours = Advanced(time.hours, -1);
        if (time.hours < 0) time.hours = 0;
      }
    }
  }
  time.frames = Advanced(time.frames, -tick_speed);
}

[[nodiscard]] std::uint8_t FramesMirror(std::int8_t frames,
                                        int random) noexcept {
  const std::uint32_t scaled =
      (static_cast<std::uint32_t>(static_cast<int>(frames))
       << kMirrorFixedPointBits) +
      static_cast<std::uint32_t>(random);
  return static_cast<std::uint8_t>(
      ((scaled * kMirrorScale) >> kMirrorFixedPointBits) / kMirrorDivisor);
}
}  // namespace

SceneTime UnpackSceneTime(int packed_time) noexcept {
  SceneTime time;
  time.frames = TakePackedByte(packed_time);
  time.seconds = TakePackedByte(packed_time);
  time.minutes = TakePackedByte(packed_time);
  time.hours = TakePackedByte(packed_time);
  return time;
}

void StartSceneTimer(SceneTimer& timer, int mode, int reset_scope,
                     int packed_time, ScriptFlags& flags,
                     SceneTimerHost& host) {
  timer.mode = static_cast<std::uint8_t>(mode);
  timer.reset_scope = static_cast<std::uint16_t>(reset_scope);
  timer.time =
      mode == kSceneTimerCountsUp ? SceneTime{} : UnpackSceneTime(packed_time);
  (void)flags.Set(kSceneTimerFramesFlag, FlagByte(timer.time.frames));
  MirrorWholeTime(timer.time, flags);
  host.ShowTimer();
}

void ResetSceneTimer(SceneTimer& timer, ScriptFlags& flags,
                     SceneTimerHost& host) {
  timer.mode = kSceneTimerStopped;
  timer.time = SceneTime{};
  (void)flags.Set(kSceneTimerFramesFlag, 0);
  MirrorWholeTime(timer.time, flags);
  host.HideTimer();
}

void SetSceneTimerPaused(SceneTimer& timer, bool paused, SceneTimerHost& host) {
  if (paused) {
    timer.paused_mode = timer.mode;
    timer.mode = kSceneTimerStopped;
    host.HideTimer();
    return;
  }
  timer.mode = timer.paused_mode;
  host.ShowTimer();
}

int SelectTickSpeed(int current, int requested) noexcept {
  if (requested == kNormalTickSpeed || requested == kSlowedTickSpeed) {
    return requested;
  }
  return current;
}

bool AdvanceSceneTimer(SceneTimer& timer, int tick_speed, ScriptFlags& flags,
                       SceneTimerHost& host) {
  if (timer.mode == kSceneTimerStopped) return false;
  if (timer.mode == kSceneTimerCountsUp) {
    CountUp(timer.time, tick_speed);
  } else {
    CountDown(timer.time, tick_speed);
  }
  const std::uint8_t frames_mirror =
      timer.time.frames == 0 ? 0
                             : FramesMirror(timer.time.frames, host.Random());
  (void)flags.Set(kSceneTimerFramesFlag, frames_mirror);
  MirrorWholeTime(timer.time, flags);
  return timer.time.frames == 0;
}
}  // namespace VS::State
