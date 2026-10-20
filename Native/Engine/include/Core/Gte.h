#pragma once

// Fixed-point vector/matrix math that replaces the PS1 geometry coprocessor
// (GTE) and its library (libgte), plan §6.1. The copy tool rewrites every
// libgte call and inline GTE macro to these functions (strategy R). The GTE
// kept a small register state between calls (rotation, translation, projection,
// the last vertex), and the copied code relies on it, so that state lives here
// as math state, not as a device.
//
// Vector/matrix types keep their PS1 names and layouts while the code is C;
// plan §6.1 replaces them with Core::Vec3s / Mat3x3s on promotion.

#ifdef __cplusplus
extern "C" {
#endif

// Compatibility entry points require the Win32 long ABI for 32-bit words.
typedef long Core_GteWord;

typedef struct SVECTOR {
  short vx, vy, vz, pad;
} SVECTOR;

typedef struct VECTOR {
  long vx, vy, vz, pad;
} VECTOR;

typedef struct DVECTOR {
  short vx, vy;
} DVECTOR;

typedef struct CVECTOR {
  unsigned char r, g, b, cd;
} CVECTOR;

typedef struct MATRIX {
  short m[3][3];
  long t[3];
} MATRIX;

// ---- state set by the game
void Core_Gte_Reset(void);
void Core_Gte_SetRotMatrix(const MATRIX* m);
void Core_Gte_SetTransMatrix(const MATRIX* m);
void Core_Gte_SetTransVector(const VECTOR* v);
void Core_Gte_SetGeomOffset(long ofx, long ofy);
void Core_Gte_GetGeomOffset(long* ofx, long* ofy);
void Core_Gte_SetGeomScreen(long h);
void Core_Gte_SetFarColor(long r, long g, long b);
void Core_Gte_SetFogNear(long a, long h);

// ---- single steps (the inline macros of the copied code)
void Core_Gte_LoadV0(const SVECTOR* v);
void Core_Gte_LoadSxy3(long sxy0, long sxy1, long sxy2);
void Core_Gte_Rtps(void);    // rotate, translate, project V0
void Core_Gte_Rtv0Tr(void);  // rotate and translate V0, no projection
void Core_Gte_Nclip(void);   // winding of the three screen points
void Core_Gte_ClearTranslation(void);
void Core_Gte_StoreSxy2(long* out);
void Core_Gte_StoreSz3Otz(long* out);  // SZ3 / 4
void Core_Gte_StoreOtz(long* out);
void Core_Gte_StoreOpz(long* out);  // last outer-product / clip result (MAC0)
void Core_Gte_StoreLvnl(VECTOR* out);  // last rotate/translate result (MAC1-3)
void Core_Gte_StoreIr(SVECTOR* out);  // last saturated result (IR1-3) as shorts

// ---- library functions (libgte)
MATRIX* Core_Gte_MulMatrix(MATRIX* m0, MATRIX* m1);
MATRIX* Core_Gte_CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2);
VECTOR* Core_Gte_ApplyMatrix(MATRIX* m, SVECTOR* v0, VECTOR* v1);
VECTOR* Core_Gte_ApplyRotMatrix(SVECTOR* v0, VECTOR* v1);
VECTOR* Core_Gte_ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1);
SVECTOR* Core_Gte_ApplyMatrixSV(MATRIX* m, SVECTOR* v0, SVECTOR* v1);
MATRIX* Core_Gte_RotMatrix(SVECTOR* r, MATRIX* m);
MATRIX* Core_Gte_RotMatrixYXZ(SVECTOR* r, MATRIX* m);
MATRIX* Core_Gte_TransMatrix(MATRIX* m, VECTOR* v);
MATRIX* Core_Gte_ScaleMatrix(MATRIX* m, VECTOR* v);
long Core_Gte_RotTransPers(SVECTOR* v0, long* sxy, long* p, long* flag);
void Core_Gte_RotTrans(SVECTOR* v0, VECTOR* v1, long* flag);
long Core_Gte_RotTransPers4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
                            long* sxy0, long* sxy1, long* sxy2, long* sxy3,
                            long* p, long* flag);
long Core_Gte_RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
                          long* sxy0, long* sxy1, long* sxy2, long* sxy3,
                          long* p, long* flag);
long Core_Gte_NormalClip(long sxy0, long sxy1, long sxy2);
void Core_Gte_OuterProduct0(VECTOR* v0, VECTOR* v1, VECTOR* v2);
long Core_Gte_VectorNormal(VECTOR* v0, VECTOR* v1);
long Core_Gte_VectorNormalSS(SVECTOR* v0, SVECTOR* v1);
long Core_Gte_SquareRoot12(long a);
void Core_Gte_DpqColor(CVECTOR* v0, long p, CVECTOR* v1);
int Core_Gte_Sin(int a);              // rsin: 4096 = full circle, result 4.12
int Core_Gte_Cos(int a);              // rcos
long Core_Gte_Atan2(long y, long x);  // ratan2: result in 4096-per-circle units

#ifdef __cplusplus
}
#endif
