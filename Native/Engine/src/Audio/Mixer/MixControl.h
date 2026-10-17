// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_MIXER_MIXCONTROL_H_TYPES
#define VS_AUDIO_MIXER_MIXCONTROL_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_CommandParams Audio_CommandParams;

// decomp: FSoundCommandParams (src/include/sound.h:501)
typedef struct Audio_CommandParams {
    uintptr_t param1;
    u_int param2;
    u_int param3; // Seems to be usually flags
    u_int param4;
    u_int extParam1;
    u_int extParam2;
} Audio_CommandParams;

// decomp: FSoundCommandHandler (src/SLUS_010.40/25AC.c:77)
typedef void (*Audio_CommandHandler)(Audio_CommandParams*);

#endif // VS_AUDIO_MIXER_MIXCONTROL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_MIXER_MIXCONTROL_H_DECLS)
#define VS_AUDIO_MIXER_MIXCONTROL_H_DECLS

void Audio_Mixer_SetSoundEffectPitchScale(Audio_CommandParams* arg0);
void Audio_Mixer_SlideSoundEffectPitchScale(Audio_CommandParams* arg0);
void Audio_Mixer_SetWorldSoundEffectPitchScale(Audio_CommandParams* arg0);
void Audio_Mixer_SlideWorldSoundEffectPitchScale(Audio_CommandParams* arg0);
void Audio_Mixer_SetTempoScale(signed char* arg0);
void Audio_Mixer_SlideTempoScale(Audio_CommandParams* arg0);
void Audio_Mixer_SetTempoScaleRamp(Audio_CommandParams* arg0);
void Audio_Mixer_SetMusicPitchScale(signed char* arg0);
void Audio_Mixer_SlideMusicPitchScale(Audio_CommandParams* arg0);
void Audio_Mixer_SetMusicPitchScaleRamp(Audio_CommandParams* arg0);
void Audio_Mixer_StopMusicAndCancelReplacement(void);
void Audio_Mixer_StopMatchingMusic(Audio_CommandParams* in_Params);
void Audio_Mixer_StopWorldSoundEffects(void);
void Audio_Mixer_EnableStereoOutput(Audio_CommandParams* in_Params __attribute__((unused)));
void Audio_Mixer_EnableMonoOutput(Audio_CommandParams* in_Params __attribute__((unused)));
void Audio_Mixer_SetMusicMuteMask(Audio_CommandParams* arg0);
void Audio_Mixer_SetMusicJumpThreshold(Audio_CommandParams* arg0);
void Audio_Mixer_PauseMusic(Audio_CommandParams* arg0 __attribute__((unused)));
void Audio_Mixer_ResumeMusic(Audio_CommandParams* in_Params __attribute__((unused)));
void Audio_Mixer_PauseWorldSoundEffects(Audio_CommandParams* in_Params __attribute__((unused)));
void Audio_Mixer_ResumeWorldSoundEffects(Audio_CommandParams* in_Params __attribute__((unused)));
void Audio_Mixer_IgnoreCommand(Audio_CommandParams* in_Params __attribute__((unused)));
void Audio_Mixer_SetReverbMode(int in_ReverbMode);
u_int Audio_Mixer_DispatchCommand(u_int in_Command);

#endif // VS_AUDIO_MIXER_MIXCONTROL_H_DECLS
