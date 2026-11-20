// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed actor motion rules (ActorMotion.cpp).
#include "Actors/ActorMotionLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdlib>
#include <iterator>

#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/Location.hpp"
#include "Actors/Movement.hpp"
#include "Battle/Actor.h"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/CollisionMapLive.hpp"

static_assert(sizeof(VS::Actors::TileTarget) == sizeof(VS::Actors::PackedTile));
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
      static_cast<std::size_t>(actor_id) >= std::size(VS::Battle::actors)) {
    return nullptr;
  }
  Battle_Actor* actor = VS::Battle::actors[actor_id];
  return actor != nullptr ? &actor->commandState : nullptr;
}
// End of the cross-slice calls.

VS::Actors::ActorRecord* Record(int actor_id) {
  if (actor_id < 0 || static_cast<std::size_t>(actor_id) >=
                          std::size(VS::Actors::actorRecords)) {
    VS::Core::Warnf(
        "actors",
        "Actor motion refused: actor id %d lies outside the actor table",
        actor_id);
    return nullptr;
  }
  return VS::Actors::actorRecords[actor_id];
}

template <typename Pointer>
Pointer& Required(Pointer* value, const char* message) {
  if (value == nullptr) {
    VS::Core::Errorf("actors", "%s", message);
    std::abort();
  }
  return *value;
}

MotionVector VectorArgument(const SVECTOR* value) {
  return LiveMotionVector(Required(value, "Actor motion requires a vector"));
}

PackedTile TileArgument(const VS::Actors::TileTarget* tile) {
  return std::bit_cast<PackedTile>(
      Required(tile, "Actor motion requires a tile target"));
}

// Runs `rule` over the motion of `actor` and stores it back.
template <typename Rule>
void WithRecord(VS::Actors::ActorRecord& actor, Rule rule) {
  ActorMotion motion = LoadLiveActorMotion(actor);
  rule(motion);
  StoreLiveActorMotion(motion, actor);
}

// Runs `rule` for the actor `actor_id` names; nothing for an empty slot,
// which `absent` (when given) names in the log.
template <typename Rule>
void WithActor(int actor_id, Rule rule, const char* absent = nullptr) {
  VS::Actors::ActorRecord* actor = Record(actor_id);
  if (actor == nullptr) {
    if (absent != nullptr) {
      VS::Core::Debugf("actors", "%s skips absent actor=%d", absent, actor_id);
    }
    return;
  }
  WithRecord(*actor, rule);
}

// The record-pointer entries the originals dereference without a check.
template <typename Rule>
void WithPointer(VS::Actors::ActorRecord* actor, const char* name, Rule rule) {
  if (actor == nullptr) {
    VS::Core::Warnf("actors", "%s refused: no actor record", name);
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
  LiveActorRoute(int actor_id, VS::Actors::ActorRecord& actor)
      : id_(actor_id), actor_(actor) {}

  ActorRoute Route() override {
    return {.phase = actor_.route_phase,
            .uses_duration = actor_.route_uses_duration != 0,
            .target = LiveMotionVector(actor_.route_target),
            .move_rate = actor_.route_move_rate,
            .turn_rate = actor_.route_turn_rate};
  }
  void SetRoutePhase(int phase) override {
    actor_.route_phase = static_cast<u_char>(phase);
  }
  int TurnFrames() override { return actor_.instance.turn_frames; }
  int MoveFrames() override { return actor_.instance.current_animation; }
  bool MotionLocked() override { return actor_.instance.motion_locked != 0; }
  int TravelAnimation() override { return actor_.travel_animation; }
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
  VS::Actors::ActorRecord& actor_;
};
}  // namespace

MotionVector LiveMotionVector(const SVECTOR& live) {
  return {.x = live.vx, .y = live.vy, .z = live.vz, .w = live.pad};
}

SVECTOR LiveSvector(const MotionVector& motion) {
  return SVECTOR{
      .vx = motion.x, .vy = motion.y, .vz = motion.z, .pad = motion.w};
}

