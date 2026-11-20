// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatFactorsLive.hpp"

#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hit.hpp"
#include "Battle/StatFactors.hpp"
#include "Core/Diagnostics.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Battle {
namespace {
std::int16_t Checked(std::optional<std::int16_t> result) {
  if (!result) {
    VS::Core::Errorf("battle",
                     "Stat factor refused missing required state, actor, "
                     "stats, action or used index");
    std::abort();
  }
  return result.value();
}
const VS::Battle::ActorStats* StatsOf(const VS::Battle::HitResult* hit) {
  if (hit == nullptr) return nullptr;
  const auto slot = hit->target.target_actor;
  if (slot >= std::size(VS::Battle::actors) ||
      VS::Battle::actors[slot] == nullptr) {
    return nullptr;
  }
  return VS::Battle::actors[slot]->stats;
}
StatFactorHit Amounts(const VS::Battle::HitResult& hit) {
  return {hit.target.hp, hit.target.mp};
}
class LiveStatFactorWorld final : public StatFactorWorld {
 public:
  LiveStatFactorWorld(ActionDefinition* action,
                      const VS::Battle::HitResult* source,
                      const VS::Battle::HitResult* target)
      : action_(action), source_(source), target_(target) {}
  std::optional<std::uint16_t> QueueIndex() override {
    if (VS::Battle::actionState == nullptr) return std::nullopt;
    return static_cast<std::uint16_t>(VS::Battle::actionState->queue_index);
  }
  std::optional<std::uint8_t> Factor(int hit) override {
    if (action_ == nullptr || hit < 0 ||
        static_cast<std::size_t>(hit) >= std::size(action_->hits)) {
      return std::nullopt;
    }
    return static_cast<std::uint8_t>(action_->hits[hit].stat_factor);
  }
  std::optional<StatFactorHit> PreviousTarget() override {
    if (VS::Battle::actionState == nullptr) return std::nullopt;
    // This fixed slot remains readable even when targetCount is zero.
    return Amounts(VS::Battle::actionState->selection.targets[0]);
  }
  std::optional<StatFactorVital> SourceVital(
      StatFactorResource resource) override {
    const auto* stats = StatsOf(source_);
    if (stats == nullptr) return std::nullopt;
    if (resource == StatFactorResource::kHp) {
      return StatFactorVital{stats->current_hp, stats->max_hp};
    }
    return StatFactorVital{stats->current_mp, stats->max_mp};
  }
  std::optional<std::int16_t> TargetHp() override {
    const auto* stats = StatsOf(target_);
    return stats ? std::optional{stats->current_hp} : std::nullopt;
  }
  std::optional<StatFactorVital> TargetVital(
      StatFactorResource resource) override {
    const auto* stats = StatsOf(target_);
    if (stats == nullptr) return std::nullopt;
    if (resource == StatFactorResource::kHp) {
      return StatFactorVital{stats->current_hp, stats->max_hp};
    }
    return StatFactorVital{stats->current_mp, stats->max_mp};
  }
  std::optional<std::uint16_t> WeaponPp() override {
    const auto* stats = StatsOf(source_);
    return stats ? std::optional{stats->weapon.current_pp} : std::nullopt;
  }
  std::optional<std::uint16_t> ShieldPp() override {
    const auto* stats = StatsOf(source_);
    return stats ? std::optional{stats->shield.current_pp} : std::nullopt;
  }
  std::optional<std::uint8_t> SelectedActionCost() override {
    if (VS::Battle::actionState == nullptr) return std::nullopt;
    const auto id = VS::Battle::actionState->selection.action_id;
    if (id >= std::size(VS::Battle::actionDefinitions)) return std::nullopt;
    return VS::Battle::actionDefinitions[id].cost;
  }
  std::optional<std::uint8_t> SelectedActionType() override {
    if (VS::Battle::actionState == nullptr) return std::nullopt;
    const auto id = VS::Battle::actionState->selection.action_id;
    if (id >= std::size(VS::Battle::actionDefinitions)) return std::nullopt;
    return static_cast<std::uint8_t>(VS::Battle::actionDefinitions[id].type);
  }
  std::optional<std::uint8_t> PrimaryHpEffect() override {
    if (VS::Battle::actionState == nullptr) return std::nullopt;
    return static_cast<std::uint8_t>(
        VS::Battle::actionState->selection.primary_target.hp_effect);
  }
  std::optional<StatFactorHit> PrimaryHit() override {
    if (VS::Battle::actionState == nullptr) return std::nullopt;
    return Amounts(VS::Battle::actionState->selection.primary_target);
  }

 private:
  ActionDefinition* action_;
  const VS::Battle::HitResult* source_;
  const VS::Battle::HitResult* target_;
};
}  // namespace

