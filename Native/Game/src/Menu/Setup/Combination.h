// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_SETUP_COMBINATION_H_TYPES
#define VS_MENU_SETUP_COMBINATION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Setup_CombinedArmorRecord Menu_Setup_CombinedArmorRecord;
typedef struct Menu_Setup_SydHeader Menu_Setup_SydHeader;

// decomp: _armorInfo (src/MENU/MENUC.PRG/168.c:5000)
typedef struct Menu_Setup_CombinedArmorRecord {
    char reserved0;
    char wepId;
    char category;
    char gemSlots;
    char strength;
    char intelligence;
    char agility;
    char reserved7;
} Menu_Setup_CombinedArmorRecord;
_Static_assert(sizeof(Menu_Setup_CombinedArmorRecord) == 0x8, "Menu_Setup_CombinedArmorRecord must keep its PS1 layout");

// decomp: _syd (src/MENU/MENUC.PRG/168.c:4920)
typedef struct Menu_Setup_SydHeader {
    u_long combinationsOffset;
    u_long materialsOffset;
    u_long initDataOffset;
    char reservedC[0];
} Menu_Setup_SydHeader;
_Static_assert(sizeof(Menu_Setup_SydHeader) == 0xC, "Menu_Setup_SydHeader must keep its PS1 layout");

#endif // VS_MENU_SETUP_COMBINATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_SETUP_COMBINATION_H_DECLS)
#define VS_MENU_SETUP_COMBINATION_H_DECLS
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"

extern char Menu_Setup_layoutPadding3[2];;
extern char Menu_Setup_layoutPadding5[2];;
extern Items_InventoryArmor Menu_Setup_armorBuf;;
extern Items_InventoryBlade Menu_Setup_bladeBuf;;
extern void* Menu_Setup_bladeCombinationInitData;;
extern Menu_Setup_CombinedArmorRecord* Menu_Setup_combinationInitData;;
extern char* Menu_Setup_combinationResults;;
extern char Menu_Setup_combiningArmorIds[2];;
extern char Menu_Setup_combiningBladeIds[2];;
extern u_char Menu_Setup_currentWorkshop;;
extern u_char Menu_Setup_itemsToCombine[16];;
extern char* Menu_Setup_materialResults;;
extern Items_InventoryShield Menu_Setup_shieldBuf;;
extern Menu_Setup_CombinedArmorRecord* Menu_Setup_shieldCombinationInitData;;
extern void* Menu_Setup_sydData;
extern char Menu_Setup_workshopMaterials[];
int Menu_Setup_LoadCombinationRecipes(int id);
Menu_Framework_MenuRow* Menu_Setup_SetCombinationResultRow(int itemType, int index);
Menu_Framework_MenuRow* Menu_Setup_AnimateCombinationResult(int itemType, int index);
void Menu_Setup_PreviewBladeCombination(int preview_selection);
int Menu_Setup_SelectCombinationBlade(int ingredient_slot);
void Menu_Setup_SetCombinationBladeRow(
    Menu_Framework_MenuRow* menuItem, Items_InventoryBlade* blade);
int Menu_Setup_UpdateBladeCombination(int initialize_blade_combination);
void Menu_Setup_SetShieldBaseStats(Items_InventoryShield* shield);
void Menu_Setup_PreviewShieldCombination(int preview_selection);
void Menu_Setup_SetShieldRowWithoutGems(
    Items_InventoryShield* shield, char** menuText, int* rowTypes, char* stringBuf);
int Menu_Setup_SelectCombinationShield(int ingredient_slot);
void Menu_Setup_SetCombinationShieldRow(
    Menu_Framework_MenuRow* menuItem, Items_InventoryShield* shield);
int Menu_Setup_UpdateShieldCombination(int initialize_shield_combination);
void Menu_Setup_PreviewArmorCombination(int preview_selection);
int Menu_Setup_SelectCombinationArmor(int ingredient_slot);
void Menu_Setup_SetCombinationArmorRow(
    Menu_Framework_MenuRow* menuItem, Items_InventoryArmor* armor);
int Menu_Setup_UpdateArmorCombination(int initialize_armor_combination);
int Menu_Setup_CountCombinationIngredients(int category);
int Menu_Setup_UpdateCombinationCategory(int initialize_combination_category);
void Menu_Setup_CombineClassOrAffinityValues(signed char* first, signed char* second,
    signed char* result, int materialsDifferent, int setAffinities);
void Menu_Setup_CombinePhysicalTypeValues(
    signed char* first, signed char* second, signed char* result, int materialsDifferent);
Items_InventoryBlade* Menu_Setup_BuildCombinedBlade(Items_InventoryBlade* first,
    Items_InventoryBlade* second, Items_InventoryBlade* result, void* sydData);
Items_InventoryArmor* Menu_Setup_BuildCombinedShield(Items_InventoryArmor* first,
    Items_InventoryArmor* second, Items_InventoryArmor* result, void* sydData);
Items_InventoryArmor* Menu_Setup_BuildCombinedArmor(Items_InventoryArmor* first,
    Items_InventoryArmor* second, Items_InventoryArmor* result, void* sydData);
void Menu_Setup_Combination_ResetMenucStatics(void);

#endif // VS_MENU_SETUP_COMBINATION_H_DECLS
