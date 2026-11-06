// Hand-maintained (tools/port/owned.csv).
// Binds the typed attack prerequisites (AttackPrerequisites.cpp) to the battle
// actor table, the status effect parameters and the prerequisite table slots.
// Each slot of Battle_attackPrerequisiteFunctions in the BATTLE.PRG module
// image resolves to the C binding of its rule here
// (tools/port/copy_fixups.py MODULE_BINDING_ENTRIES).
#include "Battle/AttackPrerequisitesLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

#include "Core/Log.hpp"

extern "C" {
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/AttackPrerequisites.h"
#include "Battle/Damage.h"
#include "Battle/StatusEffects.h"
#include "Items/Equipment.h"
#include "Items/Loadout.h"
}

namespace {
namespace battle = VS::Battle;

static_assert(std::size(Battle_attackPrerequisiteFunctions) ==
              battle::kPrerequisiteCount);
static_assert(std::extent_v<decltype(Battle_ActorStats::limbs)> ==
              static_cast<std::size_t>(battle::kArmorLimbCount));

// The target result byte holding the impact kind already recorded.
constexpr std::size_t kImpactKindByte = 3;

// The target's stats, or nullptr when the result names no live actor.
const Battle_ActorStats* TargetStats(const u_char* target_result_bytes) {
  if (target_result_bytes == nullptr) return nullptr;
  const std::size_t actor = target_result_bytes[0];
  if (actor >= std::size(Battle_actors) || Battle_actors[actor] == nullptr) {
    return nullptr;
  }
  return Battle_actors[actor]->stats;
}

class LivePrerequisiteTarget final : public battle::PrerequisiteTarget {
 public:
  explicit LivePrerequisiteTarget(const u_char* target_result_bytes)
      : stats_(TargetStats(target_result_bytes)),
        actor_(target_result_bytes != nullptr ? target_result_bytes[0] : -1) {}

  // The original reads the actor table entry and its stats unchecked; a
  // target that names no live actor fails the rule instead.
  [[nodiscard]] bool Valid() const override {
    if (stats_ == nullptr) {
      VS_LOG_WARN("battle.targeting",
                  "attack prerequisite for absent target actor={}", actor_);
    }
    return stats_ != nullptr;
  }
  [[nodiscard]] battle::StatusEffectRule EffectRule(int effect) const override {
    if (effect < 0 || static_cast<std::size_t>(effect) >=
                          std::size(Battle_statusEffectParams)) {
      return {};
    }
    const Battle_StatusEffectParams& params = Battle_statusEffectParams[effect];
    return {static_cast<std::uint32_t>(params.statusFlags),
            static_cast<std::uint32_t>(params.blockingStatuses)};
  }
  [[nodiscard]] std::uint32_t StatusImmunities() const override {
    return stats_ != nullptr
               ? static_cast<std::uint32_t>(stats_->statusImmunities)
               : 0U;
  }
  [[nodiscard]] std::uint32_t Statuses() const override {
    return stats_ != nullptr ? static_cast<std::uint32_t>(stats_->statuses)
                             : 0U;
  }
  [[nodiscard]] int EnemyClass() const override {
    return stats_ != nullptr ? static_cast<int>(stats_->enemyClass) : 0;
  }
  [[nodiscard]] int LimbArmorId(int limb) const override {
    if (stats_ == nullptr || limb < 0 || limb >= battle::kArmorLimbCount) {
      return 0;
    }
    return stats_->limbs[limb].armor.armor.id;
  }
  [[nodiscard]] int ShieldId() const override {
    return stats_ != nullptr ? stats_->shield.base.id : 0;
  }
  [[nodiscard]] int BladeId() const override {
    return stats_ != nullptr ? stats_->weapon.blade.id : 0;
  }

 private:
  const Battle_ActorStats* stats_;
  int actor_;
};

int RunRule(battle::PrerequisiteRule rule, const u_char* target_result_bytes) {
  const LivePrerequisiteTarget target(target_result_bytes);
  return rule(target) ? 1 : 0;
}

// The original indexes hits[] and the 40-slot table unchecked.
bool CanCheckHit(const Battle_ActionDefinition* action,
                 const u_char* target_result_bytes, int hit_number) {
  if (action == nullptr || target_result_bytes == nullptr) {
    VS_LOG_WARN("battle.targeting", "hit prerequisite without {}",
                action == nullptr ? "an action" : "a target result");
    return false;
  }
  if (hit_number < 0 ||
      static_cast<std::size_t>(hit_number) >= std::size(action->hits)) {
    VS_LOG_WARN("battle.targeting", "hit prerequisite for hit={} of action={}",
                hit_number, action->id);
    return false;
  }
  const int rule =
      static_cast<int>(action->hits[hit_number].prerequisiteFunctionId);
  if (battle::AttackPrerequisiteRule(rule) == nullptr) {
    VS_LOG_WARN("battle.targeting",
                "unknown hit prerequisite rule={} of action={}", rule,
                action->id);
    return false;
  }
  return true;
}
}  // namespace

