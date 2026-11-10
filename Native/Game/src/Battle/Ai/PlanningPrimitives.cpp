// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_helpers.c and battle_movement.c,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0; THIRD_PARTY.md).
#include "Battle/Ai/PlanningPrimitives.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>

#include "Battle/Ai/DecisionPrimitives.hpp"
#include "Core/Diagnostics.h"
#include "Core/Math/ScalarMath.hpp"
#include "Core/PointerSort.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr unsigned kRowShift = 0;
constexpr unsigned kRowBits = 3;
constexpr unsigned kColumnShift = 3;
constexpr unsigned kColumnBits = 2;
constexpr unsigned kActionShift = 5;
constexpr unsigned kActionBits = 8;
constexpr unsigned kThresholdShift = 13;
constexpr unsigned kThresholdBits = 16;
constexpr unsigned kTargetShift = 0;
constexpr unsigned kTargetBits = 4;
constexpr unsigned kTargetLimbShift = 4;
constexpr unsigned kTargetLimbBits = 3;
constexpr unsigned kHeightOffsetShift = 7;
constexpr unsigned kHeightOffsetBits = 12;
constexpr unsigned kWeightShift = 19;
constexpr unsigned kWeightBits = 8;
constexpr unsigned kRangeShift = 6;
constexpr unsigned kRangeBits = 16;
constexpr std::size_t kScoreWord = 0;
constexpr std::size_t kSlotWord = 1;
constexpr std::size_t kDistanceWord = 2;
constexpr std::size_t kTargetWord = 3;
constexpr std::size_t kOptionWord = 4;

constexpr unsigned kAlliedClassification = 2;
constexpr unsigned kHostileClassification = 3;
constexpr std::uint16_t kUnconditionalTargetFlag = 0x100;
constexpr unsigned kTargetConditionShift = 9;
constexpr std::int16_t kMinimumLimbHp = 2;
constexpr std::uint32_t kSpellBlockingStatus = 0x1000;
constexpr std::uint32_t kSpellsAndAbilitiesOnlyStatus = 0x2000;
constexpr std::uint32_t kResourceRestrictingStatuses = 0xE2B0;
constexpr unsigned kSpellAction = 1;
constexpr unsigned kAbilityAction = 2;
constexpr unsigned kBreakArtAction = 3;
constexpr unsigned kWeaponPpAction = 4;
constexpr unsigned kRememberedTargetShift = 24;
constexpr std::uint32_t kRememberedTileMask = 0xFFFFFF;

// decomp: BATTLE.PRG statusScores (74088.c:567); the 32 signed bytes at
// 0x80069BD0 in BATTLE.PRG, one weight per status bit.
constexpr std::array<std::int8_t, 32> kStatusScores = {
    -51, -39, -39, -39, -26, -51, 50, -13, 12, -13, 12, 25, -51, -26, -45, -26,
    -39, 76,  19,  -13, 12,  12,  12, 12,  12, 12,  12, 12, 12,  0,   0,   0,
};
constexpr unsigned kActionCostStatusBit = 12;
constexpr std::int32_t kZeroHpScorePenalty = 128;
constexpr std::int32_t kLowHpScorePenalty = 84;
// HP below this many 128ths of the maximum takes the low-HP penalty.
constexpr std::int32_t kLowHpPer128 = 44;
constexpr std::int32_t kConditionRatioScale = 128;
constexpr unsigned kHitWeightSmallShift = 5;
constexpr unsigned kCandidateScoreShift = 7;
constexpr int kRareCandidateRandomMask = 127;
constexpr int kRareCandidateRejectionThreshold = 115;
constexpr unsigned kDefaultCandidateWeight = 128;
constexpr unsigned kSpecialCandidateWeight = 40;
constexpr unsigned kIgnoredClassification = 1;
constexpr std::uint16_t kLimbRecoveryReadiness = 2880;
constexpr std::int32_t kHpRecoveryPeriod = 240;
constexpr std::int32_t kMpRecoveryPeriod = 180;
constexpr unsigned kTimedStatusDurationScale = 30;
constexpr std::uint32_t kMobilityRestrictedStatus = 0x10;
constexpr std::uint32_t kPoisonStatus = 0x4000;
constexpr std::uint32_t kRegenerationStatus = 0x20000;
constexpr std::int32_t kEnumerationBaselineDistance = 256;
constexpr unsigned kFallbackLimbLowMask = 3;
constexpr unsigned kFallbackLimbHighBit = 3;
// Action category bit sets (1 << category) the slot metadata classifies.
constexpr std::uint32_t kSelfExcludingCategories = 0x4198;
constexpr std::uint32_t kObstructionIgnoringCategories = 0x154;
constexpr std::uint32_t kDirectCategories = 0xA;
constexpr std::uint32_t kLimbCategories = 0x40A0;
constexpr std::uint32_t kAimedCategories = 0x1954;
constexpr std::uint32_t kFallbackCategories = 0x4000;
constexpr unsigned kActionCategoryMask = 15;
constexpr std::uint32_t kLongRangeSquared = 0x51000;
constexpr std::uint8_t kPlayerActionCostThreshold = 1;

void UpdateEnumerationWinners(const ActionCandidate& candidate,
                              ActionCandidate& random_best,
                              ActionCandidate& best,
                              CandidateEnumerationHost& host) {
  const auto chance = candidate.Weight();
  const auto roll =
      static_cast<unsigned>(host.Random()) & kRareCandidateRandomMask;
  if (roll < chance && candidate.Score() > random_best.Score()) {
    random_best = candidate;
  }
  if (candidate.Score() > best.Score()) best = candidate;
}

void AppendEnumerationWinner(const ActionCandidate& baseline,
                             const ActionCandidate& random_best,
                             const ActionCandidate& best,
                             CandidateEnumerationHost& host) {
  if (random_best.Score() > baseline.Score()) {
    host.Append(random_best);
  } else if (best.Score() > baseline.Score()) {
    host.Append(best);
  }
}

