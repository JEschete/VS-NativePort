#pragma once

#include "Items/Inventory.h"
#include "Items/Loot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Temporary row metadata, not additional player inventory slots. */
enum { kMenuLootRowCapacity = sizeof(Items_InventoryIndices) };

int Menu_Loot_HasStagingSpace(int category, int quantity);
int Menu_Loot_CanStageDrop(char* difficulty_flags);
void Menu_Loot_AppendStagedDrop(char* difficulty_flags, int category,
                                int index);
void Menu_Loot_BeginPaging(Items_LootListNode* nodes);
int Menu_Loot_AdvanceStagedInventory(void);
void Menu_Loot_ResetPaging(void);

#ifdef __cplusplus
}
#endif
