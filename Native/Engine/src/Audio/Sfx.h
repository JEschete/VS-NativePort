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

#endif // VS_AUDIO_SFX_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SFX_H_DECLS)
#define VS_AUDIO_SFX_H_DECLS

extern int Audio_nextSfxChannelGroup1;
extern int Audio_nextSfxChannelGroup7E;
extern int Audio_nextSfxChannelGroup180;
extern int Audio_nextSfxChannelGroup200;
extern int Audio_nextSfxChannelGroupFF000;
extern int Audio_nextSfxChannelGroupF00000;
void Audio_PlaySfxOnPrimaryChannel(int arg0);
void Audio_PlayBuiltInSfxChannel(int id, int arg1, int arg2, int arg3);
void Audio_PlaySfxProgramChannel(void* data, int arg1, int arg2, int arg3);
void Audio_PlaySfx(int arg0, long id, int arg2, int arg3);
void Audio_PlaySfxCentered(int arg0, int id);
int Audio_SfxPanAndVolume(int coords, int z);
void Audio_PlayProjectedSfx(int arg0, int arg1, int coords, int z);
void Audio_PlayWorldSfx(int arg0, int arg1, SVECTOR* vec);
int Audio_ResolveSfxProgram(int arg0, int arg1);
void Audio_FadeSfxVolume(int arg0, int arg1, int arg2, char arg3);
void Audio_FadeSfxPitch(int arg0, int arg1, int arg2, char arg3);
void Audio_FadeSfxPan(int arg0, int arg1, int arg2, char arg3);
void Audio_StopSfx(int arg0, int arg1);
void Audio_StopAllSfxPlayback(void);
void Audio_PlayBankSfx(int arg0, u_int arg1[], u_int arg2, int arg3, int arg4);
void Audio_PlayBankSfxCentered(int arg0, int* arg1, u_int arg2);
void Audio_PlayProjectedBankSfx(int arg0, u_int arg1[], int arg2, int arg3, int arg4);
void Audio_PlayWorldBankSfx(int arg0, u_int* arg1, int arg2, SVECTOR* vec);
void Audio_StopBankSfx(int arg0, int* arg1, int arg2);
void Audio_FadeBankSfxVolume(int arg0, int* arg1, int arg2, int arg3, int arg4);

#endif // VS_AUDIO_SFX_H_DECLS
