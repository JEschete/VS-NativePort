// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_PALETTE_H_TYPES
#define VS_RENDER_PALETTE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_ColorCode Render_ColorCode;

// decomp: D_800F4538_unk58_2 (src/BATTLE/BATTLE.PRG/3A1A0.h:38)
typedef struct Render_ColorCode {
    u_char red;
    u_char green;
    u_char blue;
    u_char code;
} Render_ColorCode;
_Static_assert(sizeof(Render_ColorCode) == 0x4, "Render_ColorCode must keep its PS1 layout");

#endif // VS_RENDER_PALETTE_H_TYPES