bool EnumerateLimbCandidates(ActionCandidate& candidate,
                             const ActionCandidate& baseline, unsigned action,
                             CandidateEnumerationHost& host) {
  auto random_best = baseline;
  auto best = baseline;
  std::optional<unsigned> last;
  for (unsigned limb = 0; limb < kLimbActionRows; ++limb) {
    if (host.LimbMaximum(limb) == 0) continue;
    last = limb;
    candidate.SetTargetLimb(limb);
    if (!host.Evaluate(candidate, CandidateEnumerationKind::kLimb, action)) {
      return false;
    }
    UpdateEnumerationWinners(candidate, random_best, best, host);
  }
  const auto draw = static_cast<unsigned>(host.Random());
  auto part = (draw & kFallbackLimbLowMask) + ((draw >> 2) & 1u) +
              ((draw >> kFallbackLimbHighBit) & 1u);
  if (host.LimbMaximum(part) == 0 && last) part = *last;
  candidate.SetTargetLimb(part);
  host.Cache(candidate, action);
  AppendEnumerationWinner(baseline, random_best, best, host);
  return true;
}

bool EnumerateTargetCandidates(ActionCandidate& candidate,
                               const ActionCandidate& baseline, unsigned action,
                               CandidateEnumerationKind kind,
                               CandidateEnumerationHost& host) {
  switch (kind) {
    case CandidateEnumerationKind::kNone:
      return true;
    case CandidateEnumerationKind::kDirect:
      if (!host.Evaluate(candidate, CandidateEnumerationKind::kDirect,
                         action)) {
        return false;
      }
      host.Cache(candidate, action);
      if (candidate.Score() > baseline.Score()) host.Append(candidate);
      return true;
    case CandidateEnumerationKind::kLimb:
      return EnumerateLimbCandidates(candidate, baseline, action, host);
    case CandidateEnumerationKind::kAimed: {
      auto random_best = baseline;
      auto best = baseline;
      candidate.SetHeightOffset(0);
      const int height = host.TargetHeight();
      candidate.SetHeightOffset(
          static_cast<unsigned>(height < 0 ? -height : height));
      if (!host.Evaluate(candidate, CandidateEnumerationKind::kAimed, action)) {
        return false;
      }
      UpdateEnumerationWinners(candidate, random_best, best, host);
      host.Cache(candidate, action);
      AppendEnumerationWinner(baseline, random_best, best, host);
      return true;
    }
  }
  return false;
}

void RecoverPlanningVital(PlanningVital& vital, std::int32_t amount) {
  vital.value = std::min(
      vital.maximum,
      Core::Math::WrapSigned16(static_cast<std::uint32_t>(vital.value) +
                               static_cast<std::uint32_t>(amount)));
}

void ApplyPlanningStatusTick(PlanningVital& hp, std::int32_t ticks,
                             std::uint16_t interval, bool restores) {
  const auto count = Core::Math::R3000Quotient(ticks, interval);
  const auto amount = Core::Math::WrappingMultiply(count, hp.maximum * 5) / 100;
  const auto next = restores ? Core::Math::WrappingAdd(hp.value, amount)
                             : Core::Math::WrappingSubtract(hp.value, amount);
  hp.value = std::max<std::int16_t>(
      0, Core::Math::WrapSigned16(static_cast<std::uint32_t>(next)));
}

bool AdvanceSimulationStats(PlanningSimulationStats& stats,
                            std::uint16_t readiness, std::int32_t time_step,
                            const PlanningSimulationRules& rules) {
  if (stats.mobility_limbs[0] >= stats.limbs.size()) {
    Core_Debugf("ai", "Planning simulation requires valid mobility limbs");
    return false;
  }
  if (readiness > kLimbRecoveryReadiness) {
    for (auto& limb : stats.limbs) RecoverPlanningVital(limb, 2);
  }
  bool mobile = stats.limbs[stats.mobility_limbs[0]].value >= 2;
  if (!mobile) {
    if (stats.mobility_limbs[1] >= stats.limbs.size()) {
      Core_Debugf("ai", "Planning simulation requires a second mobility limb");
      return false;
    }
    mobile = stats.limbs[stats.mobility_limbs[1]].value >= 2;
  }
  if (mobile) {
    stats.statuses &= ~kMobilityRestrictedStatus;
  }
  RecoverPlanningVital(stats.hp,
                       std::max(time_step / kHpRecoveryPeriod - 1, 0));
  RecoverPlanningVital(stats.mp,
                       std::max(time_step / kMpRecoveryPeriod - 1, 0));
  const auto ticks = (time_step >> 5) + (time_step >> 9);
  if ((stats.statuses & kPoisonStatus) != 0) {
    ApplyPlanningStatusTick(stats.hp, ticks, rules.poison_interval, false);
  }
  if ((stats.statuses & kRegenerationStatus) != 0) {
    ApplyPlanningStatusTick(stats.hp, ticks, rules.regeneration_interval, true);
  }
  for (std::size_t index = 0; index < rules.timed_status_masks.size();
       ++index) {
    if (readiness >= stats.status_timers[index] * kTimedStatusDurationScale) {
      stats.statuses &= ~rules.timed_status_masks[index];
    }
  }
  return true;
}

bool ApplyConditionDelta(std::int16_t& value, std::int16_t maximum,
                         const SimulatedStatChange& change) {
  constexpr std::uint8_t kRestoreSimulationOperation = 2;
  const auto next =
      ApplySimulatedStatDelta(value, maximum, change.delta,
                              change.operation == kRestoreSimulationOperation);
  if (!next) return false;
  value = *next;
  return true;
}

bool ApplyActorEffect(std::span<ActorCondition> conditions,
                      const SimulatedActorEffect& effect) {
  if (effect.ignored) return true;
  auto& row = conditions[effect.actor_id];
  if (!ApplyConditionDelta(row.hp, row.max_hp, effect.hp) ||
      !ApplyConditionDelta(row.mp, row.max_mp, effect.mp) ||
      !ApplyConditionDelta(row.weapon_dp, row.weapon_max_dp,
                           effect.weapon_dp) ||
      !ApplyConditionDelta(row.shield_dp, row.shield_max_dp,
                           effect.shield_dp)) {
    return false;
  }
  row.statuses |= effect.statuses_set;
  row.statuses &= ~effect.statuses_clear;
  return true;
}

