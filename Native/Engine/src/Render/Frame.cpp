// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Render/Frame.h"

// decomp: D_800F1870 (D_800F1870@BATTLE/BATTLE.PRG)
void* Render_legacyCurrentOrderingTable;

// decomp: D_800F1970 (D_800F1970@BATTLE/BATTLE.PRG)
DR_STP Render_legacyMaskBitOffPackets[8];

// decomp: D_800F1988 (D_800F1988@BATTLE/BATTLE.PRG)
DR_STP Render_legacyMaskBitOnPackets[6];

// decomp: D_80055C80 (D_80055C80@SLUS_010.40)
void* Render_legacyOrderingTables[2];

// decomp: D_8005E0C0 (D_8005E0C0@SLUS_010.40)
void* Render_legacyPrimitiveBuffers[2];

