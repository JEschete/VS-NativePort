// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_TEXTURES_H_TYPES
#define VS_RENDER_TEXTURES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_TextureSlot Render_TextureSlot;

// decomp: vs_battle_texInfo_t (src/BATTLE/BATTLE.PRG/30DB0.h:50)
typedef struct Render_TextureSlot {
    short texturePage;
    short palette;
    short alternatePalette;
    u_char referenceCount;
} Render_TextureSlot;
_Static_assert(sizeof(Render_TextureSlot) == 0x8, "Render_TextureSlot must keep its PS1 layout");

#endif // VS_RENDER_TEXTURES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_TEXTURES_H_DECLS)
#define VS_RENDER_TEXTURES_H_DECLS

extern Render_TextureSlot Render_legacyActorTextureSlots[25];
extern Render_TextureSlot Render_legacyWeaponTextureSlots[20];

#endif // VS_RENDER_TEXTURES_H_DECLS
