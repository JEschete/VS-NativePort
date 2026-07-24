/* Native C stand-ins for the hand-written inline-MIPS functions in BATTLE.PRG.
 *
 * On PS1 these are defined as file-scope __asm__("glabel …") blocks inside
 * their translation units, which the x86 assembler cannot process. Those blocks
 * are guarded with #ifndef NATIVE_BUILD in the original .c files; the native
 * build gets these C equivalents instead, which unblocks the ~450 already-
 * matched functions that share those translation units.
 *
 * Two tiers:
 *   - memcpy-family and sfx dispatchers: faithful C, behaviour preserved.
 *   - sprite/glyph builders: memory-SAFE stubs. They bump the scratchpad
 *     primitive pointer and return a zeroed (therefore non-drawing) primitive,
 *     so callers that add the result to an ordering table are safe. Real
 *     pixel output is deferred until the render path actually runs and can be
 *     verified against DuckStation. Each is marked TODO(render).
 */
#include <psx_scratchpad.h>
#include <string.h>

typedef unsigned long u_long;
typedef unsigned int  u_int;

/* ---- memory copies (573B8) ---- */

void vs_battle_memcpy(void* dst, const void* src, int numBytes)
{
    if (numBytes > 0) memcpy(dst, src, (size_t)numBytes);
}

/* The asm aligns to an 8-byte boundary by copying halfwords, then falls through
 * to vs_battle_memcpy. The observable effect is a byte copy. */
void vs_battle_copyAligned(void* dst, const void* src, int numBytes)
{
    if (numBytes > 0) memcpy(dst, src, (size_t)numBytes);
}

/* ---- sfx dispatchers (573B8): each selects an id and calls the default
 * player. Audio itself is Phase 3; vs_main_playSfxDefault is currently a stub,
 * so these correctly surface the audio dependency when a sfx actually fires. */
extern void vs_main_playSfxDefault(int a, int b);

/* Audio is Phase 3 and the SPU voice state is not initialised in the menu
 * harness, so these are no-ops for now (calling vs_main_playSfxDefault faults
 * in the uninitialised SPU path). Restore the real dispatch once audio lands. */
void vs_battle_playMenuChangeSfx(void) { (void)0; }
void vs_battle_playMenuSelectSfx(void) { (void)0; }
void vs_battle_playMenuLeaveSfx(void)  { (void)0; }
void vs_battle_playInvalidSfx(void)    { (void)0; }
void vs_battle_playSfx10(void)         { (void)0; }

/* ---- sprite builders (573B8) ----
 * Faithful C translation of the hand-written asm (matchings/573B8/*.s). Each
 * builds a len-5 "compound" primitive in the scratchpad primitive buffer:
 *   word0: tag  = link | 0x05000000            (len 5)
 *   word1: tpageCmd (0 / 0xE100000C)           (GP0 texpage command)
 *   word2: rgb | code<<24, code = 0x64|0x66    (SPRT, semi-trans from bit 7)
 *   word3: xy
 *   word4: 0        (caller overwrites with u/v + CLUT)
 *   word5: wh
 * and threads it onto the OT slot *nextPrim. D_1F800000[0] is the bump pointer.
 * NOTE(render): these primitives are textured font/UI sprites; the native
 * rasteriser does not yet texture them (needs the font in VRAM + CLUT SPRT
 * raster), so they build correctly but do not yet produce pixels. */
extern u_long* D_1F800000[];

static u_long* setSprite_impl(int brightnessTransparency, int xy, int wh,
                              u_long* nextPrim, u_long tpageCmd)
{
    u_long* prim = D_1F800000[0];
    int b = brightnessTransparency & 0xFF;
    u_long rgb = (u_long)b | (b << 8) | (b << 16);
    u_long code = (((u_int)brightnessTransparency >> 7) & 2) + 0x64;

    prim[0] = (*nextPrim & 0xFFFFFF) | 0x05000000;
    prim[1] = tpageCmd;
    prim[2] = rgb | (code << 24);
    prim[3] = (u_long)xy;
    prim[4] = 0;
    prim[5] = (u_long)wh;

    *nextPrim = (u_long)prim & 0xFFFFFF;
    D_1F800000[0] = (u_long*)((char*)prim + 0x18);
    return prim;
}

u_long* vs_battle_setSprite(int brightnessTransparency, int xy, int wh, u_long* nextPrim)
{
    return setSprite_impl(brightnessTransparency, xy, wh, nextPrim, 0);
}

/* Presets the texpage command to 0xE100000C (x offset 768). */
u_long* vs_battle_setSpriteDefaultTexPage(int brightnessTransparency, int xy, int wh,
                                          u_long* nextPrim)
{
    return setSprite_impl(brightnessTransparency, xy, wh, nextPrim, 0xE100000C);
}

/* Brightness fixed at 0x80, transparency off, chained after D_1F800000[2]. */
u_long* vs_battle_setSpriteDefault(int wh, int xy)
{
    return setSprite_impl(0x80, xy, wh, D_1F800000[2], 0);
}

/* ---- fixed-width font glyph (5BF94) ----
 * TODO(render): the original builds a textured sprite for one glyph and returns
 * the pen x-advance. This stub draws nothing and returns a plausible advance so
 * text layout still progresses. width*scale>>12 mirrors the .12 fixed-point
 * scale the original applies. */
int _printFixedWidthFontChar(int charId, int x, int y, int width, int height, int scale)
{
    (void)charId; (void)y; (void)height;
    return x + ((width * (scale ? scale : 0x1000)) >> 12);
}
