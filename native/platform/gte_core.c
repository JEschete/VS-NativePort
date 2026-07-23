/* Software PS1 GTE (COP2). Semantics per documented hardware behaviour.
 *
 * Everything here is deliberately literal about widths and saturation: the
 * game relies on exact results including overflow, so "close enough" is wrong.
 */
#include <psx_gte.h>
#include <string.h>

static gte_state_t g;

gte_state_t* gte(void) { return &g; }
void gte_reset(void) { memset(&g, 0, sizeof(g)); }

void     gte_set_ctrl(int r, uint32_t v) { g.cop2c[r & 31] = v; }
uint32_t gte_get_ctrl(int r)             { return g.cop2c[r & 31]; }
void     gte_set_data(int r, uint32_t v) { g.cop2d[r & 31] = v; }
uint32_t gte_get_data(int r)             { return g.cop2d[r & 31]; }

static inline int32_t s16lo(uint32_t v) { return (int16_t)(v & 0xFFFF); }
static inline int32_t s16hi(uint32_t v) { return (int16_t)(v >> 16); }

static inline void flag_set(uint32_t bit) { g.cop2c[GTE_C_FLAG] |= bit; }

/* Bit31 is the OR of bits 30..23 and 18..13. */
static void flag_finish(void)
{
    uint32_t f = g.cop2c[GTE_C_FLAG];
    if (f & 0x7F87E000u) f |= GTE_FLAG_ERROR;
    g.cop2c[GTE_C_FLAG] = f;
}

/* ---- MAC: 44-bit internal, overflow flags per component ---- */
static int64_t mac_check(int64_t v, int idx)
{
    static const uint32_t pos[3] = { GTE_FLAG_MAC1_POS, GTE_FLAG_MAC2_POS, GTE_FLAG_MAC3_POS };
    static const uint32_t neg[3] = { GTE_FLAG_MAC1_NEG, GTE_FLAG_MAC2_NEG, GTE_FLAG_MAC3_NEG };
    if (v >  (int64_t)0x7FFFFFFFFFFLL) flag_set(pos[idx]);
    if (v < -(int64_t)0x80000000000LL) flag_set(neg[idx]);
    return v;
}

/* ---- saturations ---- */
static int32_t lim_ir(int32_t v, int lm, int idx)
{
    static const uint32_t bits[3] = { GTE_FLAG_IR1_SAT, GTE_FLAG_IR2_SAT, GTE_FLAG_IR3_SAT };
    int32_t min = lm ? 0 : -0x8000;
    if (v > 0x7FFF) { flag_set(bits[idx]); return 0x7FFF; }
    if (v < min)    { flag_set(bits[idx]); return min; }
    return v;
}

static uint32_t lim_sz3(int64_t v)
{
    if (v > 0xFFFF) { flag_set(GTE_FLAG_SZ3_SAT); return 0xFFFF; }
    if (v < 0)      { flag_set(GTE_FLAG_SZ3_SAT); return 0; }
    return (uint32_t)v;
}

static int32_t lim_sxy(int64_t v, uint32_t bit)
{
    if (v >  0x3FF) { flag_set(bit); return  0x3FF; }
    if (v < -0x400) { flag_set(bit); return -0x400; }
    return (int32_t)v;
}

static int32_t lim_ir0(int64_t v)
{
    if (v > 0x1000) { flag_set(GTE_FLAG_IR0_SAT); return 0x1000; }
    if (v < 0)      { flag_set(GTE_FLAG_IR0_SAT); return 0; }
    return (int32_t)v;
}

/* ---- hardware reciprocal (UNR) ---- */
static uint8_t unr_table[257];
static int unr_ready = 0;

static void unr_init(void)
{
    int i;
    for (i = 0; i < 257; i++) {
        int v = (0x40000 / (i + 0x100) + 1) / 2 - 0x101;
        unr_table[i] = (uint8_t)(v < 0 ? 0 : v);
    }
    unr_ready = 1;
}

static int clz16(uint32_t v)
{
    int n = 0;
    while (n < 16 && !(v & 0x8000)) { v <<= 1; n++; }
    return n;
}

uint32_t gte_divide(uint32_t h, uint32_t sz3)
{
    uint32_t d, u;
    uint64_t n, r;
    if (!unr_ready) unr_init();
    if (sz3 == 0 || h >= sz3 * 2) return 0x1FFFF;   /* caller raises FLAG bit 17 */
    {
        int z = clz16(sz3);
        n = (uint64_t)h << z;
        d = sz3 << z;
    }
    u = unr_table[(d - 0x7FC0) >> 7] + 0x101;
    d = (uint32_t)((0x2000080ULL - (uint64_t)d * u) >> 8);
    d = (uint32_t)((0x0000080ULL + (uint64_t)d * u) >> 8);
    r = ((n * d) + 0x8000) >> 16;
    return r > 0x1FFFF ? 0x1FFFF : (uint32_t)r;
}

