// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_TRIG_H_TYPES
#define VS_CORE_MATH_TRIG_H_TYPES
#include "Legacy/Psx.h"


// decomp: BATTLE.PRG func_80098AB4 (src/BATTLE/BATTLE.PRG/2EA3C.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_CosineFixedPoint Core_Gte_Cos

// decomp: BATTLE.PRG func_80098AB8 (src/BATTLE/BATTLE.PRG/2EA3C.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Core_Math_SineFixedPoint Core_Gte_Sin

#endif // VS_CORE_MATH_TRIG_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_TRIG_H_DECLS)
#define VS_CORE_MATH_TRIG_H_DECLS

int Core_Math_FixedAtan2(int arg0, int arg1);

#endif // VS_CORE_MATH_TRIG_H_DECLS
