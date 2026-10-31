// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
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
#include "Audio/SequencerOutput.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"
#include "Core/Memory/Copy.h"

// decomp: SLUS_010.40 Sound_Cmd_A4_unk (src/SLUS_010.40/25AC.c:3427)
void Audio_Mixer_SetSoundEffectPitchScale(Audio_CommandParams* command)
{
    u_int i;
    Audio_SeqChannel* channel = Audio_SoundEffects_channels;
    u_int mask = Audio_Mixer_voiceScheduler.activeVoices;
    int voice_bit = 0x1000;

    if (command->param2 != 0) {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectTags & command->param2)) {
                channel->effectPitchScale = ((char)command->param3) << 8;
                channel->effectPitchScaleSlideLength = 0;
                channel->voice.dirtyFlags |= 16;
            }
        }
    } else {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectId == command->param1)) {
                channel->effectPitchScale = ((char)command->param3) << 8;
                channel->effectPitchScaleSlideLength = 0;
                channel->voice.dirtyFlags |= 16;
            }
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_A5_unk (src/SLUS_010.40/25AC.c:3453)
void Audio_Mixer_SlideSoundEffectPitchScale(Audio_CommandParams* command)
{
    u_int i;
    Audio_SeqChannel* channel = Audio_SoundEffects_channels;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;
    int voice_bit = 0x1000;

    if (command->param2 != 0) {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectTags & command->param2)) {
                short v = command->param3 != 0 ? command->param3 : 1;
                channel->effectPitchScaleSlideStep =
                    (short)((short)((((char)command->param4) << 8) - channel->effectPitchScale) / v);
                channel->effectPitchScaleSlideLength = v;
            }
        }
    } else {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectId == command->param1)) {
                short v = command->param3 != 0 ? command->param3 : 1;
                channel->effectPitchScaleSlideStep =
                    (short)((short)((((char)command->param4) << 8) - channel->effectPitchScale) / v);
                channel->effectPitchScaleSlideLength = v;
            }
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_AC_unk (src/SLUS_010.40/25AC.c:3481)
void Audio_Mixer_SetWorldSoundEffectPitchScale(Audio_CommandParams* command)
{
    int i;
    Audio_SeqChannel* channel = Audio_SoundEffects_channels;
    for (i = 12; i != 0; --i, ++channel) {
        if (!(channel->effectTags & 0x02000000)) {
            channel->effectPitchScale = (char)command->param1 << 8;
            channel->effectPitchScaleSlideLength = 0;
            channel->voice.dirtyFlags |= 0x10;
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_AD_unk (src/SLUS_010.40/25AC.c:3494)
void Audio_Mixer_SlideWorldSoundEffectPitchScale(Audio_CommandParams* command)
{
    short fade_ticks;
    int voice_bit = 0x1000;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;
    u_int i;
    Audio_SeqChannel* channel;

    for (i = 0, channel = Audio_SoundEffects_channels; i < 12; ++i, ++channel, voice_bit *= 2) {
        if ((mask & voice_bit) && !(channel->effectTags & 0x02000000)) {
            fade_ticks = 1;
            if (command->param1 != 0) {
                fade_ticks = command->param1;
            }
            channel->effectPitchScaleSlideStep =
                (short)((short)((((char)command->param2) << 8) - channel->effectPitchScale) / fade_ticks);
            channel->effectPitchScaleSlideLength = fade_ticks;
        }
    }
}

// decomp: SLUS_010.40 func_8001833C (src/SLUS_010.40/25AC.c:3515)
void Audio_Mixer_SetTempoScale(signed char* tempo_scale_argument)
{
    Audio_Mixer_tempoScale = *tempo_scale_argument << 0x10;
    Audio_Mixer_tempoScaleSlideLength = 0;
}

// decomp: SLUS_010.40 func_80018358 (src/SLUS_010.40/25AC.c:3521)
void Audio_Mixer_SlideTempoScale(Audio_CommandParams* command)
{
    int requested_fade_ticks = command->param1;
    int fade_ticks = 1;
    if (requested_fade_ticks != 0) {
        fade_ticks = requested_fade_ticks;
    }
    Audio_Mixer_tempoScaleStep = (((signed char)command->param2 << 0x10) - Audio_Mixer_tempoScale) / fade_ticks;
    Audio_Mixer_tempoScaleSlideLength = fade_ticks;
}

// decomp: SLUS_010.40 func_800183C0 (src/SLUS_010.40/25AC.c:3532)
void Audio_Mixer_SetTempoScaleRamp(Audio_CommandParams* command)
{
    int fade_ticks;

    fade_ticks = 1;
    if (command->param2 != 0) {
        fade_ticks = command->param1;
    }
    Audio_Mixer_tempoScale = (signed char)command->param2 << 0x10;
    Audio_Mixer_tempoScaleStep =
        (int)(((signed char)command->param3 << 0x10) - Audio_Mixer_tempoScale) / fade_ticks;
    Audio_Mixer_tempoScaleSlideLength = fade_ticks;
}

// decomp: SLUS_010.40 func_80018434 (src/SLUS_010.40/25AC.c:3546)
void Audio_Mixer_SetMusicPitchScale(signed char* pitch_scale_argument)
{
    Audio_Mixer_musicPitchScale = *pitch_scale_argument << 0x10;
    Audio_Mixer_musicPitchScaleSlideLength = 0;
}

// decomp: SLUS_010.40 func_80018450 (src/SLUS_010.40/25AC.c:3552)
void Audio_Mixer_SlideMusicPitchScale(Audio_CommandParams* command)
{
    int requested_fade_ticks;
    int fade_ticks;

    requested_fade_ticks = command->param1;
    fade_ticks = 1;
    if (requested_fade_ticks != 0) {
        fade_ticks = requested_fade_ticks;
    }
    Audio_Mixer_musicPitchScaleStep =
        (int)(((signed char)command->param2 << 0x10) - Audio_Mixer_musicPitchScale)
        / fade_ticks;
    Audio_Mixer_musicPitchScaleSlideLength = (short)fade_ticks;
}

// decomp: SLUS_010.40 func_800184B8 (src/SLUS_010.40/25AC.c:3568)
void Audio_Mixer_SetMusicPitchScaleRamp(Audio_CommandParams* command)
{
    int fade_ticks;

    fade_ticks = 1;
    if (command->param2 != 0) {
        fade_ticks = command->param1;
    }
    Audio_Mixer_musicPitchScale = (signed char)command->param2 << 0x10;
    Audio_Mixer_musicPitchScaleStep =
        (((signed char)command->param3 << 0x10) - Audio_Mixer_musicPitchScale) / fade_ticks;
    Audio_Mixer_musicPitchScaleSlideLength = fade_ticks;
}

// decomp: SLUS_010.40 func_8001852C (src/SLUS_010.40/25AC.c:3582)
void Audio_Mixer_StopMusicAndCancelReplacement(void)
{
    Audio_Seq_ReleaseMusicConfiguration(Audio_Seq_activeMusicConfig, Audio_Seq_musicChannels, 0);

    if (Audio_Seq_secondaryMusicConfig != NULL) {
        Audio_Seq_ReleaseMusicConfiguration(Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels, 0);
    }

    Audio_Seq_activeMusicConfig->statusFlags &= 0xFFFEFFFF;
    Audio_Seq_pendingMusicData.param1 = 0;
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
    Audio_SeqChannel* channel;
    int voice_bit;
    u_int i;

    channel = Audio_SoundEffects_channels;
    voice_bit = 0x1000;
    for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
        if ((Audio_Mixer_voiceScheduler.activeVoices & voice_bit)
            && !(channel->effectTags & 0x02000000)) {
            Audio_Mixer_voiceScheduler.pendingKeyOff |= voice_bit;
            Audio_Seq_ReleaseSoundEffectVoice(channel, voice_bit);
            channel->updateFlags = 0;
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
void Audio_Mixer_SetMusicMuteMask(Audio_CommandParams* command)
{
    u_int i;
    Audio_SeqChannel* c;

    Audio_Mixer_mutedMusicChannels = command->param1;

    for (i = 0, c = Audio_Seq_musicChannels; i < 32; ++i, ++c) {
        c->voice.dirtyFlags |= 3;
    }
}

// decomp: SLUS_010.40 Sound_Cmd_92_unk (src/SLUS_010.40/25AC.c:3660)
void Audio_Mixer_SetMusicJumpThreshold(Audio_CommandParams* command)
{
    Audio_Seq_activeMusicConfig->jumpThreshold = command->param1;
}

// decomp: SLUS_010.40 Sound_Cmd_9B_unk (src/SLUS_010.40/25AC.c:3665)
void Audio_Mixer_PauseMusic(Audio_CommandParams* unused_command __attribute__((unused)))
{
    int paused_channel_mask;
    int voice_index;
    int voices_to_pause;
    int voice_bit;

    if (Audio_Seq_activeMusicConfig->activeChannels != 0) {
        int non_effect_voice_mask = ~(Audio_Mixer_voiceScheduler.activeVoices | Audio_Cutscene_streamVoices);
        voice_bit = 1;
        voices_to_pause = non_effect_voice_mask & 0xFFFFFF;

        if (voices_to_pause != 0) {
            voice_index = 0;
            while (voices_to_pause != 0) {
                if (voices_to_pause & voice_bit) {
                    Audio_Driver_SetVoiceVolume(voice_index, 0, 0, 0);
                    Audio_Driver_SetVoiceSampleRate(voice_index, 0);
                    Audio_Driver_SetVoiceAttackRateAndMode(voice_index, 0x7F, 1);
                    Audio_Driver_SetVoiceSustainRateAndMode(voice_index, 0x7F, 3);
                    voices_to_pause &= ~voice_bit;
                }
                voice_bit *= 2;
                voice_index += 1;
            }
        }
        paused_channel_mask = Audio_Seq_activeMusicConfig->activeChannels;
        Audio_Seq_activeMusicConfig->activeChannels = 0;
        Audio_Seq_activeMusicConfig->pausedChannels = paused_channel_mask;
    }
    Audio_Cutscene_streamControlFlags |= 1;
}

// decomp: SLUS_010.40 Sound_Cmd_9A_unk (src/SLUS_010.40/25AC.c:3698)
void Audio_Mixer_ResumeMusic(Audio_CommandParams* in_Params __attribute__((unused)))
{
    if (Audio_Seq_activeMusicConfig->pausedChannels != 0) {
        Audio_SeqChannel* c = Audio_Seq_musicChannels;
        int channels_to_resume = Audio_Seq_activeMusicConfig->pausedChannels;
        int channel_bit = 1;
        u_int resumed_channel_mask;
        do {
            if (channels_to_resume & channel_bit) {
                channels_to_resume &= ~channel_bit;
                c->voice.dirtyFlags |= 0x2B13;
            }
            channel_bit *= 2;
            ++c;
        } while (channels_to_resume != 0);

        resumed_channel_mask = Audio_Seq_activeMusicConfig->pausedChannels;
        Audio_Seq_activeMusicConfig->pausedChannels = 0;
        Audio_Seq_activeMusicConfig->activeChannels = resumed_channel_mask;
        Audio_Driver_controlLatches.updateFlags |= 0x100;
    }
    Audio_Cutscene_streamControlFlags &= ~1;
}

// decomp: SLUS_010.40 Sound_Cmd_9D_unk (src/SLUS_010.40/25AC.c:3722)
void Audio_Mixer_PauseWorldSoundEffects(Audio_CommandParams* in_Params __attribute__((unused)))
{
    Audio_SeqChannel* channel;
    int voice_bit;
    int voice_index;
    int voices_to_pause;
    u_int i;

    if (Audio_Mixer_voiceScheduler.activeVoices != 0) {
        voices_to_pause = Audio_Mixer_voiceScheduler.activeVoices;
        channel = Audio_SoundEffects_channels;
        voice_bit = 0x1000;

        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((voices_to_pause & voice_bit) && (channel->effectTags & 0x02000000)) {
                voices_to_pause &= ~voice_bit;
            }
        }

        voice_bit = 0x1000;
        voice_index = 0xC;
        Audio_Mixer_voiceScheduler.pausedEffectVoices = voices_to_pause;
        Audio_Mixer_voiceScheduler.activeVoices &= ~voices_to_pause;

        while (voices_to_pause != 0) {
            if (voices_to_pause & voice_bit) {
                Audio_Driver_SetVoiceVolume(voice_index, 0, 0, 0);
                Audio_Driver_SetVoiceSampleRate(voice_index, 0);
                Audio_Driver_SetVoiceAttackRateAndMode(voice_index, 0x7F, 1);
                Audio_Driver_SetVoiceSustainRateAndMode(voice_index, 0x7F, 3);
                voices_to_pause &= ~voice_bit;
            }
            voice_bit *= 2;
            voice_index += 1;
        }
    }
    Audio_Cutscene_streamControlFlags |= 2;
}

// decomp: SLUS_010.40 Sound_Cmd_9C_unk (src/SLUS_010.40/25AC.c:3761)
void Audio_Mixer_ResumeWorldSoundEffects(Audio_CommandParams* in_Params __attribute__((unused)))
{
    if (Audio_Mixer_pausedSoundEffectVoices != 0) {
        u_int resumed_voice_mask;
        Audio_SeqChannel* c = Audio_SoundEffects_channels;
        int voices_to_resume = Audio_Mixer_pausedSoundEffectVoices;
        int voice_bit = 0x1000;

        do {
            if (voices_to_resume & voice_bit) {
                voices_to_resume &= ~voice_bit;
                c->voice.dirtyFlags |= 0x2B13;
            }
            voice_bit *= 2;
            ++c;
        } while (voices_to_resume != 0);

        resumed_voice_mask = Audio_Mixer_voiceScheduler.pausedEffectVoices;
        Audio_Mixer_voiceScheduler.pausedEffectVoices = 0;
        Audio_Mixer_voiceScheduler.activeVoices |= resumed_voice_mask;
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

    curMode = Audio_Native_ReverbMode();
    if (curMode != in_ReverbMode) {
        Audio_Native_EnableReverb(SPU_OFF);
        Audio_Native_SetReverbMode(in_ReverbMode);
        Audio_Native_EnableReverb(SPU_ON);
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
    DisableEvent(Audio_Driver_legacySpuEvent);
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

        if (Audio_Samples_SequenceMagicDifference((int *)(Audio_Driver_commandArguments.param1)) != 0) {
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
    EnableEvent(Audio_Driver_legacySpuEvent);
    return result;
}
#undef AKAO_MAGIC

}  // extern "C"
