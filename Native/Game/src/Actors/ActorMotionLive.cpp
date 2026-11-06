// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed actor motion rules (ActorMotion.cpp).
#include "Actors/ActorMotionLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdlib>
#include <iterator>

#include "Actors/AnimationControlLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/CollisionMapLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Field/Collision.h"
}

static_assert(sizeof(Actors_TileTarget) == sizeof(VS::Actors::PackedTile));
static_assert(sizeof(SVECTOR) == sizeof(VS::Actors::MotionVector));

namespace VS::Actors {
namespace {
constexpr int kRouteBlendFrames = 8;

// Cross-slice calls (rows 19, 20): retarget when those rows are promoted.
// Row 23 (Field collision) is the typed ActorCollision and CollisionMap Live
// layer. Row 20 (Animation): the actor animation. Row 19 (actor state): the
// battle actor table.
int LiveSupportHeight(SVECTOR& origin, SupportProbe probe) {
  return VS::Field::LiveSampleActorSupportHeight(&origin, 0, 0,
                                                 static_cast<int>(probe));
}
int LiveInterpolatedFloorHeight(int x, int z) {
  return VS::Field::LiveInterpolatedFloorHeight(x, z);
}
int LiveGimmickSupportHeight(int tile_x, int tile_z) {
  return VS::Field::LiveGimmickSupportHeight(tile_x, tile_z);
}
int LiveCollisionBypass(int actor_id) {
  return VS::Field::LiveActorCollisionBypass(actor_id);
}
void SetLiveCollisionBypass(int actor_id, int flags) {
  VS::Field::SetLiveActorCollisionBypass(actor_id, flags);
}
void PlayLiveRouteAnimation(int actor_id, int animation) {
  PlayLiveActorAnimation(actor_id, animation, 0, kRouteBlendFrames);
}
std::uint8_t* LiveBattleCommandState(int actor_id) {
  if (actor_id < 0 ||
      static_cast<std::size_t>(actor_id) >= std::size(Battle_actors)) {
    return nullptr;
  }
  Battle_Actor* actor = Battle_actors[actor_id];
  return actor != nullptr ? &actor->commandState : nullptr;
}
// End of the cross-slice calls.

Actors_ActorRecord* Record(int actor_id) {
  if (actor_id < 0 ||
      static_cast<std::size_t>(actor_id) >= std::size(Actors_actorRecords)) {
    Core_Warnf("actors",
               "Actor motion refused: actor id %d lies outside the actor table",
               actor_id);
    return nullptr;
  }
  return Actors_actorRecords[actor_id];
}

template <typename Pointer>
Pointer& Required(Pointer* value, const char* message) {
  if (value == nullptr) {
    Core_Errorf("actors", "%s", message);
    std::abort();
  }
  return *value;
}

MotionVector VectorArgument(const SVECTOR* value) {
  return LiveMotionVector(Required(value, "Actor motion requires a vector"));
}

PackedTile TileArgument(const Actors_TileTarget* tile) {
  return std::bit_cast<PackedTile>(
      Required(tile, "Actor motion requires a tile target"));
}

// Runs `rule` over the motion of `actor` and stores it back.
template <typename Rule>
void WithRecord(Actors_ActorRecord& actor, Rule rule) {
  ActorMotion motion = LoadLiveActorMotion(actor);
  rule(motion);
  StoreLiveActorMotion(motion, actor);
}

// Runs `rule` for the actor `actor_id` names; nothing for an empty slot,
// which `absent` (when given) names in the log.
template <typename Rule>
void WithActor(int actor_id, Rule rule, const char* absent = nullptr) {
  Actors_ActorRecord* actor = Record(actor_id);
  if (actor == nullptr) {
    if (absent != nullptr) {
      Core_Debugf("actors", "%s skips absent actor=%d", absent, actor_id);
    }
    return;
  }
  WithRecord(*actor, rule);
}

// The record-pointer entries the originals dereference without a check.
template <typename Rule>
void WithPointer(Actors_ActorRecord* actor, const char* name, Rule rule) {
  if (actor == nullptr) {
    Core_Warnf("actors", "%s refused: no actor record", name);
    return;
  }
  WithRecord(*actor, rule);
}

class LiveMotionTerrain final : public MotionTerrainHost {
 public:
  int SupportHeight(const MotionVector& origin, SupportProbe probe) override {
    SVECTOR point = LiveSvector(origin);
    return LiveSupportHeight(point, probe);
  }
  int InterpolatedFloorHeight(int x, int z) override {
    return LiveInterpolatedFloorHeight(x, z);
  }
  int GimmickSupportHeight(int tile_x, int tile_z) override {
    return LiveGimmickSupportHeight(tile_x, tile_z);
  }
};

class LiveScriptedPath final : public ScriptedPathHost {
 public:
  explicit LiveScriptedPath(int actor_id) : id_(actor_id) {}

