#include "Core/FrameClock.hpp"

namespace VS::Core {

// Public clock API: elapsed seconds and catch-up limit have distinct units.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
u32 FrameClock::Advance(double elapsed_seconds, u32 max_ticks) {
  constexpr double kTickSeconds = 1.0 / TicksPerSecond;
  m_accumulator_ += elapsed_seconds;
  u32 count = 0;
  while (m_accumulator_ >= kTickSeconds && count < max_ticks) {
    m_accumulator_ -= kTickSeconds;
    ++count;
  }
  if (count == max_ticks && m_accumulator_ >= kTickSeconds) {
    m_accumulator_ =
        0.0;  // drop the backlog after a stall instead of spiraling
  }
  m_ticks_ += count;
  return count;
}

}  // namespace VS::Core
