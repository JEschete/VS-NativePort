// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Menu_Framework_ItemText;

// decomp: _itemsText_t (src/MENU/MAINMENU.PRG/58EC.c:15)
struct Menu_Framework_ItemText {
    u_short name[14];
    int menuRowFlags;
    u_short description[48];
};
static_assert(sizeof(Menu_Framework_ItemText) == 0x80, "Menu_Framework_ItemText must keep its PS1 layout");
