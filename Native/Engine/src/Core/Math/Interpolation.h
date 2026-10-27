// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_INTERPOLATION_H_TYPES
#define VS_CORE_MATH_INTERPOLATION_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_MATH_INTERPOLATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_INTERPOLATION_H_DECLS)
#define VS_CORE_MATH_INTERPOLATION_H_DECLS

extern u_short Core_kSineTable[1674];
int Core_Math_InterpolateSevenBitScalar(int start, int end, int weight_q7);
int Core_Math_InterpolateCosineScalar(int start, int end, int total_steps, int current_step);
void Core_Math_InterpolatePairsToWideVector(short* src, int t, VECTOR* vec);
void Core_Math_InterpolatePairsToShortVector(short* src, int t, SVECTOR* vec);
void Core_Math_InterpolatePairsToPlaneVector(short* src, int t, int* vec);
int Core_Math_InterpolateShortPair(short* endpoints, int weight_q7);
int Core_Math_InterpolateFixedPointRatio(int start, int end, int numerator, int denominator);
void Core_Math_InterpolateLagrangeShortVector(
    SVECTOR* p0, SVECTOR* p1, SVECTOR* p2, SVECTOR* p3, int t, SVECTOR* out);
void Core_Math_InterpolateLagrangeScalar(int value0, int value1, int value2, int value3, int phase_q12, int* destination);
int Core_Math_InterpolateThirteenBitScalar(int start, int end, int weight_q13);
void Core_Math_InterpolateThirteenBitShortVector(SVECTOR* start, SVECTOR* end, int weight_q13, SVECTOR* destination);
void Core_Math_InterpolateCosineShortVector(SVECTOR* start, SVECTOR* end, int total_steps, int current_step, SVECTOR* destination);
void Core_Math_InterpolateTimedShortVector(SVECTOR* start, SVECTOR* end, int total_steps, int current_step, SVECTOR* destination);
void Core_Math_InterpolateThirteenBitWideVector(VECTOR* start, VECTOR* end, int weight_q13, VECTOR* destination);
void Core_Math_InterpolateCosineWideVector(VECTOR* start, VECTOR* end, int total_steps, int current_step, VECTOR* destination);
void Core_Math_InterpolateTimedWideVector(VECTOR* start, VECTOR* end, int total_steps, int current_step, VECTOR* destination);
int Core_Math_InterpolateCosineTimedScalar(int start, int end, int total_steps, int current_step);
int Core_Math_InterpolateTimedScalar(int start, int end, int total_steps, int current_step);
int Core_Math_SineFromTable(int angle);
int Core_Math_EvaluateEase(short mode, short currentStep, short totalSteps);

#endif // VS_CORE_MATH_INTERPOLATION_H_DECLS
