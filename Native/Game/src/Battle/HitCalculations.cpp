// Hand-maintained (tools/port/owned.csv).
#include "Battle/HitCalculations.hpp"

#include <cstddef>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;
constexpr int kPercent = 100;
constexpr int kFailure = 255;
constexpr std::uint8_t kExemptActor = 128;
constexpr std::uint32_t kDoubleHitStatus = 4;
enum class AttributeKind { kAgility, kIntelligence, kCostAdjusted };

int Sum(const HitAttributes& attributes, std::uint32_t drawn) {
  int sum = attributes.base + attributes.accessory;
  for (const auto armor : attributes.armor) sum += armor;
  if ((drawn & 1U) != 0) sum += attributes.weapon + attributes.shield;
  return sum;
}

std::optional<int> Difference(const HitAttributeStats& source,
                              const HitAttributeStats& target, std::size_t limb,
                              AttributeKind kind, HitAttributeWorld& world) {
  const auto from_drawn = world.WeaponDrawn(source.actor_id);
  const auto to_drawn = world.WeaponDrawn(target.actor_id);
  if (!from_drawn || !to_drawn) return std::nullopt;
  const bool intelligence = kind == AttributeKind::kIntelligence;
  const int from = Sum(intelligence ? source.intelligence : source.agility,
                       from_drawn.value());
  const int to = Sum(intelligence ? target.intelligence : target.agility,
                     to_drawn.value()) +
                 target.defense.at(limb);
  const int source_scaled =
      math::WrappingMultiply(from, kPercent - source.risk) / kPercent;
  int target_scaled =
      math::WrappingMultiply(to, kPercent - target.risk) / kPercent;
  if (intelligence) {
    target_scaled = math::R3000Quotient(
        math::WrappingMultiply(target_scaled, target.hp), target.max_hp);
  }
  int result = math::WrappingSubtract(source_scaled, target_scaled);
  if (kind == AttributeKind::kAgility) {
    result = math::WrappingAdd(result, kPercent);
  } else if (kind == AttributeKind::kCostAdjusted) {
    const auto cost = world.Cost();
    if (!cost) return std::nullopt;
    constexpr int kCostOffset = 130;
    result = math::WrappingSubtract(math::WrappingAdd(result, kCostOffset),
                                    cost.value());
  }
  return result;
}

std::optional<int> AttributeChance(const HitModifierTarget* source,
                                   const HitModifierTarget* target, int hit,
                                   int variance, AttributeKind kind,
                                   HitAttributeWorld& world) {
  if (source == nullptr || target == nullptr || target->limb < 0 ||
      static_cast<std::size_t>(target->limb) >= kHitModifierLimbCount) {
    return std::nullopt;
  }
  auto* from = world.Stats(source->actor);
  auto* to = world.Stats(target->actor);
  if (from == nullptr || to == nullptr) return std::nullopt;
  const auto difference =
      Difference(from->Read(), to->Read(),
                 static_cast<std::size_t>(target->limb), kind, world);
  if (!difference) return std::nullopt;
  int result = difference.value();
  if (variance != 0) {
    constexpr int kVarianceRange = 11;
    constexpr int kVarianceOffset = 5;
    const auto draw = world.RandomAveraged(kVarianceRange);
    result = math::WrappingAdd(math::WrappingSubtract(result, kVarianceOffset),
                               draw);
  }
  // Read the cached owners again: the callback may change gems, type or drawn.
  const auto attack = from->AttackBonus();
  if (!attack) return std::nullopt;
  result = math::WrappingAdd(result, attack.value());
  const auto defense = to->DefenseBonus(hit);
  if (!defense) return std::nullopt;
  result = math::WrappingSubtract(result, defense.value());
  if (result < 0) return 0;
  return result == kFailure ? kFailure - 1 : result;
}

std::optional<bool> Exempt(ActionCalculationWorld& world) {
  const auto target = world.Target();
  if (!target) return std::nullopt;
  if (target.value().position != 0) return false;
  const auto actor = world.Actor(target.value().actor);
  if (!actor) return std::nullopt;
  return actor.value().id == kExemptActor;
}

