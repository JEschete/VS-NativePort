// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Actors_PalettePulseView;

#include "Actors/ModelInstance.h"

// decomp: func_800AB788_t (src/BATTLE/BATTLE.PRG/40564.c:42)
struct Actors_PalettePulseView {
    u_char reserved0[0xF];
    u_char slot;
    u_char reserved10[0x54];
    Actors_ModelEffectSlot pulse;
    u_char reserved68[0xC4];
    u_short basePalette[16];
    u_short pulsedPalette[16];
};
static_assert(sizeof(Actors_PalettePulseView) == 0x16C, "Actors_PalettePulseView must keep its PS1 layout");
