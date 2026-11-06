// Hand-maintained (tools/port/owned.csv).
#include "Battle/AttackPrerequisites.hpp"  // NOLINT(build/include): typed API

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Battle {
namespace {
// Battle_statusEffectParams indices of the effects the rules check.
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
  kBanish = 29,
  kSpiritDrain = 30,
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

// decomp: _attackPrerequisiteFunctions (src/BATTLE/BATTLE.PRG/146C.c:628),
// in the original slot order.
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

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEffect
// (src/BATTLE/BATTLE.PRG/146C.c:10133)
bool IsStatusEffectBlocked(int effect, const PrerequisiteTarget& target) {
  const StatusEffectRule rule = target.EffectRule(effect);
  return (rule.status_flags & target.StatusImmunities()) != 0 ||
         (rule.blocking_statuses & target.Statuses()) != 0;
}

// decomp: BATTLE.PRG _getEnemyClass (src/BATTLE/BATTLE.PRG/146C.c:10144)
int TargetEnemyClass(const PrerequisiteTarget& target) {
  return target.EnemyClass();
}

// decomp: BATTLE.PRG _hitPrerequisiteNever0..3
// (src/BATTLE/BATTLE.PRG/146C.c:10146, 10180, 10186, 10192)
bool RejectAttackPrerequisite(const PrerequisiteTarget& /*target*/) {
  return false;
}

// decomp: BATTLE.PRG _hitPrerequisiteAlways0..2
// (src/BATTLE/BATTLE.PRG/146C.c:10152, 10158, 10164)
bool AcceptAttackPrerequisite(const PrerequisiteTarget& /*target*/) {
  return true;
}

// decomp: BATTLE.PRG _hitPrerequisiteUndead
// (src/BATTLE/BATTLE.PRG/146C.c:10170)
bool IsAttackTargetUndead(const PrerequisiteTarget& target) {
  return target.Valid() && IsUndead(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteNotUndead
// (src/BATTLE/BATTLE.PRG/146C.c:10175)
bool IsAttackTargetLiving(const PrerequisiteTarget& target) {
  return target.Valid() && !IsUndead(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyStrDown
// (src/BATTLE/BATTLE.PRG/146C.c:10198)
bool CanApplyStrengthDown(const PrerequisiteTarget& target) {
  return CanApply(Effect::kStrengthDown, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyIntDown
// (src/BATTLE/BATTLE.PRG/146C.c:10204)
bool CanApplyIntelligenceDown(const PrerequisiteTarget& target) {
  return CanApply(Effect::kIntelligenceDown, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyAglDown
// (src/BATTLE/BATTLE.PRG/146C.c:10210)
bool CanApplyAgilityDown(const PrerequisiteTarget& target) {
  return CanApply(Effect::kAgilityDown, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEquipDown
// (src/BATTLE/BATTLE.PRG/146C.c:10216): armour, a shield or a blade.
bool CanApplyEquipmentDown(const PrerequisiteTarget& target) {
  return CanApply(Effect::kEquipmentDown, target) && HasAnyEquipment(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyHalfMove
// (src/BATTLE/BATTLE.PRG/146C.c:10250)
bool CanApplyHalfMovement(const PrerequisiteTarget& target) {
  return CanApply(Effect::kHalfMovement, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyStrUp
// (src/BATTLE/BATTLE.PRG/146C.c:10256)
bool CanApplyStrengthUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kStrengthUp, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyIntUp
// (src/BATTLE/BATTLE.PRG/146C.c:10261)
bool CanApplyIntelligenceUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kIntelligenceUp, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyAglUp
// (src/BATTLE/BATTLE.PRG/146C.c:10266)
bool CanApplyAgilityUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kAgilityUp, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEquipUp
// (src/BATTLE/BATTLE.PRG/146C.c:10271): armour, a shield or a blade.
bool CanApplyEquipmentUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kEquipmentUp, target) && HasAnyEquipment(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyQuicken
// (src/BATTLE/BATTLE.PRG/146C.c:10305)
bool CanApplyQuicken(const PrerequisiteTarget& target) {
  return CanApply(Effect::kQuicken, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplySilence
// (src/BATTLE/BATTLE.PRG/146C.c:10311)
bool CanApplySilence(const PrerequisiteTarget& target) {
  return CanApply(Effect::kSilence, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyMagicWard
// (src/BATTLE/BATTLE.PRG/146C.c:10317)
bool CanApplyMagicWard(const PrerequisiteTarget& target) {
  return CanApply(Effect::kMagicWard, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyRegen
// (src/BATTLE/BATTLE.PRG/146C.c:10323)
bool CanApplyRegeneration(const PrerequisiteTarget& target) {
  return CanApply(Effect::kRegeneration, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyParalysis
// (src/BATTLE/BATTLE.PRG/146C.c:10329)
bool CanApplyParalysis(const PrerequisiteTarget& target) {
  return CanApply(Effect::kParalysis, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyPoison
// (src/BATTLE/BATTLE.PRG/146C.c:10335)
bool CanApplyPoison(const PrerequisiteTarget& target) {
  return CanApply(Effect::kPoison, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyNumbness
// (src/BATTLE/BATTLE.PRG/146C.c:10341)
bool CanApplyNumbness(const PrerequisiteTarget& target) {
  return CanApply(Effect::kNumbness, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyCurse
// (src/BATTLE/BATTLE.PRG/146C.c:10347)
bool CanApplyCurse(const PrerequisiteTarget& target) {
  return CanApply(Effect::kCurse, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyAirAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10353): needs a blade.
bool CanApplyAirAttackUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kAirAttackUp, target) && target.BladeId() != 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyFireAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10363): needs a blade.
bool CanApplyFireAttackUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kFireAttackUp, target) && target.BladeId() != 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEarthAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10373): needs a blade.
bool CanApplyEarthAttackUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kEarthAttackUp, target) && target.BladeId() != 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyWaterAttackUp
// (src/BATTLE/BATTLE.PRG/146C.c:10383): needs a blade.
bool CanApplyWaterAttackUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kWaterAttackUp, target) && target.BladeId() != 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyAirDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10393): needs armour or a shield.
bool CanApplyAirDefenseUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kAirDefenseUp, target) && HasDefensiveGear(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyFireDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10418): needs armour or a shield.
bool CanApplyFireDefenseUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kFireDefenseUp, target) && HasDefensiveGear(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEarthDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10442): needs armour or a shield.
bool CanApplyEarthDefenseUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kEarthDefenseUp, target) && HasDefensiveGear(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyWaterDefenseUp
// (src/BATTLE/BATTLE.PRG/146C.c:10466): needs armour or a shield.
bool CanApplyWaterDefenseUp(const PrerequisiteTarget& target) {
  return CanApply(Effect::kWaterDefenseUp, target) && HasDefensiveGear(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyBanish
// (src/BATTLE/BATTLE.PRG/146C.c:10491)
bool CanApplyBanish(const PrerequisiteTarget& target) {
  return CanApply(Effect::kBanish, target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyExorcise
// (src/BATTLE/BATTLE.PRG/146C.c:10497): undead targets only.
bool CanApplyExorcise(const PrerequisiteTarget& target) {
  return CanApply(Effect::kSpiritDrain, target) && IsUndead(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyDrain
// (src/BATTLE/BATTLE.PRG/146C.c:10507): living targets only.
bool CanApplyDrain(const PrerequisiteTarget& target) {
  return CanApply(Effect::kSpiritDrain, target) && !IsUndead(target);
}

// decomp: BATTLE.PRG _hitPrerequisiteUnused0..2
// (src/BATTLE/BATTLE.PRG/146C.c:10517, 10519, 10521). The original body is
// `jr ra` alone, so it returns the dispatcher's v0: the slot's own non-zero
// address (_canPerformAttack loads it into v0 for `jalr v0`). The hit passes.
bool IgnoreAttackPrerequisite(const PrerequisiteTarget& /*target*/) {
  return true;
}

PrerequisiteRule AttackPrerequisiteRule(int rule) {
  if (rule < 0 || static_cast<std::size_t>(rule) >= kRules.size()) {
    return nullptr;
  }
  return kRules[static_cast<std::size_t>(rule)];
}

// decomp: BATTLE.PRG _canPerformAttack (src/BATTLE/BATTLE.PRG/146C.c:10523)
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
