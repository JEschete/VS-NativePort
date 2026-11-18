// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Event_SplashTween;

// decomp: D_800F4B30_t (src/BATTLE/BATTLE.PRG/4A0A8.c:156)
struct Event_SplashTween {
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
};
static_assert(sizeof(Event_SplashTween) == 0x14, "Event_SplashTween must keep its PS1 layout");
