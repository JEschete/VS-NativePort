/* libgpu: ordering-table plumbing, drawing environment, primitive helpers.
 *
 * Semantics were read from the original assembly in
 * build/src/SLUS_010.40/nonmatchings/libgpu/ (P00, P01, P06, P10, P16, P18,
 * P22, P26), not guessed from the headers.
 *
 * The 24-bit address fields work natively because the binary is linked below
 * 16MB; see native/include/psx_gpu.h.
 */
#include <psx_gpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OT_TERM 0xFFFFFFu

static DRAWENV cur_draw;
static DISPENV cur_disp;
static int disp_mask;
static int ot_dump;
static int prims_drawn;
static uint16_t gp0_tpage;   /* current GP0 texture page (from 0xE1) */

void psx_gpu_set_ot_dump(int enable) { ot_dump = enable; }
int  psx_gpu_prims_drawn(void)       { return prims_drawn; }
void psx_gpu_reset_stats(void)       { prims_drawn = 0; }

static uint32_t tag_addr(const void* p) { return ((const P_TAG*)p)->addr; }

/* The ordering table stores 24-bit addresses. The binary is linked below 16MB
 * so static, .bss and heap pointers fit -- but the STACK lives high, so an
 * ordering table or primitive placed on the stack would be silently truncated.
 * Fail loudly instead of corrupting the display list. */
static void check_24bit(const void* p)
{
    uintptr_t a = (uintptr_t)p;
    if (a > OT_TERM) {
        printf("[native] FATAL: %p does not fit in the 24-bit ordering-table "
               "address field.\n         Ordering tables and primitives must be "
               "static/global or heap allocated, never on the stack.\n", p);
        abort();
    }
}

static void tag_set_addr(void* p, uint32_t a)
{
    uint32_t* w = (uint32_t*)p;
    *w = (*w & 0xFF000000u) | (a & OT_TERM);
}

/* ---- ordering table ---- */

unsigned long* ClearOTag(unsigned long* ot, int n)
{
    int i;
    check_24bit(ot);
    for (i = 0; i < n - 1; i++) {
        ot[i] = 0;
        tag_set_addr(&ot[i], (uint32_t)(uintptr_t)&ot[i + 1]);
    }
    ot[n - 1] = OT_TERM;
    return ot;
}

unsigned long* ClearOTagR(unsigned long* ot, int n)
{
    int i;
    check_24bit(ot);
    for (i = n - 1; i > 0; i--) {
        ot[i] = 0;
        tag_set_addr(&ot[i], (uint32_t)(uintptr_t)&ot[i - 1]);
    }
    ot[0] = OT_TERM;
    return ot;
}

/* AddPrim(ot, p): the primitive inherits the OT entry address, then the OT
 * entry points at the primitive. Both keep their own length byte, matching
 * the original assembly. */
void AddPrim(void* ot, void* p)
{
    check_24bit(ot);
    check_24bit(p);
    tag_set_addr(p, tag_addr(ot));
    tag_set_addr(ot, (uint32_t)(uintptr_t)p);
}

void AddPrims(void* ot, void* p0, void* p1)
{
    tag_set_addr(p1, tag_addr(ot));
    tag_set_addr(ot, (uint32_t)(uintptr_t)p0);
}

/* ---- rasteriser (minimal for now: solid TILE) ---- */

static uint16_t rgb555(unsigned r, unsigned g, unsigned b)
{
    return (uint16_t)(((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3));
}

static void fill_rect(int x, int y, int w, int h, uint16_t c)
{
    uint16_t* vram = psx_vram();
    int i, j;
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    int cx0 = cur_draw.clip.x, cy0 = cur_draw.clip.y;
    int cx1 = cur_draw.clip.x + cur_draw.clip.w;
    int cy1 = cur_draw.clip.y + cur_draw.clip.h;

    if (x0 < cx0) x0 = cx0;
    if (y0 < cy0) y0 = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > PSX_VRAM_W) x1 = PSX_VRAM_W;
    if (y1 > PSX_VRAM_H) y1 = PSX_VRAM_H;

    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++)
            vram[j * PSX_VRAM_W + i] = c;
}

/* ---- triangle rasteriser (flat and Gouraud) ---- */

