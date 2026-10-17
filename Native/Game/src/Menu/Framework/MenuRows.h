// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_MENUROWS_H_TYPES
#define VS_MENU_FRAMEWORK_MENUROWS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Framework_ItemText Menu_Framework_ItemText;

// decomp: _itemsText_t (src/MENU/MAINMENU.PRG/58EC.c:15)
typedef struct Menu_Framework_ItemText {
    u_short name[14];
    int menuRowFlags;
    u_short description[48];
} Menu_Framework_ItemText;

#endif // VS_MENU_FRAMEWORK_MENUROWS_H_TYPES
