// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.hpp"
#include "Render/Frame.hpp"

using VS::Legacy::DR_STP;

// decomp: D_800F1870 (D_800F1870@BATTLE/BATTLE.PRG)
void* VS::Render::legacyCurrentOrderingTable;

// decomp: D_800F1970 (D_800F1970@BATTLE/BATTLE.PRG)
DR_STP VS::Render::legacyMaskBitOffPackets[VS::Render::kMaskBitOffPacketCount];

// decomp: D_800F1988 (D_800F1988@BATTLE/BATTLE.PRG)
DR_STP VS::Render::legacyMaskBitOnPackets[VS::Render::kMaskBitOnPacketCount];

// decomp: D_80055C80 (D_80055C80@SLUS_010.40)
void* VS::Render::legacyOrderingTables[VS::Render::kFrameBufferCount];

// decomp: D_8005E0C0 (D_8005E0C0@SLUS_010.40)
void* VS::Render::legacyPrimitiveBuffers[VS::Render::kFrameBufferCount];

