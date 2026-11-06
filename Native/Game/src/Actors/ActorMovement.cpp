// Hand-maintained (tools/port/owned.csv).
// The typed per-frame actor movement (BATTLE.PRG 38C1C.c, 3A1A0.c and
// 40564.c). Tweens act on the typed actor they are handed; the original
// looked the actor up again by its slot, which named the same record.
#include "Actors/ActorMovement.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Actors {
namespace {
using Core::Math::Cosine;
using Core::Math::R3000Quotient;
using Core::Math::Sine;
using Core::Math::WrappingMultiply;

constexpr int kAshleySlot = 0;
constexpr int kActorSlots = 17;
constexpr int kGimmickSlots = 16;
constexpr int kTileCentre = 64;
constexpr int kByteBits = 8;
constexpr int kByteMask = 0xFF;
constexpr int kNibbleMask = 0xF;
constexpr int kFirstCompanionSlot = 2;
constexpr int kNoRider = -1;
// Rates and step counts a tween takes from the actor.
constexpr int kFromActor = -1;
constexpr int kFacingTurnSteps = 12;
constexpr int kJumpTurnRate = 8;
constexpr int kFixedOne = 4096;

// Animations and blend frames.
constexpr int kIdleAnimation = 1;
constexpr int kJumpAnimation = 0x2F;
constexpr int kAirborneJumpAnimation = 33;
constexpr int kAirborneClimbAnimation = 35;
constexpr int kTeleportOutAnimation = 0x13;
constexpr int kTeleportOutLoopAnimation = 0x14;
constexpr int kTeleportInAnimation = 0x15;
constexpr int kTeleportInLoopAnimation = 0x16;
constexpr int kTravelLeapAnimation = 9;
constexpr int kTravelLandAnimation = 0xA;
constexpr int kGlideAnimation = 7;
constexpr int kGlideLoopAnimation = 0x25;
constexpr int kTravelHoldAnimationA = 0xC7;
constexpr int kTravelHoldAnimationB = 0xC9;
constexpr int kFlightLevelAnimation = 31;
constexpr int kFlightDescendAnimation = 32;
constexpr int kFlightAscendAnimation = 34;
constexpr int kFlightFastOffset = 6;
constexpr int kBlendQuick = 4;
constexpr int kBlendNormal = 8;
constexpr int kJumpSound = 0xC;

// movement_mode: the jump and fall phases.
constexpr std::uint8_t kModeNone = 0;
constexpr std::uint8_t kModeRunUp = 1;
constexpr std::uint8_t kModeTakeoff = 2;
constexpr std::uint8_t kModeAirborne = 3;
constexpr std::uint8_t kModeLanding = 4;
// destination_mode: none, a slide stopping at the destination, a tile jump.
constexpr std::int16_t kDestinationNone = 0;
constexpr std::int16_t kDestinationSlide = 1;
constexpr std::int16_t kDestinationJump = 2;
// request_mode values.
constexpr std::uint8_t kRequestStick = 0;
constexpr std::uint8_t kRequestStep = 1;
constexpr std::uint8_t kRequestGround = 2;
constexpr std::uint8_t kRequestFlight = 3;
constexpr std::uint8_t kRequestVector = 4;
constexpr std::uint8_t kRequestJump = 7;
// action_animation_state values the transitions use.
constexpr std::uint8_t kActionStatesBusyEnd = 6;
constexpr std::uint8_t kTeleportOutAction = 6;
constexpr std::uint8_t kTeleportReadyAction = 8;
constexpr std::uint8_t kTeleportInAction = 9;
// reaction_state: the teleport transition's phases.
constexpr std::uint8_t kTeleportIdle = 0;
constexpr std::uint8_t kTeleportBegin = 1;
constexpr std::uint8_t kTeleportVanishing = 2;
constexpr std::uint8_t kTeleportAppearing = 3;
constexpr std::uint8_t kTeleportSettle = 4;
constexpr std::uint8_t kTeleportAwaitCompanion = 5;
// pose_state: the travel transition's phases.
constexpr std::uint8_t kTravelIdle = 0;
constexpr std::uint8_t kTravelLeap = 1;
constexpr std::uint8_t kTravelLeaping = 2;
constexpr std::uint8_t kTravelLanding = 3;
constexpr std::uint8_t kTravelGlide = 4;
constexpr std::uint8_t kTravelGliding = 5;
constexpr std::uint8_t kTravelHolding = 6;

// The tile jump arcs.
constexpr std::uint8_t kJumpCountdownWalk = 6;
constexpr std::uint8_t kJumpCountdownRun = 2;
constexpr std::uint8_t kStandingJumpCountdown = 6;
constexpr int kMinimumJumpSpeed = 18;
constexpr int kGroundAcceleration = 6;
constexpr int kAirborneAcceleration = 1;
constexpr int kShortDropDepth = -64;
constexpr std::size_t kAirborneArcOffset = 3;
constexpr std::size_t kClimbArc = 2;
constexpr std::size_t kAirborneClimbArc = 5;
constexpr int kAirborneClimbCap = 128;

// Ground offsets and flight.
constexpr int kNoGroundHeight = -3000;
constexpr int kStepUpLimit = 64;
constexpr int kFlightScale = kFixedOne * kFixedOne;
constexpr int kFlightDescendLimit = -2;
constexpr int kFlightAscendLimit = 3;
constexpr int kFlightFastSpeed = 12;

// MovingPlatformContactHeight modes the transitions sample with.
constexpr int kTravelContactMode = 2;
constexpr int kGlideContactMode = 3;

[[nodiscard]] std::int16_t Narrow16(int value) {
  return static_cast<std::int16_t>(value);
}

[[nodiscard]] std::uint8_t Byte(int value) {
  return static_cast<std::uint8_t>(value);
}

[[nodiscard]] std::uint8_t TileOf(int coordinate) {
  return Byte(coordinate / kTileUnits);
}

void SetTileFromPosition(ActorMotion& motion) {
  motion.current_tile.x = TileOf(motion.position.x);
  motion.current_tile.z = TileOf(motion.position.z);
}

// The tile packed into the route target's x and y halfwords (x, layer, z,
// spare in byte order), as BeginActorJumpToTile stores it.
[[nodiscard]] PackedTile RouteTile(const ActorMotion& motion) {
  const auto packed_x = static_cast<std::uint16_t>(motion.route_target.x);
  const auto packed_y = static_cast<std::uint16_t>(motion.route_target.y);
  return {.x = Byte(packed_x & kByteMask),
          .layer = Byte(packed_x >> kByteBits),
          .z = Byte(packed_y & kByteMask),
          .spare = Byte(packed_y >> kByteBits)};
}

void SetRouteTile(ActorMotion& motion, PackedTile tile) {
  motion.route_target.x = Narrow16(tile.x | (tile.layer << kByteBits));
  motion.route_target.y = Narrow16(tile.z | (tile.spare << kByteBits));
}

// func_800ACF54 for the typed actor.
void SaveGroundedTransform(MovementActor& actor) {
  const ActorMotion& motion = actor.motion;
  const GroundedHistory history = SaveActorGroundedTransform(
      {.tile = {.x = motion.current_tile.x,
                .layer = motion.current_tile.layer,
                .z = motion.current_tile.z,
                .spare = actor.tile_spare},
       .position = {motion.position.x, motion.position.y, motion.position.z},
       .facing = motion.facing,
       .support_layer = actor.support_layer});
  actor.previous_tile = history.previous_tile;
  actor.grounded_position = history.position;
  actor.previous_facing = history.previous_facing;
  if (history.saved_placement) actor.saved_placement = *history.saved_placement;
}

void ReleaseCurrentTile(MovementHost& host, const MovementActor& actor) {
  host.ReleaseAiDestinationTile(actor.motion.current_tile.x,
                                actor.motion.current_tile.z);
}

// Field_SetMovingPlatformActor for collision layer `layer`; the original
// wrote platform layer - 2 without a bound.
void SetLayerRider(MovementHost& host, int layer, int rider) {
  const int platform = layer - kFirstPlatformLayer;
  if (platform < 0 || platform >= host.MovingPlatformCount()) {
    VS_LOG_DEBUG("field.platform",
                 "rider {} skipped for layer {}: no moving platform", rider,
                 layer);
    return;
  }
  host.SetPlatformRider(platform, rider);
}

// The action state of the actor this one rides; nothing (settled) when the
// parent slot is empty.
[[nodiscard]] std::optional<int> ParentActionState(MovementHost& host,
                                                   const MovementActor& actor) {
  const MovementActor* parent = host.Actor(actor.attach_parent);
  if (parent == nullptr) {
    VS_LOG_WARN("actors", "actor {} rides absent parent {}",
                static_cast<int>(actor.motion.slot),
                static_cast<int>(actor.attach_parent));
    return std::nullopt;
  }
  return parent->motion.action_animation_state;
}

[[nodiscard]] bool CompanionReacting(MovementHost& host,
                                     const MovementActor& actor) {
  if (actor.companion_slot < kFirstCompanionSlot) return false;
  const MovementActor* companion = host.Actor(actor.companion_slot);
  if (companion == nullptr) {
    VS_LOG_WARN("actors", "actor {} has absent companion {}",
                static_cast<int>(actor.motion.slot),
                static_cast<int>(actor.companion_slot));
    return false;
  }
  return companion->reaction_state != 0;
}

struct JumpArc {
  int frames = 0;
  MotionVector root{};
};

// A drop: the frames to fall `drop` below plus the hang frames, retried with
// more hang until each horizontal step fits the speed.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): x, drop, z order.
[[nodiscard]] JumpArc PlanDropArc(const MovementTables& tables, int x, int drop,
                                  int z, int speed, bool airborne) {
  std::size_t arc = drop >= kShortDropDepth ? 1 : 0;
  if (airborne) arc += kAirborneArcOffset;
  const int accel = airborne ? kAirborneAcceleration : kGroundAcceleration;
  int hang = tables.hang_frames.at(arc);
  drop -= tables.height_adjustments.at(arc);
  JumpArc result;
  for (;;) {
    int frames = 0;
    int fall = 0;
    int velocity = 0;
    for (; drop < fall; ++frames) {
      velocity += accel;
      fall -= velocity / 2;
    }
    frames += hang;
    result.frames = frames;
    result.root.x = Narrow16(R3000Quotient(x, frames));
    result.root.z = Narrow16(R3000Quotient(z, frames));
    result.root.y = Narrow16(-velocity / 2);
    if (std::abs(result.root.x) <= speed && std::abs(result.root.z) <= speed) {
      return result;
    }
    ++hang;
    drop -= ((hang + 1) * accel) / 2;
  }
}

// A climb: the frames to rise `rise` plus the hang frames, stretched to the
// frames the horizontal distance needs at the speed.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): x, rise, z order.
[[nodiscard]] JumpArc PlanClimbArc(const MovementTables& tables, int x,
                                   int rise, int z, int min_frames,
                                   bool airborne) {
  const std::size_t arc = airborne ? kAirborneClimbArc : kClimbArc;
  const int accel = airborne ? kAirborneAcceleration : kGroundAcceleration;
  int hang = tables.hang_frames.at(arc);
  rise += tables.height_adjustments.at(arc);
  if (airborne && rise > kAirborneClimbCap) rise = kAirborneClimbCap;
  int frames = 0;
  int fall = 0;
  int velocity = 0;
  for (; fall < rise; ++frames) {
    velocity += accel;
    fall += velocity / 2;
  }
  frames += hang;
  if (frames < min_frames) {
    hang += ((min_frames - frames) / 2) + 1;
    frames = min_frames;
  }
  JumpArc result;
  result.frames = frames;
  result.root.x = Narrow16(R3000Quotient(x, frames));
  result.root.z = Narrow16(R3000Quotient(z, frames));
  result.root.y = Narrow16((-(hang + 1) * accel) / 2);
  return result;
}

void CancelOneTeleport(MovementHost& host, MovementActor& actor) {
  TranslateActorToTileAtRate(host.Terrain(), actor.motion,
                             RouteTile(actor.motion), 0);
  actor.queued_animation = 0;
  actor.reaction_state = kTeleportSettle;
  UpdateActorTeleportTransition(host, actor);
}

void LandTravel(MovementHost& host, MovementActor& actor) {
  actor.motion.motion_locked = false;
  SaveGroundedTransform(actor);
  ReleaseCurrentTile(host, actor);
  host.PlayAnimation(actor.motion.slot, kIdleAnimation, kBlendNormal);
  actor.motion.pose_state = kTravelIdle;
}

void IdleTravel(MovementHost& host, MovementActor& actor) {
  host.PlayAnimation(actor.motion.slot, kIdleAnimation, kBlendNormal);
  actor.motion.pose_state = kTravelIdle;
}

[[nodiscard]] bool OnSupportLayer(MovementHost& host,
                                  const MotionVector& position, int mode) {
  (void)host.PlatformContactHeight(position.x, position.y, position.z, mode);
  // #292: the layer is a halfword; the original read a word over it.
  return host.CurrentSupport().layer != 0;
}

void BeginTravelLeap(MovementHost& host, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  host.PlayAnimation(motion.slot, kTravelLeapAnimation, kBlendNormal);
  TranslateActorToTileAtRate(host.Terrain(), motion, RouteTile(motion),
                             kFromActor);
  TurnActorToAtRate(motion, motion.route_target.w, kJumpTurnRate);
  motion.transition_frames = motion.move_frames;
  motion.move_frames = 0;
  if (motion.transition_frames != 0) {
    motion.pose_state = kTravelLeaping;
    return;
  }
  LandTravel(host, actor);
}

void BeginTravelGlide(MovementHost& host, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  host.PlayAnimation(motion.slot, kGlideAnimation, kBlendNormal);
  host.QueueAnimation(motion.slot, kGlideLoopAnimation, kBlendNormal, 0);
  TranslateActorToOverTime(motion, motion.route_target, kFromActor);
  TurnActorToAtRate(motion, motion.route_target.w, kJumpTurnRate);
  const std::uint16_t steps = motion.move_frames;
  motion.move_frames = 0;
  motion.transition_frames = steps;
  motion.motion_locked = true;
  if (motion.transition_frames == 0) {
    actor.queued_animation = 0;
    IdleTravel(host, actor);
    return;
  }
  motion.pose_state = kTravelGliding;
}

// The collision-resolved move the player's stick or a navigation step asks
// for, then the jump it may start.
void RequestMove(MovementHost& host, int id, MovementActor& actor,
                 const MoveInput& input) {
  MoveActorWithCollision(host, id, actor.motion.request_direction,
                         static_cast<int>(actor.motion.request_distance),
                         input);
  if (actor.motion.movement_mode != kModeNone) {
    UpdateActorJumpAndFall(host, id, actor);
  }
}

void UpdateAshleyRequest(MovementHost& host, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  if (motion.movement_mode != kModeNone &&
      motion.destination_mode != kDestinationNone) {
    UpdateActorJumpAndFall(host, kAshleySlot, actor);
    return;
  }
  switch (motion.request_mode) {
    case kRequestJump:
    case kRequestStick:
      RequestMove(host, kAshleySlot, actor, {});
      break;
    case kRequestStep:
      RequestMove(host, kAshleySlot, actor,
                  {.x = motion.request_facing, .pad = kRequestStep});
      break;
    case kRequestGround:
      ApplyActorGroundOffset(host, actor);
      break;
    default:
      break;
  }
}

void UpdateOtherRequest(MovementHost& host, int id, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  if (motion.request_mode == kRequestGround) {
    ApplyActorGroundOffset(host, actor);
  } else if (motion.movement_mode != kModeNone) {
    UpdateActorJumpAndFall(host, id, actor);
  } else if (actor.reaction_state != 0) {
    UpdateActorTeleportTransition(host, actor);
  } else if (motion.pose_state != 0) {
    UpdateActorTravelTransition(host, actor);
  } else if (motion.request_mode == kRequestStep) {
    if (actor.was_drawn || motion.motion_locked) return;
    RequestMove(host, id, actor,
                {.x = motion.request_facing, .pad = kRequestStep});
  } else if (motion.request_mode == kRequestFlight) {
    ApplyActorFlightMovement(host, actor);
  } else if (motion.request_mode == kRequestVector) {
    ApplyActorMovementVector(actor);
  }
}

// An actor that left the platform it was told about loses the contact.
void UpdateContactKind(MovementHost& host, MovementActor& actor) {
  const int contact = actor.contact_kind;
  if (contact == 0) return;
  if (actor.support_layer == contact) actor.support_layer = 0;
  if (!host.IsPointOnMovingPlatform(contact, actor.motion.position)) {
    actor.contact_kind = 0;
    host.ClearAiNavigationReset(actor.motion.slot);
  }
  if (actor.motion.destination_mode == kDestinationSlide) {
    actor.motion.destination_x = actor.motion.position.x;
    actor.motion.destination_z = actor.motion.position.z;
  }
}

void FollowPlatform(MovementHost& host, MovementActor& actor) {
  if (actor.support_layer == 0) return;
  const std::optional<PlatformPoint> contact =
      host.MovingPlatform(actor.support_layer);
  if (!contact) {
    VS_LOG_ERROR("field.platform", "actor {} rides invisible platform layer {}",
                 static_cast<int>(actor.motion.slot),
                 static_cast<int>(actor.support_layer));
    return;
  }
  MotionVector& anchor = actor.platform_anchor;
  MotionVector& position = actor.motion.position;
  actor.platform_offset_x = static_cast<std::int8_t>(contact->x - anchor.x);
  actor.platform_offset_z = static_cast<std::int8_t>(contact->z - anchor.z);
  position.x = Narrow16(position.x + actor.platform_offset_x);
  position.z = Narrow16(position.z + actor.platform_offset_z);
  position.y = Narrow16(position.y + contact->height - anchor.y);
  anchor.x = contact->x;
  anchor.y = contact->height;
  anchor.z = contact->z;
}

void AnchorPlatform(MovementHost& host, MovementActor& actor) {
  const ActorMotion& motion = actor.motion;
  if (motion.movement_mode == kModeTakeoff ||
      motion.movement_mode == kModeAirborne) {
    return;
  }
  const int support = actor.support_layer;
  if (actor.placement_layer != support) {
    if (motion.slot == kAshleySlot && support != 0 &&
        support != actor.platform_anchor.w) {
      host.SetAiContactOccupancy(support);
    }
    actor.platform_offset_x = 0;
    actor.platform_offset_z = 0;
  }
  if (actor.support_layer == 0) return;
  const std::optional<PlatformPoint> contact =
      host.MovingPlatform(actor.support_layer);
  if (!contact) {
    VS_LOG_ERROR(
        "field.platform", "actor {} anchors to invisible platform layer {}",
        static_cast<int>(motion.slot), static_cast<int>(actor.support_layer));
    return;
  }
  actor.platform_anchor.x = contact->x;
  actor.platform_anchor.y = contact->height;
  actor.platform_anchor.z = contact->z;
}

void CommitPlatform(MovementHost& host, MovementActor& actor) {
  const ActorMotion& motion = actor.motion;
  const int current = actor.support_layer;
  const int previous = actor.placement_layer;
  if (current != previous) {
    if (current != 0) {
      actor.platform_anchor.w = Narrow16(current);
      SetLayerRider(host, current, motion.slot);
    } else {
      SetLayerRider(host, previous, kNoRider);
      if (motion.movement_mode == kModeNone) {
        actor.platform_offset_x = 0;
        actor.platform_offset_z = 0;
      }
    }
  }
  if (motion.slot == kAshleySlot && motion.movement_mode == kModeNone &&
      actor.support_layer != actor.platform_anchor.w) {
    host.ClearAiContactOccupancy(actor.platform_anchor.w);
    actor.platform_anchor.w = 0;
  }
  actor.motion.current_tile.layer = actor.support_layer;
  actor.placement_layer = actor.support_layer;
}

// MoveActorWithCollision (func_800A5280, 3A1A0.c:1347-1910).
constexpr std::uint8_t kLocomotionFree = 0;
constexpr std::uint8_t kLocomotionStepping = 1;
constexpr std::uint8_t kLocomotionHanging = 2;
constexpr int kQuarterTurn = kAngleOne / 4;
constexpr int kThreeQuarterTurn = kAngleOne * 3 / 4;
constexpr int kStickDistance = -1;
constexpr int kFixedShift = 12;
constexpr int kTurnOutBlend = 10;
constexpr int kHangDropFrame = 7;
constexpr std::int16_t kHangDropLift = 3;
constexpr std::uint8_t kNoAnimationSet = 0xFF;
constexpr int kAnimationVariantOffset = 100;
constexpr int kWalkAnimationFirst = 13;
constexpr unsigned kWalkAnimationCount = 14;
constexpr unsigned kStepAnimationCount = 2;
constexpr int kStepPoseAnimationA = 0x13;
constexpr int kStepPoseAnimationB = 0x14;
constexpr int kPushAnimation = 0x54;
constexpr int kRunStartFrames = 20;
constexpr int kRunStartTurns = 9;
constexpr int kCounterCap = 0xFF;
constexpr std::int16_t kFullCollision = 4;
constexpr std::int16_t kSlidingCollision = 2;
constexpr std::int16_t kSteppingDownCollision = 5;
constexpr int kStickSpeedCap = 0x1EFFF;
constexpr int kStickSpeedLimit = 0x1F000;
constexpr int kWallSlowdown = 4;
constexpr int kStatusFree = 0;
constexpr int kStatusAlongX = 1;
constexpr int kStatusAlongZ = 2;
constexpr int kStatusBlocked = 3;
constexpr int kRunUpAnimation = 0x30;
constexpr int kStepClimb = 1;
constexpr int kLowClimb = 2;
constexpr int kHighClimb = 3;
constexpr int kLedgeClimb = 4;
constexpr int kRefusedClimb = 5;
constexpr int kClimbUnit = 64;
constexpr int kClimbUnitMask = 0x3F;
constexpr int kSteepDrop = -0x20;
constexpr int kLedgeReach = -0xEC;
constexpr int kLowCeiling = -0x5F;
constexpr std::uint8_t kMaxLedgeAttempts = 8;
constexpr unsigned kSlideCellBit = 5;
constexpr int kLedgeGrabSteps = 4;
constexpr int kLedgeDirectionShift = 9;
constexpr int kHangStepFrames = 0x18;
constexpr int kClimbStepFrames = 8;
constexpr int kStepHeightBias = 0xFF;
constexpr unsigned kStepHeightRange = 0x100;
constexpr std::int16_t kSpinGimmickMode = 0xFD;
constexpr int kHangSoundA = 0x2A;
constexpr int kHangSoundB = 0x2B;
constexpr int kClimbSound = 0x29;
constexpr int kFallReach = 0x40;
constexpr int kFallBlockReach = 0x100;
constexpr int kSmallDrop = -2;

enum class Flow : std::uint8_t {
  kContinue,
  kReturn,
  kHalt,
  kStop,
  kScale,
  kMove,
  kLand,
  kDone,
};

struct MoveContext {
  MovementHost& host;
  int id;
  MovementActor& actor;
  const MoveInput& input;
  int direction;
  int distance;
  bool turning = false;
  int speed = 0;
  int facing = 0;
  int status = kStatusFree;
  int result = 0;
  int previous_area = 0;
  MotionVector motion{};
};

[[nodiscard]] int BaseAnimation(const MovementActor& actor) {
  int animation = actor.animation_id;
  if (animation >= kAnimationVariantOffset) {
    animation -= kAnimationVariantOffset;
  }
  return animation;
}

[[nodiscard]] bool IsWalkAnimation(int animation) {
  return static_cast<unsigned>(animation - kWalkAnimationFirst) <
         kWalkAnimationCount;
}

[[nodiscard]] bool SlidesOnProbedCell(MovementHost& host) {
  return ((host.Probe().cell_word >> kSlideCellBit) & 1U) != 0;
}

void UpdateLocomotion(MoveContext& ctx) {
  if (ctx.actor.motion.movement_mode == kModeNone) {
    ctx.host.UpdateLocomotionAnimation(ctx.actor, ctx.distance, ctx.direction);
  }
}

void FallFromHang(MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  motion.root_motion.y = kHangDropLift;
  actor.animation_set = kNoAnimationSet;
  motion.movement_mode = kModeAirborne;
  motion.locomotion_state = kLocomotionFree;
  motion.movement_pending = false;
  actor.support_layer = 0;
  actor.support_gimmick = 0;
}

// A hanging actor drops when its platform bounces past the ledge (#293: the
// bounce frames start at the first platform layer; the original indexed two
// bytes before them).
[[nodiscard]] bool HangPlatformDrops(MovementHost& host,
                                     const MovementActor& actor) {
  const int layer = actor.support_layer;
  if (layer < kFirstPlatformLayer ||
      host.PlatformBounceFrame(layer - kFirstPlatformLayer) != kHangDropFrame) {
    return false;
  }
  const MotionVector& position = actor.motion.position;
  return host.SampleSupportHeight(position, 0, 0, 1) <= position.y;
}

[[nodiscard]] int TurnFromMoveFacing(const MoveContext& ctx) {
  return (ctx.direction - ctx.actor.move_facing) & kAngleMask;
}

Flow HandleHang(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  if (actor.support_layer != 0) {
    SaveGroundedTransform(actor);
    if (HangPlatformDrops(ctx.host, actor)) {
      FallFromHang(actor);
      return Flow::kReturn;
    }
  }
  if (ctx.distance == 0) return Flow::kReturn;
  if (static_cast<unsigned>(TurnFromMoveFacing(ctx) - kQuarterTurn) <=
      static_cast<unsigned>(kHalfTurn)) {
    FallFromHang(actor);
    return Flow::kReturn;
  }
  if (motion.move_frames != 0) return Flow::kReturn;
  ctx.direction = actor.move_facing;
  if (ctx.distance >= motion.run_speed << kFixedShift) {
    ctx.distance = motion.walk_speed << kFixedShift;
  }
  return Flow::kContinue;
}

Flow HandleTurnOut(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  const int turn = TurnFromMoveFacing(ctx);
  if (ctx.distance == 0 || turn <= kQuarterTurn || turn >= kThreeQuarterTurn) {
    return Flow::kReturn;
  }
  ctx.turning = true;
  motion.locomotion_state = kLocomotionFree;
  motion.movement_pending = false;
  motion.move_frames = 0;
  motion.root_motion.x = 0;
  motion.root_motion.y = 0;
  motion.root_motion.z = 0;
  actor.ledge_height = 0;
  ctx.host.PlayAnimation(ctx.id, kIdleAnimation, kTurnOutBlend);
  return Flow::kContinue;
}

Flow HandleJumpMode(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  const ActorMotion& motion = actor.motion;
  if (motion.movement_mode == kModeNone) return Flow::kContinue;
  if (ctx.id != kAshleySlot) return Flow::kReturn;
  if (ctx.distance / kFixedOne >= motion.run_speed) {
    if (!actor.jump_started) ctx.distance = motion.walk_speed << kFixedShift;
  } else {
    actor.jump_started = false;
  }
  if (motion.movement_mode < kModeLanding) {
    if (ctx.distance == 0) return Flow::kStop;
  } else if (motion.movement_mode == kModeLanding) {
    return Flow::kReturn;
  }
  return Flow::kContinue;
}

Flow StickStep(MoveContext& ctx) {
  if (ctx.input.x == 0 && ctx.input.z == 0) {
    const int animation = BaseAnimation(ctx.actor);
    if (animation == kIdleAnimation) return Flow::kHalt;
    if (!IsWalkAnimation(animation)) return Flow::kReturn;
  }
  ctx.motion.x = ctx.input.x;
  ctx.motion.z = ctx.input.z;
  ctx.speed = ctx.actor.motion.walk_speed << kFixedShift;
  return Flow::kMove;
}

// The run momentum carries on in its own direction after the stick lets go.
void Coast(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  int distance = motion.momentum_distance;
  ctx.direction = actor.momentum_direction;
  distance -= ctx.host.TickSpeed();
  motion.momentum_distance = Narrow16(distance);
  if (distance <= 0) {
    motion.is_running = false;
    distance = 0;
  }
  ctx.distance = distance << kFixedShift;
}

Flow IdleStep(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  if (motion.is_running && ctx.input.pad == 0) {
    Coast(ctx);
    return Flow::kScale;
  }
  if (motion.momentum_distance >= motion.run_speed) {
    if (motion.run_frames >= kRunStartFrames &&
        actor.run_turns < kRunStartTurns) {
      motion.run_frames = 0;
      motion.is_running = true;
      if (ctx.input.pad == 0) {
        Coast(ctx);
        return Flow::kScale;
      }
      UpdateLocomotion(ctx);
      return Flow::kReturn;
    }
    motion.run_frames = 0;
  }
  if (actor.contact_kind != 0) return Flow::kHalt;
  if (motion.momentum_distance != 0 || actor.support_layer != 0) {
    motion.momentum_distance = 0;
  } else if (!actor.wall_contact) {
    const int animation = BaseAnimation(actor);
    if (animation != kPushAnimation &&
        (animation == kIdleAnimation || !IsWalkAnimation(animation))) {
      return Flow::kReturn;
    }
  }
  return Flow::kHalt;
}

[[nodiscard]] std::uint8_t AddTicks(int counter, int ticks) {
  return Byte(std::min(counter + ticks, kCounterCap));
}

// #294: the facing is defined on every path; the original left it unset
// unless the stick moved.
[[nodiscard]] int MoveFacing(const MoveContext& ctx) {
  const int facing =
      ctx.input.pad == kRequestStep ? ctx.input.x : ctx.direction;
  return facing & kAngleMask;
}

void RunStep(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  const int half_ticks = ctx.host.TickSpeed() / 2;
  motion.is_running = false;
  motion.momentum_distance = Narrow16(ctx.distance / kFixedOne);
  if (ctx.distance >= motion.run_speed << kFixedShift) {
    actor.momentum_direction = Narrow16(ctx.direction);
    motion.run_frames = AddTicks(motion.run_frames, half_ticks);
    actor.run_turns = 0;
  } else {
    actor.run_turns = AddTicks(actor.run_turns, half_ticks);
  }
  ctx.facing = MoveFacing(ctx);
  if (ctx.distance != 0 && motion.locomotion_state == kLocomotionFree) {
    UpdateActorSteering(actor, ctx.facing, ctx.input.pad != 0);
  }
}

Flow ChooseStep(MoveContext& ctx) {
  if (ctx.distance == kStickDistance) return StickStep(ctx);
  if (ctx.distance == 0) return IdleStep(ctx);
  RunStep(ctx);
  return Flow::kScale;
}

Flow ScaleStep(MoveContext& ctx) {
  const MovementActor& actor = ctx.actor;
  int speed = ctx.distance;
  if (actor.collision_state != kFullCollision) {
    speed =
        WrappingMultiply(ctx.distance, actor.collision_state) / kFullCollision;
    if (ctx.input.pad == 0 && speed > kStickSpeedCap) {
      speed = kStickSpeedLimit;
    }
  }
  ctx.speed = speed;
  if (speed <= 0) {
    UpdateLocomotion(ctx);
    return Flow::kReturn;
  }
  const int angle = kThreeQuarterTurn - ctx.direction;
  ctx.motion.x =
      Narrow16(WrappingMultiply(Cosine(angle), speed) / kFlightScale);
  ctx.motion.z = Narrow16(WrappingMultiply(Sine(angle), speed) / kFlightScale);
  return Flow::kMove;
}

void StopStep(MoveContext& ctx) {
  ctx.actor.motion.root_motion.x = 0;
  ctx.actor.motion.root_motion.z = 0;
  ctx.motion.x = 0;
  ctx.motion.z = 0;
}

// The resolve status sets the distance moved; a blocked jump or hang ends
// the move.
Flow ApplyResolveStatus(MoveContext& ctx) {
  ActorMotion& motion = ctx.actor.motion;
  switch (ctx.status) {
    case kStatusFree:
      ctx.distance = ctx.speed / kFixedOne;
      return Flow::kContinue;
    case kStatusAlongX:
      ctx.distance = std::abs(static_cast<int>(ctx.motion.x));
      return Flow::kContinue;
    case kStatusAlongZ:
      ctx.distance = std::abs(static_cast<int>(ctx.motion.z));
      return Flow::kContinue;
    case kStatusBlocked:
      break;
    default:
      return Flow::kContinue;
  }
  ctx.distance = 0;
  if (motion.locomotion_state == kLocomotionHanging) return Flow::kReturn;
  if (ctx.turning) motion.movement_mode = kModeAirborne;
  if (motion.movement_mode != kModeNone) {
    motion.root_motion.x = 0;
    motion.root_motion.z = 0;
    if (motion.movement_mode == kModeRunUp) {
      ctx.host.PlayAnimation(ctx.id, kRunUpAnimation, 2);
      motion.movement_mode = kModeTakeoff;
    }
    return Flow::kReturn;
  }
  ctx.motion.x = 0;
  ctx.motion.z = 0;
  return Flow::kContinue;
}

// The player's step: how high a climb or how deep a drop the terrain asks.
void ClassifyClimb(MoveContext& ctx) {
  const MotionVector& position = ctx.actor.motion.position;
  const int rise = ctx.motion.y;
  if (rise < kSteepDrop || ctx.status == kStatusBlocked) {
    int climb = -(rise / kClimbUnit);
    if ((rise & kClimbUnitMask) != 0) ++climb;
    ctx.result = std::max(ctx.result, climb);
  } else if (ctx.result == kLedgeClimb) {
    ctx.motion.y = Narrow16(ctx.motion.y + position.y);
    ctx.motion.y = Narrow16(ctx.host.Probe().position.y - ctx.motion.y);
    if (ctx.motion.y >= kLedgeReach) ctx.result = kHighClimb;
  } else if (ctx.host.Probe().position.y - position.y < kLowCeiling &&
             position.y < 0) {
    ctx.result = kLowClimb;
  }
  if (ctx.status == kStatusBlocked && ctx.result >= 0) {
    ctx.result = kRefusedClimb;
  }
}

// A wall-bound actor that keeps pushing into the same point stops there.
[[nodiscard]] bool PushesSameWallPoint(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  const MotionVector& position = actor.motion.position;
  if (!actor.wall_contact || ctx.speed != 0) {
    actor.previous_target_x = actor.pending_target_x;
    actor.previous_target_z = actor.pending_target_z;
    actor.pending_target_x = 0;
    actor.pending_target_z = 0;
    return false;
  }
  const int target_x = position.x + ctx.motion.x;
  const int target_z = position.z + ctx.motion.z;
  if (actor.previous_target_x == target_x &&
      actor.previous_target_z == target_z) {
    ctx.distance = 0;
    return true;
  }
  actor.previous_target_x = actor.pending_target_x;
  actor.previous_target_z = actor.pending_target_z;
  actor.pending_target_x = static_cast<std::uint16_t>(target_x);
  actor.pending_target_z = static_cast<std::uint16_t>(target_z);
  return false;
}

// An airborne or running-up actor only takes the step's root motion.
Flow ApplyJumpModeResult(MoveContext& ctx) {
  ActorMotion& motion = ctx.actor.motion;
  if (motion.movement_mode == kModeRunUp) {
    motion.root_motion.x = ctx.motion.x;
    motion.root_motion.z = ctx.motion.z;
    ctx.host.PlayAnimation(ctx.id, kRunUpAnimation, kBlendQuick);
    motion.movement_mode = kModeTakeoff;
    return Flow::kReturn;
  }
  if (ctx.result < kLowClimb) {
    const bool rising = ctx.motion.y > 0;
    motion.root_motion.x = rising ? ctx.motion.x : Narrow16(0);
    motion.root_motion.z = rising ? ctx.motion.z : Narrow16(0);
    return Flow::kReturn;
  }
  if (ctx.result > kLedgeClimb) {
    motion.root_motion.x = 0;
    motion.root_motion.z = 0;
    return Flow::kReturn;
  }
  return Flow::kContinue;
}

Flow ResolveStep(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  if (motion.movement_mode == kModeAirborne) {
    if (motion.position.y > 0) {
      actor.platform_offset_x = 0;
      actor.platform_offset_z = 0;
    }
    ctx.motion.x = Narrow16(ctx.motion.x + actor.platform_offset_x);
    ctx.motion.z = Narrow16(ctx.motion.z + actor.platform_offset_z);
  }
  if (actor.wall_contact) {
    ctx.motion.x = Narrow16(ctx.motion.x / kWallSlowdown);
    ctx.motion.z = Narrow16(ctx.motion.z / kWallSlowdown);
  }
  // The original also cleared Actors_motionResultResetWord here; nothing
  // reads it.
  const TerrainResolve resolved =
      ctx.host.ResolveTerrainMotion(actor, ctx.motion, ctx.input.pad != 0);
  ctx.status = resolved.status;
  ctx.result = resolved.climb;
  if (ApplyResolveStatus(ctx) == Flow::kReturn) return Flow::kReturn;
  ctx.motion.y = Narrow16(ctx.host.SampleSupportHeight(
      motion.position, ctx.motion.x, ctx.motion.z, 1));
  ctx.motion.y = Narrow16(ctx.motion.y - motion.position.y);
  if (motion.movement_mode == kModeNone &&
      motion.locomotion_state == kLocomotionFree) {
    ctx.previous_area = actor.support_gimmick;
    const SupportContact support = ctx.host.CurrentSupport();
    actor.support_gimmick = Byte(support.gimmick & kNibbleMask);
    actor.support_layer = Byte(support.layer & kNibbleMask);
  }
  if (ctx.input.pad == 0) ClassifyClimb(ctx);
  if (PushesSameWallPoint(ctx)) return Flow::kDone;
  actor.collision_state = kFullCollision;
  if (motion.movement_mode != kModeNone) return ApplyJumpModeResult(ctx);
  return Flow::kContinue;
}

Flow Reject(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  motion.root_motion.x = 0;
  motion.root_motion.z = 0;
  if (ctx.turning) {
    motion.root_motion.y = 0;
    motion.movement_mode = kModeAirborne;
    return Flow::kReturn;
  }
  actor.support_layer = actor.placement_layer;
  actor.support_gimmick = Byte(ctx.previous_area);
  ctx.distance = 0;
  return Flow::kDone;
}

Flow Blocked(MoveContext& ctx) {
  if (ctx.actor.motion.locomotion_state != kLocomotionFree) {
    return Flow::kReturn;
  }
  return Reject(ctx);
}

// Field_gimmickInstances[gimmick]: a spinning gimmick raises the ledge by
// the spin steps still to come.
void AddGimmickSpin(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  const std::optional<MovementGimmick> gimmick =
      ctx.host.Gimmick(actor.support_gimmick);
  if (!gimmick) {
    VS_LOG_WARN("field.gimmick", "actor {} hangs from absent gimmick {}",
                ctx.id, static_cast<int>(actor.support_gimmick));
    return;
  }
  if (gimmick->mode != kSpinGimmickMode) return;
  const auto& steps = ctx.host.Tables().gimmick_spin_steps;
  for (std::size_t i = gimmick->support_index; i < steps.size(); ++i) {
    ctx.motion.y = Narrow16(ctx.motion.y + steps.at(i));
    actor.ledge_height = Narrow16(actor.ledge_height + steps.at(i));
  }
}

Flow GrabLedge(MoveContext& ctx, MotionVector& saved) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  if (!ctx.host.CanHangFromLedge(actor, ctx.motion, saved)) return Reject(ctx);
  const MovementProbe probe = ctx.host.Probe();
  actor.support_gimmick =
      Byte((static_cast<std::uint16_t>(probe.position.w) >> kByteBits) &
           kNibbleMask);
  actor.ledge_height = probe.position.y;
  if (actor.support_gimmick != 0) AddGimmickSpin(ctx);
  if (!ctx.host.CalcLedgeOffset(actor, ctx.motion)) return Reject(ctx);
  SetActorMotion(motion, ctx.motion, kLedgeGrabSteps);
  TurnActorToOverTime(motion,
                      static_cast<std::uint16_t>(ctx.host.Probe().direction
                                                 << kLedgeDirectionShift),
                      kLedgeGrabSteps);
  actor.ledge_attempts = 0;
  SetTileFromPosition(motion);
  PackedTile& previous = actor.previous_tile;
  if (previous.x != motion.current_tile.x ||
      previous.z != motion.current_tile.z ||
      previous.layer != actor.support_layer) {
    const MotionVector point = ctx.host.Probe().position;
    previous.x = TileOf(point.x);
    previous.z = TileOf(point.z);
    previous.layer = actor.support_layer;
  }
  return Flow::kContinue;
}

