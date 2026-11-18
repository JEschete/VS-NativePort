// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Actors_ActorAllocation;
struct Actors_ActorCore;

#include "Actors/ActorModelSlot.hpp"
#include "Actors/ModelInstance.h"
#include "Battle/Actor.h"

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
    VS::Actors::ActorModelSlot modelSlots[2];
};
