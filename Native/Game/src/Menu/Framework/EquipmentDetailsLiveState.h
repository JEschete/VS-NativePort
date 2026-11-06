#pragma once
// Hand-maintained (tools/port/owned.csv).
// The equipment details' variables in the shapes the copied menus, the
// container binding and the tests read: the item tables the menus show, the
// detail screen's selection, the stat pages and the comparison panel.
// EquipmentDetailsLive.cpp defines them and mirrors them with the typed
// state (EquipmentDetails.hpp) around each call.

#include <stdint.h>

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Items_InventoryArmor Items_InventoryArmor;
typedef struct Items_InventoryBlade Items_InventoryBlade;
typedef struct Items_InventoryGem Items_InventoryGem;
typedef struct Items_InventoryGrip Items_InventoryGrip;
typedef struct Items_InventoryShield Items_InventoryShield;
typedef struct Items_InventoryWeapon Items_InventoryWeapon;

enum {
  kMenuComparisonStatCount = 4,
  kMenuRangeRiskBytes = 8,
  kMenuEquipmentStatCount = 64,
  kMenuPrimaryStatCount = 8,
  kMenuStatusViewElementCount = 21,
  kMenuStatusViewDirections = 4,
};

// decomp: D_80102488 (MENU/MAINMENU.PRG C48.c): [0..2] the actor's base
// minus total strength, intelligence and agility, [3] the actor id.
extern int16_t Menu_Framework_comparisonStatDeltas[kMenuComparisonStatCount];
// decomp: D_80102490 (MENU/MAINMENU.PRG C48.c)
extern int16_t Menu_Framework_comparisonStatTotals[kMenuComparisonStatCount];
// decomp: D_80102498 (MENU/MAINMENU.PRG C48.c)
extern int16_t
    Menu_Framework_comparisonAccessoryStats[kMenuComparisonStatCount];
// decomp: _rangeRiskData (MENU/MAINMENU.PRG C48.c)
extern char Menu_Framework_rangeRiskData[kMenuRangeRiskBytes];
// decomp: vs_mainMenu_weapons .. vs_mainMenu_armor (MENU/MAINMENU.PRG
// 2D10.c): the item tables the menus show, Ashley's inventory's or the open
// container's.
extern Items_InventoryWeapon* Menu_Framework_weapons;
extern Items_InventoryBlade* Menu_Framework_blades;
extern Items_InventoryGrip* Menu_Framework_grips;
extern Items_InventoryShield* Menu_Framework_shields;
extern Items_InventoryArmor* Menu_Framework_armor;
extern Items_InventoryGem* Menu_Framework_gems;
// decomp: vs_mainMenu_currentStatusViewItem (MENU/MAINMENU.PRG 2D10.c)
extern char Menu_Framework_currentStatusViewItem;
// decomp: vs_mainMenu_enabledStatPages (MENU/MAINMENU.PRG C48.c)
extern char Menu_Framework_enabledStatPages;
// decomp: vs_mainMenu_equipmentStats (MENU/MAINMENU.PRG 2D10.c)
extern int16_t Menu_Framework_equipmentStats[kMenuEquipmentStatCount];
// decomp: vs_mainMenu_equipmentSubtype (MENU/MAINMENU.PRG 2D10.c)
extern char Menu_Framework_equipmentSubtype;
// decomp: vs_mainMenu_itemStatPage (MENU/MAINMENU.PRG 2D10.c)
extern char Menu_Framework_itemStatPage;
// decomp: vs_mainMenu_selectedStatusViewElement (MENU/MAINMENU.PRG 2D10.c)
extern u_char Menu_Framework_selectedStatusViewElement;
// decomp: vs_mainMenu_statusViewCursorPositions (MENU/MAINMENU.PRG 2D10.c):
// vs_getXY words.
extern int
    Menu_Framework_statusViewCursorPositions[kMenuStatusViewElementCount];
// decomp: vs_mainMenu_statusViewNavigationMap (MENU/MAINMENU.PRG 2D10.c):
// the element up, down, left and right of each element.
extern u_char
    Menu_Framework_statusViewNavigationMap[kMenuStatusViewElementCount]
                                          [kMenuStatusViewDirections];
// decomp: vs_mainMenu_strIntAgi (MENU/MAINMENU.PRG 2D10.c)
extern int16_t Menu_Framework_strIntAgi[kMenuPrimaryStatCount];

#ifdef __cplusplus
}
#endif
