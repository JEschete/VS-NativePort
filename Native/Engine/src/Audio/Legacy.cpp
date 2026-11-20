// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Audio/Legacy.hpp"

#include "Legacy/Psx.hpp"

// decomp: D_80094FAC (D_80094FAC@BATTLE/BATTLE.PRG)
// contents: module image (ModuleData.cpp)
int VS::Audio::legacyUnreferencedTable
    [VS::Audio::kLegacyUnreferencedTableWords];

// decomp: D_80094FFC (D_80094FFC@BATTLE/BATTLE.PRG)
// contents: module image (ModuleData.cpp)
int VS::Audio::legacyUnreferencedWord;

// decomp: g_SpuMallocRecTable (g_SpuMallocRecTable@SLUS_010.40)
std::uint8_t VS::Audio::legacySpuAllocationTable
    [VS::Audio::kLegacySpuAllocationTableBytes];

// decomp: g_bSpuTransferring (g_bSpuTransferring@SLUS_010.40)
volatile int VS::Audio::legacySpuTransferBusy;
