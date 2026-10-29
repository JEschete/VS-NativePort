// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/MusicSequence.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SequencerOutput.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"

// decomp: D_80035910 (D_80035910@SLUS_010.40)
Audio_SeqChannel Audio_SoundEffects_channels[22];

// decomp: SLUS_010.40 Sound_EvictSfxVoice (src/SLUS_010.40/25AC.c:2539)
void Audio_SoundEffects_ReleaseMatchingVoices(intptr_t in_SfxId, int in_StopFlags)
{
    u_int i;
    int maxPriority;
    u_int voiceBit = 0x1000;
    u_int activeVoices = Audio_Mixer_voiceScheduler.activeVoices
                       | Audio_Mixer_voiceScheduler.pausedEffectVoices;
    Audio_SeqChannel* pChannel = Audio_SoundEffects_channels;

    if (in_StopFlags & 0x0FFFFFFF) {
        for (i = 0; i < 12; i++, pChannel++, voiceBit <<= 1) {
            if ((activeVoices & voiceBit) && (pChannel->effectTags & in_StopFlags)) {
                if (pChannel->updateFlags & 0x100000) {
                    pChannel->updateFlags |= 0x200000;
                } else {
                    Audio_Mixer_voiceScheduler.pendingKeyOff |= voiceBit;
                    Audio_Seq_ReleaseSoundEffectVoice(pChannel, voiceBit);
                    pChannel->updateFlags = 0;
                }
            }
        }
    } else if (in_StopFlags < 0) {
        pChannel += in_SfxId;
        voiceBit <<= in_SfxId;
        if (activeVoices & voiceBit) {
            Audio_SoundEffects_ReleaseMatchingVoices(pChannel->effectId, 0);
        }
        voiceBit <<= 1;
        pChannel++;
        if (activeVoices & voiceBit) {
            Audio_SoundEffects_ReleaseMatchingVoices(pChannel->effectId, 0);
        }
        return;
    } else if (in_StopFlags & 0x40000000) {
        for (i = 0; i < 12; i++, pChannel++, voiceBit <<= 1) {
            if (pChannel->effectTags != 0) {
                activeVoices &= ~voiceBit;
            }
        }
        pChannel = Audio_SoundEffects_channels;
        voiceBit = 0x1000;
        maxPriority = 0;
        for (i = 0; i < 12; i++, pChannel++, voiceBit <<= 1) {
            if ((activeVoices & voiceBit) && (maxPriority < pChannel->priority)) {
                maxPriority = pChannel->priority;
            }
        }
        pChannel = Audio_SoundEffects_channels;
        voiceBit = 0x1000;
        for (i = 0; i < 12; i++, pChannel++, voiceBit <<= 1) {
            if ((activeVoices & voiceBit) && (maxPriority == pChannel->priority)) {
                if (pChannel->updateFlags & 0x100000) {
                    pChannel->updateFlags |= 0x200000;
                } else {
                    Audio_Mixer_voiceScheduler.pendingKeyOff |= voiceBit;
                    Audio_Seq_ReleaseSoundEffectVoice(pChannel, voiceBit);
                    pChannel->updateFlags = 0;
                }
            }
        }
    } else {
        for (i = 0; i < 12; i++, pChannel++, voiceBit <<= 1) {
            if (!(activeVoices & voiceBit)) {
                continue;
            }
            if (in_SfxId == -1) {
                if ((uintptr_t)pChannel->effectId >= 0x80000000U) {
                    if (pChannel->updateFlags & 0x100000) {
                        pChannel->updateFlags |= 0x200000;
                    } else {
                        Audio_Mixer_voiceScheduler.pendingKeyOff |= voiceBit;
                        Audio_Seq_ReleaseSoundEffectVoice(pChannel, voiceBit);
                        pChannel->updateFlags = 0;
                    }
                }
            } else if (in_SfxId == -2) {
                if (pChannel->effectTags == 0) {
                    if (pChannel->updateFlags & 0x100000) {
                        pChannel->updateFlags |= 0x200000;
                    } else {
                        Audio_Mixer_voiceScheduler.pendingKeyOff |= voiceBit;
                        Audio_Seq_ReleaseSoundEffectVoice(pChannel, voiceBit);
                        pChannel->updateFlags = 0;
                    }
                }
            } else if (pChannel->effectId == in_SfxId) {
                if (pChannel->updateFlags & 0x100000) {
                    pChannel->updateFlags |= 0x200000;
                } else {
                    Audio_Mixer_voiceScheduler.pendingKeyOff |= voiceBit;
                    Audio_Seq_ReleaseSoundEffectVoice(pChannel, voiceBit);
                    pChannel->updateFlags = 0;
                }
            }
        }
    }
    Audio_Driver_controlLatches.updateFlags |= 0x110;
}

