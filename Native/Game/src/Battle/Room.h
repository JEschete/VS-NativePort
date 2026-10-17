// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ROOM_H_TYPES
#define VS_BATTLE_ROOM_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_DoorRecord Battle_DoorRecord;
typedef struct Battle_PrimitiveBytes Battle_PrimitiveBytes;

// decomp: vs_battle_door (src/BATTLE/BATTLE.PRG/146C.h:628)
typedef struct Battle_DoorRecord {
    char reserved0;
    char reserved1;
    char reserved2;
    char reserved3;
} Battle_DoorRecord;

// decomp: vs_battle_prim (src/BATTLE/BATTLE.PRG/146C.h:635)
typedef struct Battle_PrimitiveBytes {
    char reserved0;
    char reserved1;
    char reserved2;
    char reserved3;
} Battle_PrimitiveBytes;

#endif // VS_BATTLE_ROOM_H_TYPES
