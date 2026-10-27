// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Abilities.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Rewards.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Core/Random.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomEffects.h"
#include "Field/Zone.h"
#include "Items/Equipment.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Loot/Paging.h"
#include "Render/Palette.h"
#include "State/Flags.h"

// decomp: _lootListHead (_lootListHead@BATTLE/BATTLE.PRG)
Items_LootListNode* Items_lootListHead;

// decomp: BATTLE.PRG _dropWeaponRand (src/BATTLE/BATTLE.PRG/146C.c:1608)
int Items_RollEnemyWeaponDrop(Items_LootedWeapon* looted, Items_WeaponLoadout* base)
{
    if (Core_Random_NextBounded(0xFF) < base->dropRate) {
        int i;

        Items_WriteInventoryBladeStats(&looted->blade, &base->blade);
        Items_WriteInventoryGripStats(&looted->grip, &base->grip);

        for (i = 0; i < 3; ++i) {
            if (base->gems[i].id != 0) {
                Items_WriteInventoryGemStats(&looted->gems[i], &base->gems[i]);
            }
        }

        Core_Memory_CopyAlignedWords(looted->name, base->name, sizeof looted->name);

        looted->difficultyFlags = 3;

        return 1;
    }

    return 0;
}

// decomp: BATTLE.PRG _dropShieldRand (src/BATTLE/BATTLE.PRG/146C.c:1632)
int Items_RollEnemyShieldDrop(Items_LootedShield* looted, Items_ShieldLoadout* base)
{
    if (Core_Random_NextBounded(0xFF) < base->dropRate) {
        int i;

        Items_WriteInventoryArmorStats(&looted->shield, &base->base);

        for (i = 0; i < 3; ++i) {
            if (base->gems[i].id != 0) {
                Items_WriteInventoryGemStats(&looted->gems[i], &base->gems[i]);
            }
        }

        looted->difficultyFlags = 3;
        return 1;
    }

    return 0;
}

// decomp: BATTLE.PRG _dropAccessoryRand (src/BATTLE/BATTLE.PRG/146C.c:1652)
int Items_RollEnemyAccessoryDrop(Items_LootedAccessory* looted, Items_AccessorySpawnLoadout* base)
{
    if (Core_Random_NextBounded(0xFF) < base->dropRate) {

        Items_WriteInventoryArmorStats(&looted->accessory, &base->accessory);

        looted->difficultyFlags = 3;
        return 1;
    }

    return 0;
}

// decomp: BATTLE.PRG _dropArmorRand (src/BATTLE/BATTLE.PRG/146C.c:1665)
int Items_RollEnemyArmorDrop(Items_LootedArmor* looted, Items_ArmorLoadout* base)
{
    if (Core_Random_NextBounded(0xFF) < base->dropRate) {
        int i;

        for (i = 0; i < 2; ++i) {
            if (looted[i].armor.id == 0) {

                Items_WriteInventoryArmorStats(&looted[i].armor, &base->armor);

                looted[i].difficultyFlags = 3;
                return 1;
            }
        }
    }

    return 0;
}

// decomp: BATTLE.PRG _dropMisc (src/BATTLE/BATTLE.PRG/146C.c:1684)
int Items_CopyGuaranteedEnemyItemDrops(Items_BattleLoot* looted, Items_MiscItemStack* base)
{
    int ret;
    int i;

    ret = 0;

    for (i = 0; i < 2; ++i) {

        looted->misc[i].itemId = base[i].id;

        if (looted->misc[i].itemId) {
            looted->misc[i].count = base[i].count;
            looted->misc[i].difficultyFlags = 3;
            ret = 1;
        }
    }

    return ret;
}

// decomp: BATTLE.PRG _dropMiscRand (src/BATTLE/BATTLE.PRG/146C.c:1705)
int Items_RollEnemyItemDrop(Items_BattleLoot* looted, Items_MiscItemDrop* base)
{
    looted->misc[2].itemId = base->id;

    if (looted->misc[2].itemId) {
        if (Core_Random_NextBounded(0xFF) < base->dropRate) {
            looted->misc[2].count = 1;
            looted->misc[2].difficultyFlags = 3;
            return 1;
        }
    }

    return 0;
}

