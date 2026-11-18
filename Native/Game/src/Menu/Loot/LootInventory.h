// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Menu_Loot_LootListItem;

// decomp: lootItemState (src/MENU/MENUB.PRG/260.c:15)
enum Menu_Loot_LootItemState {
    Menu_Loot_LootItemState_None,
    Menu_Loot_LootItemState_Copied,
    Menu_Loot_LootItemState_NoIndent,
    Menu_Loot_LootItemState_Indent1,
    Menu_Loot_LootItemState_Indent2,
    Menu_Loot_LootItemState_Indent3
};

// decomp: _lootListItem (src/MENU/MENUB.PRG/260.c:29)
struct Menu_Loot_LootListItem {
    char itemCategory;
    u_char itemIndex;
    char opaque2;
    char state;
};
static_assert(sizeof(Menu_Loot_LootListItem) == 0x4, "Menu_Loot_LootListItem must keep its PS1 layout");
