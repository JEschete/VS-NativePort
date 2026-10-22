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
void Audio_Control_SetMusicMuteMask(void* arg0);
void Audio_Control_SetMusicBranchThreshold(void* arg0);
void Audio_Control_PausePlaybackGroup(u_int arg0);
void Audio_Control_ResumePlaybackGroup(u_int arg0);
int Audio_Control_SetMixOptionLatch(int arg0);
void Audio_Control_SetSceneSoundEffectVolume(u_long arg0);
void Audio_Control_FadeSceneSoundEffectVolume(void* arg0, int arg1);
void Audio_Control_SetSoundEffectVolume(void* arg0, int arg1, int arg2);
void Audio_Control_FadeSoundEffectVolume(int arg0, int arg1, int arg2, int arg3);
void Audio_Control_SetSceneSoundEffectPan(int arg0);
void Audio_Control_FadeSceneSoundEffectPan(void* arg0, int arg1);
void Audio_Control_SetSoundEffectPan(void* arg0, int arg1, int arg2);
void Audio_Control_FadeSoundEffectPan(int arg0, int arg1, int arg2, int arg3);
void Audio_Control_SetSceneSoundEffectPitch(int arg0);
void Audio_Control_FadeSceneSoundEffectPitch(void* arg0, int arg1);
void Audio_Control_SetSoundEffectPitch(void* arg0, int arg1, int arg2);
void Audio_Control_FadeSoundEffectPitch(int arg0, int arg1, int arg2, int arg3);
void Audio_Control_SetMusicVolume(u_long arg0, int arg1);
void Audio_Control_FadeMusicVolume(u_long arg0, int arg1, int arg2);
void Audio_Control_FadeMusicVolumeFrom(void* arg0, int arg1, int arg2, int arg3);
void Audio_Control_SetDiscVolume(int arg0);
void Audio_Control_FadeDiscVolume(int arg0, int arg1);
void Audio_Control_FadeDiscVolumeFrom(void* arg0, int arg1, int arg2);
void Audio_Control_SetMusicTempoScale(int arg0);
void Audio_Control_FadeMusicTempoScale(int arg0, int arg1);
void Audio_Control_FadeMusicTempoScaleFrom(void* arg0, int arg1, int arg2);
void Audio_Control_SetMusicPitchScale(int arg0);
int Audio_Control_FadeMusicPitchScale(int arg0, int arg1);
void Audio_Control_FadeMusicPitchScaleFrom(void* arg0, int arg1, int arg2);
void Audio_Control_SetMusicTempoAndPitch(int arg0);
void Audio_Control_FadeMusicTempoAndPitch(void* arg0, int arg1);
void Audio_Control_FadeMusicTempoAndPitchFrom(void* arg0, int arg1, int arg2);
void Audio_Control_StopAllMusic(void);
void Audio_Control_StopSceneSoundEffects(void);
void Audio_Control_StartCutsceneStream(void* arg0, int arg1, int arg2);
void Audio_Control_StopCutsceneStream(void);
void Audio_Control_SetCutsceneVolume(int arg0);
void Audio_Control_FadeCutsceneVolume(int arg0, int arg1);
void Audio_Control_SetCutscenePan(int arg0);
u_int Audio_Control_SoundEffectChannelIds(u_int* arg0);

#endif // VS_AUDIO_CONTROL_SOUNDCONTROL_H_DECLS
