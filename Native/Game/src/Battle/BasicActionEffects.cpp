// Hand-maintained (tools/port/owned.csv).
#include "Battle/BasicActionEffects.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace VS::Battle {
namespace {
enum class StatDirection : std::uint8_t { kDecrease = 1, kIncrease = 2 };
constexpr std::uint8_t kPhysicalAffinity = 0;
constexpr std::uint8_t kAirAffinity = 1;
constexpr std::uint8_t kFireAffinity = 2;
constexpr std::uint8_t kEarthAffinity = 3;
constexpr std::uint8_t kWaterAffinity = 4;
constexpr std::uint8_t kLightAffinity = 5;
constexpr std::uint8_t kDarkAffinity = 6;
constexpr std::uint8_t kCriticalHpDamage = 3;

void QueueTriggeredRecovery(TriggeredRecoveryQueue& queue) {
  const auto amount = queue.Calculate();
  auto change = queue.ReadHp();
  change.mode = 2;
  change.amount = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(change.amount + amount));
  queue.WriteHp(change);
  queue.RecoverLimbs(amount);
}

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

// decomp: func_80084390 (src/BATTLE/BATTLE.PRG/146C.c:12606).
void QueueSpellTriggeredRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedActionType() == 1) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084440 (src/BATTLE/BATTLE.PRG/146C.c:12617).
void QueueNonSpellTriggeredRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedActionType() != 1) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_800844F0 (src/BATTLE/BATTLE.PRG/146C.c:12628).
void QueuePhysicalAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kPhysicalAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084570 (src/BATTLE/BATTLE.PRG/146C.c:12639).
void QueueAirAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kAirAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_800845F0 (src/BATTLE/BATTLE.PRG/146C.c:12650).
void QueueFireAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kFireAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084670 (src/BATTLE/BATTLE.PRG/146C.c:12661).
void QueueEarthAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kEarthAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_800846F0 (src/BATTLE/BATTLE.PRG/146C.c:12672).
void QueueWaterAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kWaterAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084770 (src/BATTLE/BATTLE.PRG/146C.c:12683).
void QueueLightAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kLightAffinity) {
    QueueTriggeredRecovery(queue);
  }
}

// decomp: func_800847F0 (src/BATTLE/BATTLE.PRG/146C.c:12694).
void QueueDarkAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kDarkAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084870 (src/BATTLE/BATTLE.PRG/146C.c:12705).
void QueueDamageTriggeredRecovery(std::uint8_t primary_hp_mode,
                                  TriggeredRecoveryQueue& queue) {
  if (primary_hp_mode == 1 || primary_hp_mode == kCriticalHpDamage) {
    QueueTriggeredRecovery(queue);
  }
}

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
