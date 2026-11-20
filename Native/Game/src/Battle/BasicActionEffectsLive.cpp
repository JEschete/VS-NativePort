// Hand-maintained (tools/port/owned.csv).
#include "Battle/BasicActionEffectsLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/ActionFlow.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/BasicActionEffects.hpp"
#include "Battle/Damage.hpp"
#include "Battle/DamagePoliciesLive.hpp"
#include "Battle/Hit.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/RandomLive.hpp"
#include "State/ScriptFlagsLive.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::ActionRequestRecord;

namespace VS::Battle {
namespace {
void Require(bool valid, const char* message) {
  if (!valid) {
    VS::Core::Errorf("battle", "%s", message);
    std::abort();
  }
}

VS::Battle::HitResult& HitOf(VS::Battle::HitResult* hit) {
  Require(hit != nullptr, "Basic action effect requires its used hit result");
  return *hit;
}

class LiveTriggeredRecoveryQueue final : public TriggeredRecoveryQueue {
 public:
  LiveTriggeredRecoveryQueue(ActionDefinition* action,
                             VS::Battle::HitResult* source,
                             VS::Battle::HitResult* target, int hit,
                             int execute)
      : action_(action),
        source_(source),
        target_(target),
        hit_(hit),
        execute_(execute) {}
  std::uint8_t SelectedActionType() override {
    const auto id = Selection().action_id;
    Require(id < std::size(VS::Battle::actionDefinitions),
            "Triggered recovery selected action is outside definitions");
    return static_cast<std::uint8_t>(VS::Battle::actionDefinitions[id].type);
  }
  std::uint8_t SelectedAffinity() override {
    return static_cast<std::uint8_t>(Selection().effect_variant);
  }
  std::int16_t Calculate() override {
    return LiveCalculateActionStatChange(action_, source_, target_, hit_,
                                         execute_);
  }
  ActionResultChange ReadHp() override {
    const auto& target = HitOf(target_);
    return {target.target.hp, static_cast<std::uint8_t>(target.hp_effect)};
  }
  void WriteHp(ActionResultChange change) override {
    auto& target = HitOf(target_);
    target.hp_effect = change.mode;
    target.target.hp = change.amount;
  }
  void RecoverLimbs(std::int16_t amount) override {
    LiveDistributeLimbRecovery(action_, target_, amount);
  }

 private:
  static const ActionRequestRecord& Selection() {
    Require(VS::Battle::actionState != nullptr,
            "Triggered recovery requires selected action state");
    return VS::Battle::actionState->selection;
  }
  ActionDefinition* action_;
  VS::Battle::HitResult* source_;
  VS::Battle::HitResult* target_;
  int hit_;
  int execute_;
};

class LiveInstantDeathQueue final : public InstantDeathQueue {
 public:
  explicit LiveInstantDeathQueue(VS::Battle::HitResult& hit) : hit_(hit) {}
  void RemoveStatuses(StatusRuleId rule) override {
    QueueLiveStatusRemovalMask(static_cast<int>(rule), &hit_);
  }
  std::uint16_t CurrentHp() override {
    const unsigned slot = hit_.target.target_actor;
    Require(slot < std::size(VS::Battle::actors),
            "Instant death target is outside the battle actor table");
    const auto* actor = VS::Battle::actors[slot];
    Require(actor != nullptr && actor->stats != nullptr,
            "Instant death requires the target actor's stats");
    return static_cast<std::uint16_t>(actor->stats->current_hp);
  }
  void QueueHp(ActionResultChange change) override {
    hit_.hp_effect = change.mode;
    hit_.target.hp = change.amount;
  }

