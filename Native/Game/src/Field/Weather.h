// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_WEATHER_H_TYPES
#define VS_FIELD_WEATHER_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_WeatherParticle Field_WeatherParticle;

// decomp: D_800F1DD4_t (src/BATTLE/BATTLE.PRG/146C.c:184)
typedef struct Field_WeatherParticle {
    SVECTOR position;
    short screenX;
    short screenY;
    char opaqueC;
    char swayPhase;
    char fallShift;
    char speed;
} Field_WeatherParticle;

#endif // VS_FIELD_WEATHER_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_WEATHER_H_DECLS)
#define VS_FIELD_WEATHER_H_DECLS

extern char Field_screenWeatherFade;

#endif // VS_FIELD_WEATHER_H_DECLS