bool ApplySimulationCost(ActorCondition& row, const ActionCost& action) {
  constexpr unsigned kSimulationSpell = 1;
  constexpr unsigned kSimulationBreakArt = 3;
  constexpr unsigned kSimulationBaseAttack = 6;
  const SimulatedStatChange cost{.delta =
                                     static_cast<std::int16_t>(action.cost)};
  switch (action.type) {
    case kSimulationSpell:
      return ApplyConditionDelta(row.mp, row.max_mp, cost);
    case kSimulationBreakArt:
      return ApplyConditionDelta(row.hp, row.max_hp, cost);
    case kSimulationBaseAttack:
      return ApplyConditionDelta(row.weapon_dp, row.weapon_max_dp, cost);
    default:
      return true;
  }
}

std::nullopt_t Refuse(const char* message) {
  Core_Debugf("ai", "%s", message);
  return std::nullopt;
}

bool ValidPlanningStatsRows(const PlanningStatsSnapshot& snapshot,
                            std::span<const PlanningActorStats> actors) {
  return snapshot.bytes.size() == kPlanningStatsTableBytes &&
         actors.size() <= kBattleActorSlotCount &&
         std::all_of(actors.begin(), actors.end(), [](const auto& actor) {
           return actor.slot < kBattleActorSlotCount &&
                  actor.bytes.size() == kActorStatsRecordBytes;
         });
}

std::span<std::byte, kActorStatsRecordBytes> PlanningSnapshotRow(
    const PlanningStatsSnapshot& snapshot, std::size_t slot) {
  return std::span<std::byte, kActorStatsRecordBytes>(
      snapshot.bytes.data() + slot * kActorStatsRecordBytes,
      kActorStatsRecordBytes);
}

std::optional<std::size_t> ReadinessCount(const CandidateReadinessHost& host) {
  const int count = host.Count();
  if (count < 0 || static_cast<std::size_t>(count) > kCandidateCapacity) {
    return Refuse("AI readiness count is outside its ranking");
  }
  return static_cast<std::size_t>(count);
}

struct ReadinessRank {
  std::size_t handle;
  ActionCandidate candidate;
};

std::optional<ReadinessRank> ReadReadinessRank(
    const CandidateReadinessHost& host, std::size_t rank) {
  const auto handle = host.RankedHandle(rank);
  if (!handle) return Refuse("AI readiness ranking has no candidate");
  const auto candidate = host.Candidate(*handle);
  if (!candidate) return Refuse("AI readiness candidate is unavailable");
  return ReadinessRank{*handle, *candidate};
}

bool RevalidateReadinessCandidate(CandidateReadinessHost& host,
                                  const ReadinessRank& entry) {
  if (entry.candidate.Weight() == 0 ||
      entry.candidate.Threshold() > host.ReadinessLevel()) {
    return true;
  }
  const auto affects = host.CanAffect(entry.handle, entry.candidate.TargetId());
  if (!affects) return false;
  if (*affects) {
    const auto fresh = host.Candidate(entry.handle);
    if (!fresh || fresh->Row() >= kLimbActionRows ||
        fresh->Column() >= kLimbActionColumns) {
      return false;
    }
    const auto usable = host.RefreshUsability(fresh->Row(), fresh->Column());
    if (!usable) return false;
    if (*usable) return true;
  }
  return host.ClearWeight(entry.handle);
}

std::optional<bool> PromoteReadinessCandidate(CandidateReadinessHost& host) {
  const auto count = ReadinessCount(host);
  if (!count) return std::nullopt;
  if (*count == 0) return false;
  const auto first = ReadReadinessRank(host, 0);
  if (!first) return std::nullopt;
  if (first->candidate.Weight() != 0) return false;
  for (std::size_t rank = 1; rank < *count; ++rank) {
    const auto next = ReadReadinessRank(host, rank);
    if (!next) return std::nullopt;
    if (next->candidate.Weight() != 0) {
      if (!host.SetFirstRank(next->handle) || !host.SelectMovementMode()) {
        return Refuse("AI readiness promotion was refused");
      }
      return true;
    }
  }
  if (!host.ClearCount()) return Refuse("AI readiness count clear was refused");
  return true;
}

std::optional<std::uint8_t> CandidateReadinessStatus(
    const CandidateReadinessHost& host) {
  constexpr std::uint32_t kReadinessMargin = 30;
  constexpr std::uint8_t kNoCandidateHostilePresent = 2;
  constexpr std::uint8_t kNoCandidateHostileAbsent = 3;
  const auto count = ReadinessCount(host);
  if (!count) return std::nullopt;
  if (*count == 0) {
    const auto hostile = host.HasHostileActor();
    if (!hostile) {
      return Refuse("AI readiness actor classification is unavailable");
    }
    return *hostile ? kNoCandidateHostilePresent : kNoCandidateHostileAbsent;
  }
  const auto level = static_cast<std::uint32_t>(host.ReadinessLevel());
  const auto first = ReadReadinessRank(host, 0);
  if (!first) return std::nullopt;
  return static_cast<std::uint8_t>(level + kReadinessMargin <
                                   first->candidate.Threshold());
}

constexpr std::uint32_t FieldMask(unsigned bits) {
  return (std::uint32_t{1} << bits) - 1;
}

constexpr unsigned Field(std::uint32_t word, unsigned shift, unsigned bits) {
  return (word >> shift) & FieldMask(bits);
}

constexpr std::uint32_t WithField(std::uint32_t word, unsigned shift,
                                  unsigned bits, unsigned value) {
  const std::uint32_t mask = FieldMask(bits) << shift;
  return (word & ~mask) | ((std::uint32_t{value} << shift) & mask);
}

bool IsSlot(int row, int column) {
  return row >= 0 && row < kLimbActionRows && column >= 0 &&
         column < kLimbActionColumns;
}

