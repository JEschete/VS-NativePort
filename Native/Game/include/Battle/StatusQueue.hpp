#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Battle {
inline constexpr std::size_t kStatusRuleCount = 32;

enum class StatusRuleId : std::uint8_t {
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
  kInstantDeathRemoval = 30,
};

enum class StatusFeedback : std::uint8_t {
  kNone = 0,
  kNegative = 1,
  kPositive = 2,
};

struct StatusRule {
  std::uint32_t statuses = 0;
  std::uint32_t cancelled = 0;
  std::uint32_t blocking = 0;
};

struct StatusTarget {
  std::uint32_t active = 0;
  std::uint32_t immunities = 0;
  bool exempt = false;
};

struct QueuedStatuses {
  std::uint32_t added = 0;
  std::uint32_t removed = 0;
  StatusFeedback feedback = StatusFeedback::kNone;

  friend bool operator==(const QueuedStatuses&,
                         const QueuedStatuses&) = default;
};

struct BanishOutcome {
  std::uint32_t changed_mask = 0;
  bool analyze_requested = false;

  friend bool operator==(const BanishOutcome&, const BanishOutcome&) = default;
};

// decomp: _setApplicableStatuses (src/BATTLE/BATTLE.PRG/146C.c:11195).
std::uint32_t QueueApplicableStatusChanges(const StatusRule& rule,
                                           const StatusTarget& target,
                                           QueuedStatuses& queued);
// decomp: _setInflictedStatuses (src/BATTLE/BATTLE.PRG/146C.c:11217).
std::uint32_t QueueStatusRemovalMask(const StatusRule& rule,
                                     const StatusTarget& target,
                                     QueuedStatuses& queued);