void HangStep(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  if (actor.wall_contact) {
    ctx.motion.x = 0;
    ctx.motion.z = 0;
  }
  SetActorMotion(actor.motion, ctx.motion, kHangStepFrames);
  actor.ledge_height = 0;
}

Flow ClimbStep(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  if (ctx.input.pad == 0 &&
      (static_cast<std::uint16_t>(ctx.motion.y + kStepHeightBias) >=
           kStepHeightRange ||
       ctx.distance == 0)) {
    return Blocked(ctx);
  }
  if (ctx.result >= kLowClimb) {
    if (actor.motion.movement_mode != kModeNone && actor.wall_contact) {
      return Reject(ctx);
    }
    if (!ctx.host.HasLedgeSideSupport(actor)) return Reject(ctx);
  }
  SetActorMotion(actor.motion, ctx.motion, ctx.result * kClimbStepFrames);
  return Flow::kContinue;
}

[[nodiscard]] std::uint16_t TurnedFacing(const ActorMotion& motion) {
  return static_cast<std::uint16_t>((motion.facing + motion.facing_delta) &
                                    kAngleMask);
}

void PlayClimbSound(MoveContext& ctx, int first, int second) {
  if (ctx.id == kAshleySlot) {
    ctx.host.PlaySoundPair(ctx.actor, first, second);
  } else {
    ctx.host.PlayModelSound(ctx.actor, kJumpSound);
  }
}

