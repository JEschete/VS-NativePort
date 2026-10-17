// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_ITEMROWS_H_TYPES
#define VS_MENU_FRAMEWORK_ITEMROWS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_FRAMEWORK_ITEMROWS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_FRAMEWORK_ITEMROWS_H_DECLS)
#define VS_MENU_FRAMEWORK_ITEMROWS_H_DECLS
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"

extern u_short* Menu_Framework_itemHelp;;
extern char (*Menu_Framework_itemNames)[24];
extern u_short* Menu_Framework_menu12Text;;
extern u_char Menu_Framework_weaponHands[];
Menu_Framework_MenuRow* Menu_Framework_CreateWeaponComponentRow(
    int row, int weaponIndex, int slide);
Menu_Framework_MenuRow* Menu_Framework_CreateShieldGemRow(
    int row, int shieldIndex, int slide);
void Menu_Framework_FormatWeaponRow(
    Items_WeaponLoadout* weapon, char* menuText[], int* rowType, void* buf);
void Menu_Framework_FormatInventoryWeaponRow(
    Items_InventoryWeapon* weapon, char* menuText[], int* rowType, void* buf);
void Menu_Framework_FormatBladeRow(
    Items_InventoryBlade* blade, char* menuText[], int* rowType, void* buf);
void Menu_Framework_FormatGripRow(
    Items_InventoryGrip* grip, char* menuText[], int* rowType, void* buf);
void Menu_Framework_FormatShieldRow(
    Items_ShieldLoadout* shield, char* menuText[], int* rowType, void* buf);
void Menu_Framework_FormatInventoryShieldRow(
    Items_InventoryShield* shield, char* menuText[], int* rowType, void* buf);
void Menu_Framework_FormatArmorRow(
    Items_ArmorLoadout* armor, char* menuText[], int* rowType, void* buf);
void Menu_Framework_FormatInventoryArmorRow(
    Items_InventoryArmor* armor, char* menuText[], int* rowType, void* buf);
void Menu_Framework_FormatGemRow(
    Items_InventoryGem* gem, char* menuText[], int* rowType, void* buf);
void Menu_Framework_FormatMiscItemRow(
    Items_InventoryMisc* misc, char* menuText[], int* rowType, void* buf);
int Menu_Framework_LoadItemText(int load);
int Menu_Framework_UnloadItemTextWhenReady(void);
int Menu_Framework_EquipmentTitleRanks(void* item, u_int itemCategory);
void Menu_Framework_ItemRows_ResetMainMenuStatics(void);

#endif // VS_MENU_FRAMEWORK_ITEMROWS_H_DECLS
