// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg082.h"

// decomp: D_800FD088 (D_800FD088@src/EFFECT/particleSetup.h)
int Effects_Plg082_particleEmissionCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_800FD08C (D_800FD08C@src/EFFECT/closedRings.h)
CVECTOR Effects_Plg082_closedRingColorFactors[1]; /* contents: module image (ModuleData.cpp) */

}  // extern "C"
