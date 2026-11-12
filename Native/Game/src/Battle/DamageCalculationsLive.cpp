// Hand-maintained (tools/port/owned.csv).
#include "Battle/DamageCalculationsLive.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Battle/DamageCalculations.hpp"
#include "Core/RandomLive.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/ItemRecordsLive.hpp"
extern "C" {
#include "Battle/Damage.h"
#include "Battle/Hit.h"
}

namespace VS::Battle {
namespace {
class LiveDamageOwner final : public DamageOwner {
 public:
  LiveDamageOwner(Battle_ActorStats& stats, Battle_ActionDefinition* action)
      : stats_(stats), action_(action) {}
  DamageStats Read() override {
    return {stats_.actorId,
            static_cast<std::uint8_t>(stats_.enemyClass),
            stats_.strength,
            stats_.intelligence,
            stats_.risk,
            Items::Live::Typed(stats_.weapon),
            Items::Live::Typed(stats_.shield),
            Items::Live::Typed(stats_.accessory)};
  }
  std::optional<DamageLimb> Limb(int index) override {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= std::size(stats_.limbs)) {
      return std::nullopt;
    }
    const auto& limb = stats_.limbs[index];
    DamageLimb result;
    for (std::size_t i = 0; i < result.types.size(); ++i) {
      result.types.at(i) = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(limb.types[i]));
    }
    for (std::size_t i = 0; i < result.affinities.size(); ++i) {
      result.affinities.at(i) = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(limb.affinities[i]));
    }
    result.armor = Items::Live::Typed(limb.armor);
    return result;
  }
  std::uint32_t Statuses() override {
    return static_cast<std::uint32_t>(stats_.statuses);
  }
  std::uint16_t Risk() override { return stats_.risk; }
  std::uint8_t EnemyClass() override {
    return static_cast<std::uint8_t>(stats_.enemyClass);
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
  Battle_ActorStats& stats_;
  Battle_ActionDefinition* action_;
};
class LiveDamageWorld final : public DamageWorld {
 public:
  LiveDamageWorld(Battle_ActionDefinition* action, Battle_HitResult* target)
      : action_(action), target_(target) {}
  DamageOwner* Stats(std::uint8_t slot) override {
    if (slot >= std::size(Battle_actors) || Battle_actors[slot] == nullptr ||
        Battle_actors[slot]->stats == nullptr || count_ >= owners_.size()) {
      return nullptr;
    }
    return &owners_.at(count_++).emplace(*Battle_actors[slot]->stats, action_);
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    if (slot >= std::size(Battle_actors) || Battle_actors[slot] == nullptr) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(Battle_actors[slot]->weaponDrawn);
  }
  std::optional<DamageHit> Hit(int hit) override {
    if (action_ == nullptr || hit < 0 ||
        static_cast<std::size_t>(hit) >= std::size(action_->hits)) {
      return std::nullopt;
    }
    return DamageHit{static_cast<std::uint8_t>(action_->hits[hit].damageType),
                     static_cast<std::uint8_t>(action_->hits[hit].affinity),
                     static_cast<std::uint8_t>(action_->hits[hit].statFactor)};
  }
  std::optional<std::uint8_t> ActionType() override {
    return action_ ? std::optional{static_cast<std::uint8_t>(action_->type)}
                   : std::nullopt;
  }
  std::optional<std::uint8_t> ActionId() override {
    return action_ ? std::optional{action_->id} : std::nullopt;
  }
  std::optional<std::uint16_t> BladeBonus(std::uint8_t category) override {
    if (category >= std::size(Battle_bladePpDamageBonuses)) return std::nullopt;
    return Battle_bladePpDamageBonuses[category];
  }
  std::int32_t TargetPosition() override { return target_->targetsPosition; }
  std::int32_t RandomBounded(int bound) override {
    return Core::NextLiveRandomBounded(bound);
  }
  std::int32_t RandomAveraged(int bound) override {
    return Core::NextLiveRandomAveraged(bound);
  }
  void SetCriticalHit() override { Battle_criticalHitOccurred = 1; }

 private:
  Battle_ActionDefinition* action_;
  Battle_HitResult* target_;
  std::array<std::optional<LiveDamageOwner>, 2> owners_{};
  std::size_t count_{};
};
std::optional<DamageTarget> Target(const Battle_HitResult* hit) {
  if (hit == nullptr) return std::nullopt;
  return DamageTarget{hit->target.targetActor,
                      std::bit_cast<std::int8_t>(
                          static_cast<std::uint8_t>(hit->target.targetLimb))};
}
std::int16_t Checked(std::optional<std::int16_t> value) {
  if (!value) {
    Core_Errorf(
        "battle",
        "Damage calculation refused missing required record, used index, "
        "or unsupported healing affinity (zero; drawn one, #351)");
    std::abort();
  }
  return value.value();
}
using Calculator = decltype(&CalculateSpellHitDamage);
std::int16_t Calculate(Calculator function, Battle_ActionDefinition* action,
                       Battle_HitResult* source, Battle_HitResult* target,
                       int hit, int random) {
  LiveDamageWorld world(action, target);
  const auto from = Target(source), to = Target(target);
  return Checked(function(from ? &from.value() : nullptr,
                          to ? &to.value() : nullptr, hit, random, world));
}
}  // namespace
// decomp: _getPhysicalAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11271).
std::int16_t LiveCalculatePhysicalHitDamage(Battle_ActionDefinition* action,
                                            Battle_HitResult* source,
                                            Battle_HitResult* target, int hit,
                                            int random, int apply_statuses) {
  LiveDamageWorld world(action, target);
  const auto from = Target(source), to = Target(target);
  return Checked(CalculatePhysicalHitDamage(
      from ? &from.value() : nullptr, to ? &to.value() : nullptr, hit, random,
      apply_statuses != 0, world));
}
// decomp: _getPhysicalAttackDamageWithoutStatusModifiers
// (src/BATTLE/BATTLE.PRG/146C.c:11467).
std::int16_t LivePhysicalDamageWithoutDebuffs(Battle_ActionDefinition* action,
                                              Battle_HitResult* source,
                                              Battle_HitResult* target, int hit,
                                              int random) {
  return Calculate(PhysicalDamageWithoutDebuffs, action, source, target, hit,
                   random);
}
// decomp: _getMagicAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11473).
std::int16_t LiveCalculateSpellHitDamage(Battle_ActionDefinition* action,
                                         Battle_HitResult* source,
                                         Battle_HitResult* target, int hit,
                                         int random) {
  return Calculate(CalculateSpellHitDamage, action, source, target, hit,
                   random);
}
// decomp: _hpHealed (src/BATTLE/BATTLE.PRG/146C.c:11850).
std::int16_t LiveCalculateSpellHealing(Battle_ActionDefinition* action,
                                       Battle_HitResult* source,
                                       Battle_HitResult* target, int hit,
                                       int random) {
  return Calculate(CalculateSpellHealing, action, source, target, hit, random);
}
// decomp: _getPhysicalAttackDamageWithStatusModifiers
// (src/BATTLE/BATTLE.PRG/146C.c:12235).
std::int16_t LivePhysicalDamageWithDebuffs(Battle_ActionDefinition* action,
                                           Battle_HitResult* source,
                                           Battle_HitResult* target, int hit,
                                           int random) {
  return Calculate(PhysicalDamageWithDebuffs, action, source, target, hit,
                   random);
}
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LivePhysicalDamageWithoutDebuffs)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveCalculateSpellHitDamage)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveCalculateSpellHealing)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LivePhysicalDamageWithDebuffs)&>);
}  // namespace VS::Battle
