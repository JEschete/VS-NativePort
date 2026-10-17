// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg051.h"

// decomp: D_800FA770 (D_800FA770@EFFECT/PLG051.BIN)
CVECTOR Effects_Plg051_closedRingColorFactorsExtern[]; /* data: contents loaded in Step 5 */

// decomp: D_800FA770 (D_800FA770@src/EFFECT/PLG051.BIN/0.c)
CVECTOR Effects_Plg051_closedRingColorFactors[] = { { 0xFF, 0xFF, 0xFF }, { 0x80, 0x80, 0x80 },
    { 0xFF, 0x80, 0x00 }, { 0x00, 0x80, 0xFF }, { 0x00, 0xFF, 0x00 },
    { 0xFF, 0x00, 0xFF }, { 0x00, 0x80, 0x00 }, { 0x80, 0x00, 0x80 } };

