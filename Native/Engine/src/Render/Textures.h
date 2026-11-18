// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Render_TextureSlot;

// decomp: vs_battle_texInfo_t (src/BATTLE/BATTLE.PRG/30DB0.h:50)
struct Render_TextureSlot {
    short texturePage;
    short palette;
    short alternatePalette;
    u_char referenceCount;
};
static_assert(sizeof(Render_TextureSlot) == 0x8, "Render_TextureSlot must keep its PS1 layout");

extern Render_TextureSlot Render_legacyActorTextureSlots[25];
extern Render_TextureSlot Render_legacyWeaponTextureSlots[20];
