#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Actors/MotionRequests.hpp"

struct Actors_ActorRecord;
struct SVECTOR;

// Binding from the movement scripts' actor table and request arguments to the
// typed motion requests. Each entry aborts with a diagnostic on an actor id
// outside the actor table or without a record, and on a missing target or
// step; the packed targets are copied before any actor state is written.
namespace VS::Actors {
void SaveLiveActorGroundedTransform(Actors_ActorRecord* actor);
void BeginLiveTileJump(int actor_id, int* target, int speed, int facing);
void BeginLiveAirborneTileJump(int actor_id, int* target, int speed,
                               int facing);
void BeginLiveNavigationJump(int actor_id, int* target, int speed, int facing);
void BeginLiveTeleport(int actor_id, int* target, int facing);
void BeginLiveTileTravel(int actor_id, int* target, int facing);
void BeginLiveTimedTravel(int actor_id, int* target, int facing);
void QueueLiveGroundOffset(int actor_id, SVECTOR* step);
void QueueLiveMovementVector(int actor_id, SVECTOR* step, int facing);
void QueueLiveFlightMovement(int actor_id, SVECTOR* step, int speed,
                             int facing);
// Typed entries for hand callers; they check only the actor.
void BeginLiveTileJump(int actor_id, PackedTile target, int speed, int facing);
void BeginLiveAirborneTileJump(int actor_id, PackedTile target, int speed,
                               int facing);
void BeginLiveNavigationJump(int actor_id, PackedTile target, int speed,
                             int facing);
void BeginLiveTeleport(int actor_id, PackedTile target, int facing);
void BeginLiveTileTravel(int actor_id, const MotionVector& route, int facing);
void BeginLiveTimedTravel(int actor_id, const MotionVector& route, int facing);
void QueueLiveGroundOffset(int actor_id, const MotionVector& step);
void QueueLiveMovementVector(int actor_id, const MotionVector& step,
                             int facing);
void QueueLiveFlightMovement(int actor_id, const MotionVector& step, int speed,
                             int facing);
void TurnLiveNavigationActor(int actor_id, int facing);
// Also aborts on a zero angular speed, which the turn consumer divides by.
void TurnLiveActorAtSpeed(int actor_id, std::int16_t facing, int speed);
}  // namespace VS::Actors
