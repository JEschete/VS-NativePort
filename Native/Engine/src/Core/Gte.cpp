#include "Core/Gte.h"

#include <algorithm>
#include <cstdint>

#include "Core/Math/ScalarMath.hpp"

// Fixed-point math replacing the GTE (plan §6.1). Arithmetic follows the
// documented GTE behaviour: 4.12 matrices, 64-bit accumulation, >>12 scaling,
// saturation of the 16-bit results and the projection divide. Rounding and flag
// details that gameplay may depend on are checked by the GTE parity tests (plan
// §6.1).

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

// FLAG bits: MAC1-3 positive/negative overflow (30-28/27-25), IR1-3 (24-22),
// SZ3 (18), divide overflow (17), MAC0 (16/15), SX2/SY2 (14/13) and IR0
// (12). Bit 31 summarizes bits 30-23 and 18-13.
constexpr uint32_t kFlagErrorBits = 0x7F87E000u;
constexpr uint32_t kFlagError = 0x80000000u;
constexpr int64_t kMacLimit = int64_t{1} << 43;

void FinishFlag() {
  if ((g.flag & kFlagErrorBits) != 0) {
    g.flag |= kFlagError;
  }
}

// The GTE keeps MAC1-3 intermediates in signed 44 bits.
int64_t WrapSigned44(int64_t value) {
  constexpr uint64_t kMask = (uint64_t{1} << 44) - 1;
  return static_cast<int64_t>(
             (static_cast<uint64_t>(value) + static_cast<uint64_t>(kMacLimit)) &
             kMask) -
         kMacLimit;
}

int64_t Limit(int64_t v, int64_t lo, int64_t hi, unsigned flag_bit) {
  if (v < lo || v > hi) {
    g.flag |= 1u << flag_bit;
  }
  return std::clamp(v, lo, hi);
}

int16_t SaturateIr(int64_t v, unsigned flag_bit) {
  return static_cast<int16_t>(Limit(v, -0x8000, 0x7FFF, flag_bit));
}

// MAC0 keeps the low word of a wider sum; flag the sum when it leaves the
// signed word range.
void CheckMac0(int64_t v) {
  if (v > INT32_MAX) {
    g.flag |= 1u << 16;
  } else if (v < INT32_MIN) {
    g.flag |= 1u << 15;
  }
}

void PushScreenXY(int16_t x, int16_t y) {
  g.sx[0] = g.sx[1], g.sy[0] = g.sy[1];
  g.sx[1] = g.sx[2], g.sy[1] = g.sy[2];
  g.sx[2] = x, g.sy[2] = y;
}

void PushZ(uint16_t z) {
  g.sz[0] = g.sz[1], g.sz[1] = g.sz[2], g.sz[2] = g.sz[3], g.sz[3] = z;
}

// Rotate and translate a vector by the current state: MAC = (T << 12 + R * v)
// >> 12, each partial sum flagged and wrapped to signed 44 bits.
void RotateTranslate(const int16_t v[3]) {
  for (unsigned i = 0; i < 3; ++i) {
    int64_t acc = static_cast<int64_t>(g.trans[i]) * 4096;
    for (int j = 0; j < 3; ++j) {
      acc += static_cast<int64_t>(g.rot[i][j]) * v[j];
      if (acc >= kMacLimit) {
        g.flag |= 1u << (30 - i);
      } else if (acc < -kMacLimit) {
        g.flag |= 1u << (27 - i);
      }
      acc = WrapSigned44(acc);
    }
    g.mac[i] = static_cast<int32_t>(acc >> 12);
    g.ir[i] = SaturateIr(g.mac[i], 24 - i);
  }
}

// RTPS projection (SDK RotTransPers, libgte/SMP_02.s at 0x80041814): the
// reciprocal divide, screen and depth-cue sums are wider than a word. SX2/SY2
// and IR0 saturate from the wide sums while MAC0 keeps the low word.
void Project() {
  const auto z = static_cast<uint16_t>(Limit(g.mac[2], 0, 0xFFFF, 18));
  PushZ(z);
  if (VS::Core::Math::PerspectiveDivideOverflows(g.h, z)) {
    g.flag |= 1u << 17;
  }
  const int64_t n = VS::Core::Math::DividePerspective(g.h, z);
  const int64_t x = n * g.ir[0] + g.ofx;
  CheckMac0(x);
  const int64_t y = n * g.ir[1] + g.ofy;
  CheckMac0(y);
  PushScreenXY(static_cast<int16_t>(Limit(x >> 16, -0x400, 0x3FF, 14)),
               static_cast<int16_t>(Limit(y >> 16, -0x400, 0x3FF, 13)));
  const int64_t cue = n * g.dqa + g.dqb;
  CheckMac0(cue);
  g.mac0 = static_cast<int32_t>(static_cast<uint32_t>(cue));
  g.ir0 = static_cast<int16_t>(Limit(cue >> 12, 0, 0x1000, 12));
}

