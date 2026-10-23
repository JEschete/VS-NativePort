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
    Battle_ActorStats* temp_s2 = enemy->stats;
    Items_LootListNode* lootList = Core_Memory_AllocateFromHighEnd(sizeof *lootList);

    if (lootList == NULL) {
        return;
    }

    Core_Memory_ZeroAlignedWords(lootList, sizeof *lootList);

    lootList->droppedMask |= Items_RollEnemyWeaponDrop(&lootList->loot.weapon, &temp_s2->weapon);
    lootList->droppedMask |= Items_RollEnemyShieldDrop(&lootList->loot.shield, &temp_s2->shield);
    lootList->droppedMask |=
        Items_RollEnemyAccessoryDrop(&lootList->loot.accessory, &temp_s2->accessory);

    for (i = 0; i < 6; ++i) {
        if (temp_s2->limbs[i].armor.armor.id != 0) {
            lootList->droppedMask |=
                Items_RollEnemyArmorDrop(lootList->loot.armor, &temp_s2->limbs[i].armor);
        }
    }

    lootList->droppedMask |= Items_CopyGuaranteedEnemyItemDrops(&lootList->loot, &temp_s2->dropItem);
    lootList->droppedMask |= Items_RollEnemyItemDrop(&lootList->loot, &temp_s2->randomDropItem);

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
    Items_LootListNode* temp_a0;
    Items_LootListNode* temp_v0;

    temp_v0 = Core_Memory_AllocateFromHighEnd(sizeof *temp_v0);
    if (temp_v0 != NULL) {
        Core_Memory_ZeroAlignedWords(temp_v0, sizeof *temp_v0);
        temp_v0->loot.misc[0].itemId = id0;
        if (id0 & 0xFFFF) {
            temp_v0->loot.misc[0].count = count0;
            temp_v0->loot.misc[0].difficultyFlags = 3;
            temp_v0->droppedMask |= 1;
        }
        temp_v0->loot.misc[1].itemId = id1;
        if (id1 & 0xFFFF) {
            temp_v0->loot.misc[1].count = count1;
            temp_v0->loot.misc[1].difficultyFlags = 3;
            temp_v0->droppedMask |= 1;
        }
        if (temp_v0->droppedMask != 0) {
            temp_a0 = Items_lootListHead;
            temp_v0->droppedMask = 0xFF;
            Items_lootListHead = temp_v0;
            temp_v0->next = temp_a0;
        } else {
            Core_Memory_FreeFromHighEnd(temp_v0);
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
    Items_LootListNode* var_a0 = Items_lootListHead;

    Menu_Loot_ResetPaging();

    while (var_a0 != 0) {
        Items_LootListNode* temp_s0 = var_a0->next;
        Core_Memory_FreeFromHighEnd(var_a0);
        var_a0 = temp_s0;
    }
    Items_lootListHead = NULL;
}

// decomp: BATTLE.PRG func_8008D508 (src/BATTLE/BATTLE.PRG/146C.c:16832)
Items_TreasureChest* Items_TreasureChestAtTile(int arg0, int arg1, int arg2)
{
    int temp_t1;
    Items_TreasureChest* var_a3;
    int i;

    var_a3 = Field_roomData.containers;
    do {
        if (var_a3 != NULL) {
            temp_t1 = Field_roomData.header.containersLength
                    / sizeof *Field_roomData.containers;
            for (i = 0; i < temp_t1; ++i, ++var_a3) {
                if ((var_a3->tileX == arg0) && (var_a3->tileZ == arg1)) {
                    if (var_a3->layer == arg2) {
                        return var_a3;
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
    Field_MpdTileCell* temp_s0;
    Items_TreasureChest* var_s3;
    int temp_s4;
    int i;

    if (Field_roomData.containers != NULL) {

        var_s3 = Field_roomData.containers;
        temp_s4 =
            Field_roomData.header.containersLength / sizeof *Field_roomData.containers;

        for (i = 0; i < temp_s4; ++i, ++var_s3) {
            temp_s0 = Field_CollisionCellAt(
                (u_int)var_s3->tileX, (u_int)var_s3->tileZ, (int)var_s3->layer);
            if (State_ScriptFlag((short)(var_s3->openedFlag + 0x3C0))) {
                var_s3->isOpen = 1;
                Field_FinishRoomTiltRotation(var_s3->lidRotationId);
                if (temp_s0 != NULL) {
                    temp_s0->doorPassability = 5;
                }
            } else {
                var_s3->isOpen = 0;
                if (temp_s0 != NULL) {
                    temp_s0->doorPassability = 0;
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_8008E370 (src/BATTLE/BATTLE.PRG/146C.c:17285)
Items_TreasureChest* Items_RoomTreasureChests(int* arg0)
{
    if (Field_roomData.containers != NULL) {
        *arg0 =
            Field_roomData.header.containersLength / sizeof *Field_roomData.containers;
        return Field_roomData.containers;
    }
    *arg0 = 0;
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

