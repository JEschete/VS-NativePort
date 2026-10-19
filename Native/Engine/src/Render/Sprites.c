// Hand-maintained (tools/port/owned.csv).
#include "Render/Sprites.h"

#include "Core/Gte.h"
#include "Legacy/Psx.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// Packet: tag (length 5), draw mode, colour + code, xy, uv/clut, wh. Code 0x64
// is a textured sprite, 0x66 the semi-transparent one (bit 8 of
// brightnessTransparency). The packet is linked in front of nextPrim's chain.
static u_long* QueueSpritePacket(u_int draw_mode, int brightness_transparency,
                                 int xy, int wh, u_long* next_prim) {
  u_long* prim = (u_long*)Render_frameScratch.primitiveCursor;
  u_int brightness = (u_int)brightness_transparency & 0xFF;
  u_int code = 100 + (((u_int)brightness_transparency >> 7) & 2);

  prim[0] = (*next_prim & 0xFFFFFF) | 0x05000000;
  prim[1] = draw_mode;
  prim[2] = brightness | (brightness << 8) | (brightness << 16) | (code << 24);
  prim[3] = (u_long)xy;
  prim[4] = 0;
  prim[5] = (u_long)wh;
  *next_prim = LegacyGpu_Handle(prim);
  Render_frameScratch.primitiveCursor = prim + 6;
  return prim;
}

// decomp: BATTLE.PRG vs_battle_setSpriteDefault
// (src/BATTLE/BATTLE.PRG/573B8.c:270)
u_long* Render_QueueDefaultSprite(int wh, int xy) {
  return QueueSpritePacket(0xE100000C, 0x80, xy, wh,
                           (u_long*)Render_frameScratch.overlayOrderingTable);
}

// decomp: BATTLE.PRG vs_battle_setSpriteDefaultTexPage
// (src/BATTLE/BATTLE.PRG/573B8.c:275)
u_long* Render_QueueSpriteWithTexturePage(int brightness_transparency, int xy,
                                          int wh, u_long* next_prim) {
  return QueueSpritePacket(0xE100000C, brightness_transparency, xy, wh,
                           next_prim);
}

// decomp: BATTLE.PRG vs_battle_setSprite (src/BATTLE/BATTLE.PRG/573B8.c:281)
u_long* Render_QueueSprite(int brightness_transparency, int xy, int wh,
                           u_long* next_prim) {
  return QueueSpritePacket(0, brightness_transparency, xy, wh, next_prim);
}

// decomp: BATTLE.PRG func_800C02A8 (src/BATTLE/BATTLE.PRG/573B8.c:315)
void Render_LoadFrameViewMatrix(void) {
  Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
  Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
}
