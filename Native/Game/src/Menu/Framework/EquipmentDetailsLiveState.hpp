#pragma once
// Hand-maintained (tools/port/owned.csv).
// The equipment details' variables in the shapes the copied menus, the
// container binding and the tests read: the item tables the menus show, the
// detail screen's selection, the stat pages and the comparison panel.
// EquipmentDetailsLive.cpp defines them and mirrors them with the typed
// state (EquipmentDetails.hpp) around each call.

#include <stdint.h>

#include "Legacy/Psx.h"
#include "Menu/Framework/EquipmentDetails.hpp"

struct Items_InventoryArmor;
struct Items_InventoryBlade;
struct Items_InventoryGem;
struct Items_InventoryGrip;
struct Items_InventoryShield;
struct Items_InventoryWeapon;

namespace VS::Menu::Framework {
// decomp: D_80102488 (MENU/MAINMENU.PRG C48.c): [0..2] the actor's base
// minus total strength, intelligence and agility, [3] the actor id.
extern int16_t comparisonStatDeltas[kComparisonStatCount];
// decomp: D_80102490 (MENU/MAINMENU.PRG C48.c)
extern int16_t comparisonStatTotals[kComparisonStatCount];
// decomp: D_80102498 (MENU/MAINMENU.PRG C48.c)
extern int16_t comparisonAccessoryStats[kComparisonStatCount];
// decomp: _rangeRiskData (MENU/MAINMENU.PRG C48.c)
extern char rangeRiskData[kRangeRiskBytes];
// decomp: vs_mainMenu_weapons .. vs_mainMenu_armor (MENU/MAINMENU.PRG
// 2D10.c): the item tables the menus show, Ashley's inventory's or the open
// container's.
extern Items_InventoryWeapon* weapons;
extern Items_InventoryBlade* blades;
extern Items_InventoryGrip* grips;
extern Items_InventoryShield* shields;
extern Items_InventoryArmor* armor;
extern Items_InventoryGem* gems;
// decomp: vs_mainMenu_currentStatusViewItem (MENU/MAINMENU.PRG 2D10.c)
extern char currentStatusViewItem;
// decomp: vs_mainMenu_enabledStatPages (MENU/MAINMENU.PRG C48.c)
extern char enabledStatPages;
// decomp: vs_mainMenu_equipmentStats (MENU/MAINMENU.PRG 2D10.c)
extern int16_t equipmentStats[kEquipmentStatCount];
// decomp: vs_mainMenu_equipmentSubtype (MENU/MAINMENU.PRG 2D10.c)
extern char equipmentSubtype;
// decomp: vs_mainMenu_itemStatPage (MENU/MAINMENU.PRG 2D10.c)
extern char itemStatPage;
// decomp: vs_mainMenu_selectedStatusViewElement (MENU/MAINMENU.PRG 2D10.c)
extern u_char selectedStatusViewElement;
// decomp: vs_mainMenu_statusViewCursorPositions (MENU/MAINMENU.PRG 2D10.c):
// vs_getXY words.
extern int statusViewCursorPositions[kStatusViewElementCount];
// decomp: vs_mainMenu_statusViewNavigationMap (MENU/MAINMENU.PRG 2D10.c):
// the element up, down, left and right of each element.
using StatusViewNeighbours = u_char[kStatusViewDirections];
extern StatusViewNeighbours statusViewNavigationMap[kStatusViewElementCount];
// decomp: vs_mainMenu_strIntAgi (MENU/MAINMENU.PRG 2D10.c)
extern int16_t strIntAgi[kPrimaryStatCount];
}  // namespace VS::Menu::Framework
