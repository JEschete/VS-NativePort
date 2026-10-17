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
int Audio_Playback_Initialize(void);
int Audio_Playback_Shutdown(void);
int Audio_Playback_BindSoundEffectPrograms(void* in_Blob);
int Audio_Playback_ActivePlaybackFlags(void);
int Audio_Playback_MusicVolume(u_int arg0);
int Audio_Playback_PackedMusicSampleBankIds(void);
int Audio_Playback_LongestMusicFade(int arg0);
int Audio_Playback_MusicPosition(void);
u_int Audio_Playback_StartMusic(void* arg0);
void Audio_Playback_StopMusic(u_int arg0);
void Audio_Playback_SaveMusicState(void);
int Audio_Playback_InvalidateSavedMusicId(void);
u_int Audio_Playback_StartLayeredMusic(void* arg0, int arg1);
u_int Audio_Playback_CrossfadeMusic(void* arg0, u_int arg1, int arg2);
void Audio_Playback_StartMusicAtLoop(void* arg0, int arg1);
u_int Audio_Playback_QueueMusicTransition(void* arg0);
void* Audio_Playback_StartMusicPair(void* arg0, int arg1);
void Audio_Playback_RequestMusicLayerSwap(void);
int Audio_Playback_PlaySoundEffect(u_long arg0, int arg1, int arg2, int arg3);
int Audio_Playback_PlaySoundEffectBlob(void* arg0, int arg1, int arg2, int arg3);
void Audio_Playback_StopSoundEffects(u_long arg0, int arg1);
void Audio_Playback_PlayInterfaceSound(u_long arg0);
int Audio_Playback_ActiveSoundEffectTags(void);
int Audio_Playback_SoundEffectIdForTag(int arg0);
int Audio_Playback_UsesSoundEffectTags(Audio_SfxProgramHeader* arg0);
void Audio_Playback_SetMonoOutput(int arg0);

#endif // VS_AUDIO_PLAYBACK_SOUNDPLAYBACK_H_DECLS
