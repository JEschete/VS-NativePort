// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_TYPES
#define VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_TYPES
#include "Legacy/Psx.h"


// decomp: SfxSoundChannels (SfxSoundChannels@SLUS_010.40): a view into another global
#define Audio_SoundEffects_programPairChannels (Audio_SoundEffects_channels + 10)

#endif // VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_DECLS)
#define VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_DECLS
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"

extern Audio_SeqChannel Audio_SoundEffects_channels[22];
void Audio_SoundEffects_ReleaseMatchingVoices(int in_SfxId, int in_StopFlags);
void Audio_SoundEffects_StartChannelProgram(Audio_SeqChannel* in_Channel, Audio_CommandParams* in_CommandParams,
    int in_VoiceBit, char* in_ProgramCounter);
void Audio_SoundEffects_DetachMusicVoice(Audio_SeqChannel* in_Channel, u_int in_Voice);
void Audio_SoundEffects_PlayProgramPair(Audio_CommandParams* in_CommandParams, char* in_ProgramCounter1,
    char* in_ProgramCounter2, int in_SkipRelease);
void Audio_SoundEffects_ProgramCounters(
    char** out_ProgramCounter1, char** out_ProgramCounter2, int in_SfxIndex);
void Audio_SoundEffects_InvalidateMusicChannelVolumes(
    Audio_MusicConfig* in_pChannelConfig, Audio_SeqChannel* in_pChannel);
void Audio_SoundEffects_InvalidateSoundEffectVolumes(void);
void Audio_SoundEffects_PlayInterfaceProgramPair(Audio_CommandParams* arg0);
void Audio_SoundEffects_PlayInterfaceProgram(Audio_CommandParams* arg0);
void Audio_SoundEffects_PlaySharedProgram(Audio_CommandParams* arg0);
void Audio_SoundEffects_PlayBlobPrograms(Audio_CommandParams* in_Params);
void Audio_SoundEffects_StopPrograms(Audio_CommandParams* arg0);
void Audio_SoundEffects_SetProgramVolume(Audio_CommandParams* arg0);
void Audio_SoundEffects_FadeProgramVolume(Audio_CommandParams* arg0);
void Audio_SoundEffects_SetSceneVolume(Audio_CommandParams* arg0);
void Audio_SoundEffects_FadeSceneVolume(Audio_CommandParams* arg0);
void Audio_SoundEffects_SetProgramPan(Audio_CommandParams* arg0);
void Audio_SoundEffects_FadeProgramPan(Audio_CommandParams* arg0);
void Audio_SoundEffects_SetScenePan(Audio_CommandParams* arg0);
void Audio_SoundEffects_FadeScenePan(Audio_CommandParams* arg0);

#endif // VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_DECLS
