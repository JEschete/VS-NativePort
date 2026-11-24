// Hand-maintained (tools/port/owned.csv).
#include "Battle/AttackPrerequisites.hpp"  // NOLINT(build/include): typed API

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Battle {
namespace {
// Explicit legacy action-to-catalog rule mapping.
enum class Effect : std::uint8_t {
  kHalfMovement = 4,
  kStrengthDown = 5,
  kStrengthUp = 6,
  kIntelligenceDown = 7,
  kIntelligenceUp = 8,
  kAgilityDown = 9,
  kAgilityUp = 10,
  kQuicken = 11,
  kSilence = 12,
  kParalysis = 13,
  kPoison = 14,
  kNumbness = 15,
  kCurse = 16,
  kRegeneration = 17,
  kMagicWard = 18,
  kEquipmentDown = 19,
  kEquipmentUp = 20,
  kAirAttackUp = 21,
  kFireAttackUp = 22,
  kEarthAttackUp = 23,
  kWaterAttackUp = 24,
  kAirDefenseUp = 25,
  kFireDefenseUp = 26,
  kEarthDefenseUp = 27,
  kWaterDefenseUp = 28,
  kAnalyze = 29,
  kInstantDefeat = 30,
};

// A follow-up hit with this rule skips targets with this impact kind.
constexpr int kFollowUpRule = 1;
constexpr int kFollowUpSkippedImpact = 4;

bool CanApply(Effect effect, const PrerequisiteTarget& target) {
  return target.Valid() &&
         !IsStatusEffectBlocked(static_cast<int>(effect), target);
}

bool HasArmor(const PrerequisiteTarget& target) {
  for (int limb = 0; limb < kArmorLimbCount; ++limb) {
    if (target.LimbArmorId(limb) != 0) return true;
  }
  return false;
}

bool HasDefensiveGear(const PrerequisiteTarget& target) {
  return HasArmor(target) || target.ShieldId() != 0;
}

bool HasAnyEquipment(const PrerequisiteTarget& target) {
  return HasDefensiveGear(target) || target.BladeId() != 0;
}

bool IsUndead(const PrerequisiteTarget& target) {
  return TargetEnemyClass(target) == kUndeadEnemyClass;
}

// decomp: attackPrerequisiteFunctions (src/BATTLE/BATTLE.PRG/146C.c:301; was
// _attackPrerequisiteFunctions), in the original slot order.
constexpr std::array<PrerequisiteRule, kPrerequisiteCount> kRules{
    RejectAttackPrerequisite,
    AcceptAttackPrerequisite,
    AcceptAttackPrerequisite,
    AcceptAttackPrerequisite,
    IsAttackTargetUndead,
    IsAttackTargetLiving,
    RejectAttackPrerequisite,
    RejectAttackPrerequisite,
    RejectAttackPrerequisite,
    CanApplyStrengthDown,
    CanApplyIntelligenceDown,
    CanApplyAgilityDown,
    CanApplyEquipmentDown,
    CanApplyHalfMovement,
    CanApplyStrengthUp,
    CanApplyIntelligenceUp,
    CanApplyAgilityUp,
    CanApplyEquipmentUp,
    CanApplyQuicken,
    CanApplySilence,
    CanApplyMagicWard,
    CanApplyRegeneration,
    CanApplyParalysis,
    CanApplyPoison,
    CanApplyNumbness,
    CanApplyCurse,
    CanApplyAirAttackUp,
    CanApplyFireAttackUp,
    CanApplyEarthAttackUp,
    CanApplyWaterAttackUp,
    CanApplyAirDefenseUp,
    CanApplyFireDefenseUp,
    CanApplyEarthDefenseUp,
    CanApplyWaterDefenseUp,
    CanApplyBanish,
    CanApplyExorcise,
    CanApplyDrain,
    IgnoreAttackPrerequisite,
    IgnoreAttackPrerequisite,
    IgnoreAttackPrerequisite,
};
}  // namespace

// decomp: BATTLE.PRG hitPrerequisiteCanApplyEffect
// (src/BATTLE/BATTLE.PRG/146C.c:10118; was _hitPrerequisiteCanApplyEffect)
bool IsStatusEffectBlocked(int effect, const PrerequisiteTarget& target) {
  const StatusEffectRule rule = target.EffectRule(effect);
  return !rule.available ||
         (rule.status_flags & target.StatusImmunities()) != 0 ||
         (rule.blocking_statuses & target.Statuses()) != 0;
}

// decomp: BATTLE.PRG getEnemyClass (src/BATTLE/BATTLE.PRG/146C.c:10130; was
// _getEnemyClass)
int TargetEnemyClass(const PrerequisiteTarget& target) {
  return target.EnemyClass();
}

// decomp: BATTLE.PRG hitPrerequisiteNever0..3
// (src/BATTLE/BATTLE.PRG/146C.c:10132; was _hitPrerequisiteNever0, 10180,
// 10186, 10192)
bool RejectAttackPrerequisite(const PrerequisiteTarget& /*target*/) {
  return false;
}

// decomp: BATTLE.PRG hitPrerequisiteAlways0..2
// (src/BATTLE/BATTLE.PRG/146C.c:10134; was _hitPrerequisiteAlways0, 10158,
// 10164)
bool AcceptAttackPrerequisite(const PrerequisiteTarget& /*target*/) {
  return true;
}

// decomp: BATTLE.PRG hitPrerequisiteUndead
// (src/BATTLE/BATTLE.PRG/146C.c:10140; was _hitPrerequisiteUndead)
bool IsAttackTargetUndead(const PrerequisiteTarget& target) {
  return target.Valid() && IsUndead(target);
}

// decomp: BATTLE.PRG hitPrerequisiteNotUndead
// (src/BATTLE/BATTLE.PRG/146C.c:10145; was _hitPrerequisiteNotUndead)
bool IsAttackTargetLiving(const PrerequisiteTarget& target) {
  return target.Valid() && !IsUndead(target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyStrDown
// (src/BATTLE/BATTLE.PRG/146C.c:10156; was _hitPrerequisiteCanApplyStrDown)
bool CanApplyStrengthDown(const PrerequisiteTarget& target) {
  return CanApply(Effect::kStrengthDown, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyIntDown
// (src/BATTLE/BATTLE.PRG/146C.c:10161; was _hitPrerequisiteCanApplyIntDown)
bool CanApplyIntelligenceDown(const PrerequisiteTarget& target) {
  return CanApply(Effect::kIntelligenceDown, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyAglDown
// (src/BATTLE/BATTLE.PRG/146C.c:10166; was _hitPrerequisiteCanApplyAglDown)
bool CanApplyAgilityDown(const PrerequisiteTarget& target) {
  return CanApply(Effect::kAgilityDown, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyEquipDown
// (src/BATTLE/BATTLE.PRG/146C.c:10171; was _hitPrerequisiteCanApplyEquipDown):
// armour, a shield or a blade.
bool CanApplyEquipmentDown(const PrerequisiteTarget& target) {
  return CanApply(Effect::kEquipmentDown, target) && HasAnyEquipment(target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyHalfMove
// (src/BATTLE/BATTLE.PRG/146C.c:10205; was _hitPrerequisiteCanApplyHalfMove)
bool CanApplyHalfMovement(const PrerequisiteTarget& target) {
  return CanApply(Effect::kHalfMovement, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyStrUp
// (src/BATTLE/BATTLE.PRG/146C.c:10210; was _hitPrerequisiteCanApplyStrUp)
bool CanApplyStrengthUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kStrengthUp, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyIntUp
// (src/BATTLE/BATTLE.PRG/146C.c:10215; was _hitPrerequisiteCanApplyIntUp)
bool CanApplyIntelligenceUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kIntelligenceUp, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyAglUp
// (src/BATTLE/BATTLE.PRG/146C.c:10220; was _hitPrerequisiteCanApplyAglUp)
bool CanApplyAgilityUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kAgilityUp, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyEquipUp
// (src/BATTLE/BATTLE.PRG/146C.c:10225; was _hitPrerequisiteCanApplyEquipUp):
// armour, a shield or a blade.
bool CanApplyEquipmentUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kEquipmentUp, target) && HasAnyEquipment(target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyQuicken
// (src/BATTLE/BATTLE.PRG/146C.c:10259; was _hitPrerequisiteCanApplyQuicken)
bool CanApplyQuicken(const PrerequisiteTarget& target) {
  return CanApply(Effect::kQuicken, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplySilence
// (src/BATTLE/BATTLE.PRG/146C.c:10264; was _hitPrerequisiteCanApplySilence)
bool CanApplySilence(const PrerequisiteTarget& target) {
  return CanApply(Effect::kSilence, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyMagicWard
// (src/BATTLE/BATTLE.PRG/146C.c:10269; was _hitPrerequisiteCanApplyMagicWard)
bool CanApplyMagicWard(const PrerequisiteTarget& target) {
  return CanApply(Effect::kMagicWard, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyRegen
// (src/BATTLE/BATTLE.PRG/146C.c:10274; was _hitPrerequisiteCanApplyRegen)
bool CanApplyRegeneration(const PrerequisiteTarget& target) {
  return CanApply(Effect::kRegeneration, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyParalysis
// (src/BATTLE/BATTLE.PRG/146C.c:10279; was _hitPrerequisiteCanApplyParalysis)
bool CanApplyParalysis(const PrerequisiteTarget& target) {
  return CanApply(Effect::kParalysis, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyPoison
// (src/BATTLE/BATTLE.PRG/146C.c:10284; was _hitPrerequisiteCanApplyPoison)
bool CanApplyPoison(const PrerequisiteTarget& target) {
  return CanApply(Effect::kPoison, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyNumbness
// (src/BATTLE/BATTLE.PRG/146C.c:10289; was _hitPrerequisiteCanApplyNumbness)
bool CanApplyNumbness(const PrerequisiteTarget& target) {
  return CanApply(Effect::kNumbness, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyCurse
// (src/BATTLE/BATTLE.PRG/146C.c:10294; was _hitPrerequisiteCanApplyCurse)
bool CanApplyCurse(const PrerequisiteTarget& target) {
  return CanApply(Effect::kCurse, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyAirAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10299; was
// _hitPrerequisiteCanApplyAirAttackUp): needs a blade.
bool CanApplyAirAttackUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kAirAttackUp, target) && target.BladeId() != 0;
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyFireAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10308; was
// _hitPrerequisiteCanApplyFireAttackUp): needs a blade.
bool CanApplyFireAttackUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kFireAttackUp, target) && target.BladeId() != 0;
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyEarthAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10317; was
// _hitPrerequisiteCanApplyEarthAttackUp): needs a blade.
bool CanApplyEarthAttackUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kEarthAttackUp, target) && target.BladeId() != 0;
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyWaterAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10326; was
// _hitPrerequisiteCanApplyWaterAttackUp): needs a blade.
bool CanApplyWaterAttackUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kWaterAttackUp, target) && target.BladeId() != 0;
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyAirDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10335; was
// _hitPrerequisiteCanApplyAirDefenseUp): needs armour or a shield.
bool CanApplyAirDefenseUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kAirDefenseUp, target) && HasDefensiveGear(target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyFireDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10359; was
// _hitPrerequisiteCanApplyFireDefenseUp): needs armour or a shield.
bool CanApplyFireDefenseUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kFireDefenseUp, target) && HasDefensiveGear(target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyEarthDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10382; was
// _hitPrerequisiteCanApplyEarthDefenseUp): needs armour or a shield.
bool CanApplyEarthDefenseUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kEarthDefenseUp, target) && HasDefensiveGear(target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyWaterDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10405; was
// _hitPrerequisiteCanApplyWaterDefenseUp): needs armour or a shield.
bool CanApplyWaterDefenseUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kWaterDefenseUp, target) && HasDefensiveGear(target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyBanish
// (src/BATTLE/BATTLE.PRG/146C.c:10429; was _hitPrerequisiteCanApplyBanish)
bool CanApplyBanish(const PrerequisiteTarget& target) {
  return CanApply(Effect::kAnalyze, target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyExorcise
// (src/BATTLE/BATTLE.PRG/146C.c:10434; was _hitPrerequisiteCanApplyExorcise):
// undead targets only.
bool CanApplyExorcise(const PrerequisiteTarget& target) {
  return CanApply(Effect::kInstantDefeat, target) && IsUndead(target);
}

// decomp: BATTLE.PRG hitPrerequisiteCanApplyDrain
// (src/BATTLE/BATTLE.PRG/146C.c:10443; was _hitPrerequisiteCanApplyDrain):
// living targets only.
bool CanApplyDrain(const PrerequisiteTarget& target) {
  return CanApply(Effect::kInstantDefeat, target) && !IsUndead(target);
}

// decomp: BATTLE.PRG hitPrerequisiteUnused0..2
// (src/BATTLE/BATTLE.PRG/146C.c:10452; was _hitPrerequisiteUnused0, 10519,
// 10521). The original body is `jr ra` alone, so it returns the dispatcher's
// v0: the slot's own non-zero address (_canPerformAttack loads it into v0 for
// `jalr v0`). The hit passes.
bool IgnoreAttackPrerequisite(const PrerequisiteTarget& /*target*/) {
  return true;
}

PrerequisiteRule AttackPrerequisiteRule(int rule) {
  if (rule < 0 || static_cast<std::size_t>(rule) >= kRules.size()) {
    return nullptr;
  }
  return kRules[static_cast<std::size_t>(rule)];
}

// decomp: BATTLE.PRG canPerformAttack (src/BATTLE/BATTLE.PRG/146C.c:10458; was
// _canPerformAttack)
bool CheckActionHitPrerequisite(const HitPrerequisiteRequest& request,
                                const PrerequisiteTarget& target) {
  if (request.hit_number != 0 && request.rule == kFollowUpRule &&
      request.impact_kind == kFollowUpSkippedImpact) {
    return false;
  }
  const PrerequisiteRule rule = AttackPrerequisiteRule(request.rule);
  return rule != nullptr && rule(target);
}
}  // namespace VS::Battle
