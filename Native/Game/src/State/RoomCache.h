// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct State_RoomEnemyStates;

// decomp: D_80061078_t (src/SLUS_010.40/main.h:412)
struct State_RoomEnemyStates {
    char recency;
    char zoneId;
    u_short mapId;
    u_int nextSlot;
    char actorStates[8][0x28];
};
static_assert(sizeof(State_RoomEnemyStates) == 0x148, "State_RoomEnemyStates must keep its PS1 layout");
