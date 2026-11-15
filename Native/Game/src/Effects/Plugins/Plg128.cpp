// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg128.h"

// decomp: D_801004F0 (D_801004F0@EFFECT/PLG128.BIN)
SVECTOR Effects_Plg128_surfaceVertices[5][16]; /* contents: module image (ModuleData.cpp) */

// decomp: D_80100770 (D_80100770@EFFECT/PLG128.BIN)
CVECTOR Effects_Plg128_closedRingColorFactors[8]; /* contents: module image (ModuleData.cpp) */

// decomp: D_80100790 (D_80100790@EFFECT/PLG128.BIN)
SVECTOR Effects_Plg128_surfaceVertices2[5][16]; /* contents: module image (ModuleData.cpp) */

// decomp: D_80100A10 (D_80100A10@EFFECT/PLG128.BIN)
int Effects_Plg128_trailEmissionCounter; /* contents: module image (ModuleData.cpp) */

}  // extern "C"
