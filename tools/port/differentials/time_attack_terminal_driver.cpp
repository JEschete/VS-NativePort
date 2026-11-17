#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "Menu/TimeAttackTerminalFixture.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace fixture = VS::tests::time_attack_terminal;
using Output = std::vector<std::int64_t>;
enum class Operation : int {
  kLoadResults,
  kLoadStart,
  kResults,
  kStart,
  kDispatch,
  kInitialize,
  kUpdate,
};
constexpr std::int32_t kLoaded = 4;
constexpr std::uint32_t kConfirm = 0x20;
constexpr std::uint8_t kNeighbor = 0xAA;

template <typename T = std::int32_t>
T Read() {
  std::int64_t value = 0;
  if (!(std::cin >> value)) throw std::runtime_error("short terminal vector");
  return static_cast<T>(value);
}

r::TimeAttackState ReadState() {
  return {.init_phase = Read(),
          .timer = Read(),
          .phase = Read(),
          .animation = Read<std::uint32_t>(),
          .new_slot = Read(),
          .time = Read(),
          .shown_map = Read(),
          .shown_score = Read(),
          .selection = Read<std::uint32_t>()};
}

template <typename Range>
void Append(Output& output, const Range& values) {
  output.insert(output.end(), values.begin(), values.end());
}

void AppendCalls(Output& output, const fixture::Calls& calls) {
  output.push_back(static_cast<std::int64_t>(calls.rows.size()));
  for (const auto& row : calls.rows) Append(output, row);
}

int ReturnCode(r::GameClearStep step) {
  if (step == r::GameClearStep::kRefused) return -1;
  return step == r::GameClearStep::kComplete ? 1 : 0;
}

void ReadLoad(fixture::Load& host) {
  host.loaded = Read() == kLoaded;
  host.music_busy = Read() != 0;
  host.file_bytes = Read();
  host.run.course = Read<std::uint8_t>();
  host.run.minutes = Read<std::uint8_t>();
  host.run.seconds = Read<std::uint8_t>();
  host.run.hundredths = Read<std::uint8_t>();
  host.run.round = Read<std::uint8_t>();
  host.run.difficulty = Read<std::uint8_t>();
  for (auto& row : host.scores) {
    for (auto& score : row) score = Read<std::uint32_t>();
  }
  for (auto& present : host.present) present = Read() != 0;
  for (auto& area : host.rectangles) {
    area = {Read<std::int16_t>(), Read<std::int16_t>(), Read<std::int16_t>(),
            Read<std::int16_t>()};
  }
  for (auto& color : host.palette) color = Read<std::uint16_t>();
  for (auto& color : host.saved_palette) color = Read<std::uint16_t>();
}

Output RunLoad(Operation operation) {
  auto state = ReadState();
  fixture::Load host;
  ReadLoad(host);
  const auto step = operation == Operation::kLoadResults
                        ? r::InitializeTimeAttackResults(state, host)
                        : r::InitializeTimeAttackStart(state, host);
  Output output{ReturnCode(step)};
  Append(output, fixture::Words(state));
  for (const auto& row : host.scores) Append(output, row);
  for (const auto& area : host.rectangles) {
    Append(output, std::array{area.x, area.y, area.width, area.height});
  }
  Append(output, host.palette);
  Append(output, host.saved_palette);
  AppendCalls(output, host.calls);
  return output;
}

Output RunScreen(Operation operation) {
  auto state = ReadState();
  const bool confirm = (Read<std::uint32_t>() & kConfirm) != 0;
  fixture::Screen host;
  const auto step = operation == Operation::kResults
                        ? r::UpdateTimeAttackResults(state, confirm, host)
                        : r::UpdateTimeAttackStart(state, confirm, host);
  Output output{ReturnCode(step)};
  Append(output, fixture::Words(state));
  AppendCalls(output, host.calls);
  return output;
}

Output RunDispatch(Operation operation) {
  std::array<std::uint8_t, 2> bytes{Read<std::uint8_t>(), kNeighbor};
  const auto selector = Read<std::uint8_t>();
  fixture::Presentations host;
  host.result = Read();
  const auto mutation = Read();
  if (mutation >= 0) host.callback_write = static_cast<std::uint8_t>(mutation);
  if (operation == Operation::kDispatch) host.caller = bytes.data();
  const auto result = operation == Operation::kDispatch
                          ? r::UpdateResultsScreen(bytes, selector, host)
                      : operation == Operation::kInitialize
                          ? r::InitializeResultsScreen(selector, host)
                          : r::UpdateResultsPresentation(selector, host);
  if (!result.has_value()) throw std::runtime_error("invalid proof context");
  Output output{*result, bytes.front(), bytes.back()};
  AppendCalls(output, host.calls);
  return output;
}
}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): malformed fixtures must fail.
int main() {
  int mode = 0;
  while (std::cin >> mode) {
    const auto operation = static_cast<Operation>(mode);
    const auto output = operation <= Operation::kLoadStart ? RunLoad(operation)
                        : operation <= Operation::kStart
                            ? RunScreen(operation)
                            : RunDispatch(operation);
    for (const auto word : output) std::cout << word << ' ';
    std::cout << '\n';
  }
}
