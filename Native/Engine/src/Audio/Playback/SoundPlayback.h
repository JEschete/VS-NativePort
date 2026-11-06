// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_PLAYBACK_SOUNDPLAYBACK_H_TYPES
#define VS_AUDIO_PLAYBACK_SOUNDPLAYBACK_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_SfxProgramHeader Audio_SfxProgramHeader;

// decomp: vs_main_sfxContext (src/SLUS_010.40/25AC.h:9)
typedef struct Audio_SfxProgramHeader {
    int reserved0;
    int reserved4;
    char reserved8;
    char flags;
} Audio_SfxProgramHeader;
_Static_assert(sizeof(Audio_SfxProgramHeader) == 0xC, "Audio_SfxProgramHeader must keep its PS1 layout");

#endif // VS_AUDIO_PLAYBACK_SOUNDPLAYBACK_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_PLAYBACK_SOUNDPLAYBACK_H_DECLS)
#define VS_AUDIO_PLAYBACK_SOUNDPLAYBACK_H_DECLS

extern u_int* Audio_Playback_soundEffectProgramOffsets;
extern u_short* Audio_Playback_soundEffectBankSelectors;
extern char* Audio_Playback_soundEffectProgramData;

#endif // VS_AUDIO_PLAYBACK_SOUNDPLAYBACK_H_DECLS
