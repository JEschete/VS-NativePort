// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_VECTOR_H_TYPES
#define VS_CORE_MATH_VECTOR_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_MATH_VECTOR_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_VECTOR_H_DECLS)
#define VS_CORE_MATH_VECTOR_H_DECLS

int Core_Math_AddPackedScreenCoordinates(int left_coordinates, int right_coordinates);
VECTOR* Core_Math_AddVectors(VECTOR* destination, VECTOR* left, VECTOR* right);
VECTOR* Core_Math_SubtractVectors(VECTOR* destination, VECTOR* left, VECTOR* right);
VECTOR* Core_Math_CopyVector(VECTOR* destination, VECTOR* source);
int Core_Math_GreatestMagnitudeComponent(int first, int second, int third);
int Core_Math_LargerMagnitude(int first, int second);
void Core_Math_AddShortAndWideVectors(SVECTOR* svec, VECTOR* vec, VECTOR* out);
void Core_Math_AddWideVectors(VECTOR* left, VECTOR* right, VECTOR* destination);
void Core_Math_AddWideAndShortVectors(VECTOR* wide, SVECTOR* narrow, VECTOR* destination);
void Core_Math_WidenShortVector(SVECTOR* source, VECTOR* destination);
void Core_Math_NarrowWideVector(VECTOR* source, SVECTOR* destination);

#endif // VS_CORE_MATH_VECTOR_H_DECLS
