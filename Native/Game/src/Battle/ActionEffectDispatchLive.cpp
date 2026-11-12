// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionEffectDispatchLive.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/ActionEffectDispatch.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Core/Diagnostics.h"
#include "Items/Loadout.h"
#include "State/Progress.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    Core_Errorf("battle",
                "Action effect requires its used hit, actor, stats, "
                "limb, rule, definition, mediator or character");
    std::abort();
  }
}
Battle_HitResult& HitOf(Battle_HitResult* hit) {
  Require(hit != nullptr);
  return *hit;
}
Battle_Actor& ActorOf(const Battle_HitResult& hit) {
  const auto slot = hit.target.targetActor;
  Require(slot < std::size(Battle_actors) && Battle_actors[slot] != nullptr);
  return *Battle_actors[slot];
}
Battle_ActorStats& StatsOf(const Battle_HitResult& hit) {
  auto* stats = ActorOf(hit).stats;
  Require(stats != nullptr);
  return *stats;
}

class LiveInjuryQueue final : public LimbInjuryQueue {
 public:
  explicit LiveInjuryQueue(Battle_HitResult& hit)
      : hit_(hit), stats_(StatsOf(hit)) {}
  std::uint8_t MobilityLimb(std::size_t index) override {
    return index == 0 ? stats_.mobilityLimb0 : stats_.mobilityLimb1;
  }
  std::int16_t CurrentHp(std::uint8_t limb) override {
    Require(limb < std::size(stats_.limbs));
    return stats_.limbs[limb].hp;
  }
  ActionResultChange PendingHp(std::uint8_t limb) override {
    Require(limb < std::size(hit_.limbs));
    return {hit_.limbs[limb].effects.opaque0,
            static_cast<std::uint8_t>(hit_.hpEffect)};
  }
  std::uint32_t ActiveStatuses() override {
    return std::bit_cast<std::uint32_t>(stats_.statuses);
  }
  bool PlayerTarget() override { return hit_.target.targetActor == 0; }
  std::uint8_t InjuryRule(std::size_t limb) override {
    Require(limb < std::size(Battle_limbInjuryStatusIds));
    return Battle_limbInjuryStatusIds[limb];
  }
  std::uint32_t RuleMask(std::uint8_t rule) override {
    Require(rule < std::size(Battle_statusEffectParams));
    return std::bit_cast<std::uint32_t>(
        Battle_statusEffectParams[rule].statusFlags);
  }
  void QueueStatus(std::uint8_t rule, bool remove) override {
    if (remove) {
      QueueLiveStatusRemovalMask(rule, &hit_);
    } else {
      QueueLiveApplicableStatusChanges(rule, &hit_);
    }
  }

 private:
  Battle_HitResult& hit_;
  Battle_ActorStats& stats_;
};

class LiveEffectWorld final : public ActionEffectWorld {
 public:
  LiveEffectWorld(Battle_ActionDefinition* action, Battle_HitResult* source,
                  Battle_HitResult& target)
      : action_(action), source_(source), target_(target) {}
  bool TargetPosition() override { return target_.targetsPosition != 0; }
  std::uint8_t TargetActorId() override { return StatsOf(target_).actorId; }
  std::uint32_t TargetStatuses() override {
    return std::bit_cast<std::uint32_t>(StatsOf(target_).statuses);
  }
  std::uint8_t ActionType() override {
    Require(action_ != nullptr);
    return static_cast<std::uint8_t>(action_->type);
  }
  void QueueExemptDamage() override {
    constexpr int kExemptDamage = 999;
    target_.hpEffect = 1;
    target_.target.hp = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(target_.target.hp + kExemptDamage));
  }
  void ClearLastValue() override { Battle_lastActionStatValue = 0; }
  void Dispatch(int hit, int execute) override {
    Require(action_ != nullptr && hit >= 0 &&
            static_cast<std::size_t>(hit) < std::size(action_->hits));
    const auto effect = action_->hits[hit].effectId;
    Require(effect < std::size(Battle_actionEffectMediators));
    const auto mediator = Battle_actionEffectMediators[effect];
    Require(mediator != nullptr);
    mediator(action_, source_, &target_, hit, execute);
  }
  void QueueStatusRemoval(std::uint32_t mask) override {
    target_.inflictStatuses = std::bit_cast<std::int32_t>(
        std::bit_cast<std::uint32_t>(target_.inflictStatuses) | mask);
  }
  bool SourcePosition() override { return HitOf(source_).targetsPosition != 0; }
  std::uint8_t SourceSlot() override {
    return HitOf(source_).target.targetActor;
  }
  std::int16_t TargetHp() override { return StatsOf(target_).currentHp; }
  std::int16_t PendingHp() override { return target_.target.hp; }
  std::uint8_t TargetSlot() override { return target_.target.targetActor; }
  bool KillCounted() override { return ActorOf(target_).killCounted != 0; }
  void MarkKillCounted() override { ActorOf(target_).killCounted = 1; }
  std::uint8_t WeaponCategory() override {
    Require(Battle_characterState != nullptr);
    return Battle_characterState->equippedWeaponCategory;
  }
  std::uint32_t ProgressFlag() override {
    constexpr int kWeaponArtProgressFlag = 0x82;
    return State::LiveScriptFlag(kWeaponArtProgressFlag);
  }
  std::uint8_t LearnedArts(std::uint8_t category) override {
    return Audio_Seq_unusedArtsStatus.artsLearned[category];
  }
  std::uint16_t WeaponKills(std::uint8_t category) override {
    return Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[category];
  }
  std::uint16_t RequiredKills(std::uint8_t category,
                              std::uint8_t learned) override {
    return Battle_breakArtPointRequirements[category][learned];
  }
  void SetWeaponKills(std::uint8_t category, std::uint16_t kills) override {
    Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[category] = kills;
  }

 private:
  Battle_ActionDefinition* action_;
  Battle_HitResult* source_;
  Battle_HitResult& target_;
};
using Mediator = std::remove_extent_t<decltype(Battle_actionEffectMediators)>;
static_assert(std::is_same_v<decltype(&ApplyLiveActionHitEffect), Mediator>);
}  // namespace

// decomp: func_80085008 (src/BATTLE/BATTLE.PRG/146C.c:12930).
void DeriveLiveLimbInjuryStatuses(Battle_HitResult* hit) {
  LiveInjuryQueue queue(HitOf(hit));
  DeriveLimbInjuryStatuses(queue);
}
// decomp: func_80085390 (src/BATTLE/BATTLE.PRG/146C.c:13012).
void ApplyLiveActionHitEffect(Battle_ActionDefinition* action,
                              Battle_HitResult* source,
                              Battle_HitResult* target, int execute, int hit) {
  LiveEffectWorld world(action, source, HitOf(target));
  ApplyActionHitEffect(world, execute, hit);
}
}  // namespace VS::Battle
