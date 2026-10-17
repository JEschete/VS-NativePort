// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Legacy.h"

// decomp: D_80094FAC (D_80094FAC@BATTLE/BATTLE.PRG)
int Audio_legacyUnreferencedTable[795]; /* contents: module image (ModuleData.c) */

// decomp: D_80094FFC (D_80094FFC@BATTLE/BATTLE.PRG)
int Audio_legacyUnreferencedWord; /* contents: module image (ModuleData.c) */

// decomp: g_SpuMallocRecTable (g_SpuMallocRecTable@SLUS_010.40)
char Audio_legacySpuAllocationTable[SPU_MALLOC_RECSIZ * (SPU_MALLOC_NUM_BLOCKS + 1)];

// decomp: g_bSpuTransferring (g_bSpuTransferring@SLUS_010.40)
volatile int Audio_legacySpuTransferBusy;

