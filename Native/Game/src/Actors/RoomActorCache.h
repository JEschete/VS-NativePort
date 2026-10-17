// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ROOMACTORCACHE_H_TYPES
#define VS_ACTORS_ROOMACTORCACHE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_RoomActorRecord Actors_RoomActorRecord;
typedef struct Actors_RoomActorRecordWrite Actors_RoomActorRecordWrite;

// decomp: func_80069EF0_t (src/BATTLE/BATTLE.PRG/146C.c:154)
typedef struct Actors_RoomActorRecordWrite {
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
} Actors_RoomActorRecordWrite;

// decomp: savedEnemyState (src/BATTLE/BATTLE.PRG/146C.c:376)
typedef struct Actors_RoomActorRecord {
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
} Actors_RoomActorRecord;

#endif // VS_ACTORS_ROOMACTORCACHE_H_TYPES
