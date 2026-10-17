// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Akao.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/MusicSequence.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"
#include "Core/Memory/Copy.h"

// decomp: SLUS_010.40 Sound_Cmd_A4_unk (src/SLUS_010.40/25AC.c:3427)
void Audio_Mixer_SetSoundEffectPitchScale(Audio_CommandParams* arg0)
{
    u_int i;
    Audio_SeqChannel* var_a1 = Audio_SoundEffects_channels;
    u_int mask = Audio_Mixer_voiceScheduler.activeVoices;
    int var_a3 = 0x1000;

    if (arg0->param2 != 0) {
        for (i = 0; i < 12; ++i, ++var_a1, var_a3 *= 2) {
            if ((mask & var_a3) && (var_a1->effectTags & arg0->param2)) {
                var_a1->effectPitchScale = ((char)arg0->param3) << 8;
                var_a1->effectPitchScaleSlideLength = 0;
                var_a1->voice.dirtyFlags |= 16;
            }
        }
    } else {
        for (i = 0; i < 12; ++i, ++var_a1, var_a3 *= 2) {
            if ((mask & var_a3) && (var_a1->effectId == arg0->param1)) {
                var_a1->effectPitchScale = ((char)arg0->param3) << 8;
                var_a1->effectPitchScaleSlideLength = 0;
                var_a1->voice.dirtyFlags |= 16;
            }
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_A5_unk (src/SLUS_010.40/25AC.c:3453)
void Audio_Mixer_SlideSoundEffectPitchScale(Audio_CommandParams* arg0)
{
    u_int i;
    Audio_SeqChannel* var_a2 = Audio_SoundEffects_channels;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;
    int var_t0 = 0x1000;

    if (arg0->param2 != 0) {
        for (i = 0; i < 12; ++i, ++var_a2, var_t0 *= 2) {
            if ((mask & var_t0) && (var_a2->effectTags & arg0->param2)) {
                short v = arg0->param3 != 0 ? arg0->param3 : 1;
                var_a2->effectPitchScaleSlideStep =
                    (short)((short)((((char)arg0->param4) << 8) - var_a2->effectPitchScale) / v);
                var_a2->effectPitchScaleSlideLength = v;
            }
        }
    } else {
        for (i = 0; i < 12; ++i, ++var_a2, var_t0 *= 2) {
            if ((mask & var_t0) && (var_a2->effectId == arg0->param1)) {
                short v = arg0->param3 != 0 ? arg0->param3 : 1;
                var_a2->effectPitchScaleSlideStep =
                    (short)((short)((((char)arg0->param4) << 8) - var_a2->effectPitchScale) / v);
                var_a2->effectPitchScaleSlideLength = v;
            }
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_AC_unk (src/SLUS_010.40/25AC.c:3481)
void Audio_Mixer_SetWorldSoundEffectPitchScale(Audio_CommandParams* arg0)
{
    int i;
    Audio_SeqChannel* var_a1 = Audio_SoundEffects_channels;
    for (i = 12; i != 0; --i, ++var_a1) {
        if (!(var_a1->effectTags & 0x02000000)) {
            var_a1->effectPitchScale = (char)arg0->param1 << 8;
            var_a1->effectPitchScaleSlideLength = 0;
            var_a1->voice.dirtyFlags |= 0x10;
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_AD_unk (src/SLUS_010.40/25AC.c:3494)
void Audio_Mixer_SlideWorldSoundEffectPitchScale(Audio_CommandParams* arg0)
{
    short var_a1;
    int var_t0 = 0x1000;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;
    u_int i;
    Audio_SeqChannel* var_a2;

    for (i = 0, var_a2 = Audio_SoundEffects_channels; i < 12; ++i, ++var_a2, var_t0 *= 2) {
        if ((mask & var_t0) && !(var_a2->effectTags & 0x02000000)) {
            var_a1 = 1;
            if (arg0->param1 != 0) {
                var_a1 = arg0->param1;
            }
            var_a2->effectPitchScaleSlideStep =
                (short)((short)((((char)arg0->param2) << 8) - var_a2->effectPitchScale) / var_a1);
            var_a2->effectPitchScaleSlideLength = var_a1;
        }
    }
}

// decomp: SLUS_010.40 func_8001833C (src/SLUS_010.40/25AC.c:3515)
void Audio_Mixer_SetTempoScale(signed char* arg0)
{
    Audio_Mixer_tempoScale = *arg0 << 0x10;
    Audio_Mixer_tempoScaleSlideLength = 0;
}

// decomp: SLUS_010.40 func_80018358 (src/SLUS_010.40/25AC.c:3521)
void Audio_Mixer_SlideTempoScale(Audio_CommandParams* arg0)
{
    int temp_v0 = arg0->param1;
    int var_a1 = 1;
    if (temp_v0 != 0) {
        var_a1 = temp_v0;
    }
    Audio_Mixer_tempoScaleStep = (((signed char)arg0->param2 << 0x10) - Audio_Mixer_tempoScale) / var_a1;
    Audio_Mixer_tempoScaleSlideLength = var_a1;
}

// decomp: SLUS_010.40 func_800183C0 (src/SLUS_010.40/25AC.c:3532)
void Audio_Mixer_SetTempoScaleRamp(Audio_CommandParams* arg0)
{
    int var_a1;

    var_a1 = 1;
    if (arg0->param2 != 0) {
        var_a1 = arg0->param1;
    }
    Audio_Mixer_tempoScale = (signed char)arg0->param2 << 0x10;
    Audio_Mixer_tempoScaleStep =
        (int)(((signed char)arg0->param3 << 0x10) - Audio_Mixer_tempoScale) / var_a1;
    Audio_Mixer_tempoScaleSlideLength = var_a1;
}

// decomp: SLUS_010.40 func_80018434 (src/SLUS_010.40/25AC.c:3546)
void Audio_Mixer_SetMusicPitchScale(signed char* arg0)
{
    Audio_Mixer_musicPitchScale = *arg0 << 0x10;
    Audio_Mixer_musicPitchScaleSlideLength = 0;
}

// decomp: SLUS_010.40 func_80018450 (src/SLUS_010.40/25AC.c:3552)
void Audio_Mixer_SlideMusicPitchScale(Audio_CommandParams* arg0)
{
    int temp_v0;
    int var_a1;

    temp_v0 = arg0->param1;
    var_a1 = 1;
    if (temp_v0 != 0) {
        var_a1 = temp_v0;
    }
    Audio_Mixer_musicPitchScaleStep =
        (int)(((signed char)arg0->param2 << 0x10) - Audio_Mixer_musicPitchScale)
        / var_a1;
    Audio_Mixer_musicPitchScaleSlideLength = (short)var_a1;
}

// decomp: SLUS_010.40 func_800184B8 (src/SLUS_010.40/25AC.c:3568)
void Audio_Mixer_SetMusicPitchScaleRamp(Audio_CommandParams* arg0)
{
    int var_a1;

    var_a1 = 1;
    if (arg0->param2 != 0) {
        var_a1 = arg0->param1;
    }
    Audio_Mixer_musicPitchScale = (signed char)arg0->param2 << 0x10;
    Audio_Mixer_musicPitchScaleStep =
        (((signed char)arg0->param3 << 0x10) - Audio_Mixer_musicPitchScale) / var_a1;
    Audio_Mixer_musicPitchScaleSlideLength = var_a1;
}

// decomp: SLUS_010.40 func_8001852C (src/SLUS_010.40/25AC.c:3582)
void Audio_Mixer_StopMusicAndCancelReplacement(void)
{
    Audio_Seq_ReleaseMusicConfiguration(Audio_Seq_activeMusicConfig, Audio_Seq_musicChannels, 0);

    if (Audio_Seq_secondaryMusicConfig != NULL) {
        Audio_Seq_ReleaseMusicConfiguration(Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels, 0);
    }

    Audio_Seq_activeMusicConfig->statusFlags &= 0xFFFEFFFF;
    *(int*)&Audio_Seq_pendingMusicData.param1 = 0;
}

// decomp: SLUS_010.40 Sound_Cmd_11_StopAllMusic (src/SLUS_010.40/25AC.c:3594)
void Audio_Mixer_StopMatchingMusic(Audio_CommandParams* in_Params)
{
    Audio_Seq_ReleaseMusicConfiguration(Audio_Seq_activeMusicConfig, Audio_Seq_musicChannels, in_Params->param1);
    if (Audio_Seq_secondaryMusicConfig != NULL) {
        if (in_Params->param1 != 0) {
            Audio_Seq_ReleaseMusicConfiguration(
                Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels, in_Params->param1);
        }
    }
}

// decomp: SLUS_010.40 func_80018608 (src/SLUS_010.40/25AC.c:3607)
void Audio_Mixer_StopWorldSoundEffects(void)
{
    Audio_SeqChannel* var_s2;
    int var_s1;
    u_int i;

    var_s2 = Audio_SoundEffects_channels;
    var_s1 = 0x1000;
    for (i = 0; i < 12; ++i, ++var_s2, var_s1 *= 2) {
        if ((Audio_Mixer_voiceScheduler.activeVoices & var_s1)
            && !(var_s2->effectTags & 0x02000000)) {
            Audio_Mixer_voiceScheduler.pendingKeyOff |= var_s1;
            Audio_Seq_ReleaseSoundEffectVoice(var_s2, var_s1);
            var_s2->updateFlags = 0;
        }
    }
    Audio_Driver_controlLatches.updateFlags |= 0x110;
}

// decomp: SLUS_010.40 Sound_Cmd_80_unk (src/SLUS_010.40/25AC.c:3626)
void Audio_Mixer_EnableStereoOutput(Audio_CommandParams* in_Params __attribute__((unused)))
{
    Audio_Driver_mixBehavior = 1;
    Audio_SoundEffects_InvalidateMusicChannelVolumes(Audio_Seq_activeMusicConfig, Audio_Seq_musicChannels);
    if (Audio_Seq_secondaryMusicConfig != NULL) {
        Audio_SoundEffects_InvalidateMusicChannelVolumes(
            Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels);
    }
    Audio_SoundEffects_InvalidateSoundEffectVolumes();
}

// decomp: SLUS_010.40 Sound_Cmd_81_unk (src/SLUS_010.40/25AC.c:3637)
void Audio_Mixer_EnableMonoOutput(Audio_CommandParams* in_Params __attribute__((unused)))
{
    Audio_Driver_mixBehavior = 2;
    Audio_SoundEffects_InvalidateMusicChannelVolumes(Audio_Seq_activeMusicConfig, Audio_Seq_musicChannels);
    if (Audio_Seq_secondaryMusicConfig != NULL) {
        Audio_SoundEffects_InvalidateMusicChannelVolumes(
            Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels);
    }
    Audio_SoundEffects_InvalidateSoundEffectVolumes();
}

// decomp: SLUS_010.40 Sound_Cmd_90_unk (src/SLUS_010.40/25AC.c:3648)
void Audio_Mixer_SetMusicMuteMask(Audio_CommandParams* arg0)
{
    u_int i;
    Audio_SeqChannel* c;

    Audio_Mixer_mutedMusicChannels = arg0->param1;

    for (i = 0, c = Audio_Seq_musicChannels; i < 32; ++i, ++c) {
        c->voice.dirtyFlags |= 3;
    }
}

// decomp: SLUS_010.40 Sound_Cmd_92_unk (src/SLUS_010.40/25AC.c:3660)
void Audio_Mixer_SetMusicJumpThreshold(Audio_CommandParams* arg0)
{
    Audio_Seq_activeMusicConfig->jumpThreshold = arg0->param1;
}

// decomp: SLUS_010.40 Sound_Cmd_9B_unk (src/SLUS_010.40/25AC.c:3665)
void Audio_Mixer_PauseMusic(Audio_CommandParams* arg0 __attribute__((unused)))
{
    int temp_v1;
    int var_s0;
    int var_s1;
    int var_s2;

    if (Audio_Seq_activeMusicConfig->activeChannels != 0) {
        int v0 = ~(Audio_Mixer_voiceScheduler.activeVoices | Audio_Cutscene_streamVoices);
        var_s2 = 1;
        var_s1 = v0 & 0xFFFFFF;

        if (var_s1 != 0) {
            var_s0 = 0;
            while (var_s1 != 0) {
                if (var_s1 & var_s2) {
                    Audio_Driver_SetVoiceVolume(var_s0, 0, 0, 0);
                    Audio_Driver_SetVoiceSampleRate(var_s0, 0);
                    Audio_Driver_SetVoiceAttackRateAndMode(var_s0, 0x7F, 1);
                    Audio_Driver_SetVoiceSustainRateAndMode(var_s0, 0x7F, 3);
                    var_s1 &= ~var_s2;
                }
                var_s2 *= 2;
                var_s0 += 1;
            }
        }
        temp_v1 = Audio_Seq_activeMusicConfig->activeChannels;
        Audio_Seq_activeMusicConfig->activeChannels = 0;
        Audio_Seq_activeMusicConfig->pausedChannels = temp_v1;
    }
    Audio_Cutscene_streamControlFlags |= 1;
}

// decomp: SLUS_010.40 Sound_Cmd_9A_unk (src/SLUS_010.40/25AC.c:3698)
void Audio_Mixer_ResumeMusic(Audio_CommandParams* in_Params __attribute__((unused)))
{
    if (Audio_Seq_activeMusicConfig->pausedChannels != 0) {
        Audio_SeqChannel* c = Audio_Seq_musicChannels;
        int var_a2 = Audio_Seq_activeMusicConfig->pausedChannels;
        int var_a1 = 1;
        u_int temp_v1;
        do {
            if (var_a2 & var_a1) {
                var_a2 &= ~var_a1;
                c->voice.dirtyFlags |= 0x2B13;
            }
            var_a1 *= 2;
            ++c;
        } while (var_a2 != 0);

        temp_v1 = Audio_Seq_activeMusicConfig->pausedChannels;
        Audio_Seq_activeMusicConfig->pausedChannels = 0;
        Audio_Seq_activeMusicConfig->activeChannels = temp_v1;
        Audio_Driver_controlLatches.updateFlags |= 0x100;
    }
    Audio_Cutscene_streamControlFlags &= ~1;
}

// decomp: SLUS_010.40 Sound_Cmd_9D_unk (src/SLUS_010.40/25AC.c:3722)
void Audio_Mixer_PauseWorldSoundEffects(Audio_CommandParams* in_Params __attribute__((unused)))
{
    Audio_SeqChannel* var_a0;
    int var_s0;
    int var_s2;
    int var_s1;
    u_int i;

    if (Audio_Mixer_voiceScheduler.activeVoices != 0) {
        var_s1 = Audio_Mixer_voiceScheduler.activeVoices;
        var_a0 = Audio_SoundEffects_channels;
        var_s0 = 0x1000;

        for (i = 0; i < 12; ++i, ++var_a0, var_s0 *= 2) {
            if ((var_s1 & var_s0) && (var_a0->effectTags & 0x02000000)) {
                var_s1 &= ~var_s0;
            }
        }

        var_s0 = 0x1000;
        var_s2 = 0xC;
        Audio_Mixer_voiceScheduler.pausedEffectVoices = var_s1;
        Audio_Mixer_voiceScheduler.activeVoices &= ~var_s1;

        while (var_s1 != 0) {
            if (var_s1 & var_s0) {
                Audio_Driver_SetVoiceVolume(var_s2, 0, 0, 0);
                Audio_Driver_SetVoiceSampleRate(var_s2, 0);
                Audio_Driver_SetVoiceAttackRateAndMode(var_s2, 0x7F, 1);
                Audio_Driver_SetVoiceSustainRateAndMode(var_s2, 0x7F, 3);
                var_s1 &= ~var_s0;
            }
            var_s0 *= 2;
            var_s2 += 1;
        }
    }
    Audio_Cutscene_streamControlFlags |= 2;
}

// decomp: SLUS_010.40 Sound_Cmd_9C_unk (src/SLUS_010.40/25AC.c:3761)
void Audio_Mixer_ResumeWorldSoundEffects(Audio_CommandParams* in_Params __attribute__((unused)))
{
    if (Audio_Mixer_pausedSoundEffectVoices != 0) {
        u_int temp_a2;
        Audio_SeqChannel* c = Audio_SoundEffects_channels;
        int var_a2 = Audio_Mixer_pausedSoundEffectVoices;
        int var_a1 = 0x1000;

        do {
            if (var_a2 & var_a1) {
                var_a2 &= ~var_a1;
                c->voice.dirtyFlags |= 0x2B13;
            }
            var_a1 *= 2;
            ++c;
        } while (var_a2 != 0);

        temp_a2 = Audio_Mixer_voiceScheduler.pausedEffectVoices;
        Audio_Mixer_voiceScheduler.pausedEffectVoices = 0;
        Audio_Mixer_voiceScheduler.activeVoices |= temp_a2;
        Audio_Driver_controlLatches.updateFlags |= 0x100;
    }
    Audio_Cutscene_streamControlFlags &= ~2;
}

// decomp: SLUS_010.40 Sound_Cmd_11_Nop (src/SLUS_010.40/25AC.c:3804)
void Audio_Mixer_IgnoreCommand(Audio_CommandParams* in_Params __attribute__((unused))) { }

// decomp: SLUS_010.40 Sound_SetReverbMode (src/SLUS_010.40/25AC.c:3806)
void Audio_Mixer_SetReverbMode(int in_ReverbMode)
{
    long curMode;

    SpuGetReverbModeType(&curMode);
    if (curMode != in_ReverbMode) {
        SpuSetReverb(SPU_OFF);
        SpuSetReverbModeType(in_ReverbMode | SPU_REV_MODE_CLEAR_WA);
        SpuSetReverb(SPU_ON);
    }
}

// decomp: src/SLUS_010.40/25AC.c:75
#define AKAO_MAGIC 0x4F414B41 // 'AKAO'
// decomp: SLUS_010.40 func_80018C30 (src/SLUS_010.40/25AC.c:3818)
u_int Audio_Mixer_DispatchCommand(u_int in_Command)
{
    Audio_AkaoFileHeader* pSequence;
    Audio_CommandParams* pParams;
    u_int result;
    int fadeLength;

    result = 0;
    in_Command &= 0xFF;
    pParams = &Audio_Mixer_commandParams;

    switch (in_Command) {
    case 0x10:
    case 0x12:
    case 0x19:
    case 0x1A:
        pSequence = (Audio_AkaoFileHeader*)Audio_Driver_commandArguments.param1;
        if (*(int*)pSequence->magic == AKAO_MAGIC) {
            Audio_Driver_controlLatches.controlLatches &= ~0x7700;
            if ((Audio_Seq_activeMusicConfig->musicId != pSequence->sequenceId)
                || ((Audio_Seq_secondaryMusicConfig != NULL)
                    && (Audio_Seq_secondaryMusicConfig->musicId
                        != Audio_Seq_activeMusicConfig->musicId))) {
                Audio_Mixer_SetReverbMode(pSequence->reverbMode);
                pParams->param1 = (uintptr_t)pSequence;
                pParams->param3 = pSequence->sequenceId;
                switch (in_Command) {
                case 0x12:
                    pParams->param4 = Audio_Driver_commandArguments.param2;
                    break;
                case 0x19:
                    pParams->extParam1 = Audio_Driver_commandArguments.param2;
                    break;
                case 0x1A:
                    fadeLength = Audio_Driver_commandArguments.param2;
                    if (fadeLength == 0) {
                        fadeLength = 1;
                    }
                    pParams->extParam1 = fadeLength;
                    pParams->extParam2 = Audio_Driver_commandArguments.param3;
                    break;
                }
                result = pSequence->sequenceId;
            } else {
                in_Command = 0;
                result = 0;
            }
        } else {
            in_Command = 0;
            result = -1;
        }
        break;
    case 0x14: {
        void* pData;

        if (Audio_Samples_SequenceMagicDifference(Audio_Driver_commandArguments.param1) != 0) {
            result = -1;
            break;
        }
        pData = Audio_Driver_commandArguments.param1;
        Audio_Seq_activeMusicConfig->statusFlags |= 0x10000;
        Audio_Seq_pendingMusicData.param1 = (uintptr_t)pData;
        result = ((Audio_AkaoFileHeader*)Audio_Driver_commandArguments.param1)->sequenceId;
        break;
    }
    case 0x16:
        if ((*(int*)Audio_Driver_commandArguments.param1 == AKAO_MAGIC)
            && (*(int*)Audio_Driver_commandArguments.param2 == AKAO_MAGIC)) {
            pSequence = (Audio_AkaoFileHeader*)Audio_Driver_commandArguments.param2;
            Audio_Driver_controlLatches.controlLatches &= ~0x7700;
            Audio_Mixer_SetReverbMode(pSequence->reverbMode);
            Audio_Driver_commandArguments.param2 = pSequence->sequenceId;
            Audio_Seq_LoadMusicSequence((Audio_AkaoSequenceFile*)pSequence);
            Audio_Seq_activeMusicConfig->musicId = pSequence->sequenceId;
            Core_Memory_CopyWords(Audio_Seq_musicChannels, Audio_Seq_savedMusicChannels, 0x2200);
            Core_Memory_CopyWords(&Audio_Seq_primaryMusicConfig, &Audio_Seq_savedMusicConfig, 0x80);
            pSequence = (Audio_AkaoFileHeader*)Audio_Driver_commandArguments.param1;
            Audio_Driver_commandArguments.param1 = (void*)(u_int)pSequence->sequenceId;
            Audio_Seq_LoadMusicSequence((Audio_AkaoSequenceFile*)pSequence);
            Audio_Seq_secondaryMusicConfig = &Audio_Seq_savedMusicConfig;
            Audio_Seq_secondaryMusicChannels = Audio_Seq_savedMusicChannels;
            Audio_Seq_activeMusicConfig->musicId = pSequence->sequenceId;
            Audio_Seq_savedMusicConfig.activeChannels &= ~Audio_Seq_savedMusicConfig.pushReleaseMask;
            Audio_Driver_controlLatches.controlLatches |= 0x100;
        }
        in_Command = 0;
        break;
    case 0x17:
        if (Audio_Driver_controlLatches.controlLatches & 0x1100) {
            if (Audio_Driver_controlLatches.controlLatches & 0x100) {
                Audio_Driver_controlLatches.controlLatches |= 0x4000;
            } else {
                Audio_Driver_controlLatches.controlLatches |= 0x400;
            }
        }
        in_Command = 0;
        break;
    case 0xD8:
        pParams->param1 = (uintptr_t)Audio_Driver_commandArguments.param1;
        Audio_Mixer_kCommandHandlers[0xD0](pParams);
        in_Command = 0xD4;
        break;
    case 0xD9:
        pParams->param1 = (uintptr_t)Audio_Driver_commandArguments.param1;
        pParams->param2 = Audio_Driver_commandArguments.param2;
        Audio_Mixer_kCommandHandlers[0xD1](pParams);
        in_Command = 0xD5;
        break;
    case 0xDA:
        pParams->param1 = (uintptr_t)Audio_Driver_commandArguments.param1;
        pParams->param2 = Audio_Driver_commandArguments.param2;
        pParams->param3 = Audio_Driver_commandArguments.param3;
        Audio_Mixer_kCommandHandlers[0xD2](pParams);
        in_Command = 0xD6;
        break;
    case 0x99:
        Audio_Mixer_kCommandHandlers[0x9B](pParams);
        Audio_Mixer_kCommandHandlers[0x9D](pParams);
        in_Command = 0x9F;
        break;
    case 0x98:
        Audio_Mixer_kCommandHandlers[0x9A](pParams);
        Audio_Mixer_kCommandHandlers[0x9C](pParams);
        in_Command = 0x9E;
        break;
    default:
        pParams->param1 = (uintptr_t)Audio_Driver_commandArguments.param1;
        pParams->param2 = Audio_Driver_commandArguments.param2;
        pParams->param3 = Audio_Driver_commandArguments.param3;
        pParams->param4 = Audio_Driver_commandArguments.param4;
        pParams->extParam1 = Audio_Driver_commandArguments.extParam1;
        pParams->extParam2 = Audio_Driver_commandArguments.extParam2;
        break;
    }
    Audio_Mixer_kCommandHandlers[in_Command](pParams);
    return result;
}
#undef AKAO_MAGIC