// decomp: BATTLE.PRG _dropLoot (src/BATTLE/BATTLE.PRG/146C.c:1720)
void Items_QueueEnemyLoot(Battle_Actor* enemy)
{
    int i;
    Battle_ActorStats* enemy_stats = enemy->stats;
    Items_LootListNode* lootList = Core_Memory_AllocateFromHighEnd(sizeof *lootList);

    if (lootList == NULL) {
        return;
    }

    Core_Memory_ZeroAlignedWords(lootList, sizeof *lootList);

    lootList->droppedMask |= Items_RollEnemyWeaponDrop(&lootList->loot.weapon, &enemy_stats->weapon);
    lootList->droppedMask |= Items_RollEnemyShieldDrop(&lootList->loot.shield, &enemy_stats->shield);
    lootList->droppedMask |=
        Items_RollEnemyAccessoryDrop(&lootList->loot.accessory, &enemy_stats->accessory);

    for (i = 0; i < 6; ++i) {
        if (enemy_stats->limbs[i].armor.armor.id != 0) {
            lootList->droppedMask |=
                Items_RollEnemyArmorDrop(lootList->loot.armor, &enemy_stats->limbs[i].armor);
        }
    }

    lootList->droppedMask |= Items_CopyGuaranteedEnemyItemDrops(&lootList->loot, &enemy_stats->dropItem);
    lootList->droppedMask |= Items_RollEnemyItemDrop(&lootList->loot, &enemy_stats->randomDropItem);

    if (lootList->droppedMask != 0) {

        Items_LootListNode* node = Items_lootListHead;
        int insertLoot = 1;

        while (node != NULL) {
            if (node->droppedMask == enemy->id) {
                insertLoot = 0;
            }
            node = node->next;
        }

        if (insertLoot != 0) {
            int actorId = enemy->id;
            Items_LootListNode* next = Items_lootListHead;
            Items_lootListHead = lootList;
            lootList->droppedMask = actorId;
            lootList->next = next;
            return;
        }
    }

    Core_Memory_FreeFromHighEnd(lootList);
}

// decomp: BATTLE.PRG func_8006FB48 (src/BATTLE/BATTLE.PRG/146C.c:3372)
void Items_OpenPendingEnemyLoot(void)
{
    if (State_ScriptFlag(0xAB) > 2) {
        if (Items_lootListHead != NULL) {
            Menu_Framework_OpenLootListMenu(Items_lootListHead);
            Field_controlMode = 2;
            Battle_actionFlowMode = 12;
            Field_PauseFieldForMenu();
        } else {
            Battle_BeginPendingAbilityReward();
        }
        return;
    }
    Battle_WaitForPostActionMovement();
}

// decomp: BATTLE.PRG func_8007BA98 (src/BATTLE/BATTLE.PRG/146C.c:8470)
void Items_PresentScriptedLoot(int id0, int count0, int id1, int count1)
{
    Items_LootListNode* previous_loot_head;
    Items_LootListNode* scripted_loot;

    scripted_loot = Core_Memory_AllocateFromHighEnd(sizeof *scripted_loot);
    if (scripted_loot != NULL) {
        Core_Memory_ZeroAlignedWords(scripted_loot, sizeof *scripted_loot);
        scripted_loot->loot.misc[0].itemId = id0;
        if (id0 & 0xFFFF) {
            scripted_loot->loot.misc[0].count = count0;
            scripted_loot->loot.misc[0].difficultyFlags = 3;
            scripted_loot->droppedMask |= 1;
        }
        scripted_loot->loot.misc[1].itemId = id1;
        if (id1 & 0xFFFF) {
            scripted_loot->loot.misc[1].count = count1;
            scripted_loot->loot.misc[1].difficultyFlags = 3;
            scripted_loot->droppedMask |= 1;
        }
        if (scripted_loot->droppedMask != 0) {
            previous_loot_head = Items_lootListHead;
            scripted_loot->droppedMask = 0xFF;
            Items_lootListHead = scripted_loot;
            scripted_loot->next = previous_loot_head;
        } else {
            Core_Memory_FreeFromHighEnd(scripted_loot);
        }
    }
    if (Items_lootListHead != NULL) {
        Battle_lootMenuOpened = 1;
        Menu_Framework_OpenLootListMenu(Items_lootListHead);
        Battle_actionFlowMode = 4;
        Battle_BeginRewardPause();
        return;
    }
    Battle_ResumeAfterRewards();
}

