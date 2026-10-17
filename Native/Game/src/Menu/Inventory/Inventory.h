// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_INVENTORY_INVENTORY_H_TYPES
#define VS_MENU_INVENTORY_INVENTORY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_INVENTORY_INVENTORY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_INVENTORY_INVENTORY_H_DECLS)
#define VS_MENU_INVENTORY_INVENTORY_H_DECLS

extern int Menu_Inventory_ashleyRenderState;
extern int Menu_Inventory_ashleyYOffset;
extern int Menu_Inventory_exitToBattle;;
extern char Menu_Inventory_itemCategoryIconsAnimationStep;;
extern char Menu_Inventory_itemCategoryIconsEnabled;;
extern char Menu_Inventory_secondaryTitleAnimationStep;;
extern char Menu_Inventory_secondaryTitleEnabled;;
extern char Menu_Inventory_selectedItemCategoryIconOnTop;;
extern char Menu_Inventory_statusCommandAnimationStep;;
extern char Menu_Inventory_statusCommandState;;
extern char Menu_Inventory_equipmentModelLoadStep;
extern u_char Menu_Inventory_miscItemToActionMap[];
extern char Menu_Inventory_equippedItem;;
extern char Menu_Inventory_itemsFirstRow;;
extern char* Menu_Inventory_itemsText;;
extern char Menu_Inventory_menuRowCount;;
extern u_short Menu_Inventory_menuText[];
extern char Menu_Inventory_unusedMiscActions[64];;
extern int Menu_Inventory_selectedItemRow;;
extern u_char Menu_Inventory_equipListStep;;
void Menu_Inventory_UpdateMiscItemShortcut(int initialize);
void Menu_Inventory_CreateEquipmentRows(int count, u_short** menuText, int* rowTypes);
int Menu_Inventory_GetSelectedEquipmentRow(void);
void Menu_Inventory_UpdateEquipmentChoices(int hasSfx);
void Menu_Inventory_SetAccessoryPreviewStats(int accessoryId);
int Menu_Inventory_UpdateAccessoryMenu(int initialize);
int Menu_Inventory_UpdateWeaponMenu(int initialize);
int Menu_Inventory_UpdateShieldMenu(int initialize);
int Menu_Inventory_UpdateArmorMenu(int initBodyPart);
int Menu_Inventory_DispatchEquipmentMenu(int row);
int Menu_Inventory_UpdateEquipmentMenu(int initialize);
int Menu_Inventory_Update(u_char* state);
int Menu_Inventory_ReloadEquipmentModels(int category, int wepId, int material);
int Menu_Inventory_IsTwoHandedWeapon(int category);
void Menu_Inventory_QueueEquipmentModel(int wepId, int material);
void Menu_Inventory_ResetMainMenuStatics(void);
void Menu_Inventory_ResetMenu3Statics(void);
void Menu_Inventory_ResetMenubStatics(void);

#endif // VS_MENU_INVENTORY_INVENTORY_H_DECLS
