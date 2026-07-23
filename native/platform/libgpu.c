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

typedef struct { int x, y, r, g, b; } VERT;

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

static void draw_one(const P_TAG* p)
{
    const unsigned char* body = (const unsigned char*)p + 4;
    const short* s = (const short*)((const char*)p + 8);
    prims_drawn++;
    if (ot_dump)
        printf("  prim code=%02X len=%u rgb=%02X%02X%02X\n",
               p->code, p->len, p->r0, p->g0, p->b0);

    switch (p->code & 0xFC) {
    case 0x60: {  /* TILE: x0, y0, w, h */
        fill_rect(s[0] + cur_draw.ofs[0], s[1] + cur_draw.ofs[1],
                  s[2], s[3], rgb555(p->r0, p->g0, p->b0));
        break;
    }
    case 0x64: {  /* SPRT: x0,y0, u0,v0,clut, w,h -- untextured fill for now */
        fill_rect(s[0] + cur_draw.ofs[0], s[1] + cur_draw.ofs[1],
                  s[4], s[5], rgb555(p->r0, p->g0, p->b0));
        break;
    }
    case 0x28: {  /* POLY_F4: flat quad, 4 vertices after the colour word */
        VERT v[4];
        int i;
        for (i = 0; i < 4; i++) set_vert(&v[i], s + i * 2, p->r0, p->g0, p->b0);
        draw_tri(&v[0], &v[1], &v[2], 0);
        draw_tri(&v[1], &v[2], &v[3], 0);
        break;
    }
    case 0x38: {  /* POLY_G4: colour and vertex interleaved, 8 words */
        VERT v[4];
        int i;
        for (i = 0; i < 4; i++) {
            const unsigned char* col = body + i * 8;
            const short* xy = (const short*)(body + i * 8 + 4);
            set_vert(&v[i], xy, col[0], col[1], col[2]);
        }
        draw_tri(&v[0], &v[1], &v[2], 1);
        draw_tri(&v[1], &v[2], &v[3], 1);
        break;
    }
    default:
        break;   /* textured primitives still to come */
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

DISPENV* PutDispEnv(DISPENV* env) { cur_disp = *env; return env; }
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

int LoadImage(RECT* r, unsigned long* p)
{
    uint16_t* vram = psx_vram();
    const uint16_t* src = (const uint16_t*)p;
    int j;
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

/* ---- debug font: inert, menus do not need it ---- */

int FntOpen(int x, int y, int w, int h, int isbg, int n)
{ (void)x; (void)y; (void)w; (void)h; (void)isbg; (void)n; return 0; }
void FntLoad(int x, int y) { (void)x; (void)y; }
unsigned long* FntFlush(int id) { (void)id; return 0; }
void SetDumpFnt(int id) { (void)id; }
