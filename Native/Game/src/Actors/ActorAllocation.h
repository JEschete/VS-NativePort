// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ACTORALLOCATION_H_TYPES
#define VS_ACTORS_ACTORALLOCATION_H_TYPES
#include "Legacy/Psx.h"

struct Actors_ActorAllocation;
struct Actors_ActorCore;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ACTORS_ACTORALLOCATION_H_SET_TYPES_ONLY
#endif
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Battle/Actor.h"
#ifdef VS_ACTORS_ACTORALLOCATION_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ACTORS_ACTORALLOCATION_H_SET_TYPES_ONLY
#endif

// decomp: vs_battle_actor_dat2 (src/BATTLE/BATTLE.PRG/146C.h:615)
struct Actors_ActorCore {
    struct {
        Battle_Actor actor;
        Battle_ActorStats stats;
    } actor;
    Actors_ModelInstance model;
    MATRIX reserved6C[8];
    int reserved16C;
    int reserved170;
    int reserved174;
};

// decomp: vs_battle_actor_dat (src/BATTLE/BATTLE.PRG/146C.h:621)
struct Actors_ActorAllocation {
    Actors_ActorCore core;
    char reservedB2C[0x1788 + 80]; /* x64: the actor record is larger */
    Actors_ActorModelSlot modelSlots[2];
};

#endif // VS_ACTORS_ACTORALLOCATION_H_TYPES
