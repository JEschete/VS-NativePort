// Hand-maintained (tools/port/owned.csv).
#include "Core/FrameTiming.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>

#include "Core/Diagnostics.h"

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point frame_start = Clock::now();
constexpr std::int64_t kNanosecondsPerSecond = 1000000000;
constexpr std::int64_t kCounterModulus = 65536;
// Original root counter 1 counts horizontal lines, not CPU cycles. The
// read-only PC timing reference uses 15,734 units/second for this ABI.
constexpr std::int64_t kCounterUnitsPerSecond = 15734;
}  // namespace

namespace VS::Core {
int FrameCounterTicks(std::chrono::nanoseconds elapsed) {
  const auto nanoseconds = elapsed.count();
  if (nanoseconds < 0) {
    Core_Errorf("clock", "Frame work elapsed duration must not be negative");
    std::abort();
  }
  const auto seconds = nanoseconds / kNanosecondsPerSecond;
  const auto remainder = nanoseconds % kNanosecondsPerSecond;
  const auto units = (seconds % kCounterModulus) * kCounterUnitsPerSecond +
                     remainder * kCounterUnitsPerSecond / kNanosecondsPerSecond;
  return static_cast<int>(units % kCounterModulus);
}
}  // namespace VS::Core

void Core_ResetFrameTimingBaseline(void) { frame_start = Clock::now(); }

// decomp: func_800E45B4 (src/BATTLE/BATTLE.PRG/7BDB4.s:7-16).
// Preserve the elapsed-unit/16-bit-wrap contract using native monotonic time;
// no hardware register, guest counter, or frame-wait time is represented.
int Core_ElapsedFrameCounterTicks(void) {
  return VS::Core::FrameCounterTicks(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() -
                                                           frame_start));
}
