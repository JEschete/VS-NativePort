// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCREEN_H_TYPES
#define VS_EVENT_SCREEN_H_TYPES
#include "Legacy/Psx.h"

typedef struct Event_EasedFractionScratch Event_EasedFractionScratch;
typedef struct Event_TripleTween Event_TripleTween;

// decomp: func_800BDBB4_t (src/BATTLE/BATTLE.PRG/4A0A8.c:98)
typedef struct Event_TripleTween {
    char reserved00;
    char easingMode;
    short currentValue0;
    short currentValue1;
    short currentValue2;
    short reserved08;
    short startValue0;
    short startValue1;
    short startValue2;
    short reserved10;
    short deltaValue0;
    short deltaValue1;
    short deltaValue2;
    short reserved18;
    short reserved1A;
    short elapsedFrames;
    short durationFrames;
} Event_TripleTween;
_Static_assert(sizeof(Event_TripleTween) == 0x20, "Event_TripleTween must keep its PS1 layout");

// decomp: func_800BDBB4_t2 (src/BATTLE/BATTLE.PRG/4A0A8.c:176)
typedef struct Event_EasedFractionScratch {
    int easedFraction;
} Event_EasedFractionScratch;
_Static_assert(sizeof(Event_EasedFractionScratch) == 0x4, "Event_EasedFractionScratch must keep its PS1 layout");

#endif // VS_EVENT_SCREEN_H_TYPES
