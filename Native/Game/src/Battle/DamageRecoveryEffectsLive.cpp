// Hand-maintained (tools/port/owned.csv).
#include "Battle/DamageRecoveryEffectsLive.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/DamagePoliciesLive.hpp"
#include "Battle/DamageRecoveryEffects.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Items/EquipmentLive.hpp"
extern "C" {
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    Core_Errorf("battle",
                "Damage/recovery effect requires its used hit, actor, stats or "
                "selection");
    std::abort();
  }
}
Battle_HitResult& HitOf(Battle_HitResult* hit) {
  Require(hit != nullptr);
  return *hit;
}
Battle_ActorStats* CaptureStats(Battle_HitResult* hit) {
  const auto slot = HitOf(hit).target.targetActor;
  Require(slot < std::size(Battle_actors) && Battle_actors[slot] != nullptr);
  return Battle_actors[slot]->stats;
}
Battle_ActorStats& StatsOf(Battle_ActorStats* stats) {
  Require(stats != nullptr);
  return *stats;
}

class LiveQueue final : public DamageRecoveryQueue {
 public:
  LiveQueue(Battle_ActionDefinition* action, Battle_HitResult* source,
            Battle_HitResult* target, int hit, int execute)
      : action_(action),
        source_(source),
        target_(target),
        hit_(hit),
        execute_(execute) {}
  void CaptureDamageOwners() override {
    source_stats_ = CaptureStats(source_);
    CaptureTargetOwner();
  }
  void CaptureTargetOwner() override { target_stats_ = CaptureStats(target_); }
  void ClearCriticalHit() override { Battle_criticalHitOccurred = 0; }
  bool CriticalHit() override { return Battle_criticalHitOccurred != 0; }
  std::int16_t Calculate() override {
    return LiveCalculateActionStatChange(action_, source_, target_, hit_,
                                         execute_);
  }
  ActionResultChange ReadChange(ActionResultField field) override {
    const auto& hit = HitOf(target_);
    switch (field) {
      case ActionResultField::kHp:
        return {hit.target.hp, static_cast<std::uint8_t>(hit.hpEffect)};
      case ActionResultField::kMp:
        return {hit.target.mp, static_cast<std::uint8_t>(hit.mpEffect)};
      case ActionResultField::kWeaponPp:
        return {hit.weaponPpChange,
                static_cast<std::uint8_t>(hit.weaponPpEffect)};
      case ActionResultField::kShieldPp:
        return {hit.shieldPpChange,
                static_cast<std::uint8_t>(hit.shieldPpEffect)};
      default:
        Require(false);
        return {};
    }
  }
  void WriteChange(ActionResultField field,
                   ActionResultChange change) override {
    auto& hit = HitOf(target_);
    switch (field) {
      case ActionResultField::kHp:
        hit.hpEffect = change.mode;
        hit.target.hp = change.amount;
        break;
      case ActionResultField::kMp:
        hit.mpEffect = change.mode;
        hit.target.mp = change.amount;
        break;
      case ActionResultField::kWeaponPp:
        hit.weaponPpEffect = change.mode;
        hit.weaponPpChange = change.amount;
        break;
      case ActionResultField::kShieldPp:
        hit.shieldPpEffect = change.mode;
        hit.shieldPpChange = change.amount;
        break;
      default:
        Require(false);
    }
  }
  bool TargetsPosition() override {
    return HitOf(target_).targetsPosition != 0;
  }
  void DistributeDamage(std::int16_t amount) override {
    LiveDistributeHitLimbDamage(action_, target_, amount);
  }
  void RecoverLimbs(std::int16_t amount) override {
    LiveDistributeLimbRecovery(action_, target_, amount);
  }
  std::uint32_t TargetRemovalStatuses() override {
    const auto& stats = StatsOf(target_stats_);
    constexpr std::uint8_t kExemptActor = 128;
    constexpr std::uint32_t kRemovedOnDamage = 0x8000;
    return stats.actorId == kExemptActor ? 0
                                         : stats.statuses & kRemovedOnDamage;
  }
  void QueueRemovalStatuses(std::uint32_t statuses) override {
    auto& hit = HitOf(target_);
    hit.inflictStatuses = std::bit_cast<std::int32_t>(
        std::bit_cast<std::uint32_t>(hit.inflictStatuses) | statuses);
  }
  void ConsumeEquipmentDp(std::int16_t amount) override {
    Require(action_ != nullptr);
    Items::ConsumeLiveHitEquipmentDp(action_, &StatsOf(source_stats_),
                                     &StatsOf(target_stats_), amount);
  }
  void AccumulateEquipmentPp(std::int16_t amount) override {
    Require(action_ != nullptr);
    Items::AccumulateLiveHitEquipmentPp(action_, &StatsOf(source_stats_),
                                        &StatsOf(target_stats_), amount);
  }
  std::int16_t TargetMp() override { return StatsOf(target_stats_).currentMp; }
  void StoreLastValue(std::int16_t amount) override {
    Battle_lastActionStatValue =
        static_cast<std::uint32_t>(static_cast<std::int32_t>(amount));
  }
  bool HasWeapon() override {
    return StatsOf(target_stats_).weapon.blade.id != 0;
  }
  bool HasShield() override {
    return StatsOf(target_stats_).shield.base.id != 0;
  }
  std::uint16_t WeaponPp() override {
    return StatsOf(target_stats_).weapon.currentPp;
  }
  std::uint16_t ShieldPp() override {
    return StatsOf(target_stats_).shield.currentPp;
  }
  std::uint8_t SelectedPrimaryHpMode() override {
    Require(Battle_actionState != nullptr);
    return static_cast<std::uint8_t>(
        Battle_actionState->selection.primaryTarget.hpEffect);
  }

