// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_FIXEDPOINT_H_TYPES
#define VS_CORE_MATH_FIXEDPOINT_H_TYPES
#include "Legacy/Psx.h"


// decomp: SLUS_010.40 RotMatrixYXZ_gte (src/SLUS_010.40/31724.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_BuildRotationYxz Core_Gte_RotMatrixYXZ

// decomp: SLUS_010.40 RotMatrix_gte (src/SLUS_010.40/31724.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_BuildRotationXyz Core_Gte_RotMatrix

// decomp: SLUS_010.40 func_80041104 (src/SLUS_010.40/31724.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_LookupSineCosinePair Core_Math_LookupRotationSineCosine

// decomp: SLUS_010.40 rcos (src/SLUS_010.40/31724.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_Cosine Core_Gte_Cos

// decomp: SLUS_010.40 rsin (src/SLUS_010.40/31724.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_Sine Core_Gte_Sin

// decomp: SLUS_010.40 OuterProduct0 (src/SLUS_010.40/32154.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_CrossProductFixed Core_Gte_OuterProduct0

// decomp: SLUS_010.40 SquareRoot12 (src/SLUS_010.40/32154.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_SquareRootFixed Core_Gte_SquareRoot12

// decomp: SLUS_010.40 VectorNormal (src/SLUS_010.40/32154.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_NormalizeVector Core_Gte_VectorNormal

// decomp: SLUS_010.40 VectorNormalS (src/SLUS_010.40/32154.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_NormalizeVectorToShort Core_Gte_VectorNormalS

// decomp: SLUS_010.40 VectorNormalSS (src/SLUS_010.40/32154.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_NormalizeShortVector Core_Gte_VectorNormalSS

#endif // VS_CORE_MATH_FIXEDPOINT_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_FIXEDPOINT_H_DECLS)
#define VS_CORE_MATH_FIXEDPOINT_H_DECLS

extern u_char Core_Math_kArcTangentTable[1296];
short Core_Math_ClampSignedShort(short value, int bound_a, int bound_b);
VECTOR* Core_Math_ScaleFixedVector(VECTOR* destination, VECTOR* source, int scale_q12);
int Core_Math_FixedVectorMagnitude(VECTOR* source);
VECTOR* Core_Math_ShortVectorToFixed(VECTOR* destination, SVECTOR* source);
void Core_Math_NormalizeFixedVector(VECTOR* source, VECTOR* destination);
int Core_Math_SplitDecimalDigit(int value);
long Core_Math_ArcTangent2(long y, long x);

#endif // VS_CORE_MATH_FIXEDPOINT_H_DECLS
