// Hand-maintained (tools/port/owned.csv).
#include "Battle/HitCalculationsLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Battle/Action.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hit.hpp"
#include "Battle/HitCalculations.hpp"
#include "Battle/HitChance.hpp"
#include "Battle/HitModifiersLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/RandomLive.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Battle {
namespace {
[[noreturn]] void Refuse() {
  VS::Core::Errorf("battle",
                   "Action calculation refused missing state, used index or "
                   "retired hit/stat/effect selector");
  std::abort();
}
template <typename T>
T Checked(std::optional<T> result) {
  if (!result) Refuse();
  return result.value();
}
VS::Battle::ActorStats* StatsAt(std::uint8_t slot) {
  if (slot >= std::size(VS::Battle::actors) ||
      VS::Battle::actors[slot] == nullptr) {
    return nullptr;
  }
  return VS::Battle::actors[slot]->stats;
}

class LiveAttributeAccess final : public HitAttributeAccess {
 public:
  LiveAttributeAccess(VS::Battle::ActorStats& owner, ActionDefinition* action)
      : owner_(owner), action_(action) {}
  HitAttributeStats Read() override {
    HitAttributeStats result;
    result.actor_id = owner_.actor_id;
    result.risk = owner_.risk;
    result.hp = owner_.current_hp;
    result.max_hp = owner_.max_hp;
    result.agility = {owner_.agility,
                      owner_.accessory.current_agility,
                      owner_.weapon.attributes.current_agility,
                      owner_.shield.attributes.current_agility,
                      {}};
    result.intelligence = {owner_.intelligence,
                           owner_.accessory.current_intelligence,
                           owner_.weapon.attributes.current_intelligence,
                           owner_.shield.attributes.current_intelligence,
                           {}};
    for (std::size_t i = 0; i < result.defense.size(); ++i) {
      result.agility.armor.at(i) =
          owner_.limbs[i].armor.attributes.current_agility;
      result.intelligence.armor.at(i) =
          owner_.limbs[i].armor.attributes.current_intelligence;
      result.defense.at(i) = owner_.limbs[i].agility_defense_bonus;
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
  VS::Battle::ActorStats& owner_;
  ActionDefinition* action_;
};
class LiveAttributeWorld final : public HitAttributeWorld {
 public:
  explicit LiveAttributeWorld(ActionDefinition* action) : action_(action) {}
  HitAttributeAccess* Stats(std::uint8_t slot) override {
    auto* owner = StatsAt(slot);
    if (owner == nullptr || count_ >= stats_.size()) return nullptr;
    return &stats_.at(count_++).emplace(*owner, action_);
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    if (slot >= std::size(VS::Battle::actors) ||
        VS::Battle::actors[slot] == nullptr) {
      return std::nullopt;
    }
    return std::bit_cast<std::uint32_t>(VS::Battle::actors[slot]->weapon_drawn);
  }
  std::optional<std::uint8_t> Cost() override {
    return action_ ? std::optional{action_->cost} : std::nullopt;
  }
  std::int32_t RandomAveraged(int bound) override {
    return Core::NextLiveRandomAveraged(bound);
  }

 private:
  ActionDefinition* action_;
  std::array<std::optional<LiveAttributeAccess>, 2> stats_{};
  std::size_t count_{};
};
std::optional<HitModifierTarget> AttributeHit(
    const VS::Battle::HitResult* hit) {
  if (hit == nullptr) return std::nullopt;
  return HitModifierTarget{hit->target.target_actor,
                           std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
                               hit->target.target_limb)),
                           hit->targets_position};
}
using AttributeCalculator = decltype(&AgilityHitChance);
int CalculateAttributes(AttributeCalculator calculate, ActionDefinition* action,
                        VS::Battle::HitResult* source,
                        VS::Battle::HitResult* target, int hit, int variance) {
  LiveAttributeWorld world(action);
  const auto from = AttributeHit(source);
  const auto to = AttributeHit(target);
  return Checked(calculate(from ? &from.value() : nullptr,
                           to ? &to.value() : nullptr, hit, variance, world));
}

std::optional<CalculationTarget> ReadTarget(const VS::Battle::HitResult* hit) {
  if (hit == nullptr) return std::nullopt;
  return CalculationTarget{hit->target.target_actor, hit->targets_position,
                           hit->target.hit_threshold};
}
class LiveCalculationWorld final : public ActionCalculationWorld {
 public:
  LiveCalculationWorld(ActionDefinition* action, VS::Battle::HitResult* source,
                       VS::Battle::HitResult* target)
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
    return CalculationActor{stats->actor_id, stats->statuses};
  }
  std::optional<std::uint8_t> ActionType() override {
    return action_ ? std::optional{static_cast<std::uint8_t>(action_->type)}
                   : std::nullopt;
  }
  std::optional<int> HitCalculator(int hit, int execute) override {
    if (!HasHit(hit)) return std::nullopt;
    const auto selector = action_->hits[hit].hit_function_id;
    if (selector == 0 || selector >= std::size(hitFunctions) ||
        hitFunctions[selector] == nullptr) {
      return std::nullopt;
    }
    return hitFunctions[selector](action_, source_, target_, hit, execute);
  }
  std::optional<std::int16_t> StatCalculator(int hit, int execute) override {
    if (!HasHit(hit)) return std::nullopt;
    const auto selector = action_->hits[hit].stat_calculator_id;
    if (selector == 0 || selector >= std::size(VS::Battle::statCalculators) ||
        VS::Battle::statCalculators[selector] == nullptr) {
      return std::nullopt;
    }
    return VS::Battle::statCalculators[selector](action_, source_, target_, hit,
                                                 execute);
  }
  std::uint32_t Prerequisite() override {
    return VS::Battle::hitPrerequisiteResult;
  }
  void StoreThreshold(std::uint8_t threshold) override {
    target_->target.hit_threshold = threshold;
  }
  void StoreLastValue(std::uint32_t value) override {
    VS::Battle::lastActionStatValue = value;
  }
  std::int32_t RandomBounded(int bound) override {
    return Core::NextLiveRandomBounded(bound);
  }

