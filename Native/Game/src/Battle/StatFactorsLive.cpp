// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatFactorsLive.hpp"

#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/StatFactors.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
std::int16_t Checked(std::optional<std::int16_t> result) {
  if (!result) {
    Core_Errorf("battle",
                "Stat factor refused missing required state, actor, "
                "stats, action or used index");
    std::abort();
  }
  return result.value();
}
const Battle_ActorStats* StatsOf(const Battle_HitResult* hit) {
  if (hit == nullptr) return nullptr;
  const auto slot = hit->target.targetActor;
  if (slot >= std::size(Battle_actors) || Battle_actors[slot] == nullptr) {
    return nullptr;
  }
  return Battle_actors[slot]->stats;
}
StatFactorHit Amounts(const Battle_HitResult& hit) {
  return {hit.target.hp, hit.target.mp};
}
class LiveStatFactorWorld final : public StatFactorWorld {
 public:
  LiveStatFactorWorld(Battle_ActionDefinition* action,
                      const Battle_HitResult* source,
                      const Battle_HitResult* target)
      : action_(action), source_(source), target_(target) {}
  std::optional<std::uint16_t> QueueIndex() override {
    if (Battle_actionState == nullptr) return std::nullopt;
    return static_cast<std::uint16_t>(Battle_actionState->queueIndex);
  }
  std::optional<std::uint8_t> Factor(int hit) override {
    if (action_ == nullptr || hit < 0 ||
        static_cast<std::size_t>(hit) >= std::size(action_->hits)) {
      return std::nullopt;
    }
    return static_cast<std::uint8_t>(action_->hits[hit].statFactor);
  }
  std::optional<StatFactorHit> PreviousTarget() override {
    if (Battle_actionState == nullptr) return std::nullopt;
    // This fixed slot remains readable even when targetCount is zero.
    return Amounts(Battle_actionState->selection.targets[0]);
  }
  std::optional<StatFactorVital> SourceVital(
      StatFactorResource resource) override {
    const auto* stats = StatsOf(source_);
    if (stats == nullptr) return std::nullopt;
    if (resource == StatFactorResource::kHp) {
      return StatFactorVital{stats->currentHp, stats->maxHp};
    }
    return StatFactorVital{stats->currentMp, stats->maxMp};
  }
  std::optional<std::int16_t> TargetHp() override {
    const auto* stats = StatsOf(target_);
    return stats ? std::optional{stats->currentHp} : std::nullopt;
  }
  std::optional<StatFactorVital> TargetVital(
      StatFactorResource resource) override {
    const auto* stats = StatsOf(target_);
    if (stats == nullptr) return std::nullopt;
    if (resource == StatFactorResource::kHp) {
      return StatFactorVital{stats->currentHp, stats->maxHp};
    }
    return StatFactorVital{stats->currentMp, stats->maxMp};
  }
  std::optional<std::uint16_t> WeaponPp() override {
    const auto* stats = StatsOf(source_);
    return stats ? std::optional{stats->weapon.currentPp} : std::nullopt;
  }
  std::optional<std::uint16_t> ShieldPp() override {
    const auto* stats = StatsOf(source_);
    return stats ? std::optional{stats->shield.currentPp} : std::nullopt;
  }
  std::optional<std::uint8_t> SelectedActionCost() override {
    if (Battle_actionState == nullptr) return std::nullopt;
    const auto id = Battle_actionState->selection.actionId;
    if (id >= std::size(Battle_actionDefinitions)) return std::nullopt;
    return Battle_actionDefinitions[id].cost;
  }
  std::optional<std::uint8_t> SelectedActionType() override {
    if (Battle_actionState == nullptr) return std::nullopt;
    const auto id = Battle_actionState->selection.actionId;
    if (id >= std::size(Battle_actionDefinitions)) return std::nullopt;
    return static_cast<std::uint8_t>(Battle_actionDefinitions[id].type);
  }
  std::optional<std::uint8_t> PrimaryHpEffect() override {
    if (Battle_actionState == nullptr) return std::nullopt;
    return static_cast<std::uint8_t>(
        Battle_actionState->selection.primaryTarget.hpEffect);
  }
  std::optional<StatFactorHit> PrimaryHit() override {
    if (Battle_actionState == nullptr) return std::nullopt;
    return Amounts(Battle_actionState->selection.primaryTarget);
  }

 private:
  Battle_ActionDefinition* action_;
  const Battle_HitResult* source_;
  const Battle_HitResult* target_;
};
}  // namespace

