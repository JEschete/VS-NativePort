// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_MANUAL_H_TYPES
#define VS_FIELD_MANUAL_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_ManualDisplayState Field_ManualDisplayState;

// decomp: vs_battle_manualDisplayState_t (src/BATTLE/BATTLE.PRG/146C.h:655)
typedef struct Field_ManualDisplayState {
    u_short currentManual;
    u_short reserved02;
    u_short scrollPositions[14];
    u_short reserved20;
    char reserved22;
    char reserved23;
} Field_ManualDisplayState;
_Static_assert(sizeof(Field_ManualDisplayState) == 0x24, "Field_ManualDisplayState must keep its PS1 layout");

#endif // VS_FIELD_MANUAL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_MANUAL_H_DECLS)
#define VS_FIELD_MANUAL_H_DECLS

extern Field_ManualDisplayState Field_manualDisplayState;

#endif // VS_FIELD_MANUAL_H_DECLS
