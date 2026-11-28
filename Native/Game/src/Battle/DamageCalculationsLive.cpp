// Hand-maintained (tools/port/owned.csv).
#include "Battle/DamageCalculationsLive.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/DamageCalculations.hpp"
#include "Battle/Hit.hpp"
#include "Core/RandomLive.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/ItemRecordsLive.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Battle {
DamageStats ReadLiveDamageStats(const ActorStats& stats) {
  return {stats.actor_id, static_cast<std::uint8_t>(stats.enemy_class),
          stats.strength, stats.intelligence,
          stats.risk,     stats.weapon,
          stats.shield,   stats.accessory};
}

std::optional<DamageLimb> ReadLiveDamageLimb(const ActorStats& stats,
                                             int index) {
  if (index < 0 || static_cast<std::size_t>(index) >= stats.limbs.size()) {
    return std::nullopt;
  }
  const auto& limb = stats.limbs.at(static_cast<std::size_t>(index));
  DamageLimb result;
  for (std::size_t i = 0; i < result.types.size(); ++i) {
    result.types.at(i) = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(limb.types.at(i)));
  }
  for (std::size_t i = 0; i < result.affinities.size(); ++i) {
    result.affinities.at(i) = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(limb.affinities.at(i)));
  }
  result.armor = limb.armor;
  return result;
}

namespace {
class LiveDamageOwner final : public DamageOwner {
 public:
  LiveDamageOwner(VS::Battle::ActorStats& stats, ActionDefinition* action)
      : stats_(stats), action_(action) {}
  DamageStats Read() override { return ReadLiveDamageStats(stats_); }
  std::optional<DamageLimb> Limb(int index) override {
    return ReadLiveDamageLimb(stats_, index);
  }
  std::uint32_t Statuses() override {
    return static_cast<std::uint32_t>(stats_.statuses);
  }
  std::uint16_t Risk() override { return stats_.risk; }
  std::uint8_t EnemyClass() override {
    return static_cast<std::uint8_t>(stats_.enemy_class);
  }
  void GrowAttacker(int target_class, int target_affinity) override {
    Items::GrowLiveAttackerWeaponRatings(action_, &stats_, target_class,
                                         target_affinity);
  }
  void GrowDefender(int source_class, int source_affinity, int limb) override {
    Items::GrowLiveDefenderEquipmentRatings(action_, &stats_, source_class,
                                            source_affinity, limb);
  }

 private:
  VS::Battle::ActorStats& stats_;
  ActionDefinition* action_;
};
class LiveDamageWorld final : public DamageWorld {
 public:
  LiveDamageWorld(ActionDefinition* action, VS::Battle::HitResult* target)
      : action_(action), target_(target) {}
  DamageOwner* Stats(std::uint8_t slot) override {
    if (slot >= std::size(VS::Battle::actors) ||
        VS::Battle::actors[slot] == nullptr ||
        VS::Battle::actors[slot]->stats == nullptr ||
        count_ >= owners_.size()) {
      return nullptr;
    }
    return &owners_.at(count_++).emplace(*VS::Battle::actors[slot]->stats,
                                         action_);
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    if (slot >= std::size(VS::Battle::actors) ||
        VS::Battle::actors[slot] == nullptr) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(VS::Battle::actors[slot]->weapon_drawn);
  }
  std::optional<DamageHit> Hit(int hit) override {
    if (action_ == nullptr || hit < 0 ||
        static_cast<std::size_t>(hit) >= std::size(action_->hits)) {
      return std::nullopt;
    }
    return DamageHit{static_cast<std::uint8_t>(action_->hits[hit].damage_type),
                     static_cast<std::uint8_t>(action_->hits[hit].affinity),
                     static_cast<std::uint8_t>(action_->hits[hit].stat_factor)};
  }
  std::optional<std::uint8_t> ActionType() override {
    return action_ ? std::optional{static_cast<std::uint8_t>(action_->type)}
                   : std::nullopt;
  }
  std::optional<std::uint8_t> ActionId() override {
    return action_ ? std::optional{action_->id} : std::nullopt;
  }
  std::optional<std::uint16_t> BladeBonus(std::uint8_t category) override {
    if (category >= std::size(VS::Battle::bladePpDamageBonuses)) {
      return std::nullopt;
    }
    return VS::Battle::bladePpDamageBonuses[category];
  }
  std::int32_t TargetPosition() override { return target_->targets_position; }
  std::int32_t RandomBounded(int bound) override {
    return Core::NextLiveRandomBounded(bound);
  }
  std::int32_t RandomAveraged(int bound) override {
    return Core::NextLiveRandomAveraged(bound);
  }
  void SetCriticalHit() override { VS::Battle::criticalHitOccurred = 1; }

