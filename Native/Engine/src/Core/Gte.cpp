#include "Core/Gte.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

// Fixed-point math replacing the GTE (plan §6.1). Arithmetic follows the documented GTE behaviour: 4.12
// matrices, 64-bit accumulation, >>12 scaling, saturation of the 16-bit results and the projection divide.
// Rounding and flag details that gameplay may depend on are checked by the GTE parity tests (plan §6.1).

namespace {

struct GteState {
    int16_t rot[3][3] = {{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}};
    int32_t trans[3] = {};
    int32_t ofx = 0, ofy = 0;  // 16.16
    uint16_t h = 1000;
    int16_t dqa = 0;
    int32_t dqb = 0;
    int16_t zsf4 = 0x400;
    int32_t farColor[3] = {};
    int16_t v0[3] = {};
    int16_t sx[3] = {}, sy[3] = {};
    uint16_t sz[4] = {};
    int32_t mac0 = 0, mac[3] = {};
    int16_t ir0 = 0, ir[3] = {};
    uint16_t otz = 0;
    uint32_t flag = 0;
};

GteState g;

int16_t SaturateIr(int64_t v, bool lm = false)
{
    const int64_t lo = lm ? 0 : -0x8000;
    if (v < lo) {
        g.flag |= 1u << 24;
        return static_cast<int16_t>(lo);
    }
    if (v > 0x7FFF) {
        g.flag |= 1u << 24;
        return 0x7FFF;
    }
    return static_cast<int16_t>(v);
}

void PushScreenXY(int16_t x, int16_t y)
{
    g.sx[0] = g.sx[1], g.sy[0] = g.sy[1];
    g.sx[1] = g.sx[2], g.sy[1] = g.sy[2];
    g.sx[2] = x, g.sy[2] = y;
}

void PushZ(uint16_t z)
{
    g.sz[0] = g.sz[1], g.sz[1] = g.sz[2], g.sz[2] = g.sz[3], g.sz[3] = z;
}

// Rotate and translate a vector by the current state: MAC = (T << 12 + R * v) >> 12.
void RotateTranslate(const int16_t v[3])
{
    for (int i = 0; i < 3; ++i) {
        int64_t acc = static_cast<int64_t>(g.trans[i]) * 4096;
        for (int j = 0; j < 3; ++j) {
            acc += static_cast<int64_t>(g.rot[i][j]) * v[j];
        }
        g.mac[i] = static_cast<int32_t>(acc >> 12);
        g.ir[i] = SaturateIr(g.mac[i]);
    }
}

void Project()
{
    const int32_t z = std::clamp(g.mac[2], 0, 0xFFFF);
    if (z != g.mac[2]) {
        g.flag |= 1u << 18;
    }
    PushZ(static_cast<uint16_t>(z));
    int64_t n;
    if (z > 0 && g.h < static_cast<uint32_t>(z) * 2) {
        n = std::min<int64_t>(((static_cast<int64_t>(g.h) * 0x20000 / z) + 1) / 2, 0x1FFFF);
    } else {
        n = 0x1FFFF;
        g.flag |= 1u << 17;
    }
    const int64_t x = (n * g.ir[0] + g.ofx) >> 16;
    const int64_t y = (n * g.ir[1] + g.ofy) >> 16;
    PushScreenXY(static_cast<int16_t>(std::clamp<int64_t>(x, -0x400, 0x3FF)),
                 static_cast<int16_t>(std::clamp<int64_t>(y, -0x400, 0x3FF)));
    g.mac0 = static_cast<int32_t>(n * g.dqa + g.dqb);
    g.ir0 = static_cast<int16_t>(std::clamp(g.mac0 >> 12, 0, 0x1000));
}

long PackXY(int i)
{
    return static_cast<long>((static_cast<uint32_t>(static_cast<uint16_t>(g.sy[i])) << 16) |
                             static_cast<uint16_t>(g.sx[i]));
}

void UnpackXY(long sxy, int16_t& x, int16_t& y)
{
    x = static_cast<int16_t>(sxy & 0xFFFF);
    y = static_cast<int16_t>((static_cast<uint32_t>(sxy) >> 16) & 0xFFFF);
}

void LoadVector(const SVECTOR* v)
{
    g.v0[0] = v->vx, g.v0[1] = v->vy, g.v0[2] = v->vz;
}

void Multiply(const short a[3][3], const short b[3][3], short out[3][3])
{
    short r[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            int64_t acc = 0;
            for (int k = 0; k < 3; ++k) {
                acc += static_cast<int64_t>(a[i][k]) * b[k][j];
            }
            r[i][j] = static_cast<short>(acc >> 12);
        }
    }
    std::copy(&r[0][0], &r[0][0] + 9, &out[0][0]);
}

