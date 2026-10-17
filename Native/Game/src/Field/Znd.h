// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ZND_H_TYPES
#define VS_FIELD_ZND_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_ZndEnemyRecord Field_ZndEnemyRecord;
typedef struct Field_ZndHeader Field_ZndHeader;

// decomp: _zndEnemy (src/BATTLE/BATTLE.PRG/146C.h:659)
typedef struct Field_ZndEnemyRecord {
    u_char recordBytes[0x464];
} Field_ZndEnemyRecord;
_Static_assert(sizeof(Field_ZndEnemyRecord) == 0x464, "Field_ZndEnemyRecord must keep its PS1 layout");

// decomp: _zndHeader (src/BATTLE/BATTLE.PRG/146C.c:290)
typedef struct Field_ZndHeader {
    long mpdListOffset;
    int mpdListSize;
    long enemiesOffset;
    int enemiesSize;
    long texturesOffset;
    int texturesSize;
    int musicId;
} Field_ZndHeader;
_Static_assert(sizeof(Field_ZndHeader) == 0x1C, "Field_ZndHeader must keep its PS1 layout");

#endif // VS_FIELD_ZND_H_TYPES