std::optional<std::int32_t> CheckedInt32(std::int64_t value) {
  if (value < std::numeric_limits<std::int32_t>::min() ||
      value > std::numeric_limits<std::int32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(value);
}

// The value scaled to 128 at the maximum, truncated toward zero; zero without
// a maximum.
constexpr std::int32_t ConditionRatio(std::int16_t value,
                                      std::int16_t maximum) {
  return maximum == 0 ? 0
                      : std::int32_t{value} * kConditionRatioScale / maximum;
}

bool IsActionUsable(const ActionCost& action, const ActorResources& resources) {
  switch (action.type) {
    case kSpellAction:
      return std::bit_cast<std::int16_t>(resources.mp) >=
                 static_cast<int>(action.cost) &&
             (resources.statuses & kSpellBlockingStatus) == 0;
    case kBreakArtAction:
      return unsigned{resources.hp} >= action.cost + (resources.max_hp / 2u);
    case kWeaponPpAction:
      return std::bit_cast<std::int16_t>(resources.weapon_pp) >=
             static_cast<int>(action.cost);
    default:
      return true;
  }
}
}  // namespace

// decomp: BATTLE.PRG func_800DC888 (74088.c:473), original134MIPS.
bool ApplySimulatedActionResults(
    std::span<ActorCondition> conditions, const ActionCost& action,
    const SimulatedActorEffect& performer,
    std::span<const SimulatedActorEffect> targets) {
  const auto valid = [conditions](const SimulatedActorEffect& effect) {
    return effect.ignored || effect.actor_id < conditions.size();
  };
  if (!valid(performer) ||
      std::any_of(targets.begin(), targets.end(),
                  [&valid](const auto& effect) { return !valid(effect); }) ||
      (!performer.ignored && action.cost > UINT8_MAX)) {
    Refuse("AI simulation actor or byte cost is outside its table");
    return false;
  }
  if (!performer.ignored &&
      !ApplySimulationCost(conditions[performer.actor_id], action)) {
    return false;
  }
  if (!ApplyActorEffect(conditions, performer)) return false;
  for (const auto& target : targets) {
    if (!ApplyActorEffect(conditions, target)) return false;
  }
  return true;
}

// decomp: BATTLE.PRG func_800DCAA0 (74088.c:513), original78MIPS.
bool SimulateActionCandidate(CandidateSimulationHost& host,
                             const ActionCandidate& candidate,
                             CandidateSimulationMode mode) {
  std::optional<bool> available;
  if (mode == CandidateSimulationMode::kAimed) {
    auto target = host.TargetPosition(candidate.TargetId());
    if (!target) {
      Refuse("AI aimed candidate has no target position");
      return false;
    }
    target->y = Core::Math::WrapSigned16(static_cast<std::uint32_t>(target->y) -
                                         candidate.HeightOffset());
    target->pad = 0;
    auto source = *target;
    source.z = Core::Math::WrapSigned16(
        static_cast<std::uint32_t>(target->z) +
        static_cast<std::uint32_t>(
            Core::Math::SquareRootInteger(candidate.DistanceSquared())));
    available = host.PreviewAimed(candidate, source, *target);
  } else {
    available = host.PreviewLimb(candidate);
  }
  if (!available || (*available && !host.ApplyResults())) {
    Refuse("AI candidate preview or projected results were refused");
    return false;
  }
  return true;
}

// decomp: BATTLE.PRG func_800DCBD8 (74088.c:539), original47MIPS.
void UpdateCandidateHitWeight(ActionCandidate& candidate,
                              std::span<const SimulatedActorEffect> targets) {
  const auto found = std::find_if(
      targets.begin(), targets.end(), [&candidate](const auto& target) {
        return !target.ignored && target.actor_id == candidate.TargetId();
      });
  const unsigned threshold = found == targets.end() ? 0u : found->hit_threshold;
  candidate.SetWeight(threshold + (threshold >> 2) +
                      (threshold >> kHitWeightSmallShift));
}

// decomp: BATTLE.PRG func_800DCC94 (74088.c:620), original219MIPS.
bool EvaluateActionCandidate(ActionCandidate& candidate,
                             const CandidateEvaluationOptions& options,
                             CandidateEvaluationHost& host) {
  if (options.rarely_chosen && (host.Random() & kRareCandidateRandomMask) <
                                   kRareCandidateRejectionThreshold) {
    candidate.SetScore(std::numeric_limits<std::int32_t>::min());
    return true;
  }
  candidate.SetScore(0);
  candidate.SetWeight(kDefaultCandidateWeight);
  if (!host.Capture(candidate)) {
    Refuse("AI candidate condition capture was refused");
    return false;
  }
  const bool special = host.SpecialMode() && options.special_action;
  if (special) {
    candidate.SetWeight(kSpecialCandidateWeight);
  } else if (options.simulation) {
    if (!host.Simulate(candidate, *options.simulation)) {
      Refuse("AI candidate simulation was refused");
      return false;
    }
    const auto hits = host.Hits();
    if (!hits) {
      Refuse("AI candidate hit results are unavailable");
      return false;
    }
    UpdateCandidateHitWeight(candidate, *hits);
  }
  const auto conditions = host.Conditions();
  if (!conditions || conditions->size() > kBattleActorSlotCount) {
    Refuse("AI candidate condition list is unavailable or too large");
    return false;
  }
  auto score = std::bit_cast<std::uint32_t>(candidate.Score());
  for (const auto& actor : *conditions) {
    if (actor.classification == kIgnoredClassification) continue;
    const auto value = ScoreActorCondition(
        {.status = ActorStatusConditionScore(actor.condition,
                                             actor.action_cost_enabled),
         .hp = Core::Math::WrapSigned16(
             static_cast<std::uint32_t>(ActorHpRatio(actor.condition))),
         .mp = Core::Math::WrapSigned16(
             static_cast<std::uint32_t>(ActorMpRatio(actor.condition))),
         .dp = Core::Math::WrapSigned16(static_cast<std::uint32_t>(
             ActorEquipmentDpRatio(actor.condition)))});
    if (!value) return false;
    const auto word = std::bit_cast<std::uint32_t>(*value);
    score += actor.classification == kAlliedClassification ? word : 0u - word;
  }
  if (special) ++score;
  candidate.SetScore(std::bit_cast<std::int32_t>(
      (score << kCandidateScoreShift) + candidate.Weight()));
  return true;
}

