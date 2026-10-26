#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_ACTORS_MOTIONREQUESTS_H
#define VS_OWNED_ACTORS_MOTIONREQUESTS_H
#include <stdint.h>

#include "Legacy/Psx.h"

typedef struct Actors_ActorRecord Actors_ActorRecord;

#ifdef __cplusplus
extern "C" {
#endif

void Actors_BeginTileJump(int actor_id, int* target, int speed, int facing);
void Actors_BeginAirborneTileJump(int actor_id, int* target, int speed,
                                  int facing);
void Actors_BeginNavigationJump(int actor_id, int* target, int speed,
                                int facing);
void Actors_BeginTeleport(int actor_id, int* target, int facing);
void Actors_BeginTileTravel(int actor_id, int* target, int facing);
void Actors_BeginTimedTravel(int actor_id, int* target, int facing);
void Actors_SaveActorGroundedTransform(Actors_ActorRecord* actor);
void Actors_TurnNavigationActor(int actor_id, int facing);
void Actors_TurnActorAtSpeed(int actor_id, int16_t facing, int speed);
void Actors_QueueGroundOffset(int actor_id, SVECTOR* step);
void Actors_QueueMovementVector(int actor_id, SVECTOR* step, int facing);
void Actors_QueueFlightMovement(int actor_id, SVECTOR* step, int speed,
                                int facing);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_ACTORS_MOTIONREQUESTS_H
