// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Event_WeatherTween;

// decomp: D_800F4B88_t (src/BATTLE/BATTLE.PRG/4A0A8.c:172)
struct Event_WeatherTween {
    char reserved00;
    char reserved01;
    char elapsedFrames;
    char durationFrames;
    short windDirection;
    short startWindDirection;
    short deltaWindDirection;
    short windStrength;
    short startWindStrength;
    short deltaWindStrength;
    short intensity;
    short startIntensity;
    short deltaIntensity;
};
static_assert(sizeof(Event_WeatherTween) == 0x16, "Event_WeatherTween must keep its PS1 layout");
