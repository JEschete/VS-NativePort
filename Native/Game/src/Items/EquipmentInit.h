// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Items_AccessoryInit;
struct Items_ArmorInit;

#include "Items/Equipment.h"

// decomp: _accessoryIntermediate (src/BATTLE/BATTLE.PRG/146C.h:839)
struct Items_AccessoryInit {
    Items_EquipmentPartStats equipment;
    char dropRate;
    char index;
    char reserved32;
    char reserved33;
};
static_assert(sizeof(Items_AccessoryInit) == 0x34, "Items_AccessoryInit must keep its PS1 layout");

// decomp: _armorIntermediate (src/BATTLE/BATTLE.PRG/146C.h:831)
struct Items_ArmorInit {
    Items_EquipmentPartStats equipment;
    char material;
    char dropRate;
    char opaque32;
    char index;
};
static_assert(sizeof(Items_ArmorInit) == 0x34, "Items_ArmorInit must keep its PS1 layout");
