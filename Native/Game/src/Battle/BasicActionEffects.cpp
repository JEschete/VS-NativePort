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

// decomp: func_80084390 (src/BATTLE/BATTLE.PRG/146C.c:12476).
void QueueSpellTriggeredRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedActionType() == 1) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084440 (src/BATTLE/BATTLE.PRG/146C.c:12486).
void QueueNonSpellTriggeredRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedActionType() != 1) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_800844F0 (src/BATTLE/BATTLE.PRG/146C.c:12496).
void QueuePhysicalAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kPhysicalAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084570 (src/BATTLE/BATTLE.PRG/146C.c:12506).
void QueueAirAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kAirAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_800845F0 (src/BATTLE/BATTLE.PRG/146C.c:12516).
void QueueFireAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kFireAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084670 (src/BATTLE/BATTLE.PRG/146C.c:12526).
void QueueEarthAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kEarthAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_800846F0 (src/BATTLE/BATTLE.PRG/146C.c:12536).
void QueueWaterAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kWaterAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084770 (src/BATTLE/BATTLE.PRG/146C.c:12546).
void QueueLightAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kLightAffinity) {
    QueueTriggeredRecovery(queue);
  }
}

// decomp: func_800847F0 (src/BATTLE/BATTLE.PRG/146C.c:12556).
void QueueDarkAffinityRecovery(TriggeredRecoveryQueue& queue) {
  if (queue.SelectedAffinity() == kDarkAffinity) {
    QueueTriggeredRecovery(queue);
  }
}
// decomp: func_80084870 (src/BATTLE/BATTLE.PRG/146C.c:12566).
void QueueDamageTriggeredRecovery(std::uint8_t primary_hp_mode,
                                  TriggeredRecoveryQueue& queue) {
  if (primary_hp_mode == 1 || primary_hp_mode == kCriticalHpDamage) {
    QueueTriggeredRecovery(queue);
  }
}

// decomp: increaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12577; was
// _increaseStrengthRand).
void QueueRandomStrengthIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kStrength,
                  StatDirection::kIncrease);
}

// decomp: increaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12585; was
// _increaseIntelligenceRand).
void QueueRandomIntelligenceIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kIntelligence,
                  StatDirection::kIncrease);
}

// decomp: increaseAgilityRand (src/BATTLE/BATTLE.PRG/146C.c:12593; was
// _increaseAgilityRand).
void QueueRandomAgilityIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kAgility, StatDirection::kIncrease);
}

// decomp: increaseMaxHpRand (src/BATTLE/BATTLE.PRG/146C.c:12601; was
// _increaseMaxHpRand).
void QueueRandomMaxHpIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kMaxHp, StatDirection::kIncrease);
}

// decomp: increaseMaxMpRand (src/BATTLE/BATTLE.PRG/146C.c:12609; was
// _increaseMaxMpRand).
void QueueRandomMaxMpIncrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kMaxMp, StatDirection::kIncrease);
}

// decomp: decreaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12617; was
// _decreaseStrengthRand).
void QueueRandomStrengthDecrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kStrength,
                  StatDirection::kDecrease);
}

// decomp: decreaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12625; was
// _decreaseIntelligenceRand).
void QueueRandomIntelligenceDecrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kIntelligence,
                  StatDirection::kDecrease);
}

// decomp: decreaseAglityRand (src/BATTLE/BATTLE.PRG/146C.c:12633; was
// _decreaseAglityRand).
void QueueRandomAgilityDecrease(RandomStatQueue& queue) {
  QueueRandomStat(queue, ActionResultField::kAgility, StatDirection::kDecrease);
}

// decomp: func_80084228 / func_800842F0 / func_80084370
// (src/BATTLE/BATTLE.PRG/146C.c:12470:12547/12572/12599).
void MarkSourceActionSuccess(SourceActionOutcome& outcome) {
  outcome = SourceActionOutcome::kSuccess;
}

// decomp: func_80084248 (src/BATTLE/BATTLE.PRG/146C.c:12432).
void QueueInstantDeath(InstantDeathQueue& queue) {
  queue.RemoveStatuses(StatusRuleId::kInstantDefeat);
  const auto hp = queue.CurrentHp();
  queue.QueueHp({std::bit_cast<std::int16_t>(hp), 1});
}

// decomp: func_800842AC (src/BATTLE/BATTLE.PRG/146C.c:12440).
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

// decomp: func_80084310 (src/BATTLE/BATTLE.PRG/146C.c:12454).
void SetActionFlag64(bool execute, ActionScriptFlags& flags) {
  if (execute) {
    flags.Set(ActionScriptFlag::kFlag64, 1);
  }
}

// decomp: func_80084340 (src/BATTLE/BATTLE.PRG/146C.c:12462).
void SetActionFlag32(bool execute, ActionScriptFlags& flags) {
  if (execute) {
    flags.Set(ActionScriptFlag::kFlag32, 1);
  }
}
}  // namespace VS::Battle
