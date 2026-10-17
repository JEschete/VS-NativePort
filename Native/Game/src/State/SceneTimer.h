// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_SCENETIMER_H_TYPES
#define VS_STATE_SCENETIMER_H_TYPES
#include "Legacy/Psx.h"

typedef struct State_SceneTimer State_SceneTimer;

// decomp: D_80050468_t (src/SLUS_010.40/main.h:422)
typedef struct State_SceneTimer {
    char mode;
    char pausedMode;
    u_short resetScope;
    signed char frames;
    signed char seconds;
    signed char minutes;
    signed char hours;
} State_SceneTimer;
_Static_assert(sizeof(State_SceneTimer) == 0x8, "State_SceneTimer must keep its PS1 layout");

#endif // VS_STATE_SCENETIMER_H_TYPES
