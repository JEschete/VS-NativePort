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

/* Five one-line helpers that the PSX build writes as hand-rolled assembly
 * (573B8.c, a shared tail that loads $a1 and falls into a single call). They
 * were no-ops while audio was Phase 3, because reaching the uninitialised SPU
 * path faulted; the driver works now, so they do the real thing.
 *
 * The ids are the ones the assembly loads: VS_SFX_MENUCHANGE 4, MENUSELECT 5,
 * MENULEAVE 6, INVALID 7, and 10 for the unnamed one. 0x7E is the SFX bank
 * selector vs_main_playSfx switches on. */
void vs_battle_playMenuChangeSfx(void) { vs_main_playSfxDefault(0x7E, 4); }
void vs_battle_playMenuSelectSfx(void) { vs_main_playSfxDefault(0x7E, 5); }
void vs_battle_playMenuLeaveSfx(void)  { vs_main_playSfxDefault(0x7E, 6); }
void vs_battle_playInvalidSfx(void)    { vs_main_playSfxDefault(0x7E, 7); }
void vs_battle_playSfx10(void)         { vs_main_playSfxDefault(0x7E, 10); }

/* func_800E6694: a battle-state query (146C.c) reached when saving syncs the
 * character's live battle stats into the save (func_80042CB0). The menu harness
 * has no real battle state, so return 0 -- the inventory the save actually cares
 * about is copied separately and round-trips correctly. See NATIVE_DEVIATIONS. */
int func_800E6694(int arg0) { (void)arg0; return 0; }

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
#define D_1F800000 ((u_long**)0x1F800000UL)

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

/* overlay.c is excluded from the native build (inline MIPS asm), and the
 * overlay system is moot natively -- every overlay is statically linked, so
 * there is nothing to load or wait for. */
void vs_overlay_wait(void) { }
void vs_overlay_getSp(int* sp) { int local; *sp = (int)(long)&local; }

/* func_8007ACB0: sets up the GTE camera matrix for the ITEMS submenu 3D
 * background scene (rotating character/weapon model). The synthetic harness
 * loads no 3D models (vs_battle_actorModels slots null), so the scene has nothing to
 * draw -- no-op it and let the 2D item list render. Phase-3+ work will load
 * real models and reinstate the full decomp. */
void func_8007ACB0(void) { (void)0; }

/* ---- variable-width font string renderer (5BF94) ----
 *
 * Faithful native decomp of vs_battle_renderTextRawColor, translated from
 * build/src/BATTLE/BATTLE.PRG/nonmatchings/5BF94/vs_battle_renderTextRawColor.s.
 * Builds the same GP0 command list (E1 texpage + per-char textured SPRTs) that
 * the native libgpu rasteriser already walks, so it renders like the real game.
 *
 *   text     : ASCII string (0x20-based glyph indices)
 *   xy       : packed position, x = (s16)xy, y = xy >> 16
 *   rgb      : 0x00BBGGRR modulation colour
 *   nextPrim : OT slot to splice the chain into (NULL -> D_1F800000[2])
 *
 * Three harvested font tables drive metrics:
 *   D_800EB7B4 : per-glyph width, 4-bit nibbles (even idx low, odd idx high)
 *   D_800EB708 : letter-pair kerning, 2-bit fields, index 26*l1 + l2
 *   D_800EB7D4 : per-glyph VRAM u base / clut-parity byte
 */
extern unsigned char D_800EB7B4[];   /* glyph widths (nibbles)        */
extern unsigned char D_800EB708[];   /* kerning pairs (2-bit)         */
extern unsigned char D_800EB7D4[];   /* glyph tile map (u base|parity) */

static int vs_glyph_width(int ci)
{
    return (D_800EB7B4[ci >> 1] >> ((ci & 1) << 2)) & 0xF;
}

