// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundEffects/SoundEffects.h"

// decomp: _cdlAtv (_cdlAtv@SLUS_010.40)
CdlATV Audio_Control_legacyDiscMixAttenuation;

// decomp: g_Sound_CdVolumeFadeStep (g_Sound_CdVolumeFadeStep@SLUS_010.40)
int Audio_Seq_discVolumeFadeStep;

// decomp: g_Sound_CdVolumeFadeLength (g_Sound_CdVolumeFadeLength@SLUS_010.40)
short Audio_Seq_discVolumeFadeLength;

// decomp: g_CdVolume (g_CdVolume@SLUS_010.40)
int Audio_Seq_discVolume;

// decomp: SLUS_010.40 func_80012440 (src/SLUS_010.40/25AC.c:459)
void Audio_Control_SetMusicMuteMask(void* arg0)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Mixer_DispatchCommand(0x90);
}

// decomp: SLUS_010.40 func_80012468 (src/SLUS_010.40/25AC.c:465)
void Audio_Control_SetMusicBranchThreshold(void* arg0)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Mixer_DispatchCommand(0x92);
}

// decomp: SLUS_010.40 func_80012490 (src/SLUS_010.40/25AC.c:471)
void Audio_Control_PausePlaybackGroup(u_int arg0)
{
    switch (arg0) {
    case 1:
        Audio_Mixer_DispatchCommand(0x9B);
        break;
    case 2:
        Audio_Mixer_DispatchCommand(0x9D);
        break;
    case 3:
        Audio_Mixer_DispatchCommand(0x9F);
        break;
    default:
        Audio_Mixer_DispatchCommand(0x99);
        break;
    }
}

// decomp: SLUS_010.40 func_800124FC (src/SLUS_010.40/25AC.c:489)
void Audio_Control_ResumePlaybackGroup(u_int arg0)
{
    switch (arg0) {
    case 1:
        Audio_Mixer_DispatchCommand(0x9A);
        break;
    case 2:
        Audio_Mixer_DispatchCommand(0x9C);
        break;
    case 3:
        Audio_Mixer_DispatchCommand(0x9E);
        break;
    default:
        Audio_Mixer_DispatchCommand(0x98);
        break;
    }
}

// decomp: SLUS_010.40 func_80012568 (src/SLUS_010.40/25AC.c:507)
int Audio_Control_SetMixOptionLatch(int arg0)
{
    if (arg0 != 0) {
        Audio_Driver_controlLatches.mixBehavior |= 0x10;
    } else {
        Audio_Driver_controlLatches.mixBehavior &= ~0x10;
    }
    return 0;
}

// decomp: SLUS_010.40 func_800125A8 (src/SLUS_010.40/25AC.c:517)
void Audio_Control_SetSceneSoundEffectVolume(u_long arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)(arg0 & 0x7F);
    Audio_Mixer_DispatchCommand(0xA8);
}

// decomp: SLUS_010.40 func_800125D4 (src/SLUS_010.40/25AC.c:523)
void Audio_Control_FadeSceneSoundEffectVolume(void* arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0x7F;
    Audio_Mixer_DispatchCommand(0xA9);
}

// decomp: SLUS_010.40 func_80012608 (src/SLUS_010.40/25AC.c:530)
void Audio_Control_SetSoundEffectVolume(void* arg0, int arg1, int arg2)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = arg2 & 0x7F;
    Audio_Mixer_DispatchCommand(0xA0);
}

// decomp: SLUS_010.40 func_8001264C (src/SLUS_010.40/25AC.c:538)
void Audio_Control_FadeSoundEffectVolume(int arg0, int arg1, int arg2, int arg3)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = arg2;
    Audio_Driver_commandArguments.param4 = arg3 & 0x7F;
    Audio_Mixer_DispatchCommand(0xA1);
}

// decomp: SLUS_010.40 func_80012694 (src/SLUS_010.40/25AC.c:547)
void Audio_Control_SetSceneSoundEffectPan(int arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)(arg0 & 0xFF);
    Audio_Mixer_DispatchCommand(0xAA);
}

// decomp: SLUS_010.40 func_800126C0 (src/SLUS_010.40/25AC.c:553)
void Audio_Control_FadeSceneSoundEffectPan(void* arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFF;
    Audio_Mixer_DispatchCommand(0xAB);
}

// decomp: SLUS_010.40 func_800126F4 (src/SLUS_010.40/25AC.c:560)
void Audio_Control_SetSoundEffectPan(void* arg0, int arg1, int arg2)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = arg2 & 0xFF;
    Audio_Mixer_DispatchCommand(0xA2);
}