// decomp: _previousHitFactor0 (src/BATTLE/BATTLE.PRG/146C.c:11640),
// _previousHitFactor1 (:11659), _previousHitFactor2 (:12146).
std::int16_t LiveScalePreviousTargetHit(Battle_ActionDefinition* action,
                                        Battle_HitResult*, Battle_HitResult*,
                                        int hit, int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScalePreviousTargetHit(hit, world));
}
// decomp: _hpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11650).
std::int16_t LiveScaleSourceMissingHp(Battle_ActionDefinition* action,
                                      Battle_HitResult* source,
                                      Battle_HitResult*, int hit, int) {
  LiveStatFactorWorld world(action, source, nullptr);
  return Checked(ScaleSourceMissingHp(hit, world));
}
// decomp: _factorCurrentHp (src/BATTLE/BATTLE.PRG/146C.c:11669).
std::int16_t LiveScaleTargetCurrentHp(Battle_ActionDefinition* action,
                                      Battle_HitResult*,
                                      Battle_HitResult* target, int hit, int) {
  LiveStatFactorWorld world(action, nullptr, target);
  return Checked(ScaleTargetCurrentHp(hit, world));
}
// decomp: _getWeaponPp (src/BATTLE/BATTLE.PRG/146C.c:11678).
std::int16_t LiveWeaponPpWithChainOffset(Battle_ActionDefinition*,
                                         Battle_HitResult* source,
                                         Battle_HitResult*, int, int) {
  LiveStatFactorWorld world(nullptr, source, nullptr);
  return Checked(WeaponPpWithChainOffset(world));
}
// decomp: _factorMagicDamageReceived (src/BATTLE/BATTLE.PRG/146C.c:11686),
// _factorMagicDamageReceivedOfType (:12156); F00417/F00427 share this binding.
std::int16_t LiveScaleIncomingSpellDamage(Battle_ActionDefinition* action,
                                          Battle_HitResult*, Battle_HitResult*,
                                          int hit, int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScaleIncomingSpellDamage(hit, world));
}
// decomp: _mpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11720).
std::int16_t LiveScaleSourceMissingMp(Battle_ActionDefinition* action,
                                      Battle_HitResult* source,
                                      Battle_HitResult*, int hit, int) {
  LiveStatFactorWorld world(action, source, nullptr);
  return Checked(ScaleSourceMissingMp(hit, world));
}
// decomp: _factorPhysicalDamageReceived0 (src/BATTLE/BATTLE.PRG/146C.c:11703),
// _factorPhysicalDamageReceived1 (:12174); F00418/F00428 share this binding.
std::int16_t LiveScaleIncomingPhysicalDamage(Battle_ActionDefinition* action,
                                             Battle_HitResult*,
                                             Battle_HitResult*, int hit, int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScaleIncomingPhysicalDamage(hit, world));
}
// decomp: _getRecoverableHp (src/BATTLE/BATTLE.PRG/146C.c:12130).
std::int16_t LiveTargetMissingHp(Battle_ActionDefinition*, Battle_HitResult*,
                                 Battle_HitResult* target, int, int) {
  LiveStatFactorWorld world(nullptr, nullptr, target);
  return Checked(TargetMissingHp(world));
}
// decomp: _getRecoverableMp (src/BATTLE/BATTLE.PRG/146C.c:12138).
std::int16_t LiveTargetMissingMp(Battle_ActionDefinition*, Battle_HitResult*,
                                 Battle_HitResult* target, int, int) {
  LiveStatFactorWorld world(nullptr, nullptr, target);
  return Checked(TargetMissingMp(world));
}
// decomp: _factorSpellCost (src/BATTLE/BATTLE.PRG/146C.c:12191).
std::int16_t LiveScaleIncomingSpellCost(Battle_ActionDefinition* action,
                                        Battle_HitResult*, Battle_HitResult*,
                                        int hit, int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScaleIncomingSpellCost(hit, world));
}
// decomp: _factorDamageReceived (src/BATTLE/BATTLE.PRG/146C.c:12208).
std::int16_t LiveScaleIncomingDamage(Battle_ActionDefinition* action,
                                     Battle_HitResult*, Battle_HitResult*,
                                     int hit, int) {
  LiveStatFactorWorld world(action, nullptr, nullptr);
  return Checked(ScaleIncomingDamage(hit, world));
}
// decomp: _getEquippedShieldDp (src/BATTLE/BATTLE.PRG/146C.c:12221).
std::int16_t LiveSourceShieldPp(Battle_ActionDefinition*,
                                Battle_HitResult* source, Battle_HitResult*,
                                int, int) {
  LiveStatFactorWorld world(nullptr, source, nullptr);
  return Checked(SourceShieldPp(world));
}
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveScalePreviousTargetHit)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveScaleSourceMissingHp)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveScaleTargetCurrentHp)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveWeaponPpWithChainOffset)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveScaleIncomingSpellDamage)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveScaleSourceMissingMp)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveScaleIncomingPhysicalDamage)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveTargetMissingHp)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveTargetMissingMp)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveScaleIncomingSpellCost)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveScaleIncomingDamage)&>);
static_assert(std::is_same_v<decltype(Battle_statCalculators[0]),
                             decltype(&LiveSourceShieldPp)&>);
}  // namespace VS::Battle