// decomp: BATTLE.PRG _getStatusScore (74088.c:565), inlined in func_800DCC94
// (0x800DCDD4-0x800DCF5C, original99MIPS with the three ratios).
std::int32_t ActorStatusConditionScore(const ActorCondition& condition,
                                       bool action_cost_enabled) {
  std::int32_t score = 0;
  for (unsigned bit = 0; bit < kStatusScores.size(); ++bit) {
    const bool set = (condition.statuses & (std::uint32_t{1} << bit)) != 0;
    if (set && (bit != kActionCostStatusBit || action_cost_enabled)) {
      score += kStatusScores[bit];
    }
  }
  if (condition.hp == 0) score -= kZeroHpScorePenalty;
  if (condition.hp * kConditionRatioScale < condition.max_hp * kLowHpPer128) {
    score -= kLowHpScorePenalty;
  }
  return score;
}

// decomp: BATTLE.PRG _getHpRatio (74088.c:590), inlined in func_800DCC94.
std::int32_t ActorHpRatio(const ActorCondition& condition) {
  return ConditionRatio(condition.hp, condition.max_hp);
}

// decomp: BATTLE.PRG _getMpRatio (74088.c:598), inlined in func_800DCC94.
std::int32_t ActorMpRatio(const ActorCondition& condition) {
  return ConditionRatio(condition.mp, condition.max_mp);
}

// decomp: BATTLE.PRG _getExtraRatio (74088.c:606), inlined in func_800DCC94.
std::int32_t ActorEquipmentDpRatio(const ActorCondition& condition) {
  return ConditionRatio(condition.weapon_dp, condition.weapon_max_dp) +
         ConditionRatio(condition.shield_dp, condition.shield_max_dp);
}

// decomp: BATTLE.PRG func_800D85D8 (6FC00.c:515), original132MIPS.
std::optional<bool> RefreshActionCandidateReadiness(
    CandidateReadinessHost& host) {
  for (std::size_t rank = 0;; ++rank) {
    const auto count = ReadinessCount(host);
    if (!count) return std::nullopt;
    if (rank >= *count) break;
    const auto entry = ReadReadinessRank(host, rank);
    if (!entry || !RevalidateReadinessCandidate(host, *entry)) {
      return Refuse("AI readiness revalidation was refused");
    }
  }
  const auto changed = PromoteReadinessCandidate(host);
  if (!changed) return std::nullopt;
  const auto status = CandidateReadinessStatus(host);
  if (!status || !host.StoreActionStatus(*status)) {
    return Refuse("AI readiness status store was refused");
  }
  return changed;
}

std::int32_t ActionCandidate::Score() const {
  return std::bit_cast<std::int32_t>(words_[kScoreWord]);
}

unsigned ActionCandidate::Row() const {
  return Field(words_[kSlotWord], kRowShift, kRowBits);
}

unsigned ActionCandidate::Column() const {
  return Field(words_[kSlotWord], kColumnShift, kColumnBits);
}

unsigned ActionCandidate::Action() const {
  return Field(words_[kSlotWord], kActionShift, kActionBits);
}

unsigned ActionCandidate::Threshold() const {
  return Field(words_[kSlotWord], kThresholdShift, kThresholdBits);
}

std::int32_t ActionCandidate::DistanceSquared() const {
  return std::bit_cast<std::int32_t>(words_[kDistanceWord]);
}

unsigned ActionCandidate::TargetId() const {
  return Field(words_[kTargetWord], kTargetShift, kTargetBits);
}

unsigned ActionCandidate::TargetLimb() const {
  return Field(words_[kTargetWord], kTargetLimbShift, kTargetLimbBits);
}

unsigned ActionCandidate::HeightOffset() const {
  return Field(words_[kTargetWord], kHeightOffsetShift, kHeightOffsetBits);
}

unsigned ActionCandidate::Weight() const {
  return Field(words_[kTargetWord], kWeightShift, kWeightBits);
}

bool ActionCandidate::Option(CandidateOption option) const {
  return Field(words_[kOptionWord], static_cast<unsigned>(option), 1) != 0;
}

unsigned ActionCandidate::Range() const {
  return Field(words_[kOptionWord], kRangeShift, kRangeBits);
}

void ActionCandidate::SetScore(std::int32_t score) {
  words_[kScoreWord] = std::bit_cast<std::uint32_t>(score);
}

void ActionCandidate::SetSlot(unsigned row, unsigned column) {
  auto& word = words_[kSlotWord];
  word = WithField(word, kRowShift, kRowBits, row);
  word = WithField(word, kColumnShift, kColumnBits, column);
}

void ActionCandidate::SetAction(unsigned action) {
  words_[kSlotWord] =
      WithField(words_[kSlotWord], kActionShift, kActionBits, action);
}

void ActionCandidate::SetThreshold(unsigned threshold) {
  words_[kSlotWord] =
      WithField(words_[kSlotWord], kThresholdShift, kThresholdBits, threshold);
}

void ActionCandidate::SetDistanceSquared(std::int32_t distance_squared) {
  words_[kDistanceWord] = std::bit_cast<std::uint32_t>(distance_squared);
}

void ActionCandidate::SetTargetId(unsigned target_id) {
  words_[kTargetWord] =
      WithField(words_[kTargetWord], kTargetShift, kTargetBits, target_id);
}

void ActionCandidate::SetTargetLimb(unsigned limb) {
  words_[kTargetWord] =
      WithField(words_[kTargetWord], kTargetLimbShift, kTargetLimbBits, limb);
}

void ActionCandidate::SetHeightOffset(unsigned height) {
  words_[kTargetWord] = WithField(words_[kTargetWord], kHeightOffsetShift,
                                  kHeightOffsetBits, height);
}

void ActionCandidate::SetWeight(unsigned weight) {
  words_[kTargetWord] =
      WithField(words_[kTargetWord], kWeightShift, kWeightBits, weight);
}