// decomp: _previousHitFactor0 (src/BATTLE/BATTLE.PRG/146C.c:11640),
// _previousHitFactor1 (:11659), _previousHitFactor2 (:12146).
std::int16_t LiveScalePreviousTargetHit(ActionDefinition* action,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int hit, int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScalePreviousTargetHit(hit, world));
}
// decomp: _hpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11650).
std::int16_t LiveScaleSourceMissingHp(ActionDefinition* action,
                                      VS::Battle::HitResult* source,
                                      VS::Battle::HitResult*, int hit, int) {
  LiveStatFactorWorld world(action, source, nullptr);
  return Checked(ScaleSourceMissingHp(hit, world));
}
// decomp: _factorCurrentHp (src/BATTLE/BATTLE.PRG/146C.c:11669).
std::int16_t LiveScaleTargetCurrentHp(ActionDefinition* action,
                                      VS::Battle::HitResult*,
                                      VS::Battle::HitResult* target, int hit,
                                      int) {
  LiveStatFactorWorld world(action, nullptr, target);
  return Checked(ScaleTargetCurrentHp(hit, world));
}
// decomp: _getWeaponPp (src/BATTLE/BATTLE.PRG/146C.c:11678).
std::int16_t LiveWeaponPpWithChainOffset(ActionDefinition*,
                                         VS::Battle::HitResult* source,
                                         VS::Battle::HitResult*, int, int) {
  LiveStatFactorWorld world(nullptr, source, nullptr);
  return Checked(WeaponPpWithChainOffset(world));
}
// decomp: _factorMagicDamageReceived (src/BATTLE/BATTLE.PRG/146C.c:11686),
// _factorMagicDamageReceivedOfType (:12156); F00417/F00427 share this binding.
std::int16_t LiveScaleIncomingSpellDamage(ActionDefinition* action,
                                          VS::Battle::HitResult*,
                                          VS::Battle::HitResult*, int hit,
                                          int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScaleIncomingSpellDamage(hit, world));
}
// decomp: _mpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11720).
std::int16_t LiveScaleSourceMissingMp(ActionDefinition* action,
                                      VS::Battle::HitResult* source,
                                      VS::Battle::HitResult*, int hit, int) {
  LiveStatFactorWorld world(action, source, nullptr);
  return Checked(ScaleSourceMissingMp(hit, world));
}
// decomp: _factorPhysicalDamageReceived0 (src/BATTLE/BATTLE.PRG/146C.c:11703),
// _factorPhysicalDamageReceived1 (:12174); F00418/F00428 share this binding.
std::int16_t LiveScaleIncomingPhysicalDamage(ActionDefinition* action,
                                             VS::Battle::HitResult*,
                                             VS::Battle::HitResult*, int hit,
                                             int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScaleIncomingPhysicalDamage(hit, world));
}
// decomp: _getRecoverableHp (src/BATTLE/BATTLE.PRG/146C.c:12130).
std::int16_t LiveTargetMissingHp(ActionDefinition*, VS::Battle::HitResult*,
                                 VS::Battle::HitResult* target, int, int) {
  LiveStatFactorWorld world(nullptr, nullptr, target);
  return Checked(TargetMissingHp(world));
}
// decomp: _getRecoverableMp (src/BATTLE/BATTLE.PRG/146C.c:12138).
std::int16_t LiveTargetMissingMp(ActionDefinition*, VS::Battle::HitResult*,
                                 VS::Battle::HitResult* target, int, int) {
  LiveStatFactorWorld world(nullptr, nullptr, target);
  return Checked(TargetMissingMp(world));
}
// decomp: _factorSpellCost (src/BATTLE/BATTLE.PRG/146C.c:12191).
std::int16_t LiveScaleIncomingSpellCost(ActionDefinition* action,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int hit, int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScaleIncomingSpellCost(hit, world));
}
// decomp: _factorDamageReceived (src/BATTLE/BATTLE.PRG/146C.c:12208).
std::int16_t LiveScaleIncomingDamage(ActionDefinition* action,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int hit, int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScaleIncomingDamage(hit, world));
}
// decomp: _getEquippedShieldDp (src/BATTLE/BATTLE.PRG/146C.c:12221).
std::int16_t LiveSourceShieldPp(ActionDefinition*,
                                VS::Battle::HitResult* source,
                                VS::Battle::HitResult*, int, int) {
  LiveStatFactorWorld world(nullptr, source, nullptr);
  return Checked(SourceShieldPp(world));
}
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveScalePreviousTargetHit)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveScaleSourceMissingHp)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveScaleTargetCurrentHp)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveWeaponPpWithChainOffset)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveScaleIncomingSpellDamage)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveScaleSourceMissingMp)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveScaleIncomingPhysicalDamage)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveTargetMissingHp)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveTargetMissingMp)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveScaleIncomingSpellCost)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveScaleIncomingDamage)&>);
static_assert(std::is_same_v<decltype(VS::Battle::statCalculators[0]),
                             decltype(&LiveSourceShieldPp)&>);
}  // namespace VS::Battle