typedef struct { int x, y, r, g, b, u, v; } VERT;

static void put_px(int x, int y, uint16_t c)
{
    if (x < cur_draw.clip.x || y < cur_draw.clip.y) return;
    if (x >= cur_draw.clip.x + cur_draw.clip.w) return;
    if (y >= cur_draw.clip.y + cur_draw.clip.h) return;
    if (x < 0 || y < 0 || x >= PSX_VRAM_W || y >= PSX_VRAM_H) return;
    psx_vram()[y * PSX_VRAM_W + x] = c;
}

/* Half-space rasterisation: robust for any winding, no edge sorting needed. */
static void draw_tri(const VERT* a, const VERT* b, const VERT* c, int gouraud)
{
    int minx, maxx, miny, maxy, x, y;
    long area = (long)(b->x - a->x) * (c->y - a->y)
              - (long)(b->y - a->y) * (c->x - a->x);
    if (area == 0) return;

    minx = a->x < b->x ? (a->x < c->x ? a->x : c->x) : (b->x < c->x ? b->x : c->x);
    maxx = a->x > b->x ? (a->x > c->x ? a->x : c->x) : (b->x > c->x ? b->x : c->x);
    miny = a->y < b->y ? (a->y < c->y ? a->y : c->y) : (b->y < c->y ? b->y : c->y);
    maxy = a->y > b->y ? (a->y > c->y ? a->y : c->y) : (b->y > c->y ? b->y : c->y);

    if (minx < cur_draw.clip.x) minx = cur_draw.clip.x;
    if (miny < cur_draw.clip.y) miny = cur_draw.clip.y;
    if (maxx > cur_draw.clip.x + cur_draw.clip.w) maxx = cur_draw.clip.x + cur_draw.clip.w;
    if (maxy > cur_draw.clip.y + cur_draw.clip.h) maxy = cur_draw.clip.y + cur_draw.clip.h;

    for (y = miny; y < maxy; y++) {
        for (x = minx; x < maxx; x++) {
            long w0 = (long)(c->x - b->x) * (y - b->y) - (long)(c->y - b->y) * (x - b->x);
            long w1 = (long)(a->x - c->x) * (y - c->y) - (long)(a->y - c->y) * (x - c->x);
            long w2 = (long)(b->x - a->x) * (y - a->y) - (long)(b->y - a->y) * (x - a->x);
            int inside = (area > 0) ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                                    : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (!inside) continue;
            if (gouraud) {
                long A = area > 0 ? area : -area;
                long u0 = w0 > 0 ? w0 : -w0;
                long u1 = w1 > 0 ? w1 : -w1;
                long u2 = w2 > 0 ? w2 : -w2;
                int r = (int)((u0 * a->r + u1 * b->r + u2 * c->r) / A);
                int g = (int)((u0 * a->g + u1 * b->g + u2 * c->g) / A);
                int bl = (int)((u0 * a->b + u1 * b->b + u2 * c->b) / A);
                put_px(x, y, rgb555((unsigned)r, (unsigned)g, (unsigned)bl));
            } else {
                put_px(x, y, rgb555((unsigned)a->r, (unsigned)a->g, (unsigned)a->b));
            }
        }
    }
}

static void set_vert(VERT* v, const short* xy, unsigned r, unsigned g, unsigned b)
{
    v->x = xy[0] + cur_draw.ofs[0];
    v->y = xy[1] + cur_draw.ofs[1];
    v->r = (int)r; v->g = (int)g; v->b = (int)b;
}

/* ---- GP0 command-list rasteriser ----
 * VS builds each ordering-table packet as a stream of raw GP0 words -- usually
 * a texpage command (0xE1) followed by a textured sprite/polygon. The old code
 * decoded a fixed PSYQ struct and read the 0xE1 as the primitive code, skipping
 * everything. This walks the packet body (len words at offset 4) as a GP0
 * command stream and dispatches each command by its top byte.
 *
 * STAGE 1: parse every command with the correct word count (so the walk stays
 * in sync) and rasterise geometry with flat colour. Textured primitives are
 * drawn with their modulation colour as a visible placeholder -- real
 * CLUT-texture sampling and font upload come next (see TODO(texture)). */

