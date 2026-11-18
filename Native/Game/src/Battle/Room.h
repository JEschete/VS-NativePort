// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ROOM_H_TYPES
#define VS_BATTLE_ROOM_H_TYPES
#include "Legacy/Psx.h"

struct Battle_DoorRecord;
struct Battle_PrimitiveBytes;

// decomp: vs_battle_door (src/BATTLE/BATTLE.PRG/146C.h:628)
struct Battle_DoorRecord {
    char reserved0;
    char reserved1;
    char reserved2;
    char reserved3;
};
static_assert(sizeof(Battle_DoorRecord) == 0x4, "Battle_DoorRecord must keep its PS1 layout");

// decomp: vs_battle_prim (src/BATTLE/BATTLE.PRG/146C.h:635)
struct Battle_PrimitiveBytes {
    char reserved0;
    char reserved1;
    char reserved2;
    char reserved3;
};
static_assert(sizeof(Battle_PrimitiveBytes) == 0x4, "Battle_PrimitiveBytes must keep its PS1 layout");

#endif // VS_BATTLE_ROOM_H_TYPES
