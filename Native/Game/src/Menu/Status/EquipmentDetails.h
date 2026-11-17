// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_STATUS_EQUIPMENTDETAILS_H_TYPES
#define VS_MENU_STATUS_EQUIPMENTDETAILS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_STATUS_EQUIPMENTDETAILS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_STATUS_EQUIPMENTDETAILS_H_DECLS)
#define VS_MENU_STATUS_EQUIPMENTDETAILS_H_DECLS
#include "Items/Equipment.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

int Menu_Status_NavigateDetailElements(u_int newSelection, int flags);
void Menu_Status_HoistEquipmentRow(int row);
void Menu_Status_BeginDetailTransition(int selectedRow);
void Menu_Status_InitializeDetailPanel(int selectedRow, int classAffinity);
void Menu_Status_SetWeaponComponentRow(int row, Items_WeaponLoadout* weapon, int init);
void Menu_Status_SetShieldGemRow(int row, Items_ShieldLoadout* shield, int init);
void Menu_Status_ExitEquipmentDetails(int clearDpPp);
int Menu_Status_UpdateEquipmentDetails(int row);
int Menu_Status_UpdateEquipmentList(int element);
void Menu_Status_EquipmentDetails_ResetMenu4Statics(void);

#endif // VS_MENU_STATUS_EQUIPMENTDETAILS_H_DECLS