 private:
  VS::Battle::HitResult& hit_;
};

class LiveActionScriptFlags final : public ActionScriptFlags {
 public:
  void Set(ActionScriptFlag flag, std::uint8_t value) override {
    State::SetLiveScriptFlag(static_cast<int>(flag), value);
  }
};

class LiveRandomStatQueue final : public RandomStatQueue {
 public:
  explicit LiveRandomStatQueue(VS::Battle::HitResult& hit) : hit_(hit) {}
  std::int32_t DrawBounded(std::int32_t bound) override {
    return Core::NextLiveRandomBounded(bound);
  }
  ActionResultChange ReadChange(ActionResultField field) override {
    switch (field) {
      case ActionResultField::kStrength:
        return {hit_.strength_change,
                static_cast<std::uint8_t>(hit_.strength_effect)};
      case ActionResultField::kIntelligence:
        return {hit_.intelligence_change,
                static_cast<std::uint8_t>(hit_.intelligence_effect)};
      case ActionResultField::kAgility:
        return {hit_.agility_change,
                static_cast<std::uint8_t>(hit_.agility_effect)};
      case ActionResultField::kMaxHp:
        return {hit_.max_hp_change,
                static_cast<std::uint8_t>(hit_.max_hp_effect)};
      case ActionResultField::kMaxMp:
        return {hit_.max_mp_change,
                static_cast<std::uint8_t>(hit_.max_mp_effect)};
      default:
        Require(false, "Random stat queue requires a supported stat field");
        return {};
    }
  }
  void WriteChange(ActionResultField field,
                   ActionResultChange change) override {
    switch (field) {
      case ActionResultField::kStrength:
        hit_.strength_effect = change.mode;
        hit_.strength_change = change.amount;
        break;
      case ActionResultField::kIntelligence:
        hit_.intelligence_effect = change.mode;
        hit_.intelligence_change = change.amount;
        break;
      case ActionResultField::kAgility:
        hit_.agility_effect = change.mode;
        hit_.agility_change = change.amount;
        break;
      case ActionResultField::kMaxHp:
        hit_.max_hp_effect = change.mode;
        hit_.max_hp_change = change.amount;
        break;
      case ActionResultField::kMaxMp:
        hit_.max_mp_effect = change.mode;
        hit_.max_mp_change = change.amount;
        break;
      default:
        Require(false, "Random stat queue requires a supported stat field");
    }
  }

 private:
  VS::Battle::HitResult& hit_;
};

using Mediator = VS::Battle::ActionEffectMediator;
static_assert(
    std::is_same_v<decltype(&QueueLiveDarkAffinityRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveDamageTriggeredRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveSpellTriggeredRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveNonSpellTriggeredRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLivePhysicalAffinityRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveAirAffinityRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveFireAffinityRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveEarthAffinityRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveWaterAffinityRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveLightAffinityRecovery), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveRandomStrengthIncrease), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveRandomIntelligenceIncrease), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveRandomAgilityIncrease), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveRandomMaxHpIncrease), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveRandomMaxMpIncrease), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveRandomStrengthDecrease), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveRandomIntelligenceDecrease), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveRandomAgilityDecrease), Mediator>);
static_assert(std::is_same_v<decltype(&MarkLiveSourceActionSuccess), Mediator>);
static_assert(std::is_same_v<decltype(&QueueLiveInstantDeath), Mediator>);
static_assert(
    std::is_same_v<decltype(&QueueLiveDamageBasedArmorWear), Mediator>);
static_assert(std::is_same_v<decltype(&SetLiveActionFlag64), Mediator>);
static_assert(std::is_same_v<decltype(&SetLiveActionFlag32), Mediator>);
}  // namespace

// decomp: func_800847F0 (src/BATTLE/BATTLE.PRG/146C.c:12694).
void QueueLiveDarkAffinityRecovery(ActionDefinition* action,
                                   VS::Battle::HitResult* source,
                                   VS::Battle::HitResult* target, int hit,
                                   int execute) {
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueueDarkAffinityRecovery(queue);
}
// decomp: func_80084870 (src/BATTLE/BATTLE.PRG/146C.c:12705).
void QueueLiveDamageTriggeredRecovery(ActionDefinition* action,
                                      VS::Battle::HitResult* source,
                                      VS::Battle::HitResult* target, int hit,
                                      int execute) {
  Require(VS::Battle::actionState != nullptr,
          "Damage-triggered recovery requires selected action state");
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueueDamageTriggeredRecovery(
      static_cast<std::uint8_t>(
          VS::Battle::actionState->selection.primary_target.hp_effect),
      queue);
}