void FinishClimb(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  const MovementTables& tables = ctx.host.Tables();
  if (ctx.result == kStepClimb) {
    const int animation = BaseAnimation(actor);
    if (static_cast<unsigned>(animation - kWalkAnimationFirst) >=
            kStepAnimationCount &&
        animation != kStepPoseAnimationA && animation != kStepPoseAnimationB &&
        animation != kPushAnimation) {
      ctx.host.PlayAnimation(ctx.id, tables.result_one_animation, kBlendQuick);
    }
    motion.locomotion_state = kLocomotionStepping;
    actor.move_facing = TurnedFacing(motion);
    motion.movement_pending = true;
  } else if (motion.locomotion_state == kLocomotionHanging) {
    ctx.host.PlayAnimation(ctx.id, tables.hang_animation, kBlendQuick);
    PlayClimbSound(ctx, kHangSoundA, kHangSoundB);
    UpdateActorSteering(actor, ctx.facing, ctx.input.pad != 0);
  } else {
    if (ctx.result == kLedgeClimb) motion.locomotion_state = kLocomotionHanging;
    ctx.host.PlayAnimation(
        ctx.id,
        tables.result_animations.at(static_cast<std::size_t>(ctx.result)),
        kBlendQuick);
    actor.move_facing = TurnedFacing(motion);
    PlayClimbSound(ctx, kClimbSound, 0);
  }
  motion.jump_countdown = 0;
  actor.platform_offset_x = 0;
  actor.platform_offset_z = 0;
  motion.movement_mode = kModeNone;
}