static int gp0_x(uint32_t w) { return (int)(int16_t)(w & 0xFFFF) + cur_draw.ofs[0]; }
static int gp0_y(uint32_t w) { return (int)(int16_t)(w >> 16)    + cur_draw.ofs[1]; }

static void gp0_setv(VERT* v, uint32_t xy, uint32_t col)
{
    v->x = gp0_x(xy); v->y = gp0_y(xy);
    v->r = col & 0xFF; v->g = (col >> 8) & 0xFF; v->b = (col >> 16) & 0xFF;
}

/* Number of GP0 words a command occupies (command word included). */
static unsigned gp0_words(uint32_t w)
{
    unsigned cmd = w >> 24;
    if (cmd == 0x00 || cmd == 0x01 || cmd == 0x03 || (cmd >= 0xE1 && cmd <= 0xE6))
        return 1;
    if (cmd == 0x02) return 3;                       /* fill rect */
    if (cmd >= 0x20 && cmd <= 0x3F) {                /* polygon */
        unsigned n = (cmd & 0x08) ? 4 : 3;
        unsigned shaded = cmd & 0x10, tex = cmd & 0x04;
        return shaded ? (tex ? 3 * n : 2 * n) : (1 + (tex ? 2 : 1) * n);
    }
    if (cmd >= 0x40 && cmd <= 0x5F)                  /* line: mono=3, shaded=4 */
        return (cmd & 0x10) ? 4 : 3;
    if (cmd >= 0x60 && cmd <= 0x7F) {                /* rect / sprite */
        unsigned tex = cmd & 0x04, size = (cmd >> 3) & 3;
        return 1 /*colour*/ + 1 /*xy*/ + (tex ? 1 : 0) + (size == 0 ? 1 : 0);
    }
    return 1;   /* unknown: step one word to avoid desyncing */
}

/* Sample one texel from VRAM via the current texture page + CLUT.
 * Returns 0 for the transparent (index 0 / all-zero) texel. */
static uint16_t gp0_sample(int u, int v, uint16_t tp, uint16_t clut)
{
    uint16_t* vram = psx_vram();
    int mode = (tp >> 7) & 3;
    int bx = (tp & 0x0F) * 64;
    int by = ((tp >> 4) & 1) * 256;
    if (mode == 2) {                                  /* 15-bit direct */
        return vram[((by + v) & 0x1FF) * PSX_VRAM_W + ((bx + u) & 0x3FF)];
    } else {                                          /* 4-bit or 8-bit CLUT */
        int shift = (mode == 0) ? 2 : 1;
        uint16_t word = vram[((by + v) & 0x1FF) * PSX_VRAM_W
                             + ((bx + (u >> shift)) & 0x3FF)];
        int idx = (mode == 0) ? ((word >> ((u & 3) * 4)) & 0x0F)
                              : ((word >> ((u & 1) * 8)) & 0xFF);
        int cx, cy;
        if (idx == 0) return 0;
        cx = (clut & 0x3F) * 16;
        cy = (clut >> 6) & 0x1FF;
        return vram[(cy & 0x1FF) * PSX_VRAM_W + ((cx + idx) & 0x3FF)];
    }
}

static void blit_sprite(int x, int y, int w, int h,
                        int u0, int v0, uint16_t tp, uint16_t clut)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint16_t t = gp0_sample(u0 + i, v0 + j, tp, clut);
            if (t) put_px(x + i, y + j, t);           /* 0 = transparent */
        }
}

/* Textured triangle: affine u,v interpolation + CLUT sample, modulated by the
 * (optionally Gouraud) primitive colour. 0x80 per channel is neutral (texture
 * shown as-is), matching PS1 texture modulation. */