// decomp: SLUS_010.40 func_8001653C (src/SLUS_010.40/25AC.c:2638)
void Audio_SoundEffects_StartChannelProgram(Audio_SeqChannel* in_Channel, Audio_CommandParams* in_CommandParams,
    int in_VoiceBit, char* in_ProgramCounter)
{
    u_int i;
    int pan;
    int volume;
    int voiceMask;

    in_Channel->effectId = in_CommandParams->param1;
    in_Channel->effectTags = in_CommandParams->param2;
    pan = (u_char)in_CommandParams->param3 << 8;
    in_Channel->effectPanSlideLength = 0;
    in_Channel->pan = 0x8000;
    in_Channel->panSlideLength = 0;
    in_Channel->effectPan = pan;
    volume = ((u_short)in_CommandParams->param4 & 0x7F) << 8;
    in_Channel->effectVolumeSlideLength = 0;
    in_Channel->effectVolume = volume;
    voiceMask = 1 << in_CommandParams->extParam1;
    in_Channel->noteLength = 2;
    in_Channel->effectVoiceMask = voiceMask;
    in_Channel->gateLength = 1;
    in_Channel->type = 1;
    in_Channel->priority = -2;
    in_Channel->effectPitchScale = 0;
    in_Channel->effectPitchScaleSlideLength = 0;
    Audio_Seq_ResetSoundEffectChannel(in_Channel, in_ProgramCounter);
    Audio_Mixer_voiceOwners[in_Channel->voice.assignedVoice] = NULL;
    Audio_Driver_SetVoiceReleaseRateAndMode(in_Channel->voice.assignedVoice, 5, 3);

    Audio_Mixer_voiceScheduler.activeVoices |= in_VoiceBit;
    Audio_Mixer_voiceScheduler.pendingKeyOff |= in_VoiceBit;
    in_VoiceBit = ~in_VoiceBit;
    Audio_Mixer_voiceScheduler.pendingKeyOn &= in_VoiceBit;
    Audio_Mixer_voiceScheduler.keyedVoices &= in_VoiceBit;
    Audio_Mixer_voiceScheduler.noiseVoices &= in_VoiceBit;
    Audio_Mixer_voiceScheduler.reverbVoices &= in_VoiceBit;
    Audio_Mixer_voiceScheduler.fmVoices &= in_VoiceBit;

    if (Audio_Cutscene_streamControlFlags & 2) {
        in_VoiceBit = 0x1000;
        in_Channel = Audio_SoundEffects_channels;
        for (i = 12; i != 0; i--, in_Channel++, in_VoiceBit <<= 1) {
            if ((Audio_Mixer_voiceScheduler.activeVoices & in_VoiceBit)
                && !(in_Channel->effectTags & 0x02000000)) {
                Audio_Mixer_voiceScheduler.activeVoices &= ~in_VoiceBit;
                Audio_Mixer_voiceScheduler.pausedEffectVoices |= in_VoiceBit;
            }
        }
    }
}