Flow ApplyClimb(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  if (SlidesOnProbedCell(ctx.host) && ctx.result == kStepClimb) {
    actor.collision_state = kSlidingCollision;
    SetActorTranslation(motion, ctx.motion, 0);
    return Flow::kDone;
  }
  if (ctx.result >= kRefusedClimb) {
    actor.ledge_attempts = 0;
    return Blocked(ctx);
  }
  if (ctx.result >= kLowClimb &&
      (actor.weapon_drawn || motion.action_pose_active)) {
    return Blocked(ctx);
  }
  if (motion.movement_mode == kModeNone && ctx.input.pad == 0 &&
      ctx.result != kStepClimb && actor.ledge_attempts < kMaxLedgeAttempts) {
    ++actor.ledge_attempts;
    if (motion.locomotion_state != kLocomotionHanging) return Blocked(ctx);
    return Flow::kReturn;
  }
  MotionVector saved = ctx.motion;
  const MovementProbe probe = ctx.host.Probe();
  ctx.motion.x = Narrow16(probe.position.x - motion.position.x);
  ctx.motion.y = Narrow16(probe.position.y - motion.position.y);
  ctx.motion.z = Narrow16(probe.position.z - motion.position.z);
  actor.support_layer = Byte(probe.position.w & kNibbleMask);
  Flow flow = Flow::kContinue;
  if (motion.locomotion_state == kLocomotionHanging) {
    HangStep(ctx);
  } else if (ctx.result == kLedgeClimb) {
    flow = GrabLedge(ctx, saved);
  } else {
    flow = ClimbStep(ctx);
  }
  if (flow != Flow::kContinue) return flow;
  FinishClimb(ctx);
  return Flow::kReturn;
}

