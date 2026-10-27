// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_SETUP_ASSEMBLY_H_TYPES
#define VS_MENU_SETUP_ASSEMBLY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_SETUP_ASSEMBLY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_SETUP_ASSEMBLY_H_DECLS)
#define VS_MENU_SETUP_ASSEMBLY_H_DECLS
#include "Items/Inventory.h"
#include "Items/Weapons.h"

extern char Menu_Setup_layoutPadding6[2];;
extern char Menu_Setup_availableItems[64];;
extern u_char Menu_Setup_combiningItem;;
extern u_int Menu_Setup_slotSelectionHistory[5];;
void Menu_Setup_RecordSlotSelection(int index);
int Menu_Setup_PopSlotSelection(int index);
void Menu_Setup_DisassembleWeapon(int weaponIndex);
void Menu_Setup_AssembleWeapon(int bladeIndex, int gripIndex, int gemInfo);
void Menu_Setup_DetachShieldGems(int index);
int Menu_Setup_IsGripCompatible(Items_InventoryBlade* blade, Items_InventoryGrip* grip);
void Menu_Setup_SetAssemblyStats(int blade, int grip);
void Menu_Setup_SetAssemblyHeader(int bladeGrip);
int Menu_Setup_SelectAssemblyBlade(int params);
int Menu_Setup_SelectAssemblyGrip(int params);
int Menu_Setup_SelectGem(int params);
int Menu_Setup_CancelAssembly(void);
int Menu_Setup_UpdateAssembly(int initialize_assembly);
void Menu_Setup_CreateEmptyGemSlotRow(int arg0 __attribute__((unused)), int gem_row_index);
int Menu_Setup_RestoreGemsAndLeaveSelection(char* gems, int isShield);
int Menu_Setup_UpdateGemAttachment(int equipment_category_selection);
int Menu_Setup_UpdateGemCategoryMenu(int initialize_gem_category_menu);
void Menu_Setup_Assembly_ResetMenucStatics(void);

#endif // VS_MENU_SETUP_ASSEMBLY_H_DECLS
