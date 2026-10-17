// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_WEAPONS_H_TYPES
#define VS_ITEMS_WEAPONS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Items_WeaponRange Items_WeaponRange;

// decomp: vs_battle_range_t (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:9)
typedef struct Items_WeaponRange {
    char range;
    char secondRange;
    char thirdRange;
    char attackShape : 3;
    char attackShapeAngle : 5;
} Items_WeaponRange;
_Static_assert(sizeof(Items_WeaponRange) == 0x4, "Items_WeaponRange must keep its PS1 layout");

#endif // VS_ITEMS_WEAPONS_H_TYPES