  int CollisionBypass() override { return LiveCollisionBypass(id_); }
  void SetCollisionBypass(int flags) override {
    SetLiveCollisionBypass(id_, flags);
  }
  void SetNavigationOverride(int x, int z) override {
    Battle::Ai::SetLiveActorNavigationOverride(id_, x, z);
  }
  int NavigationStatus() override {
    return Battle::Ai::LiveActorNavigationOverrideStatus(id_);
  }

 private:
  int id_;
};

class LiveActorRoute final : public ActorRouteHost {
 public:
  LiveActorRoute(int actor_id, Actors_ActorRecord& actor)
      : id_(actor_id), actor_(actor) {}

  ActorRoute Route() override {
    return {.phase = actor_.routePhase,
            .uses_duration = actor_.routeUsesDuration != 0,
            .target = LiveMotionVector(actor_.routeTarget),
            .move_rate = actor_.routeMoveRate,
            .turn_rate = actor_.routeTurnRate};
  }
  void SetRoutePhase(int phase) override {
    actor_.routePhase = static_cast<u_char>(phase);
  }
  int TurnFrames() override { return actor_.instance.turnFrames; }
  int MoveFrames() override { return actor_.instance.currentAnimation; }
  bool MotionLocked() override { return actor_.instance.motionLocked != 0; }
  int TravelAnimation() override { return actor_.travelAnimation; }
  void PlayAnimation(int animation) override {
    PlayLiveRouteAnimation(id_, animation);
  }
  void TranslateTo(const MotionVector& target, int rate,
                   bool over_time) override {
    SVECTOR point = LiveSvector(target);
    if (over_time) {
      TranslateLiveActorToOverTime(id_, &point, rate);
    } else {
      TranslateLiveActorToAtRate(id_, &point, rate);
    }
  }
  void TurnTo(int target_yaw, int rate, bool over_time) override {
    if (over_time) {
      TurnLiveActorToOverTime(id_, static_cast<std::uint16_t>(target_yaw),
                              rate);
    } else {
      TurnLiveActorToAtRate(id_, static_cast<std::int16_t>(target_yaw), rate);
    }
  }