long PackXY(int i) {
  return static_cast<long>(
      (static_cast<uint32_t>(static_cast<uint16_t>(g.sy[i])) << 16) |
      static_cast<uint16_t>(g.sx[i]));
}

void UnpackXY(long sxy, int16_t& x, int16_t& y) {
  x = static_cast<int16_t>(sxy & 0xFFFF);
  y = static_cast<int16_t>((static_cast<uint32_t>(sxy) >> 16) & 0xFFFF);
}

void LoadVector(const SVECTOR* v) {
  g.v0[0] = v->vx, g.v0[1] = v->vy, g.v0[2] = v->vz;
}

void Multiply(const short a[3][3], const short b[3][3], short out[3][3]) {
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

}  // namespace

void Core_Gte_Reset(void) { g = GteState{}; }

void Core_Gte_SetRotMatrix(const MATRIX* m) {
  std::copy(&m->m[0][0], &m->m[0][0] + 9, &g.rot[0][0]);
}

void Core_Gte_SetTransMatrix(const MATRIX* m) {
  for (int i = 0; i < 3; ++i) {
    g.trans[i] = static_cast<int32_t>(m->t[i]);
  }
}

void Core_Gte_SetTransVector(const VECTOR* v) {
  g.trans[0] = static_cast<int32_t>(v->vx);
  g.trans[1] = static_cast<int32_t>(v->vy);
  g.trans[2] = static_cast<int32_t>(v->vz);
}

void Core_Gte_ClearTranslation(void) {
  g.trans[0] = g.trans[1] = g.trans[2] = 0;
}

void Core_Gte_SetGeomOffset(long ofx, long ofy) {
  g.ofx = static_cast<int32_t>(static_cast<uint32_t>(ofx) << 16);
  g.ofy = static_cast<int32_t>(static_cast<uint32_t>(ofy) << 16);
}

void Core_Gte_GetGeomOffset(long* ofx, long* ofy) {
  *ofx = g.ofx >> 16;
  *ofy = g.ofy >> 16;
}

void Core_Gte_SetGeomScreen(long h) { g.h = static_cast<uint16_t>(h); }

// SDK-compatible entry point; copied callers retain this parameter order.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Core_Gte_SetFarColor(long r, long gg, long b) {
  g.farColor[0] = static_cast<int32_t>(r) * 16;
  g.farColor[1] = static_cast<int32_t>(gg) * 16;
  g.farColor[2] = static_cast<int32_t>(b) * 16;
}

// Mirrors SetFarColor's channel order.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Core_Gte_GetFarColor(long* r, long* gg, long* b) {
  *r = g.farColor[0];
  *gg = g.farColor[1];
  *b = g.farColor[2];
}

// The DQA register holds a halfword, as ctc2 stores it.
void Core_Gte_SetDepthCue(long coefficient, long offset) {
  g.dqa = static_cast<int16_t>(coefficient);
  g.dqb = static_cast<int32_t>(offset);
}

void Core_Gte_LoadV0(const SVECTOR* v) { LoadVector(v); }

void Core_Gte_LoadSxy3(long sxy0, long sxy1, long sxy2) {
  UnpackXY(sxy0, g.sx[0], g.sy[0]);
  UnpackXY(sxy1, g.sx[1], g.sy[1]);
  UnpackXY(sxy2, g.sx[2], g.sy[2]);
}

void Core_Gte_Rtps(void) {
  g.flag = 0;
  RotateTranslate(g.v0);
  Project();
  FinishFlag();
}

void Core_Gte_Rtv0Tr(void) {
  g.flag = 0;
  RotateTranslate(g.v0);
  FinishFlag();
}

void Core_Gte_Nclip(void) {
  const int64_t r = static_cast<int64_t>(g.sx[0]) * g.sy[1] +
                    static_cast<int64_t>(g.sx[1]) * g.sy[2] +
                    static_cast<int64_t>(g.sx[2]) * g.sy[0] -
                    static_cast<int64_t>(g.sx[0]) * g.sy[2] -
                    static_cast<int64_t>(g.sx[1]) * g.sy[0] -
                    static_cast<int64_t>(g.sx[2]) * g.sy[1];
  g.mac0 = static_cast<int32_t>(r);
}

void Core_Gte_StoreSxy2(long* out) { *out = PackXY(2); }

void Core_Gte_StoreSz3Otz(long* out) { *out = g.sz[3] >> 2; }

void Core_Gte_StoreOtz(long* out) { *out = g.otz; }

