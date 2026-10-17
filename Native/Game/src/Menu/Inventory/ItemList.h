// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_INVENTORY_ITEMLIST_H_TYPES
#define VS_MENU_INVENTORY_ITEMLIST_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_INVENTORY_ITEMLIST_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_INVENTORY_ITEMLIST_H_DECLS)
#define VS_MENU_INVENTORY_ITEMLIST_H_DECLS
#include "Items/Equipment.h"
#include "Items/Weapons.h"

extern char Menu_Inventory_freezeTabArrows;
extern u_char* Menu_Inventory_inventoryIndices[];
void Menu_Inventory_SortWeapons(int stat);
void Menu_Inventory_SortShields(int stat);
void Menu_Inventory_SortMisc(int stat);
void Menu_Inventory_CopyEquipmentStats(
    Items_EquipmentPartStats* equipment, int itemCategory, int itemIndex);
void Menu_Inventory_SortEquipment(int itemCategory, int stat);
int Menu_Inventory_UpdateSortMenu(int initItemCategory);
int Menu_Inventory_UpdateDiscardMenu(int params);
int Menu_Inventory_ParentWeaponIndex(int itemCategory, int index);
int Menu_Inventory_ParentShieldIndex(int itemCategory, int itemIndex);
int Menu_Inventory_BuildItemRows(
    int itemCategory, char* menuText[], int rowTypes[], char* textBuf);
void Menu_Inventory_RestoreItemRows(int itemCategory);
int Menu_Inventory_UpdateItemList(int initialize);
void Menu_Inventory_ItemList_ResetMainMenuStatics(void);
void Menu_Inventory_ItemList_ResetMenu3Statics(void);

#endif // VS_MENU_INVENTORY_ITEMLIST_H_DECLS
