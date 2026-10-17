// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg044.h"

// decomp: D_800FA6F8 (D_800FA6F8@EFFECT/PLG044.BIN)
CVECTOR Effects_Plg044_tiledRingColorFactorsExtern[32 / sizeof(CVECTOR)]; /* contents: module image (ModuleData.c) */

// decomp: D_800FA6F8 (D_800FA6F8@src/EFFECT/PLG044.BIN/0.c)
CVECTOR Effects_Plg044_tiledRingColorFactors[] = { { 0xFF, 0xFF, 0xFF }, { 0x80, 0x80, 0x80 },
    { 0xFF, 0x80, 0x00 }, { 0x00, 0x80, 0xFF }, { 0x00, 0xFF, 0x00 },
    { 0xFF, 0x00, 0xFF }, { 0x00, 0x80, 0x00 }, { 0x80, 0x00, 0x80 } };

