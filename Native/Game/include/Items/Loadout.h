// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_LOADOUT_H_TYPES
#define VS_ITEMS_LOADOUT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Items_AccessorySpawnLoadout Items_AccessorySpawnLoadout;
typedef struct Items_ArmorLoadout Items_ArmorLoadout;
typedef struct Items_ClassAffinityBaseline Items_ClassAffinityBaseline;
typedef struct Items_ClassAffinityCurrent Items_ClassAffinityCurrent;
typedef struct Items_MiscItemDrop Items_MiscItemDrop;
typedef struct Items_ShieldLoadout Items_ShieldLoadout;
typedef struct Items_ShieldSpawnLoadout Items_ShieldSpawnLoadout;
typedef struct Items_WeaponLoadout Items_WeaponLoadout;
typedef struct Items_WeaponReservedBlock Items_WeaponReservedBlock;
typedef struct Items_WeaponSpawnLoadout Items_WeaponSpawnLoadout;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ITEMS_LOADOUT_H_SET_TYPES_ONLY
#endif
#include "Items/Equipment.h"
#include "Items/Weapons.h"
#ifdef VS_ITEMS_LOADOUT_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ITEMS_LOADOUT_H_SET_TYPES_ONLY
#endif

// decomp: vs_battle_classAffinityBaseline (src/BATTLE/BATTLE.PRG/146C.h:282)
typedef struct Items_ClassAffinityBaseline {
    /* 0x00 */ short classes[8];
    /* 0x10 */ u_short affinities[8];
    /* 0x20 */
} Items_ClassAffinityBaseline;

// decomp: vs_battle_classAffinityCurrent (src/BATTLE/BATTLE.PRG/146C.h:276)
typedef struct Items_ClassAffinityCurrent {
    /* 0x00 */ short classes[2][8];
    /* 0x20 */ short affinities[2][8];
    /* 0x40 */
} Items_ClassAffinityCurrent;

// decomp: vs_battle_shieldIntermediate (src/BATTLE/BATTLE.PRG/146C.h:780)
typedef struct Items_ShieldSpawnLoadout {
    Items_EquipmentPartStats base;
    Items_EquipmentPartStats gems[3];
    char material;
    char dropRate;
    short index;
} Items_ShieldSpawnLoadout;

// decomp: vs_battle_uiAccessory (src/BATTLE/BATTLE.PRG/146C.h:424)
typedef struct Items_AccessorySpawnLoadout {
    /* 0x00 */ Items_EquipmentPartStats accessory;
    /* 0x30 */ short currentStr;
    /* 0x32 */ short currentInt;
    /* 0x34 */ short currentAgility;
    /* 0x36 */ char dropRate;
    /* 0x37 */ u_char index;
    /* 0x38 */ u_short types[4];
    /* 0x40 */ u_short classes[8];
    /* 0x50 */ u_short affinities[8];
    /* 0x60 */
} Items_AccessorySpawnLoadout;

// decomp: vs_battle_uiArmor (src/BATTLE/BATTLE.PRG/146C.h:398)
typedef struct Items_ArmorLoadout {
    /* 0x00 */ Items_EquipmentPartStats armor;
    /* 0x30 */ u_short currentDp;
    /* 0x32 */ u_short maxDp;
    /* 0x34 */ short currentStr;
    /* 0x36 */ u_short baseStr;
    /* 0x38 */ short currentInt;
    /* 0x3A */ u_short baseInt;
    /* 0x3C */ short currentAgility;
    /* 0x3E */ u_short baseAgility;
    /* 0x40 */ u_short types[4];
    /* 0x48 */ Items_ClassAffinityCurrent classAffinityCurrent;
    /* 0x88 */ short affinityModifiers[8];
    /* 0x98 */ char dropRate;
    /* 0x99 */ char opaque99;
    /* 0x9A */ u_char index;
    /* 0x9B */ u_char reserved9B;
    /* 0x9C */ Items_ClassAffinityBaseline classAffinityBaseline;
    /* 0xBC */
} Items_ArmorLoadout;

