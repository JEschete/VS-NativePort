// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg169.h"

// decomp: D_800FD590 (D_800FD590@EFFECT/PLG169.BIN)
int Effects_Plg169_trailEmissionCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_800FD594 (D_800FD594@EFFECT/PLG169.BIN)
u_char Effects_Plg169_ringColorFactors[1][4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800FD5B4 (D_800FD5B4@EFFECT/PLG169.BIN)
CVECTOR Effects_Plg169_closedRingColorFactors[1]; /* contents: module image (ModuleData.cpp) */

}  // extern "C"
