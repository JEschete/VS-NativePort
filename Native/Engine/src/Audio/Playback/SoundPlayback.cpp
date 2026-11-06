// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Playback/SoundPlayback.h"

// decomp: g_Sound_SfxProgramOffsets (g_Sound_SfxProgramOffsets@SLUS_010.40)
u_int* Audio_Playback_soundEffectProgramOffsets;

// decomp: g_Sound_SfxMetadataTable (g_Sound_SfxMetadataTable@SLUS_010.40)
u_short* Audio_Playback_soundEffectBankSelectors;

// decomp: g_Sound_SfxProgramData (g_Sound_SfxProgramData@SLUS_010.40)
char* Audio_Playback_soundEffectProgramData;

}  // extern "C"
