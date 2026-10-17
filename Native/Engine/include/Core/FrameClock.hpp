#pragma once

#include "Core/Types.hpp"

namespace VS::Core {

// Logical game time (replaces VSync()/vblank counters). The original runs on NTSC fields
// (59.94 Hz); game logic advances in whole ticks and some scenes step every 2nd/3rd tick.
class FrameClock {
public:
    static constexpr double TicksPerSecond = 60000.0 / 1001.0;

    // Feed real elapsed seconds; returns how many logical ticks to run now (capped).
    u32 Advance(double elapsedSeconds, u32 maxTicks = 4);

    u64 Ticks() const { return m_ticks; }
    void Reset()
    {
        m_ticks = 0;
        m_accumulator = 0.0;
    }

private:
    u64 m_ticks = 0;
    double m_accumulator = 0.0;
};

} // namespace VS::Core
