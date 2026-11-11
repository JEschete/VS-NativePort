// Hand-maintained (tools/port/owned.csv).
// AI perception: target relations and the per-frame actor perception pass.
#include "Battle/Ai/ActorPerception.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Battle/Ai/BlockedMovement.hpp"
#include "Battle/Ai/MovementBehavior.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
using Core::Math::Position3;

constexpr std::uint32_t kOneBit = 1;
constexpr std::uint32_t kTwoBits = 3;
constexpr std::uint32_t kFiveBits = 0x1F;
constexpr std::uint32_t kSevenBits = 0x7F;
constexpr int kWasActiveBit = 1;
constexpr int kPointHeightBit = 2;
constexpr int kInViewBit = 3;
constexpr int kReservedBit = 4;
constexpr int kVisibleBit = 5;
constexpr int kClassificationShift = 6;
constexpr int kCounterShift = 8;
constexpr int kTimerShift = 15;
constexpr int kClassificationTimerShift = 20;
constexpr int kBlockedBit = 27;
constexpr int kPreviousClassificationShift = 28;
constexpr int kBodyHeightBit = 30;
constexpr int kMarkedBit = 31;

// Tile words compare x (byte 0) and z (byte 2) only.
constexpr std::uint32_t kTileMask = 0xFF00FF;
// The height used for a target without an AI state, and the frame-shape
// defaults for line of sight and occlusion.
constexpr int kNoStateHeight = -64;
constexpr int kDefaultBodyExtent = 128;
constexpr int kDefaultPointHeight = 64;
constexpr std::uint16_t kDefaultRadius = 63;
constexpr int kQ12Shift = 12;
constexpr std::uint8_t kHostileClassification = 3;
constexpr std::uint8_t kNeutralClassification = 1;
constexpr std::uint8_t kHostilityDecayFrames = 121;

bool Bit(std::uint32_t word, int bit) { return ((word >> bit) & kOneBit) != 0; }
std::uint8_t BitField(std::uint32_t word, int shift, std::uint32_t mask) {
  return static_cast<std::uint8_t>((word >> shift) & mask);
}
std::uint32_t Put(bool value, int bit) { return (value ? kOneBit : 0U) << bit; }
std::uint32_t Put(std::uint8_t value, int shift, std::uint32_t mask) {
  return (value & mask) << shift;
}

bool SameTile(std::uint32_t first, std::uint32_t second) {
  return ((first ^ second) & kTileMask) == 0;
}

std::uint32_t Magnitude(std::int32_t value) {
  const auto bits = static_cast<std::uint32_t>(value);
  return value < 0 ? 0U - bits : bits;
}

// Whether the target shares the observer's tile closely enough that the
// line-of-sight checks are skipped.
bool SharesObserverTile(const TargetPerceptionWorld& world,
                        const PerceivingObserver& observer, int target,
                        const PerceivedModel& model) {
  return world.HasGimmick(target) || SameTile(model.tile, observer.tile) ||
         (model.locomoting && SameTile(model.last_tile, observer.tile));
}

// The lines of sight: both clear on a shared tile, otherwise one height per
// call, alternating with the observer's phase.
void UpdateLineOfSight(const TargetPerceptionWorld& world,
                       PerceivingObserver& observer, int target,
                       const PerceivedModel& model) {
  PerceivedRelation& relation = observer.relations.at(target);
  if (SharesObserverTile(world, observer, target, model)) {
    relation.clear_at_point_height = true;
    relation.clear_at_body_height = true;
    return;
  }
  ++observer.phase;
  if ((observer.phase & 1U) != 0) {
    relation.clear_at_body_height =
        !IsTargetOccludedAtBodyHeight(world, observer, target, model.position);
  } else {
    relation.clear_at_point_height =
        !IsTargetOccludedAtPointHeight(world, observer, target, model.position);
  }
  if (relation.clear_at_point_height) relation.clear_at_body_height = true;
}

