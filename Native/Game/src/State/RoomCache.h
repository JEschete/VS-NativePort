// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_ROOMCACHE_H_TYPES
#define VS_STATE_ROOMCACHE_H_TYPES
#include "Legacy/Psx.h"

typedef struct State_RoomEnemyStates State_RoomEnemyStates;

// decomp: D_80061078_t (src/SLUS_010.40/main.h:412)
typedef struct State_RoomEnemyStates {
    char recency;
    char zoneId;
    u_short mapId;
    u_int nextSlot;
    char actorStates[8][0x28];
} State_RoomEnemyStates;
_Static_assert(sizeof(State_RoomEnemyStates) == 0x148, "State_RoomEnemyStates must keep its PS1 layout");

#endif // VS_STATE_ROOMCACHE_H_TYPES
