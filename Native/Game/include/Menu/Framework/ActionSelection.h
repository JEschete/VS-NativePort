// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_ACTIONSELECTION_H_TYPES
#define VS_MENU_FRAMEWORK_ACTIONSELECTION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Framework_ActionSelection Menu_Framework_ActionSelection;

// decomp: D_800F18EC_t (src/BATTLE/BATTLE.PRG/5BF94.h:160)
typedef struct Menu_Framework_ActionSelection {
    short menuStatus;
    short selectionKind;
    union {
        struct {
            u_int location : 9;
            u_int tileX : 5;
            u_int tileZ : 5;
            u_int mpCost : 13;
        } bits;
        u_short halves[2];
        int word;
    } executeAbility;
} Menu_Framework_ActionSelection;

#endif // VS_MENU_FRAMEWORK_ACTIONSELECTION_H_TYPES