static void draw_tri_tex(const VERT* a, const VERT* b, const VERT* c,
                         uint16_t tp, uint16_t clut, int shaded)
{
    int minx, maxx, miny, maxy, x, y;
    long area = (long)(b->x - a->x) * (c->y - a->y)
              - (long)(b->y - a->y) * (c->x - a->x);
    long A = area < 0 ? -area : area;
    if (area == 0) return;

    minx = a->x < b->x ? (a->x < c->x ? a->x : c->x) : (b->x < c->x ? b->x : c->x);
    maxx = a->x > b->x ? (a->x > c->x ? a->x : c->x) : (b->x > c->x ? b->x : c->x);
    miny = a->y < b->y ? (a->y < c->y ? a->y : c->y) : (b->y < c->y ? b->y : c->y);
    maxy = a->y > b->y ? (a->y > c->y ? a->y : c->y) : (b->y > c->y ? b->y : c->y);
    if (minx < cur_draw.clip.x) minx = cur_draw.clip.x;
    if (miny < cur_draw.clip.y) miny = cur_draw.clip.y;
    if (maxx > cur_draw.clip.x + cur_draw.clip.w) maxx = cur_draw.clip.x + cur_draw.clip.w;
    if (maxy > cur_draw.clip.y + cur_draw.clip.h) maxy = cur_draw.clip.y + cur_draw.clip.h;

    for (y = miny; y < maxy; y++)
        for (x = minx; x < maxx; x++) {
            long w0 = (long)(c->x - b->x) * (y - b->y) - (long)(c->y - b->y) * (x - b->x);
            long w1 = (long)(a->x - c->x) * (y - c->y) - (long)(a->y - c->y) * (x - c->x);
            long w2 = (long)(b->x - a->x) * (y - a->y) - (long)(b->y - a->y) * (x - a->x);
            int inside = (area > 0) ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                                    : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            long u0, u1, u2;
            int tu, tv, cr, cg, cb, tr, tg, tb;
            uint16_t t;
            if (!inside) continue;
            u0 = w0 < 0 ? -w0 : w0; u1 = w1 < 0 ? -w1 : w1; u2 = w2 < 0 ? -w2 : w2;
            tu = (int)((u0 * a->u + u1 * b->u + u2 * c->u) / A);
            tv = (int)((u0 * a->v + u1 * b->v + u2 * c->v) / A);
            t = gp0_sample(tu, tv, tp, clut);
            if (!t) continue;                         /* transparent texel */
            if (shaded) {
                cr = (int)((u0 * a->r + u1 * b->r + u2 * c->r) / A);
                cg = (int)((u0 * a->g + u1 * b->g + u2 * c->g) / A);
                cb = (int)((u0 * a->b + u1 * b->b + u2 * c->b) / A);
            } else { cr = a->r; cg = a->g; cb = a->b; }
            tr = (t & 0x1F) * cr / 128; if (tr > 31) tr = 31;
            tg = ((t >> 5) & 0x1F) * cg / 128; if (tg > 31) tg = 31;
            tb = ((t >> 10) & 0x1F) * cb / 128; if (tb > 31) tb = 31;
            put_px(x, y, (uint16_t)(tr | (tg << 5) | (tb << 10)));
        }
}

