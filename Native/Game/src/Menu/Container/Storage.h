// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Menu_Container_InventoryStorage;

#include "Items/ItemRecords.hpp"

// decomp: copyContainerFlags (src/MENU/MENUD.PRG/234.c:17)
enum Menu_Container_CopyContainerFlag { Menu_Container_CopyContainerFlag_CopyContainerFlagsCopy = 0x10 };

// decomp: vs_menu_inventoryStorage_t (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:153)
struct Menu_Container_InventoryStorage {
    char savedInventory[15][256];
    VS::Items::InventoryIndices savedInventoryIndices;
    VS::Items::ContainerData storedContainer;
    VS::Items::ContainerData transferPreview;
    VS::Items::ContainerData stagedTransfer;
    u_short itemsToTransfer[64];
    VS::Items::Container container;
    VS::Items::Container gadgetBag;
};
static_assert(sizeof(Menu_Container_InventoryStorage) == 0x14730, "Menu_Container_InventoryStorage must keep its PS1 layout");

extern u_short Menu_Container_transferredMiscCount;;
extern u_short Menu_Container_containerItemCapacities[];
void Menu_Container_Storage_ResetMenudStatics();