Flow BeginFall(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  if (ctx.input.pad == 0) {
    if (ctx.motion.y >= kFallBlockReach) {
      if (motion.locomotion_state != kLocomotionHanging &&
          actor.ledge_attempts < kMaxLedgeAttempts) {
        ++actor.ledge_attempts;
        return Blocked(ctx);
      }
      motion.jump_countdown = 0;
    }
  } else {
    SetActorFallDestination(motion, ctx.motion.x, ctx.motion.z);
  }
  motion.root_motion.x = ctx.motion.x;
  motion.root_motion.z = ctx.motion.z;
  motion.root_motion.y = 0;
  ctx.host.PlayAnimation(ctx.id, kJumpAnimation, kBlendQuick);
  motion.movement_mode = kModeAirborne;
  actor.jump_started = false;
  motion.jump_profile = 1;
  if (ctx.distance >= motion.run_speed) {
    motion.jump_profile = 2;
    actor.jump_started = true;
  }
  actor.support_layer = 0;
  return Flow::kReturn;
}

Flow ApplyStepDown(MoveContext& ctx) {
  MovementActor& actor = ctx.actor;
  ActorMotion& motion = actor.motion;
  if (motion.locomotion_state == kLocomotionHanging) {
    ctx.motion.x = 0;
    ctx.motion.z = 0;
    FallFromHang(actor);
    return Flow::kReturn;
  }
  if (actor.support_layer != 0) {
    (void)ctx.host.PlatformContactHeight(motion.position.x, motion.position.y,
                                         motion.position.z, 0);
    actor.support_layer = Byte(ctx.host.CurrentSupport().layer & kNibbleMask);
  }
  if (ctx.motion.y >= kFallReach) return BeginFall(ctx);
  actor.ledge_attempts = 0;
  if (!SlidesOnProbedCell(ctx.host)) {
    actor.collision_state = kSteppingDownCollision;
  }
  return Flow::kLand;
}

Flow ApplyLevelStep(MoveContext& ctx) {
  if (ctx.motion.y > 0) return ApplyStepDown(ctx);
  MovementActor& actor = ctx.actor;
  actor.ledge_height = 0;
  actor.motion.locomotion_state = kLocomotionFree;
  actor.motion.movement_pending = false;
  if (ctx.motion.y < kSmallDrop) actor.collision_state = kSlidingCollision;
  actor.ledge_attempts = 0;
  return Flow::kLand;
}

// The move's step before the terrain resolves it.
Flow PlanStep(MoveContext& ctx) {
  ActorMotion& motion = ctx.actor.motion;
  if (motion.locomotion_state == kLocomotionHanging) {
    if (HandleHang(ctx) == Flow::kReturn) return Flow::kReturn;
  } else if (motion.locomotion_state != kLocomotionFree) {
    if (HandleTurnOut(ctx) == Flow::kReturn) return Flow::kReturn;
  }
  ctx.facing = MoveFacing(ctx);
  Flow flow = HandleJumpMode(ctx);
  if (flow == Flow::kContinue) flow = ChooseStep(ctx);
  if (flow == Flow::kHalt) {
    ctx.speed = 0;
    flow = Flow::kStop;
  }
  if (flow == Flow::kStop) {
    StopStep(ctx);
    flow = Flow::kMove;
  }
  if (flow == Flow::kScale) flow = ScaleStep(ctx);
  return flow;
}

[[nodiscard]] bool RefusesMove(const MovementActor& actor) {
  const ActorMotion& motion = actor.motion;
  return motion.route_phase != 0 || actor.reaction_state != 0 ||
         motion.weapon_pose_active ||
         static_cast<std::int8_t>(motion.jump_countdown) > 0;
}
}  // namespace

