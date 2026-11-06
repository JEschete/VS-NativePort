#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Actors/ActorMotion.hpp"

struct Actors_ActorRecord;
struct Actors_FallDirection;
struct Actors_TileTarget;
struct SVECTOR;

// Binding from the actor table and the original arguments to the typed actor
// motion rules (ActorMotion.hpp). Each adapter keeps the original argument
// order for the generated callers. An actor id outside the actor table is
// refused with a warning; an id without a record does nothing, as the
// originals that check for one do, and the originals that dereference a
// missing record now refuse it with a warning.
namespace VS::Actors {
// A legacy SVECTOR as the typed MotionVector (pad becomes w), and back.
[[nodiscard]] MotionVector LiveMotionVector(const SVECTOR& live);
[[nodiscard]] SVECTOR LiveSvector(const MotionVector& motion);

// The record's motion fields as the typed ActorMotion, and their store.
[[nodiscard]] ActorMotion LoadLiveActorMotion(const Actors_ActorRecord& live);
void StoreLiveActorMotion(const ActorMotion& motion, Actors_ActorRecord& live);

void BeginLiveScriptedActorPath(int actor_id, SVECTOR* target_position);
int IsLiveScriptedActorPathFinished(int actor_id);
void ResetLiveActorMovement(int actor_id);
void StopLiveActorMotion(int actor_id);
void ClearLiveActorMovementModes(int actor_id);
int LiveActorFacingDeltaToPoint(Actors_ActorRecord* actor,
                                SVECTOR* target_point);
void ResetLiveActorRunMomentum(int actor_id);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveActorWalkRunSpeeds(int actor_id, int walk_speed_fixed,
                               int run_speed_fixed);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveActorTravelStepCounts(int actor_id, int move_steps_per_tile,
                                  int turn_steps_per_revolution);
// Leaves both outputs unwritten for a missing actor.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveActorTravelStepCounts(int actor_id, int* move_steps_per_tile_out,
                               int* turn_steps_per_revolution_out);
void StepLiveActorTransitionDisplacement(Actors_ActorRecord* actor);
void SetLiveActorFallDestination(Actors_ActorRecord* actor,
                                 Actors_FallDirection* fall_direction);
void ClearLiveActorMovementRequest(Actors_ActorRecord* actor);
int LiveActorJumpAnimationProfile(Actors_ActorRecord* actor);

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void MoveLiveActorToTileAtRate(int actor_id, Actors_TileTarget* tile_target,
                               int move_steps_per_tile,
                               int turn_steps_per_revolution);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void MoveLiveActorToPositionAtRate(int actor_id, SVECTOR* target,
                                   int move_steps_per_tile,
                                   int turn_steps_per_revolution);
void TranslateLiveActorToTileAtRate(int actor_id,
                                    Actors_TileTarget* tile_target,
                                    int move_steps_per_tile);
void TurnLiveActorTowardTileAtRate(int actor_id, Actors_TileTarget* tile_target,
                                   int turn_steps_per_revolution);
void TranslateLiveActorByAtRate(int actor_id, SVECTOR* translation,
                                int move_steps_per_tile);
void TranslateLiveActorToAtRate(int actor_id, SVECTOR* target_position,
                                int move_steps_per_tile);
void TurnLiveActorByAtRate(int actor_id, std::int16_t yaw_delta,
                           int turn_steps_per_revolution);
void TurnLiveActorToAtRate(int actor_id, std::int16_t target_yaw,
                           int turn_steps_per_revolution);
void RotateLiveActorByAtRate(int actor_id, SVECTOR* rotation_delta,
                             int turn_steps_per_revolution);
void RotateLiveActorToAtRate(int actor_id, SVECTOR* target_rotation,
                             int turn_steps_per_revolution);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void MoveLiveActorToTileOverTime(int actor_id, Actors_TileTarget* tile_target,
                                 int move_steps, int turn_steps);
void TranslateLiveActorToTileOverTime(int actor_id,
                                      Actors_TileTarget* tile_target,
                                      int move_steps);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveActorTileTranslation(int actor_id, Actors_TileTarget* tile_target,
                                 int move_steps, int move_steps_per_tile);
void TurnLiveActorTowardTileOverTime(int actor_id,
                                     Actors_TileTarget* tile_target,
                                     int turn_steps);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveActorTileTurn(int actor_id, Actors_TileTarget* tile_target,
                          int turn_steps, int turn_steps_per_revolution);
void SetLiveActorMotion(int actor_id, SVECTOR* translation, int move_steps);
void TranslateLiveActorToOverTime(int actor_id, SVECTOR* target_position,
                                  int move_steps);
void SetLiveActorTranslation(int actor_id, SVECTOR* translation,
                             int move_steps);
void SetLiveActorYawMotion(int actor_id, std::int16_t yaw_delta,
                           int turn_steps);
void TurnLiveActorToOverTime(int actor_id, std::uint16_t target_yaw,
                             int turn_steps);
void TurnLiveActorToAtAngularSpeed(int actor_id, std::int16_t target_yaw,
                                   int angular_speed);
void SetLiveActorYawTurn(int actor_id, std::int16_t yaw_delta, int turn_steps);
void SetLiveActorRotationMotion(int actor_id, SVECTOR* rotation_delta,
                                int turn_steps);
void SetLiveActorRotation(int actor_id, SVECTOR* rotation_delta,
                          int turn_steps);
void ScaleLiveActorToOverTime(int actor_id, SVECTOR* target_scale,
                              int scale_steps);
// Steps the route of `actor`, which the generated callers pass alongside its
// id; the moves and animations run for the id.
void UpdateLiveActorRoute(int actor_id, Actors_ActorRecord* actor);
}  // namespace VS::Actors
