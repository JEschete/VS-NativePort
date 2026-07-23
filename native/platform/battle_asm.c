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

void vs_battle_playMenuChangeSfx(void) { vs_main_playSfxDefault(0x7E, 4); }
void vs_battle_playMenuSelectSfx(void) { vs_main_playSfxDefault(0x7E, 5); }
void vs_battle_playMenuLeaveSfx(void)  { vs_main_playSfxDefault(0x7E, 6); }
void vs_battle_playInvalidSfx(void)    { vs_main_playSfxDefault(0x7E, 7); }
void vs_battle_playSfx10(void)         { vs_main_playSfxDefault(0x7E, 10); }

/* ---- sprite builders (573B8) ----
 * TODO(render): the originals build a textured sprite primitive (with an
 * E100000C tpage command) in the scratchpad primitive buffer and thread it
 * onto a tpage list. These stubs bump-allocate a zeroed 0x18-byte primitive so
 * the caller gets a valid, non-drawing pointer. */
static u_long* sprite_alloc(void)
{
    unsigned long* scratch = _psx_scratchpad();
    unsigned long* prim = (unsigned long*)scratch[0];
    if (!prim) return 0;
    memset(prim, 0, 0x18);
    scratch[0] = (unsigned long)((char*)prim + 0x18);
    return (u_long*)prim;
}

u_long* vs_battle_setSprite(int brightnessTransparency, int xy, int wh, int uv)
{
    (void)brightnessTransparency; (void)xy; (void)wh; (void)uv;
    return sprite_alloc();
}

u_long* vs_battle_setSpriteDefault(int wh, int xy)
{
    (void)wh; (void)xy;
    return sprite_alloc();
}

u_long* vs_battle_setSpriteDefaultTexPage(int wh, int xy, int texpage)
{
    (void)wh; (void)xy; (void)texpage;
    return sprite_alloc();
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
