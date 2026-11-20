#pragma once
// Sound state the original declared and the port does not read: an
// unreferenced table and word, the libspu allocation table and the SPU
// transfer flag.
#include <cstddef>
#include <cstdint>

#include "Legacy/Audio/LibSpu.hpp"

namespace VS::Audio {
// decomp: SPU_MALLOC_NUM_BLOCKS (src/include/sound.h:545)
inline constexpr std::size_t kLegacySpuAllocationBlocks = 4;
// libspu keeps one record per block plus a terminating record.
inline constexpr std::size_t kLegacySpuAllocationTableBytes(
    (kLegacySpuAllocationBlocks + 1) * VS::Legacy::kSpuMallocRecordSize);
inline constexpr std::size_t kLegacyUnreferencedTableWords = 795;

// decomp: D_80094FAC (D_80094FAC@BATTLE/BATTLE.PRG)
extern std::int32_t legacyUnreferencedTable[kLegacyUnreferencedTableWords];
// decomp: D_80094FFC (D_80094FFC@BATTLE/BATTLE.PRG)
extern std::int32_t legacyUnreferencedWord;
// decomp: g_SpuMallocRecTable (g_SpuMallocRecTable@SLUS_010.40)
extern std::uint8_t legacySpuAllocationTable[kLegacySpuAllocationTableBytes];
// decomp: g_bSpuTransferring (g_bSpuTransferring@SLUS_010.40)
extern volatile std::int32_t legacySpuTransferBusy;
}  // namespace VS::Audio