// decomp: SLUS_010.40 FreeVoiceChannels (src/SLUS_010.40/25AC.c:2690)
void Audio_SoundEffects_DetachMusicVoice(Audio_SeqChannel* in_Channel, u_int in_Voice)
{
    u_int VoiceIndex;

    if (in_Voice < VOICE_COUNT) {
        VoiceIndex = 0;
        while (VoiceIndex < SOUND_CHANNEL_COUNT) {
            if (in_Channel->voice.assignedVoice == in_Voice) {
                in_Channel->voice.assignedVoice = VOICE_COUNT;
                Audio_Seq_activeMusicConfig->activeNotes &= ~(1 << VoiceIndex);
            }
            in_Channel++;
            VoiceIndex++;
        };
    }
}

// decomp: src/SLUS_010.40/25AC.c:2707
#define SOUND_UPDATE_STEREO_LINKED (1 << 16) // Second channel of stereo pair
// decomp: SLUS_010.40 Sound_PlaySfxProgram (src/SLUS_010.40/25AC.c:2709)
void Audio_SoundEffects_PlayProgramPair(Audio_CommandParams* in_CommandParams, char* in_ProgramCounter1,
    char* in_ProgramCounter2, int in_SkipRelease)
{
    Audio_SeqChannel* channel;
    int activeVoices;
    int slotsRemaining;
    u_int voiceBit;

    if ((in_ProgramCounter1 == NULL) && (in_ProgramCounter2 == NULL)) {
        return;
    }

    if ((in_SkipRelease == 0) && (in_CommandParams->param2 != 0)) {
        Audio_SoundEffects_ReleaseMatchingVoices(0, in_CommandParams->param2);
    }

    while (1) {
        channel = Audio_SoundEffects_programPairChannels;
        voiceBit = 0x400000;

        activeVoices = Audio_Mixer_voiceScheduler.activeVoices
                     | Audio_Mixer_voiceScheduler.pausedEffectVoices
                     | Audio_Cutscene_streamState.voiceMask;

        for (slotsRemaining = 0xC; slotsRemaining != 0;
            slotsRemaining -= 2, channel -= 2, voiceBit /= 4) {
            if (!(activeVoices & (voiceBit | (voiceBit * 2)))) {
                break;
            }
        }

        if (slotsRemaining != 0) {
            break;
        }

        Audio_SoundEffects_ReleaseMatchingVoices(0, 0x40000000);

        if (activeVoices
            == (Audio_Mixer_voiceScheduler.activeVoices
                | Audio_Mixer_voiceScheduler.pausedEffectVoices
                | Audio_Cutscene_streamState.voiceMask)) {
            return;
        }
    }

    if (in_ProgramCounter1 != NULL) {
        Audio_SoundEffects_StartChannelProgram(channel, in_CommandParams, voiceBit, in_ProgramCounter1);
        Audio_SoundEffects_DetachMusicVoice(
            Audio_Seq_musicChannels, channel->voice.assignedVoice);
    }

    if (in_ProgramCounter2 != NULL) {
        if (in_ProgramCounter1 != NULL) {
            ++channel;
            voiceBit *= 2;
        }

        Audio_SoundEffects_StartChannelProgram(channel, in_CommandParams, voiceBit, in_ProgramCounter2);
        Audio_SoundEffects_DetachMusicVoice(
            Audio_Seq_musicChannels, channel->voice.assignedVoice);

        if (in_ProgramCounter1 != NULL) {
            channel->updateFlags |= SOUND_UPDATE_STEREO_LINKED;
        }
    }
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_04 | SOUND_GLOBAL_UPDATE_08;
}
#undef SOUND_UPDATE_STEREO_LINKED

