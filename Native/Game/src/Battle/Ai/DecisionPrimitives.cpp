// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_helpers.c, battle_movement.c and
// battle_gte.c, revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/DecisionPrimitives.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>

#include "Core/Diagnostics.h"

namespace VS::Battle::Ai {
namespace {
constexpr unsigned kHostileClassification = 3;
constexpr unsigned kInvertedTargetingAction = 0x60;
constexpr unsigned kInvertedTargetingClass = 2;
constexpr unsigned kHarmfulFlagBit = 15;
constexpr unsigned kBeneficialFlagBit = 14;
constexpr unsigned kPromotionRollMask = 0x7F;
constexpr unsigned kMaximumPromotionWeight = 0x4C;
constexpr std::uint32_t kStatusWeight = 6;
constexpr std::int64_t kHpWeight = 4;
constexpr std::int64_t kDpWeight = 2;

std::nullopt_t Refuse(const char* message) {
  Core_Debugf("ai", "%s", message);
  return std::nullopt;
}

std::optional<std::int32_t> CheckedInt32(std::int64_t value) {
  if (value < std::numeric_limits<std::int32_t>::min() ||
      value > std::numeric_limits<std::int32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(value);
}

unsigned TargetingFlag(unsigned action, std::uint16_t ai_flags,
                       unsigned enemy_class, unsigned bit) {
  const unsigned flags = static_cast<unsigned>(ai_flags) >> bit;
  if (action == kInvertedTargetingAction &&
      enemy_class == kInvertedTargetingClass) {
    return flags ^ 1u;
  }
  return flags & 1u;
}

int SquaredTileDistance(GridTile a, GridTile b) {
  const int dx = int{a.x} - int{b.x};
  const int dz = int{a.z} - int{b.z};
  return (dx * dx) + (dz * dz);
}

// The candidate's minimum range keeps the target out of reach: the squared
// range is the original's wrapping 32-bit product read as signed.
bool InsideMinimumRange(const ActionCandidate& candidate,
                        std::int32_t distance_squared) {
  const std::uint32_t range = candidate.Range();
  if (range == 0 || !candidate.Option(CandidateOption::kMinimumRange)) {
    return false;
  }
  return distance_squared < std::bit_cast<std::int32_t>(range * range);
}

// The gimmick gate: in range, the actor not on the room delay, and a hostile
// actor next to it.
std::optional<bool> CanActOnGimmick(const ActionExecutionHost& host,
                                    unsigned target,
                                    std::int32_t distance_squared) {
  if (distance_squared > kGimmickActionRangeSquared ||
      host.Cooldown().uses_room_action_delay) {
    return false;
  }
  const std::optional<bool> hostile_near = host.HostileNearGimmick(target);
  if (!hostile_near.has_value()) {
    return Refuse("AI execution check cannot read the gimmick's neighbours");
  }
  return *hostile_near;
}
}  // namespace

// decomp: func_800DBC60 (src/BATTLE/BATTLE.PRG/73460.s:8).
bool DoesActionRequireCooldown(const ActionCooldown& cooldown,
                               const ActionCandidate& candidate,
                               bool target_is_gimmick) {
  return cooldown.uses_room_action_delay ||
         (candidate.Option(CandidateOption::kHarmful) && !target_is_gimmick);
}

// decomp: func_800DBCB4 (src/BATTLE/BATTLE.PRG/73460.s:38).
bool IsActionCooldownReady(const ActionCooldown& cooldown,
                           const ActionCandidate& candidate,
                           bool target_is_gimmick) {
  return cooldown.action_delay == 0 &&
         (!DoesActionRequireCooldown(cooldown, candidate, target_is_gimmick) ||
          cooldown.minimum_action_delay == 0);
}

// decomp: func_800DBCEC (src/BATTLE/BATTLE.PRG/73460.s:59).
// The original starts its walk at the actor pointer array itself, reading the
// second array entry as an actor ID; only the listed actors are checked here.
std::optional<bool> HasNearbyEnemy(GridTile gimmick,
                                   std::span<const ActorProximity> actors) {
  for (const ActorProximity& actor : actors) {
    if (actor.classification < kHostileClassification) continue;
    if (!actor.tile.has_value()) {
      return Refuse("AI proximity check requires the hostile actor's model");
    }
    if (SquaredTileDistance(*actor.tile, gimmick) <
        kGimmickNeighborTilesSquared) {
      return true;
    }
  }
  return false;
}

// decomp: func_800DBD80 (src/BATTLE/BATTLE.PRG/73580.s:11).
std::optional<bool> CanExecuteActionCandidate(
    const ActionExecutionHost& host, const ActionCandidate& candidate) {
  const unsigned target = candidate.TargetId();
  const bool target_is_gimmick = host.TargetIsGimmick(target);
  if (candidate.Weight() == 0 ||
      unsigned{host.ReadinessLevel()} < candidate.Threshold() ||
      !IsActionCooldownReady(host.Cooldown(), candidate, target_is_gimmick)) {
    return false;
  }
  const std::int32_t distance = host.DistanceSquared(target);
  if (target_is_gimmick) {
    const std::optional<bool> gimmick_ready =
        CanActOnGimmick(host, target, distance);
    if (!gimmick_ready.has_value() || !*gimmick_ready) return gimmick_ready;
  }
  if (candidate.DistanceSquared() < distance ||
      InsideMinimumRange(candidate, distance)) {
    return false;
  }
  const TargetRelation relation = host.Relation(target);
  if (!relation.ready) return false;
  const std::optional<bool> ashley_moving = host.AshleyMoving();
  if (!ashley_moving.has_value()) {
    return Refuse("AI execution check requires Ashley's model");
  }
  if (*ashley_moving || !(candidate.Option(CandidateOption::kIgnoresSight) ||
                          relation.in_sight)) {
    return false;
  }
  return candidate.Option(CandidateOption::kIgnoresObstruction) ||
         !relation.obstructed;
}

// decomp: func_800DC284 (src/BATTLE/BATTLE.PRG/7399C.s:79).
// The GTE sums the halfword terms exactly; only the final `add` can trap.
std::optional<std::int32_t> ScoreActorCondition(
    const ConditionScoreTerms& terms) {
  const auto weighted_status = std::bit_cast<std::int32_t>(
      std::bit_cast<std::uint32_t>(terms.status) * kStatusWeight);
  const std::int64_t resources = (std::int64_t{terms.hp} * kHpWeight) +
                                 terms.mp +
                                 (std::int64_t{terms.dp} * kDpWeight);
  const std::optional<std::int32_t> score =
      CheckedInt32(std::int64_t{weighted_status} + resources);
  if (!score.has_value()) {
    return Refuse("AI condition score overflows its signed range");
  }
  return score;
}

// decomp: func_800DC484 (src/BATTLE/BATTLE.PRG/73C84.s:10).
unsigned IsActionHarmfulToActor(unsigned action, std::uint16_t ai_flags,
                                unsigned enemy_class) {
  return TargetingFlag(action, ai_flags, enemy_class, kHarmfulFlagBit);
}

// decomp: func_800DC48C (src/BATTLE/BATTLE.PRG/73C84.s:16).
unsigned IsActionBeneficialToActor(unsigned action, std::uint16_t ai_flags,
                                   unsigned enemy_class) {
  return TargetingFlag(action, ai_flags, enemy_class, kBeneficialFlagBit);
}

// decomp: func_800DC4F0 (src/BATTLE/BATTLE.PRG/73C84.s:49).
std::optional<bool> CacheFallbackActionCandidate(
    FallbackCandidateCache& cache, const ActionCandidate& candidate,
    bool fallback_eligible) {
  if (!fallback_eligible) return false;
  if (candidate.Row() >= static_cast<unsigned>(kLimbActionRows)) {
    return Refuse("AI fallback action slot is outside the limb table");
  }
  const std::uint32_t bit = std::uint32_t{1}
                            << ((candidate.Row() * kLimbActionColumns) +
                                candidate.Column());
  const std::uint32_t mask = cache.SlotMask();
  if ((mask & bit) != 0) return false;
  const int count = cache.Count();
  if (count < 0 || count > static_cast<int>(kFallbackCandidateCapacity)) {
    return Refuse("AI fallback cache count is outside the cache");
  }
  cache.SetSlotMask(mask | bit);
  if (count == static_cast<int>(kFallbackCandidateCapacity)) return false;
  if (!cache.Store(static_cast<std::size_t>(count), candidate)) {
    return Refuse("AI fallback cache storage is not allocated");
  }
  cache.SetCount(count + 1);
  return true;
}

// decomp: func_800DC574 (src/BATTLE/BATTLE.PRG/73C84.s:89).
// The original draws once even for an empty ranking; the only caller passes
// at least one candidate, so an empty ranking draws nothing here.
std::optional<bool> PromoteRandomizedActionCandidate(
    std::span<RankedCandidate> ranking, CandidateRandom& random) {
  if (ranking.size() > kCandidateCapacity) {
    return Refuse("AI candidate promotion exceeds the candidate list");
  }
  for (std::size_t index = 0; index < ranking.size(); ++index) {
    const unsigned roll =
        static_cast<unsigned>(random.Next()) & kPromotionRollMask;
    const RankedCandidate& candidate = ranking[index];
    if (candidate.action != 0 &&
        roll < std::min(candidate.weight, kMaximumPromotionWeight)) {
      std::swap(ranking.front(), ranking[index]);
      if (!SortActionCandidates(ranking.subspan(1))) {
        return Refuse("AI candidate promotion cannot sort the ranking");
      }
      return true;
    }
  }
  return false;
}

// decomp: func_800E45D4 (src/BATTLE/BATTLE.PRG/7BDD4.s:10).
std::optional<int> NextActorRandomMasked(ActorRandomState& state, int range) {
  if (range <= 0) return Refuse("AI random mask requires a positive range");
  const auto current = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(state.accumulator));
  const std::optional<std::int32_t> next =
      CheckedInt32(std::int64_t{current} + state.step);
  if (!next.has_value()) {
    return Refuse("AI random accumulator overflows its signed range");
  }
  state.accumulator = std::bit_cast<std::uint32_t>(*next);
  return static_cast<int>(state.accumulator &
                          (static_cast<std::uint32_t>(range) - 1));
}

// decomp: func_800E4FB0 (src/BATTLE/BATTLE.PRG/7C7B0.s:8).
// The original's range adjustment for one actor kind never reaches the
// comparison.
bool IsAiTargetBeyondNearRadius(std::uint32_t distance_squared) {
  return distance_squared > kNearTargetDistanceSquared;
}

// decomp: func_800DB8AC (src/BATTLE/BATTLE.PRG/72B70.s:439).
// The original also calls func_800DBCB4 and discards its result.
std::optional<bool> CanApproachEngagedTarget(int actor_id,
                                             const EngagementTimers& timers,
                                             const EngagementRoster* roster,
                                             bool target_active) {
  if (actor_id < 0 || actor_id >= kEngagementActorSlots) {
    return Refuse("AI approach gate actor ID is outside the engaged mask");
  }
  if (timers.engaged_frames != 0) return true;
  if (timers.cooldown_frames != 0) return false;
  if (roster == nullptr) {
    return Refuse("AI approach gate requires the room roster");
  }
  if (!target_active ||
      std::uint32_t{roster->limit} <
          std::bit_cast<std::uint32_t>(roster->approach_moves)) {
    return false;
  }
  if ((roster->engaged_mask & (1u << static_cast<unsigned>(actor_id))) != 0) {
    return true;
  }
  return roster->limit >= roster->engaged_count;
}
}  // namespace VS::Battle::Ai