void AxisRotation(int axis, int angle, short out[3][3])
{
    const short s = static_cast<short>(Core_Gte_Sin(angle));
    const short c = static_cast<short>(Core_Gte_Cos(angle));
    short m[3][3] = {{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}};
    if (axis == 0) {
        m[1][1] = c, m[1][2] = static_cast<short>(-s), m[2][1] = s, m[2][2] = c;
    } else if (axis == 1) {
        m[0][0] = c, m[0][2] = s, m[2][0] = static_cast<short>(-s), m[2][2] = c;
    } else {
        m[0][0] = c, m[0][1] = static_cast<short>(-s), m[1][0] = s, m[1][1] = c;
    }
    std::copy(&m[0][0], &m[0][0] + 9, &out[0][0]);
}

} // namespace

extern "C" {

void Core_Gte_Reset(void)
{
    g = GteState{};
}

void Core_Gte_SetRotMatrix(const MATRIX* m)
{
    std::copy(&m->m[0][0], &m->m[0][0] + 9, &g.rot[0][0]);
}

void Core_Gte_SetTransMatrix(const MATRIX* m)
{
    for (int i = 0; i < 3; ++i) {
        g.trans[i] = static_cast<int32_t>(m->t[i]);
    }
}

void Core_Gte_SetTransVector(const VECTOR* v)
{
    g.trans[0] = static_cast<int32_t>(v->vx);
    g.trans[1] = static_cast<int32_t>(v->vy);
    g.trans[2] = static_cast<int32_t>(v->vz);
}

void Core_Gte_ClearTranslation(void)
{
    g.trans[0] = g.trans[1] = g.trans[2] = 0;
}

void Core_Gte_SetGeomOffset(long ofx, long ofy)
{
    g.ofx = static_cast<int32_t>(ofx) * 65536;
    g.ofy = static_cast<int32_t>(ofy) * 65536;
}

void Core_Gte_SetGeomScreen(long h)
{
    g.h = static_cast<uint16_t>(h);
}

void Core_Gte_SetFarColor(long r, long gg, long b)
{
    g.farColor[0] = static_cast<int32_t>(r) * 16;
    g.farColor[1] = static_cast<int32_t>(gg) * 16;
    g.farColor[2] = static_cast<int32_t>(b) * 16;
}

void Core_Gte_SetFogNear(long a, long h)
{
    // Depth cueing starts at distance `a` with screen distance `h`: interpolation reaches 0 at z = a.
    if (a <= 0) {
        g.dqa = 0, g.dqb = 0;
        return;
    }
    g.dqa = static_cast<int16_t>(std::clamp<long>(-(a * 0x100) / std::max<long>(h, 1), -0x8000, 0x7FFF));
    g.dqb = 0x1400000;
}

void Core_Gte_LoadV0(const SVECTOR* v)
{
    LoadVector(v);
}

void Core_Gte_LoadSxy3(long sxy0, long sxy1, long sxy2)
{
    UnpackXY(sxy0, g.sx[0], g.sy[0]);
    UnpackXY(sxy1, g.sx[1], g.sy[1]);
    UnpackXY(sxy2, g.sx[2], g.sy[2]);
}

void Core_Gte_Rtps(void)
{
    g.flag = 0;
    RotateTranslate(g.v0);
    Project();
}

void Core_Gte_Rtv0Tr(void)
{
    g.flag = 0;
    RotateTranslate(g.v0);
}

void Core_Gte_Nclip(void)
{
    const int64_t r = static_cast<int64_t>(g.sx[0]) * g.sy[1] + static_cast<int64_t>(g.sx[1]) * g.sy[2] +
                      static_cast<int64_t>(g.sx[2]) * g.sy[0] - static_cast<int64_t>(g.sx[0]) * g.sy[2] -
                      static_cast<int64_t>(g.sx[1]) * g.sy[0] - static_cast<int64_t>(g.sx[2]) * g.sy[1];
    g.mac0 = static_cast<int32_t>(r);
}

void Core_Gte_StoreSxy2(long* out)
{
    *out = PackXY(2);
}

void Core_Gte_StoreSz3Otz(long* out)
{
    *out = g.sz[3] >> 2;
}

void Core_Gte_StoreOtz(long* out)
{
    *out = g.otz;
}

void Core_Gte_StoreOpz(long* out)
{
    *out = g.mac0;
}

void Core_Gte_StoreLvnl(VECTOR* out)
{
    out->vx = g.mac[0], out->vy = g.mac[1], out->vz = g.mac[2];
}

void Core_Gte_StoreIr(SVECTOR* out)
{
    out->vx = g.ir[0], out->vy = g.ir[1], out->vz = g.ir[2];
}

MATRIX* Core_Gte_MulMatrix(MATRIX* m0, MATRIX* m1)
{
    Multiply(m0->m, m1->m, m0->m);
    return m0;
}

MATRIX* Core_Gte_CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2)
{
    long t[3];
    for (int i = 0; i < 3; ++i) {
        int64_t acc = 0;
        for (int k = 0; k < 3; ++k) {
            acc += static_cast<int64_t>(m0->m[i][k]) * m1->t[k];
        }
        t[i] = static_cast<long>((acc >> 12) + m0->t[i]);
    }
    Multiply(m0->m, m1->m, m2->m);
    std::copy(t, t + 3, m2->t);
    return m2;
}