static void gp0_exec(const uint32_t* w)
{
    unsigned cmd = w[0] >> 24;
    uint32_t col = w[0] & 0xFFFFFF;
    uint16_t c = rgb555(col & 0xFF, (col >> 8) & 0xFF, (col >> 16) & 0xFF);

    if (cmd >= 0x20 && cmd <= 0x3F) {                /* polygon */
        int n = (cmd & 0x08) ? 4 : 3;
        int shaded = cmd & 0x10, tex = cmd & 0x04;
        int stride = shaded ? (tex ? 3 : 2) : (tex ? 2 : 1);
        int base = shaded ? 0 : 1;                   /* flat: skip colour word */
        int uvoff = shaded ? 2 : 1;                  /* uv word within a vertex */
        VERT v[4];
        int i;
        for (i = 0; i < n; i++) {
            const uint32_t* e = w + base + i * stride;
            uint32_t vcol = shaded ? e[0] : col;
            uint32_t xy = shaded ? e[1] : e[0];
            gp0_setv(&v[i], xy, vcol);
            if (tex) { uint32_t uv = e[uvoff]; v[i].u = uv & 0xFF; v[i].v = (uv >> 8) & 0xFF; }
        }
        if (tex) {
            /* CLUT is in vertex0's uv word, texpage in vertex1's. */
            uint16_t clut = (uint16_t)((w[base + uvoff] >> 16) & 0xFFFF);
            uint16_t tp   = (uint16_t)((w[base + stride + uvoff] >> 16) & 0xFFFF);
            draw_tri_tex(&v[0], &v[1], &v[2], tp, clut, shaded);
            if (n == 4) draw_tri_tex(&v[1], &v[2], &v[3], tp, clut, shaded);
        } else {
            draw_tri(&v[0], &v[1], &v[2], shaded);
            if (n == 4) draw_tri(&v[1], &v[2], &v[3], shaded);
        }
        prims_drawn++;
    } else if (cmd >= 0x60 && cmd <= 0x7F) {         /* rect / sprite */
        int size = (cmd >> 3) & 3, tex = cmd & 0x04;
        int x = gp0_x(w[1]), y = gp0_y(w[1]);
        int rw, rh;
        const uint32_t* whw = w + 2 + (tex ? 1 : 0);
        if (size == 1) { rw = rh = 1; }
        else if (size == 2) { rw = rh = 8; }
        else if (size == 3) { rw = rh = 16; }
        else { rw = (int)(int16_t)(*whw & 0xFFFF); rh = (int)(int16_t)(*whw >> 16); }
        if (tex) {                                   /* textured sprite: sample */
            int u0 = w[2] & 0xFF, v0 = (w[2] >> 8) & 0xFF;
            uint16_t clut = (uint16_t)((w[2] >> 16) & 0xFFFF);
            blit_sprite(x, y, rw, rh, u0, v0, gp0_tpage, clut);
        } else {
            fill_rect(x, y, rw, rh, c);
        }
        prims_drawn++;
    } else if (cmd == 0x02) {                        /* fill rect (raw VRAM) */
        int x = (int)(w[1] & 0xFFFF), y = (int)(w[1] >> 16);
        int rw = (int)(w[2] & 0xFFFF), rh = (int)(w[2] >> 16);
        fill_rect(x, y, rw, rh, c);
    } else if (cmd == 0xE1) {
        gp0_tpage = (uint16_t)(w[0] & 0xFFFF);       /* current texture page */
    }
    /* 0xE2..0xE6 (tex window, draw area/offset, mask) and 0x00/0x01/0x03: nop */
}

static void draw_one(const P_TAG* p)
{
    const uint32_t* w = (const uint32_t*)((const char*)p + 4);
    unsigned n = p->len, i = 0, guard = 0;
    if (ot_dump)
        printf("  packet len=%u first=%08X\n", p->len, n ? w[0] : 0);
    while (i < n && guard++ < 64) {
        unsigned step = gp0_words(w[i]);
        if (i + step > n) break;                     /* malformed: stop */
        gp0_exec(w + i);
        i += step;
    }
}

void DrawOTag(unsigned long* ot)
{
    uint32_t addr = tag_addr(ot);
    int guard = 0;
    if (ot_dump) printf("DrawOTag @%p\n", (void*)ot);
    while (addr != OT_TERM && addr != 0) {
        const P_TAG* p = (const P_TAG*)(uintptr_t)addr;
        if (p->len) draw_one(p);
        addr = p->addr;
        if (++guard > 0x40000) {
            printf("[native] DrawOTag: runaway ordering table, aborting walk\n");
            break;
        }
    }
}

DRAWENV* PutDrawEnv(DRAWENV* env);

void DrawOTagIO(unsigned long* ot) { DrawOTag(ot); }
void DrawOTagEnv(unsigned long* ot, DRAWENV* env) { PutDrawEnv(env); DrawOTag(ot); }
void DrawPrim(void* p) { draw_one((const P_TAG*)p); }
int  DrawSync(int mode) { (void)mode; return 0; }

/* ---- environment ---- */

DISPENV* SetDefDispEnv(DISPENV* env, int x, int y, int w, int h)
{
    memset(env, 0, sizeof *env);
    env->disp.x = (short)x; env->disp.y = (short)y;
    env->disp.w = (short)w; env->disp.h = (short)h;
    return env;
}

DRAWENV* SetDefDrawEnv(DRAWENV* env, int x, int y, int w, int h)
{
    memset(env, 0, sizeof *env);
    env->clip.x = (short)x; env->clip.y = (short)y;
    env->clip.w = (short)w; env->clip.h = (short)h;
    env->ofs[0] = (short)x; env->ofs[1] = (short)y;
    env->dtd = 1;
    env->dfe = (h > 256) ? 1 : 0;
    return env;
}

