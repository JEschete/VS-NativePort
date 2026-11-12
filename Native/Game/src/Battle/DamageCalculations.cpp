// Hand-maintained (tools/port/owned.csv).
#include "Battle/DamageCalculations.hpp"

#include <algorithm>
#include <bit>

#include "Battle/DamagePolicies.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
using Core::Math::R3000Quotient;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;
enum class Kind { kPhysical, kSpell, kHealing };
constexpr int kPercent = 100;
constexpr int kRatingDivisor = 4;
constexpr int kStatFactorScale = 10;
constexpr int kMaximumStatFactor = 31;
constexpr int kDefenseOffset = 20;
constexpr int kDamageVariance = 5;
constexpr int kCriticalActionLimit = 6;
constexpr int kCriticalRiskDivisor = 10;
constexpr int kCriticalBaseChance = 10;
constexpr int kHealingVariance = 11;
constexpr int kHealingVarianceOffset = 5;
std::int16_t Narrow(std::int32_t value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
int DpPenalty(std::uint16_t current, std::uint16_t maximum) {
  return maximum == 0
             ? 0
             : (static_cast<int>(maximum) - current) * kPercent / maximum;
}
int PpBonus(std::uint16_t current, std::uint16_t maximum, int coefficient) {
  return maximum == 0
             ? 0
             : R3000Quotient(WrappingMultiply(current, coefficient), maximum);
}
std::uint8_t BestAffinity(const Items::AffinityTotals& affinities) {
  std::uint8_t best = 0;
  std::int16_t maximum = 0;
  for (std::uint8_t i = 0; i < Items::kAffinityCount; ++i) {
    if (affinities.at(i) > maximum) {
      maximum = affinities.at(i);
      best = i;
    }
  }
  return best;
}
std::uint8_t TargetAffinity(const DamageLimb& limb,
                            const Items::ShieldLoadout& shield, bool drawn) {
  Items::AffinityTotals totals{};
  for (std::size_t i = 0; i < Items::kAffinityCount; ++i) {
    totals.at(i) = Narrow(limb.affinities.at(i) +
                          limb.armor.class_affinity.affinities.at(0).at(i));
    if (drawn) {
      totals.at(i) =
          Narrow(totals.at(i) + shield.class_affinity.affinities.at(0).at(i));
    }
  }
  return BestAffinity(totals);
}
struct Calculation {
  DamageOwner* source{};
  DamageOwner* target{};
  DamageStats from{};
  DamageStats to{};
  DamageLimb limb{};
  DamageHit hit{};
  int limb_index{};
  bool source_drawn{};
  bool target_drawn{};
  std::uint8_t source_affinity{};
  std::uint8_t target_affinity{};
};
std::optional<Calculation> Prepare(const DamageTarget* source,
                                   const DamageTarget* target, int hit,
                                   Kind kind, DamageWorld& world) {
  if (source == nullptr || target == nullptr) return std::nullopt;
  Calculation c;
  c.source = world.Stats(source->actor);
  c.target = world.Stats(target->actor);
  if (c.source == nullptr || c.target == nullptr) return std::nullopt;
  if (target->limb < 0) return std::nullopt;
  c.limb_index = static_cast<std::uint8_t>(target->limb);
  c.from = c.source->Read();
  c.to = c.target->Read();
  const auto limb = c.target->Limb(c.limb_index);
  const auto parameters = world.Hit(hit);
  if (!limb || !parameters) return std::nullopt;
  c.limb = limb.value();
  c.hit = parameters.value();
  const auto from_drawn = world.WeaponDrawn(c.from.actor_id);
  const auto to_drawn = world.WeaponDrawn(c.to.actor_id);
  if (!from_drawn || !to_drawn) return std::nullopt;
  c.source_drawn = (from_drawn.value() & 1) != 0;
  c.target_drawn = (to_drawn.value() & 1) != 0;
  if (kind == Kind::kPhysical && c.hit.type == 0) {
    c.hit.type = c.from.weapon.damage_type == 0 ? 1 : c.from.weapon.damage_type;
  }
  if (c.hit.type >= Items::kPartTypeSlots ||
      c.hit.affinity > Items::kAffinityCount ||
      c.hit.factor > kMaximumStatFactor) {
    return std::nullopt;
  }
  if (kind == Kind::kHealing &&
      (c.hit.affinity == 0 || (c.hit.affinity == 1 && c.source_drawn))) {
    return std::nullopt;
  }
  c.source_affinity =
      c.hit.affinity == 0
          ? (c.source_drawn && c.from.weapon.blade.id != 0
                 ? BestAffinity(c.from.weapon.class_affinity.affinities.at(0))
                 : 0)
          : static_cast<std::uint8_t>(c.hit.affinity - 1);
  if (kind == Kind::kPhysical ||
      (kind == Kind::kSpell && c.hit.affinity == 0)) {
    c.target_affinity = TargetAffinity(c.limb, c.to.shield, c.target_drawn);
  } else {
    c.target_affinity = c.source_affinity;
  }
  if (kind == Kind::kPhysical &&
      (c.from.enemy_class >= Items::kPartClassSlots ||
       c.to.enemy_class >= Items::kPartClassSlots)) {
    return std::nullopt;
  }
  return c;
}
std::optional<int> Attack(const Calculation& c, Kind kind, DamageWorld& world) {
  const bool physical = kind == Kind::kPhysical;
  const auto& weapon = c.from.weapon;
  const auto& accessory = c.from.accessory;
  auto stat = Narrow(physical ? c.from.strength : c.from.intelligence);
  std::int16_t factor = 0;
  if (c.source_drawn) {
    stat = Narrow(stat + (physical ? weapon.attributes.current_strength
                                   : weapon.attributes.current_intelligence));
    factor = weapon.class_affinity.affinities.at(0).at(c.source_affinity);
    if (physical) {
      factor = Narrow(factor + weapon.damage_type_value +
                      weapon.class_affinity.classes.at(0).at(c.to.enemy_class));
    }
    factor = Narrow(factor - DpPenalty(weapon.current_dp, weapon.max_dp));
    int coefficient = kPercent;
    if (physical && weapon.max_pp != 0) {
      const auto bonus = world.BladeBonus(weapon.blade.category);
      if (!bonus) return std::nullopt;
      coefficient = bonus.value();
    }
    factor = Narrow(WrappingAdd(
        factor, PpBonus(weapon.current_pp, weapon.max_pp, coefficient)));
  }
  stat = Narrow(stat + (physical ? accessory.current_strength
                                 : accessory.current_intelligence));
  factor = Narrow(factor + accessory.types.at(c.hit.type) +
                  accessory.affinities.at(c.source_affinity) +
                  (physical ? accessory.classes.at(c.to.enemy_class) : 0));
  factor = Narrow(factor / kRatingDivisor + kPercent);
  const auto attack = WrappingMultiply(stat, factor) / kPercent;
  return WrappingMultiply(attack, kPercent + kStatFactorScale * c.hit.factor) /
         kPercent;
}
int Defense(const Calculation& c, Kind kind) {
  const bool physical = kind == Kind::kPhysical;
  const bool use_type = physical || c.hit.type != 0;
  const bool use_affinity = kind != Kind::kHealing || c.source_affinity != 0;
  const auto& armor = c.limb.armor;
  const auto& shield = c.to.shield;
  const auto& accessory = c.to.accessory;
  auto base_factor =
      Narrow(kPercent + (use_type ? c.limb.types.at(c.hit.type) : 0));
  auto armor_factor =
      Narrow(kPercent + (use_type ? armor.types.at(c.hit.type) : 0));
  std::int16_t shield_factor = kPercent;
  if (use_affinity) {
    base_factor = Narrow(base_factor + c.limb.affinities.at(c.source_affinity));
    armor_factor =
        Narrow(armor_factor +
               armor.class_affinity.affinities.at(0).at(c.source_affinity));
  }
  if (physical) {
    armor_factor = Narrow(armor_factor + armor.class_affinity.classes.at(0).at(
                                             c.from.enemy_class));
  }
  armor_factor =
      Narrow(armor_factor -
             DpPenalty(armor.current_dp, armor.max_dp) / kRatingDivisor);
  std::int16_t shield_stat = 0;
  if (c.target_drawn) {
    shield_stat = physical ? shield.attributes.current_strength
                           : shield.attributes.current_intelligence;
    shield_factor = Narrow(
        shield_factor + (use_type ? shield.types.at(c.hit.type) : 0) +
        (use_affinity
             ? shield.class_affinity.affinities.at(0).at(c.source_affinity)
             : 0) +
        (physical ? shield.class_affinity.classes.at(0).at(c.from.enemy_class)
                  : 0));
    shield_factor =
        Narrow(shield_factor -
               DpPenalty(shield.current_dp, shield.max_dp) / kRatingDivisor);
    shield_factor = Narrow(shield_factor +
                           PpBonus(shield.current_pp, shield.max_pp, kPercent) /
                               kRatingDivisor);
  }
  auto stat = Narrow(physical ? c.to.strength : c.to.intelligence);
  stat = Narrow(stat + (physical ? accessory.current_strength
                                 : accessory.current_intelligence));
  base_factor =
      Narrow(base_factor + accessory.types.at(c.hit.type) +
             accessory.affinities.at(c.source_affinity) +
             (physical ? accessory.classes.at(c.from.enemy_class) : 0));
  const auto armor_stat = physical ? armor.attributes.current_strength
                                   : armor.attributes.current_intelligence;
  auto defense = WrappingMultiply(stat - kDefenseOffset, base_factor);
  defense = WrappingAdd(defense, WrappingMultiply(shield_stat, shield_factor));
  defense = WrappingAdd(defense, WrappingMultiply(armor_stat, armor_factor));
  return R3000Quotient(defense, (c.to.risk >> 1) + kPercent);
}
std::optional<std::int16_t> BaseAmount(const Calculation& c, Kind kind,
                                       DamageWorld& world) {
  const auto attack = Attack(c, kind, world);
  const auto type = world.ActionType();
  if (!attack || !type) return std::nullopt;
  const auto raw = Narrow(WrappingSubtract(attack.value(), Defense(c, kind)));
  return std::max<std::int16_t>(
      0, ApplyDamageGemMultiplier(type.value(), raw, c.from.weapon.gems,
                                  c.to.shield.gems));
}
void GrowSpellDefense(const Calculation& c, int hit, int random,
                      DamageWorld& world) {
  if (random != 0 && hit == 0 && c.source_affinity != 0 &&
      world.TargetPosition() == 0) {
    c.target->GrowDefender(0, c.source_affinity + 1, c.limb_index);
  }
}
}  // namespace