namespace VS::Battle {
int LiveCheckActionHitPrerequisite(Battle_ActionDefinition* action,
                                   unsigned char* target_result_bytes,
                                   int hit_number) {
  if (!CanCheckHit(action, target_result_bytes, hit_number)) return 0;
  const HitPrerequisiteRequest request{
      hit_number,
      static_cast<int>(action->hits[hit_number].prerequisiteFunctionId),
      target_result_bytes[kImpactKindByte]};
  const LivePrerequisiteTarget target(target_result_bytes);
  return CheckActionHitPrerequisite(request, target) ? 1 : 0;
}
}  // namespace VS::Battle

// The prerequisite table slots (decomp: _attackPrerequisiteFunctions,
// src/BATTLE/BATTLE.PRG/146C.c:628), with the dispatch ABI.
extern "C" {
// decomp: _hitPrerequisiteNever0..3 (146C.c:10146, 10180, 10186, 10192)
int Battle_RejectAttackPrerequisiteSlot(Battle_ActionDefinition* /*action*/,
                                        u_char* target_result_bytes) {
  return RunRule(battle::RejectAttackPrerequisite, target_result_bytes);
}

// decomp: _hitPrerequisiteAlways0..2 (146C.c:10152, 10158, 10164)
int Battle_AcceptAttackPrerequisiteSlot(Battle_ActionDefinition* /*action*/,
                                        u_char* target_result_bytes) {
  return RunRule(battle::AcceptAttackPrerequisite, target_result_bytes);
}

// decomp: _hitPrerequisiteUndead (146C.c:10170)
int Battle_IsAttackTargetUndeadSlot(Battle_ActionDefinition* /*action*/,
                                    u_char* target_result_bytes) {
  return RunRule(battle::IsAttackTargetUndead, target_result_bytes);
}

// decomp: _hitPrerequisiteNotUndead (146C.c:10175)
int Battle_IsAttackTargetLivingSlot(Battle_ActionDefinition* /*action*/,
                                    u_char* target_result_bytes) {
  return RunRule(battle::IsAttackTargetLiving, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyStrDown (146C.c:10198)
int Battle_CanApplyStrengthDownSlot(Battle_ActionDefinition* /*action*/,
                                    u_char* target_result_bytes) {
  return RunRule(battle::CanApplyStrengthDown, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyIntDown (146C.c:10204)
int Battle_CanApplyIntelligenceDownSlot(Battle_ActionDefinition* /*action*/,
                                        u_char* target_result_bytes) {
  return RunRule(battle::CanApplyIntelligenceDown, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyAglDown (146C.c:10210)
int Battle_CanApplyAgilityDownSlot(Battle_ActionDefinition* /*action*/,
                                   u_char* target_result_bytes) {
  return RunRule(battle::CanApplyAgilityDown, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyEquipDown (146C.c:10216)
int Battle_CanApplyEquipmentDownSlot(Battle_ActionDefinition* /*action*/,
                                     u_char* target_result_bytes) {
  return RunRule(battle::CanApplyEquipmentDown, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyHalfMove (146C.c:10250)
int Battle_CanApplyHalfMovementSlot(Battle_ActionDefinition* /*action*/,
                                    u_char* target_result_bytes) {
  return RunRule(battle::CanApplyHalfMovement, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyStrUp (146C.c:10256)
int Battle_CanApplyStrengthUpSlot(Battle_ActionDefinition* /*action*/,
                                  u_char* target_result_bytes) {
  return RunRule(battle::CanApplyStrengthUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyIntUp (146C.c:10261)
int Battle_CanApplyIntelligenceUpSlot(Battle_ActionDefinition* /*action*/,
                                      u_char* target_result_bytes) {
  return RunRule(battle::CanApplyIntelligenceUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyAglUp (146C.c:10266)
int Battle_CanApplyAgilityUpSlot(Battle_ActionDefinition* /*action*/,
                                 u_char* target_result_bytes) {
  return RunRule(battle::CanApplyAgilityUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyEquipUp (146C.c:10271)
int Battle_CanApplyEquipmentUpSlot(Battle_ActionDefinition* /*action*/,
                                   u_char* target_result_bytes) {
  return RunRule(battle::CanApplyEquipmentUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyQuicken (146C.c:10305)
int Battle_CanApplyQuickenSlot(Battle_ActionDefinition* /*action*/,
                               u_char* target_result_bytes) {
  return RunRule(battle::CanApplyQuicken, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplySilence (146C.c:10311)
int Battle_CanApplySilenceSlot(Battle_ActionDefinition* /*action*/,
                               u_char* target_result_bytes) {
  return RunRule(battle::CanApplySilence, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyMagicWard (146C.c:10317)
int Battle_CanApplyMagicWardSlot(Battle_ActionDefinition* /*action*/,
                                 u_char* target_result_bytes) {
  return RunRule(battle::CanApplyMagicWard, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyRegen (146C.c:10323)
int Battle_CanApplyRegenerationSlot(Battle_ActionDefinition* /*action*/,
                                    u_char* target_result_bytes) {
  return RunRule(battle::CanApplyRegeneration, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyParalysis (146C.c:10329)
int Battle_CanApplyParalysisSlot(Battle_ActionDefinition* /*action*/,
                                 u_char* target_result_bytes) {
  return RunRule(battle::CanApplyParalysis, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyPoison (146C.c:10335)
int Battle_CanApplyPoisonSlot(Battle_ActionDefinition* /*action*/,
                              u_char* target_result_bytes) {
  return RunRule(battle::CanApplyPoison, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyNumbness (146C.c:10341)
int Battle_CanApplyNumbnessSlot(Battle_ActionDefinition* /*action*/,
                                u_char* target_result_bytes) {
  return RunRule(battle::CanApplyNumbness, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyCurse (146C.c:10347)
int Battle_CanApplyCurseSlot(Battle_ActionDefinition* /*action*/,
                             u_char* target_result_bytes) {
  return RunRule(battle::CanApplyCurse, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyAirAttackUp (146C.c:10353)
int Battle_CanApplyAirAttackUpSlot(Battle_ActionDefinition* /*action*/,
                                   u_char* target_result_bytes) {
  return RunRule(battle::CanApplyAirAttackUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyFireAttackUp (146C.c:10363)
int Battle_CanApplyFireAttackUpSlot(Battle_ActionDefinition* /*action*/,
                                    u_char* target_result_bytes) {
  return RunRule(battle::CanApplyFireAttackUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyEarthAttackUp (146C.c:10373)
int Battle_CanApplyEarthAttackUpSlot(Battle_ActionDefinition* /*action*/,
                                     u_char* target_result_bytes) {
  return RunRule(battle::CanApplyEarthAttackUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyWaterAttackUp (146C.c:10383)
int Battle_CanApplyWaterAttackUpSlot(Battle_ActionDefinition* /*action*/,
                                     u_char* target_result_bytes) {
  return RunRule(battle::CanApplyWaterAttackUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyAirDefenseUp (146C.c:10393)
int Battle_CanApplyAirDefenseUpSlot(Battle_ActionDefinition* /*action*/,
                                    u_char* target_result_bytes) {
  return RunRule(battle::CanApplyAirDefenseUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyFireDefenseUp (146C.c:10418)
int Battle_CanApplyFireDefenseUpSlot(Battle_ActionDefinition* /*action*/,
                                     u_char* target_result_bytes) {
  return RunRule(battle::CanApplyFireDefenseUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyEarthDefenseUp (146C.c:10442)
int Battle_CanApplyEarthDefenseUpSlot(Battle_ActionDefinition* /*action*/,
                                      u_char* target_result_bytes) {
  return RunRule(battle::CanApplyEarthDefenseUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyWaterDefenseUp (146C.c:10466)
int Battle_CanApplyWaterDefenseUpSlot(Battle_ActionDefinition* /*action*/,
                                      u_char* target_result_bytes) {
  return RunRule(battle::CanApplyWaterDefenseUp, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyBanish (146C.c:10491)
int Battle_CanApplyBanishSlot(Battle_ActionDefinition* /*action*/,
                              u_char* target_result_bytes) {
  return RunRule(battle::CanApplyBanish, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyExorcise (146C.c:10497)
int Battle_CanApplyExorciseSlot(Battle_ActionDefinition* /*action*/,
                                u_char* target_result_bytes) {
  return RunRule(battle::CanApplyExorcise, target_result_bytes);
}

// decomp: _hitPrerequisiteCanApplyDrain (146C.c:10507)
int Battle_CanApplyDrainSlot(Battle_ActionDefinition* /*action*/,
                             u_char* target_result_bytes) {
  return RunRule(battle::CanApplyDrain, target_result_bytes);
}

// decomp: _hitPrerequisiteUnused0..2 (146C.c:10517, 10519, 10521)
int Battle_IgnoreAttackPrerequisiteSlot(Battle_ActionDefinition* /*action*/,
                                        u_char* target_result_bytes) {
  return RunRule(battle::IgnoreAttackPrerequisite, target_result_bytes);
}
}  // extern "C"
