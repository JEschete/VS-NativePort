#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// The frames, seconds, minutes and hours clock the play time (decomp:
// vs_Gametime_t, SLUS_010.40 main.h:173) and the scene timer (decomp:
// D_80050468_t, main.h:422) both keep, with the original's byte arithmetic.
namespace VS::State {
inline constexpr int kFramesPerSecond = 60;
inline constexpr int kSecondsPerMinute = 60;
inline constexpr int kMinutesPerHour = 60;

struct ClockTime {
  std::int8_t frames = 0;
  std::int8_t seconds = 0;
  std::int8_t minutes = 0;
  std::int8_t hours = 0;

  friend bool operator==(const ClockTime&, const ClockTime&) = default;
};

// Advances `time` by `frames`, carrying a whole second into the seconds and
// minutes. Returns true when a whole hour carried; the caller applies its
// own hour rule.
[[nodiscard]] inline bool AdvanceClock(ClockTime& time, int frames) noexcept {
  time.frames = static_cast<std::int8_t>(time.frames + frames);
  if (time.frames < kFramesPerSecond) return false;
  time.frames = 0;
  time.seconds = static_cast<std::int8_t>(time.seconds + 1);
  if (time.seconds < kSecondsPerMinute) return false;
  time.seconds = 0;
  time.minutes = static_cast<std::int8_t>(time.minutes + 1);
  if (time.minutes < kMinutesPerHour) return false;
  time.minutes = 0;
  return true;
}
}  // namespace VS::State
