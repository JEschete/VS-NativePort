// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/FixedPoint.h"
#include "Render/Font.h"
#include "Render/Frame.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/TextureAnimation.h"
#include "Render/UiPrimitives.h"
#include "Render/Upload.h"
#include "Save/MemoryCard/GameFiles.h"

// decomp: D_800EBC04 (D_800EBC04@BATTLE/BATTLE.PRG)
int Render_kStatGaugeColors[4]; /* data: contents loaded in Step 5 */

// decomp: BATTLE.PRG func_8007A824 (src/BATTLE/BATTLE.PRG/146C.c:8055)
void Render_AppendOverlayPrimitive(int* arg0) { Render_SplicePrimitiveList(Render_legacyCurrentOrderingTable + 0x2084, arg0); }

// decomp: BATTLE.PRG vs_battle_setSpriteDefault (src/BATTLE/BATTLE.PRG/573B8.c:270) [inline-asm: not yet ported]
u_long* Render_QueueDefaultSprite(int wh, int xy)
{
    VS_UNPORTED("Render::QueueDefaultSprite");
    return 0;
}

// decomp: BATTLE.PRG vs_battle_setSpriteDefaultTexPage (src/BATTLE/BATTLE.PRG/573B8.c:275) [inline-asm-entry: not yet ported]
u_long* Render_QueueSpriteWithTexturePage(
    int brightnessTransparency, int xy, int wh, u_long* nextPrim)
{
    VS_UNPORTED("Render::QueueSpriteWithTexturePage");
    return 0;
}

// decomp: BATTLE.PRG vs_battle_setSprite (src/BATTLE/BATTLE.PRG/573B8.c:281) [inline-asm: not yet ported]
u_long* Render_QueueSprite(int brightnessTransparency, int xy, int wh, u_long* nextPrim)
{
    VS_UNPORTED("Render::QueueSprite");
    return 0;
}

// decomp: BATTLE.PRG func_800C02A8 (src/BATTLE/BATTLE.PRG/573B8.c:315) [inline-asm: not yet ported]
void Render_LoadFrameViewMatrix(void)
{
    VS_UNPORTED("Render::LoadFrameViewMatrix");
}

// decomp: BATTLE.PRG func_800C64D0 (src/BATTLE/BATTLE.PRG/5BF94.c:1332)
void Render_SplicePrimitiveList(u_long* arg0, int* arg1)
{
    int a3 = 0xFFFFFF;
    u_long temp_t0 = LegacyGpu_Handle(arg0 - 1);

    while ((*arg0 & a3) != temp_t0) {
        arg0 = (u_long*)LegacyGpu_Pointer(*arg0 & a3);
    }

    *arg0 = (*arg0 & 0xFF000000) | LegacyGpu_Handle(arg1);
    *arg1 = (int)((*arg1 & 0xFF000000) | temp_t0);
}

// decomp: BATTLE.PRG _lerpColor (src/BATTLE/BATTLE.PRG/5BF94.c:2704)
u_int Render_BlendRgbWithBrightness(u_int color0, u_int color1, int brightness, int blendFactor)
{
    u_int i;
    u_int ret = 0;

    for (i = 0; i < 3; ++i) {
        u_int temp_lo = ((((color0 >> 0x10) & 0xFF) * (8 - blendFactor))
                            + (((color1 >> 0x10) & 0xFF) * blendFactor))
                      * brightness;
        color0 <<= 8;
        ret = (ret << 8) + (temp_lo >> 0xA);
        color1 <<= 8;
    }

    return ret;
}

// decomp: BATTLE.PRG _renderDigit (src/BATTLE/BATTLE.PRG/5BF94.c:3021)
void Render_DrawNumericGlyph(int font, int xy, int digit, u_long* before)
{
    u_long* prim = Render_QueueSpriteWithTexturePage(
        128, xy + ((font & 2) << 0x10), (0x05CA0576 >> (font * 4)) & 0xF000F, before);

    prim[4] = vs_getUV0Clut(32, 11, 832, 223);

    if (font != 2) {
        ((short*)prim)[8] = ((((digit * 2) + 64) * (font + 3)) - 192);
    }
}