void Core_Gte_StoreOpz(long* out) { *out = g.mac0; }

void Core_Gte_StoreLvnl(VECTOR* out) {
  out->vx = g.mac[0], out->vy = g.mac[1], out->vz = g.mac[2];
}

void Core_Gte_StoreIr(SVECTOR* out) {
  out->vx = g.ir[0], out->vy = g.ir[1], out->vz = g.ir[2];
}

MATRIX* Core_Gte_MulMatrix(MATRIX* m0, MATRIX* m1) {
  Multiply(m0->m, m1->m, m0->m);
  return m0;
}

MATRIX* Core_Gte_CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2) {
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

VECTOR* Core_Gte_ApplyMatrix(MATRIX* m, SVECTOR* v0, VECTOR* v1) {
  VECTOR input{v0->vx, v0->vy, v0->vz, 0};
  return Core_Gte_ApplyMatrixLV(m, &input, v1);
}

VECTOR* Core_Gte_ApplyRotMatrix(SVECTOR* v0, VECTOR* v1) {
  MATRIX m{};
  std::copy(&g.rot[0][0], &g.rot[0][0] + 9, &m.m[0][0]);
  SVECTOR result{};
  Core_Gte_ApplyMatrixSV(&m, v0, &result);
  v1->vx = result.vx, v1->vy = result.vy, v1->vz = result.vz;
  return v1;
}

VECTOR* Core_Gte_ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1) {
  const int64_t v[3] = {v0->vx, v0->vy, v0->vz};
  long r[3];
  for (int i = 0; i < 3; ++i) {
    r[i] = static_cast<long>(
        (m->m[i][0] * v[0] + m->m[i][1] * v[1] + m->m[i][2] * v[2]) >> 12);
  }
  v1->vx = r[0], v1->vy = r[1], v1->vz = r[2];
  return v1;
}

SVECTOR* Core_Gte_ApplyMatrixSV(MATRIX* m, SVECTOR* v0, SVECTOR* v1) {
  const int64_t v[3] = {v0->vx, v0->vy, v0->vz};
  short r[3];
  for (int i = 0; i < 3; ++i) {
    r[i] = SaturateIr(
        (m->m[i][0] * v[0] + m->m[i][1] * v[1] + m->m[i][2] * v[2]) >> 12,
        static_cast<unsigned>(24 - i));
  }
  v1->vx = r[0], v1->vy = r[1], v1->vz = r[2];
  return v1;
}

MATRIX* Core_Gte_TransMatrix(MATRIX* m, VECTOR* v) {
  m->t[0] = v->vx, m->t[1] = v->vy, m->t[2] = v->vz;
  return m;
}

MATRIX* Core_Gte_ScaleMatrix(MATRIX* m, VECTOR* v) {
  const long s[3] = {v->vx, v->vy, v->vz};
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      m->m[i][j] =
          static_cast<short>((static_cast<int64_t>(m->m[i][j]) * s[j]) >> 12);
    }
  }
  return m;
}

// SDK-compatible entry point; output pointers have distinct register roles.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
long Core_Gte_RotTransPers(SVECTOR* v0, long* sxy, long* p, long* flag) {
  LoadVector(v0);
  Core_Gte_Rtps();
  *sxy = PackXY(2);
  *p = g.ir0;
  *flag = static_cast<long>(g.flag);
  return g.sz[3] >> 2;
}

void Core_Gte_RotTrans(SVECTOR* v0, VECTOR* v1, long* flag) {
  LoadVector(v0);
  Core_Gte_Rtv0Tr();
  v1->vx = g.mac[0], v1->vy = g.mac[1], v1->vz = g.mac[2];
  *flag = static_cast<long>(g.flag);
}

// SDK-compatible entry point; copied callers retain this parameter order.
long Core_Gte_RotTransPers4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    long* sxy0, long* sxy1, long* sxy2, long* sxy3, long* p, long* flag) {
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

long Core_Gte_RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
                          long* sxy0, long* sxy1, long* sxy2, long* sxy3,
                          long* p, long* flag) {
  Core_Gte_RotTransPers4(v0, v1, v2, v3, sxy0, sxy1, sxy2, sxy3, p, flag);
  const int64_t sum =
      static_cast<int64_t>(g.sz[0]) + g.sz[1] + g.sz[2] + g.sz[3];
  g.otz = static_cast<uint16_t>(
      std::clamp<int64_t>((g.zsf4 * sum) >> 12, 0, 0xFFFF));
  return g.otz;
}

long Core_Gte_NormalClip(long sxy0, long sxy1, long sxy2) {
  Core_Gte_LoadSxy3(sxy0, sxy1, sxy2);
  Core_Gte_Nclip();
  return g.mac0;
}