// The result describes this call, while queued.feedback changes only if
// some status was added or removed. Nothing refuses a missing rule.
// decomp: func_80083998 (src/BATTLE/BATTLE.PRG/146C.c:12273).
std::optional<StatusFeedback> QueueStrengthDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_800839E8 (src/BATTLE/BATTLE.PRG/146C.c:12282).
std::optional<StatusFeedback> QueueIntelligenceDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083A38 (src/BATTLE/BATTLE.PRG/146C.c:12291).
std::optional<StatusFeedback> QueueAgilityDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083A88 (src/BATTLE/BATTLE.PRG/146C.c:12300).
std::optional<StatusFeedback> QueueEquipmentDown(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083AD8 (src/BATTLE/BATTLE.PRG/146C.c:12309).
std::optional<StatusFeedback> QueueHalfMovement(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083B28 (src/BATTLE/BATTLE.PRG/146C.c:12318).
std::optional<StatusFeedback> QueueStrengthUp(std::span<const StatusRule> rules,
                                              const StatusTarget& target,
                                              QueuedStatuses& queued);
// decomp: func_80083B78 (src/BATTLE/BATTLE.PRG/146C.c:12327).
std::optional<StatusFeedback> QueueIntelligenceUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083BC8 (src/BATTLE/BATTLE.PRG/146C.c:12336).
std::optional<StatusFeedback> QueueAgilityUp(std::span<const StatusRule> rules,
                                             const StatusTarget& target,
                                             QueuedStatuses& queued);
// decomp: func_80083C18 (src/BATTLE/BATTLE.PRG/146C.c:12345).
std::optional<StatusFeedback> QueueEquipmentUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083C68 (src/BATTLE/BATTLE.PRG/146C.c:12354).
std::optional<StatusFeedback> QueueQuicken(std::span<const StatusRule> rules,
                                           const StatusTarget& target,
                                           QueuedStatuses& queued);
// decomp: func_80083CB8 (src/BATTLE/BATTLE.PRG/146C.c:12363).
std::optional<StatusFeedback> QueueSilence(std::span<const StatusRule> rules,
                                           const StatusTarget& target,
                                           QueuedStatuses& queued);
// decomp: func_80083D08 (src/BATTLE/BATTLE.PRG/146C.c:12372).
std::optional<std::uint32_t> QueueMagicWard(std::span<const StatusRule> rules,
                                            const StatusTarget& target,
                                            QueuedStatuses& queued);
// decomp: func_80083D2C (src/BATTLE/BATTLE.PRG/146C.c:12380).
std::optional<StatusFeedback> QueueRegeneration(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// Analysis is requested on execution even when the status rule has no effect.
// decomp: func_80083D7C (src/BATTLE/BATTLE.PRG/146C.c:12389).
std::optional<BanishOutcome> QueueBanish(std::span<const StatusRule> rules,
                                         const StatusTarget& target,
                                         QueuedStatuses& queued,
                                         bool execute_action);
// decomp: func_80083DEC (src/BATTLE/BATTLE.PRG/146C.c:12399).
std::optional<StatusFeedback> QueueParalysis(std::span<const StatusRule> rules,
                                             const StatusTarget& target,
                                             QueuedStatuses& queued);
// decomp: func_80083E3C (src/BATTLE/BATTLE.PRG/146C.c:12408).
std::optional<StatusFeedback> QueuePoison(std::span<const StatusRule> rules,
                                          const StatusTarget& target,
                                          QueuedStatuses& queued);
// decomp: func_80083E8C (src/BATTLE/BATTLE.PRG/146C.c:12417).
std::optional<StatusFeedback> QueueNumbness(std::span<const StatusRule> rules,
                                            const StatusTarget& target,
                                            QueuedStatuses& queued);
// decomp: func_80083EDC (src/BATTLE/BATTLE.PRG/146C.c:12426).
std::optional<StatusFeedback> QueueCurse(std::span<const StatusRule> rules,
                                         const StatusTarget& target,
                                         QueuedStatuses& queued);
// decomp: func_80083F2C (src/BATTLE/BATTLE.PRG/146C.c:12435).
std::optional<std::uint32_t> QueueAirAttackUp(std::span<const StatusRule> rules,
                                              const StatusTarget& target,
                                              QueuedStatuses& queued);
// decomp: func_80083F50 (src/BATTLE/BATTLE.PRG/146C.c:12442).
std::optional<std::uint32_t> QueueFireAttackUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083F74 (src/BATTLE/BATTLE.PRG/146C.c:12449).
std::optional<std::uint32_t> QueueEarthAttackUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083F98 (src/BATTLE/BATTLE.PRG/146C.c:12456).
std::optional<std::uint32_t> QueueWaterAttackUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083FBC (src/BATTLE/BATTLE.PRG/146C.c:12463).
std::optional<std::uint32_t> QueueAirDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80083FE0 (src/BATTLE/BATTLE.PRG/146C.c:12470).
std::optional<std::uint32_t> QueueFireDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80084004 (src/BATTLE/BATTLE.PRG/146C.c:12477).
std::optional<std::uint32_t> QueueEarthDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_80084028 (src/BATTLE/BATTLE.PRG/146C.c:12484).
std::optional<std::uint32_t> QueueWaterDefenseUp(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_8008404C (src/BATTLE/BATTLE.PRG/146C.c:12491).
std::optional<StatusFeedback> QueueParalysisCure(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// decomp: func_8008409C (src/BATTLE/BATTLE.PRG/146C.c:12500).
std::optional<StatusFeedback> QueuePoisonCure(std::span<const StatusRule> rules,
                                              const StatusTarget& target,
                                              QueuedStatuses& queued);
// decomp: func_800840EC (src/BATTLE/BATTLE.PRG/146C.c:12509).
std::optional<StatusFeedback> QueueCurseCure(std::span<const StatusRule> rules,
                                             const StatusTarget& target,
                                             QueuedStatuses& queued);
// decomp: func_8008413C (src/BATTLE/BATTLE.PRG/146C.c:12518).
std::optional<StatusFeedback> QueueNumbnessCure(
    std::span<const StatusRule> rules, const StatusTarget& target,
    QueuedStatuses& queued);
// These direct filters do not consult rule tables, immunity or exemption.
// decomp: func_8008418C (src/BATTLE/BATTLE.PRG/146C.c:12527).
std::uint32_t QueueDispel(std::uint32_t active, QueuedStatuses& queued);
// decomp: func_800841C8 (src/BATTLE/BATTLE.PRG/146C.c:12535).
StatusFeedback QueueNegativeStatusCure(std::uint32_t active,
                                       QueuedStatuses& queued);
}  // namespace VS::Battle