/* Kerning adjustment between two A-Z letter indices (0..25). */
static int vs_kern(int l1, int l2)
{
    int idx = 26 * l1 + l2;
    return (D_800EB708[idx >> 2] >> ((idx & 3) << 1)) & 3;
}

void vs_battle_renderTextRawColor(const char* text, int xy, int rgb, u_long* nextPrim)
{
    const unsigned char* s = (const unsigned char*)text;
    u_long* prim = D_1F800000[0];
    int y = xy >> 16;
    int x = (short)(xy & 0xFFFF);
    unsigned char c;

    if (nextPrim == 0)
        nextPrim = D_1F800000[2];

    /* texpage primitive (1 GP0 word, 0xE100000C) */
    {
        u_long* next = prim + 2;
        prim[0] = ((u_long)(unsigned long)next & 0xFFFFFF) | 0x01000000;
        prim[1] = 0xE100000C;
        prim = next;
    }

    c = *s++;
    while (c != 0) {
        int t6 = (unsigned char)(c - 0x20);   /* raw glyph offset (kept for kerning) */
        int ci = t6 & 0xFF;                    /* glyph index */

        if (ci >= 0x3B) {
            if (ci == 0x3B) x -= 1;            /* narrow control glyph */
            c = *s++;
            continue;
        }

        if (ci == 3 || ci == 4) {
            /* auto-advance control code: shift x left by the measured width of
             * the following run (# full,  half). */
            int total = 0;
            const unsigned char* m = s;
            unsigned char mc = *m;
            while (mc != 0) {
                int mi = (mc - 0x20) & 0xFF;
                if (mi < 0x3C) {
                    int letter = (mi - 0x21) & 0xFF;
                    total += vs_glyph_width(mi);
                    if (letter < 0x1A) {
                        int nc = m[1] - 0x41;
                        if ((nc & 0xFF) < 0x1A)
                            total -= vs_kern(letter, nc);
                    }
                }
                mc = *(++m);
            }
            x -= total >> (ci - 3);
            c = *s++;
            continue;
        }

        /* normal glyph */
        {
            int w = vs_glyph_width(ci);
            if (w != 0) {
                unsigned char tile = D_800EB7D4[ci];
                u_long* next = (u_long*)((char*)prim + 0x14);
                int parity = tile & 1;
                u_long uvclut = (u_long)(tile & 0xFE)
                              | (u_long)(((parity * 5) << 9) + 0x4C00)
                              | 0x37F60000u;

                prim[0] = ((u_long)(unsigned long)next & 0xFFFFFF) | 0x04000000;
                prim[1] = (u_long)rgb | 0x64000000u;
                prim[2] = ((u_long)(x & 0xFFFF)) | ((u_long)(y & 0xFFFF) << 16);
                prim[4] = (u_long)w | 0x000A0000u;
                if (ci != 0xA) {
                    prim[3] = uvclut;
                } else {
                    /* special glyph (index 0x0A): fixed clut, 1px up/left, h=0x0B */
                    prim[2] = ((u_long)((x - 1) & 0xFFFF))
                            | ((u_long)((y - 1) & 0xFFFF) << 16);
                    prim[3] = 0x37F64BE8u;
                    prim[4] = (u_long)w | 0x000B0000u;
                }
                prim = next;
            }

            /* advance pen + kerning to the next char */
            x += w;
            {
                int letter = (t6 - 0x21) & 0xFF;   /* (c - 0x41) letter index */
                if (letter < 0x1A) {
                    int nc = *s - 0x41;
                    if ((nc & 0xFF) < 0x1A)
                        x -= vs_kern(letter, nc);
                }
            }
        }

        c = *s++;
    }

    /* splice the chain into the OT slot and bump the free-prim cursor */
    prim[0] = *nextPrim & 0xFFFFFF;
    *nextPrim = (u_long)((unsigned long)D_1F800000[0] & 0xFFFFFF);
    D_1F800000[0] = (u_long*)((char*)prim + 4);
}
