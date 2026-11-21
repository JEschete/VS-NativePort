// Hand-maintained (tools/port/owned.csv).
#include "Battle/DamageRecoveryEffectsLive.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/DamagePoliciesLive.hpp"
#include "Battle/DamageRecoveryEffects.hpp"
#include "Battle/Hit.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Items/EquipmentLive.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    VS::Core::Errorf(
        "battle",
        "Damage/recovery effect requires its used hit, actor, stats or "
        "selection");
    std::abort();
  }
}
VS::Battle::HitResult& HitOf(VS::Battle::HitResult* hit) {
  Require(hit != nullptr);
  return *hit;
}
VS::Battle::ActorStats* CaptureStats(VS::Battle::HitResult* hit) {
  const auto slot = HitOf(hit).target.target_actor;
  Require(slot < std::size(VS::Battle::actors) &&
          VS::Battle::actors[slot] != nullptr);
  return VS::Battle::actors[slot]->stats;
}
VS::Battle::ActorStats& StatsOf(VS::Battle::ActorStats* stats) {
  Require(stats != nullptr);
  return *stats;
}

class LiveQueue final : public DamageRecoveryQueue {
 public:
  LiveQueue(ActionDefinition* action, VS::Battle::HitResult* source,
            VS::Battle::HitResult* target, int hit, int execute)
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
  void ClearCriticalHit() override { VS::Battle::criticalHitOccurred = 0; }
  bool CriticalHit() override { return VS::Battle::criticalHitOccurred != 0; }
  std::int16_t Calculate() override {
    return LiveCalculateActionStatChange(action_, source_, target_, hit_,
                                         execute_);
  }
  ActionResultChange ReadChange(ActionResultField field) override {
    const auto& hit = HitOf(target_);
    switch (field) {
      case ActionResultField::kHp:
        return {hit.target.hp, static_cast<std::uint8_t>(hit.hp_effect)};
      case ActionResultField::kMp:
        return {hit.target.mp, static_cast<std::uint8_t>(hit.mp_effect)};
      case ActionResultField::kWeaponDp:
        return {hit.weapon_dp_change,
                static_cast<std::uint8_t>(hit.weapon_dp_effect)};
      case ActionResultField::kRisk:
        return {hit.risk_change, static_cast<std::uint8_t>(hit.risk_effect)};
      case ActionResultField::kWeaponPp:
        return {hit.weapon_pp_change,
                static_cast<std::uint8_t>(hit.weapon_pp_effect)};
      case ActionResultField::kShieldPp:
        return {hit.shield_pp_change,
                static_cast<std::uint8_t>(hit.shield_pp_effect)};
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
        hit.hp_effect = change.mode;
        hit.target.hp = change.amount;
        break;
      case ActionResultField::kMp:
        hit.mp_effect = change.mode;
        hit.target.mp = change.amount;
        break;
      case ActionResultField::kWeaponDp:
        hit.weapon_dp_effect = change.mode;
        hit.weapon_dp_change = change.amount;
        break;
      case ActionResultField::kRisk:
        hit.risk_effect = change.mode;
        hit.risk_change = change.amount;
        break;
      case ActionResultField::kWeaponPp:
        hit.weapon_pp_effect = change.mode;
        hit.weapon_pp_change = change.amount;
        break;
      case ActionResultField::kShieldPp:
        hit.shield_pp_effect = change.mode;
        hit.shield_pp_change = change.amount;
        break;
      default:
        Require(false);
    }
  }
  bool TargetsPosition() override {
    return HitOf(target_).targets_position != 0;
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
    return stats.actor_id == kExemptActor ? 0
                                          : stats.statuses & kRemovedOnDamage;
  }
  void QueueRemovalStatuses(std::uint32_t statuses) override {
    auto& hit = HitOf(target_);
    hit.inflict_statuses = std::bit_cast<std::int32_t>(
        std::bit_cast<std::uint32_t>(hit.inflict_statuses) | statuses);
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
  std::int16_t TargetMp() override { return StatsOf(target_stats_).current_mp; }
  void StoreLastValue(std::int16_t amount) override {
    VS::Battle::lastActionStatValue =
        static_cast<std::uint32_t>(static_cast<std::int32_t>(amount));
  }
  bool HasWeapon() override {
    return StatsOf(target_stats_).weapon.blade.id != 0;
  }
  bool HasShield() override {
    return StatsOf(target_stats_).shield.base.id != 0;
  }
  std::uint16_t WeaponPp() override {
    return StatsOf(target_stats_).weapon.current_pp;
  }
  std::uint16_t ShieldPp() override {
    return StatsOf(target_stats_).shield.current_pp;
  }
  std::uint8_t SelectedPrimaryHpMode() override {
    Require(VS::Battle::actionState != nullptr);
    return static_cast<std::uint8_t>(
        VS::Battle::actionState->selection.primary_target.hp_effect);
  }

 private:
  ActionDefinition* action_;
  VS::Battle::HitResult* source_;
  VS::Battle::HitResult* target_;
  int hit_;
  int execute_;
  VS::Battle::ActorStats* source_stats_{};
  VS::Battle::ActorStats* target_stats_{};
};

using Mediator = VS::Battle::ActionEffectMediator;
static_assert(
    std::is_same_v<decltype(&QueueLiveHitDamageAndEquipmentWear), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveMpDrain), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveWeaponPpDrain), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveShieldPpDrain), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveHpRecovery), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveMpRecovery), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveWeaponPpRecovery), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveWeaponDpRecovery), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveRiskRecovery), Mediator>);
}  // namespace

