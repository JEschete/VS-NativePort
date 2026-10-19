#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_ACTORS_MOVEMENTCOMMANDS_H
#define VS_OWNED_ACTORS_MOVEMENTCOMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

void Actors_SetDirectMovement(int actor_id, int direction, int speed);
void Actors_QueueNavigationTileTarget(int actor_id, int direction, int distance,
                                      int facing);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_ACTORS_MOVEMENTCOMMANDS_H
