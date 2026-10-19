// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_VECTOR_H_TYPES
#define VS_CORE_MATH_VECTOR_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_MATH_VECTOR_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_VECTOR_H_DECLS)
#define VS_CORE_MATH_VECTOR_H_DECLS

int Core_Math_AddPackedScreenCoordinates(int arg0, int arg1);
VECTOR* Core_Math_AddVectors(VECTOR* arg0, VECTOR* arg1, VECTOR* arg2);
VECTOR* Core_Math_SubtractVectors(VECTOR* arg0, VECTOR* arg1, VECTOR* arg2);
VECTOR* Core_Math_CopyVector(VECTOR* arg0, VECTOR* arg1);
int Core_Math_GreatestMagnitudeComponent(int arg0, int arg1, int arg2);
int Core_Math_LargerMagnitude(int arg0, int arg1);
void Core_Math_AddShortAndWideVectors(SVECTOR* svec, VECTOR* vec, VECTOR* out);
void Core_Math_AddWideVectors(VECTOR* arg0, VECTOR* arg1, VECTOR* arg2);
void Core_Math_AddWideAndShortVectors(VECTOR* arg0, SVECTOR* arg1, VECTOR* arg2);
void Core_Math_WidenShortVector(SVECTOR* arg0, VECTOR* arg1);
void Core_Math_NarrowWideVector(VECTOR* arg0, SVECTOR* arg1);
unsigned int Core_Math_SquaredTileDistance(unsigned int, unsigned int);
int Core_Math_SquaredTileDistanceFromComponents();

#endif // VS_CORE_MATH_VECTOR_H_DECLS
