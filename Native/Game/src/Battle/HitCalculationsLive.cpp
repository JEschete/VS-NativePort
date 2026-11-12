// Hand-maintained (tools/port/owned.csv).
#include "Battle/HitCalculationsLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Battle/HitCalculations.hpp"
#include "Battle/HitModifiersLive.hpp"
#include "Core/RandomLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
[[noreturn]] void Refuse() {
  Core_Errorf("battle",
              "Action calculation refused missing state, used index or "
              "retired hit/stat/effect selector");
  std::abort();
}
template <typename T>
T Checked(std::optional<T> result) {
  if (!result) Refuse();
  return result.value();
}
Battle_ActorStats* StatsAt(std::uint8_t slot) {
  if (slot >= std::size(Battle_actors) || Battle_actors[slot] == nullptr) {
    return nullptr;
  }
  return Battle_actors[slot]->stats;
}

class LiveAttributeAccess final : public HitAttributeAccess {
 public:
  LiveAttributeAccess(Battle_ActorStats& owner, Battle_ActionDefinition* action)
      : owner_(owner), action_(action) {}
  HitAttributeStats Read() override {
    HitAttributeStats result;
    result.actor_id = owner_.actorId;
    result.risk = owner_.risk;
    result.hp = owner_.currentHp;
    result.max_hp = owner_.maxHp;
    result.agility = {owner_.agility,
                      owner_.accessory.currentAgility,
                      owner_.weapon.currentAgility,
                      owner_.shield.currentAgility,
                      {}};
    result.intelligence = {owner_.intelligence,
                           owner_.accessory.currentInt,
                           owner_.weapon.currentInt,
                           owner_.shield.currentInt,
                           {}};
    for (std::size_t i = 0; i < result.defense.size(); ++i) {
      result.agility.armor.at(i) = owner_.limbs[i].armor.currentAgility;
      result.intelligence.armor.at(i) = owner_.limbs[i].armor.currentInt;
      result.defense.at(i) = owner_.limbs[i].agilityDefenseBonus;
    }
    return result;
  }
  std::optional<std::int16_t> AttackBonus() override {
    return AttackLiveGemHitBonus(action_, &owner_);
  }
  std::optional<std::int16_t> DefenseBonus(int hit) override {
    return DefenseLiveGemHitBonus(action_, &owner_, hit);
  }

 private:
  Battle_ActorStats& owner_;
  Battle_ActionDefinition* action_;
};
class LiveAttributeWorld final : public HitAttributeWorld {
 public:
  explicit LiveAttributeWorld(Battle_ActionDefinition* action)
      : action_(action) {}
  HitAttributeAccess* Stats(std::uint8_t slot) override {
    auto* owner = StatsAt(slot);
    if (owner == nullptr || count_ >= stats_.size()) return nullptr;
    return &stats_.at(count_++).emplace(*owner, action_);
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    if (slot >= std::size(Battle_actors) || Battle_actors[slot] == nullptr) {
      return std::nullopt;
    }
    return std::bit_cast<std::uint32_t>(Battle_actors[slot]->weaponDrawn);
  }
  std::optional<std::uint8_t> Cost() override {
    return action_ ? std::optional{action_->cost} : std::nullopt;
  }
  std::int32_t RandomAveraged(int bound) override {
    return Core::NextLiveRandomAveraged(bound);
  }

 private:
  Battle_ActionDefinition* action_;
  std::array<std::optional<LiveAttributeAccess>, 2> stats_{};
  std::size_t count_{};
};
std::optional<HitModifierTarget> AttributeHit(const Battle_HitResult* hit) {
  if (hit == nullptr) return std::nullopt;
  return HitModifierTarget{hit->target.targetActor,
                           std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
                               hit->target.targetLimb)),
                           hit->targetsPosition};
}
using AttributeCalculator = decltype(&AgilityHitChance);
int CalculateAttributes(AttributeCalculator calculate,
                        Battle_ActionDefinition* action,
                        Battle_HitResult* source, Battle_HitResult* target,
                        int hit, int variance) {
  LiveAttributeWorld world(action);
  const auto from = AttributeHit(source);
  const auto to = AttributeHit(target);
  return Checked(calculate(from ? &from.value() : nullptr,
                           to ? &to.value() : nullptr, hit, variance, world));
}

