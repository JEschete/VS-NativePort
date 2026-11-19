#pragma once
// The original's disc file tables for the music, sound-effect bank and wave
// bank files: each file's size and first sector, indexed by file id.
#include <cstdint>

namespace VS::Audio {
// decomp: _musicFileSizes (_musicFileSizes@SLUS_010.40)
extern std::int32_t legacyMusicFileSizes[];
// decomp: _musicLBAs (_musicLBAs@SLUS_010.40)
extern std::int32_t legacyMusicDiscOffsets[];
// decomp: _seFileSizes (_seFileSizes@SLUS_010.40)
extern std::int32_t legacySfxBankFileSizes[];
// decomp: _seLBAs (_seLBAs@SLUS_010.40)
extern std::int32_t legacySfxBankDiscOffsets[];
// decomp: _soundFileSizes (_soundFileSizes@SLUS_010.40)
extern std::int32_t legacyWaveBankFileSizes[];
// decomp: _soundLBAs (_soundLBAs@SLUS_010.40)
extern std::int32_t legacyWaveBankDiscOffsets[];
}  // namespace VS::Audio
