// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_SETUP_SETUPSCREEN_H_TYPES
#define VS_MENU_SETUP_SETUPSCREEN_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_SETUP_SETUPSCREEN_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_SETUP_SETUPSCREEN_H_DECLS)
#define VS_MENU_SETUP_SETUPSCREEN_H_DECLS

extern char Menu_Setup_itemCount;;
extern u_short* Menu_Setup_itemsList;;
extern int Menu_Setup_itemsListSelection;;
extern char Menu_Setup_itemsListState;;
extern char Menu_Setup_itemsListWindow;;
extern char Menu_Setup_rowInfoUnused;;
void Menu_Setup_DrawConfirmationLabel(int row, int brightness);
void Menu_Setup_SetTitleRow(int id, int textTableOffset);
void Menu_Setup_InitializeEquipmentHeader(int textOffset, int icon);
void Menu_Setup_MoveMenuRow(int source, int target);
int Menu_Setup_UpdateConfirmationPrompt(int arg0);
int Menu_Setup_UpdateThreeChoicePrompt(int arg0);
int Menu_Setup_LeaveItemSelection(int arg0);
void Menu_Setup_BuildItemList(int count, char** text, int* rowTypes);
int Menu_Setup_GetSelectedItemRow(void);
void Menu_Setup_UpdateItemList(int arg0);
void Menu_Setup_AnimateItemHeader(int arg0);
int Menu_Setup_UpdateDisassembly(int arg0);
int Menu_Setup_UpdateDisassemblyCategory(int arg0);
int Menu_Setup_UpdateWeaponRename(int arg0);
int Menu_Setup_UpdateRepair(int arg0);
int Menu_Setup_Update(u_char* state);
int Menu_Setup_ReloadAshleyEquipment(int arg0);
void Menu_Setup_SetupScreen_ResetMenucStatics(void);

#endif // VS_MENU_SETUP_SETUPSCREEN_H_DECLS
