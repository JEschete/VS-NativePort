#include "Core/FrameClock.hpp"

namespace VS::Core {

u32 FrameClock::Advance(double elapsedSeconds, u32 maxTicks)
{
    constexpr double tick = 1.0 / TicksPerSecond;
    m_accumulator += elapsedSeconds;
    u32 count = 0;
    while (m_accumulator >= tick && count < maxTicks) {
        m_accumulator -= tick;
        ++count;
    }
    if (count == maxTicks && m_accumulator >= tick) {
        m_accumulator = 0.0; // drop the backlog after a stall instead of spiraling
    }
    m_ticks += count;
    return count;
}

} // namespace VS::Core
