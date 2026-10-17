// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_DIALOG_H_TYPES
#define VS_EVENT_DIALOG_H_TYPES
#include "Legacy/Psx.h"

typedef struct Event_DialogPlacement Event_DialogPlacement;

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
typedef struct Event_DialogPlacement {
    Battle_TextAnchorOffset anchorOffset;
    short screenX;
    short screenY;
    int clampDistance;
} Event_DialogPlacement;

#endif // VS_EVENT_DIALOG_H_TYPES
