// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Menu/Inventory/Inventory.h"

// decomp: _ashleyRenderState (_ashleyRenderState@src/MENU/items1.h)
int Menu_Inventory_ashleyRenderState = 0;

// decomp: _ashleyYOffset (_ashleyYOffset@src/MENU/items1.h)
int Menu_Inventory_ashleyYOffset = 0;

// decomp: _exitToBattle (_exitToBattle@src/MENU/items1.h)
int Menu_Inventory_exitToBattle;

// decomp: _itemCategoryIconsAnimationStep (_itemCategoryIconsAnimationStep@src/MENU/items1.h)
char Menu_Inventory_itemCategoryIconsAnimationStep;

// decomp: _itemCategoryIconsEnabled (_itemCategoryIconsEnabled@src/MENU/_toggleUi.h)
char Menu_Inventory_itemCategoryIconsEnabled;

// decomp: _secondaryTitleAnimationStep (_secondaryTitleAnimationStep@src/MENU/items1.h)
char Menu_Inventory_secondaryTitleAnimationStep;

// decomp: _secondaryTitleEnabled (_secondaryTitleEnabled@src/MENU/_toggleUi.h)
char Menu_Inventory_secondaryTitleEnabled;

// decomp: _selectedItemCategoryIconOnTop (_selectedItemCategoryIconOnTop@src/MENU/items1.h)
char Menu_Inventory_selectedItemCategoryIconOnTop;

// decomp: _statusCommandAnimationStep (_statusCommandAnimationStep@src/MENU/items1.h)
char Menu_Inventory_statusCommandAnimationStep;

// decomp: _statusCommandState (_statusCommandState@src/MENU/_toggleUi.h)
char Menu_Inventory_statusCommandState;

// decomp: vs_battle_loadObjectDataState (vs_battle_loadObjectDataState@BATTLE/BATTLE.PRG)
char Menu_Inventory_equipmentModelLoadStep; /* contents: module image (ModuleData.cpp) */

// decomp: vs_mainMenu_miscItemToActionMap (vs_mainMenu_miscItemToActionMap@MENU/MAINMENU.PRG)
u_char Menu_Inventory_miscItemToActionMap[] = { 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
    0xE7, 0xF3, 0xF4, 0xF5, 0xF6, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEF, 0xEE, 0xF0,
    0xF2, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xF1, 0x00,
    0xFE, 0xFD, 0xFB, 0xF4, 0xFE, 0xFB, 0xF6, 0xF3, 0xF3, 0xFC, 0xFD, 0xFF, 0xFB, 0xF5,
    0xFE, 0xFD, 0xFF, 0xFD, 0xFD, 0xFF, 0xFD, 0xFE, 0xFE, 0xFF, 0xFE, 0xFC, 0xFF, 0xFD,
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x00, 0x00, 0x00, 0x6E,
    0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00, 0x7A, 0x00, 0x00,
    0x00, 0x7E, 0x00, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00, 0x5E, 0x5F, 0x60, 0x57, 0x58,
    0x5A, 0x5B, 0x4D, 0x3C, 0x36, 0x3D, 0x37, 0x3E, 0x38, 0x3F, 0x39, 0x4B, 0x4C, 0x52,
    0x53, 0x55, 0x4F, 0x50, 0x5C, 0x5D, 0x3A, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49 };

// decomp: _equippedItem (_equippedItem@src/MENU/MENU3.PRG/16C.c)
char Menu_Inventory_equippedItem;

// decomp: _itemsFirstRow (_itemsFirstRow@src/MENU/MENU3.PRG/16C.c)
char Menu_Inventory_itemsFirstRow;

// decomp: _itemsText (_itemsText@src/MENU/MENU3.PRG/16C.c)
char* Menu_Inventory_itemsText;

// decomp: _menuRowCount (_menuRowCount@src/MENU/MENU3.PRG/16C.c)
char Menu_Inventory_menuRowCount;

// decomp: _menuText (_menuText@src/MENU/items0.h)
u_short Menu_Inventory_menuText[472]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _miscActions (_miscActions@src/MENU/MENU3.PRG/16C.c)
char Menu_Inventory_unusedMiscActions[64];

// decomp: _selectedItemRow (_selectedItemRow@src/MENU/MENU3.PRG/16C.c)
int Menu_Inventory_selectedItemRow;

// decomp: state (state@src/MENU/MENU3.PRG/16C.c)
u_char Menu_Inventory_equipListStep;

// MENU3.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ResetMenu3Statics(void)
{
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Inventory_ashleyRenderState, &initial, sizeof Menu_Inventory_ashleyRenderState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Inventory_ashleyYOffset, &initial, sizeof Menu_Inventory_ashleyYOffset);
    }
}

// MENUB.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ResetMenubStatics(void)
{
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Inventory_ashleyRenderState, &initial, sizeof Menu_Inventory_ashleyRenderState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Inventory_ashleyYOffset, &initial, sizeof Menu_Inventory_ashleyYOffset);
    }
}

}  // extern "C"