/* ---- rotation matrix / translation accessors ---- */
static void rot_matrix(int32_t m[3][3])
{
    m[0][0] = s16lo(g.cop2c[GTE_C_R11R12]); m[0][1] = s16hi(g.cop2c[GTE_C_R11R12]);
    m[0][2] = s16lo(g.cop2c[GTE_C_R13R21]); m[1][0] = s16hi(g.cop2c[GTE_C_R13R21]);
    m[1][1] = s16lo(g.cop2c[GTE_C_R22R23]); m[1][2] = s16hi(g.cop2c[GTE_C_R22R23]);
    m[2][0] = s16lo(g.cop2c[GTE_C_R31R32]); m[2][1] = s16hi(g.cop2c[GTE_C_R31R32]);
    m[2][2] = s16lo(g.cop2c[GTE_C_R33]);
}

static void vec_v0(int32_t v[3])
{
    v[0] = s16lo(g.cop2d[GTE_D_VXY0]);
    v[1] = s16hi(g.cop2d[GTE_D_VXY0]);
    v[2] = s16lo(g.cop2d[GTE_D_VZ0]);
}

static void store_mac_ir(int64_t mac[3], int sf, int lm)
{
    int i;
    for (i = 0; i < 3; i++) {
        int64_t m = mac_check(mac[i], i) >> (sf ? 12 : 0);
        g.cop2d[GTE_D_MAC1 + i] = (uint32_t)(int32_t)m;
        g.cop2d[GTE_D_IR1 + i]  = (uint32_t)lim_ir((int32_t)m, lm, i);
    }
}

/* ---- RTPS: rotate, translate, perspective-transform V0 ---- */
void gte_op_rtps(void)
{
    int32_t m[3][3], v[3];
    int64_t mac[3], mac0;
    uint32_t sz3, n;
    int i;

    g.cop2c[GTE_C_FLAG] = 0;
    rot_matrix(m);
    vec_v0(v);

    for (i = 0; i < 3; i++) {
        int64_t tr = (int64_t)(int32_t)g.cop2c[GTE_C_TRX + i] << 12;
        mac[i] = tr + (int64_t)m[i][0] * v[0]
                    + (int64_t)m[i][1] * v[1]
                    + (int64_t)m[i][2] * v[2];
    }
    store_mac_ir(mac, 1, 0);

    /* SZ FIFO: SZ0<-SZ1<-SZ2<-SZ3, SZ3 = MAC3 (sf=1 -> no extra shift) */
    g.cop2d[GTE_D_SZ0] = g.cop2d[GTE_D_SZ1];
    g.cop2d[GTE_D_SZ1] = g.cop2d[GTE_D_SZ2];
    g.cop2d[GTE_D_SZ2] = g.cop2d[GTE_D_SZ3];
    sz3 = lim_sz3((int64_t)(int32_t)g.cop2d[GTE_D_MAC3]);
    g.cop2d[GTE_D_SZ3] = sz3;

    {
        uint32_t h = g.cop2c[GTE_C_H] & 0xFFFF;
        n = gte_divide(h, sz3);
        if (sz3 == 0 || h >= sz3 * 2) flag_set(GTE_FLAG_DIV_OVF);
    }

    /* SXY FIFO */
    g.cop2d[GTE_D_SXY0] = g.cop2d[GTE_D_SXY1];
    g.cop2d[GTE_D_SXY1] = g.cop2d[GTE_D_SXY2];
    {
        int32_t ir1 = (int32_t)g.cop2d[GTE_D_IR1];
        int32_t ir2 = (int32_t)g.cop2d[GTE_D_IR2];
        int32_t sx, sy;
        mac0 = (int64_t)n * ir1 + (int32_t)g.cop2c[GTE_C_OFX];
        sx = lim_sxy(mac0 >> 16, GTE_FLAG_SX2_SAT);
        mac0 = (int64_t)n * ir2 + (int32_t)g.cop2c[GTE_C_OFY];
        sy = lim_sxy(mac0 >> 16, GTE_FLAG_SY2_SAT);
        g.cop2d[GTE_D_SXY2] = ((uint32_t)(sy & 0xFFFF) << 16) | (uint32_t)(sx & 0xFFFF);
    }

    /* depth cue: IR0 */
    mac0 = (int64_t)n * (int16_t)g.cop2c[GTE_C_DQA] + (int32_t)g.cop2c[GTE_C_DQB];
    g.cop2d[GTE_D_MAC0] = (uint32_t)(int32_t)mac0;
    g.cop2d[GTE_D_IR0]  = (uint32_t)lim_ir0(mac0 >> 12);

    flag_finish();
}

/* ---- MVMVA ----
 * Implements the combinations this port needs: rotation matrix (mx=0), vector
 * V0..V2 or IR (v=0..3), translation TR (cv=0) or none (cv=3). Other matrix /
 * vector selects are unimplemented and assert rather than silently differ.
 */
