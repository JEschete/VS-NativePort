// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Field_SavedPositionWords;

// decomp: D_800F4B0A (D_800F4B0A@BATTLE/BATTLE.PRG): a view into another global
#define Field_collisionProbeHeight (*(short*)((u_char*)&Field_collisionProbePosition + 2))

// decomp: func_800A6798_t (src/BATTLE/BATTLE.PRG/3A1A0.c:29)
struct Field_SavedPositionWords {
    int words[2];
};
static_assert(sizeof(Field_SavedPositionWords) == 0x8, "Field_SavedPositionWords must keep its PS1 layout");

extern u_char Field_kTileSurfaceIds[2744];
extern u_char Field_collisionProbeDirection;
extern short Field_platformContactSearchDepth;
extern SVECTOR Field_collisionProbePosition;
