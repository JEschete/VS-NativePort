#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

// Typed attack prerequisites (#276 row 31). Before a hit lands, the hit's
// prerequisite rule decides from the target alone whether the effect can apply:
// a status the target is immune to or already carries blocks it, some effects
// need equipment or an undead target, and a few rules always pass or fail.
// The rules read the target through a PrerequisiteTarget, so they hold no
// actor records or tables.
namespace VS::Battle {
// The prerequisite rules an action hit can name.
inline constexpr std::size_t kPrerequisiteCount = 40;
// The armoured limbs an actor has.
inline constexpr int kArmorLimbCount = 6;
// The enemy class of undead targets.
inline constexpr int kUndeadEnemyClass = 2;

// The status-effect parameters a rule checks: the status bits the effect sets
// and the statuses that block it.
struct StatusEffectRule {
  std::uint32_t status_flags = 0;
  std::uint32_t blocking_statuses = 0;
};

// The target of a hit, as the rules read it.
class PrerequisiteTarget {
 public:
  virtual ~PrerequisiteTarget() = default;
  // False when the target is not an actor; a rule that reads it then fails.
  [[nodiscard]] virtual bool Valid() const = 0;
  // The parameters of a status effect (Battle_statusEffectParams).
  [[nodiscard]] virtual StatusEffectRule EffectRule(int effect) const = 0;
  [[nodiscard]] virtual std::uint32_t StatusImmunities() const = 0;
  [[nodiscard]] virtual std::uint32_t Statuses() const = 0;
  [[nodiscard]] virtual int EnemyClass() const = 0;
  // The armour id on a limb, 0 when the limb is bare.
  [[nodiscard]] virtual int LimbArmorId(int limb) const = 0;
  [[nodiscard]] virtual int ShieldId() const = 0;
  [[nodiscard]] virtual int BladeId() const = 0;
};

// What decides one hit's prerequisite: the hit within the action, its rule
// and the impact kind already recorded for the target.
struct HitPrerequisiteRequest {
  int hit_number = 0;
  int rule = 0;
  int impact_kind = 0;
};

// One prerequisite rule.
using PrerequisiteRule = bool (*)(const PrerequisiteTarget&);

[[nodiscard]] bool IsStatusEffectBlocked(int effect,
                                         const PrerequisiteTarget& target);
[[nodiscard]] int TargetEnemyClass(const PrerequisiteTarget& target);

[[nodiscard]] bool RejectAttackPrerequisite(const PrerequisiteTarget& target);
[[nodiscard]] bool AcceptAttackPrerequisite(const PrerequisiteTarget& target);
[[nodiscard]] bool IsAttackTargetUndead(const PrerequisiteTarget& target);
[[nodiscard]] bool IsAttackTargetLiving(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyStrengthDown(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyIntelligenceDown(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyAgilityDown(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyEquipmentDown(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyHalfMovement(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyStrengthUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyIntelligenceUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyAgilityUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyEquipmentUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyQuicken(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplySilence(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyMagicWard(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyRegeneration(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyParalysis(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyPoison(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyNumbness(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyCurse(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyAirAttackUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyFireAttackUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyEarthAttackUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyWaterAttackUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyAirDefenseUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyFireDefenseUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyEarthDefenseUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyWaterDefenseUp(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyBanish(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyExorcise(const PrerequisiteTarget& target);
[[nodiscard]] bool CanApplyDrain(const PrerequisiteTarget& target);
// The three unused table slots: they pass.
[[nodiscard]] bool IgnoreAttackPrerequisite(const PrerequisiteTarget& target);

// The rule a table slot holds; nullptr for a slot past the table.
[[nodiscard]] PrerequisiteRule AttackPrerequisiteRule(int rule);

// Whether the hit's prerequisite lets it apply. A follow-up hit with rule 1
// never applies to a target whose impact kind is 4; an unknown rule fails.
[[nodiscard]] bool CheckActionHitPrerequisite(
    const HitPrerequisiteRequest& request, const PrerequisiteTarget& target);
}  // namespace VS::Battle
