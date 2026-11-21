// Hand-maintained (tools/port/owned.csv).
// Binds the typed attack prerequisites (AttackPrerequisites.cpp) to the battle
// actor table, the status effect parameters and the prerequisite table slots.
// Each slot of VS::Battle::attackPrerequisiteFunctions in the BATTLE.PRG module
// image resolves to the C binding of its rule here
// (tools/port/copy_fixups.py MODULE_BINDING_ENTRIES).
#include "Battle/AttackPrerequisitesLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

#include "Battle/Action.hpp"
#include "Battle/Actor.hpp"
#include "Battle/AttackPrerequisiteTable.hpp"
#include "Battle/Damage.hpp"
#include "Battle/StatusEffects.hpp"
#include "Core/Log.hpp"
#include "Items/ItemRecords.hpp"

using VS::Battle::ActionDefinition;

namespace {
namespace battle = VS::Battle;

static_assert(std::size(battle::attackPrerequisiteFunctions) ==
              battle::kPrerequisiteCount);
static_assert(std::tuple_size_v<decltype(VS::Battle::ActorStats::limbs)> ==
              static_cast<std::size_t>(battle::kArmorLimbCount));

// The target result byte holding the impact kind already recorded.
constexpr std::size_t kImpactKindByte = 3;

// The target's stats, or nullptr when the result names no live actor.
const VS::Battle::ActorStats* TargetStats(
    const std::uint8_t* target_result_bytes) {
  if (target_result_bytes == nullptr) return nullptr;
  const std::size_t actor = target_result_bytes[0];
  if (actor >= std::size(VS::Battle::actors) ||
      VS::Battle::actors[actor] == nullptr) {
    return nullptr;
  }
  return VS::Battle::actors[actor]->stats;
}

class LivePrerequisiteTarget final : public battle::PrerequisiteTarget {
 public:
  explicit LivePrerequisiteTarget(const std::uint8_t* target_result_bytes)
      : stats_(TargetStats(target_result_bytes)),
        actor_(target_result_bytes != nullptr ? target_result_bytes[0] : -1) {}