// decomp: _getPhysicalAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11271).
std::optional<std::int16_t> CalculatePhysicalHitDamage(
    const DamageTarget* source, const DamageTarget* target, int hit, int random,
    bool apply_statuses, DamageWorld& world) {
  const auto prepared = Prepare(source, target, hit, Kind::kPhysical, world);
  if (!prepared) return std::nullopt;
  const auto& c = prepared.value();
  const auto base = BaseAmount(c, Kind::kPhysical, world);
  if (!base) return std::nullopt;
  auto value = base.value();
  if (random != 0) {
    value =
        Narrow(WrappingAdd(value + 1, world.RandomBounded(kDamageVariance)));
  }
  if (apply_statuses && (c.source->Statuses() & 2) != 0) {
    value = Narrow(value / 2);
  }
  if (random != 0) {
    const auto id = world.ActionId();
    if (!id) return std::nullopt;
    if (id.value() < kCriticalActionLimit) {
      const auto draw = world.RandomBounded(kPercent);
      if (draw <
          c.source->Risk() / kCriticalRiskDivisor + kCriticalBaseChance) {
        value = Narrow(value * 2);
        world.SetCriticalHit();
      }
    }
    if (hit == 0 && world.TargetPosition() == 0) {
      c.source->GrowAttacker(c.target->EnemyClass() + 1, c.target_affinity + 1);
      c.target->GrowDefender(c.source->EnemyClass() + 1, c.source_affinity + 1,
                             c.limb_index);
    }
  }
  return value;
}
// decomp: _getPhysicalAttackDamageWithoutStatusModifiers
// (src/BATTLE/BATTLE.PRG/146C.c:11467).
std::optional<std::int16_t> PhysicalDamageWithoutDebuffs(
    const DamageTarget* source, const DamageTarget* target, int hit, int random,
    DamageWorld& world) {
  return CalculatePhysicalHitDamage(source, target, hit, random, false, world);
}
// decomp: _getMagicAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11473).
std::optional<std::int16_t> CalculateSpellHitDamage(const DamageTarget* source,
                                                    const DamageTarget* target,
                                                    int hit, int random,
                                                    DamageWorld& world) {
  const auto prepared = Prepare(source, target, hit, Kind::kSpell, world);
  if (!prepared) return std::nullopt;
  const auto& c = prepared.value();
  const auto base = BaseAmount(c, Kind::kSpell, world);
  if (!base) return std::nullopt;
  auto value = base.value();
  if (random != 0) {
    value =
        Narrow(WrappingAdd(value + 1, world.RandomBounded(kDamageVariance)));
  }
  GrowSpellDefense(c, hit, random, world);
  return value;
}
// decomp: _hpHealed (src/BATTLE/BATTLE.PRG/146C.c:11850).
std::optional<std::int16_t> CalculateSpellHealing(const DamageTarget* source,
                                                  const DamageTarget* target,
                                                  int hit, int random,
                                                  DamageWorld& world) {
  const auto prepared = Prepare(source, target, hit, Kind::kHealing, world);
  if (!prepared) return std::nullopt;
  const auto& c = prepared.value();
  const auto base = BaseAmount(c, Kind::kHealing, world);
  if (!base) return std::nullopt;
  const auto variance =
      random != 0 ? WrappingSubtract(world.RandomAveraged(kHealingVariance),
                                     kHealingVarianceOffset)
                  : 0;
  const auto value =
      std::max<std::int16_t>(0, Narrow(WrappingAdd(base.value(), variance)));
  GrowSpellDefense(c, hit, random, world);
  return value;
}
// decomp: _getPhysicalAttackDamageWithStatusModifiers
// (src/BATTLE/BATTLE.PRG/146C.c:12235).
std::optional<std::int16_t> PhysicalDamageWithDebuffs(
    const DamageTarget* source, const DamageTarget* target, int hit, int random,
    DamageWorld& world) {
  return CalculatePhysicalHitDamage(source, target, hit, random, true, world);
}
}  // namespace VS::Battle
