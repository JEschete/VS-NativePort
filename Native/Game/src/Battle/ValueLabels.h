// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_VALUELABELS_H_TYPES
#define VS_BATTLE_VALUELABELS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_FloatingLabel Battle_FloatingLabel;
typedef struct Battle_StatusLabelLayout Battle_StatusLabelLayout;

// decomp: (unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2842C.c:1518:1) (src/BATTLE/BATTLE.PRG/2842C.c:1518)
struct Battle_StatusLabelLayout {
    u_char count;
    u_char labels[2];
    u_char spacing;
};
typedef struct Battle_StatusLabelLayout Battle_StatusLabelLayout;
_Static_assert(sizeof(Battle_StatusLabelLayout) == 0x4, "Battle_StatusLabelLayout must keep its PS1 layout");

// decomp: D_800F1BAC_t (src/BATTLE/BATTLE.PRG/146C.h:762)
typedef struct Battle_FloatingLabel {
    SVECTOR position;
    union {
        char bytes[2];
        u_short halfword;
    } key;
    u_char limb;
    u_char frame;
    u_short value;
    u_char colorFlags;
    u_char textLength;
    short scaleX;
    short scaleY;
    signed char text[4];
} Battle_FloatingLabel;

#endif // VS_BATTLE_VALUELABELS_H_TYPES
