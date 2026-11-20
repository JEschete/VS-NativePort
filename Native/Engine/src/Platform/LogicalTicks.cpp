#include "Platform/LogicalTicks.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Core/FrameTiming.hpp"
#include "Platform/GameFiber.hpp"

namespace VS::Platform {
namespace {

std::uint32_t elapsedTicks = 0;

std::vector<TickListener>& Listeners() {
  static std::vector<TickListener> listeners;
  return listeners;
}

void CompleteTick() {
  GameFiber::YieldToHost();
  Core::ResetFrameTimingBaseline();
  ++elapsedTicks;
  // By index: a listener may register another.
  auto& listeners = Listeners();
  for (std::size_t i = 0; i < listeners.size(); ++i) {
    listeners[i]();
  }
}

}  // namespace

std::uint32_t ElapsedTicks() noexcept { return elapsedTicks; }

void WaitTicks(int count) {
  const int ticks = std::max(count, 1);
  for (int i = 0; i < ticks; ++i) {
    CompleteTick();
  }
}

void AddTickListener(TickListener listener) {
  auto& listeners = Listeners();
  if (listener != nullptr && std::find(listeners.begin(), listeners.end(),
                                       listener) == listeners.end()) {
    listeners.push_back(listener);
  }
}

void RemoveTickListener(TickListener listener) {
  auto& listeners = Listeners();
  listeners.erase(std::remove(listeners.begin(), listeners.end(), listener),
                  listeners.end());
}

}  // namespace VS::Platform
