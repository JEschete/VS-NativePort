// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_LOOT_LOOTINVENTORY_H_TYPES
#define VS_MENU_LOOT_LOOTINVENTORY_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Loot_LootListItem Menu_Loot_LootListItem;

// decomp: lootItemState (src/MENU/MENUB.PRG/260.c:15)
enum Menu_Loot_LootItemState {
    Menu_Loot_LootItemState_None,
    Menu_Loot_LootItemState_Copied,
    Menu_Loot_LootItemState_NoIndent,
    Menu_Loot_LootItemState_Indent1,
    Menu_Loot_LootItemState_Indent2,
    Menu_Loot_LootItemState_Indent3
};
typedef enum Menu_Loot_LootItemState Menu_Loot_LootItemState;

// decomp: _lootListItem (src/MENU/MENUB.PRG/260.c:29)
typedef struct Menu_Loot_LootListItem {
    char itemCategory;
    u_char itemIndex;
    char opaque2;
    char state;
} Menu_Loot_LootListItem;
_Static_assert(sizeof(Menu_Loot_LootListItem) == 0x4, "Menu_Loot_LootListItem must keep its PS1 layout");

#endif // VS_MENU_LOOT_LOOTINVENTORY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_LOOT_LOOTINVENTORY_H_DECLS)
#define VS_MENU_LOOT_LOOTINVENTORY_H_DECLS
#include "Items/Inventory.h"
#include "Items/Loot.h"

void Menu_Loot_RefreshClaimableRows(void);
void Menu_Loot_AppendLootRow(int itemCategory, int itemIndex);
int Menu_Loot_CountContainedCategory(int counted_category, Menu_Loot_LootListItem* loot_list_item);
int Menu_Loot_IsDropAllowed(int drop_difficulty_flags);
void Menu_Loot_StageDropRecord(Items_BattleLoot* drop_record);
void Menu_Loot_BuildStagedInventory(Items_LootListNode* node);
void Menu_Loot_RecordClaimedKey(int item_id);

#endif // VS_MENU_LOOT_LOOTINVENTORY_H_DECLS