void ActionCandidate::SetOption(CandidateOption option, bool enabled) {
  words_[kOptionWord] = WithField(
      words_[kOptionWord], static_cast<unsigned>(option), 1, enabled ? 1 : 0);
}

void ActionCandidate::SetRange(unsigned range) {
  words_[kOptionWord] =
      WithField(words_[kOptionWord], kRangeShift, kRangeBits, range);
}

// decomp: func_800DD604 (74088.c:899), 114MIPS at 800DD604-800DD7C8.
bool EnumerateActionCandidates(std::uint16_t readiness_threshold,
                               CandidateEnumerationHost& host) {
  ActionCandidate baseline;
  baseline.SetDistanceSquared(kEnumerationBaselineDistance);
  baseline.SetThreshold(readiness_threshold);
  if (!host.Evaluate(baseline, CandidateEnumerationKind::kNone, std::nullopt)) {
    Core_Debugf("ai", "AI enumeration could not evaluate its baseline");
    return false;
  }
  auto candidate = baseline;
  candidate.SetThreshold(readiness_threshold);
  for (int row = 0; row < kLimbActionRows; ++row) {
    for (int column = 0; column < kLimbActionColumns; ++column) {
      const auto slot = host.ReadSlot(row, column);
      if (!slot) {
        Core_Debugf("ai", "AI enumeration requires readable slot data");
        return false;
      }
      if (slot->action == 0 || !slot->usable) continue;
      if (slot->action >= kActionDefinitionCount) {
        Core_Debugf("ai", "AI enumeration action is outside its table");
        return false;
      }
      host.BeginTargets();
      candidate.SetSlot(static_cast<unsigned>(row),
                        static_cast<unsigned>(column));
      candidate.SetAction(slot->action);
      candidate.SetOption(CandidateOption::kMinimumRange, slot->minimum_range);
      candidate.SetOption(CandidateOption::kIgnoresSight, slot->ignores_sight);
      candidate.SetOption(CandidateOption::kIgnoresObstruction,
                          slot->ignores_obstruction);
      candidate.SetRange(slot->radius);
      candidate.SetDistanceSquared(
          std::bit_cast<std::int32_t>(slot->range_squared));
      std::size_t count = 0;
      while (const auto target = host.NextTarget()) {
        if (++count > kBattleActorSlotCount ||
            target->id >= kCandidateTargetSlots) {
          Core_Debugf("ai",
                      "AI enumeration requires a bounded valid actor list");
          return false;
        }
        if (target->dead) continue;
        candidate.SetTargetId(target->id);
        candidate.SetOption(CandidateOption::kHarmful,
                            host.Harmful(slot->action, target->id));
        candidate.SetOption(CandidateOption::kBeneficial,
                            host.Beneficial(slot->action, target->id));
        candidate.SetOption(CandidateOption::kLongRangeHarmful,
                            host.LongRange(row, column) &&
                                candidate.Option(CandidateOption::kHarmful));
        if (!host.CanAffect(candidate, target->id) ||
            host.Excluded(target->id, slot->action)) {
          continue;
        }
        if (!EnumerateTargetCandidates(candidate, baseline, slot->action,
                                       host.Kind(row, column), host)) {
          Core_Debugf("ai", "AI enumeration could not evaluate a target");
          return false;
        }
      }
    }
  }
  return true;
}

bool AdvancePlanningSimulation(std::int32_t time_step,
                               const PlanningSimulationRules& rules,
                               PlanningSimulationHost& host) {
  const auto count = host.ActorCount();
  if (count > kBattleActorSlotCount) {
    Core_Debugf("ai", "Planning simulation actor list exceeds its capacity");
    return false;
  }
  host.SetReadiness(static_cast<std::uint16_t>(
      host.Readiness() + static_cast<std::uint32_t>(time_step)));
  for (std::size_t index = 0; index < count; ++index) {
    auto stats = host.ReadActor(index);
    if (!stats) {
      Core_Debugf("ai", "Planning simulation requires readable actor stats");
      return false;
    }
    if (!AdvanceSimulationStats(*stats, host.Readiness(), time_step, rules)) {
      return false;
    }
    host.WriteActor(index, *stats);
    if (!host.RefreshRestrictions(index)) {
      Core_Debugf("ai", "Planning simulation could not refresh restrictions");
      return false;
    }
    for (int row = 0; row < kLimbActionRows; ++row) {
      for (int column = 0; column < kLimbActionColumns; ++column) {
        if (!host.RefreshSlot(index, row, column)) {
          Core_Debugf("ai", "Planning simulation could not refresh a slot");
          return false;
        }
      }
    }
  }
  return true;
}

bool SelectFallbackAction(ActionCandidate& choice, FallbackActionHost& host) {
  std::uint32_t best = 0;
  for (int row = 0; row < kLimbActionRows; ++row) {
    for (int column = 0; column < kLimbActionColumns; ++column) {
      const auto slot = host.RefreshSlot(row, column);
      if (!slot) {
        Refuse("AI fallback selection requires readable action slots");
        return false;
      }
      if (!slot->eligible || slot->range_squared <= best) continue;
      if (slot->action >= kActionDefinitionCount) {
        Refuse("AI fallback action is outside the definition table");
        return false;
      }
      best = slot->range_squared;
      choice.SetDistanceSquared(std::bit_cast<std::int32_t>(best));
      choice.SetSlot(static_cast<unsigned>(row), static_cast<unsigned>(column));
      choice.SetAction(slot->action);
      choice.SetOption(CandidateOption::kMinimumRange, false);
      const bool ignores_sight = host.IgnoresSight(slot->action);
      choice.SetWeight(kDefaultCandidateWeight);
      choice.SetOption(CandidateOption::kIgnoresSight, ignores_sight);
      choice.SetOption(CandidateOption::kLongRangeHarmful, false);
      choice.SetOption(CandidateOption::kIgnoresObstruction, false);
    }
  }
  if (best == 0) choice.SetAction(0);
  return true;
}

