// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_TEXTBOXES_H_TYPES
#define VS_BATTLE_TEXTBOXES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_TextAnchorOffset Battle_TextAnchorOffset;
typedef struct Battle_TextBoxFlags Battle_TextBoxFlags;

// decomp: textOffset (src/BATTLE/BATTLE.PRG/4A0A8.c:189)
typedef struct Battle_TextAnchorOffset {
    int x : 12;
    int y : 12;
    u_int alignment : 2;
    u_int opaque3Bit2 : 4;
} Battle_TextAnchorOffset;
_Static_assert(sizeof(Battle_TextAnchorOffset) == 0x4, "Battle_TextAnchorOffset must keep its PS1 layout");

// decomp: vs_battle_textBox_fields (src/BATTLE/BATTLE.PRG/5BF94.h:93)
typedef struct Battle_TextBoxFlags {
    u_int kind : 4;
    u_int openStyle : 2;
    u_int alignment : 2;
    u_int frameShape : 4;
    u_int framePart : 4;
    u_int arrowAnimFrame : 8;
    u_int speakerTexture : 6;
    u_int done : 1;
    u_int opaque3Bit7 : 1;
} Battle_TextBoxFlags;
_Static_assert(sizeof(Battle_TextBoxFlags) == 0x4, "Battle_TextBoxFlags must keep its PS1 layout");

#endif // VS_BATTLE_TEXTBOXES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_TEXTBOXES_H_DECLS)
#define VS_BATTLE_TEXTBOXES_H_DECLS

extern u_short Battle_doorStrings[190];

#endif // VS_BATTLE_TEXTBOXES_H_DECLS