 private:
  ActionDefinition* action_;
  VS::Battle::HitResult* target_;
  std::array<std::optional<LiveDamageOwner>, 2> owners_{};
  std::size_t count_{};
};
std::optional<DamageTarget> Target(const VS::Battle::HitResult* hit) {
  if (hit == nullptr) return std::nullopt;
  return DamageTarget{hit->target.target_actor,
                      std::bit_cast<std::int8_t>(
                          static_cast<std::uint8_t>(hit->target.target_limb))};
}
std::int16_t Checked(std::optional<std::int16_t> value) {
  if (!value) {
    VS::Core::Errorf(
        "battle",
        "Damage calculation refused missing required record, used index, "
        "or unsupported healing affinity (zero; drawn one, #351)");
    std::abort();
  }
  return value.value();
}
using Calculator = decltype(&CalculateSpellHitDamage);
std::int16_t Calculate(Calculator function, ActionDefinition* action,
                       VS::Battle::HitResult* source,
                       VS::Battle::HitResult* target, int hit, int random) {
  LiveDamageWorld world(action, target);
  const auto from = Target(source), to = Target(target);
  return Checked(function(from ? &from.value() : nullptr,
                          to ? &to.value() : nullptr, hit, random, world));
}
}  // namespace
// decomp: getPhysicalAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11202; was
// _getPhysicalAttackDamage).
std::int16_t LiveCalculatePhysicalHitDamage(ActionDefinition* action,
                                            VS::Battle::HitResult* source,
                                            VS::Battle::HitResult* target,
                                            int hit, int random,
                                            int apply_statuses) {
  LiveDamageWorld world(action, target);
  const auto from = Target(source), to = Target(target);
  return Checked(CalculatePhysicalHitDamage(
      from ? &from.value() : nullptr, to ? &to.value() : nullptr, hit, random,
      apply_statuses != 0, world));
}
// decomp: getPhysicalAttackDamageWithoutStatusModifiers
// (src/BATTLE/BATTLE.PRG/146C.c:11398; was
// _getPhysicalAttackDamageWithoutStatusModifiers).
std::int16_t LivePhysicalDamageWithoutDebuffs(ActionDefinition* action,
                                              VS::Battle::HitResult* source,
                                              VS::Battle::HitResult* target,
                                              int hit, int random) {
  return Calculate(PhysicalDamageWithoutDebuffs, action, source, target, hit,
                   random);
}
// decomp: getMagicAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11404; was
// _getMagicAttackDamage).
std::int16_t LiveCalculateSpellHitDamage(ActionDefinition* action,
                                         VS::Battle::HitResult* source,
                                         VS::Battle::HitResult* target, int hit,
                                         int random) {
  return Calculate(CalculateSpellHitDamage, action, source, target, hit,
                   random);
}
// decomp: calculateHpHealed (src/BATTLE/BATTLE.PRG/146C.c:11774; was
// _hpHealed).
std::int16_t LiveCalculateSpellHealing(ActionDefinition* action,
                                       VS::Battle::HitResult* source,
                                       VS::Battle::HitResult* target, int hit,
                                       int random) {
  return Calculate(CalculateSpellHealing, action, source, target, hit, random);
}
// decomp: getPhysicalAttackDamageWithStatusModifiers
// (src/BATTLE/BATTLE.PRG/146C.c:12150; was
// _getPhysicalAttackDamageWithStatusModifiers).
std::int16_t LivePhysicalDamageWithDebuffs(ActionDefinition* action,
                                           VS::Battle::HitResult* source,
                                           VS::Battle::HitResult* target,
                                           int hit, int random) {
  return Calculate(PhysicalDamageWithDebuffs, action, source, target, hit,
                   random);
}
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LivePhysicalDamageWithoutDebuffs)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveCalculateSpellHitDamage)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveCalculateSpellHealing)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LivePhysicalDamageWithDebuffs)&>);
}  // namespace VS::Battle
