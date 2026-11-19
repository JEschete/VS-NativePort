// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Legacy.hpp"

// decomp: D_80094FAC (D_80094FAC@BATTLE/BATTLE.PRG)
int VS::Audio::legacyUnreferencedTable[VS::Audio::kLegacyUnreferencedTableWords]; /* contents: module image (ModuleData.cpp) */

// decomp: D_80094FFC (D_80094FFC@BATTLE/BATTLE.PRG)
int VS::Audio::legacyUnreferencedWord; /* contents: module image (ModuleData.cpp) */

// decomp: g_SpuMallocRecTable (g_SpuMallocRecTable@SLUS_010.40)
u_char VS::Audio::legacySpuAllocationTable[VS::Audio::kLegacySpuAllocationTableBytes];

// decomp: g_bSpuTransferring (g_bSpuTransferring@SLUS_010.40)
volatile int VS::Audio::legacySpuTransferBusy;