// decomp: func_800E4C8C (src/BATTLE/BATTLE.PRG/7BDD4.s:585).
// C reference: rood-boi battle_movement.c:1749, revision e59320ca (CC0-1.0).
std::optional<bool> RememberAiActionTarget(TargetMemoryHost& host) {
  const int count = host.CandidateCount();
  if (count <= 0 || host.MemoryDisabled()) return false;
  if (count > static_cast<int>(kCandidateCapacity)) {
    return Refuse("AI target memory candidate count is past the list");
  }
  const std::optional<unsigned> target = host.FirstRankedTarget();
  if (!target.has_value() ||
      *target >= static_cast<unsigned>(kCandidateTargetSlots)) {
    return Refuse("AI target memory requires a ranked candidate");
  }
  const std::optional<std::uint32_t> tile = host.TargetTile(*target);
  if (!tile.has_value()) {
    return Refuse("AI target memory requires a placed target model");
  }
  host.Remember({
      .previous_tile = (*target << kRememberedTargetShift) |
                       (host.OwnTile() & kRememberedTileMask),
      .remembered_target_tile = *tile,
      .frames = kTargetMemoryFrames,
  });
  return true;
}

// decomp: func_800E4CE8 (src/BATTLE/BATTLE.PRG/7C4E8.c:407).
void ForgetAiActionTarget(TargetMemoryHost& host) {
  host.SetPreviousTile(kForgottenTargetTile);
}

// decomp: func_800DC2E0 (src/BATTLE/BATTLE.PRG/73AE0.s:9).
void CopyActionCandidate(ActionCandidate& destination,
                         const ActionCandidate& source) {
  destination = source;
}

// decomp: func_800DC30C (src/BATTLE/BATTLE.PRG/73B0C.s:9).
std::optional<std::int16_t> ApplySimulatedStatDelta(std::int16_t value,
                                                    std::int16_t maximum,
                                                    std::int32_t delta,
                                                    bool restores) {
  const std::optional<std::int32_t> spent =
      CheckedInt32(std::int64_t{value} - delta);
  const std::optional<std::int32_t> restored =
      CheckedInt32(std::int64_t{value} + delta);
  if (!spent.has_value() || (restores && !restored.has_value())) {
    return Refuse("AI simulated stat delta overflows");
  }
  const std::int32_t next = restores ? *restored : *spent;
  if (next < 0) return std::int16_t{0};
  if (maximum < next) return maximum;
  return static_cast<std::int16_t>(next);
}

// decomp: func_800DC344 (src/BATTLE/BATTLE.PRG/73B0C.s:30).
std::optional<bool> AppendActionCandidate(CandidateList& list,
                                          const ActionCandidate& candidate) {
  const int count = list.Count();
  if (count < 0 || count > static_cast<int>(kCandidateCapacity)) {
    return Refuse("AI candidate count is outside the list");
  }
  if (count == static_cast<int>(kCandidateCapacity) ||
      candidate.Weight() == 0) {
    return false;
  }
  if (!list.ExcludeTargetAction(candidate.TargetId(), candidate.Action())) {
    return Refuse("AI candidate exclusion table is not allocated");
  }
  list.Store(static_cast<std::size_t>(count), candidate);
  list.SetCount(count + 1);
  return true;
}

// decomp: func_800DC3CC (src/BATTLE/BATTLE.PRG/73B0C.s:70);
// partition: func_800E6974 / L800E6958 (src/BATTLE/BATTLE.PRG/7E05C.s:114).
// Equal scores must retain the original midpoint partition's tie ordering.
bool SortActionCandidates(std::span<RankedCandidate> ranking) {
  if (ranking.size() > kCandidateCapacity) {
    Refuse("AI candidate sort exceeds the candidate list");
    return false;
  }
  std::array<Core::SortKey, kCandidateCapacity> keys{};
  for (std::size_t index = 0; index < ranking.size(); ++index) {
    keys.at(index) = {ranking[index].score, index};
  }
  const auto ranked = std::span(keys).first(ranking.size());
  if (!Core::SortPointersByDescendingKey(ranked)) {
    Refuse("AI candidate sort exceeds the shared key sort");
    return false;
  }
  std::array<RankedCandidate, kCandidateCapacity> sorted{};
  for (std::size_t index = 0; index < ranking.size(); ++index) {
    sorted.at(index) = ranking[ranked[index].position];
  }
  std::ranges::copy(std::span(sorted).first(ranking.size()), ranking.begin());
  return true;
}

// decomp: func_800DC3E8 (src/BATTLE/BATTLE.PRG/73B0C.s:81).
void CopyPlanningActorStats(
    std::span<const std::byte, kActorStatsRecordBytes> source,
    std::span<std::byte, kActorStatsRecordBytes> destination) {
  for (const StatsByteRange& range : kPlanningStatSnapshot) {
    // The caller may pass the same record as source and destination.
    std::memmove(destination.subspan(range.offset, range.size).data(),
                 source.subspan(range.offset, range.size).data(), range.size);
  }
}

// decomp: snapshotActorStats (74088.c:1295), original28MIPS in func_800DE3E4.
bool SnapshotPlanningActorStats(PlanningStatsSnapshot& snapshot,
                                std::span<const PlanningActorStats> actors) {
  if (!ValidPlanningStatsRows(snapshot, actors)) {
    Refuse("AI stat capture requires complete rows and a sixteen-slot table");
    return false;
  }
  snapshot.captured_slots.fill(false);
  for (const auto& actor : actors) {
    CopyPlanningActorStats(std::span<const std::byte, kActorStatsRecordBytes>(
                               actor.bytes.data(), kActorStatsRecordBytes),
                           PlanningSnapshotRow(snapshot, actor.slot));
    snapshot.captured_slots[actor.slot] = true;
  }
  return true;
}

// decomp: restoreActorStats (74088.c:1309), original25MIPS in func_800DE3E4.
bool RestorePlanningActorStats(const PlanningStatsSnapshot& snapshot,
                               std::span<const PlanningActorStats> actors) {
  if (!ValidPlanningStatsRows(snapshot, actors) ||
      !std::all_of(actors.begin(), actors.end(),
                   [&snapshot](const auto& actor) {
                     return snapshot.captured_slots[actor.slot];
                   })) {
    Refuse("AI stat restore requires complete previously captured actor rows");
    return false;
  }
  for (const auto& actor : actors) {
    CopyPlanningActorStats(PlanningSnapshotRow(snapshot, actor.slot),
                           std::span<std::byte, kActorStatsRecordBytes>(
                               actor.bytes.data(), kActorStatsRecordBytes));
  }
  return true;
}

