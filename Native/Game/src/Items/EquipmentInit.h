// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_EQUIPMENTINIT_H_TYPES
#define VS_ITEMS_EQUIPMENTINIT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Items_AccessoryInit Items_AccessoryInit;
typedef struct Items_ArmorInit Items_ArmorInit;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ITEMS_EQUIPMENTINIT_H_SET_TYPES_ONLY
#endif
#include "Items/Equipment.h"
#ifdef VS_ITEMS_EQUIPMENTINIT_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ITEMS_EQUIPMENTINIT_H_SET_TYPES_ONLY
#endif

// decomp: _accessoryIntermediate (src/BATTLE/BATTLE.PRG/146C.h:839)
typedef struct Items_AccessoryInit {
    Items_EquipmentPartStats equipment;
    char dropRate;
    char index;
    char reserved32;
    char reserved33;
} Items_AccessoryInit;

// decomp: _armorIntermediate (src/BATTLE/BATTLE.PRG/146C.h:831)
typedef struct Items_ArmorInit {
    Items_EquipmentPartStats equipment;
    char material;
    char dropRate;
    char opaque32;
    char index;
} Items_ArmorInit;

#endif // VS_ITEMS_EQUIPMENTINIT_H_TYPES