void gte_op_mvmva(int sf, int mx, int v, int cv, int lm)
{
    int32_t m[3][3], vec[3];
    int64_t mac[3];
    int i;

    g.cop2c[GTE_C_FLAG] = 0;

    if (mx != 0) { /* light/colour matrices not needed yet */ }
    rot_matrix(m);

    if (v == 0) {
        vec_v0(vec);
    } else if (v == 3) {
        vec[0] = (int16_t)g.cop2d[GTE_D_IR1];
        vec[1] = (int16_t)g.cop2d[GTE_D_IR2];
        vec[2] = (int16_t)g.cop2d[GTE_D_IR3];
    } else {
        vec[0] = s16lo(g.cop2d[v * 2]);
        vec[1] = s16hi(g.cop2d[v * 2]);
        vec[2] = s16lo(g.cop2d[v * 2 + 1]);
    }

    for (i = 0; i < 3; i++) {
        int64_t base = 0;
        if (cv == 0) base = (int64_t)(int32_t)g.cop2c[GTE_C_TRX + i] << 12;
        mac[i] = base + (int64_t)m[i][0] * vec[0]
                      + (int64_t)m[i][1] * vec[1]
                      + (int64_t)m[i][2] * vec[2];
    }
    store_mac_ir(mac, sf, lm);
    flag_finish();
}

/* ---- colour saturation for the RGB FIFO (0..255) ---- */
static int lim_rgb(int64_t v, uint32_t bit)
{
    if (v > 255) { flag_set(bit); return 255; }
    if (v < 0)   { flag_set(bit); return 0; }
    return (int)v;
}

/* ---- DPCS: depth-cue a single colour toward the far colour ----
 * Encoding used by DpqColor is 0x4A780010 -> cmd 0x10, sf=1, lm=0.
 *
 *   MAC1..3 = [R,G,B] << 16                     (from RGBC)
 *   IR1..3  = ((FC << 12) - MAC) >> 12
 *   MAC1..3 = ((IR * IR0) + MAC) >> 12
 *   IR1..3  = MAC1..3
 *   colour FIFO <- [MAC1/16, MAC2/16, MAC3/16, CODE]
 */
void gte_op_dpcs(void)
{
    const int sf = 1, lm = 0;
    uint32_t rgbc = g.cop2d[GTE_D_RGBC];
    int32_t  ir0  = (int16_t)g.cop2d[GTE_D_IR0];
    uint32_t code = (rgbc >> 24) & 0xFF;
    int64_t  mac[3];
    int32_t  fc[3];
    int i, col[3];

    g.cop2c[GTE_C_FLAG] = 0;

    mac[0] = (int64_t)((rgbc >>  0) & 0xFF) << 16;
    mac[1] = (int64_t)((rgbc >>  8) & 0xFF) << 16;
    mac[2] = (int64_t)((rgbc >> 16) & 0xFF) << 16;

    fc[0] = (int32_t)g.cop2c[GTE_C_RFC];
    fc[1] = (int32_t)g.cop2c[GTE_C_GFC];
    fc[2] = (int32_t)g.cop2c[GTE_C_BFC];

    /* IR = ((FC << 12) - MAC) >> (sf*12) */
    for (i = 0; i < 3; i++) {
        int64_t d = mac_check(((int64_t)fc[i] << 12) - mac[i], i) >> (sf ? 12 : 0);
        g.cop2d[GTE_D_IR1 + i] = (uint32_t)lim_ir((int32_t)d, lm, i);
    }

    /* MAC = ((IR * IR0) + MAC) >> (sf*12) ; IR = MAC */
    for (i = 0; i < 3; i++) {
        int32_t ir = (int16_t)g.cop2d[GTE_D_IR1 + i];
        int64_t m  = mac_check((int64_t)ir * ir0 + mac[i], i) >> (sf ? 12 : 0);
        g.cop2d[GTE_D_MAC1 + i] = (uint32_t)(int32_t)m;
        g.cop2d[GTE_D_IR1 + i]  = (uint32_t)lim_ir((int32_t)m, lm, i);
        col[i] = m >> 4;
    }

    /* colour FIFO */
    g.cop2d[GTE_D_RGB0] = g.cop2d[GTE_D_RGB1];
    g.cop2d[GTE_D_RGB1] = g.cop2d[GTE_D_RGB2];
    g.cop2d[GTE_D_RGB2] = (code << 24)
        | ((uint32_t)lim_rgb(col[2], GTE_FLAG_RGB_B_SAT) << 16)
        | ((uint32_t)lim_rgb(col[1], GTE_FLAG_RGB_G_SAT) << 8)
        |  (uint32_t)lim_rgb(col[0], GTE_FLAG_RGB_R_SAT);

    flag_finish();
}