// decomp: SLUS_010.40 Sound_GetProgramCounters (src/SLUS_010.40/25AC.c:2777)
void Audio_SoundEffects_ProgramCounters(
    char** out_ProgramCounter1, char** out_ProgramCounter2, int in_SfxIndex)
{
    char* block2;
    u_short* block0;

    in_SfxIndex &= 0x3FF;
    in_SfxIndex *= 2;
    block2 = Audio_Playback_soundEffectProgramData;
    block0 = (u_short*)Audio_Playback_soundEffectProgramOffsets;

    *out_ProgramCounter1 =
        block0[in_SfxIndex] == 0xFFFF ? NULL : block2 + block0[in_SfxIndex];

    ++in_SfxIndex;

    *out_ProgramCounter2 =
        block0[in_SfxIndex] == 0xFFFF ? NULL : block2 + block0[in_SfxIndex];
}

// decomp: SLUS_010.40 Sound_MarkActiveChannelsVolumeDirty (src/SLUS_010.40/25AC.c:2797)
void Audio_SoundEffects_InvalidateMusicChannelVolumes(
    Audio_MusicConfig* in_pChannelConfig, Audio_SeqChannel* in_pChannel)
{
    u_int ActiveChannelMask;
    u_int Flags;
    u_int Mask;

    ActiveChannelMask = in_pChannelConfig->activeChannels;
    if (ActiveChannelMask == 0) {
        return;
    }

    Flags = ActiveChannelMask;
    Mask = 1;

    while (Flags != 0) {
        if (Flags & Mask) {
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
            Flags ^= Mask;
        }
        in_pChannel++;
        Mask <<= 1;
    }
}

// decomp: SLUS_010.40 Sound_MarkScheduledSfxChannelsVolumeDirty (src/SLUS_010.40/25AC.c:2822)
void Audio_SoundEffects_InvalidateSoundEffectVolumes(void)
{
    u_int Mask;
    u_int ActiveChannelMask;
    Audio_SeqChannel* pChannel = Audio_SoundEffects_programPairChannels - 10;

    if (Audio_Mixer_voiceScheduler.activeVoices == 0) {
        return;
    }

    ActiveChannelMask = Audio_Mixer_voiceScheduler.activeVoices;

    Mask = (1 << 12); // SFX Channels start at channel 12
    while (ActiveChannelMask != 0) {
        if (ActiveChannelMask & Mask) {
            ActiveChannelMask ^= Mask;
            pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
        }
        pChannel++;
        Mask <<= 1;
    };
}

// decomp: SLUS_010.40 Sound_Cmd_34_unk (src/SLUS_010.40/25AC.c:3046)
void Audio_SoundEffects_PlayInterfaceProgramPair(Audio_CommandParams* command)
{
    char* first_program;
    char* second_program;

    first_program = (char*)command->param1;
    second_program = (char*)command->param2;
    command->param1 = 0x400;
    command->param2 = 0x01000000;
    command->param3 = 0x80;
    command->param4 = 0x7F;
    command->extParam1 = 0;
    Audio_SoundEffects_PlayProgramPair(command, first_program, second_program, 0);
}

// decomp: SLUS_010.40 Sound_Cmd_30_unk (src/SLUS_010.40/25AC.c:3061)
void Audio_SoundEffects_PlayInterfaceProgram(Audio_CommandParams* command)
{
    char* first_program;
    char* second_program;

    Audio_SoundEffects_ProgramCounters(&first_program, &second_program, command->param1);
    command->param2 = 0x02000000;
    command->param3 = 0x80;
    command->param4 = 0x7F;
    command->extParam1 = Audio_Samples_SoundEffectBankSlot(Audio_Playback_soundEffectBankSelectors[command->param1]);
    Audio_SoundEffects_PlayProgramPair(command, first_program, second_program, 0);
}

// decomp: SLUS_010.40 Sound_Cmd_20_unk (src/SLUS_010.40/25AC.c:3074)
void Audio_SoundEffects_PlaySharedProgram(Audio_CommandParams* command)
{
    char* akaoOffset;
    char* akaoData;

    Audio_SoundEffects_ProgramCounters(&akaoOffset, &akaoData, command->param1);
    command->extParam1 = Audio_Samples_SoundEffectBankSlot(Audio_Playback_soundEffectBankSelectors[command->param1]);
    Audio_SoundEffects_PlayProgramPair(command, akaoOffset, akaoData, 0);
}

