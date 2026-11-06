// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Event/ScriptEffects.h"

// decomp: D_800F4B20 (D_800F4B20@BATTLE/BATTLE.PRG)
u_short Event_scanlineOverlayFrame;

// decomp: D_800F4BB9 (D_800F4BB9@BATTLE/BATTLE.PRG)
u_char Event_isEffectWaitPending;

// decomp: D_800F4C48 (D_800F4C48@BATTLE/BATTLE.PRG)
void* Event_vibrationPatternBuffer;

}  // extern "C"
