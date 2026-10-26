// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Trig.h"

// decomp: BATTLE.PRG _atan2FixedPoint (src/BATTLE/BATTLE.PRG/4A0A8.c:4177)
int Core_Math_FixedAtan2(int arg0, int arg1) { return Core_Gte_Atan2(arg0 >> 6, arg1 >> 6); }

