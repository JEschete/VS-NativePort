// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_BACKGROUND_H_TYPES
#define VS_FIELD_BACKGROUND_H_TYPES
#include "Legacy/Psx.h"

struct Field_BackgroundLayout;
struct Field_BackgroundTile;

// decomp: vs_battle_backgroundTile (src/BATTLE/BATTLE.PRG/2842C.h:9)
struct Field_BackgroundTile {
    RECT textureRect;
    CVECTOR cornerColors[4];
};
static_assert(sizeof(Field_BackgroundTile) == 0x18, "Field_BackgroundTile must keep its PS1 layout");

// decomp: vs_battle_backgroundLayout (src/BATTLE/BATTLE.PRG/2842C.h:16)
struct Field_BackgroundLayout {
    int width;
    int height;
    int reserved08;
    Field_BackgroundTile tiles[0];
};
static_assert(sizeof(Field_BackgroundLayout) == 0xC, "Field_BackgroundLayout must keep its PS1 layout");

#endif // VS_FIELD_BACKGROUND_H_TYPES
