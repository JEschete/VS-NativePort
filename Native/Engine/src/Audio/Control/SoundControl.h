// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_CONTROL_SOUNDCONTROL_H_TYPES
#define VS_AUDIO_CONTROL_SOUNDCONTROL_H_TYPES
#include "Legacy/Psx.h"


// decomp: D_800378E2 (D_800378E2@SLUS_010.40): a view into another global
#define Audio_Seq_discVolumeLevel (*(short*)((u_char*)&Audio_Seq_discVolume + 2))

#endif // VS_AUDIO_CONTROL_SOUNDCONTROL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_CONTROL_SOUNDCONTROL_H_DECLS)
#define VS_AUDIO_CONTROL_SOUNDCONTROL_H_DECLS

extern CdlATV Audio_Control_legacyDiscMixAttenuation;
extern int Audio_Seq_discVolumeFadeStep;
extern short Audio_Seq_discVolumeFadeLength;
extern int Audio_Seq_discVolume;
void Audio_Control_SetMusicMuteMask(void* mute_mask);
void Audio_Control_SetMusicBranchThreshold(void* jump_threshold);
void Audio_Control_PausePlaybackGroup(u_int playback_group);
void Audio_Control_ResumePlaybackGroup(u_int playback_group);
int Audio_Control_SetMixOptionLatch(int enabled);
void Audio_Control_SetSceneSoundEffectVolume(u_long volume);
void Audio_Control_FadeSceneSoundEffectVolume(void* fade_ticks, int target_volume);
void Audio_Control_SetSoundEffectVolume(void* effect_selector, int effect_tag_mask, int volume);
void Audio_Control_FadeSoundEffectVolume(intptr_t effect_selector, int effect_tag_mask, int fade_ticks, int target_volume);
void Audio_Control_SetSceneSoundEffectPan(int pan);
void Audio_Control_FadeSceneSoundEffectPan(void* fade_ticks, int target_pan);
void Audio_Control_SetSoundEffectPan(void* effect_selector, int effect_tag_mask, int pan);
void Audio_Control_FadeSoundEffectPan(intptr_t effect_selector, int effect_tag_mask, int fade_ticks, int target_pan);
void Audio_Control_SetSceneSoundEffectPitch(int pitch_scale);
void Audio_Control_FadeSceneSoundEffectPitch(void* fade_ticks, int target_pitch_scale);
void Audio_Control_SetSoundEffectPitch(void* effect_selector, int effect_tag_mask, int pitch_scale);
void Audio_Control_FadeSoundEffectPitch(intptr_t effect_selector, int effect_tag_mask, int fade_ticks, int target_pitch_scale);
void Audio_Control_SetMusicVolume(u_long music_id, int volume);
void Audio_Control_FadeMusicVolume(u_long music_id, int fade_ticks, int target_volume);
void Audio_Control_FadeMusicVolumeFrom(void* music_id, int fade_ticks, int start_volume, int target_volume);
void Audio_Control_SetDiscVolume(int volume);
void Audio_Control_FadeDiscVolume(int fade_ticks, int target_volume);
void Audio_Control_FadeDiscVolumeFrom(void* fade_ticks, int start_volume, int target_volume);
void Audio_Control_SetMusicTempoScale(int tempo_scale);
void Audio_Control_FadeMusicTempoScale(int fade_ticks, int target_tempo_scale);
void Audio_Control_FadeMusicTempoScaleFrom(void* fade_ticks, int start_tempo_scale, int target_tempo_scale);
void Audio_Control_SetMusicPitchScale(int pitch_scale);
int Audio_Control_FadeMusicPitchScale(int fade_ticks, int target_pitch_scale);
void Audio_Control_FadeMusicPitchScaleFrom(void* fade_ticks, int start_pitch_scale, int target_pitch_scale);
void Audio_Control_SetMusicTempoAndPitch(int tempo_pitch_scale);
void Audio_Control_FadeMusicTempoAndPitch(void* fade_ticks, int target_tempo_pitch_scale);
void Audio_Control_FadeMusicTempoAndPitchFrom(void* fade_ticks, int start_tempo_pitch_scale, int target_tempo_pitch_scale);
void Audio_Control_StopAllMusic(void);
void Audio_Control_StopSceneSoundEffects(void);
void Audio_Control_StartCutsceneStream(void* stream_data, int pan, int reverb_enabled);
void Audio_Control_StopCutsceneStream(void);
void Audio_Control_SetCutsceneVolume(int volume);
void Audio_Control_FadeCutsceneVolume(int fade_ticks, int target_volume);
void Audio_Control_SetCutscenePan(int pan);
u_int Audio_Control_SoundEffectChannelIds(intptr_t* output_effect_ids);

#endif // VS_AUDIO_CONTROL_SOUNDCONTROL_H_DECLS
