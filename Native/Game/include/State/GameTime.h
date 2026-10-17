// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_GAMETIME_H_TYPES
#define VS_STATE_GAMETIME_H_TYPES
#include "Legacy/Psx.h"

typedef union State_PlayTime State_PlayTime;

// decomp: vs_Gametime_t (src/SLUS_010.40/main.h:173)
typedef union State_PlayTime {
    struct {
        signed char frames;
        signed char seconds;
        signed char minutes;
        signed char hours;
    } t;
    u_int all;
} State_PlayTime;

#endif // VS_STATE_GAMETIME_H_TYPES
