// Hand-maintained (tools/port/owned.csv).
// The play time clock (State/GameClock.hpp).
#include "State/GameClock.hpp"

#include <cstdint>

#include "State/Clock.hpp"

namespace VS::State {
void AdvancePlayTime(PlayTime& time, int tick_speed) noexcept {
  if (time.hours >= kMaxPlayTimeHours) return;
  if (!AdvanceClock(time, tick_speed)) return;
  time.hours = static_cast<std::int8_t>(time.hours + 1);
  if (time.hours >= kMaxPlayTimeHours) time.hours = kMaxPlayTimeHours;
}
}  // namespace VS::State
