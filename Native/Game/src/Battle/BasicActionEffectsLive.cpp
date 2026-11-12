// Hand-maintained (tools/port/owned.csv).
#include "Battle/BasicActionEffectsLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/BasicActionEffects.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Core/RandomLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("battle", "%s", message);
    std::abort();
  }
}

Battle_HitResult& HitOf(Battle_HitResult* hit) {
  Require(hit != nullptr, "Basic action effect requires its used hit result");
  return *hit;
}

class LiveInstantDeathQueue final : public InstantDeathQueue {
 public:
  explicit LiveInstantDeathQueue(Battle_HitResult& hit) : hit_(hit) {}
  void RemoveStatuses(StatusRuleId rule) override {
    QueueLiveStatusRemovalMask(static_cast<int>(rule), &hit_);
  }
  std::uint16_t CurrentHp() override {
    const unsigned slot = hit_.target.targetActor;
    Require(slot < std::size(Battle_actors),
            "Instant death target is outside the battle actor table");
    const auto* actor = Battle_actors[slot];
    Require(actor != nullptr && actor->stats != nullptr,
            "Instant death requires the target actor's stats");
    return static_cast<std::uint16_t>(actor->stats->currentHp);
  }
  void QueueHp(ActionResultChange change) override {
    hit_.hpEffect = change.mode;
    hit_.target.hp = change.amount;
  }

 private:
  Battle_HitResult& hit_;
};

class LiveActionScriptFlags final : public ActionScriptFlags {
 public:
  void Set(ActionScriptFlag flag, std::uint8_t value) override {
    State::SetLiveScriptFlag(static_cast<int>(flag), value);
  }
};

class LiveRandomStatQueue final : public RandomStatQueue {
 public:
  explicit LiveRandomStatQueue(Battle_HitResult& hit) : hit_(hit) {}
  std::int32_t DrawBounded(std::int32_t bound) override {
    return Core::NextLiveRandomBounded(bound);
  }
  ActionResultChange ReadChange(ActionResultField field) override {
    switch (field) {
      case ActionResultField::kStrength:
        return {hit_.strengthChange,
                static_cast<std::uint8_t>(hit_.strengthEffect)};
      case ActionResultField::kIntelligence:
        return {hit_.intelligenceChange,
                static_cast<std::uint8_t>(hit_.intelligenceEffect)};
      case ActionResultField::kAgility:
        return {hit_.agilityChange,
                static_cast<std::uint8_t>(hit_.agilityEffect)};
      case ActionResultField::kMaxHp:
        return {hit_.maxHpChange, static_cast<std::uint8_t>(hit_.maxHpEffect)};
      case ActionResultField::kMaxMp:
        return {hit_.maxMpChange, static_cast<std::uint8_t>(hit_.maxMpEffect)};
      default:
        Require(false, "Random stat queue requires a supported stat field");
        return {};
    }
  }
  void WriteChange(ActionResultField field,
                   ActionResultChange change) override {
    switch (field) {
      case ActionResultField::kStrength:
        hit_.strengthEffect = change.mode;
        hit_.strengthChange = change.amount;
        break;
      case ActionResultField::kIntelligence:
        hit_.intelligenceEffect = change.mode;
        hit_.intelligenceChange = change.amount;
        break;
      case ActionResultField::kAgility:
        hit_.agilityEffect = change.mode;
        hit_.agilityChange = change.amount;
        break;
      case ActionResultField::kMaxHp:
        hit_.maxHpEffect = change.mode;
        hit_.maxHpChange = change.amount;
        break;
      case ActionResultField::kMaxMp:
        hit_.maxMpEffect = change.mode;
        hit_.maxMpChange = change.amount;
        break;
      default:
        Require(false, "Random stat queue requires a supported stat field");
    }
  }

 private:
  Battle_HitResult& hit_;
};

using Mediator = std::remove_extent_t<decltype(Battle_actionEffectMediators)>;
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

// decomp: _increaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12717).
void QueueLiveRandomStrengthIncrease(Battle_ActionDefinition*,
                                     Battle_HitResult*,
                                     Battle_HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomStrengthIncrease(queue);
}

