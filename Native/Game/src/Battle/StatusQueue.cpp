// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatusQueue.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Diagnostics.hpp"

namespace VS::Battle {
namespace {
using StatusOperation = std::uint32_t (*)(const StatusRule&,
                                          const StatusTarget&, QueuedStatuses&);

std::optional<std::uint32_t> QueueRule(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued, StatusRuleId id,
    StatusOperation operation = QueueApplicableStatusChanges) {
  const auto index = static_cast<std::size_t>(id);
  if (index >= rules.size()) {
    VS::Core::Debugf("battle", "Status queue rule %zu is outside its table",
                     index);
    return std::nullopt;
  }
  return operation(rules[index], target, queued);
}

std::optional<StatusFeedback> QueueStatRule(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued, StatusRuleId id, StatusFeedback feedback,
    StatusOperation operation = QueueApplicableStatusChanges) {
  const auto changed = QueueRule(rules, target, queued, id, operation);
  if (!changed) return std::nullopt;
  if (changed.value() == 0) {
    return StatusFeedback::kNone;
  }
  queued.feedback = feedback;
  return feedback;
}
}  // namespace

// decomp: applyStatus (src/BATTLE/BATTLE.PRG/146C.c:11129; was
// _setApplicableStatuses).
std::uint32_t QueueApplicableStatusChanges(const StatusRule& rule,
                                           const StatusTarget& target,
                                           QueuedStatuses& queued) {
  if (target.exempt || (target.active & rule.blocking) != 0) return 0;
  const auto added =
      rule.statuses & ~target.immunities & (~target.active | rule.cancelled);
  queued.added |= added;
  const auto removed = rule.cancelled & target.active;
  queued.removed |= removed;
  return added | removed;
}

// decomp: clearStatus (src/BATTLE/BATTLE.PRG/146C.c:11151; was
// _setInflictedStatuses).
std::uint32_t QueueStatusRemovalMask(const StatusRule& rule,
                                     const StatusTarget& target,
                                     QueuedStatuses& queued) {
  if (target.exempt) return 0;
  const auto removed = rule.statuses & target.active;
  queued.removed |= removed;
  return removed;
}

// decomp: func_80083998 (src/BATTLE/BATTLE.PRG/146C.c:12185).
std::optional<StatusFeedback> QueueStrengthDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kStrengthDown,
                       StatusFeedback::kNegative);
}

// decomp: func_800839E8 (src/BATTLE/BATTLE.PRG/146C.c:12193).
std::optional<StatusFeedback> QueueIntelligenceDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kIntelligenceDown,
                       StatusFeedback::kNegative);
}

// decomp: func_80083A38 (src/BATTLE/BATTLE.PRG/146C.c:12201).
std::optional<StatusFeedback> QueueAgilityDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kAgilityDown,
                       StatusFeedback::kNegative);
}

// decomp: func_80083A88 (src/BATTLE/BATTLE.PRG/146C.c:12209).
std::optional<StatusFeedback> QueueEquipmentDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kEquipmentDown,
                       StatusFeedback::kNegative);
}

// decomp: func_80083AD8 (src/BATTLE/BATTLE.PRG/146C.c:12217).
std::optional<StatusFeedback> QueueHalfMovement(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kHalfMovement,
                       StatusFeedback::kNegative);
}

// decomp: func_80083B28 (src/BATTLE/BATTLE.PRG/146C.c:12225).
std::optional<StatusFeedback> QueueStrengthUp(std::span<const StatusRule> rules,
                                              const StatusTarget& target,
                                              QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kStrengthUp,
                       StatusFeedback::kPositive);
}

// decomp: func_80083B78 (src/BATTLE/BATTLE.PRG/146C.c:12233).
std::optional<StatusFeedback> QueueIntelligenceUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kIntelligenceUp,
                       StatusFeedback::kPositive);
}

// decomp: func_80083BC8 (src/BATTLE/BATTLE.PRG/146C.c:12241).
std::optional<StatusFeedback> QueueAgilityUp(std::span<const StatusRule> rules,
                                             const StatusTarget& target,
                                             QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kAgilityUp,
                       StatusFeedback::kPositive);
}

// decomp: func_80083C18 (src/BATTLE/BATTLE.PRG/146C.c:12249).
std::optional<StatusFeedback> QueueEquipmentUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kEquipmentUp,
                       StatusFeedback::kPositive);
}

// decomp: func_80083C68 (src/BATTLE/BATTLE.PRG/146C.c:12257).
std::optional<StatusFeedback> QueueQuicken(std::span<const StatusRule> rules,
                                           const StatusTarget& target,
                                           QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kQuicken,
                       StatusFeedback::kPositive);
}

// decomp: func_80083CB8 (src/BATTLE/BATTLE.PRG/146C.c:12265).
std::optional<StatusFeedback> QueueSilence(std::span<const StatusRule> rules,
                                           const StatusTarget& target,
                                           QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kSilence,
                       StatusFeedback::kNegative);
}

// decomp: func_80083D08 (src/BATTLE/BATTLE.PRG/146C.c:12273).
std::optional<std::uint32_t> QueueMagicWard(std::span<const StatusRule> rules,
                                            const StatusTarget& target,
                                            QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kMagicWard);
}

// decomp: func_80083D2C (src/BATTLE/BATTLE.PRG/146C.c:12279).
std::optional<StatusFeedback> QueueRegeneration(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kRegeneration,
                       StatusFeedback::kPositive);
}

