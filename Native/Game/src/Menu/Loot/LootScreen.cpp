// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Loot/LootInventory.h"
#include "Menu/Loot/LootScreen.h"

// decomp: D_8010A6B7 (D_8010A6B7@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_previewedLoot;

// decomp: _controlsEnabled (_controlsEnabled@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_controlsEnabled = 0;

// decomp: _gradientStateIndex (_gradientStateIndex@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_gradientStateIndex = 0;

// decomp: _inventory (_inventory@src/MENU/MENUB.PRG/260.c)
Items_Inventory* Menu_Loot_inventory;

// decomp: _inventoryBackup (_inventoryBackup@src/MENU/MENUB.PRG/260.c)
Items_Inventory* Menu_Loot_inventoryBackup;

// decomp: _inventoryCapacityFlyinTimer (_inventoryCapacityFlyinTimer@src/MENU/MENUB.PRG/260.c)
u_char Menu_Loot_inventoryCapacityFlyinTimer;

// decomp: _itemCategoryHeaders (_itemCategoryHeaders@src/MENU/MENUB.PRG/260.c)
char* Menu_Loot_itemCategoryHeaders[] = { "WEAPON", "BLADE", "GRIP", "SHIELD", "ARMOR",
    "GEM", "MISC" };

// decomp: _lootClaimed (_lootClaimed@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_lootClaimed;

// decomp: _lootLeftEdge (_lootLeftEdge@src/MENU/MENUB.PRG/260.c)
u_char Menu_Loot_lootLeftEdge;

// decomp: _lootList (_lootList@src/MENU/MENUB.PRG/260.c)
Menu_Loot_LootListItem* Menu_Loot_lootList;

// decomp: _lootListCount (_lootListCount@src/MENU/MENUB.PRG/260.c)
u_char Menu_Loot_lootListCount;

// decomp: _lootListOffset (_lootListOffset@src/MENU/MENUB.PRG/260.c)
u_char Menu_Loot_lootListOffset;

// decomp: _lootListWorkingCopy (_lootListWorkingCopy@src/MENU/MENUB.PRG/260.c)
Menu_Loot_LootListItem* Menu_Loot_lootListWorkingCopy;

// decomp: _selectedLoot (_selectedLoot@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_selectedLoot;

// decomp: _showInventory (_showInventory@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_showInventory;

// decomp: _showLoot (_showLoot@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_showLoot;

// MENUB.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Loot_LootScreen_ResetMenubStatics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Loot_controlsEnabled, &initial, sizeof Menu_Loot_controlsEnabled);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Loot_gradientStateIndex, &initial, sizeof Menu_Loot_gradientStateIndex);
    }
    {
        static const char* initial[] = { "WEAPON", "BLADE", "GRIP", "SHIELD", "ARMOR",
    "GEM", "MISC" };
        memcpy((void*)&Menu_Loot_itemCategoryHeaders, &initial, sizeof Menu_Loot_itemCategoryHeaders);
    }
}

