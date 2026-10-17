// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_LOOT_LOOTSCREEN_H_TYPES
#define VS_MENU_LOOT_LOOTSCREEN_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_LOOT_LOOTSCREEN_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_LOOT_LOOTSCREEN_H_DECLS)
#define VS_MENU_LOOT_LOOTSCREEN_H_DECLS
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
int Menu_Loot_UpdateInventoryTransition(int category);
int Menu_Loot_UpdateDisassembly(int itemIndex);
int Menu_Loot_UpdateStackConsolidation(int lootIndex);
void Menu_Loot_RenderBagCapacities(int edgeX);
void Menu_Loot_RenderLootRows(int x);
void Menu_Loot_ShowLootDescription(int lootIndex);
int Menu_Loot_UpdateLeaveConfirmation(int init);
int Menu_Loot_UpdateOrganizationMenu(Menu_Loot_LootListItem* loot);
int Menu_Loot_UpdateLootSelection(int initialize);
int Menu_Loot_Update(u_char* state);
void Menu_Loot_LootScreen_ResetMenubStatics(void);

#endif // VS_MENU_LOOT_LOOTSCREEN_H_DECLS