// decomp: func_80083D7C (src/BATTLE/BATTLE.PRG/146C.c:12287).
std::optional<BanishOutcome> QueueBanish(std::span<const StatusRule> rules,
                                         const StatusTarget& target,
                                         QueuedStatuses& queued,
                                         bool execute_action) {
  const auto changed = QueueRule(rules, target, queued, StatusRuleId::kBanish);
  if (!changed) return std::nullopt;
  return BanishOutcome{.changed_mask = changed.value(),
                       .analyze_requested = execute_action};
}
// decomp: func_80083DEC (src/BATTLE/BATTLE.PRG/146C.c:12296).
std::optional<StatusFeedback> QueueParalysis(std::span<const StatusRule> rules,
                                             const StatusTarget& target,
                                             QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kParalysis,
                       StatusFeedback::kNegative);
}

// decomp: func_80083E3C (src/BATTLE/BATTLE.PRG/146C.c:12304).
std::optional<StatusFeedback> QueuePoison(std::span<const StatusRule> rules,
                                          const StatusTarget& target,
                                          QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kPoison,
                       StatusFeedback::kNegative);
}

// decomp: func_80083E8C (src/BATTLE/BATTLE.PRG/146C.c:12312).
std::optional<StatusFeedback> QueueNumbness(std::span<const StatusRule> rules,
                                            const StatusTarget& target,
                                            QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kNumbness,
                       StatusFeedback::kNegative);
}

// decomp: func_80083EDC (src/BATTLE/BATTLE.PRG/146C.c:12320).
std::optional<StatusFeedback> QueueCurse(std::span<const StatusRule> rules,
                                         const StatusTarget& target,
                                         QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kCurse,
                       StatusFeedback::kNegative);
}

// decomp: func_80083F2C (src/BATTLE/BATTLE.PRG/146C.c:12328).
std::optional<std::uint32_t> QueueAirAttackUp(std::span<const StatusRule> rules,
                                              const StatusTarget& target,
                                              QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kAirAttackUp);
}

// decomp: func_80083F50 (src/BATTLE/BATTLE.PRG/146C.c:12334).
std::optional<std::uint32_t> QueueFireAttackUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kFireAttackUp);
}

// decomp: func_80083F74 (src/BATTLE/BATTLE.PRG/146C.c:12340).
std::optional<std::uint32_t> QueueEarthAttackUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kEarthAttackUp);
}

// decomp: func_80083F98 (src/BATTLE/BATTLE.PRG/146C.c:12346).
std::optional<std::uint32_t> QueueWaterAttackUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kWaterAttackUp);
}

// decomp: func_80083FBC (src/BATTLE/BATTLE.PRG/146C.c:12352).
std::optional<std::uint32_t> QueueAirDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kAirDefenseUp);
}

// decomp: func_80083FE0 (src/BATTLE/BATTLE.PRG/146C.c:12358).
std::optional<std::uint32_t> QueueFireDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kFireDefenseUp);
}

// decomp: func_80084004 (src/BATTLE/BATTLE.PRG/146C.c:12364).
std::optional<std::uint32_t> QueueEarthDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kEarthDefenseUp);
}

// decomp: func_80084028 (src/BATTLE/BATTLE.PRG/146C.c:12370).
std::optional<std::uint32_t> QueueWaterDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kWaterDefenseUp);
}

// decomp: func_8008404C (src/BATTLE/BATTLE.PRG/146C.c:12376).
std::optional<StatusFeedback> QueueParalysisCure(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kParalysis,
                       StatusFeedback::kPositive, QueueStatusRemovalMask);
}

// decomp: func_8008409C (src/BATTLE/BATTLE.PRG/146C.c:12384).
std::optional<StatusFeedback> QueuePoisonCure(std::span<const StatusRule> rules,
                                              const StatusTarget& target,
                                              QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kPoison,
                       StatusFeedback::kPositive, QueueStatusRemovalMask);
}

// decomp: func_800840EC (src/BATTLE/BATTLE.PRG/146C.c:12392).
std::optional<StatusFeedback> QueueCurseCure(std::span<const StatusRule> rules,
                                             const StatusTarget& target,
                                             QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kCurse,
                       StatusFeedback::kPositive, QueueStatusRemovalMask);
}

// decomp: func_8008413C (src/BATTLE/BATTLE.PRG/146C.c:12400).
std::optional<StatusFeedback> QueueNumbnessCure(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kNumbness,
                       StatusFeedback::kPositive, QueueStatusRemovalMask);
}
// decomp: func_8008418C (src/BATTLE/BATTLE.PRG/146C.c:12408).
std::uint32_t QueueDispel(std::uint32_t active, QueuedStatuses& queued) {
  constexpr std::uint32_t kDispelStatuses = 0x1FFE1FE0;
  const auto removed = active & kDispelStatuses;
  queued.removed |= removed;
  return removed;
}

// decomp: func_800841C8 (src/BATTLE/BATTLE.PRG/146C.c:12415).
StatusFeedback QueueNegativeStatusCure(std::uint32_t active,
                                       QueuedStatuses& queued) {
  constexpr std::uint32_t kNegativeCureStatuses = 0xE000;
  const auto removed = active & kNegativeCureStatuses;
  queued.removed |= removed;
  if (removed == 0) return StatusFeedback::kNone;
  queued.feedback = StatusFeedback::kPositive;
  return StatusFeedback::kPositive;
}
}  // namespace VS::Battle