// decomp: BATTLE.PRG vs_battle_renderValue (src/BATTLE/BATTLE.PRG/5BF94.c:3033)
int Render_DrawIntegerRightAligned(int font, int xy, int value, u_long* before)
{
    int new_var;

    if (before == NULL) {
        before = Render_frameScratch.orderingTable - 0x10;
    }

    do {
        value = Core_Math_SplitDecimalDigit(value);

        Render_DrawNumericGlyph(font, xy, value & 0xF, before);

        new_var = xy - 5;
        xy = new_var - font;
        value >>= 4;
    } while (value != 0);

    return xy;
}

// decomp: BATTLE.PRG vs_battle_renderStatBar (src/BATTLE/BATTLE.PRG/5BF94.c:3054)
void Render_DrawStatGauge(int colorIndex, int w, u_long* before, int xy)
{
    int rgb0;
    u_int rgb1;
    u_long* primBuf;

    rgb1 = Render_kStatGaugeColors[colorIndex];
    primBuf = Render_frameScratch.primitiveCursor;

    rgb0 = (((w * 0xFF) + ((rgb1 & 0xFF) * (0x40 - w))) >> 6)
         | ((((w * 0xF0) + (((rgb1 >> 8) & 0xFF) * (0x40 - w))) >> 6) << 8)
         | ((((w * 0x9E) + ((rgb1 >> 16) * (0x40 - w))) >> 6) << 16);

    primBuf[0] = (*before & 0xFFFFFF) | (w == 0 ? 0x03000000 : 0x0D000000);
    primBuf[1] = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0x00, 0x28, 0x40);
    primBuf[2] = xy;
    primBuf[3] = 0x50042;
    primBuf[4] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_On);
    primBuf[5] = rgb0 | (Render_LegacyPrimitiveCode_PolyG4 << 24);
    xy += vs_getXY(1, 1);
    primBuf[6] = xy;
    primBuf[7] = rgb1;
    primBuf[8] = xy + w;
    xy += vs_getXY(0, 3);
    primBuf[9] = rgb0;
    primBuf[10] = xy;
    primBuf[11] = rgb1;
    rgb1 = w;
    primBuf[12] = xy + rgb1;
    primBuf[13] = vs_getTpage(0, 0, 0, 0, 0);
    *before = ((u_long)primBuf << 8) >> 8;
    Render_frameScratch.primitiveCursor = primBuf + 14;
}

// decomp: BATTLE.PRG vs_battle_addTile (src/BATTLE/BATTLE.PRG/5BF94.c:4503)
void Render_InsertThreeWordPrimitive(u_long* before, int rgb0, int xy, int wh)
{
    u_long* prim = Render_frameScratch.primitiveCursor;
    prim[0] = (int)((*before & 0xFFFFFF) | 0x03000000);
    prim[1] = rgb0;
    prim[2] = xy;
    prim[3] = wh;
    *before = ((u_long)prim << 8) >> 8;
    Render_frameScratch.primitiveCursor = prim + 4;
}

// decomp: BATTLE.PRG vs_battle_insertTpage (src/BATTLE/BATTLE.PRG/5BF94.c:4514)
void Render_InsertDrawModeCommand(int tpage, u_long* before)
{
    u_long* prim = Render_frameScratch.primitiveCursor;
    prim[0] = vs_getTag(sizeof(int), *before);
    prim[1] = tpage;
    *before = ((u_long)prim << 8) >> 8;
    Render_frameScratch.primitiveCursor = prim + 2;
}

// decomp: BATTLE.PRG vs_battle_renderImage (src/BATTLE/BATTLE.PRG/5BF94.c:4531)
void Render_UploadImageRectangle(int xy, void* buffer, int wh)
{
    extern int Render_legacyUploadRectCursor;
    extern RECT Render_legacyUploadRects[];
    RECT* rect;

    rect = Render_legacyUploadRectCursor + Render_legacyUploadRects;
    Render_legacyUploadRectCursor = (Render_legacyUploadRectCursor + 1) & 7;
    *(int*)&rect->x = xy;
    *(int*)&rect->w = wh;
    LoadImage(rect, buffer);
}

