#include "Audio/TickTimer.hpp"

#include <chrono>
#include <cstdint>

#include "Core/FrameClock.hpp"
#include "Platform/LogicalTicks.hpp"

namespace VS::Audio::tick_timer {
namespace {

using Clock = std::chrono::steady_clock;

constexpr std::uint32_t kFullCount = 65536;
constexpr std::uint16_t kClockMask = 0xFFFF;

Clock::time_point started = Clock::now();
std::uint32_t target = 0;
std::uint64_t count = 0;
// Clock time below one count, in 1/kTickRateNumerator counts.
std::uint64_t tickRemainder = 0;
bool running = false;
bool enabled = false;
Handler handler = nullptr;

bool Counting() { return running && target != 0; }

}  // namespace

void Configure(std::uint16_t new_target) {
  target = new_target == 0 ? kFullCount : new_target;
  count = 0;
  tickRemainder = 0;
}

void Start() {
  started = Clock::now();
  running = true;
  Platform::AddTickListener(&AdvanceLogicalTick);
}

void Stop() { running = false; }

std::int32_t OpenEvent(Handler new_handler) {
  if (new_handler == nullptr || handler != nullptr) return kNoEvent;
  handler = new_handler;
  enabled = false;
  return kEvent;
}

bool CloseEvent(std::int32_t event) {
  if (event == kEvent) {
    handler = nullptr;
    enabled = false;
  }
  return true;
}

bool EnableEvent(std::int32_t event) {
  if (event == kEvent && handler != nullptr) enabled = true;
  return true;
}

bool DisableEvent(std::int32_t event) {
  if (event == kEvent) enabled = false;
  return true;
}

void Advance(std::uint32_t clocks) {
  if (!Counting()) return;
  count += clocks;
  while (count >= target) {
    count -= target;
    if (enabled && handler != nullptr) static_cast<void>(handler());
    if (!Counting()) return;
  }
}

void AdvanceLogicalTick() {
  if (!Counting()) return;
  tickRemainder += kClockHz * Core::FrameClock::kTickRateDenominator;
  const auto clocks = tickRemainder / Core::FrameClock::kTickRateNumerator;
  tickRemainder %= Core::FrameClock::kTickRateNumerator;
  Advance(static_cast<std::uint32_t>(clocks));
}

std::uint16_t ReadClock() {
  const double seconds =
      std::chrono::duration<double>(Clock::now() - started).count();
  return static_cast<std::uint16_t>(
      static_cast<std::uint64_t>(seconds * static_cast<double>(kClockHz)) &
      kClockMask);
}

}  // namespace VS::Audio::tick_timer