// decomp: BATTLE.PRG func_800A208C (src/BATTLE/BATTLE.PRG/38C1C.c:450)
void BeginActorJumpToTile(MovementHost& host, MovementActor& actor,
                          PackedTile target, int speed, std::int16_t facing) {
  ActorMotion& motion = actor.motion;
  if (motion.movement_mode != kModeNone) return;
  std::optional<PlatformPoint> platform;
  if (target.layer >= kFirstPlatformLayer) {
    platform = host.MovingPlatform(target.layer);
    if (!platform) {
      VS_LOG_ERROR("field.platform",
                   "jump target platform layer {} is not visible",
                   static_cast<int>(target.layer));
      return;
    }
  }
  motion.jump_countdown = kJumpCountdownWalk;
  actor.landing_pending = false;
  if (speed >= motion.run_speed) motion.jump_countdown = kJumpCountdownRun;
  if (speed < kMinimumJumpSpeed) {
    speed = kMinimumJumpSpeed;
    actor.landing_pending = true;
  }
  int x = (target.x * kTileUnits) + kTileCentre;
  int z = (target.z * kTileUnits) + kTileCentre;
  int y = 0;
  if (platform) {
    x = platform->x;
    y = platform->height;
    z = platform->z;
  } else {
    y = host.TileSupportHeight(x, z);
  }
  SetRouteTile(motion, target);
  motion.destination_x = Narrow16(x);
  motion.destination_z = Narrow16(z);
  x -= motion.position.x;
  y -= motion.position.y;
  z -= motion.position.z;
  const int min_frames = std::max(std::abs(x), std::abs(z)) / speed;
  const bool airborne = motion.is_jumping;
  int animation = airborne ? kAirborneJumpAnimation : kJumpAnimation;
  JumpArc arc;
  if (y <= 0) {
    if (motion.slot != kAshleySlot) host.PlayModelSound(actor, kJumpSound);
    arc = PlanDropArc(host.Tables(), x, y, z, speed, airborne);
  } else {
    arc = PlanClimbArc(host.Tables(), x, y, z, min_frames, airborne);
    if (airborne) animation = kAirborneClimbAnimation;
  }
  motion.root_motion.x = arc.root.x;
  motion.root_motion.y = arc.root.y;
  motion.root_motion.z = arc.root.z;
  actor.jump_frames = Narrow16(arc.frames);
  motion.destination_mode = kDestinationJump;
  host.PlayAnimation(motion.slot, animation, kBlendQuick);
  motion.movement_mode = kModeTakeoff;
  TurnActorToAtRate(motion, facing, kJumpTurnRate);
}

// decomp: BATTLE.PRG func_800A25EC (src/BATTLE/BATTLE.PRG/38C1C.c:633)
void ApplyActorGroundOffset(MovementHost& host, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  motion.motion_locked = false;
  const MotionVector& step = actor.request_step;
  MotionVector next{.x = step.x, .z = step.z};
  next.y =
      Narrow16(host.SampleSupportHeight(motion.position, next.x, next.z, 1));
  if (next.y == kNoGroundHeight) return;
  next.x = Narrow16(next.x + motion.position.x);
  next.z = Narrow16(next.z + motion.position.z);
  if (!host.HasCollisionCell(next, 0)) return;
  motion.position.x = next.x;
  motion.position.z = next.z;
  if (motion.movement_mode != kModeNone) {
    UpdateActorJumpAndFall(host, motion.slot, actor);
  } else if (next.y > motion.position.y &&
             next.y - motion.position.y >= kStepUpLimit) {
    motion.root_motion.x = 0;
    motion.root_motion.y = 0;
    motion.root_motion.z = 0;
    host.PlayAnimation(motion.slot, kJumpAnimation, kBlendQuick);
    motion.destination_mode = kDestinationNone;
    motion.locomotion_state = 0;
    motion.movement_pending = false;
    motion.movement_mode = kModeAirborne;
  } else {
    motion.position.y = next.y;
  }
  SetTileFromPosition(motion);
  motion.current_tile.layer = 0;
}

// decomp: BATTLE.PRG func_800A2790 (src/BATTLE/BATTLE.PRG/38C1C.c:677)
void ApplyActorFlightMovement(MovementHost& host, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  const MotionVector& step = actor.request_step;
  const auto speed = static_cast<int>(motion.request_distance);
  const int delta_x = WrappingMultiply(step.x, speed) / kFlightScale;
  const int delta_y = WrappingMultiply(step.y, speed) / kFlightScale;
  const int delta_z = WrappingMultiply(step.z, speed) / kFlightScale;
  motion.position.x = Narrow16(motion.position.x + delta_x);
  motion.position.y = Narrow16(motion.position.y + delta_y);
  motion.position.z = Narrow16(motion.position.z + delta_z);
  SetTileFromPosition(motion);
  motion.motion_locked = true;
  TurnActorToOverTime(motion, static_cast<std::uint16_t>(motion.request_facing),
                      kFacingTurnSteps);
  int animation = kIdleAnimation;
  if (speed > 0) {
    if (delta_y < kFlightDescendLimit) {
      animation = kFlightDescendAnimation;
    } else if (delta_y >= kFlightAscendLimit) {
      animation = kFlightAscendAnimation;
    } else {
      animation = kFlightLevelAnimation;
    }
    if (speed >= kFlightFastSpeed) animation += kFlightFastOffset;
  }
  if (actor.animation_id != animation) {
    host.PlayAnimation(motion.slot, animation, kBlendQuick);
  }
}

// decomp: BATTLE.PRG func_800A291C (src/BATTLE/BATTLE.PRG/38C1C.c:712)
void ApplyActorMovementVector(MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  const MotionVector& step = actor.request_step;
  motion.position.x = Narrow16(motion.position.x + step.x);
  motion.position.y = Narrow16(motion.position.y + step.y);
  motion.position.z = Narrow16(motion.position.z + step.z);
  SetTileFromPosition(motion);
  TurnActorToOverTime(motion, static_cast<std::uint16_t>(motion.request_facing),
                      kFacingTurnSteps);
}

// decomp: BATTLE.PRG func_800A29A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:86)
void UpdateActorTeleportTransition(MovementHost& host, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  const bool rides_parent = actor.attach_bone == kParentAttachBone;
  switch (actor.reaction_state) {
    case kTeleportBegin:
      motion.motion_locked = false;
      host.PlayAnimation(motion.slot, kTeleportOutAnimation, kBlendNormal);
      host.QueueAnimation(motion.slot, kTeleportOutLoopAnimation, kBlendNormal,
                          0);
      actor.base_color_code = 0;
      actor.reaction_state = kTeleportVanishing;
      motion.action_animation_state = kTeleportOutAction;
      return;
    case kTeleportVanishing: {
      if (motion.action_animation_state < kTeleportReadyAction) return;
      bool busy = false;
      if (rides_parent) {
        const std::optional<int> parent = ParentActionState(host, actor);
        busy = parent && *parent < kTeleportInAction;
      } else if (actor.pose_reached_end) {
        busy = actor.blend_frame != 0;
      } else {
        return;
      }
      if (busy) return;
      motion.action_animation_state = kTeleportInAction;
      TranslateActorToTileAtRate(host.Terrain(), motion, RouteTile(motion), 0);
      motion.facing = motion.route_target.w;
      host.PlayAnimation(motion.slot, kTeleportInAnimation, kBlendNormal);
      host.QueueAnimation(motion.slot, kTeleportInLoopAnimation, kBlendNormal,
                          0);
      actor.reaction_state = kTeleportAppearing;
      return;
    }
    case kTeleportAppearing:
      if (motion.action_animation_state != 0) return;
      if ((rides_parent ? ParentActionState(host, actor).value_or(0)
                        : actor.blend_frame) != 0) {
        return;
      }
      host.PlayAnimation(motion.slot, kIdleAnimation, kBlendNormal);
      [[fallthrough]];
    case kTeleportSettle:
      SaveGroundedTransform(actor);
      ReleaseCurrentTile(host, actor);
      [[fallthrough]];
    case kTeleportAwaitCompanion:
      actor.reaction_state = CompanionReacting(host, actor)
                                 ? kTeleportAwaitCompanion
                                 : kTeleportIdle;
      return;
    default:
      return;
  }
}

// decomp: BATTLE.PRG func_800A2C48 (src/BATTLE/BATTLE.PRG/3A1A0.c:153)
// The route target is the original's sharedSequences[4] alias.
void CancelActorTeleportTransition(MovementHost& host, MovementActor& actor) {
  std::array<bool, kActorSlots> cancelled{};
  MovementActor* current = &actor;
  if (current->motion.slot < kActorSlots) {
    cancelled.at(current->motion.slot) = true;
  }
  for (;;) {
    CancelOneTeleport(host, *current);
    const int companion = current->companion_slot;
    if (companion < kFirstCompanionSlot) return;
    if (companion >= kActorSlots || cancelled.at(companion)) {
      VS_LOG_WARN("actors", "teleport cancel stops at repeated companion {}",
                  companion);
      return;
    }
    cancelled.at(companion) = true;
    current = host.Actor(companion);
    if (current == nullptr) {
      VS_LOG_WARN("actors", "teleport cancel skips absent companion {}",
                  companion);
      return;
    }
  }
}

// decomp: BATTLE.PRG func_800A2CD4 (src/BATTLE/BATTLE.PRG/3A1A0.c:164)
void UpdateActorTravelTransition(MovementHost& host, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  switch (motion.pose_state) {
    case kTravelLeap:
      BeginTravelLeap(host, actor);
      return;
    case kTravelLeaping:
      StepActorTransitionDisplacement(motion);
      if (OnSupportLayer(host, motion.position, kTravelContactMode)) {
        motion.motion_locked = true;
        IdleTravel(host, actor);
        return;
      }
      if (motion.transition_frames == 0) {
        host.PlayAnimation(motion.slot, kTravelLandAnimation, kBlendNormal);
        motion.motion_locked = false;
        motion.pose_state = kTravelLanding;
      }
      return;
    case kTravelLanding:
      if (motion.move_frames == 0 && actor.pose_reached_end) {
        LandTravel(host, actor);
      }
      return;
    case kTravelGlide:
      BeginTravelGlide(host, actor);
      return;
    case kTravelGliding:
      StepActorTransitionDisplacement(motion);
      if (OnSupportLayer(host, motion.position, kGlideContactMode) ||
          motion.transition_frames == 0) {
        IdleTravel(host, actor);
      }
      return;
    case kTravelHolding:
      if (actor.animation_id != kTravelHoldAnimationA &&
          actor.animation_id != kTravelHoldAnimationB &&
          actor.blend_frame == 0) {
        motion.pose_state = kTravelIdle;
      }
      return;
    default:
      return;
  }
}

