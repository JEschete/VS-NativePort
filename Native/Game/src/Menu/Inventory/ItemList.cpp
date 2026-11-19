// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Items/Inventory.h"
#include "Menu/Inventory/ItemList.h"

// decomp: vs_mainMenu_freezeTabArrows (vs_mainMenu_freezeTabArrows@MENU/MAINMENU.PRG)
char Menu_Inventory_freezeTabArrows = 0;

// decomp: vs_mainMenu_inventoryIndices (vs_mainMenu_inventoryIndices@MENU/MAINMENU.PRG)
u_char* Menu_Inventory_inventoryIndices[] = { VS::Items::inventoryIndices.weapons,
    VS::Items::inventoryIndices.blades, VS::Items::inventoryIndices.grips,
    VS::Items::inventoryIndices.shields, VS::Items::inventoryIndices.armor,
    VS::Items::inventoryIndices.gems, VS::Items::inventoryIndices.misc };

// MAINMENU.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ItemList_ResetMainMenuStatics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Inventory_freezeTabArrows, &initial, sizeof Menu_Inventory_freezeTabArrows);
    }
    {
        static const u_char* initial[] = { VS::Items::inventoryIndices.weapons,
    VS::Items::inventoryIndices.blades, VS::Items::inventoryIndices.grips,
    VS::Items::inventoryIndices.shields, VS::Items::inventoryIndices.armor,
    VS::Items::inventoryIndices.gems, VS::Items::inventoryIndices.misc };
        memcpy((void*)&Menu_Inventory_inventoryIndices, &initial, sizeof Menu_Inventory_inventoryIndices);
    }
}