// decomp: func_80084390 (src/BATTLE/BATTLE.PRG/146C.c:12606).
void QueueLiveSpellTriggeredRecovery(ActionDefinition* action,
                                     VS::Battle::HitResult* source,
                                     VS::Battle::HitResult* target, int hit,
                                     int execute) {
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueueSpellTriggeredRecovery(queue);
}
// decomp: func_80084440 (src/BATTLE/BATTLE.PRG/146C.c:12617).
void QueueLiveNonSpellTriggeredRecovery(ActionDefinition* action,
                                        VS::Battle::HitResult* source,
                                        VS::Battle::HitResult* target, int hit,
                                        int execute) {
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueueNonSpellTriggeredRecovery(queue);
}
// decomp: func_800844F0 (src/BATTLE/BATTLE.PRG/146C.c:12628).
void QueueLivePhysicalAffinityRecovery(ActionDefinition* action,
                                       VS::Battle::HitResult* source,
                                       VS::Battle::HitResult* target, int hit,
                                       int execute) {
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueuePhysicalAffinityRecovery(queue);
}
// decomp: func_80084570 (src/BATTLE/BATTLE.PRG/146C.c:12639).
void QueueLiveAirAffinityRecovery(ActionDefinition* action,
                                  VS::Battle::HitResult* source,
                                  VS::Battle::HitResult* target, int hit,
                                  int execute) {
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueueAirAffinityRecovery(queue);
}
// decomp: func_800845F0 (src/BATTLE/BATTLE.PRG/146C.c:12650).
void QueueLiveFireAffinityRecovery(ActionDefinition* action,
                                   VS::Battle::HitResult* source,
                                   VS::Battle::HitResult* target, int hit,
                                   int execute) {
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueueFireAffinityRecovery(queue);
}
// decomp: func_80084670 (src/BATTLE/BATTLE.PRG/146C.c:12661).
void QueueLiveEarthAffinityRecovery(ActionDefinition* action,
                                    VS::Battle::HitResult* source,
                                    VS::Battle::HitResult* target, int hit,
                                    int execute) {
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueueEarthAffinityRecovery(queue);
}
// decomp: func_800846F0 (src/BATTLE/BATTLE.PRG/146C.c:12672).
void QueueLiveWaterAffinityRecovery(ActionDefinition* action,
                                    VS::Battle::HitResult* source,
                                    VS::Battle::HitResult* target, int hit,
                                    int execute) {
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueueWaterAffinityRecovery(queue);
}
// decomp: func_80084770 (src/BATTLE/BATTLE.PRG/146C.c:12683).
void QueueLiveLightAffinityRecovery(ActionDefinition* action,
                                    VS::Battle::HitResult* source,
                                    VS::Battle::HitResult* target, int hit,
                                    int execute) {
  LiveTriggeredRecoveryQueue queue(action, source, target, hit, execute);
  QueueLightAffinityRecovery(queue);
}

// decomp: _increaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12717).
void QueueLiveRandomStrengthIncrease(ActionDefinition*, VS::Battle::HitResult*,
                                     VS::Battle::HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomStrengthIncrease(queue);
}

// decomp: _increaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12726).
void QueueLiveRandomIntelligenceIncrease(ActionDefinition*,
                                         VS::Battle::HitResult*,
                                         VS::Battle::HitResult* target, int,
                                         int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomIntelligenceIncrease(queue);
}

