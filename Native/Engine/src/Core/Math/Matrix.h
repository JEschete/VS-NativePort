// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_MATRIX_H_TYPES
#define VS_CORE_MATH_MATRIX_H_TYPES
#include "Legacy/Psx.h"


// decomp: BATTLE.PRG func_800B16F4 (src/BATTLE/BATTLE.PRG/48C7C.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_LookupRotationTrigQuadrant Core_Math_LookupRotationSineCosine

#endif // VS_CORE_MATH_MATRIX_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_MATRIX_H_DECLS)
#define VS_CORE_MATH_MATRIX_H_DECLS

void Core_Math_BuildLookAtOrientation(VECTOR* from, VECTOR* to, MATRIX* mat);
void Core_Math_DirectionRotationMatrix(VECTOR* direction_q12, MATRIX* rotation);
void Core_Math_SetIdentityRotation(MATRIX* matrix);
void Core_Math_SetTranslationMatrix(SVECTOR* translation, MATRIX* matrix);

#endif // VS_CORE_MATH_MATRIX_H_DECLS
