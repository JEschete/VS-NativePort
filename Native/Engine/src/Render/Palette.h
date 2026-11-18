// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Render_ColorCode;

// decomp: D_800F4538_unk58_2 (src/BATTLE/BATTLE.PRG/3A1A0.h:38)
struct Render_ColorCode {
    u_char red;
    u_char green;
    u_char blue;
    u_char code;
};
static_assert(sizeof(Render_ColorCode) == 0x4, "Render_ColorCode must keep its PS1 layout");
