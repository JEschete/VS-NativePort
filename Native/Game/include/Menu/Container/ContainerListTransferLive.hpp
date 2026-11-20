#pragma once

#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.hpp"

// Binds the container list transfer (ContainerListTransfer.hpp) to MENUD's
// variables, the menu framework and the container helpers in the shapes the
// copied callers use. An invalid input is refused with a throttled warning
// on the "menu.container" channel and the entry returns 0.
namespace VS::Menu::container {
// decomp: _setWeaponUi (src/MENU/MENUD.PRG/234.c:810), 801041E0.
void LiveBuildWeaponRow(VS::Items::ContainerData* container, char** row_strings,
                        int* row_types, char* buffer, int index);
// decomp: _setShieldUi (src/MENU/MENUD.PRG/234.c:820), 8010425C.
void LiveBuildShieldRow(VS::Items::ContainerData* container, char** row_strings,
                        int* row_types, char* buffer, int index);
// decomp: func_80107490 (src/MENU/MENUD.PRG/234.c:2393), 80107490.
int LiveUpdateItemList(int transfer_direction_selection);
// decomp: func_801089BC (src/MENU/MENUD.PRG/234.c:3037), 801089BC.
int LiveUpdateTransferDirection(int initialize_direction_menu);
// decomp: vs_menuD_exec (src/MENU/MENUD.PRG/234.c:3130), 80108C6C.
int LiveUpdateScreen(std::uint8_t* screen_step);
// Restores the controllers' MENUD statics to their load-time values; a
// MENUD.PRG reload (VS::Boot::ResetMenudStatics) must call it.
void ResetLiveContainerTransferStatics();
}  // namespace VS::Menu::container