std::optional<CalculationTarget> ReadTarget(const Battle_HitResult* hit) {
  if (hit == nullptr) return std::nullopt;
  return CalculationTarget{hit->target.targetActor, hit->targetsPosition,
                           hit->target.hitThreshold};
}
class LiveCalculationWorld final : public ActionCalculationWorld {
 public:
  LiveCalculationWorld(Battle_ActionDefinition* action,
                       Battle_HitResult* source, Battle_HitResult* target)
      : action_(action), source_(source), target_(target) {}
  std::optional<CalculationTarget> Target() override {
    return ReadTarget(target_);
  }
  std::optional<CalculationTarget> Source() override {
    return ReadTarget(source_);
  }
  std::optional<CalculationActor> Actor(std::uint8_t slot) override {
    const auto* stats = StatsAt(slot);
    if (stats == nullptr) return std::nullopt;
    return CalculationActor{stats->actorId, stats->statuses};
  }
  std::optional<std::uint8_t> ActionType() override {
    return action_ ? std::optional{static_cast<std::uint8_t>(action_->type)}
                   : std::nullopt;
  }
  std::optional<int> HitCalculator(int hit, int execute) override {
    if (!HasHit(hit)) return std::nullopt;
    const auto selector = action_->hits[hit].hitFunctionId;
    if (selector == 0 || selector >= std::size(Battle_hitFunctions) ||
        Battle_hitFunctions[selector] == nullptr) {
      return std::nullopt;
    }
    return Battle_hitFunctions[selector](action_, source_, target_, hit,
                                         execute);
  }
  std::optional<std::int16_t> StatCalculator(int hit, int execute) override {
    if (!HasHit(hit)) return std::nullopt;
    const auto selector = action_->hits[hit].statCalculatorId;
    if (selector == 0 || selector >= std::size(Battle_statCalculators) ||
        Battle_statCalculators[selector] == nullptr) {
      return std::nullopt;
    }
    return Battle_statCalculators[selector](action_, source_, target_, hit,
                                            execute);
  }
  std::uint32_t Prerequisite() override { return Battle_hitPrerequisiteResult; }
  void StoreThreshold(std::uint8_t threshold) override {
    target_->target.hitThreshold = threshold;
  }
  void StoreLastValue(std::uint32_t value) override {
    Battle_lastActionStatValue = value;
  }
  std::int32_t RandomBounded(int bound) override {
    return Core::NextLiveRandomBounded(bound);
  }

 private:
  bool HasHit(int hit) const {
    return action_ != nullptr && hit >= 0 &&
           static_cast<std::size_t>(hit) < std::size(action_->hits);
  }
  Battle_ActionDefinition* action_;
  Battle_HitResult* source_;
  Battle_HitResult* target_;
};
}  // namespace

// decomp: _getAgilityDifference (src/BATTLE/BATTLE.PRG/146C.c:9890).
int LiveAgilityHitChance(Battle_ActionDefinition* action,
                         Battle_HitResult* source, Battle_HitResult* target,
                         int hit, int variance) {
  return CalculateAttributes(AgilityHitChance, action, source, target, hit,
                             variance);
}
// decomp: _getIntelligenceDifference (src/BATTLE/BATTLE.PRG/146C.c:9941).
int LiveIntelligenceHitChance(Battle_ActionDefinition* action,
                              Battle_HitResult* source,
                              Battle_HitResult* target, int hit, int variance) {
  return CalculateAttributes(IntelligenceHitChance, action, source, target, hit,
                             variance);
}
// decomp: _getAgilityDifference2 (src/BATTLE/BATTLE.PRG/146C.c:9992).
int LiveCostAdjustedAgilityHitChance(Battle_ActionDefinition* action,
                                     Battle_HitResult* source,
                                     Battle_HitResult* target, int hit,
                                     int variance) {
  return CalculateAttributes(CostAdjustedAgilityHitChance, action, source,
                             target, hit, variance);
}
// decomp: _doesAttackHit (src/BATTLE/BATTLE.PRG/146C.c:10075).
int LiveRollActionHit(Battle_ActionDefinition* action, Battle_HitResult* source,
                      Battle_HitResult* target, int hit, int execute) {
  LiveCalculationWorld world(action, source, target);
  return Checked(RollActionHit(hit, execute, world));
}
// decomp: _calculateStatChange (src/BATTLE/BATTLE.PRG/146C.c:12248).
std::int16_t LiveCalculateActionStatChange(Battle_ActionDefinition* action,
                                           Battle_HitResult* source,
                                           Battle_HitResult* target, int hit,
                                           int execute) {
  LiveCalculationWorld world(action, source, target);
  return Checked(CalculateActionStatChange(hit, execute, world));
}
// Retired _invalidHitFunction (146C.c:9860); see #350 routing evidence.
int RejectLiveHitSelector(Battle_ActionDefinition*, Battle_HitResult*,
                          Battle_HitResult*, int, int) {
  Refuse();
}
// Retired _statCalculatorNop (146C.c:11230); see #350 routing evidence.
std::int16_t RejectLiveStatSelector(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult*, int, int) {
  Refuse();
}
// Retired func_80083990 (146C.c:12265); effect table has a void contract.
void RejectLiveEffectSelector(Battle_ActionDefinition*, Battle_HitResult*,
                              Battle_HitResult*, int, int) {
  Refuse();
}
static_assert(std::is_same_v<decltype(Battle_hitFunctions[0]),
                             decltype(&LiveAgilityHitChance)&>);
static_assert(std::is_same_v<decltype(Battle_hitFunctions[0]),
                             decltype(&LiveIntelligenceHitChance)&>);
static_assert(std::is_same_v<decltype(Battle_hitFunctions[0]),
                             decltype(&LiveCostAdjustedAgilityHitChance)&>);
static_assert(std::is_same_v<decltype(Battle_hitFunctions[0]),
                             decltype(&RejectLiveHitSelector)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&RejectLiveStatSelector)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveCalculateActionStatChange)&>);
static_assert(std::is_same_v<decltype(Battle_actionEffectMediators[0]),
                             decltype(&RejectLiveEffectSelector)&>);
}  // namespace VS::Battle
