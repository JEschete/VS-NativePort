#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>

#include "Battle/FloatingLabelQueue.hpp"
#include "TestSupport/FloatingLabelQueueFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::floating_labels;
constexpr std::size_t kStateWords = 16;
constexpr std::size_t kWordsPerRecord = 6;
using RecordWords = std::array<std::uint32_t, kWordsPerRecord>;
using StateWords = std::array<std::uint32_t, kStateWords>;
using StateHalves = std::array<std::int16_t, kStateWords * 2>;

void ReadState(f::Queue& queue, const StateWords& state) {
  const auto halves = std::bit_cast<StateHalves>(state);
  queue.visibility = halves[8];
  queue.cue = halves[9];
  queue.timer = halves[12];
  queue.suppression = halves[13];
  queue.count = halves[15];
}

StateWords WriteState(const f::Queue& queue, const StateWords& state) {
  auto halves = std::bit_cast<StateHalves>(state);
  halves[8] = queue.visibility;
  halves[9] = queue.cue;
  halves[12] = queue.timer;
  halves[13] = queue.suppression;
  halves[15] = queue.count;
  return std::bit_cast<StateWords>(halves);
}

bool Execute(int operation, int argument, int alias, f::Queue& queue,
             const std::array<std::int16_t, 3>& position, int& result) {
  switch (operation) {
    case 0:
      return b::ResetFloatingLabels(queue, queue.ResetView());
    case 1: {
      const auto source =
          alias < 0
              ? std::span<const std::int16_t>(position)
              : std::span<const std::int16_t>(
                    queue.records.at(static_cast<std::size_t>(alias)).position);
      return b::QueueWorldLockedLabel(queue, source);
    }
    case 2: {
      auto& record = queue.records.at(static_cast<std::size_t>(argument));
      b::AdvanceFloatingLabelQueue({record.waiting_steps, record.elapsed});
      return true;
    }
    case 3: {
      const auto slot = b::ReserveFloatingLabel(queue);
      if (!slot) return false;
      result = slot.value().index;
      return true;
    }
    case 4:
      return b::CompactFloatingLabels(queue, argument);
    case 5:
      return b::RemoveFloatingLabel(queue, {argument});
    default:
      return false;
  }
}
}  // namespace

int main() {
  int operation = 0;
  int argument = 0;
  int alias = -1;
  while (std::cin >> operation >> argument >> alias) {
    f::Queue queue;
    StateWords state{};
    for (auto& word : state) std::cin >> word;
    ReadState(queue, state);
    for (auto& record : queue.records) {
      RecordWords raw{};
      for (auto& word : raw) std::cin >> word;
      record = std::bit_cast<f::Record>(raw);
    }
    std::array<std::int16_t, 3> position{};
    for (auto& coordinate : position) std::cin >> coordinate;
    int result = 0;
    const bool success =
        Execute(operation, argument, alias, queue, position, result);
    std::cout << !success << ' ' << result;
    for (const auto word : WriteState(queue, state)) std::cout << ' ' << word;
    for (const auto& record : queue.records) {
      for (const auto word : std::bit_cast<RecordWords>(record)) {
        std::cout << ' ' << word;
      }
    }
    std::cout << '\n';
  }
}
