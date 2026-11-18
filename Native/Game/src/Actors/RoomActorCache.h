// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Actors_RoomActorRecord;
struct Actors_RoomActorRecordWrite;

// decomp: func_80069EF0_t (src/BATTLE/BATTLE.PRG/146C.c:154)
struct Actors_RoomActorRecordWrite {
    char spawnSlot;
    char isHidden;
    short aiFlags;
    short currentHp;
    short maxHp;
    short currentMp;
    short maxMp;
    short strength;
    short intelligence;
    short agility;
    short limbHp[6];
    int statuses;
    int placement;
};
static_assert(sizeof(Actors_RoomActorRecordWrite) == 0x28, "Actors_RoomActorRecordWrite must keep its PS1 layout");

// decomp: savedEnemyState (src/BATTLE/BATTLE.PRG/146C.c:376)
struct Actors_RoomActorRecord {
    u_char spawnSlot;
    u_char isHidden;
    short aiFlags;
    short currentHp;
    short maxHp;
    short currentMp;
    short maxMp;
    short strength;
    short intelligence;
    short agility;
    short limbHp[6];
    short reserved1E;
    int statuses;
    u_char tileX;
    u_char direction;
    u_char tileZ;
    u_char height;
};
static_assert(sizeof(Actors_RoomActorRecord) == 0x28, "Actors_RoomActorRecord must keep its PS1 layout");
