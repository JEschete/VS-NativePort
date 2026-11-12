// Native side of status_queue.py: F00405/F00406/F00437-F00468, original 146C.c
// _setApplicableStatuses, _setInflictedStatuses and func_80083998..func_8008413C.
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
  constexpr std::array<Wrapper, 32> wrappers{
      QueueStrengthDown, QueueIntelligenceDown, QueueAgilityDown,
      QueueEquipmentDown, QueueHalfMovement, QueueStrengthUp,
      QueueIntelligenceUp, QueueAgilityUp, QueueEquipmentUp,
      QueueQuicken, QueueSilence, nullptr, QueueRegeneration, nullptr,
      QueueParalysis, QueuePoison, QueueNumbness, QueueCurse,
      nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr,
      QueueParalysisCure, QueuePoisonCure, QueueCurseCure, QueueNumbnessCure,
      nullptr, nullptr};
  using MaskWrapper = std::optional<std::uint32_t> (*)(
      std::span<const StatusRule>, const StatusTarget&, QueuedStatuses&);
  constexpr std::array<MaskWrapper, 8> elemental_wrappers{
      QueueAirAttackUp, QueueFireAttackUp, QueueEarthAttackUp, QueueWaterAttackUp,
      QueueAirDefenseUp, QueueFireDefenseUp, QueueEarthDefenseUp, QueueWaterDefenseUp};
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    unsigned function = 0, rule_id = 0, actor_id = 0;
    std::uint32_t execute = 0;
    Guarded<StatusTarget> target;
    Guarded<QueuedStatuses> queued;
    Guarded<Rules> rules;
    Hit hit{};
    std::array<std::uint32_t, 0x970 / 4> stats{};
    std::array<std::uint32_t, kStatusRuleCount * 4 + 8> params{};
    input >> function >> rule_id >> actor_id >> target.value.active >> target.value.immunities >> execute;
    for (auto& word : hit) input >> word;
    for (auto& word : params) input >> word;
    for (auto& word : stats) input >> word;
    if (!input || function >= wrappers.size() + 2 || rule_id >= kStatusRuleCount || actor_id > 255) return 2;
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
    } else if (function == 13) {
      const auto mask = QueueMagicWard(rules.value, target.value, queued.value);
      refused = !mask.has_value();
      if (mask) result = mask.value();
    } else if (function == 15) {
      const auto outcome = QueueBanish(rules.value, target.value, queued.value, execute != 0);
      refused = !outcome.has_value();
      if (outcome) {
        result = outcome.value().changed_mask;
        if (outcome.value().analyze_requested) {
          // Project the original incidental v0 for comparison only. The live
          // mediator is void and commits analysis through the named stats field.
          stats[0x954 / 4] |= 0x00020000u;
          result = stats[0x954 / 4];
        }
      }
    } else if (function >= 20 && function < 28) {
      const auto mask = elemental_wrappers.at(function - 20)(rules.value, target.value, queued.value);
      refused = !mask.has_value();
      if (mask) result = mask.value();
    } else if (function == 32) {
      result = QueueDispel(target.value.active, queued.value);
    } else if (function == 33) {
      const auto feedback = QueueNegativeStatusCure(target.value.active, queued.value);
      result = feedback == StatusFeedback::kNone ? queued.value.removed : 0x20000u;
    } else {
      const auto feedback = wrappers[function - 2](rules.value, target.value, queued.value);
      refused = !feedback.has_value();
      if (feedback) result = static_cast<std::uint32_t>(feedback.value()) << 16;
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
    for (const auto word : stats) std::cout << ' ' << word;
    std::cout << '\n';
  }
}
