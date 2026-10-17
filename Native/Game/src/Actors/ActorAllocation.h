// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ACTORALLOCATION_H_TYPES
#define VS_ACTORS_ACTORALLOCATION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ActorAllocation Actors_ActorAllocation;
typedef struct Actors_ActorCore Actors_ActorCore;
typedef struct Actors_ActorModelSlot Actors_ActorModelSlot;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ACTORS_ACTORALLOCATION_H_SET_TYPES_ONLY
#endif
#include "Actors/ModelInstance.h"
#include "Battle/Actor.h"
#ifdef VS_ACTORS_ACTORALLOCATION_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ACTORS_ACTORALLOCATION_H_SET_TYPES_ONLY
#endif

// decomp: vs_battle_actor4 (src/BATTLE/BATTLE.PRG/146C.h:440)
typedef struct Actors_ActorModelSlot {
    char storage[0x5E8];
} Actors_ActorModelSlot;
_Static_assert(sizeof(Actors_ActorModelSlot) == 0x5E8, "Actors_ActorModelSlot must keep its PS1 layout");

// decomp: vs_battle_actor_dat2 (src/BATTLE/BATTLE.PRG/146C.h:615)
typedef struct Actors_ActorCore {
    struct {
        Battle_Actor actor;
        Battle_ActorStats stats;
    } actor;
    Actors_ModelInstance model;
    MATRIX reserved6C[8];
    int reserved16C;
    int reserved170;
    int reserved174;
} Actors_ActorCore;

// decomp: vs_battle_actor_dat (src/BATTLE/BATTLE.PRG/146C.h:621)
typedef struct Actors_ActorAllocation {
    Actors_ActorCore core;
    char reservedB2C[0x1788 + 72]; /* x64: the actor record is larger */
    Actors_ActorModelSlot modelSlots[2];
} Actors_ActorAllocation;

#endif // VS_ACTORS_ACTORALLOCATION_H_TYPES
