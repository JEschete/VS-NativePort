// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_INVENTORY_H_TYPES
#define VS_ITEMS_INVENTORY_H_TYPES
#include "Legacy/Psx.h"

typedef struct Items_Container Items_Container;
typedef struct Items_ContainerData Items_ContainerData;
typedef struct Items_ContainerIndices Items_ContainerIndices;
typedef struct Items_Inventory Items_Inventory;
typedef struct Items_InventoryArmor Items_InventoryArmor;
typedef struct Items_InventoryBlade Items_InventoryBlade;
typedef struct Items_InventoryGem Items_InventoryGem;
typedef struct Items_InventoryGrip Items_InventoryGrip;
typedef struct Items_InventoryIndices Items_InventoryIndices;
typedef struct Items_InventoryMisc Items_InventoryMisc;
typedef struct Items_InventoryShield Items_InventoryShield;
typedef struct Items_InventoryWeapon Items_InventoryWeapon;
typedef struct Items_MiscItemStack Items_MiscItemStack;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ITEMS_INVENTORY_H_SET_TYPES_ONLY
#endif
#include "Items/Weapons.h"
#ifdef VS_ITEMS_INVENTORY_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ITEMS_INVENTORY_H_SET_TYPES_ONLY
#endif

// decomp: vs_itemCategories (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:155)
enum Items_ItemCategory {
    Items_ItemCategory_Weapon,
    Items_ItemCategory_Blade,
    Items_ItemCategory_Grip,
    Items_ItemCategory_Shield,
    Items_ItemCategory_Armor,
    Items_ItemCategory_Gem,
    Items_ItemCategory_Misc,
    Items_ItemCategory_Manage
};
typedef enum Items_ItemCategory Items_ItemCategory;

// decomp: vs_battle_uiMisc (src/BATTLE/BATTLE.PRG/146C.h:430)
typedef struct Items_MiscItemStack {
    u_short id;
    char count;
    char reserved03;
} Items_MiscItemStack;

// decomp: vs_main_inventoryArmor (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:92)
typedef struct Items_InventoryArmor {
    u_char id;
    u_char subId;
    char wepId;
    char category;
    u_short maxDp;
    u_short maxPp;
    u_short currentDp;
    u_short currentPp;
    char gemSlots;
    signed char strength;
    signed char intelligence;
    signed char agility;
    signed char types[4]; // 0x10
    signed char classes[8];
    signed char affinities[8];
    u_short material;
    u_char limb;
    u_char index;
} Items_InventoryArmor;

// decomp: vs_main_inventoryBlade (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:35)
typedef struct Items_InventoryBlade {
    u_char id;
    char subId;
    char wepId;
    char category;
    u_short maxDp;
    u_short maxPp;
    u_short currentDp;
    u_short currentPp;
    signed char strength;
    signed char intelligence;
    signed char agility;
    char cost;
    char damageType : 2;
    char costType : 3;
    char reserved10Bit5 : 3;
    char reserved11;
    u_short opaque12;
    Items_WeaponRange range;
    signed char classes[8];
    signed char affinities[8];
    u_short material;
    char assembledWeaponIndex;
    char index;
} Items_InventoryBlade;

// decomp: vs_main_inventoryGem (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:62)
typedef struct Items_InventoryGem {
    u_short id;
    char subId;
    char reserved3;
    char gemEffects;
    signed char strength;
    signed char intelligence;
    signed char agility;
    signed char classes[8];
    signed char affinities[8];
    u_short setItemIndex;
    short index;
} Items_InventoryGem;

// decomp: vs_main_inventoryGrip (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:48)
typedef struct Items_InventoryGrip {
    u_short id;
    char subId;
    u_char category;
    char gemSlots;
    signed char strength;
    signed char intelligence;
    signed char agility;
    signed char types[4];
    u_short assembledWeaponIndex;
    u_short index;
} Items_InventoryGrip;

