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

void Menu_Status_PopulateWeaponStats(Items_WeaponLoadout* weapon);
void Menu_Status_PopulateShieldStats(Items_ShieldLoadout* shield);
void Menu_Status_PopulateArmorStats(Items_ArmorLoadout* armor);
void Menu_Status_PopulateAccessoryStats(Items_AccessorySpawnLoadout* accessory);
void Menu_Status_PopulateBladeStats(Items_WeaponLoadout* weapon);
void Menu_Status_PopulateGripStats(Items_EquipmentPartStats* grip);
void Menu_Status_PopulateGemStats(Items_EquipmentPartStats* gem);
void Menu_Status_FormatBladeDescription(Items_EquipmentPartStats* blade, char* menuText[],
    int rowTypes[] __attribute__((unused)));
void Menu_Status_FormatGripDescription(
    Items_EquipmentPartStats* grip, char* menuText[], int rowTypes[] __attribute__((unused)));
void Menu_Status_CopyGemDescription(
    Items_EquipmentPartStats* gem, char* menuText[], int rowTypes[] __attribute__((unused)));
char* Menu_Status_SelectWeaponComponent(int row, Items_WeaponLoadout* weapon);
char* Menu_Status_SelectShieldComponent(int row, Items_ShieldLoadout* shield);
char* Menu_Status_ArmorDescription(Items_ArmorLoadout* armor_loadout);
char* Menu_Status_AccessoryDescription(Items_AccessorySpawnLoadout* accessory_loadout);
int Menu_Status_IsDetailElementSelectable(int newSelection, int flags);
int Menu_Status_NavigateDetailElements(u_int newSelection, int flags);
int Menu_Status_EquipmentRowCount(void);
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
