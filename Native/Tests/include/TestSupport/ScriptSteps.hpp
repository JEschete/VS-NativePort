#pragma once

#include <cstdint>
#include <vector>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptStep.hpp"

namespace VS::test_support {
// One event script instruction and the opcode handler that runs it on a
// `Host` interface.
template <typename Host>
struct ScriptStepCase {
  Event::ScriptStep (*handler)(Event::ScriptCommand, Host&);
  std::vector<std::uint8_t> bytes;
};

// The result of each instruction, run in order on one host.
template <typename Host>
std::vector<Event::ScriptStep> RunScriptSteps(
    const std::vector<ScriptStepCase<Host>>& steps, Host& host) {
  std::vector<Event::ScriptStep> results;
  results.reserve(steps.size());
  for (const auto& [handler, bytes] : steps) {
    results.push_back(handler(Event::ScriptCommand{bytes}, host));
  }
  return results;
}
}  // namespace VS::test_support
