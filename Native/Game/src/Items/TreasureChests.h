// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Items_TreasureChest;

#include "Items/Loot.h"

// decomp: _mpdRoomSection13 (src/BATTLE/BATTLE.PRG/146C.h:207)
struct Items_TreasureChest {
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
};
static_assert(sizeof(Items_TreasureChest) == 0x234, "Items_TreasureChest must keep its PS1 layout");
