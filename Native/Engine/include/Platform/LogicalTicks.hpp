#pragma once

// The game's logical tick: one 59.94 Hz NTSC field (Core::FrameClock), the
// unit the original counted with the vertical blank (libetc VSync). Waiting
// suspends the game fiber; the host loop presents, samples input and resumes
// it on the next tick. Each completed tick restarts the frame-work baseline and
// then runs the tick listeners (the sound driver's tick timer).

#include <cstdint>

namespace VS::Platform {

using TickListener = void (*)();

// Ticks the game has completed since start-up, wrapping at 2^32 (the
// original's VSync(-1)).
[[nodiscard]] std::uint32_t ElapsedTicks() noexcept;

// Suspends the game for `count` ticks; a count below one waits one (the
// original's VSync(0) and VSync(count)).
void WaitTicks(int count = 1);

// Runs `listener` at the end of every completed tick, in registration order.
// Adding a registered listener again keeps the one registration.
void AddTickListener(TickListener listener);
void RemoveTickListener(TickListener listener);

}  // namespace VS::Platform
