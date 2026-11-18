// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Items_Container;
struct Items_ContainerData;
struct Items_ContainerIndices;
struct Items_Inventory;
struct Items_InventoryArmor;
struct Items_InventoryBlade;
struct Items_InventoryGem;
struct Items_InventoryGrip;
struct Items_InventoryIndices;
struct Items_InventoryMisc;
struct Items_InventoryShield;
struct Items_InventoryWeapon;
struct Items_MiscItemStack;

#include "Items/Weapons.h"

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

// decomp: vs_battle_uiMisc (src/BATTLE/BATTLE.PRG/146C.h:430)
struct Items_MiscItemStack {
    u_short id;
    char count;
    char reserved03;
};
static_assert(sizeof(Items_MiscItemStack) == 0x4, "Items_MiscItemStack must keep its PS1 layout");

// decomp: vs_main_inventoryArmor (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:92)
struct Items_InventoryArmor {
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
};
static_assert(sizeof(Items_InventoryArmor) == 0x28, "Items_InventoryArmor must keep its PS1 layout");

// decomp: vs_main_inventoryBlade (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:35)
struct Items_InventoryBlade {
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
};
static_assert(sizeof(Items_InventoryBlade) == 0x2C, "Items_InventoryBlade must keep its PS1 layout");

// decomp: vs_main_inventoryGem (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:62)
struct Items_InventoryGem {
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
};
static_assert(sizeof(Items_InventoryGem) == 0x1C, "Items_InventoryGem must keep its PS1 layout");

// decomp: vs_main_inventoryGrip (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:48)
struct Items_InventoryGrip {
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
};
static_assert(sizeof(Items_InventoryGrip) == 0x10, "Items_InventoryGrip must keep its PS1 layout");

// decomp: vs_main_inventoryIndices_t (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:142)
struct Items_InventoryIndices {
    u_char weapons[8];
    u_char blades[16];
    u_char grips[16];
    u_char shields[8];
    u_char armor[16];
    u_char gems[48];
    u_char misc[64];
};
static_assert(sizeof(Items_InventoryIndices) == 0xB0, "Items_InventoryIndices must keep its PS1 layout");

// decomp: vs_main_inventoryMisc (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:106)
struct Items_InventoryMisc {
    u_short id;
    char count;
    char index;
};
static_assert(sizeof(Items_InventoryMisc) == 0x4, "Items_InventoryMisc must keep its PS1 layout");

// decomp: vs_main_inventoryShield (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:100)
struct Items_InventoryShield {
    char index;
    char isEquipped;
    short reserved2;
    Items_InventoryArmor base;
    u_char gems[4]; // 0x2C
};
static_assert(sizeof(Items_InventoryShield) == 0x30, "Items_InventoryShield must keep its PS1 layout");

// decomp: vs_main_inventoryWeapon (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:71)
struct Items_InventoryWeapon {
    char index;
    u_char blade;
    u_char grip;
    char isEquipped;
    u_char gems[4];
    char name[24];
};
static_assert(sizeof(Items_InventoryWeapon) == 0x20, "Items_InventoryWeapon must keep its PS1 layout");

// decomp: vs_main_inventory_t (src/SLUS_010.40/main.h:404)
struct Items_Inventory {
    Items_InventoryWeapon weapons[8];
    Items_InventoryShield shields[8]; // 0x100
    Items_InventoryBlade blades[16]; // 0x280
    Items_InventoryGrip grips[16]; // 0x540
    Items_InventoryArmor armor[16]; // 0x640
    Items_InventoryGem gems[48]; // 0x8C0
    Items_InventoryMisc misc[64]; // 0xE00
};
static_assert(sizeof(Items_Inventory) == 0xF00, "Items_Inventory must keep its PS1 layout");

// decomp: vs_menu_containerData (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:117)
struct Items_ContainerData {
    Items_InventoryWeapon weapons[32];
    Items_InventoryShield shields[32];
    Items_InventoryBlade blades[64];
    Items_InventoryGrip grips[64];
    Items_InventoryArmor armor[64];
    Items_InventoryGem gems[192];
    Items_InventoryMisc misc[256];
    // 0x3C00
};
static_assert(sizeof(Items_ContainerData) == 0x3C00, "Items_ContainerData must keep its PS1 layout");

// decomp: vs_menu_containerIndices (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:127)
struct Items_ContainerIndices {
    u_short weapons[32];
    u_short blades[64];
    u_short grips[64];
    u_short shields[32];
    u_short armor[64];
    u_short gems[192];
    u_short misc[256];
};
static_assert(sizeof(Items_ContainerIndices) == 0x580, "Items_ContainerIndices must keep its PS1 layout");

// decomp: vs_menu_container (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:132)
struct Items_Container {
    Items_ContainerData items;
    Items_ContainerIndices indices;
};
static_assert(sizeof(Items_Container) == 0x4180, "Items_Container must keep its PS1 layout");