/* Selecting the displayed region also re-aims the host window at it (no-op when
 * running headless) -- the boot sequence moves between a 320x240 menu area and
 * the title screen's 512x480 area at x=512. */
void psx_present_set_display(int x, int y, int w, int h, int rgb24);

DISPENV* PutDispEnv(DISPENV* env)
{
    cur_disp = *env;
    /* isrgb24 matters as much as the rect: the intro movie displays MDEC output
     * in 24-bit colour, which the window has to unpack differently. */
    if (getenv("VS_GPU_TRACE"))
        printf("[gpu] PutDispEnv disp(%d,%d %dx%d) screen(%d,%d %dx%d) rgb24=%d inter=%d\n",
               env->disp.x, env->disp.y, env->disp.w, env->disp.h,
               env->screen.x, env->screen.y, env->screen.w, env->screen.h,
               env->isrgb24, env->isinter);
    /* DISPENV has two rectangles and they mean different things. `disp` is
     * where in VRAM the picture starts; `screen` is how much of it the video
     * signal actually carries -- on hardware it sets the display range, so
     * anything past screen.h is simply never scanned out.
     *
     * The game relies on that: it draws into 240-line buffers and asks for 224
     * lines (screen.y = 8, screen.h = 224), the standard way of centring a
     * 240-line buffer in NTSC's visible area. Showing the full disp.h instead
     * put 16 lines of whatever VRAM sits below the picture on screen, which is
     * font and texture data -- the band of coloured garbage along the bottom
     * edge of the title and publisher screens. Interlaced modes carry two
     * fields per frame, so their visible height is twice screen.h. */
    {
        int h = env->disp.h;
        if (env->screen.h > 0) {
            int visible = env->screen.h * (env->isinter ? 2 : 1);
            if (visible < h) h = visible;
        }
        psx_present_set_display(env->disp.x, env->disp.y, env->disp.w, h,
            env->isrgb24 != 0);
    }
    return env;
}
DRAWENV* PutDrawEnv(DRAWENV* env) { cur_draw = *env; return env; }
int SetDispMask(int mask) { disp_mask = mask; return mask; }

int ResetGraph(int mode)
{
    (void)mode;
    psx_vram_reset();
    memset(&cur_draw, 0, sizeof cur_draw);
    cur_draw.clip.w = 640;
    cur_draw.clip.h = 512;
    return 0;
}

void SetDrawArea(void* p, RECT* r) { (void)p; (void)r; }
void SetDrawMode(void* p, int dfe, int dtd, int tpage, RECT* tw)
{ (void)p; (void)dfe; (void)dtd; (void)tpage; (void)tw; }
void SetDrawMove(void* p, RECT* r, int x, int y)
{ (void)p; (void)r; (void)x; (void)y; }
void SetDrawTPage(void* p, int dfe, int dtd, int tpage)
{ (void)p; (void)dfe; (void)dtd; (void)tpage; }

/* ---- primitive helpers (len/code values read from the original assembly) ---- */

void SetPolyF4(void* p) { ((P_TAG*)p)->len = 5; ((P_TAG*)p)->code = 0x28; }
void SetPolyG4(void* p) { ((P_TAG*)p)->len = 8; ((P_TAG*)p)->code = 0x38; }
void SetSprt(void* p)   { ((P_TAG*)p)->len = 4; ((P_TAG*)p)->code = 0x64; }
void SetTile(void* p)   { ((P_TAG*)p)->len = 3; ((P_TAG*)p)->code = 0x60; }

void SetSemiTrans(void* p, int abe)
{
    P_TAG* t = (P_TAG*)p;
    t->code = (unsigned char)(abe ? (t->code | 0x02) : (t->code & 0xFD));
}

unsigned short GetTPage(int tp, int abr, int x, int y)
{
    return (unsigned short)(((tp & 0x3) << 7) | ((abr & 0x3) << 5)
                            | ((y & 0x100) >> 4) | ((x & 0x3FF) >> 6)
                            | ((y & 0x200) << 2));
}

unsigned short GetClut(int x, int y)
{
    return (unsigned short)((((y) << 6) | ((x >> 4) & 0x3F)) & 0xFFFF);
}

