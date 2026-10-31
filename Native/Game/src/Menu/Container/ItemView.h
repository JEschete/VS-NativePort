// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_CONTAINER_ITEMVIEW_H_TYPES
#define VS_MENU_CONTAINER_ITEMVIEW_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_CONTAINER_ITEMVIEW_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_CONTAINER_ITEMVIEW_H_DECLS)
#define VS_MENU_CONTAINER_ITEMVIEW_H_DECLS
#include "Items/Inventory.h"
#include "Items/Weapons.h"

extern char const* Menu_Container_kStorageNames[];
extern char const* Menu_Container_kCategoryLabels[];
extern u_char Menu_Container_browsingGadgetBag;;
extern char Menu_Container_categoryTitleVisible;;
extern char Menu_Container_categoryIconsVisible;;
extern char Menu_Container_controlsPanelMode;;
extern char Menu_Container_categoryTitleSlideStep;;
extern char Menu_Container_categoryIconsSlideStep;;
extern char Menu_Container_controlsPanelSlideStep;;
extern char Menu_Container_detailsViewPhase;;
extern int Menu_Container_detailsClosedWithTriangle;;
extern Items_Container* Menu_Container_otherStorage;;
extern u_short Menu_Container_menuText[690]; /* contents: disc string table (ModuleData.cpp) */;
extern Items_Container* Menu_Container_containerData;;
void Menu_Container_SetPanelVisibility(char panel_visibility);
int Menu_Container_PollDetailsPaging(int itemCategory, int currentSelection);
void Menu_Container_DrawCategoryIcons(int arg0, int details_view_phase);
void Menu_Container_BeginDetailsTransition(int menu_row_id);
void Menu_Container_InitializeDetailsLayout(int selected_menu_row_id, int stat_page_panel_command);
int Menu_Container_SelectDetailsItem(int category_id, int category_item_index);
void Menu_Container_ExitDetailsView(int hide_dp_pp_bars);
void Menu_Container_BuildWeaponRow(Items_ContainerData* container, char** rowStrings,
    int* rowTypes, char* buf, int index);
void Menu_Container_BuildShieldRow(Items_ContainerData* container, char** rowStrings,
    int* rowTypes, char* buf, int index);
int Menu_Container_UpdateWeaponDetails(int weaponIndex);
int Menu_Container_UpdateBladeDetails(int bladeIndex);
int Menu_Container_UpdateGripDetails(int packed_details_selection);
int Menu_Container_UpdateShieldDetails(int packed_details_selection);
int Menu_Container_UpdateArmorDetails(int packed_details_selection);
int Menu_Container_UpdateGemDetails(int packed_details_selection);
int Menu_Container_UpdateItemDetails(int packed_category_selection);
void Menu_Container_DrawCapacityCount(int capacity_limit, int packed_count_position, int occupied_count);
void Menu_Container_DrawStorageCapacities(void);
int Menu_Container_BuildItemRows(int itemCategory, char** text, int* rowTypes, char* textBuf);
void Menu_Container_RestoreItemRows(int category_id);
int Menu_Container_UpdateTransferConfirmation(int initialize_confirmation);
int Menu_Container_UpdateItemList(int transfer_direction_selection);
int Menu_Container_UpdateTransferDirection(int initialize_direction_menu);
int Menu_Container_UpdateScreen(u_char* screen_step);
void Menu_Container_ItemView_ResetMenudStatics(void);

#endif // VS_MENU_CONTAINER_ITEMVIEW_H_DECLS
