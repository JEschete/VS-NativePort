// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SFX_H_TYPES
#define VS_AUDIO_SFX_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_SfxSlotRequest Audio_SfxSlotRequest;

// decomp: func_800172D4_t (src/SLUS_010.40/25AC.c:56)
typedef struct Audio_SfxSlotRequest {
    int index;
    int reserved04;
    int reserved08;
    int reserved0C;
    int reserved10;
} Audio_SfxSlotRequest;
_Static_assert(sizeof(Audio_SfxSlotRequest) == 0x14, "Audio_SfxSlotRequest must keep its PS1 layout");

#endif // VS_AUDIO_SFX_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SFX_H_DECLS)
#define VS_AUDIO_SFX_H_DECLS

extern int Audio_nextSfxChannelGroup1;
extern int Audio_nextSfxChannelGroup7E;
extern int Audio_nextSfxChannelGroup180;
extern int Audio_nextSfxChannelGroup200;
extern int Audio_nextSfxChannelGroupFF000;
extern int Audio_nextSfxChannelGroupF00000;

#endif // VS_AUDIO_SFX_H_DECLS