 private:
  int id_;
  Actors_ActorRecord& actor_;
};
}  // namespace

MotionVector LiveMotionVector(const SVECTOR& live) {
  return {.x = live.vx, .y = live.vy, .z = live.vz, .w = live.pad};
}

SVECTOR LiveSvector(const MotionVector& motion) {
  return SVECTOR{
      .vx = motion.x, .vy = motion.y, .vz = motion.z, .pad = motion.w};
}

ActorMotion LoadLiveActorMotion(const Actors_ActorRecord& live) {
  const Actors_ModelInstance& instance = live.instance;
  ActorMotion motion;
  motion.action_animation_state = instance.actionAnimationState;
  motion.movement_pending = instance.movementPending != 0;
  motion.movement_mode = instance.movementMode;
  motion.locomotion_state = instance.locomotionState;
  motion.motion_locked = instance.motionLocked != 0;
  motion.is_jumping = instance.isJumping != 0;
  motion.pose_state = instance.poseState;
  motion.slot = instance.slot;
  motion.jump_countdown = instance.jumpCountdown;
  motion.turn_frames = instance.turnFrames;
  motion.move_frames = instance.currentAnimation;
  motion.position = LiveMotionVector(instance.position);
  motion.rotation_x = instance.rotationX;
  motion.facing = instance.facing;
  motion.rotation_z = instance.rotationZ;
  motion.scale = {
      .x = instance.scaleX, .y = instance.scaleY, .z = instance.scaleZ};
  motion.root_motion = LiveMotionVector(instance.rootMotion);
  motion.rotation_step_x = instance.rotationStepX;
  motion.facing_delta = instance.facingDelta;
  motion.rotation_step_z = instance.rotationStepZ;
  motion.scale_delta = LiveMotionVector(instance.scaleDelta);
  motion.current_tile = {.x = instance.currentTileX,
                         .layer = instance.currentTileLayer,
                         .z = instance.currentTileZ};
  motion.route_phase = live.routePhase;
  motion.route_uses_duration = live.routeUsesDuration != 0;
  motion.action_pose_active = live.actionPoseActive != 0;
  motion.is_running = live.isRunning != 0;
  motion.weapon_pose_active = live.weaponPoseActive != 0;
  motion.jump_profile = live.jumpProfile;
  motion.run_speed = live.runSpeed;
  motion.walk_speed = live.walkSpeed;
  motion.turn_rate = live.turnRate;
  motion.move_rate = live.moveRate;
  motion.route_move_rate = live.routeMoveRate;
  motion.route_turn_rate = live.routeTurnRate;
  motion.scale_frames = live.scaleFrames;
  motion.transition_frames = live.transitionFrames;
  motion.route_target = LiveMotionVector(live.routeTarget);
  motion.destination_x = live.destinationX;
  motion.destination_z = live.destinationZ;
  motion.destination_mode = live.destinationMode;
  motion.momentum_distance = live.momentumDistance;
  motion.run_frames = live.runFrames;
  motion.request_mode = live.turnState.mode;
  motion.request_direction = live.turnState.direction;
  motion.request_facing = live.turnState.facing;
  motion.request_distance = live.turnState.distance;
  return motion;
}

void StoreLiveActorMotion(const ActorMotion& motion, Actors_ActorRecord& live) {
  Actors_ModelInstance& instance = live.instance;
  instance.actionAnimationState = motion.action_animation_state;
  instance.movementPending = motion.movement_pending ? 1 : 0;
  instance.movementMode = motion.movement_mode;
  instance.locomotionState = motion.locomotion_state;
  instance.motionLocked = motion.motion_locked ? 1 : 0;
  instance.isJumping = motion.is_jumping ? 1 : 0;
  instance.poseState = motion.pose_state;
  instance.slot = motion.slot;
  instance.jumpCountdown = motion.jump_countdown;
  instance.turnFrames = motion.turn_frames;
  instance.currentAnimation = motion.move_frames;
  instance.position = LiveSvector(motion.position);
  instance.rotationX = motion.rotation_x;
  instance.facing = motion.facing;
  instance.rotationZ = motion.rotation_z;
  instance.scaleX = motion.scale.x;
  instance.scaleY = motion.scale.y;
  instance.scaleZ = motion.scale.z;
  instance.rootMotion = LiveSvector(motion.root_motion);
  instance.rotationStepX = motion.rotation_step_x;
  instance.facingDelta = motion.facing_delta;
  instance.rotationStepZ = motion.rotation_step_z;
  instance.scaleDelta = LiveSvector(motion.scale_delta);
  instance.currentTileX = motion.current_tile.x;
  instance.currentTileLayer = motion.current_tile.layer;
  instance.currentTileZ = motion.current_tile.z;
  live.routePhase = motion.route_phase;
  live.routeUsesDuration = motion.route_uses_duration ? 1 : 0;
  live.actionPoseActive = motion.action_pose_active ? 1 : 0;
  live.isRunning = motion.is_running ? 1 : 0;
  live.weaponPoseActive = motion.weapon_pose_active ? 1 : 0;
  live.jumpProfile = motion.jump_profile;
  live.runSpeed = motion.run_speed;
  live.walkSpeed = motion.walk_speed;
  live.turnRate = motion.turn_rate;
  live.moveRate = motion.move_rate;
  live.routeMoveRate = motion.route_move_rate;
  live.routeTurnRate = motion.route_turn_rate;
  live.scaleFrames = motion.scale_frames;
  live.transitionFrames = motion.transition_frames;
  live.routeTarget = LiveSvector(motion.route_target);
  live.destinationX = motion.destination_x;
  live.destinationZ = motion.destination_z;
  live.destinationMode = motion.destination_mode;
  live.momentumDistance = motion.momentum_distance;
  live.runFrames = motion.run_frames;
  live.turnState.mode = motion.request_mode;
  live.turnState.direction = motion.request_direction;
  live.turnState.facing = motion.request_facing;
  live.turnState.distance = motion.request_distance;
}

void BeginLiveScriptedActorPath(int actor_id, SVECTOR* target_position) {
  LiveScriptedPath host(actor_id);
  BeginScriptedActorPath(host, VectorArgument(target_position));
}

int IsLiveScriptedActorPathFinished(int actor_id) {
  LiveScriptedPath host(actor_id);
  return IsScriptedActorPathFinished(host) ? 1 : 0;
}

void ResetLiveActorMovement(int actor_id) {
  std::uint8_t* command_state = LiveBattleCommandState(actor_id);
  Actors_ActorRecord* actor = Record(actor_id);
  if (actor == nullptr) {
    ResetActorMovement(command_state, nullptr);
    return;
  }
  WithRecord(*actor, [&](ActorMotion& motion) {
    ResetActorMovement(command_state, &motion);
  });
}

void StopLiveActorMotion(int actor_id) {
  WithActor(actor_id, [](ActorMotion& motion) { StopActorMotion(motion); });
}

void ClearLiveActorMovementModes(int actor_id) {
  WithActor(actor_id,
            [](ActorMotion& motion) { ClearActorMovementModes(motion); });
}

int LiveActorFacingDeltaToPoint(Actors_ActorRecord* actor,
                                SVECTOR* target_point) {
  if (target_point == nullptr) return 0;
  if (actor == nullptr) {
    Core_Warnf("actors", "ActorFacingDeltaToPoint refused: no actor record");
    return 0;
  }
  const MotionVector target = LiveMotionVector(*target_point);
  return ActorFacingDeltaToPoint(LoadLiveActorMotion(*actor), &target);
}

void ResetLiveActorRunMomentum(int actor_id) {
  WithActor(actor_id,
            [](ActorMotion& motion) { ResetActorRunMomentum(motion); });
}

void SetLiveActorWalkRunSpeeds(int actor_id, int walk_speed_fixed,
                               int run_speed_fixed) {
  WithActor(actor_id, [&](ActorMotion& motion) {
    SetActorWalkRunSpeeds(motion, walk_speed_fixed, run_speed_fixed);
  });
}

void SetLiveActorTravelStepCounts(int actor_id, int move_steps_per_tile,
                                  int turn_steps_per_revolution) {
  WithActor(actor_id, [&](ActorMotion& motion) {
    SetActorTravelStepCounts(motion, move_steps_per_tile,
                             turn_steps_per_revolution);
  });
}

void LiveActorTravelStepCounts(int actor_id, int* move_steps_per_tile_out,
                               int* turn_steps_per_revolution_out) {
  const Actors_ActorRecord* actor = Record(actor_id);
  if (actor == nullptr) return;
  const TravelStepCounts counts =
      ActorTravelStepCounts(LoadLiveActorMotion(*actor));
  Required(move_steps_per_tile_out, "Actor step counts require outputs") =
      counts.move_steps_per_tile;
  Required(turn_steps_per_revolution_out, "Actor step counts require outputs") =
      counts.turn_steps_per_revolution;
}

void StepLiveActorTransitionDisplacement(Actors_ActorRecord* actor) {
  WithPointer(
      actor, "StepActorTransitionDisplacement",
      [](ActorMotion& motion) { StepActorTransitionDisplacement(motion); });
}

void SetLiveActorFallDestination(Actors_ActorRecord* actor,
                                 Actors_FallDirection* fall_direction) {
  const Actors_FallDirection& direction =
      Required(fall_direction, "Fall destination requires a direction");
  WithPointer(actor, "SetActorFallDestination", [&](ActorMotion& motion) {
    SetActorFallDestination(motion, direction.directionX, direction.directionZ);
  });
}

void ClearLiveActorMovementRequest(Actors_ActorRecord* actor) {
  WithPointer(actor, "ClearActorMovementRequest",
              [](ActorMotion& motion) { ClearActorMovementRequest(motion); });
}

int LiveActorJumpAnimationProfile(Actors_ActorRecord* actor) {
  const Actors_ActorRecord& record =
      Required(actor, "Jump animation profile requires an actor");
  return ActorJumpAnimationProfile(LoadLiveActorMotion(record));
}

void MoveLiveActorToTileAtRate(int actor_id, Actors_TileTarget* tile_target,
                               int move_steps_per_tile,
                               int turn_steps_per_revolution) {
  const PackedTile tile = TileArgument(tile_target);
  LiveMotionTerrain terrain;
  WithActor(
      actor_id,
      [&](ActorMotion& motion) {
        MoveActorToTileAtRate(terrain, motion, tile, move_steps_per_tile,
                              turn_steps_per_revolution);
      },
      "movement wrapper");
}

void MoveLiveActorToPositionAtRate(int actor_id, SVECTOR* target,
                                   int move_steps_per_tile,
                                   int turn_steps_per_revolution) {
  const MotionVector position = VectorArgument(target);
  LiveMotionTerrain terrain;
  WithActor(
      actor_id,
      [&](ActorMotion& motion) {
        MoveActorToPositionAtRate(terrain, motion, position,
                                  move_steps_per_tile,
                                  turn_steps_per_revolution);
      },
      "movement");
}

void TranslateLiveActorToTileAtRate(int actor_id,
                                    Actors_TileTarget* tile_target,
                                    int move_steps_per_tile) {
  const PackedTile tile = TileArgument(tile_target);
  LiveMotionTerrain terrain;
  WithActor(actor_id, [&](ActorMotion& motion) {
    TranslateActorToTileAtRate(terrain, motion, tile, move_steps_per_tile);
  });
}

void TurnLiveActorTowardTileAtRate(int actor_id, Actors_TileTarget* tile_target,
                                   int turn_steps_per_revolution) {
  const PackedTile tile = TileArgument(tile_target);
  WithActor(actor_id, [&](ActorMotion& motion) {
    TurnActorTowardTileAtRate(motion, tile, turn_steps_per_revolution);
  });
}

void TranslateLiveActorByAtRate(int actor_id, SVECTOR* translation,
                                int move_steps_per_tile) {
  const MotionVector step = VectorArgument(translation);
  WithActor(actor_id, [&](ActorMotion& motion) {
    TranslateActorByAtRate(motion, step, move_steps_per_tile);
  });
}

void TranslateLiveActorToAtRate(int actor_id, SVECTOR* target_position,
                                int move_steps_per_tile) {
  const MotionVector target = VectorArgument(target_position);
  WithActor(actor_id, [&](ActorMotion& motion) {
    TranslateActorToAtRate(motion, target, move_steps_per_tile);
  });
}

void TurnLiveActorByAtRate(int actor_id, std::int16_t yaw_delta,
                           int turn_steps_per_revolution) {
  WithActor(actor_id, [&](ActorMotion& motion) {
    TurnActorByAtRate(motion, yaw_delta, turn_steps_per_revolution);
  });
}

void TurnLiveActorToAtRate(int actor_id, std::int16_t target_yaw,
                           int turn_steps_per_revolution) {
  WithActor(actor_id, [&](ActorMotion& motion) {
    TurnActorToAtRate(motion, target_yaw, turn_steps_per_revolution);
  });
}

void RotateLiveActorByAtRate(int actor_id, SVECTOR* rotation_delta,
                             int turn_steps_per_revolution) {
  const MotionVector delta = VectorArgument(rotation_delta);
  WithActor(
      actor_id,
      [&](ActorMotion& motion) {
        RotateActorByAtRate(motion, delta, turn_steps_per_revolution);
      },
      "RotateActorByAtRate");
}

void RotateLiveActorToAtRate(int actor_id, SVECTOR* target_rotation,
                             int turn_steps_per_revolution) {
  const MotionVector target = VectorArgument(target_rotation);
  WithActor(
      actor_id,
      [&](ActorMotion& motion) {
        RotateActorToAtRate(motion, target, turn_steps_per_revolution);
      },
      "RotateActorToAtRate");
}

void MoveLiveActorToTileOverTime(int actor_id, Actors_TileTarget* tile_target,
                                 int move_steps, int turn_steps) {
  const PackedTile tile = TileArgument(tile_target);
  LiveMotionTerrain terrain;
  WithActor(actor_id, [&](ActorMotion& motion) {
    MoveActorToTileOverTime(terrain, motion, tile, move_steps, turn_steps);
  });
}

void TranslateLiveActorToTileOverTime(int actor_id,
                                      Actors_TileTarget* tile_target,
                                      int move_steps) {
  const PackedTile tile = TileArgument(tile_target);
  LiveMotionTerrain terrain;
  WithActor(actor_id, [&](ActorMotion& motion) {
    TranslateActorToTileOverTime(terrain, motion, tile, move_steps);
  });
}

void SetLiveActorTileTranslation(int actor_id, Actors_TileTarget* tile_target,
                                 int move_steps, int move_steps_per_tile) {
  const PackedTile tile = TileArgument(tile_target);
  LiveMotionTerrain terrain;
  WithActor(actor_id, [&](ActorMotion& motion) {
    SetActorTileTranslation(terrain, motion, tile, move_steps,
                            move_steps_per_tile);
  });
}

void TurnLiveActorTowardTileOverTime(int actor_id,
                                     Actors_TileTarget* tile_target,
                                     int turn_steps) {
  const PackedTile tile = TileArgument(tile_target);
  WithActor(
      actor_id,
      [&](ActorMotion& motion) {
        TurnActorTowardTileOverTime(motion, tile, turn_steps);
      },
      "TurnActorTowardTileOverTime");
}

void SetLiveActorTileTurn(int actor_id, Actors_TileTarget* tile_target,
                          int turn_steps, int turn_steps_per_revolution) {
  const PackedTile tile = TileArgument(tile_target);
  WithActor(actor_id, [&](ActorMotion& motion) {
    SetActorTileTurn(motion, tile, turn_steps, turn_steps_per_revolution);
  });
}

void SetLiveActorMotion(int actor_id, SVECTOR* translation, int move_steps) {
  const MotionVector step = VectorArgument(translation);
  WithActor(actor_id, [&](ActorMotion& motion) {
    SetActorMotion(motion, step, move_steps);
  });
}

void TranslateLiveActorToOverTime(int actor_id, SVECTOR* target_position,
                                  int move_steps) {
  const MotionVector target = VectorArgument(target_position);
  WithActor(actor_id, [&](ActorMotion& motion) {
    TranslateActorToOverTime(motion, target, move_steps);
  });
}

void SetLiveActorTranslation(int actor_id, SVECTOR* translation,
                             int move_steps) {
  const MotionVector step = VectorArgument(translation);
  WithActor(actor_id, [&](ActorMotion& motion) {
    SetActorTranslation(motion, step, move_steps);
  });
}

void SetLiveActorYawMotion(int actor_id, std::int16_t yaw_delta,
                           int turn_steps) {
  WithActor(actor_id, [&](ActorMotion& motion) {
    SetActorYawMotion(motion, yaw_delta, turn_steps);
  });
}

void TurnLiveActorToOverTime(int actor_id, std::uint16_t target_yaw,
                             int turn_steps) {
  WithActor(actor_id, [&](ActorMotion& motion) {
    TurnActorToOverTime(motion, target_yaw, turn_steps);
  });
}

void TurnLiveActorToAtAngularSpeed(int actor_id, std::int16_t target_yaw,
                                   int angular_speed) {
  WithActor(actor_id, [&](ActorMotion& motion) {
    TurnActorToAtAngularSpeed(motion, target_yaw, angular_speed);
  });
}

void SetLiveActorYawTurn(int actor_id, std::int16_t yaw_delta, int turn_steps) {
  WithActor(actor_id, [&](ActorMotion& motion) {
    SetActorYawTurn(motion, yaw_delta, turn_steps);
  });
}

void SetLiveActorRotationMotion(int actor_id, SVECTOR* rotation_delta,
                                int turn_steps) {
  const MotionVector delta = VectorArgument(rotation_delta);
  WithActor(
      actor_id,
      [&](ActorMotion& motion) {
        SetActorRotationMotion(motion, delta, turn_steps);
      },
      "SetActorRotationMotion");
}

void SetLiveActorRotation(int actor_id, SVECTOR* rotation_delta,
                          int turn_steps) {
  const MotionVector delta = VectorArgument(rotation_delta);
  WithActor(
      actor_id,
      [&](ActorMotion& motion) { SetActorRotation(motion, delta, turn_steps); },
      "SetActorRotation");
}

void ScaleLiveActorToOverTime(int actor_id, SVECTOR* target_scale,
                              int scale_steps) {
  const MotionVector scale = VectorArgument(target_scale);
  WithActor(actor_id, [&](ActorMotion& motion) {
    ScaleActorToOverTime(motion, scale, scale_steps);
  });
}

void UpdateLiveActorRoute(int actor_id, Actors_ActorRecord* actor) {
  if (actor == nullptr) {
    Core_Warnf("actors", "UpdateActorRoute refused: no actor record");
    return;
  }
  LiveActorRoute host(actor_id, *actor);
  UpdateActorRoute(host);
}
}  // namespace VS::Actors