 private:
  bool HasHit(int hit) const {
    return action_ != nullptr && hit >= 0 &&
           static_cast<std::size_t>(hit) < std::size(action_->hits);
  }
  ActionDefinition* action_;
  VS::Battle::HitResult* source_;
  VS::Battle::HitResult* target_;
};
}  // namespace

// decomp: _getAgilityDifference (src/BATTLE/BATTLE.PRG/146C.c:9890).
int LiveAgilityHitChance(ActionDefinition* action,
                         VS::Battle::HitResult* source,
                         VS::Battle::HitResult* target, int hit, int variance) {
  return CalculateAttributes(AgilityHitChance, action, source, target, hit,
                             variance);
}
// decomp: _getIntelligenceDifference (src/BATTLE/BATTLE.PRG/146C.c:9941).
int LiveIntelligenceHitChance(ActionDefinition* action,
                              VS::Battle::HitResult* source,
                              VS::Battle::HitResult* target, int hit,
                              int variance) {
  return CalculateAttributes(IntelligenceHitChance, action, source, target, hit,
                             variance);
}
// decomp: _getAgilityDifference2 (src/BATTLE/BATTLE.PRG/146C.c:9992).
int LiveCostAdjustedAgilityHitChance(ActionDefinition* action,
                                     VS::Battle::HitResult* source,
                                     VS::Battle::HitResult* target, int hit,
                                     int variance) {
  return CalculateAttributes(CostAdjustedAgilityHitChance, action, source,
                             target, hit, variance);
}
// decomp: _doesAttackHit (src/BATTLE/BATTLE.PRG/146C.c:10075).
int LiveRollActionHit(ActionDefinition* action, VS::Battle::HitResult* source,
                      VS::Battle::HitResult* target, int hit, int execute) {
  LiveCalculationWorld world(action, source, target);
  return Checked(RollActionHit(hit, execute, world));
}
// decomp: _calculateStatChange (src/BATTLE/BATTLE.PRG/146C.c:12248).
std::int16_t LiveCalculateActionStatChange(ActionDefinition* action,
                                           VS::Battle::HitResult* source,
                                           VS::Battle::HitResult* target,
                                           int hit, int execute) {
  LiveCalculationWorld world(action, source, target);
  return Checked(CalculateActionStatChange(hit, execute, world));
}
// Retired _invalidHitFunction (146C.c:9860); see #350 routing evidence.
int RejectLiveHitSelector(ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult*, int, int) {
  Refuse();
}
// Retired _statCalculatorNop (146C.c:11230); see #350 routing evidence.
std::int16_t RejectLiveStatSelector(ActionDefinition*, VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int) {
  Refuse();
}
// Retired func_80083990 (146C.c:12265); effect table has a void contract.
void RejectLiveEffectSelector(ActionDefinition*, VS::Battle::HitResult*,
                              VS::Battle::HitResult*, int, int) {
  Refuse();
}
static_assert(std::is_same_v<decltype(hitFunctions[0]),
                             decltype(&LiveAgilityHitChance)&>);
static_assert(std::is_same_v<decltype(hitFunctions[0]),
                             decltype(&LiveIntelligenceHitChance)&>);
static_assert(std::is_same_v<decltype(hitFunctions[0]),
                             decltype(&LiveCostAdjustedAgilityHitChance)&>);
static_assert(std::is_same_v<decltype(hitFunctions[0]),
                             decltype(&RejectLiveHitSelector)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&RejectLiveStatSelector)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveCalculateActionStatChange)&>);
static_assert(std::is_same_v<decltype(VS::Battle::actionEffectMediators[0]),
                             decltype(&RejectLiveEffectSelector)&>);
}  // namespace VS::Battle
