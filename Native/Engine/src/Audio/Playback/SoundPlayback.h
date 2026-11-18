// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Audio_SfxProgramHeader;

// decomp: vs_main_sfxContext (src/SLUS_010.40/25AC.h:9)
struct Audio_SfxProgramHeader {
    int reserved0;
    int reserved4;
    char reserved8;
    char flags;
};
static_assert(sizeof(Audio_SfxProgramHeader) == 0xC, "Audio_SfxProgramHeader must keep its PS1 layout");

extern u_int* Audio_Playback_soundEffectProgramOffsets;
extern u_short* Audio_Playback_soundEffectBankSelectors;
extern char* Audio_Playback_soundEffectProgramData;
