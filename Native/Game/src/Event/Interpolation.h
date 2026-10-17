// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_INTERPOLATION_H_TYPES
#define VS_EVENT_INTERPOLATION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Event_InterpolationCurve Event_InterpolationCurve;

// decomp: D_800F4B28_t (src/BATTLE/BATTLE.PRG/573B8.c:13)
typedef struct Event_InterpolationCurve {
    u_char curveFamily;
    signed char power;
} Event_InterpolationCurve;
_Static_assert(sizeof(Event_InterpolationCurve) == 0x2, "Event_InterpolationCurve must keep its PS1 layout");

#endif // VS_EVENT_INTERPOLATION_H_TYPES