 private:
  Battle_ActionDefinition* action_;
  Battle_HitResult* source_;
  Battle_HitResult* target_;
  int hit_;
  int execute_;
  Battle_ActorStats* source_stats_{};
  Battle_ActorStats* target_stats_{};
};

using Mediator = std::remove_extent_t<decltype(Battle_actionEffectMediators)>;
static_assert(
    std::is_same_v<decltype(&QueueLiveHitDamageAndEquipmentWear), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveMpDrain), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveWeaponPpDrain), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveShieldPpDrain), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveHpRecovery), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveMpRecovery), Mediator>);
}  // namespace

// decomp: func_80084B70 (src/BATTLE/BATTLE.PRG/146C.c:12789).
void QueueLiveHitDamageAndEquipmentWear(Battle_ActionDefinition* action,
                                        Battle_HitResult* source,
                                        Battle_HitResult* target, int hit,
                                        int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueHitDamageAndEquipmentWear(queue, execute != 0);
}
// decomp: func_80084CAC (src/BATTLE/BATTLE.PRG/146C.c:12820).
void QueueLiveMpDrain(Battle_ActionDefinition* action, Battle_HitResult* source,
                      Battle_HitResult* target, int hit, int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueMpDrain(queue);
}
// decomp: func_80084D44 (src/BATTLE/BATTLE.PRG/146C.c:12835).
void QueueLiveWeaponPpDrain(Battle_ActionDefinition* action,
                            Battle_HitResult* source, Battle_HitResult* target,
                            int hit, int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueWeaponPpDrain(queue);
}
// decomp: func_80084DA8 (src/BATTLE/BATTLE.PRG/146C.c:12847).
void QueueLiveShieldPpDrain(Battle_ActionDefinition* action,
                            Battle_HitResult* source, Battle_HitResult* target,
                            int hit, int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueShieldPpDrain(queue);
}
// decomp: func_80084E28 (src/BATTLE/BATTLE.PRG/146C.c:12862).
void QueueLiveHpRecovery(Battle_ActionDefinition* action,
                         Battle_HitResult* source, Battle_HitResult* target,
                         int hit, int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueHpRecovery(queue);
}
// decomp: func_80084EA0 (src/BATTLE/BATTLE.PRG/146C.c:12874).
void QueueLiveMpRecovery(Battle_ActionDefinition* action,
                         Battle_HitResult* source, Battle_HitResult* target,
                         int hit, int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueMpRecovery(queue);
}
}  // namespace VS::Battle
