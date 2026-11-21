#pragma once
// The legacy GPU frame state the copied draw code still writes: each
// framebuffer's ordering table and primitive buffer, the ordering table being
// filled and the mask-bit packets.
#include <cstddef>
#include <cstdint>

#include "Legacy/Render/LibGpu.hpp"
#include "Render/DisplayControl.hpp"

namespace VS::Render {
inline constexpr std::size_t kMaskBitOffPacketCount = 8;
inline constexpr std::size_t kMaskBitOnPacketCount = 6;

// decomp: D_800F1870 (D_800F1870@BATTLE/BATTLE.PRG)
extern void* legacyCurrentOrderingTable;
// decomp: D_800F1970 (D_800F1970@BATTLE/BATTLE.PRG)
extern VS::Legacy::DR_STP legacyMaskBitOffPackets[kMaskBitOffPacketCount];
// decomp: D_800F1988 (D_800F1988@BATTLE/BATTLE.PRG)
extern VS::Legacy::DR_STP legacyMaskBitOnPackets[kMaskBitOnPacketCount];
// decomp: D_80055C80 (D_80055C80@SLUS_010.40)
extern void* legacyOrderingTables[kFrameBufferCount];
// decomp: D_8005E0C0 (D_8005E0C0@SLUS_010.40)
extern void* legacyPrimitiveBuffers[kFrameBufferCount];

// The memory a frame buffer's packets are built in. Setting it through these
// functions keeps the pointer and its frame-relative links
// (Legacy::BindGpuLinkRegion) together: links into the memory stay valid
// only until the buffer's next frame begins, and none survive its release.
enum class FrameMemory : std::uint8_t { kPrimitives, kOrderingTable };

// Sets frame `buffer`'s primitive buffer or ordering table to `bytes` at
// `allocation`; nullptr clears it. A buffer outside the two frame buffers is
// refused.
void SetLegacyFrameMemory(FrameMemory memory, std::size_t buffer,
                          void* allocation, std::size_t bytes);
// Clears both frame buffers' memory and ends every link into it.
void ClearLegacyFrameMemory();
// Begins frame `buffer`'s next frame: the links earlier frames issued into
// its memory become stale.
void BeginLegacyFrame(std::size_t buffer);
}  // namespace VS::Render
