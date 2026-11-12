// Hand-maintained (tools/port/owned.csv).
#include "Battle/BasicActionEffects.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace VS::Battle {
namespace {
enum class StatDirection : std::uint8_t { kDecrease = 1, kIncrease = 2 };

void QueueRandomStat(RandomStatQueue& queue, ActionResultField field,
                     StatDirection direction) {
  constexpr std::int32_t kRandomBound = 4;
  const auto amount = queue.DrawBounded(kRandomBound) + 1;
  auto change = queue.ReadChange(field);
  change.amount = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(change.amount + amount));
  change.mode = static_cast<std::uint8_t>(direction);
  queue.WriteChange(field, change);
}

std::int16_t DoubledHp(const ActorActionResult& target) {
  const auto hp =
      target.changes.at(static_cast<std::size_t>(ActionResultField::kHp))
          .amount;
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp * 2));
}
}  // namespace

// decomp: _increaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12717).
void QueueRandomStrengthIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kStrength,
                  StatDirection::kIncrease);
}

// decomp: _increaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12726).
void QueueRandomIntelligenceIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kIntelligence,
                  StatDirection::kIncrease);
}

// decomp: _increaseAgilityRand (src/BATTLE/BATTLE.PRG/146C.c:12735).
void QueueRandomAgilityIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kAgility, StatDirection::kIncrease);
}

// decomp: _increaseMaxHpRand (src/BATTLE/BATTLE.PRG/146C.c:12744).
void QueueRandomMaxHpIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kMaxHp, StatDirection::kIncrease);
}

// decomp: _increaseMaxMpRand (src/BATTLE/BATTLE.PRG/146C.c:12753).
void QueueRandomMaxMpIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kMaxMp, StatDirection::kIncrease);
}

// decomp: _decreaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12762).
void QueueRandomStrengthDecrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kStrength,
                  StatDirection::kDecrease);
}

// decomp: _decreaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12771).
void QueueRandomIntelligenceDecrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kIntelligence,
                  StatDirection::kDecrease);
}

// decomp: _decreaseAglityRand (src/BATTLE/BATTLE.PRG/146C.c:12780).
void QueueRandomAgilityDecrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kAgility, StatDirection::kDecrease);
}

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
