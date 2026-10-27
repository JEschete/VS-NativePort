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

extern u_char Render_legacyTexturePageReservations[40];
extern Render_TextureSlot Render_legacyActorTextureSlots[25];
extern Render_TextureSlot Render_legacyWeaponTextureSlots[20];
void Render_InitializeTextureSlotReservations(void);
void Render_AdvanceTexturePageRetirements(void);
int Render_ReserveTexturePageRange(u_int first_page, u_int page_count, int owner_tag);
int Render_RetireTexturePageRange(u_int first_page, u_int page_count, int owner_tag);
void Render_ParseTextureImage(u_long* image_cursor, TIM_IMAGE* tim);
DR_MOVE* Render_BeginTextureMovePacket(void);
void Render_EndTextureMovePacket(DR_MOVE* p);
void Render_QueueTextureRectangleMove(RECT* source_rectangle, int destination_x, int destination_y);

#endif // VS_RENDER_TEXTURES_H_DECLS
