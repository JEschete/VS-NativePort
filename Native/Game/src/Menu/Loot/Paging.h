#pragma once

#include "Items/Inventory.h"
#include "Items/Loot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Temporary row metadata, not additional player inventory slots. */
enum { kMenuLootRowCapacity = sizeof(Items_InventoryIndices) };

/* Native paging (LootRowsLive.cpp): stages the drops the last batch left
 * behind; 0 when none were deferred or none remain. */
int Menu_Loot_AdvanceStagedInventory(void);
/* Forgets the pending batch before its loot records are released. */
void Menu_Loot_ResetPaging(void);

#ifdef __cplusplus
}
#endif