// decomp: vs_battle_uiMiscRand (src/BATTLE/BATTLE.PRG/146C.h:436)
typedef struct Items_MiscItemDrop {
    u_short id;
    char dropRate;
    char reserved03;
} Items_MiscItemDrop;

// decomp: vs_battle_uiShield (src/BATTLE/BATTLE.PRG/146C.h:377)
typedef struct Items_ShieldLoadout {
    /* 0x000 */ int reserved0[6];
    /* 0x018 */ Items_EquipmentPartStats base;
    /* 0x048 */ Items_EquipmentPartStats gems[3];
    /* 0x0D8 */ char wepId;
    /* 0x0D9 */ char dropRate;
    /* 0x0DA */ u_char index;
    /* 0x0DB */ char reservedDB;
    /* 0x0DC */ u_short currentPp;
    /* 0x0DE */ u_short maxPp;
    /* 0x0E0 */ u_short currentDp;
    /* 0x0E2 */ u_short maxDp;
    /* 0x0E4 */ short currentStr;
    /* 0x0E6 */ u_short baseStr;
    /* 0x0E8 */ short currentInt;
    /* 0x0EA */ u_short baseInt;
    /* 0x0EC */ short currentAgility;
    /* 0x0EE */ u_short baseAgility;
    /* 0x0F0 */ char types[4];
    /* 0x0F4 */ Items_ClassAffinityCurrent classAffinityCurrent;
    /* 0x134 */ short affinityModifiers[8];
    /* 0x144 */ Items_ClassAffinityBaseline classAffinityBaseline;
    /* 0x164 */
} Items_ShieldLoadout;

// decomp: vs_battle_uiWeapon (src/BATTLE/BATTLE.PRG/146C.h:352)
typedef struct Items_WeaponLoadout {
    /* 0x000 */ char name[24];
    /* 0x018 */ Items_EquipmentPartStats blade;
    /* 0x048 */ Items_EquipmentPartStats grip;
    /* 0x078 */ Items_EquipmentPartStats gems[3];
    /* 0x108 */ char wepId;
    /* 0x109 */ char damageTypeValue;
    /* 0x10A */ char risk;
    /* 0x10B */ char recoveryDelay;
    /* 0x10C */ char dropRate;
    /* 0x10D */ u_char index;
    /* 0x10E */ u_char damageType;
    /* 0x10F */ u_char actionType;
    /* 0x110 */ u_short currentPp;
    /* 0x112 */ u_short maxPp;
    /* 0x114 */ u_short currentDp;
    /* 0x116 */ u_short maxDp;
    /* 0x118 */ short currentStr;
    /* 0x11A */ u_short baseStr;
    /* 0x11C */ short currentInt;
    /* 0x11E */ u_short baseInt;
    /* 0x120 */ short currentAgility;
    /* 0x122 */ u_short baseAgility;
    /* 0x124 */ Items_WeaponRange range;
    /* 0x128 */ Items_ClassAffinityCurrent classAffinityCurrent;
    /* 0x168 */ short affinityModifiers[8];
    /* 0x178 */ Items_ClassAffinityBaseline classAffinityBaseline;
    /* 0x198 */
} Items_WeaponLoadout;

// decomp: vs_battle_uiWeapon_unk (src/BATTLE/BATTLE.PRG/146C.h:286)
typedef struct Items_WeaponReservedBlock {
    u_short reserved0[0x18];
} Items_WeaponReservedBlock;

// decomp: vs_battle_weaponIntermediate (src/BATTLE/BATTLE.PRG/146C.h:772)
typedef struct Items_WeaponSpawnLoadout {
    Items_EquipmentPartStats blade;
    Items_EquipmentPartStats grip;
    Items_EquipmentPartStats gems[3];
    char material;
    char dropRate;
    short index;
    char name[24]; /* vs_string */
} Items_WeaponSpawnLoadout;

#endif // VS_ITEMS_LOADOUT_H_TYPES
