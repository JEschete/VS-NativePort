// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_LOCATION_H_TYPES
#define VS_ACTORS_LOCATION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ActorLocation Actors_ActorLocation;
typedef struct Actors_ActorLocationQuery Actors_ActorLocationQuery;
typedef struct Actors_PlacementBuffer Actors_PlacementBuffer;
typedef struct Actors_TileTarget Actors_TileTarget;

// decomp: func_8006EBF8_t_fields (src/BATTLE/BATTLE.PRG/func_8006EBF8_t.h:9)
typedef struct Actors_TileTarget {
    u_int tileX : 8;
    u_int tileLayer : 8;
    u_int tileZ : 8;
    u_int facing : 8;
} Actors_TileTarget;
_Static_assert(sizeof(Actors_TileTarget) == 0x4, "Actors_TileTarget must keep its PS1 layout");

// decomp: func_8006EBF8_t3 (src/BATTLE/BATTLE.PRG/func_8006EBF8_t.h:17)
typedef struct Actors_ActorLocation {
    union {
        Actors_TileTarget target;
        int word;
    } packedTile;
    SVECTOR position;
} Actors_ActorLocation;

// decomp: func_8006EBF8_t (src/BATTLE/BATTLE.PRG/func_8006EBF8_t.h:22)
typedef struct Actors_ActorLocationQuery {
    Actors_ActorLocation location;
    int actorIndex;
} Actors_ActorLocationQuery;

// decomp: func_800A1108_t (src/BATTLE/BATTLE.PRG/146C.c:340)
typedef struct Actors_PlacementBuffer {
    int reserved0;
    int reserved4;
    int reserved8;
    int reservedC;
    int reserved10;
    int reserved14;
    int reserved18;
    int reserved1C;
    int reserved20;
} Actors_PlacementBuffer;
_Static_assert(sizeof(Actors_PlacementBuffer) == 0x24, "Actors_PlacementBuffer must keep its PS1 layout");

#endif // VS_ACTORS_LOCATION_H_TYPES
