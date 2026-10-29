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
int Audio_Playback_MusicVolume(u_int music_id);
int Audio_Playback_PackedMusicSampleBankIds(void);
int Audio_Playback_LongestMusicFade(int fade_selector_mask);
int Audio_Playback_MusicPosition(void);
u_int Audio_Playback_StartMusic(void* sequence_data);
void Audio_Playback_StopMusic(u_int music_id);
void Audio_Playback_SaveMusicState(void);
int Audio_Playback_InvalidateSavedMusicId(void);
u_int Audio_Playback_StartLayeredMusic(void* sequence_data, int volume);
u_int Audio_Playback_CrossfadeMusic(void* sequence_data, u_int fade_ticks, int target_volume);
void Audio_Playback_StartMusicAtLoop(void* sequence_data, int loop_count);
u_int Audio_Playback_QueueMusicTransition(void* sequence_data);
void* Audio_Playback_StartMusicPair(void* primary_sequence, int secondary_sequence_address);
void Audio_Playback_RequestMusicLayerSwap(void);
int Audio_Playback_PlaySoundEffect(u_long effect_id, int effect_tag, int pan, int volume);
uintptr_t Audio_Playback_PlaySoundEffectBlob(void* program_blob, int effect_tag, int pan, int volume);
void Audio_Playback_StopSoundEffects(intptr_t effect_selector, int effect_tag_mask);
void Audio_Playback_PlayInterfaceSound(u_long effect_id);
int Audio_Playback_ActiveSoundEffectTags(void);
intptr_t Audio_Playback_SoundEffectIdForTag(int effect_tag);
int Audio_Playback_UsesSoundEffectTags(Audio_SfxProgramHeader* program_or_effect_id);
void Audio_Playback_SetMonoOutput(int mono_enabled);

#endif // VS_AUDIO_PLAYBACK_SOUNDPLAYBACK_H_DECLS
