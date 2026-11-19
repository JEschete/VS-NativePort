#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Menu/Inventory/EquipmentMenus.hpp"

// Bindings from the equipment menu (EquipmentMenus.hpp) to Ashley's
// inventory, MENU3.PRG's globals (Inventory.hpp), the menu rows, the equipment
// stat panels, the choice list (EquipmentRowsLive.hpp) and Ashley's
// equipment, keeping the original call shapes. Each entry keeps its menu's
// statics here. A count, listed slot or chosen item outside its inventory
// table, a body part outside 1..7, a weapon whose blade or category lies
// outside the tables, a shield chosen without Ashley's battle stats, a
// dispatch row outside the top menu and a top-menu row or text outside its
// table are refused with a throttled warning and return 0.
namespace VS::Menu::Inventory {
// decomp: MENU3.PRG _equipAccessoryMenu (src/MENU/MENU3.PRG/16C.c:343)
int UpdateLiveAccessoryMenu(int initialize);
// decomp: MENU3.PRG _equipWeaponMenu (src/MENU/MENU3.PRG/16C.c:503)
int UpdateLiveWeaponMenu(int initialize);
// decomp: MENU3.PRG _equipShieldMenu (src/MENU/MENU3.PRG/16C.c:683)
int UpdateLiveShieldMenu(int initialize);
// decomp: MENU3.PRG _equipArmorMenu (src/MENU/MENU3.PRG/16C.c:856)
int UpdateLiveArmorMenu(int body_part);
// decomp: MENU3.PRG _dispatchEquipMenu (src/MENU/MENU3.PRG/16C.c:1028)
// Calls through LiveEquipmentMenuHandlers().
int DispatchLiveEquipmentMenu(int row);
// decomp: MENU3.PRG _equipMenu (src/MENU/MENU3.PRG/16C.c:1054)
int UpdateLiveEquipmentMenu(int initialize);
// _dispatchEquipMenu's handler table bound to the Live slot menus, in the
// original's order (armor, weapon, shield, accessory).
[[nodiscard]] const EquipmentMenuHandlers& LiveEquipmentMenuHandlers();
// MENU3.PRG reloads restore the menus' statics (VS::Boot::ResetMenu3Statics).
void ResetEquipmentMenusLive();
// Refusals logged since start-up.
[[nodiscard]] std::uint64_t LiveEquipmentMenusRefusalCount();
}  // namespace VS::Menu::Inventory
