#include "Core/Gte.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "Core/Math/Matrix.hpp"
#include "Core/Math/Projection.hpp"

// The libgte bridge (plan §6.1, EC-03): the register state copied callers
// load, execute and store through. RTV0TR and RTPS run Core/Math/Projection's
// explicit arithmetic on that state and leave every result in the registers,
// as the coprocessor did. The remaining operations follow the documented GTE
// behaviour: 4.12 matrices, 64-bit accumulation, >>12 scaling and saturation
// of the 16-bit results.

namespace VS::Core::Gte {
namespace {
namespace Math = VS::Core::Math;

struct GteState {
  Math::Transform3 transform{
      Math::Mat3{{{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}}}, {}};
  Math::PerspectiveProjection projection;
  int16_t zsf4 = 0x400;
  int32_t farColor[3] = {};
  Math::Position3 v0{};
  int16_t sx[3] = {}, sy[3] = {};
  uint16_t sz[4] = {};
  int32_t mac0 = 0, mac[3] = {};
  int16_t ir0 = 0, ir[3] = {};
  uint16_t otz = 0;
  uint32_t flag = 0;
};

GteState g;

// IR1-3 saturation, flagged in FLAG bits 24-22.
int16_t SaturateIr(int64_t v, unsigned flag_bit) {
  if (v < INT16_MIN || v > INT16_MAX) {
    g.flag |= 1u << flag_bit;
  }
  return static_cast<int16_t>(std::clamp<int64_t>(v, INT16_MIN, INT16_MAX));
}

void PushScreenXY(int16_t x, int16_t y) {
  g.sx[0] = g.sx[1], g.sy[0] = g.sy[1];
  g.sx[1] = g.sx[2], g.sy[1] = g.sy[2];
  g.sx[2] = x, g.sy[2] = y;
}

void PushZ(uint16_t z) {
  g.sz[0] = g.sz[1], g.sz[1] = g.sz[2], g.sz[2] = g.sz[3], g.sz[3] = z;
}

// RTV0TR leaves MAC1-3, IR1-3 and FLAG.
void Commit(const Math::RotatedVertex& result) {
  g.mac[0] = result.mac.x, g.mac[1] = result.mac.y, g.mac[2] = result.mac.z;
  std::copy(result.ir.begin(), result.ir.end(), g.ir);
  g.flag = result.flag;
}

// RTPS also pushes SZ3 and SXY2 through their FIFOs and leaves MAC0 and IR0.
void Commit(const Math::PerspectiveVertex& result) {
  g.mac[0] = result.mac.x, g.mac[1] = result.mac.y, g.mac[2] = result.mac.z;
  std::copy(result.ir.begin(), result.ir.end(), g.ir);
  PushZ(result.depth);
  PushScreenXY(result.screen_x, result.screen_y);
  g.mac0 = result.mac0;
  g.ir0 = result.ir0;
  g.flag = result.flag;
}

Word PackXY(int i) {
  return static_cast<Word>(
      (static_cast<uint32_t>(static_cast<uint16_t>(g.sy[i])) << 16) |
      static_cast<uint16_t>(g.sx[i]));
}

void UnpackXY(Word sxy, int16_t& x, int16_t& y) {
  x = static_cast<int16_t>(sxy & 0xFFFF);
  y = static_cast<int16_t>((static_cast<uint32_t>(sxy) >> 16) & 0xFFFF);
}

void LoadVector(const SVECTOR* v) { g.v0 = {v->vx, v->vy, v->vz}; }

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

Math::PerspectiveProjection SharedProjection() noexcept { return g.projection; }

void SetSharedProjection(
    const Math::PerspectiveProjection& projection) noexcept {
  g.projection = projection;
}

void Reset() { g = GteState{}; }

void SetRotMatrix(const MATRIX* m) {
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      g.transform.rotation.m[row][column] = m->m[row][column];
    }
  }
}

