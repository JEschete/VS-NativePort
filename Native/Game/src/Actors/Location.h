// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Actors_ActorLocation;
struct Actors_ActorLocationQuery;
struct Actors_PlacementBuffer;
struct Actors_TileTarget;

// decomp: func_8006EBF8_t_fields (src/BATTLE/BATTLE.PRG/func_8006EBF8_t.h:9)
struct Actors_TileTarget {
    u_int tileX : 8;
    u_int tileLayer : 8;
    u_int tileZ : 8;
    u_int facing : 8;
};
static_assert(sizeof(Actors_TileTarget) == 0x4, "Actors_TileTarget must keep its PS1 layout");

// decomp: func_8006EBF8_t3 (src/BATTLE/BATTLE.PRG/func_8006EBF8_t.h:17)
struct Actors_ActorLocation {
    union {
        Actors_TileTarget target;
        int word;
    } packedTile;
    SVECTOR position;
};

// decomp: func_8006EBF8_t (src/BATTLE/BATTLE.PRG/func_8006EBF8_t.h:22)
struct Actors_ActorLocationQuery {
    Actors_ActorLocation location;
    int actorIndex;
};

// decomp: func_800A1108_t (src/BATTLE/BATTLE.PRG/146C.c:340)
struct Actors_PlacementBuffer {
    int reserved0;
    int reserved4;
    int reserved8;
    int reservedC;
    int reserved10;
    int reserved14;
    int reserved18;
    int reserved1C;
    int reserved20;
};
static_assert(sizeof(Actors_PlacementBuffer) == 0x24, "Actors_PlacementBuffer must keep its PS1 layout");
