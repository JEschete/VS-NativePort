// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatusQueue.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

extern "C" {
#include "Core/Diagnostics.h"
}

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
    Core_Debugf("battle", "Status queue rule %zu is outside its table", index);
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

// decomp: _setApplicableStatuses (src/BATTLE/BATTLE.PRG/146C.c:11195).
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

// decomp: _setInflictedStatuses (src/BATTLE/BATTLE.PRG/146C.c:11217).
std::uint32_t QueueStatusRemovalMask(const StatusRule& rule,
                                     const StatusTarget& target,
                                     QueuedStatuses& queued) {
  if (target.exempt) return 0;
  const auto removed = rule.statuses & target.active;
  queued.removed |= removed;
  return removed;
}

// decomp: func_80083998 (src/BATTLE/BATTLE.PRG/146C.c:12273).
std::optional<StatusFeedback> QueueStrengthDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kStrengthDown,
                       StatusFeedback::kNegative);
}

// decomp: func_800839E8 (src/BATTLE/BATTLE.PRG/146C.c:12282).
std::optional<StatusFeedback> QueueIntelligenceDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kIntelligenceDown,
                       StatusFeedback::kNegative);
}

// decomp: func_80083A38 (src/BATTLE/BATTLE.PRG/146C.c:12291).
std::optional<StatusFeedback> QueueAgilityDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kAgilityDown,
                       StatusFeedback::kNegative);
}

// decomp: func_80083A88 (src/BATTLE/BATTLE.PRG/146C.c:12300).
std::optional<StatusFeedback> QueueEquipmentDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kEquipmentDown,
                       StatusFeedback::kNegative);
}

// decomp: func_80083AD8 (src/BATTLE/BATTLE.PRG/146C.c:12309).
std::optional<StatusFeedback> QueueHalfMovement(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kHalfMovement,
                       StatusFeedback::kNegative);
}

// decomp: func_80083B28 (src/BATTLE/BATTLE.PRG/146C.c:12318).
std::optional<StatusFeedback> QueueStrengthUp(std::span<const StatusRule> rules,
                                              const StatusTarget& target,
                                              QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kStrengthUp,
                       StatusFeedback::kPositive);
}

// decomp: func_80083B78 (src/BATTLE/BATTLE.PRG/146C.c:12327).
std::optional<StatusFeedback> QueueIntelligenceUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kIntelligenceUp,
                       StatusFeedback::kPositive);
}

// decomp: func_80083BC8 (src/BATTLE/BATTLE.PRG/146C.c:12336).
std::optional<StatusFeedback> QueueAgilityUp(std::span<const StatusRule> rules,
                                             const StatusTarget& target,
                                             QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kAgilityUp,
                       StatusFeedback::kPositive);
}

// decomp: func_80083C18 (src/BATTLE/BATTLE.PRG/146C.c:12345).
std::optional<StatusFeedback> QueueEquipmentUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kEquipmentUp,
                       StatusFeedback::kPositive);
}

// decomp: func_80083C68 (src/BATTLE/BATTLE.PRG/146C.c:12354).
std::optional<StatusFeedback> QueueQuicken(std::span<const StatusRule> rules,
                                           const StatusTarget& target,
                                           QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kQuicken,
                       StatusFeedback::kPositive);
}

// decomp: func_80083CB8 (src/BATTLE/BATTLE.PRG/146C.c:12363).
std::optional<StatusFeedback> QueueSilence(std::span<const StatusRule> rules,
                                           const StatusTarget& target,
                                           QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kSilence,
                       StatusFeedback::kNegative);
}

// decomp: func_80083D08 (src/BATTLE/BATTLE.PRG/146C.c:12372).
std::optional<std::uint32_t> QueueMagicWard(std::span<const StatusRule> rules,
                                            const StatusTarget& target,
                                            QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kMagicWard);
}

// decomp: func_80083D2C (src/BATTLE/BATTLE.PRG/146C.c:12380).
std::optional<StatusFeedback> QueueRegeneration(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kRegeneration,
                       StatusFeedback::kPositive);
}

// decomp: func_80083D7C (src/BATTLE/BATTLE.PRG/146C.c:12389).
std::optional<BanishOutcome> QueueBanish(std::span<const StatusRule> rules,
                                         const StatusTarget& target,
                                         QueuedStatuses& queued,
                                         bool execute_action) {
  const auto changed = QueueRule(rules, target, queued, StatusRuleId::kBanish);
  if (!changed) return std::nullopt;
  return BanishOutcome{.changed_mask = changed.value(),
                       .analyze_requested = execute_action};
}
// decomp: func_80083DEC (src/BATTLE/BATTLE.PRG/146C.c:12399).
std::optional<StatusFeedback> QueueParalysis(std::span<const StatusRule> rules,
                                             const StatusTarget& target,
                                             QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kParalysis,
                       StatusFeedback::kNegative);
}