// decomp: BATTLE.PRG func_800A48CC (src/BATTLE/BATTLE.PRG/3A1A0.c:1048)
void BeginActorJumpMovement(MovementHost& host, int id, int direction,
                            int distance) {
  MovementActor* actor = host.Actor(id);
  if (actor == nullptr) {
    VS_LOG_WARN("actors", "jump movement skips absent actor {}", id);
    return;
  }
  ActorMotion& motion = actor->motion;
  if (motion.route_phase != 0 || motion.locomotion_state != 0) return;
  if (motion.movement_mode != kModeNone) {
    host.SetDirectMovement(id, direction, distance);
    return;
  }
  actor->jump_started = false;
  if (distance == 0) {
    motion.jump_profile = 0;
    host.PlayAnimation(id, kJumpAnimation, kBlendQuick);
    motion.jump_countdown = kStandingJumpCountdown;
    motion.movement_mode = kModeTakeoff;
  } else {
    motion.jump_profile = 1;
    if (distance / kFixedOne >= motion.run_speed) {
      motion.jump_profile = 2;
      actor->jump_started = true;
    }
    motion.jump_countdown = 0;
    motion.movement_mode = kModeRunUp;
  }
  motion.request_mode = kRequestJump;
  motion.request_direction = Narrow16(direction);
  motion.request_distance = static_cast<std::uint32_t>(distance);
}

// decomp: BATTLE.PRG func_800A4A88 (src/BATTLE/BATTLE.PRG/3A1A0.c:1107)
void UpdateActorPlatformContact(MovementHost& host, MovementActor& actor,
                                PlatformContactPhase phase) {
  switch (phase) {
    case PlatformContactPhase::kFollow:
      FollowPlatform(host, actor);
      return;
    case PlatformContactPhase::kAnchor:
      AnchorPlatform(host, actor);
      return;
    case PlatformContactPhase::kCommit:
      CommitPlatform(host, actor);
      return;
  }
}

// decomp: BATTLE.PRG func_800A4D8C (src/BATTLE/BATTLE.PRG/3A1A0.c:1182)
void UpdateRoomActorMovement(MovementHost& host) {
  for (int slot = 0; slot < kGimmickSlots; ++slot) {
    const std::optional<MovementGimmick> gimmick =
        host.GimmickForActorSlot(slot);
    if (gimmick && gimmick->bone_count != 0) {
      host.UpdateGimmickMotion(slot, host.TickSpeed() / 2);
    }
  }
  for (int id = 0; id < kActorSlots; ++id) {
    const MovementActor* actor = host.Actor(id);
    if (actor != nullptr && !actor->skipped) UpdateActorMovement(host, id);
  }
  host.SetActorUpdateComplete();
}

// decomp: BATTLE.PRG func_800A4E68 (src/BATTLE/BATTLE.PRG/3A1A0.c:1199)
void UpdateActorMovement(MovementHost& host, int id) {
  MovementActor* found = host.Actor(id);
  if (found == nullptr) {
    VS_LOG_WARN("actors", "movement update skips absent actor {}", id);
    return;
  }
  MovementActor& actor = *found;
  const std::uint8_t action = actor.motion.action_animation_state;
  if (action != 0 && action < kActionStatesBusyEnd) return;
  UpdateActorPlatformContact(host, actor, PlatformContactPhase::kFollow);
  if (id == kAshleySlot) {
    UpdateAshleyRequest(host, actor);
  } else {
    UpdateOtherRequest(host, id, actor);
    UpdateContactKind(host, actor);
  }
  UpdateActorPlatformContact(host, actor, PlatformContactPhase::kAnchor);
  UpdateActorPlatformContact(host, actor, PlatformContactPhase::kCommit);
  if (id != kAshleySlot) SetTileFromPosition(actor.motion);
  ClearActorMovementRequest(actor.motion);
}

// decomp: BATTLE.PRG func_800A5280 (src/BATTLE/BATTLE.PRG/3A1A0.c:1347)
void MoveActorWithCollision(MovementHost& host, int id, int direction,
                            int distance, const MoveInput& input) {
  if (id < 0 || id >= kActorSlots) return;
  MovementActor* found = host.Actor(id);
  if (found == nullptr || RefusesMove(*found)) return;
  // The original left the previous gimmick unset when an airborne or
  // climbing actor was rejected; it keeps the entry gimmick here.
  MoveContext ctx{.host = host,
                  .id = id,
                  .actor = *found,
                  .input = input,
                  .direction = direction,
                  .distance = distance,
                  .previous_area = found->support_gimmick};
  if (PlanStep(ctx) == Flow::kReturn) return;
  Flow flow = ResolveStep(ctx);
  if (flow == Flow::kContinue) {
    flow = ctx.result > 0 ? ApplyClimb(ctx) : ApplyLevelStep(ctx);
  }
  if (flow == Flow::kReturn) return;
  if (flow == Flow::kLand) {
    const MotionVector step = ctx.motion;
    SetActorTranslation(found->motion, step, 0);
  }
  UpdateLocomotion(ctx);
}

namespace {
constexpr int kWideActorRadius = 0x80;
constexpr int kTurnFramesPerDelta = 32;

[[nodiscard]] int WrapTurn(int delta) {
  if (delta >= kHalfTurn) return delta - kAngleOne;
  if (delta < -kHalfTurn) return delta + kAngleOne;
  return delta;
}
}  // namespace

// decomp: BATTLE.PRG func_800A6660 (src/BATTLE/BATTLE.PRG/3A1A0.c:1920)
void UpdateActorSteering(MovementActor& actor, int target_facing,
                         bool restart_turn) {
  ActorMotion& motion = actor.motion;
  if (target_facing == motion.facing) return;
  if (motion.turn_frames == 0) {
    TurnActorToOverTime(motion, static_cast<std::uint16_t>(target_facing),
                        kFacingTurnSteps);
  } else if (restart_turn) {
    motion.facing_delta = Narrow16(WrapTurn(target_facing - motion.facing));
    motion.turn_frames = kFacingTurnSteps;
  } else {
    const int turned = (motion.facing + motion.facing_delta) & kAngleMask;
    motion.facing_delta =
        Narrow16(motion.facing_delta + WrapTurn(target_facing - turned));
  }
  if (actor.collision_radius > kWideActorRadius) {
    const int frames = std::abs(motion.facing_delta / kTurnFramesPerDelta);
    if (motion.turn_frames < frames) {
      motion.turn_frames = static_cast<std::uint16_t>(frames);
    }
  }
}