// Activity, the remembered-activity timer and visibility for another actor.
void UpdateActivity(const PerceivingObserver& observer, int target,
                    PerceivedRelation& relation) {
  const std::uint32_t distance = observer.distances_squared.at(target);
  const bool was_active = relation.active;
  relation.active =
      distance <= observer.far_distance_squared && relation.in_view &&
      (observer.ignores_body_line || relation.clear_at_body_height);
  if (relation.timer != 0) {
    --relation.timer;
    relation.active = true;
  }
  relation.was_active = !relation.active && was_active;
  relation.visible =
      relation.active || (distance < observer.near_distance_squared &&
                          relation.clear_at_body_height);
}

// Whether an actor nearer than the target stands across the line to it.
// Nothing refuses a listed actor outside the slots or without a model.
std::optional<bool> IsTargetBlockedByActor(const TargetPerceptionWorld& world,
                                           const PerceivingObserver& observer,
                                           int target,
                                           Position3 observer_position,
                                           Position3 target_position) {
  std::uint16_t radius = kDefaultRadius;
  if (const auto shape = world.FrameShape(target)) radius = shape->radius;
  const int x = observer_position.x;
  const int z = observer_position.z;
  const std::uint32_t target_distance = observer.distances_squared.at(target);
  const int heading =
      Core::Math::Arctangent2(target_position.x - x, target_position.z - z);
  for (const ListedPerceptionActor& listed : world.ListedActors()) {
    const int candidate = listed.id;
    if (candidate == observer.actor_id || candidate == target) continue;
    if (candidate < 0 || candidate >= kPerceptionSlots) return std::nullopt;
    const std::uint32_t distance = observer.distances_squared.at(candidate);
    if (distance > target_distance) continue;
    if (!listed.position) return std::nullopt;
    const int angle = Core::Math::Arctangent2(listed.position->x - x,
                                              listed.position->z - z) -
                      heading;
    if (Core::Math::Cosine(angle) < 0) continue;
    const auto root = static_cast<std::uint32_t>(
        Core::Math::SquareRootInteger(static_cast<std::int32_t>(distance)));
    const auto offset = static_cast<std::int32_t>(
        root * static_cast<std::uint32_t>(Core::Math::Sine(angle)));
    if (Magnitude(offset >> kQ12Shift) < radius) return true;
  }
  return false;
}

// Classification timers and the hostility decay of an unseen hostile target.
void DecayClassification(const PerceivingObserver& observer,
                         PerceivedRelation& relation) {
  if (relation.classification_timer != 0) {
    --relation.classification_timer;
    if (relation.classification_timer == 0) {
      relation.classification = relation.previous_classification;
    }
  }
  if (!observer.decays_hostility ||
      relation.classification != kHostileClassification) {
    return;
  }
  if (relation.active) {
    relation.counter = 0;
    return;
  }
  relation.counter =
      static_cast<std::uint8_t>((relation.counter + 1U) & kSevenBits);
  if (relation.counter >= kHostilityDecayFrames) {
    relation.classification = kNeutralClassification;
  }
}

// The height the target is measured at: its body height when it is moving
// above the observer, otherwise its point height.
int TargetHeight(const TargetPerceptionWorld& world,
                 const PerceivingObserver& observer, int target,
                 const PerceivedModel& model) {
  const auto shape = world.AiStateShape(target);
  if (!shape) return kNoStateHeight;
  if (model.locomoting && model.position.y > observer.position.y) {
    return -shape->body_height;
  }
  return shape->point_height;
}
}  // namespace

// decomp: actorRelation (src/BATTLE/BATTLE.PRG/6FC00.c:608).
PerceivedRelation DecodePerceivedRelation(std::uint32_t word) {
  return PerceivedRelation{
      .active = Bit(word, 0),
      .was_active = Bit(word, kWasActiveBit),
      .clear_at_point_height = Bit(word, kPointHeightBit),
      .in_view = Bit(word, kInViewBit),
      .reserved4 = Bit(word, kReservedBit),
      .visible = Bit(word, kVisibleBit),
      .classification = BitField(word, kClassificationShift, kTwoBits),
      .counter = BitField(word, kCounterShift, kSevenBits),
      .timer = BitField(word, kTimerShift, kFiveBits),
      .classification_timer =
          BitField(word, kClassificationTimerShift, kSevenBits),
      .blocked = Bit(word, kBlockedBit),
      .previous_classification =
          BitField(word, kPreviousClassificationShift, kTwoBits),
      .clear_at_body_height = Bit(word, kBodyHeightBit),
      .marked = Bit(word, kMarkedBit)};
}

