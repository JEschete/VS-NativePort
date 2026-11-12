// Native side of status_queue.py: F00405/F00406/F00437-F00442, original 146C.c
// _setApplicableStatuses, _setInflictedStatuses and func_80083998..func_80083B28.
#include "Battle/StatusQueue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace {
unsigned diagnostics = 0;

template <typename T>
struct Guarded {
  std::array<std::uint32_t, 4> before{};
  T value{};
  std::array<std::uint32_t, 4> after{};
};

using Rules = std::array<VS::Battle::StatusRule, VS::Battle::kStatusRuleCount>;
using Hit = std::array<std::uint32_t, 16>;

template <typename T>
void SetGuards(Guarded<T>& guarded, const Hit& hit) {
  for (std::size_t index = 0; index < guarded.before.size(); ++index) {
    guarded.before[index] = hit[index + 1];
    guarded.after[index] = hit[index + 8];
  }
}

template <typename T>
bool GuardsUnchanged(const Guarded<T>& guarded, const Hit& hit) {
  for (std::size_t index = 0; index < guarded.before.size(); ++index) {
    if (guarded.before[index] != hit[index + 1] ||
        guarded.after[index] != hit[index + 8]) {
      return false;
    }
  }
  return true;
}

bool RulesUnchanged(const Rules& actual, const Rules& before) {
  for (std::size_t index = 0; index < actual.size(); ++index) {
    if (actual[index].statuses != before[index].statuses ||
        actual[index].cancelled != before[index].cancelled ||
        actual[index].blocking != before[index].blocking) {
      return false;
    }
  }
  return true;
}
}  // namespace

extern "C" void Core_Debugf(const char*, const char*, ...) { ++diagnostics; }

int main() {
  using namespace VS::Battle;
  using Wrapper = std::optional<StatusFeedback> (*)(
      std::span<const StatusRule>, const StatusTarget&, QueuedStatuses&);
  constexpr std::array<Wrapper, 6> wrappers{
      QueueStrengthDown, QueueIntelligenceDown, QueueAgilityDown,
      QueueEquipmentDown, QueueHalfMovement, QueueStrengthUp};
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    unsigned function = 0, rule_id = 0, actor_id = 0;
    Guarded<StatusTarget> target;
    Guarded<QueuedStatuses> queued;
    Guarded<Rules> rules;
    Hit hit{};
    std::array<std::uint32_t, kStatusRuleCount * 4 + 8> params{};
    input >> function >> rule_id >> actor_id >> target.value.active >> target.value.immunities;
    for (auto& word : hit) input >> word;
    for (auto& word : params) input >> word;
    if (!input || function >= 8 || rule_id >= kStatusRuleCount || actor_id > 255) return 2;
    target.value.exempt = actor_id == 128;
    queued.value = {hit[5], hit[6], static_cast<StatusFeedback>((hit[7] >> 16) & 3u)};
    for (std::size_t index = 0; index < rules.value.size(); ++index) {
      const auto offset = 4 + index * 4;
      rules.value[index] = {params[offset], params[offset + 2], params[offset + 3]};
    }
    SetGuards(target, hit);
    SetGuards(queued, hit);
    SetGuards(rules, hit);
    const auto original_target = target.value;
    const auto original_rules = rules.value;
    diagnostics = 0;
    bool refused = false;
    std::uint32_t result = 0;
    if (function == 0) {
      result = QueueApplicableStatusChanges(rules.value[rule_id], target.value, queued.value);
    } else if (function == 1) {
      result = QueueStatusRemovalMask(rules.value[rule_id], target.value, queued.value);
    } else {
      const auto feedback = wrappers[function - 2](rules.value, target.value, queued.value);
      refused = !feedback.has_value();
      if (feedback) result = static_cast<std::uint32_t>(*feedback) << 16;
    }
    const bool guards_ok = GuardsUnchanged(target, hit) && GuardsUnchanged(queued, hit) &&
                           GuardsUnchanged(rules, hit);
    const bool target_ok = target.value.active == original_target.active &&
                           target.value.immunities == original_target.immunities &&
                           target.value.exempt == original_target.exempt;
    const bool rules_ok = RulesUnchanged(rules.value, original_rules);
    hit[5] = queued.value.added;
    hit[6] = queued.value.removed;
    hit[7] = (hit[7] & ~0x00030000u) | (static_cast<std::uint32_t>(queued.value.feedback) << 16);
    std::cout << refused << ' ' << result << ' ' << diagnostics << ' ' << guards_ok << ' '
              << target_ok << ' ' << rules_ok;
    for (const auto word : hit) std::cout << ' ' << word;
    std::cout << '\n';
  }
}
