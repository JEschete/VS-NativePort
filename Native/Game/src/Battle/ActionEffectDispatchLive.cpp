// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionEffectDispatchLive.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/AbilityState.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionEffectDispatch.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hit.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Items/ItemRecords.hpp"
#include "State/Progress.hpp"
#include "State/ScriptFlagsLive.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    VS::Core::Errorf("battle",
                     "Action effect requires its used hit, actor, stats, "
                     "limb, rule, definition, mediator or character");
    std::abort();
  }
}
VS::Battle::HitResult& HitOf(VS::Battle::HitResult* hit) {
  Require(hit != nullptr);
  return *hit;
}
VS::Battle::Actor& ActorOf(const VS::Battle::HitResult& hit) {
  const auto slot = hit.target.target_actor;
  Require(slot < std::size(VS::Battle::actors) &&
          VS::Battle::actors[slot] != nullptr);
  return *VS::Battle::actors[slot];
}
VS::Battle::ActorStats& StatsOf(const VS::Battle::HitResult& hit) {
  auto* stats = ActorOf(hit).stats;
  Require(stats != nullptr);
  return *stats;
}

class LiveInjuryQueue final : public LimbInjuryQueue {
 public:
  explicit LiveInjuryQueue(VS::Battle::HitResult& hit)
      : hit_(hit), stats_(StatsOf(hit)) {}
  std::uint8_t MobilityLimb(std::size_t index) override {
    return index == 0 ? stats_.mobility_limb0 : stats_.mobility_limb1;
  }
  std::int16_t CurrentHp(std::uint8_t limb) override {
    Require(limb < std::size(stats_.limbs));
    return stats_.limbs[limb].hp;
  }
  ActionResultChange PendingHp(std::uint8_t limb) override {
    Require(limb < std::size(hit_.limbs));
    return {hit_.limbs[limb].effects.hp_change,
            static_cast<std::uint8_t>(hit_.hp_effect)};
  }
  std::uint32_t ActiveStatuses() override {
    return std::bit_cast<std::uint32_t>(stats_.statuses);
  }
  bool PlayerTarget() override { return hit_.target.target_actor == 0; }
  std::uint8_t InjuryRule(std::size_t limb) override {
    Require(limb < std::size(VS::Battle::limbInjuryStatusIds));
    return VS::Battle::limbInjuryStatusIds[limb];
  }
  std::uint32_t RuleMask(std::uint8_t rule) override {
    Require(rule < std::size(VS::Battle::statusEffectParams));
    return std::bit_cast<std::uint32_t>(
        VS::Battle::statusEffectParams[rule].status_flags);
  }
  void QueueStatus(std::uint8_t rule, bool remove) override {
    if (remove) {
      QueueLiveStatusRemovalMask(rule, &hit_);
    } else {
      QueueLiveApplicableStatusChanges(rule, &hit_);
    }
  }

 private:
  VS::Battle::HitResult& hit_;
  VS::Battle::ActorStats& stats_;
};

class LiveEffectWorld final : public ActionEffectWorld {
 public:
  LiveEffectWorld(ActionDefinition* action, VS::Battle::HitResult* source,
                  VS::Battle::HitResult& target)
      : action_(action), source_(source), target_(target) {}
  bool TargetPosition() override { return target_.targets_position != 0; }
  std::uint8_t TargetActorId() override { return StatsOf(target_).actor_id; }
  std::uint32_t TargetStatuses() override {
    return std::bit_cast<std::uint32_t>(StatsOf(target_).statuses);
  }
  std::uint8_t ActionType() override {
    Require(action_ != nullptr);
    return static_cast<std::uint8_t>(action_->type);
  }
  void QueueExemptDamage() override {
    constexpr int kExemptDamage = 999;
    target_.hp_effect = 1;
    target_.target.hp = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(target_.target.hp + kExemptDamage));
  }
  void ClearLastValue() override { VS::Battle::lastActionStatValue = 0; }
  void Dispatch(int hit, int execute) override {
    Require(action_ != nullptr && hit >= 0 &&
            static_cast<std::size_t>(hit) < std::size(action_->hits));
    const auto effect = action_->hits[hit].effect_id;
    Require(effect < std::size(VS::Battle::actionEffectMediators));
    const auto mediator = VS::Battle::actionEffectMediators[effect];
    Require(mediator != nullptr);
    mediator(action_, source_, &target_, hit, execute);
  }
  void QueueStatusRemoval(std::uint32_t mask) override {
    target_.inflict_statuses = std::bit_cast<std::int32_t>(
        std::bit_cast<std::uint32_t>(target_.inflict_statuses) | mask);
  }
  bool SourcePosition() override {
    return HitOf(source_).targets_position != 0;
  }
  std::uint8_t SourceSlot() override {
    return HitOf(source_).target.target_actor;
  }
  std::int16_t TargetHp() override { return StatsOf(target_).current_hp; }
  std::int16_t PendingHp() override { return target_.target.hp; }
  std::uint8_t TargetSlot() override { return target_.target.target_actor; }
  bool KillCounted() override { return ActorOf(target_).kill_counted != 0; }
  void MarkKillCounted() override { ActorOf(target_).kill_counted = 1; }
  std::uint8_t WeaponCategory() override {
    Require(VS::Battle::characterState != nullptr);
    return VS::Battle::characterState->equipped_weapon_category;
  }
  std::uint32_t ProgressFlag() override {
    constexpr int kWeaponArtProgressFlag = 0x82;
    return State::LiveScriptFlag(kWeaponArtProgressFlag);
  }
  std::uint8_t LearnedArts(std::uint8_t category) override {
    return VS::State::artsStatus.arts_learned[category];
  }
  std::uint16_t WeaponKills(std::uint8_t category) override {
    return VS::State::artsStatus.kills.weapon_category_kills[category];
  }
  std::uint16_t RequiredKills(std::uint8_t category,
                              std::uint8_t learned) override {
    return VS::Battle::breakArtPointRequirements[category][learned];
  }
  void SetWeaponKills(std::uint8_t category, std::uint16_t kills) override {
    VS::State::artsStatus.kills.weapon_category_kills[category] = kills;
  }

 private:
  ActionDefinition* action_;
  VS::Battle::HitResult* source_;
  VS::Battle::HitResult& target_;
};
using Mediator = VS::Battle::ActionEffectMediator;
static_assert(std::is_same_v<decltype(&ApplyLiveActionHitEffect), Mediator>);
}  // namespace

// decomp: func_80085008 (src/BATTLE/BATTLE.PRG/146C.c:12930).
void DeriveLiveLimbInjuryStatuses(VS::Battle::HitResult* hit) {
  LiveInjuryQueue queue(HitOf(hit));
  DeriveLimbInjuryStatuses(queue);
}
// decomp: func_80085390 (src/BATTLE/BATTLE.PRG/146C.c:13012).
void ApplyLiveActionHitEffect(ActionDefinition* action,
                              VS::Battle::HitResult* source,
                              VS::Battle::HitResult* target, int execute,
                              int hit) {
  LiveEffectWorld world(action, source, HitOf(target));
  ApplyActionHitEffect(world, execute, hit);
}
}  // namespace VS::Battle
