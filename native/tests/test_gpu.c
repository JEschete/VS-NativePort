/* Ordering-table plumbing test.
 *
 * Validates the display-list layer before any real rasteriser exists:
 *   - ClearOTagR builds a walkable reverse-ordered list
 *   - AddPrim links primitives without clobbering the length byte
 *   - DrawOTag walks every primitive exactly once
 *   - draw ORDER is correct (later-drawn overwrites earlier where they overlap)
 *   - TILE primitives land in VRAM at the right place with the right colour
 *
 * Writes native/build/ot_test.ppm so the result can be eyeballed.
 */
#include <psx_gpu.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned long tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    short w, h;
} TILE;

extern unsigned long* ClearOTagR(unsigned long* ot, int n);
extern void AddPrim(void* ot, void* p);
extern void DrawOTag(unsigned long* ot);
extern void SetTile(void* p);
extern DRAWENV* SetDefDrawEnv(DRAWENV* env, int x, int y, int w, int h);
extern DRAWENV* PutDrawEnv(DRAWENV* env);
extern int ResetGraph(int mode);
extern int ClearImage(RECT* r, unsigned char r0, unsigned char g0, unsigned char b0);

static int checks, failures;

static void eq(const char* what, long got, long want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("  FAIL %-34s got %ld want %ld\n", what, got, want);
    }
}

static unsigned px(int x, int y) { return psx_vram()[y * PSX_VRAM_W + x]; }

#define OTSIZE 16

int main(void)
{
    static unsigned long ot[OTSIZE];
    static TILE t[3];
    DRAWENV draw;
    RECT full;
    unsigned long head;
    int i, walked;

    printf("=== GPU / ordering-table tests ===\n");

    ResetGraph(0);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    PutDrawEnv(&draw);

    full.x = 0; full.y = 0; full.w = 320; full.h = 240;
    ClearImage(&full, 0, 0, 0);

    /* ---- ClearOTagR produces a walkable chain ---- */
    ClearOTagR(ot, OTSIZE);
    walked = 0;
    head = ((P_TAG*)&ot[OTSIZE - 1])->addr;
    while (head != 0xFFFFFF && walked < OTSIZE * 4) {
        head = ((P_TAG*)(unsigned long)head)->addr;
        walked++;
    }
    eq("ClearOTagR chain length", walked, OTSIZE - 1);

    /* ---- three overlapping tiles at different OT depths ---- */
    for (i = 0; i < 3; i++) {
        SetTile(&t[i]);
        t[i].x0 = (short)(10 + i * 10);
        t[i].y0 = (short)(10 + i * 10);
        t[i].w = 40; t[i].h = 40;
        t[i].r0 = (unsigned char)(i == 0 ? 0xF8 : 0);
        t[i].g0 = (unsigned char)(i == 1 ? 0xF8 : 0);
        t[i].b0 = (unsigned char)(i == 2 ? 0xF8 : 0);
    }
    eq("SetTile len", ((P_TAG*)&t[0])->len, 3);
    eq("SetTile code", ((P_TAG*)&t[0])->code, 0x60);

    /* higher OT index is walked first with ClearOTagR + DrawOTag(&ot[n-1]) */
    AddPrim(&ot[12], &t[0]);   /* drawn first  (red)   */
    AddPrim(&ot[8],  &t[1]);   /* drawn second (green) */
    AddPrim(&ot[4],  &t[2]);   /* drawn last   (blue)  */

    eq("AddPrim kept len byte", ((P_TAG*)&t[0])->len, 3);

    psx_gpu_reset_stats();
    DrawOTag(&ot[OTSIZE - 1]);
    eq("primitives walked", psx_gpu_prims_drawn(), 3);

    /* ---- placement ---- */
    /* tiles span (10..50), (20..60), (30..70); pick points each one owns */
    eq("tile0 only, at (12,12)", px(12, 12), 0x001F);   /* red   */
    eq("tile1 topmost at (25,25)", px(25, 25), 0x03E0); /* green over red */
    eq("tile2 only, at (65,65)", px(65, 65), 0x7C00);   /* blue  */
    eq("outside tiles is clear",  px(200, 200), 0x0000);

    /* ---- ordering: tile2 (drawn last) wins where all three overlap ---- */
    eq("overlap shows last-drawn", px(35, 35), 0x7C00);

    psx_vram_write_ppm("native/build/ot_test.ppm", 0, 0, 320, 240);
    printf("  wrote native/build/ot_test.ppm\n");

    printf("=== %d checks, %d failures ===\n", checks, failures);
    return failures ? 1 : 0;
}