std::uint32_t EncodePerceivedRelation(const PerceivedRelation& relation) {
  return Put(relation.active, 0) | Put(relation.was_active, kWasActiveBit) |
         Put(relation.clear_at_point_height, kPointHeightBit) |
         Put(relation.in_view, kInViewBit) |
         Put(relation.reserved4, kReservedBit) |
         Put(relation.visible, kVisibleBit) |
         Put(relation.classification, kClassificationShift, kTwoBits) |
         Put(relation.counter, kCounterShift, kSevenBits) |
         Put(relation.timer, kTimerShift, kFiveBits) |
         Put(relation.classification_timer, kClassificationTimerShift,
             kSevenBits) |
         Put(relation.blocked, kBlockedBit) |
         Put(relation.previous_classification, kPreviousClassificationShift,
             kTwoBits) |
         Put(relation.clear_at_body_height, kBodyHeightBit) |
         Put(relation.marked, kMarkedBit);
}

// decomp: checkExtent (src/BATTLE/BATTLE.PRG/6FC00.c:646).
bool IsTargetOccludedAtBodyHeight(const TargetPerceptionWorld& world,
                                  const PerceivingObserver& observer,
                                  int target, Position3 target_position) {
  const auto shape = world.FrameShape(target);
  const int extent = shape ? shape->body_height : kDefaultBodyExtent;
  return world.IsTerrainSegmentBlocked(target_position, observer.position,
                                       extent, observer.body_height);
}

// decomp: checkHeight (src/BATTLE/BATTLE.PRG/6FC00.c:660).
bool IsTargetOccludedAtPointHeight(const TargetPerceptionWorld& world,
                                   const PerceivingObserver& observer,
                                   int target, Position3 target_position) {
  const auto shape = world.FrameShape(target);
  const int height = shape ? std::abs(static_cast<int>(shape->point_height))
                           : kDefaultPointHeight;
  return world.IsTerrainSegmentBlocked(
      target_position, observer.position, height,
      std::abs(static_cast<int>(observer.point_height)));
}

// decomp: func_800D87E8 (src/BATTLE/BATTLE.PRG/6FC00.c:679).
bool UpdateTargetPerception(TargetPerceptionWorld& world,
                            PerceivingObserver& observer, int target) {
  if (target < 0 || target >= kPerceptionSlots) return false;
  const auto model = world.Model(target);
  const auto pose = world.ObserverModel();
  if (!model || !pose) return false;

  observer.distances_squared.at(target) =
      static_cast<std::uint32_t>(Core::Math::SquaredDistanceWithHeightOffsets(
          observer.position, model->position, observer.point_height,
          TargetHeight(world, observer, target, *model)));
  UpdateLineOfSight(world, observer, target, *model);

  PerceivedRelation& relation = observer.relations.at(target);
  relation.in_view =
      TargetFacingCosine(observer.position, model->position,
                         pose->facing + pose->look_yaw,
                         observer.distances_squared.at(target)) >=
      static_cast<std::int32_t>(observer.view_cosine);
  if (observer.actor_id == target) {
    relation.active = true;
    relation.visible = true;
    relation.was_active = false;
  } else {
    UpdateActivity(observer, target, relation);
  }
  const auto marked =
      world.IsMarkedTile(Core::Math::UnpackTileCoordinate(model->tile));
  if (!marked) return false;
  relation.marked = *marked || relation.active;

  const auto blocked = IsTargetBlockedByActor(world, observer, target,
                                              pose->position, model->position);
  if (!blocked) return false;
  relation.blocked = *blocked;
  if (observer.actor_id != target && world.FrameShape(target)) {
    DecayClassification(observer, relation);
  }
  return true;
}