// decomp: _increaseAgilityRand (src/BATTLE/BATTLE.PRG/146C.c:12735).
void QueueLiveRandomAgilityIncrease(ActionDefinition*, VS::Battle::HitResult*,
                                    VS::Battle::HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomAgilityIncrease(queue);
}

// decomp: _increaseMaxHpRand (src/BATTLE/BATTLE.PRG/146C.c:12744).
void QueueLiveRandomMaxHpIncrease(ActionDefinition*, VS::Battle::HitResult*,
                                  VS::Battle::HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomMaxHpIncrease(queue);
}

// decomp: _increaseMaxMpRand (src/BATTLE/BATTLE.PRG/146C.c:12753).
void QueueLiveRandomMaxMpIncrease(ActionDefinition*, VS::Battle::HitResult*,
                                  VS::Battle::HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomMaxMpIncrease(queue);
}

// decomp: _decreaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12762).
void QueueLiveRandomStrengthDecrease(ActionDefinition*, VS::Battle::HitResult*,
                                     VS::Battle::HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomStrengthDecrease(queue);
}

// decomp: _decreaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12771).
void QueueLiveRandomIntelligenceDecrease(ActionDefinition*,
                                         VS::Battle::HitResult*,
                                         VS::Battle::HitResult* target, int,
                                         int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomIntelligenceDecrease(queue);
}

// decomp: _decreaseAglityRand (src/BATTLE/BATTLE.PRG/146C.c:12780).
void QueueLiveRandomAgilityDecrease(ActionDefinition*, VS::Battle::HitResult*,
                                    VS::Battle::HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomAgilityDecrease(queue);
}

// decomp: func_80084228 / func_800842F0 / func_80084370
// (src/BATTLE/BATTLE.PRG/146C.c:12547/12572/12599).
void MarkLiveSourceActionSuccess(ActionDefinition*,
                                 VS::Battle::HitResult* source,
                                 VS::Battle::HitResult*, int, int) {
  auto& hit = HitOf(source);
  auto outcome = static_cast<SourceActionOutcome>(hit.source_result);
  MarkSourceActionSuccess(outcome);
  hit.source_result = static_cast<std::uint8_t>(outcome);
}

// decomp: func_80084248 (src/BATTLE/BATTLE.PRG/146C.c:12554).
void QueueLiveInstantDeath(ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int) {
  LiveInstantDeathQueue queue(HitOf(target));
  QueueInstantDeath(queue);
}

// decomp: func_800842AC (src/BATTLE/BATTLE.PRG/146C.c:12563).
void QueueLiveDamageBasedArmorWear(ActionDefinition*, VS::Battle::HitResult*,
                                   VS::Battle::HitResult* target, int, int) {
  auto& hit = HitOf(target);
  ActorActionResult queued{};
  queued.limb = std::bit_cast<std::int8_t>(hit.target.target_limb);
  queued.changes.at(static_cast<std::size_t>(ActionResultField::kHp)).amount =
      hit.target.hp;
  Require(QueueDamageBasedArmorWear(queued),
          "Armor wear requires a target limb in [0, 6)");
  const auto& armor =
      queued.changes.at(static_cast<std::size_t>(ActionResultField::kArmorDp));
  hit.armor_dp_change = armor.amount;
  hit.limbs[queued.limb].effects.opaque2 =
      queued.limbs.at(static_cast<std::size_t>(queued.limb)).armor_dp;
  hit.armor_dp_effect = armor.mode;
}

// decomp: func_80084310 (src/BATTLE/BATTLE.PRG/146C.c:12579).
void SetLiveActionFlag64(ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int execute) {
  LiveActionScriptFlags flags;
  SetActionFlag64(execute != 0, flags);
}

// decomp: func_80084340 (src/BATTLE/BATTLE.PRG/146C.c:12589).
void SetLiveActionFlag32(ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int execute) {
  LiveActionScriptFlags flags;
  SetActionFlag32(execute != 0, flags);
}
}  // namespace VS::Battle