// decomp: SLUS_010.40 func_8001733C (src/SLUS_010.40/25AC.c:3085)
void Audio_SoundEffects_PlayBlobPrograms(Audio_CommandParams* in_Params)
{
    int* header;
    int* offsets;
    char* data;
    u_short* entry;
    char* programCounter1;
    char* programCounter2;
    int count;

    in_Params->extParam1 = Audio_Samples_SoundEffectBankSlot(((int*)in_Params->param1)[2]);
    header = (int*)in_Params->param1;
    offsets = header + 4;
    count = header[1];
    data = (char*)header;
    data += 32; // skip the header

    entry = (u_short*)(data + *offsets);
    programCounter1 = *entry == 0xFFFF ? NULL : (char*)(*entry + (intptr_t)entry + 4);
    entry++;
    programCounter2 = *entry == 0xFFFF ? NULL : (char*)(*entry + (intptr_t)entry + 2);
    Audio_SoundEffects_PlayProgramPair(in_Params, programCounter1, programCounter2, 0);

    while (--count != 0) {
        offsets++;
        entry = (u_short*)(data + *offsets);
        programCounter1 = *entry == 0xFFFF ? NULL : (char*)(*entry + (intptr_t)entry + 4);
        entry++;
        programCounter2 = *entry == 0xFFFF ? NULL : (char*)(*entry + (intptr_t)entry + 2);
        Audio_SoundEffects_PlayProgramPair(in_Params, programCounter1, programCounter2, 1);
    }
}

// decomp: SLUS_010.40 Sound_Cmd_21_unk (src/SLUS_010.40/25AC.c:3118)
void Audio_SoundEffects_StopPrograms(Audio_CommandParams* command)
{
    Audio_SoundEffects_ReleaseMatchingVoices(command->param1, command->param2);
}

