// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg049.h"

// decomp: D_800FE3D4 (D_800FE3D4@EFFECT/PLG049.BIN)
int Effects_Plg049_trailEmissionCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_800FE3D8 (D_800FE3D8@EFFECT/PLG049.BIN)
u_char Effects_Plg049_ringColorFactors[1][4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800FE3F8 (D_800FE3F8@EFFECT/PLG049.BIN)
CVECTOR Effects_Plg049_tiledRingColorFactors[8]; /* contents: module image (ModuleData.cpp) */

}  // extern "C"
