// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Field_TriggerMarkerVertex;

// decomp: D_800E8594_t (src/BATTLE/BATTLE.PRG/146C.c:197)
struct Field_TriggerMarkerVertex {
    short positionX;
    short height;
    short positionZ;
    short reserved06;
    int reserved08;
    char vertexShades[3][4];
    int reserved18;
    int reserved1C;
    int reserved20;
    char lastVertexShade[4];
};
static_assert(sizeof(Field_TriggerMarkerVertex) == 0x28, "Field_TriggerMarkerVertex must keep its PS1 layout");

extern RECT Field_triggerMarkerTextureSource;
extern RECT Field_triggerMarkerPaletteSource;
