// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SPLASH_H_TYPES
#define VS_EVENT_SPLASH_H_TYPES
#include "Legacy/Psx.h"

typedef struct Event_SplashTween Event_SplashTween;

// decomp: D_800F4B30_t (src/BATTLE/BATTLE.PRG/4A0A8.c:156)
typedef struct Event_SplashTween {
    char reserved00;
    char easingMode;
    char elapsedFrames;
    char durationFrames;
    short startX;
    short startY;
    short deltaX;
    short deltaY;
    short startScaleFixed12;
    short deltaScaleFixed12;
    short startBrightness;
    short deltaBrightness;
} Event_SplashTween;
_Static_assert(sizeof(Event_SplashTween) == 0x14, "Event_SplashTween must keep its PS1 layout");

#endif // VS_EVENT_SPLASH_H_TYPES
