// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Menu/Inventory/ItemList.h"

#include "Items/InventoryLive.hpp"
#include "Legacy/Psx.h"

// decomp: vs_mainMenu_freezeTabArrows (vs_mainMenu_freezeTabArrows@MENU/MAINMENU.PRG)
char Menu_Inventory_freezeTabArrows = 0;

// decomp: vs_mainMenu_inventoryIndices (vs_mainMenu_inventoryIndices@MENU/MAINMENU.PRG)
u_char* Menu_Inventory_inventoryIndices[] = {
    VS::Items::inventoryIndices.weapons.data(),
    VS::Items::inventoryIndices.blades.data(),
    VS::Items::inventoryIndices.grips.data(),
    VS::Items::inventoryIndices.shields.data(),
    VS::Items::inventoryIndices.armor.data(),
    VS::Items::inventoryIndices.gems.data(),
    VS::Items::inventoryIndices.misc.data()};

// MAINMENU.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ItemList_ResetMainMenuStatics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Inventory_freezeTabArrows, &initial, sizeof Menu_Inventory_freezeTabArrows);
    }
    {
      static const u_char* initial[] = {
          VS::Items::inventoryIndices.weapons.data(),
          VS::Items::inventoryIndices.blades.data(),
          VS::Items::inventoryIndices.grips.data(),
          VS::Items::inventoryIndices.shields.data(),
          VS::Items::inventoryIndices.armor.data(),
          VS::Items::inventoryIndices.gems.data(),
          VS::Items::inventoryIndices.misc.data()};
      memcpy((void*)&Menu_Inventory_inventoryIndices, &initial,
             sizeof Menu_Inventory_inventoryIndices);
    }
}