// decomp: vs_main_inventoryIndices_t (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:142)
typedef struct Items_InventoryIndices {
    u_char weapons[8];
    u_char blades[16];
    u_char grips[16];
    u_char shields[8];
    u_char armor[16];
    u_char gems[48];
    u_char misc[64];
} Items_InventoryIndices;

// decomp: vs_main_inventoryMisc (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:106)
typedef struct Items_InventoryMisc {
    u_short id;
    char count;
    char index;
} Items_InventoryMisc;

// decomp: vs_main_inventoryShield (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:100)
typedef struct Items_InventoryShield {
    char index;
    char isEquipped;
    short reserved2;
    Items_InventoryArmor base;
    u_char gems[4]; // 0x2C
} Items_InventoryShield;

// decomp: vs_main_inventoryWeapon (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:71)
typedef struct Items_InventoryWeapon {
    char index;
    u_char blade;
    u_char grip;
    char isEquipped;
    u_char gems[4];
    char name[24];
} Items_InventoryWeapon;

// decomp: vs_main_inventory_t (src/SLUS_010.40/main.h:404)
typedef struct Items_Inventory {
    Items_InventoryWeapon weapons[8];
    Items_InventoryShield shields[8]; // 0x100
    Items_InventoryBlade blades[16]; // 0x280
    Items_InventoryGrip grips[16]; // 0x540
    Items_InventoryArmor armor[16]; // 0x640
    Items_InventoryGem gems[48]; // 0x8C0
    Items_InventoryMisc misc[64]; // 0xE00
} Items_Inventory;

// decomp: vs_menu_containerData (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:117)
typedef struct Items_ContainerData {
    Items_InventoryWeapon weapons[32];
    Items_InventoryShield shields[32];
    Items_InventoryBlade blades[64];
    Items_InventoryGrip grips[64];
    Items_InventoryArmor armor[64];
    Items_InventoryGem gems[192];
    Items_InventoryMisc misc[256];
    // 0x3C00
} Items_ContainerData;

// decomp: vs_menu_containerIndices (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:127)
typedef struct Items_ContainerIndices {
    u_short weapons[32];
    u_short blades[64];
    u_short grips[64];
    u_short shields[32];
    u_short armor[64];
    u_short gems[192];
    u_short misc[256];
} Items_ContainerIndices;

// decomp: vs_menu_container (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:132)
typedef struct Items_Container {
    Items_ContainerData items;
    Items_ContainerIndices indices;
} Items_Container;

#endif // VS_ITEMS_INVENTORY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ITEMS_INVENTORY_H_DECLS)
#define VS_ITEMS_INVENTORY_H_DECLS

extern u_char Items_miscItemInvoked;
extern u_char Items_Inventory_inventoryItemCapacities[];
int Items_InventoryItemCount(int id);
int Items_ConsumeMiscItem(int miscId);
void Items_Inventory_UnequipWeapon(void);
void Items_Inventory_UnequipShield(void);
void Items_Inventory_ClearItemSlot(int itemCategory, int index);
int Items_Inventory_ItemOccupancyId(int category, int index, Items_Inventory* inventory);
int Items_Inventory_FindDisplayPosition(int category, int id);
void Items_Inventory_AppendDisplayIndex(int itemCategory, int id);
void Items_Inventory_RebuildDisplayIndices(int itemCategory);
int Items_Inventory_CountItems(int itemCategory, Items_Inventory* inventory);
int Items_Inventory_FirstEmptySlot(int itemCategory, Items_Inventory* inventory);
int Items_Inventory_CopyItem(int itemCategory, Items_Inventory* targetInventory,
    int sourceItemIndex, Items_Inventory* sourceInventory);
void Items_Inventory_StoreEquippedStats(void);
void Items_Inventory_ResetMainMenuStatics(void);

#endif // VS_ITEMS_INVENTORY_H_DECLS