// decomp: func_80083E3C (src/BATTLE/BATTLE.PRG/146C.c:12408).
std::optional<StatusFeedback> QueuePoison(std::span<const StatusRule> rules,
                                          const StatusTarget& target,
                                          QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kPoison,
                       StatusFeedback::kNegative);
}

// decomp: func_80083E8C (src/BATTLE/BATTLE.PRG/146C.c:12417).
std::optional<StatusFeedback> QueueNumbness(std::span<const StatusRule> rules,
                                            const StatusTarget& target,
                                            QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kNumbness,
                       StatusFeedback::kNegative);
}

// decomp: func_80083EDC (src/BATTLE/BATTLE.PRG/146C.c:12426).
std::optional<StatusFeedback> QueueCurse(std::span<const StatusRule> rules,
                                         const StatusTarget& target,
                                         QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kCurse,
                       StatusFeedback::kNegative);
}

// decomp: func_80083F2C (src/BATTLE/BATTLE.PRG/146C.c:12435).
std::optional<std::uint32_t> QueueAirAttackUp(std::span<const StatusRule> rules,
                                              const StatusTarget& target,
                                              QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kAirAttackUp);
}

// decomp: func_80083F50 (src/BATTLE/BATTLE.PRG/146C.c:12442).
std::optional<std::uint32_t> QueueFireAttackUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kFireAttackUp);
}

// decomp: func_80083F74 (src/BATTLE/BATTLE.PRG/146C.c:12449).
std::optional<std::uint32_t> QueueEarthAttackUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kEarthAttackUp);
}

// decomp: func_80083F98 (src/BATTLE/BATTLE.PRG/146C.c:12456).
std::optional<std::uint32_t> QueueWaterAttackUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kWaterAttackUp);
}

// decomp: func_80083FBC (src/BATTLE/BATTLE.PRG/146C.c:12463).
std::optional<std::uint32_t> QueueAirDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kAirDefenseUp);
}

// decomp: func_80083FE0 (src/BATTLE/BATTLE.PRG/146C.c:12470).
std::optional<std::uint32_t> QueueFireDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kFireDefenseUp);
}

// decomp: func_80084004 (src/BATTLE/BATTLE.PRG/146C.c:12477).
std::optional<std::uint32_t> QueueEarthDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kEarthDefenseUp);
}

// decomp: func_80084028 (src/BATTLE/BATTLE.PRG/146C.c:12484).
std::optional<std::uint32_t> QueueWaterDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueRule(rules, target, queued, StatusRuleId::kWaterDefenseUp);
}

// decomp: func_8008404C (src/BATTLE/BATTLE.PRG/146C.c:12491).
std::optional<StatusFeedback> QueueParalysisCure(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kParalysis,
                       StatusFeedback::kPositive, QueueStatusRemovalMask);
}

// decomp: func_8008409C (src/BATTLE/BATTLE.PRG/146C.c:12500).
std::optional<StatusFeedback> QueuePoisonCure(std::span<const StatusRule> rules,
                                              const StatusTarget& target,
                                              QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kPoison,
                       StatusFeedback::kPositive, QueueStatusRemovalMask);
}

// decomp: func_800840EC (src/BATTLE/BATTLE.PRG/146C.c:12509).
std::optional<StatusFeedback> QueueCurseCure(std::span<const StatusRule> rules,
                                             const StatusTarget& target,
                                             QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kCurse,
                       StatusFeedback::kPositive, QueueStatusRemovalMask);
}

// decomp: func_8008413C (src/BATTLE/BATTLE.PRG/146C.c:12518).
std::optional<StatusFeedback> QueueNumbnessCure(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued) {
  return QueueStatRule(rules, target, queued, StatusRuleId::kNumbness,
                       StatusFeedback::kPositive, QueueStatusRemovalMask);
}
// decomp: func_8008418C (src/BATTLE/BATTLE.PRG/146C.c:12527).
std::uint32_t QueueDispel(std::uint32_t active, QueuedStatuses& queued) {
  constexpr std::uint32_t kDispelStatuses = 0x1FFE1FE0;
  const auto removed = active & kDispelStatuses;
  queued.removed |= removed;
  return removed;
}

// decomp: func_800841C8 (src/BATTLE/BATTLE.PRG/146C.c:12535).
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
