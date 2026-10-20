// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_EQUIPMENTDETAILS_H_TYPES
#define VS_MENU_FRAMEWORK_EQUIPMENTDETAILS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_FRAMEWORK_EQUIPMENTDETAILS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_FRAMEWORK_EQUIPMENTDETAILS_H_DECLS)
#define VS_MENU_FRAMEWORK_EQUIPMENTDETAILS_H_DECLS
#include "Items/Inventory.h"
#include "Items/Weapons.h"

extern short Menu_Framework_comparisonStatDeltas[4];;
extern short Menu_Framework_comparisonStatTotals[4];;
extern short Menu_Framework_comparisonAccessoryStats[4];;
extern u_short Menu_Framework_currentDp;
extern u_short Menu_Framework_currentPp;
extern u_short Menu_Framework_maxDp;
extern u_short Menu_Framework_maxPp;
extern char Menu_Framework_rangeRiskData[8];;
extern char Menu_Framework_uiElement15Enabled;
extern u_short Menu_Framework_dpPpBarAnimState;
extern Items_InventoryArmor* Menu_Framework_armor;;
extern Items_InventoryBlade* Menu_Framework_blades;;
extern char Menu_Framework_currentStatusViewItem;;
extern char Menu_Framework_enabledStatPages;;
extern short Menu_Framework_equipmentStats[64];;
extern char Menu_Framework_equipmentSubtype;;
extern Items_InventoryGem* Menu_Framework_gems;;
extern Items_InventoryGrip* Menu_Framework_grips;;
extern char Menu_Framework_itemStatPage;;
extern u_char Menu_Framework_selectedStatusViewElement;;
extern Items_InventoryShield* Menu_Framework_shields;;
extern int Menu_Framework_statusViewCursorPositions[];
extern u_char Menu_Framework_statusViewNavigationMap[][4];
extern short Menu_Framework_strIntAgi[8];;
extern Items_InventoryWeapon* Menu_Framework_weapons;;
void Menu_Framework_ResetEquipmentStats(void);
void Menu_Framework_SetWeaponStats(int weaponIndex);
void Menu_Framework_SetBladeStats(int bladeIndex);
void Menu_Framework_SetGripStats(int gripIndex);
void Menu_Framework_SetShieldStats(int shieldIndex);
void Menu_Framework_SetArmorStats(int armorIndex);
void Menu_Framework_SetGemStats(int gemIndex);
char* Menu_Framework_SelectWeaponDetailRow(int row);
char* Menu_Framework_BladeDetailDescription(int arg0 __attribute__((unused)));
char* Menu_Framework_GripDetailDescription(int arg0 __attribute__((unused)));
char* Menu_Framework_SelectShieldDetailRow(int arg0);
char* Menu_Framework_ArmorDetailDescription(int arg0 __attribute__((unused)));
char* Menu_Framework_GemDetailDescription(int arg0 __attribute__((unused)));
void Menu_Framework_ResetItemDetailCursor(void);
int Menu_Framework_IsItemDetailElementReachable(int selectedElement);
void Menu_Framework_UpdateItemDetails(void);
int Menu_Framework_OffsetStatBarCoordinate(int a0, int a1, int a2);
void Menu_Framework_DrawEquipmentStatPage(int xOffset);
int Menu_Framework_NextEnabledStatPage(int currentPage);
void Menu_Framework_SetEnabledStatPages(int enabledPage);
void Menu_Framework_UpdateEquipmentStatPages(int arg0);
void Menu_Framework_SetRangeAndRisk(int range, int risk, int arg2, int arg3);
void Menu_Framework_SetPrimaryStats(
    int strength, int intelligence, int agility, int valuesKnown);
void Menu_Framework_SetActorComparisonStats(int actorId);
void Menu_Framework_UpdatePrimaryStatPanel(int animate);
void Menu_Framework_SetDpPp(int currentDp, int maxDp, int currentPp, int maxPp);
void Menu_Framework_UpdateDpPpBars(int flags);
void Menu_Framework_EquipmentDetails_ResetMainMenuStatics(void);

#endif // VS_MENU_FRAMEWORK_EQUIPMENTDETAILS_H_DECLS
