// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_LOOT_H_TYPES
#define VS_ITEMS_LOOT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Items_BattleLoot Items_BattleLoot;
typedef struct Items_LootListNode Items_LootListNode;
typedef struct Items_LootedAccessory Items_LootedAccessory;
typedef struct Items_LootedArmor Items_LootedArmor;
typedef struct Items_LootedBlade Items_LootedBlade;
typedef struct Items_LootedGem Items_LootedGem;
typedef struct Items_LootedGrip Items_LootedGrip;
typedef struct Items_LootedItem Items_LootedItem;
typedef struct Items_LootedShield Items_LootedShield;
typedef struct Items_LootedWeapon Items_LootedWeapon;

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
typedef struct Items_LootedAccessory {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryArmor accessory;
} Items_LootedAccessory;
_Static_assert(sizeof(Items_LootedAccessory) == 0x2C, "Items_LootedAccessory must keep its PS1 layout");

// decomp: vs_battle_lootedArmor (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:210)
typedef struct Items_LootedArmor {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryArmor armor;
} Items_LootedArmor;
_Static_assert(sizeof(Items_LootedArmor) == 0x2C, "Items_LootedArmor must keep its PS1 layout");

// decomp: vs_battle_lootedBlade (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:185)
typedef struct Items_LootedBlade {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryBlade blade;
} Items_LootedBlade;
_Static_assert(sizeof(Items_LootedBlade) == 0x30, "Items_LootedBlade must keep its PS1 layout");

// decomp: vs_battle_lootedGem (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:226)
typedef struct Items_LootedGem {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryGem gem;
} Items_LootedGem;
_Static_assert(sizeof(Items_LootedGem) == 0x20, "Items_LootedGem must keep its PS1 layout");

// decomp: vs_battle_lootedGrip (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:193)
typedef struct Items_LootedGrip {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryGrip grip;
} Items_LootedGrip;
_Static_assert(sizeof(Items_LootedGrip) == 0x14, "Items_LootedGrip must keep its PS1 layout");

// decomp: vs_battle_lootedMisc (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:232)
typedef struct Items_LootedItem {
    u_short itemId;
    char difficultyFlags;
    char count;
} Items_LootedItem;
_Static_assert(sizeof(Items_LootedItem) == 0x4, "Items_LootedItem must keep its PS1 layout");

// decomp: vs_battle_lootedShield (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:202)
typedef struct Items_LootedShield {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryArmor shield;
    Items_InventoryGem gems[3];
} Items_LootedShield;
_Static_assert(sizeof(Items_LootedShield) == 0x80, "Items_LootedShield must keep its PS1 layout");

// decomp: vs_battle_lootedWeapon (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:177)
typedef struct Items_LootedWeapon {
    char difficultyFlags;
    char reserved1;
    char reserved2;
    char reserved3;
    Items_InventoryBlade blade;
    Items_InventoryGrip grip;
    Items_InventoryGem gems[3];
    char name[24];
} Items_LootedWeapon;
_Static_assert(sizeof(Items_LootedWeapon) == 0xAC, "Items_LootedWeapon must keep its PS1 layout");

// decomp: vs_battle_loot (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:243)
typedef struct Items_BattleLoot {
    Items_LootedWeapon weapon;
    Items_LootedBlade blade;
    Items_LootedGrip grip;
    Items_LootedShield shield; // 0xF0
    Items_LootedArmor armor[2]; // 0x170
    Items_LootedAccessory accessory; // 0x1C8
    Items_LootedGem gem; // 0x1F4
    Items_LootedItem misc[3]; // 0x214
} Items_BattleLoot;
_Static_assert(sizeof(Items_BattleLoot) == 0x220, "Items_BattleLoot must keep its PS1 layout");

// decomp: vs_battle_lootListNode (src/BATTLE/BATTLE.PRG/146C.h:797)
typedef struct Items_LootListNode {
    struct Items_LootListNode* next;
    int droppedMask;
    Items_BattleLoot loot;
} Items_LootListNode;

#endif // VS_ITEMS_LOOT_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ITEMS_LOOT_H_DECLS)
#define VS_ITEMS_LOOT_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/TreasureChests.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

extern Items_LootListNode* Items_lootListHead;
int Items_RollEnemyWeaponDrop(Items_LootedWeapon* looted, Items_WeaponLoadout* base);
int Items_RollEnemyShieldDrop(Items_LootedShield* looted, Items_ShieldLoadout* base);
int Items_RollEnemyAccessoryDrop(Items_LootedAccessory* looted, Items_AccessorySpawnLoadout* base);
int Items_RollEnemyArmorDrop(Items_LootedArmor* looted, Items_ArmorLoadout* base);
int Items_CopyGuaranteedEnemyItemDrops(Items_BattleLoot* looted, Items_MiscItemStack* base);
int Items_RollEnemyItemDrop(Items_BattleLoot* looted, Items_MiscItemDrop* base);
void Items_QueueEnemyLoot(Battle_Actor* enemy);
void Items_OpenPendingEnemyLoot(void);
void Items_PresentScriptedLoot(int id0, int count0, int id1, int count1);
void Items_ClearRoomLootList(void);
Items_TreasureChest* Items_TreasureChestAtTile(int arg0, int arg1, int arg2);
void Items_RestoreRoomChestStates(void);
Items_TreasureChest* Items_RoomTreasureChests(int* arg0);
void Items_LoadRoomChestLoot(void* data);

#endif // VS_ITEMS_LOOT_H_DECLS
