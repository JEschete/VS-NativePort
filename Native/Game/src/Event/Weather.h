// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_WEATHER_H_TYPES
#define VS_EVENT_WEATHER_H_TYPES
#include "Legacy/Psx.h"

typedef struct Event_WeatherTween Event_WeatherTween;

// decomp: D_800F4B88_t (src/BATTLE/BATTLE.PRG/4A0A8.c:172)
typedef struct Event_WeatherTween {
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
} Event_WeatherTween;

#endif // VS_EVENT_WEATHER_H_TYPES