ActorMotion LoadLiveActorMotion(const VS::Actors::ActorRecord& live) {
  const VS::Actors::ModelInstance& instance = live.instance;
  ActorMotion motion;
  motion.action_animation_state = instance.action_animation_state;
  motion.movement_pending = instance.movement_pending != 0;
  motion.movement_mode = instance.movement_mode;
  motion.locomotion_state = instance.locomotion_state;
  motion.motion_locked = instance.motion_locked != 0;
  motion.is_jumping = instance.is_jumping != 0;
  motion.pose_state = instance.pose_state;
  motion.slot = instance.slot;
  motion.jump_countdown = instance.jump_countdown;
  motion.turn_frames = instance.turn_frames;
  motion.move_frames = instance.current_animation;
  motion.position = LiveMotionVector(instance.position);
  motion.rotation_x = instance.rotation_x;
  motion.facing = instance.facing;
  motion.rotation_z = instance.rotation_z;
  motion.scale = {
      .x = instance.scale_x, .y = instance.scale_y, .z = instance.scale_z};
  motion.root_motion = LiveMotionVector(instance.root_motion);
  motion.rotation_step_x = instance.rotation_step_x;
  motion.facing_delta = instance.facing_delta;
  motion.rotation_step_z = instance.rotation_step_z;
  motion.scale_delta = LiveMotionVector(instance.scale_delta);
  motion.current_tile = {.x = instance.current_tile_x,
                         .layer = instance.current_tile_layer,
                         .z = instance.current_tile_z};
  motion.route_phase = live.route_phase;
  motion.route_uses_duration = live.route_uses_duration != 0;
  motion.action_pose_active = live.action_pose_active != 0;
  motion.is_running = live.is_running != 0;
  motion.weapon_pose_active = live.weapon_pose_active != 0;
  motion.jump_profile = live.jump_profile;
  motion.run_speed = live.run_speed;
  motion.walk_speed = live.walk_speed;
  motion.turn_rate = live.turn_rate;
  motion.move_rate = live.move_rate;
  motion.route_move_rate = live.route_move_rate;
  motion.route_turn_rate = live.route_turn_rate;
  motion.scale_frames = live.scale_frames;
  motion.transition_frames = live.transition_frames;
  motion.route_target = LiveMotionVector(live.route_target);
  motion.destination_x = live.destination_x;
  motion.destination_z = live.destination_z;
  motion.destination_mode = live.destination_mode;
  motion.momentum_distance = live.momentum_distance;
  motion.run_frames = live.run_frames;
  motion.request_mode = live.turn_state.mode;
  motion.request_direction = live.turn_state.direction;
  motion.request_facing = live.turn_state.facing;
  motion.request_distance = live.turn_state.distance;
  return motion;
}

