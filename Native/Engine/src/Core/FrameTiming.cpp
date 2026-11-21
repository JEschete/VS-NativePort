// Hand-maintained (tools/port/owned.csv).
#include "Core/FrameTiming.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>

#include "Core/Diagnostics.hpp"

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point frameStart = Clock::now();
constexpr std::int64_t kNanosecondsPerSecond = 1000000000;
constexpr std::int64_t kCounterModulus = 65536;
// Original root counter 1 counts horizontal lines, not CPU cycles. The
// read-only PC timing reference uses 15,734 units/second for this ABI.
constexpr std::int64_t kCounterUnitsPerSecond = 15734;
int tickStep = 0;
int steppedTicks = 0;
}  // namespace

namespace VS::Core {
int FrameCounterTicks(std::chrono::nanoseconds elapsed) {
  const auto nanoseconds = elapsed.count();
  if (nanoseconds < 0) {
    Errorf("clock", "Frame work elapsed duration must not be negative");
    std::abort();
  }
  const auto seconds = nanoseconds / kNanosecondsPerSecond;
  const auto remainder = nanoseconds % kNanosecondsPerSecond;
  const auto units = (seconds % kCounterModulus) * kCounterUnitsPerSecond +
                     remainder * kCounterUnitsPerSecond / kNanosecondsPerSecond;
  return static_cast<int>(units % kCounterModulus);
}

void ResetFrameTimingBaseline() {
  frameStart = Clock::now();
  steppedTicks = 0;
}

void StepFrameCounterTicks(int step) {
  tickStep = step;
  steppedTicks = 0;
}

// decomp: func_800E45B4 (src/BATTLE/BATTLE.PRG/6E4CCPrivate.h:89).
// Preserve the elapsed-unit/16-bit-wrap contract using native monotonic time;
// no hardware register, guest counter, or frame-wait time is represented.
// Stepped replays wrap the same way, so the sum cannot overflow.
int ElapsedFrameCounterTicks() {
  if (tickStep != 0) {
    steppedTicks = static_cast<int>(
        (static_cast<std::int64_t>(steppedTicks) + tickStep) % kCounterModulus);
    return steppedTicks;
  }
  return FrameCounterTicks(std::chrono::duration_cast<std::chrono::nanoseconds>(
      Clock::now() - frameStart));
}
}  // namespace VS::Core
