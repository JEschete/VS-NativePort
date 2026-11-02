#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Ai/Engagement.hpp"
#include "Battle/Ai/PlanningPrimitives.hpp"

// The battle AI's decision gates: whether a planned candidate may run now,
// how it ranks against the others, and the per-actor random stream. The
// battle runtime owns the actor AI state, the room roster and the actor and
// gimmick tables; these routines see them only through the value records and
// hosts below.
namespace VS::Battle::Ai {
// Fallback candidates the planner keeps (BATTLE.PRG 0x800F58F0 count).
inline constexpr std::size_t kFallbackCandidateCapacity = 16;
inline constexpr std::int32_t kGimmickActionRangeSquared = 0x19000;
// Squared tile distance below which an actor counts as next to a gimmick.
inline constexpr int kGimmickNeighborTilesSquared = 3;
inline constexpr std::uint32_t kNearTargetDistanceSquared = 100;

// The deciding actor's action delays (AI state 0x13, 0x90 and 0x13F).
struct ActionCooldown {
  bool uses_room_action_delay = false;
  std::uint16_t action_delay = 0;
  std::uint8_t minimum_action_delay = 0;
};

// A map tile column.
struct GridTile {
  std::uint8_t x = 0;
  std::uint8_t z = 0;
};

// A battle actor's relation classification from the deciding actor and its
// model's tile; nothing when the actor has no model.
struct ActorProximity {
  unsigned classification = 0;
  std::optional<GridTile> tile;
};

// The relation bits the execution gate reads (AI state 0x1A4 words).
struct TargetRelation {
  bool ready = false;
  bool in_sight = false;
  bool obstructed = false;
};

// The room, the deciding actor's AI state and Ashley's model as the
// execution gate reads them. Targets are below kCandidateTargetSlots.
class ActionExecutionHost {
 public:
  virtual ~ActionExecutionHost() = default;
  [[nodiscard]] virtual std::uint16_t ReadinessLevel() const = 0;
  [[nodiscard]] virtual ActionCooldown Cooldown() const = 0;
  [[nodiscard]] virtual bool TargetIsGimmick(unsigned target) const = 0;
  [[nodiscard]] virtual std::int32_t DistanceSquared(unsigned target) const = 0;
  [[nodiscard]] virtual TargetRelation Relation(unsigned target) const = 0;
  // HasNearbyEnemy for the target gimmick; nothing when it refuses.
  [[nodiscard]] virtual std::optional<bool> HostileNearGimmick(
      unsigned target) const = 0;
  // Ashley is in a movement mode or stepping; nothing without her model.
  [[nodiscard]] virtual std::optional<bool> AshleyMoving() const = 0;
};

// The planner's fallback candidate cache and its slot mask.
class FallbackCandidateCache {
 public:
  virtual ~FallbackCandidateCache() = default;
  [[nodiscard]] virtual std::uint32_t SlotMask() const = 0;
  virtual void SetSlotMask(std::uint32_t mask) = 0;
  [[nodiscard]] virtual int Count() const = 0;
  // False when the cache storage is not allocated.
  [[nodiscard]] virtual bool Store(std::size_t index,
                                   const ActionCandidate& candidate) = 0;
  virtual void SetCount(int count) = 0;
};

class CandidateRandom {
 public:
  virtual ~CandidateRandom() = default;
  [[nodiscard]] virtual int Next() = 0;
};

// The AI frame's random accumulator and the step added per draw (BATTLE.PRG
// scratch 0x1F8003F8 and 0x1F8003FC).
struct ActorRandomState {
  std::uint32_t accumulator = 0;
  std::int32_t step = 0;
};

// decomp: func_800DBC60 (src/BATTLE/BATTLE.PRG/73460.s:8).
// The action waits for the actor's minimum delay when the actor uses the
// room's delay, or when a harmful action targets a slot without a gimmick.
[[nodiscard]] bool DoesActionRequireCooldown(const ActionCooldown& cooldown,
                                             const ActionCandidate& candidate,
                                             bool target_is_gimmick);

// decomp: func_800DBCB4 (src/BATTLE/BATTLE.PRG/73460.s:38).
[[nodiscard]] bool IsActionCooldownReady(const ActionCooldown& cooldown,
                                         const ActionCandidate& candidate,
                                         bool target_is_gimmick);

// decomp: func_800DBCEC (src/BATTLE/BATTLE.PRG/73460.s:59).
// Whether a hostile battle actor stands next to the gimmick's tile. The
// original returns zero in that case. Nothing refuses a hostile actor without
// a model.
[[nodiscard]] std::optional<bool> HasNearbyEnemy(
    GridTile gimmick, std::span<const ActorProximity> actors);

// decomp: func_800DBD80 (src/BATTLE/BATTLE.PRG/73580.s:11).
// Whether the candidate may run now: it is weighted, the room is ready, its
// delay has passed, the target is in range and sight, and Ashley is standing.
// Nothing refuses a gate the host cannot answer.
[[nodiscard]] std::optional<bool> CanExecuteActionCandidate(
    const ActionExecutionHost& host, const ActionCandidate& candidate);

// The terms of an actor's condition score: its status score and its HP, MP
// and DP ratings.
struct ConditionScoreTerms {
  std::int32_t status = 0;
  std::int16_t hp = 0;
  std::int16_t mp = 0;
  std::int16_t dp = 0;
};

// decomp: func_800DC284 (src/BATTLE/BATTLE.PRG/7399C.s:79).
// Six times the status score (wrapping) plus four times HP, MP and twice DP.
// Nothing refuses a total outside the signed 32-bit range, where the
// original's `add` traps.
[[nodiscard]] std::optional<std::int32_t> ScoreActorCondition(
    const ConditionScoreTerms& terms);

// decomp: func_800DC484 (src/BATTLE/BATTLE.PRG/73C84.s:10).
// The action's harmful AI flag (bit 15), inverted for action 0x60 against an
// enemy of class 2.
[[nodiscard]] unsigned IsActionHarmfulToActor(unsigned action,
                                              std::uint16_t ai_flags,
                                              unsigned enemy_class);

// decomp: func_800DC48C (src/BATTLE/BATTLE.PRG/73C84.s:16).
// The action's beneficial AI flag (bit 14). For action 0x60 against an enemy
// of class 2 it is the flags above bit 13 with bit 14 inverted, which can be 2
// or 3 when the harmful bit is also set.
[[nodiscard]] unsigned IsActionBeneficialToActor(unsigned action,
                                                 std::uint16_t ai_flags,
                                                 unsigned enemy_class);

// decomp: func_800DC4F0 (src/BATTLE/BATTLE.PRG/73C84.s:49).
// Caches the first candidate for each limb action slot whose action may serve
// as a fallback, recording the slot even when the cache is full. Nothing
// refuses a slot outside the limb table, a count outside the cache or missing
// storage.
std::optional<bool> CacheFallbackActionCandidate(
    FallbackCandidateCache& cache, const ActionCandidate& candidate,
    bool fallback_eligible);

// decomp: func_800DC574 (src/BATTLE/BATTLE.PRG/73C84.s:89).
// Rolls each ranked candidate against its weight (at most 0x4C of 0x80) and
// promotes the first winner with an action to the front, re-sorting the rest.
// Nothing refuses more than kCandidateCapacity entries.
std::optional<bool> PromoteRandomizedActionCandidate(
    std::span<RankedCandidate> ranking, CandidateRandom& random);

// decomp: func_800E45D4 (src/BATTLE/BATTLE.PRG/7BDD4.s:10).
// Advances the accumulator by the step and masks it with `range - 1`. Nothing
// refuses a range below 1 or a sum outside the signed 32-bit range, where the
// original's `add` traps.
std::optional<int> NextActorRandomMasked(ActorRandomState& state, int range);

// decomp: func_800E4FB0 (src/BATTLE/BATTLE.PRG/7C7B0.s:8).
[[nodiscard]] bool IsAiTargetBeyondNearRadius(std::uint32_t distance_squared);

// decomp: func_800DB8AC (src/BATTLE/BATTLE.PRG/72B70.s:439).
// Whether the actor may approach its target: it is already engaged, or it is
// rested, the target is active and the room has an engagement slot for it.
// The original reads the room only for a rested actor, so `roster` may be
// null otherwise. Nothing refuses an ID outside the engaged mask or a missing
// roster that is needed.
[[nodiscard]] std::optional<bool> CanApproachEngagedTarget(
    int actor_id, const EngagementTimers& timers,
    const EngagementRoster* roster, bool target_active);
}  // namespace VS::Battle::Ai
