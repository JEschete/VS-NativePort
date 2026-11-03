#pragma once
// Hand-maintained (tools/port/owned.csv).
// The legacy view of the inventory state the copied menus read and write.
// InventoryLive.cpp defines it and is the only code that hands it to the
// typed inventory rules (Inventory.hpp).

#ifdef __cplusplus
extern "C" {
#endif

// decomp: vs_battle_miscItemInvoked (BATTLE/BATTLE.PRG 5BF94.h): the misc
// stack slot the item menu last invoked; ConsumeMiscItem searches from it.
extern unsigned char Items_miscItemInvoked;
// decomp: vs_mainMenu_inventoryItemCapacities (MENU/MAINMENU.PRG 2D10.c): the
// slots each menu category lists. The original never wrote it; it is the
// typed VS::Items::kCategoryCapacities.
extern const unsigned char Items_Inventory_inventoryItemCapacities[8];

#ifdef __cplusplus
}
#endif
