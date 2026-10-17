// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_EQUIPMENT_H_TYPES
#define VS_ITEMS_EQUIPMENT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Items_EquipmentPartStats Items_EquipmentPartStats;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ITEMS_EQUIPMENT_H_SET_TYPES_ONLY
#endif
#include "Items/Weapons.h"
#ifdef VS_ITEMS_EQUIPMENT_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ITEMS_EQUIPMENT_H_SET_TYPES_ONLY
#endif

// decomp: vs_gemTargets (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:166)
enum Items_GemTarget { Items_GemTarget_Shield = 0x80 };
typedef enum Items_GemTarget Items_GemTarget;

// decomp: vs_battle_uiEquipment (src/BATTLE/BATTLE.PRG/146C.h:322)
typedef struct Items_EquipmentPartStats {
    /* 0x00 */ u_short id;
    /* 0x02 */ signed char subId;
    /* 0x03 */ signed char weaponModelId;
    /* 0x04 */ char category;
    /* 0x05 */ signed char strength;
    /* 0x06 */ signed char intelligence;
    /* 0x07 */ signed char agility;
    /* 0x08 */ u_short currentDp;
    /* 0x0A */ u_short maxDp;
    /* 0x0C */ u_short currentPp;
    /* 0x0E */ u_short maxPp;
    /* 0x10 */ char damageType;
    /* 0x11 */ char costType;
    /* 0x12 */ char cost;
    /* 0x13 */ char material;
    /* 0x14 */ char opaque14;
    /* 0x15 */ char gemSlots;
    /* 0x16 */ char gemEffects;
    /* 0x17 */ char index;
    /* 0x18 */ Items_WeaponRange range;
    /* 0x1C */ signed char types[4];
    /* 0x20 */ signed char classes[8];
    /* 0x28 */ signed char affinities[8];
    /* 0x30 */
} Items_EquipmentPartStats;

#endif // VS_ITEMS_EQUIPMENT_H_TYPES
