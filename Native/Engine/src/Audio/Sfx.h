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
void Audio_PlaySfxOnPrimaryChannel(int effect_id);
void Audio_PlayBuiltInSfxChannel(int id, int effect_tag, int pan, int volume);
void Audio_PlaySfxProgramChannel(void* data, int effect_tag, int pan, int volume);
void Audio_PlaySfx(int channel_group, long id, int pan, int volume);
void Audio_PlaySfxCentered(int channel_group, int id);
int Audio_SfxPanAndVolume(int coords, int z);
void Audio_PlayProjectedSfx(int channel_group, int effect_id, int coords, int z);
void Audio_PlayWorldSfx(int channel_group, int effect_id, SVECTOR* vec);
intptr_t Audio_ResolveSfxProgram(int channel_group, int effect_id);
void Audio_FadeSfxVolume(int channel_group, int effect_id, int target_volume, char fade_ticks);
void Audio_FadeSfxPitch(int channel_group, int effect_id, int target_pitch_scale, char fade_ticks);
void Audio_FadeSfxPan(int channel_group, int effect_id, int target_pan, char fade_ticks);
void Audio_StopSfx(int channel_group, int effect_id);
void Audio_StopAllSfxPlayback(void);
void Audio_PlayBankSfx(int channel_group, u_int sound_bank[], u_int sound_index, int pan, int volume);
void Audio_PlayBankSfxCentered(int channel_group, int* sound_bank, u_int sound_index);
void Audio_PlayProjectedBankSfx(int channel_group, u_int sound_bank[], int sound_index, int projection_or_pan, int projected_depth);
void Audio_PlayWorldBankSfx(int channel_group, u_int* sound_bank, int sound_index, SVECTOR* vec);
void Audio_StopBankSfx(int channel_group, int* sound_bank, int sound_index);
void Audio_FadeBankSfxVolume(int channel_group, int* sound_bank, int sound_index, int target_volume, int fade_ticks);

#endif // VS_AUDIO_SFX_H_DECLS