// UpdateActorJumpAndFall (func_800AC690, 40564.c:2297).
namespace {
constexpr std::int16_t kTakeoffDrop = -30;
constexpr int kJumpingAnimationLimit = 7;
constexpr int kFallingAnimationLimit = 9;
constexpr int kBlendSlow = 16;
constexpr int kCeilingMask = 0x7FFF;
constexpr int kCeilingSign = 0x4000;
constexpr int kCeilingRange = 0x8000;
constexpr int kCompanionContactMode = 3;
constexpr int kContactSearchMargin = 0x40;
constexpr int kHeadClearance = 0xC0;
constexpr int kJumpTargetContactMode = 2;
constexpr int kBottomlessHeight = 3000;
constexpr unsigned kSurfaceIdMask = 0x1F;
constexpr int kGroundPlaneSurface = 20;
constexpr int kPlatformDropLimit = -0x80;
constexpr int kGimmickStandOffset = 0x80;
constexpr int kLandingDropUnit = 64;
constexpr int kHardLandingDrop = 4;
constexpr int kStunLandingDrop = 6;
constexpr std::uint8_t kStunLandingFrames = 8;
constexpr int kSoftLandingSound = 0x41;
constexpr int kHardLandingSound = 0x42;
constexpr int kActionAnimationProfile = 3;
constexpr int kTakeoffTimerEnd = 2;

enum class AirFlow : std::uint8_t { kSample, kFall, kLand };

[[nodiscard]] std::size_t ProfileIndex(int profile) {
  return static_cast<std::size_t>(profile);
}

// The airborne countdown's next value: it runs in half ticks.
[[nodiscard]] std::int8_t CountDown(MovementHost& host,
                                    const ActorMotion& motion) {
  return static_cast<std::int8_t>(motion.jump_countdown - host.TickSpeed() / 2);
}

void UpdateTakeoff(MovementHost& host, int id, MovementActor& actor,
                   int profile) {
  ActorMotion& motion = actor.motion;
  if (static_cast<std::int8_t>(motion.jump_countdown) > 0) {
    const std::int8_t timer = CountDown(host, motion);
    motion.jump_countdown = static_cast<std::uint8_t>(timer);
    if (timer < kTakeoffTimerEnd) {
      host.PlayAnimation(id,
                         host.Tables().air_timer_end.at(ProfileIndex(profile)),
                         kBlendNormal);
      motion.jump_countdown = 0;
    }
    return;
  }
  if (motion.destination_mode == kDestinationNone) {
    motion.root_motion.y = kTakeoffDrop;
  }
  actor.tick_remainder = 0;
  motion.movement_mode = kModeAirborne;
  if (actor.support_layer != 0) {
    SetLayerRider(host, actor.support_layer, kNoRider);
    actor.support_layer = 0;
  }
}

// False while the hang before the fall still counts down.
[[nodiscard]] bool AirborneCountdownDone(MovementHost& host,
                                         ActorMotion& motion) {
  if (static_cast<std::int8_t>(motion.jump_countdown) <= 0) return true;
  const std::int8_t timer = CountDown(host, motion);
  motion.jump_countdown = static_cast<std::uint8_t>(timer);
  if (timer > 0) return false;
  host.PlayAnimation(motion.slot, kJumpAnimation, kBlendNormal);
  motion.jump_countdown = 0;
  return true;
}

// Adds the tick's gravity; returns the speed the airborne animations switch
// at. A jump gains half a unit a half tick, carried in tick_remainder.
[[nodiscard]] int ApplyGravity(MovementHost& host, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  const int half_ticks = host.TickSpeed() / 2;
  int gain = 0;
  int limit = 0;
  if (motion.is_jumping) {
    actor.tick_remainder = Byte(actor.tick_remainder + half_ticks);
    limit = kJumpingAnimationLimit;
    gain = actor.tick_remainder >> 1;
    actor.tick_remainder = Byte(actor.tick_remainder - gain * 2);
  } else {
    limit = kFallingAnimationLimit;
    gain = half_ticks * kModeAirborne;
  }
  motion.root_motion.y = Narrow16(motion.root_motion.y + gain);
  return limit;
}

[[nodiscard]] int CeilingFloor(int height) {
  const int low = height & kCeilingMask;
  return (low & kCeilingSign) != 0 ? low - kCeilingRange : low;
}

// A rising actor that would pass the ceiling (or a platform above it)
// rebounds; a companion also drops its jump destination.
void BounceOffCeiling(MovementHost& host, int id, MovementActor& actor) {
  ActorMotion& motion = actor.motion;
  if (motion.root_motion.y >= 0) return;
  const MotionVector& position = motion.position;
  int mode = 1;
  int floor = CeilingFloor(host.CeilingHeight(position.x, position.z));
  if (id != kAshleySlot) {
    mode = kCompanionContactMode;
    host.SetContactSearchDepth(actor.collision_height + kContactSearchMargin);
  }
  const int ground =
      host.PlatformContactHeight(position.x, position.y, position.z, mode);
  if (ground != 0 && floor < ground) floor = ground;
  if (position.y + motion.root_motion.y - kHeadClearance >= floor) return;
  motion.root_motion.y = Narrow16(-motion.root_motion.y);
  if (id == kAshleySlot) return;
  motion.root_motion.x = 0;
  motion.root_motion.z = 0;
  motion.destination_mode = kDestinationNone;
  const PackedTile route = RouteTile(motion);
  host.ReleaseAiDestinationTile(route.x, route.z);
}

void PlayAirAnimation(MovementHost& host, int id, const MovementActor& actor,
                      int profile, int limit) {
  const int speed = actor.motion.root_motion.y;
  const MovementTables& tables = host.Tables();
  if (speed > limit) {
    host.PlayAnimation(id, tables.hard_landing.at(ProfileIndex(profile)),
                       kBlendNormal);
  } else if (speed >= -limit) {
    host.PlayAnimation(id, tables.soft_landing.at(ProfileIndex(profile)),
                       kBlendSlow);
  }
}

[[nodiscard]] bool PassesDestination(int speed, int next, int destination) {
  return speed > 0 ? destination < next : next < destination;
}

void StopAtDestination(ActorMotion& motion) {
  const int speed_x = motion.root_motion.x;
  if (speed_x != 0 && PassesDestination(speed_x, motion.position.x + speed_x,
                                        motion.destination_x)) {
    motion.root_motion.x = 0;
  }
  const int speed_z = motion.root_motion.z;
  if (speed_z != 0 && PassesDestination(speed_z, motion.position.z + speed_z,
                                        motion.destination_z)) {
    motion.root_motion.z = 0;
  }
}

// A tile jump lands at once on a moving platform other than its target's.
AirFlow FollowJumpTarget(MovementHost& host, int id, MovementActor& actor,
                         int& height) {
  ActorMotion& motion = actor.motion;
  const int rise = motion.root_motion.y;
  actor.jump_frames = Narrow16(actor.jump_frames - 1);
  if (rise < 0) return AirFlow::kFall;
  height = host.PlatformContactHeight(
      motion.position.x + motion.root_motion.x, motion.position.y + rise,
      motion.position.z + motion.root_motion.z, kJumpTargetContactMode);
  const int layer = host.CurrentSupport().layer;
  if (layer != 0 && layer != RouteTile(motion).layer) {
    actor.contact_kind = Byte(layer);
    SetTileFromPosition(motion);
    motion.current_tile.layer = 0;
    SetLayerRider(host, layer, id);
    SetLayerRider(host, layer, kNoRider);
    host.ClearSupportLayer();
    host.ForceReleaseAiReservations(id);
    return AirFlow::kLand;
  }
  if (actor.jump_frames > 0) return AirFlow::kFall;
  StopAtDestination(motion);
  return AirFlow::kSample;
}

AirFlow FollowDestination(MovementHost& host, int id, MovementActor& actor,
                          int& height) {
  ActorMotion& motion = actor.motion;
  switch (motion.destination_mode) {
    case kDestinationNone:
      return motion.root_motion.y < 0 ? AirFlow::kFall : AirFlow::kSample;
    case kDestinationSlide:
      StopAtDestination(motion);
      return AirFlow::kSample;
    case kDestinationJump:
      return FollowJumpTarget(host, id, actor, height);
    default:
      return AirFlow::kSample;
  }
}

// The support under the fall's next step; the fall lands when it reaches it.
AirFlow SampleLanding(MovementHost& host, MovementActor& actor, int& height) {
  ActorMotion& motion = actor.motion;
  const MotionVector& position = motion.position;
  height = host.SampleSupportHeight(position, motion.root_motion.x,
                                    motion.root_motion.z, 1);
  const int top = position.y + motion.root_motion.y;
  if (height == kBottomlessHeight &&
      host.TileSurfaceId(static_cast<int>(
          host.Probe().cell_word & kSurfaceIdMask)) == kGroundPlaneSurface &&
      position.y <= 0 && top >= 0) {
    host.SpawnGroundImpact(position.x, position.z);
  }
  if (top < height) return AirFlow::kFall;
  if (host.CurrentSupport().layer != 0 && height - top < kPlatformDropLimit) {
    motion.root_motion.x = 0;
    motion.root_motion.z = 0;
    return AirFlow::kFall;
  }
  return AirFlow::kLand;
}

void PlayLandingSound(MovementHost& host, int id, MovementActor& actor,
                      int drop) {
  const std::uint8_t action = actor.motion.action_animation_state;
  if (actor.motion.slot == kAshleySlot && action != 0 &&
      action < kActionStatesBusyEnd) {
    return;
  }
  if (drop < kHardLandingDrop) {
    host.PlaySurfaceContactSound(id, actor, kSoftLandingSound);
    return;
  }
  host.PlaySurfaceContactSound(id, actor, kHardLandingSound);
  if (drop >= kStunLandingDrop && id == kAshleySlot) {
    actor.motion.jump_countdown = kStunLandingFrames;
  }
}

// A moving gimmick carries the actor on its top instead of landing it.
[[nodiscard]] bool RidesGimmick(MovementHost& host, ActorMotion& motion,
                                int gimmick_slot) {
  if (gimmick_slot == 0) return false;
  const std::optional<MovementGimmick> gimmick =
      host.GimmickForActorSlot(gimmick_slot);
  if (!gimmick) {
    VS_LOG_WARN("field.gimmick", "actor {} lands on absent gimmick {}",
                static_cast<int>(motion.slot), gimmick_slot);
    return false;
  }
  if (gimmick->mode == 0) return false;
  motion.position.y = Narrow16(gimmick->position_y - kGimmickStandOffset);
  return true;
}

void Land(MovementHost& host, int id, MovementActor& actor, int profile,
          int height) {
  ActorMotion& motion = actor.motion;
  const SupportContact support = host.CurrentSupport();
  if (RidesGimmick(host, motion, support.gimmick)) return;
  actor.platform_offset_x = 0;
  actor.platform_offset_z = 0;
  actor.support_layer = Byte(support.layer & kNibbleMask);
  motion.current_tile.layer = Byte(support.layer);
  actor.support_gimmick = Byte(support.gimmick & kNibbleMask);
  motion.root_motion.y = Narrow16(height - motion.position.y);
  motion.movement_mode = kModeLanding;
  const MotionVector step = motion.root_motion;
  SetActorTranslation(motion, step, 0);
  const int drop =
      (motion.position.y - actor.grounded_position[1]) / kLandingDropUnit;
  motion.jump_countdown = 0;
  if (height < kBottomlessHeight) PlayLandingSound(host, id, actor, drop);
  motion.destination_mode = kDestinationNone;
  actor.landing_pending = false;
  host.PlayAnimation(id,
                     host.Tables().landing_recovery.at(ProfileIndex(profile)),
                     kBlendNormal);
}

void UpdateAirborne(MovementHost& host, int id, MovementActor& actor,
                    int profile) {
  if (!AirborneCountdownDone(host, actor.motion)) return;
  const int limit = ApplyGravity(host, actor);
  BounceOffCeiling(host, id, actor);
  PlayAirAnimation(host, id, actor, profile, limit);
  int height = 0;
  AirFlow flow = FollowDestination(host, id, actor, height);
  if (flow == AirFlow::kSample) flow = SampleLanding(host, actor, height);
  if (flow == AirFlow::kLand) {
    Land(host, id, actor, profile, height);
    return;
  }
  const MotionVector step = actor.motion.root_motion;
  SetActorTranslation(actor.motion, step, 0);
}

void UpdateLanding(MovementHost& host, int id, MovementActor& actor,
                   int profile) {
  ActorMotion& motion = actor.motion;
  if (motion.motion_locked) {
    motion.motion_locked = false;
    motion.is_jumping = false;
  }
  SaveGroundedTransform(actor);
  const std::int8_t countdown =
      std::max<std::int8_t>(CountDown(host, motion), 0);
  motion.jump_countdown = static_cast<std::uint8_t>(countdown);
  if (countdown != 0) return;
  actor.ledge_height = 0;
  if (motion.slot != kAshleySlot) {
    if (!motion.is_jumping && actor.blend_frame != 0) return;
    if (profile != kActionAnimationProfile) {
      host.PlayAnimation(id, kIdleAnimation, kBlendNormal);
    }
    host.ReleaseAiDestinationTile(motion.current_tile.x, motion.current_tile.z);
    host.ReleaseAiContactOccupancy(motion.slot);
  }
  motion.run_frames = 0;
  motion.movement_mode = kModeNone;
}
}  // namespace

// decomp: BATTLE.PRG func_800AC690 (src/BATTLE/BATTLE.PRG/40564.c:2297)
void UpdateActorJumpAndFall(MovementHost& host, int id, MovementActor& actor) {
  const int profile = ActorJumpAnimationProfile(actor.motion);
  switch (actor.motion.movement_mode) {
    case kModeTakeoff:
      UpdateTakeoff(host, id, actor, profile);
      return;
    case kModeAirborne:
      UpdateAirborne(host, id, actor, profile);
      return;
    case kModeLanding:
      UpdateLanding(host, id, actor, profile);
      return;
    default:
      return;
  }
}
}  // namespace VS::Actors
