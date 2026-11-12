// Hand-maintained (tools/port/owned.csv).
#include "Battle/BasicActionEffects.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace VS::Battle {
namespace {
std::int16_t DoubledHp(const ActorActionResult& target) {
  const auto hp =
      target.changes.at(static_cast<std::size_t>(ActionResultField::kHp))
          .amount;
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp * 2));
}
}  // namespace

// decomp: func_80084228 / func_800842F0 / func_80084370
// (src/BATTLE/BATTLE.PRG/146C.c:12547/12572/12599).
void MarkSourceActionSuccess(SourceActionOutcome& outcome) {
  outcome = SourceActionOutcome::kSuccess;
}

// decomp: func_80084248 (src/BATTLE/BATTLE.PRG/146C.c:12554).
void QueueInstantDeath(InstantDeathQueue& queue) {
  queue.RemoveStatuses(StatusRuleId::kInstantDeathRemoval);
  const auto hp = queue.CurrentHp();
  queue.QueueHp({std::bit_cast<std::int16_t>(hp), 1});
}

// decomp: func_800842AC (src/BATTLE/BATTLE.PRG/146C.c:12563).
bool QueueDamageBasedArmorWear(ActorActionResult& target) {
  if (target.limb < 0 ||
      static_cast<std::size_t>(target.limb) >= target.limbs.size()) {
    return false;
  }
  auto& armor =
      target.changes.at(static_cast<std::size_t>(ActionResultField::kArmorDp));
  armor.amount = DoubledHp(target);
  target.limbs.at(static_cast<std::size_t>(target.limb)).armor_dp =
      DoubledHp(target);
  armor.mode = 1;
  return true;
}

// decomp: func_80084310 (src/BATTLE/BATTLE.PRG/146C.c:12579).
void SetActionFlag64(bool execute, ActionScriptFlags& flags) {
  if (execute) {
    flags.Set(ActionScriptFlag::kFlag64, 1);
  }
}

// decomp: func_80084340 (src/BATTLE/BATTLE.PRG/146C.c:12589).
void SetActionFlag32(bool execute, ActionScriptFlags& flags) {
  if (execute) {
    flags.Set(ActionScriptFlag::kFlag32, 1);
  }
}
}  // namespace VS::Battle
