#pragma once
// Hand-maintained (tools/port/owned.csv).
// The legacy view of the inventory state the copied menus read and write.
// InventoryLive.cpp defines it and is the only code that hands it to the
// typed inventory rules (Inventory.hpp).

namespace VS::Items {

// decomp: vs_battle_miscItemInvoked (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/battleUi.h:305): the misc stack slot the item menu last
// invoked; ConsumeMiscItem searches from it.
extern unsigned char miscItemInvoked;

}  // namespace VS::Items
