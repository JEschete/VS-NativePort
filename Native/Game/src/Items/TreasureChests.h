// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_TREASURECHESTS_H_TYPES
#define VS_ITEMS_TREASURECHESTS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Items_TreasureChest Items_TreasureChest;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ITEMS_TREASURECHESTS_H_SET_TYPES_ONLY
#endif
#include "Items/Loot.h"
#ifdef VS_ITEMS_TREASURECHESTS_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ITEMS_TREASURECHESTS_H_SET_TYPES_ONLY
#endif

// decomp: _mpdRoomSection13 (src/BATTLE/BATTLE.PRG/146C.h:207)
typedef struct Items_TreasureChest {
    short tileX;
    short tileZ;
    short layer;
    short lidRotationId;
    short isOpen;
    short reservedA;
    short lockLevel;
    short reservedE;
    short openedFlag;
    short facing;
    Items_BattleLoot loot;
} Items_TreasureChest;
_Static_assert(sizeof(Items_TreasureChest) == 0x234, "Items_TreasureChest must keep its PS1 layout");

#endif // VS_ITEMS_TREASURECHESTS_H_TYPES
