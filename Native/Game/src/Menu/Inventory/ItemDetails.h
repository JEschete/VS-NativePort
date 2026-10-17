// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_INVENTORY_ITEMDETAILS_H_TYPES
#define VS_MENU_INVENTORY_ITEMDETAILS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_INVENTORY_ITEMDETAILS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_INVENTORY_ITEMDETAILS_H_DECLS)
#define VS_MENU_INVENTORY_ITEMDETAILS_H_DECLS

void Menu_Inventory_SetPanelVisibility(char state);
int Menu_Inventory_PollDetailsPaging(int items, int page);
void Menu_Inventory_DrawCategoryIcons(int animationStep, int highlightSelected);
void Menu_Inventory_BeginDetailsTransition(int id);
void Menu_Inventory_InitializeDetailsLayout(int selectedItem, int classAffinityType);
int Menu_Inventory_SelectDetailsItem(int itemCategory, int itemId);
void Menu_Inventory_ExitDetailsView(int clearDpPp);
int Menu_Inventory_UpdateWeaponDetails(int itemInfo);
int Menu_Inventory_UpdateBladeDetails(int itemInfo);
int Menu_Inventory_UpdateGripDetails(int itemInfo);
int Menu_Inventory_UpdateShieldDetails(int itemInfo);
int Menu_Inventory_UpdateArmorDetails(int itemInfo);
int Menu_Inventory_UpdateGemDetails(int itemInfo);
int Menu_Inventory_UpdateItemDetails(int itemInfo);
void Menu_Inventory_ItemDetails_ResetMenu3Statics(void);

#endif // VS_MENU_INVENTORY_ITEMDETAILS_H_DECLS
