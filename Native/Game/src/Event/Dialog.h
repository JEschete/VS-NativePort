// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Event_DialogPlacement;

#include "Battle/TextBoxes.h"

// decomp: textPlacement (src/BATTLE/BATTLE.PRG/4A0A8.c:196)
struct Event_DialogPlacement {
    Battle_TextAnchorOffset anchorOffset;
    short screenX;
    short screenY;
    int clampDistance;
};
static_assert(sizeof(Event_DialogPlacement) == 0xC, "Event_DialogPlacement must keep its PS1 layout");
