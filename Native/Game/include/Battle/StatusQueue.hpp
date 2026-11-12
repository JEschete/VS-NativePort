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
  kRegeneration = 17,
  kMagicWard = 18,
  kEquipmentDown = 19,
  kEquipmentUp = 20,
  kBanish = 29,
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
}  // namespace VS::Battle
