// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Battle_TextAnchorOffset;
struct Battle_TextBoxFlags;

// decomp: textOffset (src/BATTLE/BATTLE.PRG/4A0A8.c:189)
struct Battle_TextAnchorOffset {
    int x : 12;
    int y : 12;
    u_int alignment : 2;
    u_int opaque3Bit2 : 4;
};
static_assert(sizeof(Battle_TextAnchorOffset) == 0x4, "Battle_TextAnchorOffset must keep its PS1 layout");

// decomp: vs_battle_textBox_fields (src/BATTLE/BATTLE.PRG/5BF94.h:93)
struct Battle_TextBoxFlags {
    u_int kind : 4;
    u_int openStyle : 2;
    u_int alignment : 2;
    u_int frameShape : 4;
    u_int framePart : 4;
    u_int arrowAnimFrame : 8;
    u_int speakerTexture : 6;
    u_int done : 1;
    u_int opaque3Bit7 : 1;
};
static_assert(sizeof(Battle_TextBoxFlags) == 0x4, "Battle_TextBoxFlags must keep its PS1 layout");

extern u_short Battle_doorStrings[190];
