#pragma once
// The legacy GPU frame state the copied draw code still writes: each
// framebuffer's ordering table and primitive buffer, the ordering table being
// filled and the mask-bit packets.
#include <cstddef>

#include "Legacy/Render/LibGpu.h"
#include "Render/DisplayControl.hpp"

namespace VS::Render {
inline constexpr std::size_t kMaskBitOffPacketCount = 8;
inline constexpr std::size_t kMaskBitOnPacketCount = 6;

// decomp: D_800F1870 (D_800F1870@BATTLE/BATTLE.PRG)
extern void* legacyCurrentOrderingTable;
// decomp: D_800F1970 (D_800F1970@BATTLE/BATTLE.PRG)
extern DR_STP legacyMaskBitOffPackets[kMaskBitOffPacketCount];
// decomp: D_800F1988 (D_800F1988@BATTLE/BATTLE.PRG)
extern DR_STP legacyMaskBitOnPackets[kMaskBitOnPacketCount];
// decomp: D_80055C80 (D_80055C80@SLUS_010.40)
extern void* legacyOrderingTables[kFrameBufferCount];
// decomp: D_8005E0C0 (D_8005E0C0@SLUS_010.40)
extern void* legacyPrimitiveBuffers[kFrameBufferCount];
}  // namespace VS::Render
