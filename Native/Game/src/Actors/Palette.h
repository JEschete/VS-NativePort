// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_PALETTE_H_TYPES
#define VS_ACTORS_PALETTE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_PalettePulseView Actors_PalettePulseView;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ACTORS_PALETTE_H_SET_TYPES_ONLY
#endif
#include "Actors/ModelInstance.h"
#ifdef VS_ACTORS_PALETTE_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ACTORS_PALETTE_H_SET_TYPES_ONLY
#endif

// decomp: func_800AB788_t (src/BATTLE/BATTLE.PRG/40564.c:42)
typedef struct Actors_PalettePulseView {
    u_char reserved0[0xF];
    u_char slot;
    u_char reserved10[0x54];
    Actors_ModelEffectSlot pulse;
    u_char reserved68[0xC4];
    u_short basePalette[16];
    u_short pulsedPalette[16];
} Actors_PalettePulseView;

#endif // VS_ACTORS_PALETTE_H_TYPES