  // The original reads the actor table entry and its stats unchecked; a
  // target that names no live actor fails the rule instead.
  [[nodiscard]] bool Valid() const override {
    if (stats_ == nullptr) {
      VS::Core::LogWarn("battle.targeting",
                        "attack prerequisite for absent target actor={}",
                        actor_);
    }
    return stats_ != nullptr;
  }
  [[nodiscard]] battle::StatusEffectRule EffectRule(int effect) const override {
    if (effect < 0 || static_cast<std::size_t>(effect) >=
                          std::size(VS::Battle::statusEffectParams)) {
      return {};
    }
    const VS::Battle::StatusEffectParams& params =
        VS::Battle::statusEffectParams[effect];
    return {static_cast<std::uint32_t>(params.status_flags),
            static_cast<std::uint32_t>(params.blocking_statuses)};
  }
  [[nodiscard]] std::uint32_t StatusImmunities() const override {
    return stats_ != nullptr
               ? static_cast<std::uint32_t>(stats_->status_immunities)
               : 0U;
  }
  [[nodiscard]] std::uint32_t Statuses() const override {
    return stats_ != nullptr ? static_cast<std::uint32_t>(stats_->statuses)
                             : 0U;
  }
  [[nodiscard]] int EnemyClass() const override {
    return stats_ != nullptr ? static_cast<int>(stats_->enemy_class) : 0;
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
  const VS::Battle::ActorStats* stats_;
  int actor_;
};

int RunRule(battle::PrerequisiteRule rule,
            const std::uint8_t* target_result_bytes) {
  const LivePrerequisiteTarget target(target_result_bytes);
  return rule(target) ? 1 : 0;
}

// The original indexes hits[] and the 40-slot table unchecked.
bool CanCheckHit(const ActionDefinition* action,
                 const std::uint8_t* target_result_bytes, int hit_number) {
  if (action == nullptr || target_result_bytes == nullptr) {
    VS::Core::LogWarn("battle.targeting", "hit prerequisite without {}",
                      action == nullptr ? "an action" : "a target result");
    return false;
  }
  if (hit_number < 0 ||
      static_cast<std::size_t>(hit_number) >= std::size(action->hits)) {
    VS::Core::LogWarn("battle.targeting",
                      "hit prerequisite for hit={} of action={}", hit_number,
                      action->id);
    return false;
  }
  const int rule =
      static_cast<int>(action->hits[hit_number].prerequisite_function_id);
  if (battle::AttackPrerequisiteRule(rule) == nullptr) {
    VS::Core::LogWarn("battle.targeting",
                      "unknown hit prerequisite rule={} of action={}", rule,
                      action->id);
    return false;
  }
  return true;
}
}  // namespace

namespace VS::Battle {
int LiveCheckActionHitPrerequisite(ActionDefinition* action,
                                   unsigned char* target_result_bytes,
                                   int hit_number) {
  if (!CanCheckHit(action, target_result_bytes, hit_number)) return 0;
  const HitPrerequisiteRequest request{
      hit_number,
      static_cast<int>(action->hits[hit_number].prerequisite_function_id),
      target_result_bytes[kImpactKindByte]};
  const LivePrerequisiteTarget target(target_result_bytes);
  return CheckActionHitPrerequisite(request, target) ? 1 : 0;
}
}  // namespace VS::Battle

// The prerequisite table slots (decomp: _attackPrerequisiteFunctions,
// src/BATTLE/BATTLE.PRG/146C.c:628), with the dispatch ABI.
// decomp: hitPrerequisiteNever0..3 (src/BATTLE/BATTLE.PRG/146C.c:10132; was
// _hitPrerequisiteNever0, 10180, 10186, 10192)
int VS::Battle::RejectAttackPrerequisiteSlot(
    ActionDefinition* /*action*/, std::uint8_t* target_result_bytes) {
  return RunRule(battle::RejectAttackPrerequisite, target_result_bytes);
}

// decomp: hitPrerequisiteAlways0..2 (src/BATTLE/BATTLE.PRG/146C.c:10134; was
// _hitPrerequisiteAlways0, 10158, 10164)
int VS::Battle::AcceptAttackPrerequisiteSlot(
    ActionDefinition* /*action*/, std::uint8_t* target_result_bytes) {
  return RunRule(battle::AcceptAttackPrerequisite, target_result_bytes);
}

// decomp: hitPrerequisiteUndead (src/BATTLE/BATTLE.PRG/146C.c:10140; was
// _hitPrerequisiteUndead)
int VS::Battle::IsAttackTargetUndeadSlot(ActionDefinition* /*action*/,
                                         std::uint8_t* target_result_bytes) {
  return RunRule(battle::IsAttackTargetUndead, target_result_bytes);
}

// decomp: hitPrerequisiteNotUndead (src/BATTLE/BATTLE.PRG/146C.c:10145; was
// _hitPrerequisiteNotUndead)
int VS::Battle::IsAttackTargetLivingSlot(ActionDefinition* /*action*/,
                                         std::uint8_t* target_result_bytes) {
  return RunRule(battle::IsAttackTargetLiving, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyStrDown (src/BATTLE/BATTLE.PRG/146C.c:10156;
// was _hitPrerequisiteCanApplyStrDown)
int VS::Battle::CanApplyStrengthDownSlot(ActionDefinition* /*action*/,
                                         std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyStrengthDown, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyIntDown (src/BATTLE/BATTLE.PRG/146C.c:10161;
// was _hitPrerequisiteCanApplyIntDown)
int VS::Battle::CanApplyIntelligenceDownSlot(
    ActionDefinition* /*action*/, std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyIntelligenceDown, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyAglDown (src/BATTLE/BATTLE.PRG/146C.c:10166;
// was _hitPrerequisiteCanApplyAglDown)
int VS::Battle::CanApplyAgilityDownSlot(ActionDefinition* /*action*/,
                                        std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyAgilityDown, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyEquipDown (src/BATTLE/BATTLE.PRG/146C.c:10171;
// was _hitPrerequisiteCanApplyEquipDown)
int VS::Battle::CanApplyEquipmentDownSlot(ActionDefinition* /*action*/,
                                          std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyEquipmentDown, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyHalfMove (src/BATTLE/BATTLE.PRG/146C.c:10205;
// was _hitPrerequisiteCanApplyHalfMove)
int VS::Battle::CanApplyHalfMovementSlot(ActionDefinition* /*action*/,
                                         std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyHalfMovement, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyStrUp (src/BATTLE/BATTLE.PRG/146C.c:10210; was
// _hitPrerequisiteCanApplyStrUp)
int VS::Battle::CanApplyStrengthUpSlot(ActionDefinition* /*action*/,
                                       std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyStrengthUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyIntUp (src/BATTLE/BATTLE.PRG/146C.c:10215; was
// _hitPrerequisiteCanApplyIntUp)
int VS::Battle::CanApplyIntelligenceUpSlot(ActionDefinition* /*action*/,
                                           std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyIntelligenceUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyAglUp (src/BATTLE/BATTLE.PRG/146C.c:10220; was
// _hitPrerequisiteCanApplyAglUp)
int VS::Battle::CanApplyAgilityUpSlot(ActionDefinition* /*action*/,
                                      std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyAgilityUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyEquipUp (src/BATTLE/BATTLE.PRG/146C.c:10225;
// was _hitPrerequisiteCanApplyEquipUp)
int VS::Battle::CanApplyEquipmentUpSlot(ActionDefinition* /*action*/,
                                        std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyEquipmentUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyQuicken (src/BATTLE/BATTLE.PRG/146C.c:10259;
// was _hitPrerequisiteCanApplyQuicken)
int VS::Battle::CanApplyQuickenSlot(ActionDefinition* /*action*/,
                                    std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyQuicken, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplySilence (src/BATTLE/BATTLE.PRG/146C.c:10264;
// was _hitPrerequisiteCanApplySilence)
int VS::Battle::CanApplySilenceSlot(ActionDefinition* /*action*/,
                                    std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplySilence, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyMagicWard (src/BATTLE/BATTLE.PRG/146C.c:10269;
// was _hitPrerequisiteCanApplyMagicWard)
int VS::Battle::CanApplyMagicWardSlot(ActionDefinition* /*action*/,
                                      std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyMagicWard, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyRegen (src/BATTLE/BATTLE.PRG/146C.c:10274; was
// _hitPrerequisiteCanApplyRegen)
int VS::Battle::CanApplyRegenerationSlot(ActionDefinition* /*action*/,
                                         std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyRegeneration, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyParalysis (src/BATTLE/BATTLE.PRG/146C.c:10279;
// was _hitPrerequisiteCanApplyParalysis)
int VS::Battle::CanApplyParalysisSlot(ActionDefinition* /*action*/,
                                      std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyParalysis, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyPoison (src/BATTLE/BATTLE.PRG/146C.c:10284;
// was _hitPrerequisiteCanApplyPoison)
int VS::Battle::CanApplyPoisonSlot(ActionDefinition* /*action*/,
                                   std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyPoison, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyNumbness (src/BATTLE/BATTLE.PRG/146C.c:10289;
// was _hitPrerequisiteCanApplyNumbness)
int VS::Battle::CanApplyNumbnessSlot(ActionDefinition* /*action*/,
                                     std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyNumbness, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyCurse (src/BATTLE/BATTLE.PRG/146C.c:10294; was
// _hitPrerequisiteCanApplyCurse)
int VS::Battle::CanApplyCurseSlot(ActionDefinition* /*action*/,
                                  std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyCurse, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyAirAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10299; was _hitPrerequisiteCanApplyAirAttackUp)
int VS::Battle::CanApplyAirAttackUpSlot(ActionDefinition* /*action*/,
                                        std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyAirAttackUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyFireAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10308; was
// _hitPrerequisiteCanApplyFireAttackUp)
int VS::Battle::CanApplyFireAttackUpSlot(ActionDefinition* /*action*/,
                                         std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyFireAttackUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyEarthAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10317; was
// _hitPrerequisiteCanApplyEarthAttackUp)
int VS::Battle::CanApplyEarthAttackUpSlot(ActionDefinition* /*action*/,
                                          std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyEarthAttackUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyWaterAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10326; was
// _hitPrerequisiteCanApplyWaterAttackUp)
int VS::Battle::CanApplyWaterAttackUpSlot(ActionDefinition* /*action*/,
                                          std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyWaterAttackUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyAirDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10335; was
// _hitPrerequisiteCanApplyAirDefenseUp)
int VS::Battle::CanApplyAirDefenseUpSlot(ActionDefinition* /*action*/,
                                         std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyAirDefenseUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyFireDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10359; was
// _hitPrerequisiteCanApplyFireDefenseUp)
int VS::Battle::CanApplyFireDefenseUpSlot(ActionDefinition* /*action*/,
                                          std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyFireDefenseUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyEarthDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10382; was
// _hitPrerequisiteCanApplyEarthDefenseUp)
int VS::Battle::CanApplyEarthDefenseUpSlot(ActionDefinition* /*action*/,
                                           std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyEarthDefenseUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyWaterDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10405; was
// _hitPrerequisiteCanApplyWaterDefenseUp)
int VS::Battle::CanApplyWaterDefenseUpSlot(ActionDefinition* /*action*/,
                                           std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyWaterDefenseUp, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyBanish (src/BATTLE/BATTLE.PRG/146C.c:10429;
// was _hitPrerequisiteCanApplyBanish)
int VS::Battle::CanApplyBanishSlot(ActionDefinition* /*action*/,
                                   std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyBanish, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyExorcise (src/BATTLE/BATTLE.PRG/146C.c:10434;
// was _hitPrerequisiteCanApplyExorcise)
int VS::Battle::CanApplyExorciseSlot(ActionDefinition* /*action*/,
                                     std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyExorcise, target_result_bytes);
}

// decomp: hitPrerequisiteCanApplyDrain (src/BATTLE/BATTLE.PRG/146C.c:10443; was
// _hitPrerequisiteCanApplyDrain)
int VS::Battle::CanApplyDrainSlot(ActionDefinition* /*action*/,
                                  std::uint8_t* target_result_bytes) {
  return RunRule(battle::CanApplyDrain, target_result_bytes);
}

// decomp: hitPrerequisiteUnused0..2 (src/BATTLE/BATTLE.PRG/146C.c:10452; was
// _hitPrerequisiteUnused0, 10519, 10521)
int VS::Battle::IgnoreAttackPrerequisiteSlot(
    ActionDefinition* /*action*/, std::uint8_t* target_result_bytes) {
  return RunRule(battle::IgnoreAttackPrerequisite, target_result_bytes);
}
