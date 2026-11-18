// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Field_WeatherParticle;

// decomp: D_800F1DD4_t (src/BATTLE/BATTLE.PRG/146C.c:184)
struct Field_WeatherParticle {
    SVECTOR position;
    short screenX;
    short screenY;
    char opaqueC;
    char swayPhase;
    char fallShift;
    char speed;
};
static_assert(sizeof(Field_WeatherParticle) == 0x10, "Field_WeatherParticle must keep its PS1 layout");

extern char Field_screenWeatherFade;