void SetTransMatrix(const MATRIX* m) {
  g.transform.translation = {static_cast<int32_t>(m->t[0]),
                             static_cast<int32_t>(m->t[1]),
                             static_cast<int32_t>(m->t[2])};
}

void SetTransVector(const VECTOR* v) {
  g.transform.translation = {static_cast<int32_t>(v->vx),
                             static_cast<int32_t>(v->vy),
                             static_cast<int32_t>(v->vz)};
}

void ClearTranslation() { g.transform.translation = {}; }

void SetGeomOffset(Word ofx, Word ofy) {
  g.projection.offset_x = Math::ScreenOffsetFixed(static_cast<int32_t>(ofx));
  g.projection.offset_y = Math::ScreenOffsetFixed(static_cast<int32_t>(ofy));
}

void GetGeomOffset(Word* ofx, Word* ofy) {
  *ofx = Math::ScreenOffsetPixels(g.projection.offset_x);
  *ofy = Math::ScreenOffsetPixels(g.projection.offset_y);
}

void SetGeomScreen(Word h) { g.projection.distance = static_cast<uint16_t>(h); }

// SDK-compatible entry point; copied callers retain this parameter order.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void SetFarColor(Word r, Word gg, Word b) {
  g.farColor[0] = static_cast<int32_t>(r) * 16;
  g.farColor[1] = static_cast<int32_t>(gg) * 16;
  g.farColor[2] = static_cast<int32_t>(b) * 16;
}

// Mirrors SetFarColor's channel order.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void GetFarColor(Word* r, Word* gg, Word* b) {
  *r = g.farColor[0];
  *gg = g.farColor[1];
  *b = g.farColor[2];
}

// The DQA register holds a halfword, as ctc2 stores it.
void SetDepthCue(Word coefficient, Word offset) {
  g.projection.depth_cue_coefficient = static_cast<int16_t>(coefficient);
  g.projection.depth_cue_offset = static_cast<int32_t>(offset);
}

void LoadV0(const SVECTOR* v) { LoadVector(v); }

void LoadSxy3(Word sxy0, Word sxy1, Word sxy2) {
  UnpackXY(sxy0, g.sx[0], g.sy[0]);
  UnpackXY(sxy1, g.sx[1], g.sy[1]);
  UnpackXY(sxy2, g.sx[2], g.sy[2]);
}

void Rtps() { Commit(Math::ProjectVertex(g.transform, g.projection, g.v0)); }

void Rtv0Tr() { Commit(Math::RotateTranslateVertex(g.transform, g.v0)); }

void Nclip() {
  const int64_t r = static_cast<int64_t>(g.sx[0]) * g.sy[1] +
                    static_cast<int64_t>(g.sx[1]) * g.sy[2] +
                    static_cast<int64_t>(g.sx[2]) * g.sy[0] -
                    static_cast<int64_t>(g.sx[0]) * g.sy[2] -
                    static_cast<int64_t>(g.sx[1]) * g.sy[0] -
                    static_cast<int64_t>(g.sx[2]) * g.sy[1];
  g.mac0 = static_cast<int32_t>(r);
}

void StoreSxy2(Word* out) { *out = PackXY(2); }

void StoreSz3Otz(Word* out) { *out = g.sz[3] >> 2; }

void StoreOtz(Word* out) { *out = g.otz; }

void StoreOpz(Word* out) { *out = g.mac0; }

void StoreLvnl(VECTOR* out) {
  out->vx = g.mac[0], out->vy = g.mac[1], out->vz = g.mac[2];
}

void StoreIr(SVECTOR* out) {
  out->vx = g.ir[0], out->vy = g.ir[1], out->vz = g.ir[2];
}

MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1) {
  Multiply(m0->m, m1->m, m0->m);
  return m0;
}

