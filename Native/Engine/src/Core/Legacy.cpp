// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Core/Legacy.hpp"

// D_8005DBF4 was clutState.bgColors (+0x7E9C), owned by
// VS::Render::PaletteAnimation; D_8005E040 was
// VS::Audio::soundState.currentMusicId
// (+8). Both are retired (#445).

// decomp: _buildTimestamp (_buildTimestamp@SLUS_010.40)
char VS::Core::legacyBuildTimestamp[] = "Mar 28 00:09\0\0";

// decomp: _ramsize (_ramsize@SLUS_010.40)
const std::uint32_t VS::Core::kLegacyRamSize = 0x200000;

// decomp: _stacksize (_stacksize@SLUS_010.40)
const std::uint32_t VS::Core::kLegacyStackSize = 0x4000;
