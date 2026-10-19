// Hand-maintained (tools/port/owned.csv).
#include "Render/Sprites.h"

#include <limits.h>

#include "Core/Gte.h"
#include "Legacy/Psx.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

enum {
  kSpriteCommand = 0x64,
  kSemiTransparentFlag = 2,
  kTransparencyShift = 7,
  kDefaultBrightness = 0x80,
  kCommandShift = 24,
  kLinkMask = 0xFFFFFF
};

static const u_int kDefaultDrawMode = 0xE100000Cu;

enum {
  kPacketTag,
  kPacketDrawMode,
  kPacketColor,
  kPacketPosition,
  kPacketTexture,
  kPacketSize,
  kPacketWords
};

typedef struct SpriteRequest {
  u_int draw_mode;
  int brightness_transparency;
  int packed_position;
  int packed_size;
} SpriteRequest;

// Packet: tag (length 5), draw mode, colour + code, xy, uv/clut, wh. Code 0x64
// is a textured sprite, 0x66 the semi-transparent one (bit 8 of
// brightnessTransparency). The packet is linked in front of nextPrim's chain.
static u_long* QueueSpritePacket(const SpriteRequest* request,
                                 u_long* next_prim) {
  u_long* prim = (u_long*)Render_frameScratch.primitiveCursor;
  const u_int brightness = (u_int)request->brightness_transparency & UCHAR_MAX;
  const u_int code =
      kSpriteCommand +
      (((u_int)request->brightness_transparency >> kTransparencyShift) &
       kSemiTransparentFlag);

  prim[kPacketTag] =
      (*next_prim & kLinkMask) | ((kPacketWords - 1) << kCommandShift);
  prim[kPacketDrawMode] = request->draw_mode;
  prim[kPacketColor] = brightness | (brightness << CHAR_BIT) |
                       (brightness << (2 * CHAR_BIT)) | (code << kCommandShift);
  prim[kPacketPosition] = (u_long)request->packed_position;
  prim[kPacketTexture] = 0;
  prim[kPacketSize] = (u_long)request->packed_size;
  *next_prim = LegacyGpu_Handle(prim);
  Render_frameScratch.primitiveCursor = prim + kPacketWords;
  return prim;
}

// decomp: BATTLE.PRG vs_battle_setSpriteDefault
// (src/BATTLE/BATTLE.PRG/573B8.c:270)
u_long* Render_QueueDefaultSprite(int wh, int xy) {
  const SpriteRequest request = {.draw_mode = kDefaultDrawMode,
                                 .brightness_transparency = kDefaultBrightness,
                                 .packed_position = xy,
                                 .packed_size = wh};
  return QueueSpritePacket(&request,
                           (u_long*)Render_frameScratch.overlayOrderingTable);
}

// decomp: BATTLE.PRG vs_battle_setSpriteDefaultTexPage
// (src/BATTLE/BATTLE.PRG/573B8.c:275)
u_long* Render_QueueSpriteWithTexturePage(int brightness_transparency, int xy,
                                          int wh, u_long* next_prim) {
  const SpriteRequest request = {
      .draw_mode = kDefaultDrawMode,
      .brightness_transparency = brightness_transparency,
      .packed_position = xy,
      .packed_size = wh};
  return QueueSpritePacket(&request, next_prim);
}

// decomp: BATTLE.PRG vs_battle_setSprite (src/BATTLE/BATTLE.PRG/573B8.c:281)
u_long* Render_QueueSprite(int brightness_transparency, int xy, int wh,
                           u_long* next_prim) {
  const SpriteRequest request = {
      .draw_mode = 0,
      .brightness_transparency = brightness_transparency,
      .packed_position = xy,
      .packed_size = wh};
  return QueueSpritePacket(&request, next_prim);
}

// decomp: BATTLE.PRG func_800C02A8 (src/BATTLE/BATTLE.PRG/573B8.c:315)
void Render_LoadFrameViewMatrix(void) {
  Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
  Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
}
