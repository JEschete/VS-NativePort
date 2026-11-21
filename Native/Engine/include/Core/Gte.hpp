#pragma once

// Fixed-point vector/matrix math that replaces the PS1 geometry coprocessor
// (GTE) and its library (libgte), plan §6.1. The copy tool rewrote every
// libgte call and inline GTE macro to these functions (strategy R). The GTE
// kept a small register state between calls (rotation, translation, projection,
// the last vertex), and the copied code relies on it, so that state lives here
// as math state, not as a device (EC-03). The rotate-translate and projection
// arithmetic is Core/Math/Projection.hpp's; migrated callers use it directly.
//
// The vector/matrix types keep their libgte names and layouts; plan §6.1
// replaces them with Core::Vec3s / Mat3x3s.

#include <cstddef>
#include <cstdint>

#include "Core/Math/Projection.hpp"

namespace VS::Core::Gte {

// Compatibility entry points require the Win32 long ABI for 32-bit words.
using Word = long;

struct SVECTOR {
  std::int16_t vx, vy, vz, pad;
};

struct VECTOR {
  Word vx, vy, vz, pad;
};

struct DVECTOR {
  std::int16_t vx, vy;
};

struct CVECTOR {
  std::uint8_t r, g, b, cd;
};

struct MATRIX {
  std::int16_t m[3][3];
  Word t[3];
};

static_assert(sizeof(Word) == 4);
static_assert(sizeof(SVECTOR) == 8 && offsetof(SVECTOR, pad) == 6);
static_assert(sizeof(VECTOR) == 16 && offsetof(VECTOR, pad) == 12);
static_assert(sizeof(DVECTOR) == 4 && offsetof(DVECTOR, vy) == 2);
static_assert(sizeof(CVECTOR) == 4 && offsetof(CVECTOR, cd) == 3);
static_assert(sizeof(MATRIX) == 32 && offsetof(MATRIX, t) == 20);

// ---- state set by the game
void Reset();
void SetRotMatrix(const MATRIX* m);
void SetTransMatrix(const MATRIX* m);
void SetTransVector(const VECTOR* v);
void SetGeomOffset(Word ofx, Word ofy);
void GetGeomOffset(Word* ofx, Word* ofy);
void SetGeomScreen(Word h);
void SetFarColor(Word r, Word g, Word b);
// The far colour as held (each channel times 16), for VS::Render depth cueing.
void GetFarColor(Word* r, Word* g, Word* b);
// The projection cue's coefficient (DQA, stored as a halfword) and offset
// (DQB); VS::Render::SetFogNearDistance computes them.
void SetDepthCue(Word coefficient, Word offset);

// ---- single steps (the inline macros of the copied code)
void LoadV0(const SVECTOR* v);
void LoadSxy3(Word sxy0, Word sxy1, Word sxy2);
void Rtps();    // rotate, translate, project V0
void Rtv0Tr();  // rotate and translate V0, no projection
void Nclip();   // winding of the three screen points
void ClearTranslation();
void StoreSxy2(Word* out);
void StoreSz3Otz(Word* out);  // SZ3 / 4
void StoreOtz(Word* out);
void StoreOpz(Word* out);     // last outer-product / clip result (MAC0)
void StoreLvnl(VECTOR* out);  // last rotate/translate result (MAC1-3)
void StoreIr(SVECTOR* out);   // last saturated result (IR1-3) as shorts

// ---- library functions (libgte)
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);
MATRIX* CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2);
VECTOR* ApplyMatrix(MATRIX* m, SVECTOR* v0, VECTOR* v1);
VECTOR* ApplyRotMatrix(SVECTOR* v0, VECTOR* v1);
VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1);
SVECTOR* ApplyMatrixSV(MATRIX* m, SVECTOR* v0, SVECTOR* v1);
MATRIX* TransMatrix(MATRIX* m, VECTOR* v);
MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
Word RotTransPers(SVECTOR* v0, Word* sxy, Word* p, Word* flag);
void RotTrans(SVECTOR* v0, VECTOR* v1, Word* flag);
Word RotTransPers4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
                   Word* sxy0, Word* sxy1, Word* sxy2, Word* sxy3, Word* p,
                   Word* flag);
Word RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, Word* sxy0,
                 Word* sxy1, Word* sxy2, Word* sxy3, Word* p, Word* flag);
Word NormalClip(Word sxy0, Word sxy1, Word sxy2);

// ---- shared projection (MA-11, EC-03)
// Migrated callers run Math::ProjectVertex and Math::RotateTranslateVertex
// (Core/Math/Projection.hpp) with their own transform and leave the shared
// registers alone. Until every projection producer is migrated, they take the
// projection SetGeomOffset, SetGeomScreen and SetDepthCue left here once per
// draw.
[[nodiscard]] Math::PerspectiveProjection SharedProjection() noexcept;
// Replaces the shared projection (the producer side of SharedProjection).
void SetSharedProjection(
    const Math::PerspectiveProjection& projection) noexcept;

}  // namespace VS::Core::Gte