/* ---- VRAM transfers ---- */

/* VS_GPU_TRACE=1 logs every VRAM upload with a cheap checksum of the source.
 * "the rect is right but the pixels are zero" and "nothing called LoadImage at
 * all" look identical from a VRAM dump, and they have very different causes. */
int LoadImage(RECT* r, unsigned long* p)
{
    uint16_t* vram = psx_vram();
    const uint16_t* src = (const uint16_t*)p;
    int j;

    if (getenv("VS_GPU_TRACE")) {
        unsigned sum = 0, n = (unsigned)r->w * (unsigned)r->h, i;
        for (i = 0; i < n; i++) sum += src[i];
        printf("[gpu] LoadImage (%d,%d %dx%d) src=%p checksum=%08X\n",
               r->x, r->y, r->w, r->h, (void*)p, sum);
    }
    for (j = 0; j < r->h; j++) {
        int y = r->y + j;
        if (y < 0 || y >= PSX_VRAM_H) continue;
        memcpy(&vram[y * PSX_VRAM_W + r->x], &src[j * r->w],
               (size_t)r->w * sizeof(uint16_t));
    }
    return 0;
}

int LoadImage2(RECT* r, unsigned long* p) { return LoadImage(r, p); }

int StoreImage(RECT* r, unsigned long* p)
{
    const uint16_t* vram = psx_vram();
    uint16_t* dst = (uint16_t*)p;
    int j;
    for (j = 0; j < r->h; j++) {
        int y = r->y + j;
        if (y < 0 || y >= PSX_VRAM_H) continue;
        memcpy(&dst[j * r->w], &vram[y * PSX_VRAM_W + r->x],
               (size_t)r->w * sizeof(uint16_t));
    }
    return 0;
}

int MoveImage(RECT* r, int x, int y)
{
    uint16_t* vram = psx_vram();
    int j;
    for (j = 0; j < r->h; j++) {
        int sy = r->y + j, dy = y + j;
        if (sy < 0 || sy >= PSX_VRAM_H || dy < 0 || dy >= PSX_VRAM_H) continue;
        memmove(&vram[dy * PSX_VRAM_W + x], &vram[sy * PSX_VRAM_W + r->x],
                (size_t)r->w * sizeof(uint16_t));
    }
    return 0;
}

int ClearImage(RECT* r, unsigned char red, unsigned char green, unsigned char blue)
{
    uint16_t c = rgb555(red, green, blue);
    uint16_t* vram = psx_vram();
    int i, j;
    for (j = 0; j < r->h; j++) {
        int y = r->y + j;
        if (y < 0 || y >= PSX_VRAM_H) continue;
        for (i = 0; i < r->w; i++) {
            int x = r->x + i;
            if (x >= 0 && x < PSX_VRAM_W) vram[y * PSX_VRAM_W + x] = c;
        }
    }
    return 0;
}

/* Same fill, but returns without waiting for the GPU to finish. There is no
 * asynchronous GPU here -- ClearImage already completed -- so they are one and
 * the same. Used by _displayLoadingScreen on the boot path. */
int ClearImage2(RECT* r, unsigned char red, unsigned char green, unsigned char blue)
{
    return ClearImage(r, red, green, blue);
}

/* ---- debug font: inert, menus do not need it ---- */

int FntOpen(int x, int y, int w, int h, int isbg, int n)
{ (void)x; (void)y; (void)w; (void)h; (void)isbg; (void)n; return 0; }
void FntLoad(int x, int y) { (void)x; (void)y; }
unsigned long* FntFlush(int id) { (void)id; return 0; }
void SetDumpFnt(int id) { (void)id; }

/* Called when a DrawOTag finishes. Drawing is synchronous here, so the callback
 * is recorded and fired at the end of each DrawOTag. Retail installs an empty
 * function (_gpuSyncVoidCallback), so nothing depends on the timing. */
static void (*draw_sync_cb)(void);
void* DrawSyncCallback(void (*func)(void))
{
    void* old = (void*)draw_sync_cb;
    draw_sync_cb = func;
    return old;
}
void psx_gpu_fire_draw_sync(void) { if (draw_sync_cb) draw_sync_cb(); }
