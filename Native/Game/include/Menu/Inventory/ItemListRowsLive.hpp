#pragma once

#include <cstdint>

// Binds the item list rows (ItemListRows.hpp) to the inventory, its display
// order, the misc action map, the battle action table, the menu rows and the
// caller's heap row buffers, in the generated Menu_Inventory_* shapes. Invalid
// input is refused with a throttled warning: the parent links return 0, the
// row builders return 0 rows and the restore leaves the rows untouched.
namespace VS::Menu::Inventory {
// decomp: MENU3.PRG _getParentWeaponIndex (items2.h:11).
int LiveParentWeaponIndex(int item_category, int index);
// decomp: MENU3.PRG _getParentShieldIndex (items2.h:38).
int LiveParentShieldIndex(int item_category, int item_index);
// decomp: MENU3.PRG _populateItemRows (items2.h:60), with _ITEMMENU: misc rows
// record their actions in _miscActions.
int LiveBuildItemRows(int item_category, char* menu_text[], int row_types[],
                      char* text_buf);
// decomp: MENUB.PRG _populateItemRows (items2.h:60), without _ITEMMENU.
int LiveBuildLootItemRows(int item_category, char* menu_text[], int row_types[],
                          char* text_buf);
// decomp: MENU3.PRG _restoreItemMenu (items2.h:175).
void LiveRestoreItemRows(int item_category);
// Refusals since start-up.
[[nodiscard]] std::uint64_t ItemListRowsLiveRefusalCount() noexcept;
}  // namespace VS::Menu::Inventory
