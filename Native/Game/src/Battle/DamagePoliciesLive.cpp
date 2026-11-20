// Hand-maintained (tools/port/owned.csv).
#include "Battle/DamagePoliciesLive.hpp"

#include <array>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.hpp"
#include "Battle/DamagePolicies.hpp"
#include "Battle/Hit.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    VS::Core::Errorf(
        "battle",
        "Damage policy refused missing required action, hit, actor, "
        "stats or used index");
    std::abort();
  }
}
const Battle_ActorStats* StatsOf(const VS::Battle::HitResult* hit) {
  Require(hit != nullptr);
  const auto slot = hit->target.target_actor;
  Require(slot < std::size(VS::Battle::actors) &&
          VS::Battle::actors[slot] != nullptr);
  return VS::Battle::actors[slot]->stats;
}
auto HitAmounts(const VS::Battle::HitResult& hit) {
  std::array<std::int16_t, Items::kLimbCount> result{};
  for (std::size_t i = 0; i < result.size(); ++i) {
    result[i] = hit.limbs[i].effects.opaque0;
  }
  return result;
}
void CommitAmounts(VS::Battle::HitResult& hit, HitLimbAmounts amounts) {
  for (std::size_t i = 0; i < amounts.size(); ++i) {
    hit.limbs[i].effects.opaque0 = amounts[i];
  }
}
auto Vitals(const Battle_ActorStats& stats) {
  std::array<StatFactorVital, Items::kLimbCount> result{};
  for (std::size_t i = 0; i < result.size(); ++i) {
    result[i] = {stats.limbs[i].hp, stats.limbs[i].maxHp};
  }
  return result;
}
}  // namespace

// decomp: func_8007FE5C (src/BATTLE/BATTLE.PRG/146C.c:10534).
std::int16_t LiveApplyDamageGemMultiplier(Battle_ActionDefinition* action,
                                          std::int16_t damage,
                                          Battle_ActorStats* source,
                                          Battle_ActorStats* target) {
  Require(action != nullptr && source != nullptr && target != nullptr);
  std::array<Items::EquipmentPart, Items::kWeaponGemCount> weapon{}, shield{};
  for (std::size_t i = 0; i < weapon.size(); ++i) {
    weapon[i] = source->weapon.gems[i];
    shield[i] = target->shield.gems[i];
  }
  return ApplyDamageGemMultiplier(static_cast<std::uint8_t>(action->type),
                                  damage, weapon, shield);
}

// decomp: func_80080000 (src/BATTLE/BATTLE.PRG/146C.c:10612).
void LiveDistributeHitLimbDamage(Battle_ActionDefinition* action,
                                 VS::Battle::HitResult* hit,
                                 std::int16_t amount) {
  // The original clears before resolving any required actor/action input.
  ClearHitLimbArmorLoss(VS::Battle::limbArmorDpLoss);
  Require(action != nullptr);
  const auto* stats = StatsOf(hit);
  const auto category = static_cast<std::uint8_t>(action->category);
  const auto kind = DamageDistribution(category);
  std::array<StatFactorVital, Items::kLimbCount> vitals{};
  std::array<std::uint8_t, Items::kLimbCount> splash{};
  std::span<const StatFactorVital> used_vitals;
  std::span<const std::uint8_t> used_splash;
  if (kind == LimbDistribution::kEven) {
    Require(stats != nullptr);
    for (std::size_t i = 0; i < vitals.size(); ++i) {
      vitals[i].maximum = stats->limbs[i].maxHp;
    }
    used_vitals = vitals;
  } else if (kind == LimbDistribution::kSplash) {
    Require(stats != nullptr);
    const auto limb = hit->target.target_limb;
    Require(limb >= 0 && static_cast<std::size_t>(limb) < splash.size());
    for (std::size_t i = 0; i < splash.size(); ++i) {
      splash[i] = static_cast<std::uint8_t>(stats->dpSplash[limb][i]);
    }
    used_splash = splash;
  }
  auto amounts = HitAmounts(*hit);
  Require(DistributeHitLimbDamage(category, hit->target.target_limb, amount,
                                  used_vitals, used_splash, amounts,
                                  VS::Battle::limbArmorDpLoss));
  CommitAmounts(*hit, amounts);
}

// decomp: func_800801E0 (src/BATTLE/BATTLE.PRG/146C.c:10664).
void LiveDistributeLimbRecovery(Battle_ActionDefinition*,
                                VS::Battle::HitResult* hit,
                                std::int16_t amount) {
  const auto* stats = StatsOf(hit);
  Require(stats != nullptr);
  const auto vitals = Vitals(*stats);
  auto amounts = HitAmounts(*hit);
  DistributeLimbRecovery(amount, vitals, amounts, VS::Battle::limbArmorDpLoss);
  CommitAmounts(*hit, amounts);
}
// decomp: _getLastValue (src/BATTLE/BATTLE.PRG/146C.c:11237).
std::int16_t LiveLastActionStatValue(Battle_ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int) {
  return LastActionStatValue(VS::Battle::lastActionStatValue);
}
// decomp: _factorLastValue (src/BATTLE/BATTLE.PRG/146C.c:12228).
std::int16_t LiveScaleLastActionStatValue(Battle_ActionDefinition* action,
                                          VS::Battle::HitResult*,
                                          VS::Battle::HitResult*, int hit,
                                          int) {
  Require(action != nullptr && hit >= 0 &&
          static_cast<std::size_t>(hit) < std::size(action->hits));
  return ScaleLastActionStatValue(
      VS::Battle::lastActionStatValue,
      static_cast<std::uint8_t>(action->hits[hit].statFactor));
}
// decomp: _getRisk (src/BATTLE/BATTLE.PRG/146C.c:12241).
std::int16_t LiveTargetRisk(Battle_ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int) {
  const auto* stats = StatsOf(target);
  Require(stats != nullptr);
  return TargetRisk(stats->risk);
}
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveLastActionStatValue)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveScaleLastActionStatValue)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveTargetRisk)&>);
}  // namespace VS::Battle
