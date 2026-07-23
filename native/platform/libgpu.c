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

static void tag_set_addr(void* p, uint32_t a)
{
    uint32_t* w = (uint32_t*)p;
    *w = (*w & 0xFF000000u) | (a & OT_TERM);
}

/* ---- ordering table ---- */

unsigned long* ClearOTag(unsigned long* ot, int n)
{
    int i;
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

static void draw_one(const P_TAG* p)
{
    const short* s = (const short*)((const char*)p + 8);
    prims_drawn++;
    if (ot_dump)
        printf("  prim code=%02X len=%u rgb=%02X%02X%02X\n",
               p->code, p->len, p->r0, p->g0, p->b0);

    switch (p->code & 0xFC) {
    case 0x60:   /* TILE: x0, y0, w, h */
        fill_rect(s[0] + cur_draw.ofs[0], s[1] + cur_draw.ofs[1],
                  s[2], s[3], rgb555(p->r0, p->g0, p->b0));
        break;
    default:
        break;   /* textured and polygon primitives land here until step 3b */
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