// decomp: _increaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12726).
void QueueLiveRandomIntelligenceIncrease(Battle_ActionDefinition*,
                                         Battle_HitResult*,
                                         Battle_HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomIntelligenceIncrease(queue);
}

// decomp: _increaseAgilityRand (src/BATTLE/BATTLE.PRG/146C.c:12735).
void QueueLiveRandomAgilityIncrease(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomAgilityIncrease(queue);
}

// decomp: _increaseMaxHpRand (src/BATTLE/BATTLE.PRG/146C.c:12744).
void QueueLiveRandomMaxHpIncrease(Battle_ActionDefinition*, Battle_HitResult*,
                                  Battle_HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomMaxHpIncrease(queue);
}

// decomp: _increaseMaxMpRand (src/BATTLE/BATTLE.PRG/146C.c:12753).
void QueueLiveRandomMaxMpIncrease(Battle_ActionDefinition*, Battle_HitResult*,
                                  Battle_HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomMaxMpIncrease(queue);
}

// decomp: _decreaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12762).
void QueueLiveRandomStrengthDecrease(Battle_ActionDefinition*,
                                     Battle_HitResult*,
                                     Battle_HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomStrengthDecrease(queue);
}

// decomp: _decreaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12771).
void QueueLiveRandomIntelligenceDecrease(Battle_ActionDefinition*,
                                         Battle_HitResult*,
                                         Battle_HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomIntelligenceDecrease(queue);
}

// decomp: _decreaseAglityRand (src/BATTLE/BATTLE.PRG/146C.c:12780).
void QueueLiveRandomAgilityDecrease(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult* target, int, int) {
  LiveRandomStatQueue queue(HitOf(target));
  QueueRandomAgilityDecrease(queue);
}

// decomp: func_80084228 / func_800842F0 / func_80084370
// (src/BATTLE/BATTLE.PRG/146C.c:12547/12572/12599).
void MarkLiveSourceActionSuccess(Battle_ActionDefinition*,
                                 Battle_HitResult* source, Battle_HitResult*,
                                 int, int) {
  auto& hit = HitOf(source);
  auto outcome = static_cast<SourceActionOutcome>(hit.sourceResult);
  MarkSourceActionSuccess(outcome);
  hit.sourceResult = static_cast<std::uint8_t>(outcome);
}

// decomp: func_80084248 (src/BATTLE/BATTLE.PRG/146C.c:12554).
void QueueLiveInstantDeath(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult* target, int, int) {
  LiveInstantDeathQueue queue(HitOf(target));
  QueueInstantDeath(queue);
}

// decomp: func_800842AC (src/BATTLE/BATTLE.PRG/146C.c:12563).
void QueueLiveDamageBasedArmorWear(Battle_ActionDefinition*, Battle_HitResult*,
                                   Battle_HitResult* target, int, int) {
  auto& hit = HitOf(target);
  ActorActionResult queued{};
  queued.limb = std::bit_cast<std::int8_t>(hit.target.targetLimb);
  queued.changes.at(static_cast<std::size_t>(ActionResultField::kHp)).amount =
      hit.target.hp;
  Require(QueueDamageBasedArmorWear(queued),
          "Armor wear requires a target limb in [0, 6)");
  const auto& armor =
      queued.changes.at(static_cast<std::size_t>(ActionResultField::kArmorDp));
  hit.armorDpChange = armor.amount;
  hit.limbs[queued.limb].effects.opaque2 =
      queued.limbs.at(static_cast<std::size_t>(queued.limb)).armor_dp;
  hit.armorDpEffect = armor.mode;
}

// decomp: func_80084310 (src/BATTLE/BATTLE.PRG/146C.c:12579).
void SetLiveActionFlag64(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult*, int, int execute) {
  LiveActionScriptFlags flags;
  SetActionFlag64(execute != 0, flags);
}

// decomp: func_80084340 (src/BATTLE/BATTLE.PRG/146C.c:12589).
void SetLiveActionFlag32(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult*, int, int execute) {
  LiveActionScriptFlags flags;
  SetActionFlag32(execute != 0, flags);
}
}  // namespace VS::Battle