// decomp: BATTLE.PRG func_8008A6FC (src/BATTLE/BATTLE.PRG/146C.c:15596)
void Items_ClearRoomLootList(void)
{
    Items_LootListNode* loot_node = Items_lootListHead;

    Menu_Loot_ResetPaging();

    while (loot_node != 0) {
        Items_LootListNode* next_loot_node = loot_node->next;
        Core_Memory_FreeFromHighEnd(loot_node);
        loot_node = next_loot_node;
    }
    Items_lootListHead = NULL;
}

// decomp: BATTLE.PRG func_8008D508 (src/BATTLE/BATTLE.PRG/146C.c:16832)
Items_TreasureChest* Items_TreasureChestAtTile(int tile_x, int tile_z, int tile_layer)
{
    int chest_count;
    Items_TreasureChest* chest;
    int i;

    chest = Field_roomData.containers;
    do {
        if (chest != NULL) {
            chest_count = Field_roomData.header.containersLength
                    / sizeof *Field_roomData.containers;
            for (i = 0; i < chest_count; ++i, ++chest) {
                if ((chest->tileX == tile_x) && (chest->tileZ == tile_z)) {
                    if (chest->layer == tile_layer) {
                        return chest;
                    }
                }
            }
        }
    } while (0);
    return NULL;
}

// decomp: BATTLE.PRG func_8008D710 (src/BATTLE/BATTLE.PRG/146C.c:16903)
void Items_RestoreRoomChestStates(void)
{
    Field_MpdTileCell* collision_cell;
    Items_TreasureChest* chest;
    int chest_count;
    int i;

    if (Field_roomData.containers != NULL) {

        chest = Field_roomData.containers;
        chest_count =
            Field_roomData.header.containersLength / sizeof *Field_roomData.containers;

        for (i = 0; i < chest_count; ++i, ++chest) {
            collision_cell = Field_CollisionCellAt(
                (u_int)chest->tileX, (u_int)chest->tileZ, (int)chest->layer);
            if (State_ScriptFlag((short)(chest->openedFlag + 0x3C0))) {
                chest->isOpen = 1;
                Field_FinishRoomTiltRotation(chest->lidRotationId);
                if (collision_cell != NULL) {
                    collision_cell->doorPassability = 5;
                }
            } else {
                chest->isOpen = 0;
                if (collision_cell != NULL) {
                    collision_cell->doorPassability = 0;
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_8008E370 (src/BATTLE/BATTLE.PRG/146C.c:17285)
Items_TreasureChest* Items_RoomTreasureChests(int* chest_count_out)
{
    if (Field_roomData.containers != NULL) {
        *chest_count_out =
            Field_roomData.header.containersLength / sizeof *Field_roomData.containers;
        return Field_roomData.containers;
    }
    *chest_count_out = 0;
    return NULL;
}

// decomp: BATTLE.PRG _loadMpdLootSection (src/BATTLE/BATTLE.PRG/146C.c:17389)
void Items_LoadRoomChestLoot(void* data)
{
    if (Field_roomData.containers != NULL) {
        Core_Memory_CopyAlignedWords(&Field_roomData.containers->loot, data,
            sizeof Field_roomData.containers->loot);
    }
}