// decomp: SLUS_010.40 Sound_Cmd_A0_unk (src/SLUS_010.40/25AC.c:3250)
void Audio_SoundEffects_SetProgramVolume(Audio_CommandParams* command)
{
    u_int i;
    Audio_SeqChannel* channel = Audio_SoundEffects_channels;
    u_int mask = Audio_Mixer_voiceScheduler.activeVoices;
    int voice_bit = 0x1000;

    if (command->param2 != 0) {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectTags & command->param2)) {
                channel->effectVolume = (command->param3 & 0x7F) << 8;
                channel->effectVolumeSlideLength = 0;
                channel->voice.dirtyFlags |= 3;
            }
        }
    } else {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectId == command->param1)) {
                channel->effectVolume = (command->param3 & 0x7F) << 8;
                channel->effectVolumeSlideLength = 0;
                channel->voice.dirtyFlags |= 3;
            }
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_A1_unk (src/SLUS_010.40/25AC.c:3276)
void Audio_SoundEffects_FadeProgramVolume(Audio_CommandParams* command)
{
    u_int i;
    Audio_SeqChannel* channel = Audio_SoundEffects_channels;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;
    int voice_bit = 0x1000;

    if (command->param2 != 0) {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectTags & command->param2)) {
                short v = command->param3 != 0 ? command->param3 : 1;
                channel->effectVolumeSlideStep = (short)(((command->param4 & 0x7F) << 8) - channel->effectVolume) / v;
                channel->effectVolumeSlideLength = v;
            }
        }
    } else {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectId == command->param1)) {
                short v = command->param3 != 0 ? command->param3 : 1;
                channel->effectVolumeSlideStep = (short)(((command->param4 & 0x7F) << 8) - channel->effectVolume) / v;
                channel->effectVolumeSlideLength = v;
            }
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_A8_unk (src/SLUS_010.40/25AC.c:3302)
void Audio_SoundEffects_SetSceneVolume(Audio_CommandParams* command)
{
    int voice_bit = 0x1000;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;
    u_int i;
    Audio_SeqChannel* channel;

    for (i = 0, channel = Audio_SoundEffects_channels; i < 12; ++i, ++channel, voice_bit *= 2) {
        if ((mask & voice_bit) && !(channel->effectTags & 0x02000000)) {
            channel->effectVolume = (command->param1 & 0x7F) << 8;
            channel->effectVolumeSlideLength = 0;
            channel->voice.dirtyFlags |= 3;
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_A9_unk (src/SLUS_010.40/25AC.c:3318)
void Audio_SoundEffects_FadeSceneVolume(Audio_CommandParams* command)
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
            channel->effectVolumeSlideStep =
                ((short)(((command->param2 & 0x7F) << 8) - channel->effectVolume) / fade_ticks);
            channel->effectVolumeSlideLength = fade_ticks;
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_A2_unk (src/SLUS_010.40/25AC.c:3339)
void Audio_SoundEffects_SetProgramPan(Audio_CommandParams* command)
{
    u_int i;
    Audio_SeqChannel* channel = Audio_SoundEffects_channels;
    u_int mask = Audio_Mixer_voiceScheduler.activeVoices;
    int voice_bit = 0x1000;

    if (command->param2 != 0) {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectTags & command->param2)) {
                channel->effectPan = ((char)command->param3) << 8;
                channel->effectPanSlideLength = 0;
                channel->voice.dirtyFlags |= 3;
            }
        }
    } else {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectId == command->param1)) {
                channel->effectPan = ((char)command->param3) << 8;
                channel->effectPanSlideLength = 0;
                channel->voice.dirtyFlags |= 3;
            }
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_A3_unk (src/SLUS_010.40/25AC.c:3365)
void Audio_SoundEffects_FadeProgramPan(Audio_CommandParams* command)
{
    u_int i;
    Audio_SeqChannel* channel = Audio_SoundEffects_channels;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;
    int voice_bit = 0x1000;

    if (command->param2 != 0) {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectTags & command->param2)) {
                short v = command->param3 != 0 ? command->param3 : 1;
                channel->effectPanSlideStep = (short)((((char)command->param4) << 8) - channel->effectPan) / v;
                channel->effectPanSlideLength = v;
            }
        }
    } else {
        for (i = 0; i < 12; ++i, ++channel, voice_bit *= 2) {
            if ((mask & voice_bit) && (channel->effectId == command->param1)) {
                short v = command->param3 != 0 ? command->param3 : 1;
                channel->effectPanSlideStep = (short)((((char)command->param4) << 8) - channel->effectPan) / v;
                channel->effectPanSlideLength = v;
            }
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_AA_unk (src/SLUS_010.40/25AC.c:3391)
void Audio_SoundEffects_SetScenePan(Audio_CommandParams* command)
{
    int voice_bit = 0x1000;
    int mask = Audio_Mixer_voiceScheduler.activeVoices;
    u_int i;
    Audio_SeqChannel* channel;

    for (i = 0, channel = Audio_SoundEffects_channels; i < 12; ++i, ++channel, voice_bit *= 2) {
        if ((mask & voice_bit) && !(channel->effectTags & 0x02000000)) {
            channel->effectPan = ((char)command->param1) << 8;
            channel->effectPanSlideLength = 0;
            channel->voice.dirtyFlags |= 3;
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_AB_unk (src/SLUS_010.40/25AC.c:3407)
void Audio_SoundEffects_FadeScenePan(Audio_CommandParams* command)
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
            channel->effectPanSlideStep = (short)((((char)command->param2) << 8) - channel->effectPan) / fade_ticks;
            channel->effectPanSlideLength = fade_ticks;
        }
    }
}