// decomp: SLUS_010.40 func_80012738 (src/SLUS_010.40/25AC.c:568)
void Audio_Control_FadeSoundEffectPan(int arg0, int arg1, int arg2, int arg3)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = arg2;
    Audio_Driver_commandArguments.param4 = arg3 & 0xFF;
    Audio_Mixer_DispatchCommand(0xA3);
}

// decomp: SLUS_010.40 func_80012780 (src/SLUS_010.40/25AC.c:577)
void Audio_Control_SetSceneSoundEffectPitch(int arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)(arg0 & 0xFF);
    Audio_Mixer_DispatchCommand(0xAC);
}

// decomp: SLUS_010.40 func_800127AC (src/SLUS_010.40/25AC.c:583)
void Audio_Control_FadeSceneSoundEffectPitch(void* arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFF;
    Audio_Mixer_DispatchCommand(0xAD);
}

// decomp: SLUS_010.40 func_800127E0 (src/SLUS_010.40/25AC.c:590)
void Audio_Control_SetSoundEffectPitch(void* arg0, int arg1, int arg2)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = arg2 & 0xFF;
    Audio_Mixer_DispatchCommand(0xA4);
}

// decomp: SLUS_010.40 func_80012824 (src/SLUS_010.40/25AC.c:598)
void Audio_Control_FadeSoundEffectPitch(int arg0, int arg1, int arg2, int arg3)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = arg2;
    Audio_Driver_commandArguments.param4 = arg3 & 0xFF;
    Audio_Mixer_DispatchCommand(0xA5);
}

// decomp: SLUS_010.40 func_8001286C (src/SLUS_010.40/25AC.c:607)
void Audio_Control_SetMusicVolume(u_long arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0x7F;
    Audio_Mixer_DispatchCommand(0xC0);
}

// decomp: SLUS_010.40 func_800128A0 (src/SLUS_010.40/25AC.c:614)
void Audio_Control_FadeMusicVolume(u_long arg0, int arg1, int arg2)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = arg1;
    Audio_Driver_commandArguments.param3 = arg2 & 0x7F;
    Audio_Mixer_DispatchCommand(0xC1);
}

// decomp: SLUS_010.40 func_800128D8 (src/SLUS_010.40/25AC.c:622)
void Audio_Control_FadeMusicVolumeFrom(void* arg0, int arg1, int arg2, int arg3)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1;
    Audio_Driver_commandArguments.param3 = arg2 & 0x7F;
    Audio_Driver_commandArguments.param4 = arg3 & 0x7F;
    Audio_Mixer_DispatchCommand(0xC2);
}

// decomp: SLUS_010.40 func_80012918 (src/SLUS_010.40/25AC.c:631)
void Audio_Control_SetDiscVolume(int arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Mixer_DispatchCommand(0xC8);
}

// decomp: SLUS_010.40 func_80012940 (src/SLUS_010.40/25AC.c:637)
void Audio_Control_FadeDiscVolume(int arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = arg1;
    Audio_Mixer_DispatchCommand(0xC9);
}

// decomp: SLUS_010.40 func_80012970 (src/SLUS_010.40/25AC.c:644)
void Audio_Control_FadeDiscVolumeFrom(void* arg0, int arg1, int arg2)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1;
    Audio_Driver_commandArguments.param3 = arg2;
    Audio_Mixer_DispatchCommand(0xCA);
}

// decomp: SLUS_010.40 func_800129A4 (src/SLUS_010.40/25AC.c:652)
void Audio_Control_SetMusicTempoScale(int arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)(arg0 & 0xFF);
    Audio_Mixer_DispatchCommand(0xD0);
}

// decomp: SLUS_010.40 func_800129D0 (src/SLUS_010.40/25AC.c:658)
void Audio_Control_FadeMusicTempoScale(int arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFF;
    Audio_Mixer_DispatchCommand(0xD1);
}

// decomp: SLUS_010.40 func_80012A04 (src/SLUS_010.40/25AC.c:665)
void Audio_Control_FadeMusicTempoScaleFrom(void* arg0, int arg1, int arg2)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFF;
    Audio_Driver_commandArguments.param3 = arg2 & 0xFF;
    Audio_Mixer_DispatchCommand(0xD2);
}

// decomp: SLUS_010.40 func_80012A40 (src/SLUS_010.40/25AC.c:673)
void Audio_Control_SetMusicPitchScale(int arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)(arg0 & 0xFF);
    Audio_Mixer_DispatchCommand(0xD4);
}

// decomp: SLUS_010.40 func_80012A6C (src/SLUS_010.40/25AC.c:679)
int Audio_Control_FadeMusicPitchScale(int arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFF;
    return Audio_Mixer_DispatchCommand(0xD5);
}

// decomp: SLUS_010.40 func_80012AA0 (src/SLUS_010.40/25AC.c:686)
void Audio_Control_FadeMusicPitchScaleFrom(void* arg0, int arg1, int arg2)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFF;
    Audio_Driver_commandArguments.param3 = arg2 & 0xFF;
    Audio_Mixer_DispatchCommand(0xD6);
}