MATRIX* CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2) {
  Word t[3];
  for (int i = 0; i < 3; ++i) {
    int64_t acc = 0;
    for (int k = 0; k < 3; ++k) {
      acc += static_cast<int64_t>(m0->m[i][k]) * m1->t[k];
    }
    t[i] = static_cast<Word>((acc >> 12) + m0->t[i]);
  }
  Multiply(m0->m, m1->m, m2->m);
  std::copy(t, t + 3, m2->t);
  return m2;
}

VECTOR* ApplyMatrix(MATRIX* m, SVECTOR* v0, VECTOR* v1) {
  VECTOR input{v0->vx, v0->vy, v0->vz, 0};
  return ApplyMatrixLV(m, &input, v1);
}

VECTOR* ApplyRotMatrix(SVECTOR* v0, VECTOR* v1) {
  MATRIX m{};
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      m.m[row][column] = g.transform.rotation.m[row][column];
    }
  }
  SVECTOR result{};
  ApplyMatrixSV(&m, v0, &result);
  v1->vx = result.vx, v1->vy = result.vy, v1->vz = result.vz;
  return v1;
}

VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1) {
  const int64_t v[3] = {v0->vx, v0->vy, v0->vz};
  Word r[3];
  for (int i = 0; i < 3; ++i) {
    r[i] = static_cast<Word>(
        (m->m[i][0] * v[0] + m->m[i][1] * v[1] + m->m[i][2] * v[2]) >> 12);
  }
  v1->vx = r[0], v1->vy = r[1], v1->vz = r[2];
  return v1;
}

SVECTOR* ApplyMatrixSV(MATRIX* m, SVECTOR* v0, SVECTOR* v1) {
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

MATRIX* TransMatrix(MATRIX* m, VECTOR* v) {
  m->t[0] = v->vx, m->t[1] = v->vy, m->t[2] = v->vz;
  return m;
}

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v) {
  const Word s[3] = {v->vx, v->vy, v->vz};
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
Word RotTransPers(SVECTOR* v0, Word* sxy, Word* p, Word* flag) {
  LoadVector(v0);
  Rtps();
  *sxy = PackXY(2);
  *p = g.ir0;
  *flag = static_cast<Word>(g.flag);
  return g.sz[3] >> 2;
}

void RotTrans(SVECTOR* v0, VECTOR* v1, Word* flag) {
  LoadVector(v0);
  Rtv0Tr();
  v1->vx = g.mac[0], v1->vy = g.mac[1], v1->vz = g.mac[2];
  *flag = static_cast<Word>(g.flag);
}

// SDK-compatible entry point; copied callers retain this parameter order.
Word RotTransPers4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
                   // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                   Word* sxy0, Word* sxy1, Word* sxy2, Word* sxy3, Word* p,
                   Word* flag) {
  SVECTOR* v[4] = {v0, v1, v2, v3};
  Word* out[4] = {sxy0, sxy1, sxy2, sxy3};
  uint32_t f = 0;
  for (int i = 0; i < 4; ++i) {
    LoadVector(v[i]);
    Rtps();
    *out[i] = PackXY(2);
    f |= g.flag;
  }
  *p = g.ir0;
  *flag = static_cast<Word>(f);
  return g.sz[3] >> 2;
}

Word RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, Word* sxy0,
                 Word* sxy1, Word* sxy2, Word* sxy3, Word* p, Word* flag) {
  RotTransPers4(v0, v1, v2, v3, sxy0, sxy1, sxy2, sxy3, p, flag);
  const int64_t sum =
      static_cast<int64_t>(g.sz[0]) + g.sz[1] + g.sz[2] + g.sz[3];
  g.otz = static_cast<uint16_t>(
      std::clamp<int64_t>((g.zsf4 * sum) >> 12, 0, 0xFFFF));
  return g.otz;
}

Word NormalClip(Word sxy0, Word sxy1, Word sxy2) {
  LoadSxy3(sxy0, sxy1, sxy2);
  Nclip();
  return g.mac0;
}

}  // namespace VS::Core::Gte