// decomp: func_800D8EF4 (src/BATTLE/BATTLE.PRG/6FC00.c:1003).
void BeginActorPerception(PerceptionRoom& room) {
  constexpr std::uint16_t kReadinessCeiling = 0xFFFE;
  room.candidate_engaged = 0;
  room.approach_moves = 0;
  if (room.readiness_level < kReadinessCeiling) ++room.readiness_level;
}

namespace {
// Command states the pass stops at.
constexpr int kDisengagedCommand = 0x20;
constexpr int kHeldCommand = 0x80;
constexpr int kScriptedCommand = 0x40;
// An observed command that never triggers a reaction.
constexpr std::uint8_t kNoCommand = 8;
// Commands whose positions feed the movement history: 9, 10, 13 and 16.
constexpr std::uint32_t kHistoryCommands = 0x12600;
constexpr std::uint32_t kShiftMask = 31;
constexpr int kReaction = 1;

// Counts a running timer down; true when it reaches zero on this frame.
template <typename Frames>
bool CountDownExpires(Frames& frames) {
  if (frames == 0) return false;
  --frames;
  return frames == 0;
}
constexpr int kReactionDelay = 64;
constexpr std::uint16_t kAwarenessAlertMarker = 6;
constexpr std::uint16_t kStallFrames = 0xA8C;
constexpr std::uint8_t kNoDetourTarget = 16;
constexpr std::uint8_t kDetourStepFrames = 40;
constexpr std::uint8_t kDetourSteps = 5;
constexpr std::uint8_t kRecoveryFrames = 30;
constexpr int kHistorySamples = 8;
constexpr int kSamplesPerWord = 4;
constexpr int kBitsPerSample = 8;
constexpr std::uint32_t kSampleMask = 0xFF;
constexpr int kFineTileShift = 6;
constexpr int kTileZShift = 16;
constexpr int kHeldTargetShift = 24;
constexpr std::uint8_t kLowHpRule = 1;
constexpr std::uint8_t kHalfHpRule = 2;
constexpr int kLowHpFactor = 4;
constexpr int kHalfHpFactor = 2;
constexpr std::uint8_t kFleeingStatus = 2;

enum class Step { kContinue, kIdle, kSkip, kRefused };

// Disengagement, held and scripted commands, a moving, running or absent
// perceiver, and the forced partial perception.
Step RunCommandGates(ActorPerceptionWorld& world, PerceivingActor& actor,
                     int flags) {
  if ((flags & kDisengagedCommand) != 0) {
    world.Disengage(actor);
    world.ResetAwarenessIndicator(actor);
    world.UnlinkEngagedActor(actor);
    return Step::kSkip;
  }
  world.UpdateTemporaryAwareness(actor);
  world.TickDecisionTimers(actor);
  if ((flags & kHeldCommand) != 0) return Step::kSkip;
  world.UpdateEngagementBuildup(actor);
  if ((flags & kScriptedCommand) != 0 || actor.has_navigation_override) {
    return Step::kSkip;
  }
  const auto model = world.Model();
  if (!model) return Step::kRefused;
  if (model->moving || model->running || actor.battle.id == 0) {
    return Step::kSkip;
  }
  if (model->forces_perception) {
    actor.partial_perception = 1;
    if (!world.UpdateTargetPerception(actor, 0)) return Step::kRefused;
    return Step::kSkip;
  }
  return Step::kContinue;
}

// The frame budget, a held alert and movement recovery.
Step RunFrameGates(ActorPerceptionWorld& world, PerceivingActor& actor) {
  const std::int32_t elapsed = world.ElapsedFrameTicks();
  if (world.FrameTickBudget() < elapsed) return Step::kIdle;
  world.SetLastBudgetTick(elapsed);
  if (actor.alerted != 0) {
    if (actor.awareness_frames_remaining != 0 &&
        actor.awareness_marker == kAwarenessAlertMarker) {
      return Step::kIdle;
    }
    actor.alerted = 0;
  }
  if (actor.recovery_frames != 0) {
    --actor.recovery_frames;
    world.RecoverMovement(actor);
    if (!world.UpdateTargetPerception(actor, 0)) return Step::kRefused;
    actor.partial_perception = 1;
    if (actor.stall_frames >= 2) --actor.stall_frames;
    return Step::kIdle;
  }
  return Step::kContinue;
}

// The stall and detour timers while the actor stays on its linked tile, and
// the stall reset when it leaves.
Step TrackTile(ActorPerceptionWorld& world, PerceivingActor& actor) {
  actor.perception_skipped = 0;
  const auto linked = world.LinkedTile();
  if (!linked) return Step::kRefused;
  if (!SameTile(*linked, actor.tile)) {
    actor.stall_frames = kStallFrames;
    actor.stayed_on_tile = 0;
    if (actor.holds_stall_tile != 0) {
      world.ForgetActionTarget(actor);
      actor.holds_stall_tile = 0;
    }
    return Step::kContinue;
  }
  if (CountDownExpires(actor.stall_frames)) {
    actor.previous_tile = static_cast<std::int32_t>(*linked);
    actor.detour_target = kNoDetourTarget;
    actor.holds_stall_tile = 1;
    actor.detour_step_frames = kDetourStepFrames;
  }
  if (CountDownExpires(actor.detour_step_frames)) {
    if (++actor.detour_step >= kDetourSteps) actor.detour_step = 1;
    actor.detour_step_frames = kDetourStepFrames;
  }
  actor.stayed_on_tile = 1;
  return Step::kContinue;
}

void WriteSample(std::array<std::uint32_t, 2>& words, int index,
                 std::int16_t value) {
  const int shift = (index % kSamplesPerWord) * kBitsPerSample;
  std::uint32_t& word = words.at(index / kSamplesPerWord);
  word = (word & ~(kSampleMask << shift)) |
         ((static_cast<std::uint32_t>(value) & kSampleMask) << shift);
}

// The command-change reaction, the position copy and the movement history;
// a stalled history starts movement recovery.
Step TrackCommandAndPosition(ActorPerceptionWorld& world,
                             PerceivingActor& actor) {
  const std::uint8_t opcode = actor.battle.command_opcode;
  if (actor.observed_command != kNoCommand && opcode != kNoCommand &&
      actor.observed_command != opcode) {
    world.SetReactionWithDelay(actor, kReaction, kReactionDelay);
  }
  actor.observed_command = actor.battle.command_opcode;
  const auto model = world.Model();
  if (!model) return Step::kRefused;
  actor.tile = model->tile;
  const auto point = world.LinkedPosition();
  if (!point) return Step::kRefused;
  actor.position = point->position;
  actor.position_pad = point->pad;
  actor.fine_tile = ((static_cast<std::uint32_t>(
                          static_cast<std::uint16_t>(actor.position.z)) >>
                      kFineTileShift)
                     << kTileZShift) |
                    (static_cast<std::uint32_t>(
                         static_cast<std::uint16_t>(actor.position.x)) >>
                     kFineTileShift);
  if (((kHistoryCommands >> (actor.observed_command & kShiftMask)) & 1U) == 0) {
    actor.history.mask = 0;
    return Step::kContinue;
  }
  if (IsMovementHistoryStalled(actor.history)) {
    actor.history.mask = 0;
    actor.recovery_frames = kRecoveryFrames;
    return Step::kIdle;
  }
  const int index = actor.history_index;
  if (index >= kHistorySamples) return Step::kRefused;
  WriteSample(actor.history.x_words, index, actor.position.x);
  WriteSample(actor.history.z_words, index, actor.position.z);
  actor.history.mask =
      static_cast<std::uint8_t>(actor.history.mask | (1U << index));
  actor.history_index =
      static_cast<std::uint8_t>((index + 1) & (kHistorySamples - 1));
  return Step::kContinue;
}

// The navigation counter, both target perceptions and candidate readiness.
Step PerceiveTargets(ActorPerceptionWorld& world, PerceivingActor& actor) {
  if (actor.navigation_counter > 0) --actor.navigation_counter;
  if (!world.UpdateTargetPerception(actor, 0)) return Step::kRefused;
  const std::int32_t second = world.SecondPerceptionTarget();
  if (second != 0 && !world.UpdateTargetPerception(actor, second)) {
    return Step::kRefused;
  }
  world.RefreshResourceRestrictions(actor);
  if (world.RefreshCandidateReadiness(actor)) return Step::kIdle;
  return Step::kContinue;
}

// The actor's alert rule; nothing refuses a missing HP or a listed actor
// outside the relation slots.
std::optional<bool> EvaluateAlert(const ActorPerceptionWorld& world,
                                  const PerceivingActor& actor) {
  if (actor.alert_rule == 0) return false;
  bool alert = false;
  if (actor.alert_rule == kLowHpRule || actor.alert_rule == kHalfHpRule) {
    const auto hp = world.Hp();
    if (!hp) return std::nullopt;
    const int factor =
        actor.alert_rule == kLowHpRule ? kLowHpFactor : kHalfHpFactor;
    alert = hp->current * factor < hp->max;
  } else if (actor.flee_timer != 0) {
    alert = true;
  } else {
    for (const int id : world.ListedActorIds()) {
      if (id < 0 || id >= kPerceptionSlots) return std::nullopt;
      const PerceivedRelation& relation = actor.relations.at(id);
      if (relation.visible &&
          relation.classification == kHostileClassification) {
        alert = true;
        break;
      }
    }
  }
  return alert || actor.action_status == kFleeingStatus;
}

// The alert, the movement wait and the decay of a remembered target.
Step FinishFrame(ActorPerceptionWorld& world, PerceivingActor& actor) {
  const auto alert = EvaluateAlert(world, actor);
  if (!alert) return Step::kRefused;
  actor.alert = *alert ? 1 : 0;
  if (actor.movement_wait != 0) --actor.movement_wait;
  if (actor.holds_stall_tile != 0 || actor.previous_tile < 0) {
    return Step::kContinue;
  }
  bool forget = actor.target_memory_frames == 0 ||
                CountDownExpires(actor.target_memory_frames);
  if (!forget) {
    const int held = static_cast<int>(
        static_cast<std::uint32_t>(actor.previous_tile) >> kHeldTargetShift);
    if (held >= kPerceptionRecordSlots) return Step::kRefused;
    const auto tile = world.RecordTile(held);
    forget = !tile || !SameTile(*tile, actor.remembered_target_tile);
  }
  if (forget) world.ForgetActionTarget(actor);
  return Step::kContinue;
}
}  // namespace

// decomp: func_800D8EF4 (src/BATTLE/BATTLE.PRG/6FC00.c:1009).
bool UpdatePerceivingActor(ActorPerceptionWorld& world,
                           PerceivingActor& actor) {
  const int flags = actor.battle.command_state;
  actor.battle.command_flags = 0;
  actor.partial_perception = 0;
  actor.action_delay = actor.battle.action_recovery_timer;

  Step step = RunCommandGates(world, actor, flags);
  if (step == Step::kContinue) step = RunFrameGates(world, actor);
  if (step == Step::kContinue) step = TrackTile(world, actor);
  if (step == Step::kContinue) step = TrackCommandAndPosition(world, actor);
  if (step == Step::kContinue) step = PerceiveTargets(world, actor);
  if (step == Step::kContinue) step = FinishFrame(world, actor);

  switch (step) {
    case Step::kRefused:
      return false;
    case Step::kIdle:
      world.RefreshTargetTileWhenTracking(actor);
      actor.perception_skipped = 1;
      return true;
    case Step::kSkip:
      actor.perception_skipped = 1;
      return true;
    case Step::kContinue:
      break;
  }
  return true;
}
}  // namespace VS::Battle::Ai
