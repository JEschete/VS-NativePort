// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Math/Trig.h"

// decomp: BATTLE.PRG _atan2FixedPoint (src/BATTLE/BATTLE.PRG/4A0A8.c:4177)
int Core_Math_FixedAtan2(int y, int x) { return Core_Gte_Atan2(y >> 6, x >> 6); }

}  // extern "C"
