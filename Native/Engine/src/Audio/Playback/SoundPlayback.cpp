// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Playback/SoundPlayback.hpp"

// decomp: g_Sound_SfxProgramOffsets (g_Sound_SfxProgramOffsets@SLUS_010.40)
u_int* VS::Audio::Playback::soundEffectProgramOffsets;

// decomp: g_Sound_SfxMetadataTable (g_Sound_SfxMetadataTable@SLUS_010.40)
u_short* VS::Audio::Playback::soundEffectBankSelectors;

// decomp: g_Sound_SfxProgramData (g_Sound_SfxProgramData@SLUS_010.40)
u_char* VS::Audio::Playback::soundEffectProgramData;

