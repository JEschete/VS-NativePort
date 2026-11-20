#pragma once

// The sound driver's tick timer: root counter 2 and its interrupt event in the
// original (StartSound, src/SLUS_010.40/25AC.c:1060). The counter runs at the
// system clock / 8 and runs the event's handler each time it reaches its
// target: the driver's 0x44E8 counts make 240 driver ticks a second. Natively
// the counter advances by the clock time of each logical tick
// (Platform::WaitTicks), carrying the fraction, so the driver keeps its 240 Hz
// cadence against the 59.94 Hz tick.

#include <cstdint>

namespace VS::Audio::tick_timer {

// The counter's clock: the 33.8688 MHz system clock / 8.
inline constexpr std::uint64_t kClockHz = 33868800U / 8U;
// The timer's one event, and OpenEvent's failure result.
inline constexpr std::int32_t kEvent = 1;
inline constexpr std::int32_t kNoEvent = -1;

// The handler the event runs: one driver tick, returning its cost in counts.
using Handler = std::int32_t (*)();

// Sets the target (0 counts 65536) and restarts the count and its carried
// fraction.
void Configure(std::uint16_t target);
// Starts and stops the count; Start also attaches the timer to the logical
// tick.
void Start();
void Stop();
// Registers `handler`, disabled. Returns kEvent, or kNoEvent while another
// handler is registered or when `handler` is null.
[[nodiscard]] std::int32_t OpenEvent(Handler handler);
// Each acts on kEvent only and reports success, as the SDK calls did.
bool CloseEvent(std::int32_t event);
bool EnableEvent(std::int32_t event);
bool DisableEvent(std::int32_t event);

// Runs the count forward by `clocks`, running the enabled handler at each
// target.
void Advance(std::uint32_t clocks);
// Advances by one logical tick's clock time; the tick listener Start attaches.
void AdvanceLogicalTick();

// The free-running count (0-0xFFFF) at the wall-clock time since Start: a
// diagnostic of the host's tick cost (the original's GetRCnt), not game time.
[[nodiscard]] std::uint16_t ReadClock();

}  // namespace VS::Audio::tick_timer
