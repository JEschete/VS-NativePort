// Hand-maintained (tools/port/owned.csv).
#include "Field/Traps.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Field {
namespace {
constexpr std::int32_t kPercent = 100;
constexpr std::int32_t kRiskPercentBase = 150;
constexpr std::int32_t kRiskPercentScale = 256;
constexpr std::int32_t kLockBreakBonus = 20;
constexpr std::int32_t kAttackHpDivisor = 10;
constexpr std::int32_t kAttackPerTrapLevel = 10;
constexpr std::int32_t kNeutralFactor = 100;
constexpr std::int32_t kMediatingStatBase = 20;
constexpr std::int32_t kRiskDefenseBase = 100;
constexpr std::int32_t kQuarterRounding = 3;
constexpr std::int32_t kQuarterShift = 2;
constexpr std::int32_t kVariationSpread = 11;
constexpr std::int32_t kVariationCenter = 5;
constexpr std::uint8_t kLastDamageType = 3;
constexpr std::uint8_t kLastAffinity = 7;

std::int16_t Narrow(std::int32_t value) {
  return static_cast<std::int16_t>(value);
}

// The 32-bit wrapping sum the original's -fwrapv int arithmetic produces.
std::int32_t WrappingSum(std::int64_t value) {
  return static_cast<std::int32_t>(value);
}

// A quarter of `numerator` as a percent of `denominator`, rounded toward 0.
std::int32_t QuarterPercent(std::int32_t numerator, std::int32_t denominator) {
  std::int32_t percent = (numerator * kPercent) / denominator;
  if (percent < 0) percent += kQuarterRounding;
  return percent >> kQuarterShift;
}

// The halfword fields of _hitIntermediate the panel rules fill.
struct PanelCalculation {
  std::int16_t attack = 0;
  std::int16_t mediating = 0;
  std::int16_t shield_stat = 0;
  std::int16_t armor_stat = 0;
  std::int16_t stat_factor = kNeutralFactor;
  std::int16_t shield_factor = kNeutralFactor;
  std::int16_t armor_factor = kNeutralFactor;
};

void ApplyLimbDefense(PanelCalculation& calc, const TrapLimbDefense& limb,
                      std::size_t type, std::size_t affinity) {
  if (type != 0) {
    calc.stat_factor = Narrow(limb.types.at(type) + kNeutralFactor);
    calc.armor_factor = Narrow(limb.armor_types.at(type) + kNeutralFactor);
  }
  if (affinity != 0) {
    calc.stat_factor = Narrow(calc.stat_factor + limb.affinities.at(affinity));
    calc.armor_factor =
        Narrow(calc.armor_factor + limb.armor_affinities.at(affinity));
  }
  if (limb.armor_max_dp != 0) {
    calc.armor_factor =
        Narrow(calc.armor_factor -
               QuarterPercent(limb.armor_max_dp - limb.armor_current_dp,
                              limb.armor_max_dp));
  }
}

void ApplyShieldDefense(PanelCalculation& calc, const TrapShieldDefense& shield,
                        std::size_t type, std::size_t affinity) {
  calc.shield_stat = shield.strength;
  if (type != 0) {
    calc.shield_factor = Narrow(calc.shield_factor + shield.types.at(type));
  }
  if (affinity != 0) {
    calc.shield_factor =
        Narrow(calc.shield_factor + shield.affinities.at(affinity));
  }
  if (shield.max_dp != 0) {
    calc.shield_factor = Narrow(
        calc.shield_factor -
        QuarterPercent(shield.max_dp - shield.current_dp, shield.max_dp));
  }
  if (shield.max_pp != 0) {
    calc.shield_factor = Narrow(
        calc.shield_factor + QuarterPercent(shield.current_pp, shield.max_pp));
  }
}

std::int32_t PanelDefense(const PanelCalculation& calc, std::uint16_t risk) {
  const std::int32_t defense = WrappingSum(
      (static_cast<std::int64_t>(calc.mediating - kMediatingStatBase) *
       calc.stat_factor) +
      (static_cast<std::int64_t>(calc.shield_stat) * calc.shield_factor) +
      (static_cast<std::int64_t>(calc.armor_stat) * calc.armor_factor));
  return defense / ((risk / 2) + kRiskDefenseBase);
}

// The shared body of _damageFromTrapPanel and _hpHealedFromHealPanel up to
// the final clamps.
std::optional<std::int16_t> PanelAmount(const TrapPanelHit& hit,
                                        const TrapPanelTarget& target,
                                        std::int32_t trap_level,
                                        bool add_random_variation,
                                        TrapRandom& random) {
  if (hit.damage_type > kLastDamageType || hit.affinity == 0 ||
      hit.affinity > kLastAffinity) {
    return std::nullopt;
  }
  const std::size_t type = hit.damage_type;
  const std::size_t affinity = hit.affinity - 1U;
  PanelCalculation calc;
  calc.attack = Narrow(target.max_hp / kAttackHpDivisor);
  calc.attack = Narrow(calc.attack * hit.stat_factor);
  calc.attack = Narrow(calc.attack + ((trap_level + 1) * kAttackPerTrapLevel));
  calc.mediating = static_cast<std::int16_t>(target.strength);
  calc.armor_stat = target.limb.armor_strength;
  ApplyLimbDefense(calc, target.limb, type, affinity);
  if (target.shield_raised) {
    ApplyShieldDefense(calc, target.shield, type, affinity);
  }
  calc.mediating = Narrow(calc.mediating + target.accessory.strength);
  calc.stat_factor = Narrow(calc.stat_factor + target.accessory.types.at(type) +
                            target.accessory.affinities.at(affinity));
  auto amount = Narrow(calc.attack - PanelDefense(calc, target.risk));
  amount = std::max<std::int16_t>(amount, 0);
  const std::int32_t variation =
      add_random_variation
          ? random.NextAveraged(kVariationSpread) - kVariationCenter
          : 0;
  amount = Narrow(amount + variation);
  return amount;
}
}  // namespace

// decomp: BATTLE.PRG func_8007F434 (src/BATTLE/BATTLE.PRG/146C.c:10124)
bool RollRiskLockBreak(std::uint16_t risk, TrapRandom& random) {
  const std::int32_t percent = std::min(
      ((risk + kRiskPercentBase) * kPercent) / kRiskPercentScale, kPercent);
  return random.NextBounded(kPercent) < percent + kLockBreakBonus;
}

// decomp: BATTLE.PRG _damageFromTrapPanel (src/BATTLE/BATTLE.PRG/146C.c:11730)
std::optional<std::int16_t> CalculateTrapPanelDamage(
    const TrapPanelHit& hit, const TrapPanelTarget& target,
    std::int32_t trap_level, bool add_random_variation, TrapRandom& random) {
  const auto amount =
      PanelAmount(hit, target, trap_level, add_random_variation, random);
  if (!amount) return std::nullopt;
  // A trap panel never takes the last hit point.
  auto damage = std::max<std::int16_t>(*amount, 0);
  if (damage >= target.current_hp) damage = Narrow(target.current_hp - 1);
  return damage;
}

// decomp: BATTLE.PRG _hpHealedFromHealPanel
// (src/BATTLE/BATTLE.PRG/146C.c:11992)
std::optional<std::int16_t> CalculateHealingPanelRecovery(
    const TrapPanelHit& hit, const TrapPanelTarget& target,
    std::int32_t trap_level, bool add_random_variation, TrapRandom& random) {
  const auto amount =
      PanelAmount(hit, target, trap_level, add_random_variation, random);
  if (!amount) return std::nullopt;
  return std::max<std::int16_t>(*amount, 0);
}
}  // namespace VS::Field
