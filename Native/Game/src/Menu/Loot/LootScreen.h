// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Loot/LootInventory.h"

extern char Menu_Loot_previewedLoot;;
extern char Menu_Loot_controlsEnabled;
extern char Menu_Loot_gradientStateIndex;
extern Items_Inventory* Menu_Loot_inventory;;
extern Items_Inventory* Menu_Loot_inventoryBackup;;
extern u_char Menu_Loot_inventoryCapacityFlyinTimer;;
extern char* Menu_Loot_itemCategoryHeaders[];
extern char Menu_Loot_lootClaimed;;
extern u_char Menu_Loot_lootLeftEdge;;
extern Menu_Loot_LootListItem* Menu_Loot_lootList;;
extern u_char Menu_Loot_lootListCount;;
extern u_char Menu_Loot_lootListOffset;;
extern Menu_Loot_LootListItem* Menu_Loot_lootListWorkingCopy;;
extern char Menu_Loot_selectedLoot;;
extern char Menu_Loot_showInventory;;
extern char Menu_Loot_showLoot;;
void Menu_Loot_LootScreen_ResetMenubStatics();
