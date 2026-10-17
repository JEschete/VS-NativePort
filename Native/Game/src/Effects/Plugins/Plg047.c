// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg047.h"

// decomp: D_800FA76C (D_800FA76C@EFFECT/PLG047.BIN)
CVECTOR Effects_Plg047_closedRingColorFactorsExtern[]; /* data: contents loaded in Step 5 */

// decomp: D_800FA76C (D_800FA76C@src/EFFECT/PLG047.BIN/0.c)
CVECTOR Effects_Plg047_closedRingColorFactors[] = { { 0xFF, 0xFF, 0xFF }, { 0x80, 0x80, 0x80 },
    { 0xFF, 0x80, 0x00 }, { 0x00, 0x80, 0xFF }, { 0x00, 0xFF, 0x00 },
    { 0xFF, 0x00, 0xFF }, { 0x00, 0x80, 0x00 }, { 0x80, 0x00, 0x80 } };

