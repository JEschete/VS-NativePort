#include "Legacy/Core/LibEtc.hpp"

#include <chrono>
#include <cstdint>

#include "Core/FrameClock.hpp"
#include "Core/FrameTiming.hpp"
#include "Core/Unported.hpp"
#include "Platform/GameFiber.hpp"

// libetc timing (plan §2.2, strategy R). The PS1 waited for vertical blank;
// natively the game fiber hands control back to the host loop, which presents,
// samples input and resumes it on the next logical tick.

namespace VS::Legacy {
namespace {

using Clock = std::chrono::steady_clock;

std::int32_t vsyncTicks = 0;
void (*vblankCallback)() = nullptr;
Clock::time_point rcnt2Start = Clock::now();
std::uint32_t rcnt2Target = 0;
std::uint64_t rcnt2Clocks = 0;
std::uint64_t rcnt2FieldRemainder = 0;
bool rcnt2Running = false;
bool rcnt2Enabled = false;
EventHandler rcnt2Callback = nullptr;

// root counter 2 counts the system clock / 8 (33.8688 MHz / 8)
constexpr double Rcnt2Hz = 33868800.0 / 8.0;

void WaitOneFrame() {
  VS::Platform::GameFiber::YieldToHost();
  VS::Core::ResetFrameTimingBaseline();
  ++vsyncTicks;
  if (rcnt2Running && rcnt2Target != 0) {
    // Carry fractional clocks at the host's field rate, not an assumed 60 Hz.
    rcnt2FieldRemainder += static_cast<std::uint64_t>(Rcnt2Hz) *
                           VS::Core::FrameClock::kTickRateDenominator;
    const auto elapsed =
        rcnt2FieldRemainder / VS::Core::FrameClock::kTickRateNumerator;
    rcnt2FieldRemainder %= VS::Core::FrameClock::kTickRateNumerator;
    AdvanceRootCounter2(static_cast<std::uint32_t>(elapsed));
  }
  if (vblankCallback != nullptr) {
    vblankCallback();
  }
}

}  // namespace

std::int32_t VSync(std::int32_t mode) {
  if (mode < 0) {
    return vsyncTicks;  // vertical blanks since boot
  }
  if (mode == 1) {
    // horizontal lines since the last blank: the frame is always complete
    // natively, so report a full NTSC field
    return 263;
  }
  const std::int32_t frames = mode == 0 ? 1 : mode;
  for (std::int32_t i = 0; i < frames; ++i) {
    WaitOneFrame();
  }
  return 0;
}

std::int32_t VSyncCallback(void (*f)()) {
  vblankCallback = f;
  return 0;
}

std::int32_t SetVideoMode(std::int32_t mode) {
  (void)mode;  // NTSC only
  return kModeNtsc;
}

std::int32_t SetRCnt(std::uint32_t spec, std::uint16_t target,
                     std::int32_t mode) {
  (void)mode;
  if (spec == kRootCounter2) {
    rcnt2Target = target == 0 ? 65536u : target;
    rcnt2Clocks = 0;
    rcnt2FieldRemainder = 0;
  }
  return 1;
}

std::int32_t GetRCnt(std::uint32_t spec) {
  if (spec != kRootCounter2) {
    VS::Core::RecordUnported("libetc:GetRCnt (counter other than 2)");
    return 0;
  }
  const double seconds =
      std::chrono::duration<double>(Clock::now() - rcnt2Start).count();
  return static_cast<std::int32_t>(
      static_cast<std::uint64_t>(seconds * Rcnt2Hz) & 0xFFFF);
}

std::int32_t StartRCnt(std::uint32_t spec) {
  if (spec == kRootCounter2) {
    rcnt2Start = Clock::now();
    rcnt2Running = true;
  }
  return 1;
}

std::int32_t StopRCnt(std::uint32_t spec) {
  if (spec == kRootCounter2) rcnt2Running = false;
  return 1;
}

std::int32_t OpenEvent(std::uint32_t desc, std::int32_t spec, std::int32_t mode,
                       EventHandler callback) {
  if (desc != kRootCounter2 || spec != kEventSpecInterrupt ||
      mode != kEventModeInterrupt || callback == nullptr) {
    return 0;
  }
  if (rcnt2Callback != nullptr) return -1;
  rcnt2Callback = callback;
  rcnt2Enabled = false;
  return 1;
}

std::int32_t CloseEvent(std::int32_t event) {
  if (event == 1) {
    rcnt2Callback = nullptr;
    rcnt2Enabled = false;
  }
  return 1;
}

std::int32_t EnableEvent(std::int32_t event) {
  if (event == 1 && rcnt2Callback != nullptr) rcnt2Enabled = true;
  return 1;
}

std::int32_t DisableEvent(std::int32_t event) {
  if (event == 1) rcnt2Enabled = false;
  return 1;
}

void AdvanceRootCounter2(std::uint32_t elapsed_clocks) {
  if (!rcnt2Running || rcnt2Target == 0) return;
  rcnt2Clocks += elapsed_clocks;
  while (rcnt2Clocks >= rcnt2Target) {
    rcnt2Clocks -= rcnt2Target;
    if (rcnt2Enabled && rcnt2Callback != nullptr) rcnt2Callback();
    if (!rcnt2Running || rcnt2Target == 0) return;
  }
}

}  // namespace VS::Legacy