// decomp: SLUS_010.40 func_80012ADC (src/SLUS_010.40/25AC.c:694)
void Audio_Control_SetMusicTempoAndPitch(int arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)(arg0 & 0xFF);
    Audio_Mixer_DispatchCommand(0xD8);
}

// decomp: SLUS_010.40 func_80012B08 (src/SLUS_010.40/25AC.c:700)
void Audio_Control_FadeMusicTempoAndPitch(void* arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFF;
    Audio_Mixer_DispatchCommand(0xD9);
}

// decomp: SLUS_010.40 func_80012B3C (src/SLUS_010.40/25AC.c:707)
void Audio_Control_FadeMusicTempoAndPitchFrom(void* arg0, int arg1, int arg2)
{
    Audio_Driver_commandArguments.param1 = arg0;
    Audio_Driver_commandArguments.param2 = arg1 & 0xFF;
    Audio_Driver_commandArguments.param3 = arg2 & 0xFF;
    Audio_Mixer_DispatchCommand(0xDA);
}

// decomp: SLUS_010.40 func_80012B78 (src/SLUS_010.40/25AC.c:715)
void Audio_Control_StopAllMusic(void) { Audio_Mixer_DispatchCommand(240); }

// decomp: SLUS_010.40 func_80012B98 (src/SLUS_010.40/25AC.c:717)
void Audio_Control_StopSceneSoundEffects(void) { Audio_Mixer_DispatchCommand(241); }

// decomp: SLUS_010.40 vs_sound_setCdVol (src/SLUS_010.40/25AC.c:913)
int Audio_Control_SetDiscMixVolume(u_int arg0)
{
    if (Audio_Driver_mixBehavior & 2) {
        Audio_Control_legacyDiscMixAttenuation.val0 = Audio_Control_legacyDiscMixAttenuation.val2 = Audio_Control_legacyDiscMixAttenuation.val1 = Audio_Control_legacyDiscMixAttenuation.val3 =
            (arg0 * 0xB570) >> 0x11;
    } else {
        Audio_Control_legacyDiscMixAttenuation.val2 = arg0;
        Audio_Control_legacyDiscMixAttenuation.val0 = arg0;
        Audio_Control_legacyDiscMixAttenuation.val3 = 0;
        Audio_Control_legacyDiscMixAttenuation.val1 = 0;
    }
    return 0;
}

// decomp: SLUS_010.40 func_800132C4 (src/SLUS_010.40/25AC.c:928)
void Audio_Control_StartCutsceneStream(void* arg0, int arg1, int arg2)
{
    if (Audio_Samples_SequenceMagicDifference(arg0) == 0) {
        Audio_Driver_commandArguments.param1 = arg0;
        Audio_Driver_commandArguments.param2 = (arg1 & 0xFF) << 8;
        Audio_Driver_commandArguments.param3 = arg2;
        Audio_Mixer_DispatchCommand(0xE0);
    }
}

// decomp: SLUS_010.40 func_80013328 (src/SLUS_010.40/25AC.c:938)
void Audio_Control_StopCutsceneStream(void) { Audio_Mixer_DispatchCommand(0xE2); }

// decomp: SLUS_010.40 func_80013348 (src/SLUS_010.40/25AC.c:940)
void Audio_Control_SetCutsceneVolume(int arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)((arg0 & 0x7F) << 8);
    Audio_Mixer_DispatchCommand(0xE4);
}

// decomp: SLUS_010.40 func_80013378 (src/SLUS_010.40/25AC.c:946)
void Audio_Control_FadeCutsceneVolume(int arg0, int arg1)
{
    Audio_Driver_commandArguments.param1 = (void*)arg0;
    Audio_Driver_commandArguments.param2 = (arg1 & 0x7F) << 8;
    Audio_Mixer_DispatchCommand(0xE5);
}

// decomp: SLUS_010.40 func_800133B0 (src/SLUS_010.40/25AC.c:953)
void Audio_Control_SetCutscenePan(int arg0)
{
    Audio_Driver_commandArguments.param1 = (void*)((arg0 & 0xFF) << 8);
    Audio_Mixer_DispatchCommand(0xE6);
}

// decomp: SLUS_010.40 func_800133E0 (src/SLUS_010.40/25AC.c:959)
u_int Audio_Control_SoundEffectChannelIds(u_int* arg0)
{
    int i;
    Audio_SeqChannel* var_a1 = Audio_SoundEffects_channels;

    for (i = 0; i < 12; ++i) {
        *arg0++ = (var_a1++)->effectId;
    }
    return Audio_Mixer_voiceScheduler.activeVoices;
}

