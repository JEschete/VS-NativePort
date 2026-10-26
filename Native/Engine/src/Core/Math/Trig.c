// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Trig.h"

// decomp: BATTLE.PRG func_800988BC (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Core_Math_LookupRotationSineCosine()
{
    VS_UNPORTED("Core::Math::LookupRotationSineCosine");
    return 0;
}

// decomp: BATTLE.PRG _atan2FixedPoint (src/BATTLE/BATTLE.PRG/4A0A8.c:4177)
int Core_Math_FixedAtan2(int arg0, int arg1) { return Core_Gte_Atan2(arg0 >> 6, arg1 >> 6); }

