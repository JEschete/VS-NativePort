// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_INTERPOLATION_H_TYPES
#define VS_CORE_MATH_INTERPOLATION_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_MATH_INTERPOLATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_INTERPOLATION_H_DECLS)
#define VS_CORE_MATH_INTERPOLATION_H_DECLS

extern u_short Core_kSineTable[1674];
int Core_Math_InterpolateSevenBitScalar(int arg0, int arg1, int arg2);
int Core_Math_InterpolateCosineScalar(int arg0, int arg1, int arg2, int arg3);
void Core_Math_InterpolatePairsToWideVector(short* src, int t, VECTOR* vec);
void Core_Math_InterpolatePairsToShortVector(short* src, int t, SVECTOR* vec);
void Core_Math_InterpolatePairsToPlaneVector(short* src, int t, int* vec);
int Core_Math_InterpolateShortPair(short* arg0, int arg1);
int Core_Math_InterpolateFixedPointRatio(int arg0, int arg1, int arg2, int arg3);
void Core_Math_InterpolateLagrangeShortVector(
    SVECTOR* p0, SVECTOR* p1, SVECTOR* p2, SVECTOR* p3, int t, SVECTOR* out);
void Core_Math_InterpolateLagrangeScalar(int arg0, int arg1, int arg2, int arg3, int arg4, int* arg5);
int Core_Math_InterpolateThirteenBitScalar(int arg0, int arg1, int arg2);
void Core_Math_InterpolateThirteenBitShortVector(SVECTOR* arg0, SVECTOR* arg1, int arg2, SVECTOR* arg3);
void Core_Math_InterpolateCosineShortVector(SVECTOR* arg0, SVECTOR* arg1, int arg2, int arg3, SVECTOR* arg4);
void Core_Math_InterpolateTimedShortVector(SVECTOR* arg0, SVECTOR* arg1, int arg2, int arg3, SVECTOR* arg4);
void Core_Math_InterpolateThirteenBitWideVector(VECTOR* arg0, VECTOR* arg1, int arg2, VECTOR* arg3);
void Core_Math_InterpolateCosineWideVector(VECTOR* arg0, VECTOR* arg1, int arg2, int arg3, VECTOR* arg4);
void Core_Math_InterpolateTimedWideVector(VECTOR* arg0, VECTOR* arg1, int arg2, int arg3, VECTOR* arg4);
int Core_Math_InterpolateCosineTimedScalar(int arg0, int arg1, int arg2, int arg3);
int Core_Math_InterpolateTimedScalar(int arg0, int arg1, int arg2, int arg3);
int Core_Math_SineFromTable(int angle);
int Core_Math_EvaluateEase(short mode, short currentStep, short totalSteps);

#endif // VS_CORE_MATH_INTERPOLATION_H_DECLS