std::optional<int> InitialThreshold(int hit, int execute,
                                    ActionCalculationWorld& world) {
  const auto exempt = Exempt(world);
  if (!exempt) return std::nullopt;
  if (exempt.value()) return kPercent;
  const auto result = world.HitCalculator(hit, execute);
  if (!result) return std::nullopt;
  const auto type = world.ActionType();
  if (!type) return std::nullopt;
  if (type.value() == 1) {
    const auto target = world.Target();
    if (!target) return std::nullopt;
    const auto actor = world.Actor(target.value().actor);
    if (!actor) return std::nullopt;
    if (actor.value().statuses < 0) return 0;
  }
  return result;
}

std::optional<int> AdjustThreshold(int threshold,
                                   ActionCalculationWorld& world) {
  if (world.Prerequisite() == 0) return kFailure;
  if (threshold == kFailure) return threshold;
  const auto target = world.Target();
  if (!target) return std::nullopt;
  if (target.value().position == 0) {
    const auto actor = world.Actor(target.value().actor);
    if (!actor) return std::nullopt;
    if ((static_cast<std::uint32_t>(actor.value().statuses) &
         kDoubleHitStatus) != 0) {
      threshold = math::WrappingMultiply(threshold, 2);
    }
  }
  return threshold > kPercent ? kPercent : threshold;
}
}  // namespace

// decomp: _getAgilityDifference (src/BATTLE/BATTLE.PRG/146C.c:9890).
std::optional<int> AgilityHitChance(const HitModifierTarget* source,
                                    const HitModifierTarget* target, int hit,
                                    int variance, HitAttributeWorld& world) {
  return AttributeChance(source, target, hit, variance, AttributeKind::kAgility,
                         world);
}
// decomp: _getIntelligenceDifference (src/BATTLE/BATTLE.PRG/146C.c:9941).
std::optional<int> IntelligenceHitChance(const HitModifierTarget* source,
                                         const HitModifierTarget* target,
                                         int hit, int variance,
                                         HitAttributeWorld& world) {
  return AttributeChance(source, target, hit, variance,
                         AttributeKind::kIntelligence, world);
}
// decomp: _getAgilityDifference2 (src/BATTLE/BATTLE.PRG/146C.c:9992).
std::optional<int> CostAdjustedAgilityHitChance(const HitModifierTarget* source,
                                                const HitModifierTarget* target,
                                                int hit, int variance,
                                                HitAttributeWorld& world) {
  return AttributeChance(source, target, hit, variance,
                         AttributeKind::kCostAdjusted, world);
}
// decomp: _doesAttackHit (src/BATTLE/BATTLE.PRG/146C.c:10075).
std::optional<int> RollActionHit(int hit, int execute,
                                 ActionCalculationWorld& world) {
  const auto initial = InitialThreshold(hit, execute, world);
  if (!initial) return std::nullopt;
  const auto adjusted = AdjustThreshold(initial.value(), world);
  if (!adjusted) return std::nullopt;
  int threshold = adjusted.value();
  const auto target = world.Target();
  if (!target) return std::nullopt;
  if (hit == 0 || target.value().threshold == kFailure) {
    world.StoreThreshold(static_cast<std::uint8_t>(threshold));
  }
  const auto chance = world.RandomBounded(kPercent);
  if (threshold == kFailure) return 0;
  const auto source = world.Source();
  if (!source) return std::nullopt;
  if (source.value().position == 0 && source.value().actor == 0) {
    constexpr int kPlayerBonus = 10;
    threshold = math::WrappingAdd(threshold, kPlayerBonus);
  }
  return chance < threshold ? 1 : 0;
}
// decomp: _calculateStatChange (src/BATTLE/BATTLE.PRG/146C.c:12248).
std::optional<std::int16_t> CalculateActionStatChange(
    int hit, int execute, ActionCalculationWorld& world) {
  const auto exempt = Exempt(world);
  if (!exempt) return std::nullopt;
  constexpr std::int16_t kExemptAmount = 999;
  const auto amount = exempt.value() ? std::optional{kExemptAmount}
                                     : world.StatCalculator(hit, execute);
  if (!amount) return std::nullopt;
  world.StoreLastValue(
      static_cast<std::uint32_t>(static_cast<std::int32_t>(amount.value())));
  return amount;
}
}  // namespace VS::Battle