VECTOR* Core_Gte_ApplyMatrix(MATRIX* m, SVECTOR* v0, VECTOR* v1)
{
    const int64_t v[3] = {v0->vx, v0->vy, v0->vz};
    long r[3];
    for (int i = 0; i < 3; ++i) {
        r[i] = static_cast<long>((m->m[i][0] * v[0] + m->m[i][1] * v[1] + m->m[i][2] * v[2]) >> 12);
    }
    v1->vx = r[0], v1->vy = r[1], v1->vz = r[2];
    return v1;
}

VECTOR* Core_Gte_ApplyRotMatrix(SVECTOR* v0, VECTOR* v1)
{
    MATRIX m{};
    std::copy(&g.rot[0][0], &g.rot[0][0] + 9, &m.m[0][0]);
    return Core_Gte_ApplyMatrix(&m, v0, v1);
}

VECTOR* Core_Gte_ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1)
{
    const int64_t v[3] = {v0->vx, v0->vy, v0->vz};
    long r[3];
    for (int i = 0; i < 3; ++i) {
        r[i] = static_cast<long>((m->m[i][0] * v[0] + m->m[i][1] * v[1] + m->m[i][2] * v[2]) >> 12);
    }
    v1->vx = r[0], v1->vy = r[1], v1->vz = r[2];
    return v1;
}

SVECTOR* Core_Gte_ApplyMatrixSV(MATRIX* m, SVECTOR* v0, SVECTOR* v1)
{
    const int64_t v[3] = {v0->vx, v0->vy, v0->vz};
    short r[3];
    for (int i = 0; i < 3; ++i) {
        r[i] = SaturateIr((m->m[i][0] * v[0] + m->m[i][1] * v[1] + m->m[i][2] * v[2]) >> 12);
    }
    v1->vx = r[0], v1->vy = r[1], v1->vz = r[2];
    return v1;
}

MATRIX* Core_Gte_RotMatrix(SVECTOR* r, MATRIX* m)
{
    // X, then Y, then Z (the libgte order): M = Rx * Ry * Rz.
    short rx[3][3], ry[3][3], rz[3][3];
    AxisRotation(0, r->vx, rx);
    AxisRotation(1, r->vy, ry);
    AxisRotation(2, r->vz, rz);
    Multiply(rx, ry, m->m);
    Multiply(m->m, rz, m->m);
    return m;
}

MATRIX* Core_Gte_RotMatrixYXZ(SVECTOR* r, MATRIX* m)
{
    short rx[3][3], ry[3][3], rz[3][3];
    AxisRotation(0, r->vx, rx);
    AxisRotation(1, r->vy, ry);
    AxisRotation(2, r->vz, rz);
    Multiply(ry, rx, m->m);
    Multiply(m->m, rz, m->m);
    return m;
}

MATRIX* Core_Gte_TransMatrix(MATRIX* m, VECTOR* v)
{
    m->t[0] = v->vx, m->t[1] = v->vy, m->t[2] = v->vz;
    return m;
}

MATRIX* Core_Gte_ScaleMatrix(MATRIX* m, VECTOR* v)
{
    const long s[3] = {v->vx, v->vy, v->vz};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m->m[i][j] = static_cast<short>((static_cast<int64_t>(m->m[i][j]) * s[j]) >> 12);
        }
    }
    return m;
}

long Core_Gte_RotTransPers(SVECTOR* v0, long* sxy, long* p, long* flag)
{
    LoadVector(v0);
    Core_Gte_Rtps();
    *sxy = PackXY(2);
    *p = g.ir0;
    *flag = static_cast<long>(g.flag);
    return g.sz[3] >> 2;
}

void Core_Gte_RotTrans(SVECTOR* v0, VECTOR* v1, long* flag)
{
    LoadVector(v0);
    Core_Gte_Rtv0Tr();
    v1->vx = g.mac[0], v1->vy = g.mac[1], v1->vz = g.mac[2];
    *flag = static_cast<long>(g.flag);
}

