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
void Audio_Control_SetMusicMuteMask(void* mute_mask)
{
    Audio_Driver_commandArguments.param1 = mute_mask;
    Audio_Mixer_DispatchCommand(0x90);
}

// decomp: SLUS_010.40 func_80012468 (src/SLUS_010.40/25AC.c:465)
void Audio_Control_SetMusicBranchThreshold(void* jump_threshold)
{
    Audio_Driver_commandArguments.param1 = jump_threshold;
    Audio_Mixer_DispatchCommand(0x92);
}

// decomp: SLUS_010.40 func_80012490 (src/SLUS_010.40/25AC.c:471)
void Audio_Control_PausePlaybackGroup(u_int playback_group)
{
    switch (playback_group) {
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
void Audio_Control_ResumePlaybackGroup(u_int playback_group)
{
    switch (playback_group) {
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
int Audio_Control_SetMixOptionLatch(int enabled)
{
    if (enabled != 0) {
        Audio_Driver_controlLatches.mixBehavior |= 0x10;
    } else {
        Audio_Driver_controlLatches.mixBehavior &= ~0x10;
    }
    return 0;
}

// decomp: SLUS_010.40 func_800125A8 (src/SLUS_010.40/25AC.c:517)
void Audio_Control_SetSceneSoundEffectVolume(u_long volume)
{
    Audio_Driver_commandArguments.param1 = (void*)(volume & 0x7F);
    Audio_Mixer_DispatchCommand(0xA8);
}

// decomp: SLUS_010.40 func_800125D4 (src/SLUS_010.40/25AC.c:523)
void Audio_Control_FadeSceneSoundEffectVolume(void* fade_ticks, int target_volume)
{
    Audio_Driver_commandArguments.param1 = fade_ticks;
    Audio_Driver_commandArguments.param2 = target_volume & 0x7F;
    Audio_Mixer_DispatchCommand(0xA9);
}

// decomp: SLUS_010.40 func_80012608 (src/SLUS_010.40/25AC.c:530)
void Audio_Control_SetSoundEffectVolume(void* effect_selector, int effect_tag_mask, int volume)
{
    Audio_Driver_commandArguments.param1 = effect_selector;
    Audio_Driver_commandArguments.param2 = effect_tag_mask & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = volume & 0x7F;
    Audio_Mixer_DispatchCommand(0xA0);
}

// decomp: SLUS_010.40 func_8001264C (src/SLUS_010.40/25AC.c:538)
void Audio_Control_FadeSoundEffectVolume(int effect_selector, int effect_tag_mask, int fade_ticks, int target_volume)
{
    Audio_Driver_commandArguments.param1 = (void*)effect_selector;
    Audio_Driver_commandArguments.param2 = effect_tag_mask & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = fade_ticks;
    Audio_Driver_commandArguments.param4 = target_volume & 0x7F;
    Audio_Mixer_DispatchCommand(0xA1);
}

// decomp: SLUS_010.40 func_80012694 (src/SLUS_010.40/25AC.c:547)
void Audio_Control_SetSceneSoundEffectPan(int pan)
{
    Audio_Driver_commandArguments.param1 = (void*)(pan & 0xFF);
    Audio_Mixer_DispatchCommand(0xAA);
}

// decomp: SLUS_010.40 func_800126C0 (src/SLUS_010.40/25AC.c:553)
void Audio_Control_FadeSceneSoundEffectPan(void* fade_ticks, int target_pan)
{
    Audio_Driver_commandArguments.param1 = fade_ticks;
    Audio_Driver_commandArguments.param2 = target_pan & 0xFF;
    Audio_Mixer_DispatchCommand(0xAB);
}

// decomp: SLUS_010.40 func_800126F4 (src/SLUS_010.40/25AC.c:560)
void Audio_Control_SetSoundEffectPan(void* effect_selector, int effect_tag_mask, int pan)
{
    Audio_Driver_commandArguments.param1 = effect_selector;
    Audio_Driver_commandArguments.param2 = effect_tag_mask & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = pan & 0xFF;
    Audio_Mixer_DispatchCommand(0xA2);
}

// decomp: SLUS_010.40 func_80012738 (src/SLUS_010.40/25AC.c:568)
void Audio_Control_FadeSoundEffectPan(int effect_selector, int effect_tag_mask, int fade_ticks, int target_pan)
{
    Audio_Driver_commandArguments.param1 = (void*)effect_selector;
    Audio_Driver_commandArguments.param2 = effect_tag_mask & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = fade_ticks;
    Audio_Driver_commandArguments.param4 = target_pan & 0xFF;
    Audio_Mixer_DispatchCommand(0xA3);
}

// decomp: SLUS_010.40 func_80012780 (src/SLUS_010.40/25AC.c:577)
void Audio_Control_SetSceneSoundEffectPitch(int pitch_scale)
{
    Audio_Driver_commandArguments.param1 = (void*)(pitch_scale & 0xFF);
    Audio_Mixer_DispatchCommand(0xAC);
}

// decomp: SLUS_010.40 func_800127AC (src/SLUS_010.40/25AC.c:583)
void Audio_Control_FadeSceneSoundEffectPitch(void* fade_ticks, int target_pitch_scale)
{
    Audio_Driver_commandArguments.param1 = fade_ticks;
    Audio_Driver_commandArguments.param2 = target_pitch_scale & 0xFF;
    Audio_Mixer_DispatchCommand(0xAD);
}

// decomp: SLUS_010.40 func_800127E0 (src/SLUS_010.40/25AC.c:590)
void Audio_Control_SetSoundEffectPitch(void* effect_selector, int effect_tag_mask, int pitch_scale)
{
    Audio_Driver_commandArguments.param1 = effect_selector;
    Audio_Driver_commandArguments.param2 = effect_tag_mask & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = pitch_scale & 0xFF;
    Audio_Mixer_DispatchCommand(0xA4);
}

// decomp: SLUS_010.40 func_80012824 (src/SLUS_010.40/25AC.c:598)
void Audio_Control_FadeSoundEffectPitch(int effect_selector, int effect_tag_mask, int fade_ticks, int target_pitch_scale)
{
    Audio_Driver_commandArguments.param1 = (void*)effect_selector;
    Audio_Driver_commandArguments.param2 = effect_tag_mask & 0xFFFFFF;
    Audio_Driver_commandArguments.param3 = fade_ticks;
    Audio_Driver_commandArguments.param4 = target_pitch_scale & 0xFF;
    Audio_Mixer_DispatchCommand(0xA5);
}

// decomp: SLUS_010.40 func_8001286C (src/SLUS_010.40/25AC.c:607)
void Audio_Control_SetMusicVolume(u_long music_id, int volume)
{
    Audio_Driver_commandArguments.param1 = (void*)music_id;
    Audio_Driver_commandArguments.param2 = volume & 0x7F;
    Audio_Mixer_DispatchCommand(0xC0);
}

// decomp: SLUS_010.40 func_800128A0 (src/SLUS_010.40/25AC.c:614)
void Audio_Control_FadeMusicVolume(u_long music_id, int fade_ticks, int target_volume)
{
    Audio_Driver_commandArguments.param1 = (void*)music_id;
    Audio_Driver_commandArguments.param2 = fade_ticks;
    Audio_Driver_commandArguments.param3 = target_volume & 0x7F;
    Audio_Mixer_DispatchCommand(0xC1);
}

// decomp: SLUS_010.40 func_800128D8 (src/SLUS_010.40/25AC.c:622)
void Audio_Control_FadeMusicVolumeFrom(void* music_id, int fade_ticks, int start_volume, int target_volume)
{
    Audio_Driver_commandArguments.param1 = music_id;
    Audio_Driver_commandArguments.param2 = fade_ticks;
    Audio_Driver_commandArguments.param3 = start_volume & 0x7F;
    Audio_Driver_commandArguments.param4 = target_volume & 0x7F;
    Audio_Mixer_DispatchCommand(0xC2);
}

// decomp: SLUS_010.40 func_80012918 (src/SLUS_010.40/25AC.c:631)
void Audio_Control_SetDiscVolume(int volume)
{
    Audio_Driver_commandArguments.param1 = (void*)volume;
    Audio_Mixer_DispatchCommand(0xC8);
}

// decomp: SLUS_010.40 func_80012940 (src/SLUS_010.40/25AC.c:637)
void Audio_Control_FadeDiscVolume(int fade_ticks, int target_volume)
{
    Audio_Driver_commandArguments.param1 = (void*)fade_ticks;
    Audio_Driver_commandArguments.param2 = target_volume;
    Audio_Mixer_DispatchCommand(0xC9);
}

// decomp: SLUS_010.40 func_80012970 (src/SLUS_010.40/25AC.c:644)
void Audio_Control_FadeDiscVolumeFrom(void* fade_ticks, int start_volume, int target_volume)
{
    Audio_Driver_commandArguments.param1 = fade_ticks;
    Audio_Driver_commandArguments.param2 = start_volume;
    Audio_Driver_commandArguments.param3 = target_volume;
    Audio_Mixer_DispatchCommand(0xCA);
}

// decomp: SLUS_010.40 func_800129A4 (src/SLUS_010.40/25AC.c:652)
void Audio_Control_SetMusicTempoScale(int tempo_scale)
{
    Audio_Driver_commandArguments.param1 = (void*)(tempo_scale & 0xFF);
    Audio_Mixer_DispatchCommand(0xD0);
}

// decomp: SLUS_010.40 func_800129D0 (src/SLUS_010.40/25AC.c:658)
void Audio_Control_FadeMusicTempoScale(int fade_ticks, int target_tempo_scale)
{
    Audio_Driver_commandArguments.param1 = (void*)fade_ticks;
    Audio_Driver_commandArguments.param2 = target_tempo_scale & 0xFF;
    Audio_Mixer_DispatchCommand(0xD1);
}

// decomp: SLUS_010.40 func_80012A04 (src/SLUS_010.40/25AC.c:665)
void Audio_Control_FadeMusicTempoScaleFrom(void* fade_ticks, int start_tempo_scale, int target_tempo_scale)
{
    Audio_Driver_commandArguments.param1 = fade_ticks;
    Audio_Driver_commandArguments.param2 = start_tempo_scale & 0xFF;
    Audio_Driver_commandArguments.param3 = target_tempo_scale & 0xFF;
    Audio_Mixer_DispatchCommand(0xD2);
}

// decomp: SLUS_010.40 func_80012A40 (src/SLUS_010.40/25AC.c:673)
void Audio_Control_SetMusicPitchScale(int pitch_scale)
{
    Audio_Driver_commandArguments.param1 = (void*)(pitch_scale & 0xFF);
    Audio_Mixer_DispatchCommand(0xD4);
}

// decomp: SLUS_010.40 func_80012A6C (src/SLUS_010.40/25AC.c:679)
int Audio_Control_FadeMusicPitchScale(int fade_ticks, int target_pitch_scale)
{
    Audio_Driver_commandArguments.param1 = (void*)fade_ticks;
    Audio_Driver_commandArguments.param2 = target_pitch_scale & 0xFF;
    return Audio_Mixer_DispatchCommand(0xD5);
}

// decomp: SLUS_010.40 func_80012AA0 (src/SLUS_010.40/25AC.c:686)
void Audio_Control_FadeMusicPitchScaleFrom(void* fade_ticks, int start_pitch_scale, int target_pitch_scale)
{
    Audio_Driver_commandArguments.param1 = fade_ticks;
    Audio_Driver_commandArguments.param2 = start_pitch_scale & 0xFF;
    Audio_Driver_commandArguments.param3 = target_pitch_scale & 0xFF;
    Audio_Mixer_DispatchCommand(0xD6);
}

// decomp: SLUS_010.40 func_80012ADC (src/SLUS_010.40/25AC.c:694)
void Audio_Control_SetMusicTempoAndPitch(int tempo_pitch_scale)
{
    Audio_Driver_commandArguments.param1 = (void*)(tempo_pitch_scale & 0xFF);
    Audio_Mixer_DispatchCommand(0xD8);
}

// decomp: SLUS_010.40 func_80012B08 (src/SLUS_010.40/25AC.c:700)
void Audio_Control_FadeMusicTempoAndPitch(void* fade_ticks, int target_tempo_pitch_scale)
{
    Audio_Driver_commandArguments.param1 = fade_ticks;
    Audio_Driver_commandArguments.param2 = target_tempo_pitch_scale & 0xFF;
    Audio_Mixer_DispatchCommand(0xD9);
}

// decomp: SLUS_010.40 func_80012B3C (src/SLUS_010.40/25AC.c:707)
void Audio_Control_FadeMusicTempoAndPitchFrom(void* fade_ticks, int start_tempo_pitch_scale, int target_tempo_pitch_scale)
{
    Audio_Driver_commandArguments.param1 = fade_ticks;
    Audio_Driver_commandArguments.param2 = start_tempo_pitch_scale & 0xFF;
    Audio_Driver_commandArguments.param3 = target_tempo_pitch_scale & 0xFF;
    Audio_Mixer_DispatchCommand(0xDA);
}

// decomp: SLUS_010.40 func_80012B78 (src/SLUS_010.40/25AC.c:715)
void Audio_Control_StopAllMusic(void) { Audio_Mixer_DispatchCommand(240); }

// decomp: SLUS_010.40 func_80012B98 (src/SLUS_010.40/25AC.c:717)
void Audio_Control_StopSceneSoundEffects(void) { Audio_Mixer_DispatchCommand(241); }

// decomp: SLUS_010.40 func_800132C4 (src/SLUS_010.40/25AC.c:928)
void Audio_Control_StartCutsceneStream(void* stream_data, int pan, int reverb_enabled)
{
    if (Audio_Samples_SequenceMagicDifference(stream_data) == 0) {
        Audio_Driver_commandArguments.param1 = stream_data;
        Audio_Driver_commandArguments.param2 = (pan & 0xFF) << 8;
        Audio_Driver_commandArguments.param3 = reverb_enabled;
        Audio_Mixer_DispatchCommand(0xE0);
    }
}

// decomp: SLUS_010.40 func_80013328 (src/SLUS_010.40/25AC.c:938)
void Audio_Control_StopCutsceneStream(void) { Audio_Mixer_DispatchCommand(0xE2); }

// decomp: SLUS_010.40 func_80013348 (src/SLUS_010.40/25AC.c:940)
void Audio_Control_SetCutsceneVolume(int volume)
{
    Audio_Driver_commandArguments.param1 = (void*)((volume & 0x7F) << 8);
    Audio_Mixer_DispatchCommand(0xE4);
}

// decomp: SLUS_010.40 func_80013378 (src/SLUS_010.40/25AC.c:946)
void Audio_Control_FadeCutsceneVolume(int fade_ticks, int target_volume)
{
    Audio_Driver_commandArguments.param1 = (void*)fade_ticks;
    Audio_Driver_commandArguments.param2 = (target_volume & 0x7F) << 8;
    Audio_Mixer_DispatchCommand(0xE5);
}

// decomp: SLUS_010.40 func_800133B0 (src/SLUS_010.40/25AC.c:953)
void Audio_Control_SetCutscenePan(int pan)
{
    Audio_Driver_commandArguments.param1 = (void*)((pan & 0xFF) << 8);
    Audio_Mixer_DispatchCommand(0xE6);
}

// decomp: SLUS_010.40 func_800133E0 (src/SLUS_010.40/25AC.c:959)
u_int Audio_Control_SoundEffectChannelIds(u_int* output_effect_ids)
{
    int i;
    Audio_SeqChannel* channel = Audio_SoundEffects_channels;

    for (i = 0; i < 12; ++i) {
        *output_effect_ids++ = (channel++)->effectId;
    }
    return Audio_Mixer_voiceScheduler.activeVoices;
}

