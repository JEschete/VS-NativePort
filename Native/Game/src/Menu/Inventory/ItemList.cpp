// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Items/Inventory.h"
#include "Menu/Inventory/ItemList.h"

// decomp: vs_mainMenu_freezeTabArrows (vs_mainMenu_freezeTabArrows@MENU/MAINMENU.PRG)
char Menu_Inventory_freezeTabArrows = 0;

// decomp: vs_mainMenu_inventoryIndices (vs_mainMenu_inventoryIndices@MENU/MAINMENU.PRG)
u_char* Menu_Inventory_inventoryIndices[] = { Audio_Seq_unusedInventoryIndices.weapons,
    Audio_Seq_unusedInventoryIndices.blades, Audio_Seq_unusedInventoryIndices.grips,
    Audio_Seq_unusedInventoryIndices.shields, Audio_Seq_unusedInventoryIndices.armor,
    Audio_Seq_unusedInventoryIndices.gems, Audio_Seq_unusedInventoryIndices.misc };

// MAINMENU.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ItemList_ResetMainMenuStatics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Inventory_freezeTabArrows, &initial, sizeof Menu_Inventory_freezeTabArrows);
    }
    {
        static const u_char* initial[] = { Audio_Seq_unusedInventoryIndices.weapons,
    Audio_Seq_unusedInventoryIndices.blades, Audio_Seq_unusedInventoryIndices.grips,
    Audio_Seq_unusedInventoryIndices.shields, Audio_Seq_unusedInventoryIndices.armor,
    Audio_Seq_unusedInventoryIndices.gems, Audio_Seq_unusedInventoryIndices.misc };
        memcpy((void*)&Menu_Inventory_inventoryIndices, &initial, sizeof Menu_Inventory_inventoryIndices);
    }
}

