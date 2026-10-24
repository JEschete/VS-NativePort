#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_ACTORS_MOTIONREQUESTS_H
#define VS_OWNED_ACTORS_MOTIONREQUESTS_H
#include <stdint.h>

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

void Actors_BeginTileJump(int actor_id, int* target, int speed, int facing);
void Actors_BeginAirborneTileJump(int actor_id, int* target, int speed,
                                  int facing);
void Actors_BeginNavigationJump(int actor_id, int* target, int speed,
                                int facing);
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