// decomp: func_80084EEC (src/BATTLE/BATTLE.PRG/146C.c:12728).
void QueueLiveWeaponPpRecovery(ActionDefinition* action,
                               VS::Battle::HitResult* source,
                               VS::Battle::HitResult* target, int hit,
                               int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueWeaponPpRecovery(queue);
}
// decomp: func_80084F40 (src/BATTLE/BATTLE.PRG/146C.c:12735).
void QueueLiveWeaponDpRecovery(ActionDefinition* action,
                               VS::Battle::HitResult* source,
                               VS::Battle::HitResult* target, int hit,
                               int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueWeaponDpRecovery(queue);
}
// decomp: func_80084F94 (src/BATTLE/BATTLE.PRG/146C.c:12742).
void QueueLiveRiskRecovery(ActionDefinition* action,
                           VS::Battle::HitResult* source,
                           VS::Battle::HitResult* target, int hit,
                           int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueRiskRecovery(queue);
}

// decomp: func_80084B70 (src/BATTLE/BATTLE.PRG/146C.c:12641).
void QueueLiveHitDamageAndEquipmentWear(ActionDefinition* action,
                                        VS::Battle::HitResult* source,
                                        VS::Battle::HitResult* target, int hit,
                                        int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueHitDamageAndEquipmentWear(queue, execute != 0);
}
// decomp: func_80084CAC (src/BATTLE/BATTLE.PRG/146C.c:12671).
void QueueLiveMpDrain(ActionDefinition* action, VS::Battle::HitResult* source,
                      VS::Battle::HitResult* target, int hit, int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueMpDrain(queue);
}
// decomp: func_80084D44 (src/BATTLE/BATTLE.PRG/146C.c:12685).
void QueueLiveWeaponPpDrain(ActionDefinition* action,
                            VS::Battle::HitResult* source,
                            VS::Battle::HitResult* target, int hit,
                            int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueWeaponPpDrain(queue);
}
// decomp: func_80084DA8 (src/BATTLE/BATTLE.PRG/146C.c:12696).
void QueueLiveShieldPpDrain(ActionDefinition* action,
                            VS::Battle::HitResult* source,
                            VS::Battle::HitResult* target, int hit,
                            int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueShieldPpDrain(queue);
}
// decomp: func_80084E28 (src/BATTLE/BATTLE.PRG/146C.c:12710).
void QueueLiveHpRecovery(ActionDefinition* action,
                         VS::Battle::HitResult* source,
                         VS::Battle::HitResult* target, int hit, int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueHpRecovery(queue);
}
// decomp: func_80084EA0 (src/BATTLE/BATTLE.PRG/146C.c:12721).
void QueueLiveMpRecovery(ActionDefinition* action,
                         VS::Battle::HitResult* source,
                         VS::Battle::HitResult* target, int hit, int execute) {
  LiveQueue queue(action, source, target, hit, execute);
  QueueMpRecovery(queue);
}
}  // namespace VS::Battle