long Core_Gte_RotTransPers4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, long* sxy0, long* sxy1, long* sxy2,
                            long* sxy3, long* p, long* flag)
{
    SVECTOR* v[4] = {v0, v1, v2, v3};
    long* out[4] = {sxy0, sxy1, sxy2, sxy3};
    uint32_t f = 0;
    for (int i = 0; i < 4; ++i) {
        LoadVector(v[i]);
        Core_Gte_Rtps();
        *out[i] = PackXY(2);
        f |= g.flag;
    }
    *p = g.ir0;
    *flag = static_cast<long>(f);
    return g.sz[3] >> 2;
}

long Core_Gte_RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, long* sxy0, long* sxy1, long* sxy2,
                          long* sxy3, long* p, long* flag)
{
    Core_Gte_RotTransPers4(v0, v1, v2, v3, sxy0, sxy1, sxy2, sxy3, p, flag);
    const int64_t sum = static_cast<int64_t>(g.sz[0]) + g.sz[1] + g.sz[2] + g.sz[3];
    g.otz = static_cast<uint16_t>(std::clamp<int64_t>((g.zsf4 * sum) >> 12, 0, 0xFFFF));
    return g.otz;
}

long Core_Gte_NormalClip(long sxy0, long sxy1, long sxy2)
{
    Core_Gte_LoadSxy3(sxy0, sxy1, sxy2);
    Core_Gte_Nclip();
    return g.mac0;
}

void Core_Gte_OuterProduct0(VECTOR* v0, VECTOR* v1, VECTOR* v2)
{
    const int64_t a[3] = {v0->vx, v0->vy, v0->vz}, b[3] = {v1->vx, v1->vy, v1->vz};
    v2->vx = static_cast<long>((a[1] * b[2] - a[2] * b[1]) >> 12);
    v2->vy = static_cast<long>((a[2] * b[0] - a[0] * b[2]) >> 12);
    v2->vz = static_cast<long>((a[0] * b[1] - a[1] * b[0]) >> 12);
}

long Core_Gte_VectorNormal(VECTOR* v0, VECTOR* v1)
{
    const int64_t x = v0->vx, y = v0->vy, z = v0->vz;
    const int64_t sq = x * x + y * y + z * z;
    const double len = std::sqrt(static_cast<double>(sq));
    if (len > 0) {
        v1->vx = static_cast<long>(x * 4096 / len);
        v1->vy = static_cast<long>(y * 4096 / len);
        v1->vz = static_cast<long>(z * 4096 / len);
    } else {
        v1->vx = v1->vy = v1->vz = 0;
    }
    return static_cast<long>(sq);
}

long Core_Gte_VectorNormalSS(SVECTOR* v0, SVECTOR* v1)
{
    VECTOR a{v0->vx, v0->vy, v0->vz, 0}, b{};
    const long sq = Core_Gte_VectorNormal(&a, &b);
    v1->vx = static_cast<short>(b.vx), v1->vy = static_cast<short>(b.vy), v1->vz = static_cast<short>(b.vz);
    return sq;
}

long Core_Gte_SquareRoot12(long a)
{
    return a <= 0 ? 0 : static_cast<long>(std::sqrt(static_cast<double>(a) * 4096.0));
}

void Core_Gte_DpqColor(CVECTOR* v0, long p, CVECTOR* v1)
{
    const unsigned char in[3] = {v0->r, v0->g, v0->b};
    unsigned char out[3];
    for (int i = 0; i < 3; ++i) {
        const int64_t c = static_cast<int64_t>(in[i]) * 16;
        const int64_t r = c + (((g.farColor[i] - c) * p) >> 12);
        out[i] = static_cast<unsigned char>(std::clamp<int64_t>(r >> 4, 0, 255));
    }
    v1->r = out[0], v1->g = out[1], v1->b = out[2], v1->cd = v0->cd;
}

int Core_Gte_Sin(int a)
{
    return static_cast<int>(std::lround(std::sin((a & 0xFFF) * (2.0 * std::numbers::pi / 4096.0)) * 4096.0));
}

int Core_Gte_Cos(int a)
{
    return static_cast<int>(std::lround(std::cos((a & 0xFFF) * (2.0 * std::numbers::pi / 4096.0)) * 4096.0));
}

long Core_Gte_Atan2(long y, long x)
{
    if (x == 0 && y == 0) {
        return 0;
    }
    return std::lround(std::atan2(static_cast<double>(y), static_cast<double>(x)) * (2048.0 / std::numbers::pi));
}

} // extern "C"
