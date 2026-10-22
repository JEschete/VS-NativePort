#pragma once

#include "Core/Types.hpp"

namespace VS::Core {

// Logical game time (replaces VSync()/vblank counters). The original runs on
// NTSC fields (59.94 Hz); game logic advances in whole ticks and some scenes
// step every 2nd/3rd tick.
class FrameClock {
 public:
  static constexpr u32 kTickRateNumerator = 60000;
  static constexpr u32 kTickRateDenominator = 1001;
  static constexpr double TicksPerSecond =
      static_cast<double>(kTickRateNumerator) / kTickRateDenominator;
  static constexpr u32 kDefaultCatchUpTicks = 4;

  // Feed real elapsed seconds; returns how many logical ticks to run now
  // (capped).
  u32 Advance(double elapsed_seconds, u32 max_ticks = kDefaultCatchUpTicks);

  u64 Ticks() const { return m_ticks_; }
  void Reset() {
    m_ticks_ = 0;
    m_accumulator_ = 0.0;
  }

 private:
  u64 m_ticks_ = 0;
  double m_accumulator_ = 0.0;
};

}  // namespace VS::Core