// decomp: func_800E4CF4 (src/BATTLE/BATTLE.PRG/7C4F4.s:10).
std::optional<bool> CanAiActionAffectActor(const ActionTargetingHost& host,
                                           const ActionCandidate& candidate,
                                           int actor_id) {
  if (actor_id < 0 || actor_id >= kCandidateTargetSlots) {
    return Refuse("AI target actor ID is outside the AI state table");
  }
  const std::optional<ActorResourceConditions> peer =
      host.PeerConditions(actor_id);
  const unsigned classification = host.Classification(actor_id);
  if (!peer.has_value() || classification < kAlliedClassification) {
    return false;
  }
  if (candidate.Option(CandidateOption::kBeneficial)) {
    if (classification >= kHostileClassification) return false;
    const unsigned conditions = (peer->status_restricted ? 1u : 0u) |
                                (peer->low_mp ? 2u : 0u) |
                                (peer->low_hp ? 4u : 0u);
    const std::uint16_t flags = host.ActionAiFlags(candidate.Action());
    if ((flags & kUnconditionalTargetFlag) == 0 &&
        ((flags >> kTargetConditionShift) & conditions) == 0) {
      return false;
    }
  }
  if (candidate.Option(CandidateOption::kHarmful) &&
      classification < kHostileClassification) {
    return false;
  }
  const auto row = static_cast<int>(candidate.Row());
  const auto column = static_cast<int>(candidate.Column());
  if (!IsSlot(row, column)) {
    return Refuse("AI target eligibility action slot is outside the table");
  }
  return !(host.SlotExcludesSelf(row, column) && actor_id == host.ActorId());
}

// decomp: func_800E4DF8 (src/BATTLE/BATTLE.PRG/7C4F4.s:82).
std::optional<bool> RefreshAiActionUsability(ActionUsabilityHost& host, int row,
                                             int column) {
  if (!IsSlot(row, column)) {
    return Refuse("AI action usability slot is outside the table");
  }
  const unsigned action_id = host.SlotAction(row, column);
  if (action_id >= kActionDefinitionCount) {
    return Refuse("AI action usability action ID is outside the table");
  }
  const ActionCost action = host.Action(action_id);
  const ActorResources resources = host.Resources();
  const bool status_allows =
      (resources.statuses & kSpellsAndAbilitiesOnlyStatus) == 0 ||
      action.type == kSpellAction || action.type == kAbilityAction;
  const bool usable = host.LimbHp(row) >= kMinimumLimbHp && status_allows &&
                      IsActionUsable(action, resources);
  host.SetSlotUsable(row, column, usable);
  return usable;
}

// decomp: func_800E4F14 (src/BATTLE/BATTLE.PRG/7C4F4.s:162).
ActorResourceConditions RefreshAiResourceRestrictions(
    const ActorResources& resources, std::uint8_t action_cost_threshold) {
  const unsigned hp = resources.hp;
  const unsigned mp = resources.mp;
  const bool thresholds_enabled = action_cost_threshold != 0;
  return {
      .status_restricted =
          (resources.statuses & kResourceRestrictingStatuses) != 0 ||
          (thresholds_enabled &&
           (resources.statuses & kSpellBlockingStatus) != 0),
      .low_mp = thresholds_enabled && (mp < unsigned{action_cost_threshold} ||
                                       mp * 2u < unsigned{resources.max_mp}),
      .low_hp = hp * 2u < unsigned{resources.max_hp},
  };
}

// decomp: func_800E5710 (src/BATTLE/BATTLE.PRG/7CD68.c:526).
bool BuildAiActionMetadata(ActionMetadataHost& host) {
  if (host.ActorId() == 0) {
    host.SetActionCostThreshold(kPlayerActionCostThreshold);
    return true;
  }
  for (int row = 0; row < kLimbActionRows; ++row) {
    for (int column = 0; column < kLimbActionColumns; ++column) {
      const unsigned action_id = host.SlotAction(row, column);
      if (action_id >= kActionDefinitionCount) {
        Refuse("AI action metadata action ID is outside the table");
        return false;
      }
      const MetadataAction action = host.Action(action_id);
      const std::uint32_t category = std::uint32_t{1}
                                     << (action.category & kActionCategoryMask);
      ActionSlotMetadata slot = host.Slot(row, column);
      slot.range_squared = host.RangeSquared(action_id);
      const std::int32_t maximum_reach = Core::Math::SquareRootInteger(
          std::bit_cast<std::int32_t>(slot.range_squared));
      slot.radius = static_cast<std::uint16_t>(host.AreaReach(
          action_id, std::bit_cast<std::uint32_t>(maximum_reach)));
      slot.long_range =
          slot.range_squared > kLongRangeSquared && !action.radial_area;
      if ((category & kSelfExcludingCategories) != 0) {
        slot.excludes_self = true;
      }
      if ((category & kObstructionIgnoringCategories) != 0) {
        slot.ignores_obstruction = true;
      }
      if ((category & kDirectCategories) != 0) {
        slot.kind = CandidateEnumerationKind::kDirect;
      } else if ((category & kLimbCategories) != 0) {
        slot.kind = CandidateEnumerationKind::kLimb;
      } else if ((category & kAimedCategories) != 0) {
        slot.kind = CandidateEnumerationKind::kAimed;
      }
      slot.fallback_eligible = host.RoomAllowsNonSpellActions() &&
                               action.type != kSpellAction &&
                               action.type != kBreakArtAction &&
                               (category & kFallbackCategories) != 0;
      host.SetSlot(row, column, slot);
      if (action.type == kSpellAction &&
          host.ActionCostThreshold() < action.cost) {
        host.SetActionCostThreshold(action.cost);
      }
    }
  }
  return true;
}
}  // namespace VS::Battle::Ai