void StoreLiveActorMotion(const ActorMotion& motion,
                          VS::Actors::ActorRecord& live) {
  VS::Actors::ModelInstance& instance = live.instance;
  instance.action_animation_state = motion.action_animation_state;
  instance.movement_pending = motion.movement_pending ? 1 : 0;
  instance.movement_mode = motion.movement_mode;
  instance.locomotion_state = motion.locomotion_state;
  instance.motion_locked = motion.motion_locked ? 1 : 0;
  instance.is_jumping = motion.is_jumping ? 1 : 0;
  instance.pose_state = motion.pose_state;
  instance.slot = motion.slot;
  instance.jump_countdown = motion.jump_countdown;
  instance.turn_frames = motion.turn_frames;
  instance.current_animation = motion.move_frames;
  instance.position = LiveSvector(motion.position);
  instance.rotation_x = motion.rotation_x;
  instance.facing = motion.facing;
  instance.rotation_z = motion.rotation_z;
  instance.scale_x = motion.scale.x;
  instance.scale_y = motion.scale.y;
  instance.scale_z = motion.scale.z;
  instance.root_motion = LiveSvector(motion.root_motion);
  instance.rotation_step_x = motion.rotation_step_x;
  instance.facing_delta = motion.facing_delta;
  instance.rotation_step_z = motion.rotation_step_z;
  instance.scale_delta = LiveSvector(motion.scale_delta);
  instance.current_tile_x = motion.current_tile.x;
  instance.current_tile_layer = motion.current_tile.layer;
  instance.current_tile_z = motion.current_tile.z;
  live.route_phase = motion.route_phase;
  live.route_uses_duration = motion.route_uses_duration ? 1 : 0;
  live.action_pose_active = motion.action_pose_active ? 1 : 0;
  live.is_running = motion.is_running ? 1 : 0;
  live.weapon_pose_active = motion.weapon_pose_active ? 1 : 0;
  live.jump_profile = motion.jump_profile;
  live.run_speed = motion.run_speed;
  live.walk_speed = motion.walk_speed;
  live.turn_rate = motion.turn_rate;
  live.move_rate = motion.move_rate;
  live.route_move_rate = motion.route_move_rate;
  live.route_turn_rate = motion.route_turn_rate;
  live.scale_frames = motion.scale_frames;
  live.transition_frames = motion.transition_frames;
  live.route_target = LiveSvector(motion.route_target);
  live.destination_x = motion.destination_x;
  live.destination_z = motion.destination_z;
  live.destination_mode = motion.destination_mode;
  live.momentum_distance = motion.momentum_distance;
  live.run_frames = motion.run_frames;
  live.turn_state.mode = motion.request_mode;
  live.turn_state.direction = motion.request_direction;
  live.turn_state.facing = motion.request_facing;
  live.turn_state.distance = motion.request_distance;
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
  VS::Actors::ActorRecord* actor = Record(actor_id);
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

int LiveActorFacingDeltaToPoint(VS::Actors::ActorRecord* actor,
                                SVECTOR* target_point) {
  if (target_point == nullptr) return 0;
  if (actor == nullptr) {
    VS::Core::Warnf("actors",
                    "ActorFacingDeltaToPoint refused: no actor record");
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
  const VS::Actors::ActorRecord* actor = Record(actor_id);
  if (actor == nullptr) return;
  const TravelStepCounts counts =
      ActorTravelStepCounts(LoadLiveActorMotion(*actor));
  Required(move_steps_per_tile_out, "Actor step counts require outputs") =
      counts.move_steps_per_tile;
  Required(turn_steps_per_revolution_out, "Actor step counts require outputs") =
      counts.turn_steps_per_revolution;
}

void StepLiveActorTransitionDisplacement(VS::Actors::ActorRecord* actor) {
  WithPointer(
      actor, "StepActorTransitionDisplacement",
      [](ActorMotion& motion) { StepActorTransitionDisplacement(motion); });
}

void SetLiveActorFallDestination(VS::Actors::ActorRecord* actor,
                                 VS::Actors::FallDirection* fall_direction) {
  const VS::Actors::FallDirection& direction =
      Required(fall_direction, "Fall destination requires a direction");
  WithPointer(actor, "SetActorFallDestination", [&](ActorMotion& motion) {
    SetActorFallDestination(motion, direction.direction_x,
                            direction.direction_z);
  });
}

void ClearLiveActorMovementRequest(VS::Actors::ActorRecord* actor) {
  WithPointer(actor, "ClearActorMovementRequest",
              [](ActorMotion& motion) { ClearActorMovementRequest(motion); });
}

int LiveActorJumpAnimationProfile(VS::Actors::ActorRecord* actor) {
  const VS::Actors::ActorRecord& record =
      Required(actor, "Jump animation profile requires an actor");
  return ActorJumpAnimationProfile(LoadLiveActorMotion(record));
}

void MoveLiveActorToTileAtRate(int actor_id,
                               VS::Actors::TileTarget* tile_target,
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
                                    VS::Actors::TileTarget* tile_target,
                                    int move_steps_per_tile) {
  const PackedTile tile = TileArgument(tile_target);
  LiveMotionTerrain terrain;
  WithActor(actor_id, [&](ActorMotion& motion) {
    TranslateActorToTileAtRate(terrain, motion, tile, move_steps_per_tile);
  });
}

void TurnLiveActorTowardTileAtRate(int actor_id,
                                   VS::Actors::TileTarget* tile_target,
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

void MoveLiveActorToTileOverTime(int actor_id,
                                 VS::Actors::TileTarget* tile_target,
                                 int move_steps, int turn_steps) {
  const PackedTile tile = TileArgument(tile_target);
  LiveMotionTerrain terrain;
  WithActor(actor_id, [&](ActorMotion& motion) {
    MoveActorToTileOverTime(terrain, motion, tile, move_steps, turn_steps);
  });
}

void TranslateLiveActorToTileOverTime(int actor_id,
                                      VS::Actors::TileTarget* tile_target,
                                      int move_steps) {
  const PackedTile tile = TileArgument(tile_target);
  LiveMotionTerrain terrain;
  WithActor(actor_id, [&](ActorMotion& motion) {
    TranslateActorToTileOverTime(terrain, motion, tile, move_steps);
  });
}

void SetLiveActorTileTranslation(int actor_id,
                                 VS::Actors::TileTarget* tile_target,
                                 int move_steps, int move_steps_per_tile) {
  const PackedTile tile = TileArgument(tile_target);
  LiveMotionTerrain terrain;
  WithActor(actor_id, [&](ActorMotion& motion) {
    SetActorTileTranslation(terrain, motion, tile, move_steps,
                            move_steps_per_tile);
  });
}

void TurnLiveActorTowardTileOverTime(int actor_id,
                                     VS::Actors::TileTarget* tile_target,
                                     int turn_steps) {
  const PackedTile tile = TileArgument(tile_target);
  WithActor(
      actor_id,
      [&](ActorMotion& motion) {
        TurnActorTowardTileOverTime(motion, tile, turn_steps);
      },
      "TurnActorTowardTileOverTime");
}

void SetLiveActorTileTurn(int actor_id, VS::Actors::TileTarget* tile_target,
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

void UpdateLiveActorRoute(int actor_id, VS::Actors::ActorRecord* actor) {
  if (actor == nullptr) {
    VS::Core::Warnf("actors", "UpdateActorRoute refused: no actor record");
    return;
  }
  LiveActorRoute host(actor_id, *actor);
  UpdateActorRoute(host);
}
}  // namespace VS::Actors
