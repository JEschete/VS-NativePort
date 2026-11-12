// Native side of basic_action_effects.py, F00469-F00475 (146C.c:12547-12604).
#include "Battle/BasicActionEffects.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

extern "C" void Core_Debugf(const char*, const char*, ...) {}

namespace {
namespace b = VS::Battle;
using Hit = std::array<std::uint32_t, 25>;
template <typename T> struct Guarded {
  std::array<std::uint32_t, 4> before{0xAABBCCDD, 0x01234567, 0xDEADBEEF, 0x80000000};
  T value{};
  std::array<std::uint32_t, 4> after{0xFEDCBA98, 0x76543210, 0x80000001, 0x13572468};
  bool intact() const {
    return before == std::array<std::uint32_t, 4>{0xAABBCCDD, 0x01234567, 0xDEADBEEF, 0x80000000}
        && after == std::array<std::uint32_t, 4>{0xFEDCBA98, 0x76543210, 0x80000001, 0x13572468};
  }
};
constexpr std::size_t hp = static_cast<std::size_t>(b::ActionResultField::kHp);
constexpr std::size_t armor = static_cast<std::size_t>(b::ActionResultField::kArmorDp);
std::int16_t low(std::uint32_t word) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(word));
}
void half(std::uint32_t& word, std::int16_t value, unsigned shift = 0) {
  word = (word & ~(0xFFFFu << shift)) | (static_cast<std::uint32_t>(
      std::bit_cast<std::uint16_t>(value)) << shift);
}
struct Death final : b::InstantDeathQueue {
  Guarded<std::array<b::StatusRule, b::kStatusRuleCount>> rules;
  Guarded<b::StatusTarget> target;
  Guarded<b::QueuedStatuses> queued;
  b::ActionResultChange change{};
  std::uint16_t current_hp{};
  void RemoveStatuses(b::StatusRuleId rule) override {
    b::QueueStatusRemovalMask(rules.value.at(static_cast<std::size_t>(rule)),
                             target.value, queued.value);
  }
  std::uint16_t CurrentHp() override { return current_hp; }
  void QueueHp(b::ActionResultChange value) override { change = value; }
};
struct Flags final : b::ActionScriptFlags {
  unsigned count{}, index{}, value{};
  void Set(b::ActionScriptFlag flag, std::uint8_t byte) override {
    ++count; index = static_cast<unsigned>(flag); value = byte;
  }
};
}
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    unsigned function{}, execute{};
    Hit source{}, target{};
    std::array<std::uint32_t, 0x970 / 4 + 8> stats{};
    std::array<std::uint32_t, 32 * 4 + 8> rules{};
    in >> function >> execute;
    for (auto& v : source) in >> v;
    for (auto& v : target) in >> v;
    for (auto& v : stats) in >> v;
    for (auto& v : rules) in >> v;
    if (!in || function >= 7) return 2;
    Guarded<b::SourceActionOutcome> outcome;
    Guarded<b::ActorActionResult> result;
    Guarded<Flags> flags;
    Death death;
    outcome.value = static_cast<b::SourceActionOutcome>((source[11] >> 26) & 3);
    result.value.limb = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(target[4] >> 8));
    result.value.changes[hp] = {low(target[5]), static_cast<std::uint8_t>(target[11] & 3)};
    result.value.changes[armor] = {low(target[19]), static_cast<std::uint8_t>((target[11] >> 22) & 3)};
    for (std::size_t limb = 0; limb < 6; ++limb)
      result.value.limbs[limb] = {low(target[12 + limb]), low(target[12 + limb] >> 16)};
    const auto before_result = result.value;
    death.target.value = {stats[4 + 0x948 / 4], stats[4 + 0x944 / 4],
                          (stats[4 + 0x954 / 4] >> 24) == 128};
    death.current_hp = static_cast<std::uint16_t>(stats[4 + 0x18 / 4]);
    death.queued.value = {target[9], target[10], static_cast<b::StatusFeedback>((target[11] >> 16) & 3)};
    for (std::size_t i = 0; i < 32; ++i)
      death.rules.value[i] = {rules[4 + i * 4], rules[6 + i * 4], rules[7 + i * 4]};
    const auto before_target = death.target.value;
    const auto before_rules = death.rules.value;
    bool accepted = true;
    if (function == 0 || function == 3 || function == 6) {
      b::MarkSourceActionSuccess(outcome.value);
      source[11] = (source[11] & ~0x0C000000u) | (static_cast<unsigned>(outcome.value) << 26);
    } else if (function == 1) {
      b::QueueInstantDeath(death);
      target[9] = death.queued.value.added;
      target[10] = death.queued.value.removed;
      target[11] = (target[11] & ~0x00030003u) |
          (static_cast<unsigned>(death.queued.value.feedback) << 16) | death.change.mode;
      half(target[5], death.change.amount);
    } else if (function == 2) {
      accepted = b::QueueDamageBasedArmorWear(result.value);
      half(target[4 + 0x3C / 4], result.value.changes[armor].amount);
      for (std::size_t limb = 0; limb < 6; ++limb)
        half(target[12 + limb], result.value.limbs[limb].armor_dp, 16);
      target[11] = (target[11] & ~0x00C00000u) |
          (static_cast<unsigned>(result.value.changes[armor].mode) << 22);
    } else if (function == 4) {
      b::SetActionFlag64(execute != 0, flags.value);
    } else {
      b::SetActionFlag32(execute != 0, flags.value);
    }
    bool inputs_ok = death.target.value.active == before_target.active &&
        death.target.value.immunities == before_target.immunities &&
        death.target.value.exempt == before_target.exempt &&
        death.current_hp == static_cast<std::uint16_t>(stats[4 + 0x18 / 4]);
    auto expected_result = before_result;
    if (function == 2) {
      expected_result.changes[armor] = result.value.changes[armor];
      expected_result.limbs.at(static_cast<std::size_t>(result.value.limb)).armor_dp =
          result.value.limbs.at(static_cast<std::size_t>(result.value.limb)).armor_dp;
    }
    inputs_ok = inputs_ok && expected_result == result.value;
    for (std::size_t i = 0; i < 32; ++i)
      inputs_ok = inputs_ok && death.rules.value[i].statuses == before_rules[i].statuses &&
          death.rules.value[i].cancelled == before_rules[i].cancelled &&
          death.rules.value[i].blocking == before_rules[i].blocking;
    const bool guards = outcome.intact() && result.intact() && flags.intact() &&
                        death.target.intact() && death.rules.intact() && death.queued.intact();
    std::cout << !accepted << ' ' << guards << ' ' << inputs_ok << ' '
              << flags.value.count << ' ' << flags.value.index << ' ' << flags.value.value;
    for (const auto v : source) std::cout << ' ' << v;
    for (const auto v : target) std::cout << ' ' << v;
    std::cout << '\n';
  }
}
