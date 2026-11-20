// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.hpp"
#include "Audio/Playback/SoundPlayback.hpp"

// decomp: g_Sound_SfxProgramOffsets (g_Sound_SfxProgramOffsets@SLUS_010.40)
std::uint32_t* VS::Audio::Playback::soundEffectProgramOffsets;

// decomp: g_Sound_SfxMetadataTable (g_Sound_SfxMetadataTable@SLUS_010.40)
std::uint16_t* VS::Audio::Playback::soundEffectBankSelectors;

// decomp: g_Sound_SfxProgramData (g_Sound_SfxProgramData@SLUS_010.40)
std::uint8_t* VS::Audio::Playback::soundEffectProgramData;

