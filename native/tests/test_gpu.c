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

typedef struct {
    unsigned long tag;
    unsigned char r0, g0, b0, code;
    short x0, y0, x1, y1, x2, y2, x3, y3;
} POLY_F4;

typedef struct {
    unsigned long tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    unsigned char r1, g1, b1, p1;
    short x1, y1;
    unsigned char r2, g2, b2, p2;
    short x2, y2;
    unsigned char r3, g3, b3, p3;
    short x3, y3;
} POLY_G4;

extern void SetPolyF4(void* p);
extern void SetPolyG4(void* p);

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

    /* ---- polygons ---- */
    {
        static POLY_F4 f4;
        static POLY_G4 g4;
        static unsigned long ot2[4];

        ClearOTagR(ot2, 4);

        SetPolyF4(&f4);
        f4.r0 = 0xF8; f4.g0 = 0xF8; f4.b0 = 0x00;      /* yellow */
        f4.x0 = 100; f4.y0 = 100;
        f4.x1 = 180; f4.y1 = 100;
        f4.x2 = 100; f4.y2 = 180;
        f4.x3 = 180; f4.y3 = 180;
        eq("SetPolyF4 len", ((P_TAG*)&f4)->len, 5);
        eq("SetPolyF4 code", ((P_TAG*)&f4)->code, 0x28);

        SetPolyG4(&g4);
        g4.r0 = 0xF8; g4.g0 = 0;    g4.b0 = 0;
        g4.r1 = 0;    g4.g1 = 0xF8; g4.b1 = 0;
        g4.r2 = 0;    g4.g2 = 0;    g4.b2 = 0xF8;
        g4.r3 = 0xF8; g4.g3 = 0xF8; g4.b3 = 0xF8;
        g4.x0 = 200; g4.y0 = 100;
        g4.x1 = 290; g4.y1 = 100;
        g4.x2 = 200; g4.y2 = 190;
        g4.x3 = 290; g4.y3 = 190;
        eq("SetPolyG4 len", ((P_TAG*)&g4)->len, 8);
        eq("SetPolyG4 code", ((P_TAG*)&g4)->code, 0x38);

        AddPrim(&ot2[2], &f4);
        AddPrim(&ot2[1], &g4);

        psx_gpu_reset_stats();
        DrawOTag(&ot2[3]);
        eq("polys walked", psx_gpu_prims_drawn(), 2);

        eq("flat quad centre is yellow", px(140, 140), 0x03FF);
        eq("flat quad outside is clear", px(190, 140), 0x0000);

        /* Gouraud corners approach their vertex colours. */
        checks++;
        {
            unsigned c0 = px(202, 102), c3 = px(288, 188);
            if ((c0 & 0x001F) < 0x18 || (c3 & 0x7FFF) < 0x4000) {
                failures++;
                printf("  FAIL gouraud corners c0=%04X c3=%04X\n", c0, c3);
            }
        }
    }

    psx_vram_write_ppm("native/build/ot_test.ppm", 0, 0, 320, 240);
    printf("  wrote native/build/ot_test.ppm\n");

    printf("=== %d checks, %d failures ===\n", checks, failures);
    return failures ? 1 : 0;
}
