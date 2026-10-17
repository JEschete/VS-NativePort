// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_TRIG_H_TYPES
#define VS_CORE_MATH_TRIG_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_MATH_TRIG_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_TRIG_H_DECLS)
#define VS_CORE_MATH_TRIG_H_DECLS

int Core_Math_LookupRotationSineCosine();
int Core_Math_CosineFixedPoint();
int Core_Math_SineFixedPoint();
int Core_Math_FixedAtan2(int arg0, int arg1);

#endif // VS_CORE_MATH_TRIG_H_DECLS
