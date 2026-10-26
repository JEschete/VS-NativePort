// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_MATRIX_H_TYPES
#define VS_CORE_MATH_MATRIX_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_MATH_MATRIX_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_MATRIX_H_DECLS)
#define VS_CORE_MATH_MATRIX_H_DECLS

void Core_Math_BuildLookAtOrientation(VECTOR* from, VECTOR* to, MATRIX* mat);
int Core_Math_BuildEulerRotationMatrix();
int Core_Math_LookupRotationTrigQuadrant();
void Core_Math_DirectionRotationMatrix(VECTOR* arg0, MATRIX* arg1);
void Core_Math_SetIdentityRotation(MATRIX* arg0);
void Core_Math_SetTranslationMatrix(SVECTOR* arg0, MATRIX* arg1);

#endif // VS_CORE_MATH_MATRIX_H_DECLS
