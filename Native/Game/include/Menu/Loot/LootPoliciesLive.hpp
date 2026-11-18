#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Menu/Loot/LootInventory.h"

namespace VS::Menu::Loot::Live {  // NOLINT(readability-identifier-naming)
// decomp: MENUB.PRG _displayInventoryCapacities, 8010837C
void RenderBagCapacities(int edge_x);
// decomp: MENUB.PRG _displayCurrentLoot, 80108938
void RenderLootRows(int x);
// decomp: MENUB.PRG _populateLootItem, 80108E78
void ShowLootDescription(int loot_index);
// decomp: MENUB.PRG _discardItems, 801090A4
[[nodiscard]] int UpdateLeaveConfirmation(int init);
// decomp: MENUB.PRG _organizeInventory, 80109444
[[nodiscard]] int UpdateOrganizationMenu(Menu_Loot_LootListItem* loot);
// Restores the leave and organize prompts' statics (MENUB.PRG reload).
void ResetPolicyStatics();
}  // namespace VS::Menu::Loot::Live
