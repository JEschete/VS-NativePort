// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_LOOT_H_TYPES
#define VS_ITEMS_LOOT_H_TYPES
#include "Legacy/Psx.h"

struct Items_BattleLoot;
struct Items_LootListNode;
struct Items_LootedAccessory;
struct Items_LootedArmor;
struct Items_LootedBlade;
struct Items_LootedGem;
struct Items_LootedGrip;
struct Items_LootedItem;
struct Items_LootedShield;
struct Items_LootedWeapon;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ITEMS_LOOT_H_SET_TYPES_ONLY
#endif
#include "Items/Inventory.h"
#ifdef VS_ITEMS_LOOT_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ITEMS_LOOT_H_SET_TYPES_ONLY
#endif

// decomp: vs_battle_lootedAccessory (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:218)
struct Items_LootedAccessory {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryArmor accessory;
};
static_assert(sizeof(Items_LootedAccessory) == 0x2C, "Items_LootedAccessory must keep its PS1 layout");

// decomp: vs_battle_lootedArmor (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:210)
struct Items_LootedArmor {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryArmor armor;
};
static_assert(sizeof(Items_LootedArmor) == 0x2C, "Items_LootedArmor must keep its PS1 layout");

// decomp: vs_battle_lootedBlade (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:185)
struct Items_LootedBlade {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryBlade blade;
};
static_assert(sizeof(Items_LootedBlade) == 0x30, "Items_LootedBlade must keep its PS1 layout");

// decomp: vs_battle_lootedGem (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:226)
struct Items_LootedGem {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryGem gem;
};
static_assert(sizeof(Items_LootedGem) == 0x20, "Items_LootedGem must keep its PS1 layout");

// decomp: vs_battle_lootedGrip (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:193)
struct Items_LootedGrip {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryGrip grip;
};
static_assert(sizeof(Items_LootedGrip) == 0x14, "Items_LootedGrip must keep its PS1 layout");

// decomp: vs_battle_lootedMisc (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:232)
struct Items_LootedItem {
    u_short itemId;
    char difficultyFlags;
    char count;
};
static_assert(sizeof(Items_LootedItem) == 0x4, "Items_LootedItem must keep its PS1 layout");

// decomp: vs_battle_lootedShield (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:202)
struct Items_LootedShield {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryArmor shield;
    Items_InventoryGem gems[3];
};
static_assert(sizeof(Items_LootedShield) == 0x80, "Items_LootedShield must keep its PS1 layout");

// decomp: vs_battle_lootedWeapon (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:177)
struct Items_LootedWeapon {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryBlade blade;
    Items_InventoryGrip grip;
    Items_InventoryGem gems[3];
    char name[24];
};
static_assert(sizeof(Items_LootedWeapon) == 0xAC, "Items_LootedWeapon must keep its PS1 layout");

// decomp: vs_battle_loot (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:243)
struct Items_BattleLoot {
    Items_LootedWeapon weapon;
    Items_LootedBlade blade;
    Items_LootedGrip grip;
    Items_LootedShield shield; // 0xF0
    Items_LootedArmor armor[2]; // 0x170
    Items_LootedAccessory accessory; // 0x1C8
    Items_LootedGem gem; // 0x1F4
    Items_LootedItem misc[3]; // 0x214
};
static_assert(sizeof(Items_BattleLoot) == 0x220, "Items_BattleLoot must keep its PS1 layout");

// decomp: vs_battle_lootListNode (src/BATTLE/BATTLE.PRG/146C.h:797)
struct Items_LootListNode {
    struct Items_LootListNode* next;
    int droppedMask;
    Items_BattleLoot loot;
};

#endif // VS_ITEMS_LOOT_H_TYPES
