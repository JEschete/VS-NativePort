// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_DIALOG_H_TYPES
#define VS_EVENT_DIALOG_H_TYPES
#include "Legacy/Psx.h"

struct Event_DialogPlacement;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_EVENT_DIALOG_H_SET_TYPES_ONLY
#endif
#include "Battle/TextBoxes.h"
#ifdef VS_EVENT_DIALOG_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_EVENT_DIALOG_H_SET_TYPES_ONLY
#endif

// decomp: textPlacement (src/BATTLE/BATTLE.PRG/4A0A8.c:196)
struct Event_DialogPlacement {
    Battle_TextAnchorOffset anchorOffset;
    short screenX;
    short screenY;
    int clampDistance;
};
static_assert(sizeof(Event_DialogPlacement) == 0xC, "Event_DialogPlacement must keep its PS1 layout");

#endif // VS_EVENT_DIALOG_H_TYPES
