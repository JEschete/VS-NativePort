// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/MusicState.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/MusicSequence.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SequencerOutput.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"
#include "Audio/XaPlayback.h"

// decomp: SLUS_010.40 func_80015C9C (src/SLUS_010.40/25AC.c:2330)
void Audio_Seq_ResetSoundEffectChannel(Audio_SeqChannel* channel, char* program_start)
{
    channel->volumeBalance = 0x6E00;
    channel->volume = 0x32000000;
    channel->programCounter = program_start;
    channel->transpose = 0;
    channel->fineTune = 0;
    channel->portamentoSteps = 0;
    channel->pitchSlide = 0;
    channel->pitchBendTarget = 0;
    channel->pitchSlideTicksLeft = 0;
    channel->fixedNoteLength = 0;
    channel->storedNoteLength = 0;
    channel->volumeSlideLength = 0;
    channel->finePitchDelta = 0;
    channel->randomPitchDepth = 0;
    channel->loopDepth = 0;
    channel->updateFlags = 0;
    channel->autoPanOffset = 0;
    channel->playModes = 0;
    channel->opcodeStepCounter = 0xFFFF;
    channel->voice.volumeScale = 0;
    channel->autoPanDepth = 0;
    channel->tremoloDepth = 0;
    channel->vibratoDepth = 0;
    channel->autoPanDepthSlideLength = 0;
    channel->tremoloDepthSlideLength = 0;
    channel->vibratoDepthSlideLength = 0;
    channel->fmTimer = 0;
    channel->noiseTimer = 0;
    Audio_Seq_SelectChannelInstrument(channel, 0);
}

// decomp: SLUS_010.40 func_80015D38 (src/SLUS_010.40/25AC.c:2363)
u_int Audio_Seq_AssignedVoiceMask(Audio_SeqChannel* channel_cursor, u_int channel_mask)
{
    u_int i;
    u_int assigned_voice_mask;

    for (i = 0, assigned_voice_mask = 0; i < 32; ++i) {
        if (channel_mask & (1 << i)) {
            if (channel_cursor->voice.assignedVoice < 0x18) {
                assigned_voice_mask |= 1 << channel_cursor->voice.assignedVoice;
            }
        }
        ++channel_cursor;
    }
    return assigned_voice_mask;
}

// decomp: SLUS_010.40 Sound_LoadAkaoSequence (src/SLUS_010.40/25AC.c:2380)
void Audio_Seq_LoadMusicSequence(Audio_AkaoSequenceFile* in_Sequence)
{
    Audio_AkaoSequenceFile* pSequence;
    u_int channelMask;
    u_int mask;
    u_int i;
    Audio_SeqChannel* pChannel;
    u_short* pChannelOffsets;
    Audio_MusicConfig* pConfig;
    char* pRegion;
    int updateFlags;

    pSequence = in_Sequence;
    pChannelOffsets = (u_short*)in_Sequence;
    Audio_Seq_activeMusicConfig->sequenceBase = (intptr_t)in_Sequence;
    channelMask = in_Sequence->channelEnableMask;
    if (Audio_Seq_secondaryMusicConfig != NULL) {
        mask = Audio_Seq_AssignedVoiceMask(
            Audio_Seq_secondaryMusicChannels, Audio_Seq_secondaryMusicConfig->activeChannels);
    } else {
        mask = 0;
    }
    Audio_Mixer_voiceScheduler.pendingKeyOff |=
        ~mask & 0xFFFFFF & ~(Audio_Mixer_voiceScheduler.activeVoices | Audio_Cutscene_streamVoices);
    pConfig = Audio_Seq_activeMusicConfig;
    pConfig->pendingKeyOff = 0;
    if (Audio_Cutscene_streamControlFlags & 1) {
        pConfig->activeChannels = 0;
        pConfig->pausedChannels |= channelMask;
    } else {
        pConfig->pausedChannels = 0;
        pConfig->activeChannels |= channelMask;
    }

    Audio_Seq_activeMusicConfig->keyedVoices = pSequence->keyedMask;
    Audio_Seq_activeMusicConfig->allocatedVoices = pSequence->allocatedVoiceMask;
    Audio_Seq_activeMusicConfig->statusFlags &= ~0x61;
    Audio_Seq_activeMusicConfig->statusFlags |=
        pSequence->sampleBankId == Audio_Samples_musicSampleBankIds.alternateBankId ? 0x40 : 0x20;
    pRegion = (char*)pSequence + (pSequence->patchRegionOffset + 0x30);
    Audio_Seq_activeMusicConfig->patchTable =
        pSequence->patchRegionOffset != 0 ? (Audio_AkaoSequenceFile*)pRegion : NULL;
    pRegion = (char*)pSequence + (pSequence->keymapRegionOffset + 0x34);
    Audio_Seq_activeMusicConfig->keymapTable =
        pSequence->keymapRegionOffset != 0 ? (u_short*)pRegion : NULL;
    Audio_Seq_activeMusicConfig->voiceSearchStart = 0;
    Audio_Seq_activeMusicConfig->resumeKeyOnMask = pSequence->resumeKeyOnMask;
    Audio_Seq_activeMusicConfig->pushReleaseMask = pSequence->pushReleaseMask;

    mask = 1;
    i = 0;
    pChannel = Audio_Seq_musicChannels;
    pChannelOffsets += 0x20;
    while (i < 0x20) {
        if (channelMask & mask) {
            pChannel->programCounter = (char*)pChannelOffsets + *pChannelOffsets;
            pChannel->noteLength = 4;
            pChannel->gateLength = 2;
            pChannel->volumeBalance = 0x7F00;
            pChannel->volume = 0x3FFF0000;
            pChannel->effectVolume = 0x4000;
            pChannel->fineTune = 0;
            pChannel->transpose = 0;
            pChannel->portamentoSteps = 0;
            pChannel->pitchSlide = 0;
            pChannel->pitchBendTarget = 0;
            pChannel->pitchSlideTicksLeft = 0;
            pChannel->fixedNoteLength = 0;
            pChannel->storedNoteLength = 0;
            pChannel->pan = 0x8000;
            pChannel->panSlideLength = 0;
            pChannel->portamentoSteps = 0;
            pChannel->effectVolumeSlideLength = 0;
            pChannel->volumeSlideLength = 0;
            pChannel->finePitchDelta = 0;
            pChannel->randomPitchDepth = 0;
            pChannel->keyOnVolumeSlideLength = 0;
            pChannel->playModes = 0;
            pChannel->autoPanOffset = 0;
            pChannel->loopDepth = 0;
            updateFlags = !(Audio_Seq_activeMusicConfig->allocatedVoices & mask) << 6;
            pChannel->autoPanDepth = 0;
            pChannel->tremoloDepth = 0;
            pChannel->vibratoDepth = 0;
            pChannel->autoPanDepthSlideLength = 0;
            pChannel->updateFlags = updateFlags;
            pChannel->tremoloDepthSlideLength = 0;
            pChannel->vibratoDepthSlideLength = 0;
            pChannel->fmTimer = 0;
            pChannel->noiseTimer = 0;
            pChannelOffsets++;
            Audio_Seq_SelectChannelInstrument(pChannel, 0);
        } else {
            pChannel->noteLength = 3;
            pChannel->gateLength = 1;
            pChannel->programCounter = (char*)&Audio_Seq_kIdleProgram;
            pChannel->voice.dirtyFlags |= 0x4400;
            pChannel->voice.adsrUpper =
                (pChannel->voice.adsrUpper & 0xFFE0) | 5;
        }
        pChannel->voice.assignedVoice = VOICE_COUNT;
        channelMask &= ~mask;
        pChannel++;
        i++;
        mask <<= 1;
    }

    Audio_Seq_activeMusicConfig->tempo = 0xFFFF0000;
    Audio_Seq_activeMusicConfig->tickAccumulator = 1;
    Audio_Seq_activeMusicConfig->tempoSlideLength = 0;
    Audio_Seq_activeMusicConfig->reverbDepth = 0;
    Audio_Seq_activeMusicConfig->reverbDepthSlideLength = 0;
    Audio_Seq_activeMusicConfig->reverbDepthSlideStep = 0;
    Audio_Seq_activeMusicConfig->timerLowerCount = 0;
    Audio_Seq_activeMusicConfig->timerLower = 0;
    Audio_Seq_activeMusicConfig->timerUpperCount = 0;
    Audio_Seq_activeMusicConfig->timerTop = 0;
    Audio_Seq_activeMusicConfig->noiseChannels = 0;
    Audio_Seq_activeMusicConfig->reverbChannels = 0;
    Audio_Seq_activeMusicConfig->fmChannels = 0;
    Audio_Seq_activeMusicConfig->jumpThreshold = 0;
    Audio_Seq_activeMusicConfig->activeNotes = 0;
    Audio_Seq_activeMusicConfig->pendingKeyOn = 0;
    Audio_Driver_controlLatches.updateFlags |= 0x100;
}

// decomp: SLUS_010.40 Sound_KillMusicConfig (src/SLUS_010.40/25AC.c:2506)
void Audio_Seq_ReleaseMusicConfiguration(
    Audio_MusicConfig* in_Config, Audio_SeqChannel* in_pChannel, u_int music_id_filter)
{
    Audio_SeqChannel* pChannel;
    Audio_MusicConfig** ppThisChannelConfig;
    u_int Count;

    pChannel = in_pChannel;
    if ((in_Config->activeChannels != 0)
        && ((music_id_filter == 0) || (music_id_filter == in_Config->musicId))) {
        in_Config->pendingKeyOff = -1;
        for (Count = SOUND_CHANNEL_COUNT; Count != 0; Count--) {
            pChannel->noteLength = 3;
            pChannel->gateLength = 1;
            pChannel->programCounter = (char*)&Audio_Seq_kIdleProgram;
            pChannel++;
        };

        ppThisChannelConfig = Audio_Mixer_voiceOwners;
        in_Config->musicId = 0;
        in_Config->activeNotes = 0;
        in_Config->pendingKeyOn = 0;

        for (Count = 0; Count < VOICE_COUNT; Count++) {
            if (*ppThisChannelConfig == in_Config) {
                *ppThisChannelConfig = NULL;
                Audio_Driver_SetVoiceReleaseRateAndMode(Count, 5, 3U);
            }
            ppThisChannelConfig++;
        };
    }
}

// decomp: SLUS_010.40 Sound_SetMusicSequence (src/SLUS_010.40/25AC.c:2847)
void Audio_Seq_RestoreMusicSequence(Audio_AkaoSequenceFile* in_Sequence, int in_SwapWithSavedState)
{
    intptr_t delta;
    u_int mask;
    u_int channelBit;
    int count;
    Audio_SeqChannel* pChannel;
    Audio_MusicConfig* pConfig;

    Audio_Native_RestoreMusicSnapshot(in_SwapWithSavedState);

    Audio_Seq_activeMusicConfig->statusFlags &= ~0x60;
    Audio_Seq_activeMusicConfig->statusFlags |=
        in_Sequence->sampleBankId == Audio_Samples_musicSampleBankIds.alternateBankId ? 0x40 : 0x20;

    delta = (intptr_t)in_Sequence - Audio_Seq_primaryMusicConfig.sequenceBase;
    Audio_Seq_activeMusicConfig->sequenceBase = (intptr_t)in_Sequence;
    Audio_Seq_activeMusicConfig->pendingKeyOn = 0;
    Audio_Driver_controlLatches.updateFlags |= 0x90;
    Audio_Seq_activeMusicConfig->patchTable =
        (Audio_AkaoSequenceFile*)((char*)Audio_Seq_activeMusicConfig->patchTable + delta);
    mask = Audio_Seq_activeMusicConfig->activeChannels;
    Audio_Seq_activeMusicConfig->keymapTable =
        (u_short*)((char*)Audio_Seq_activeMusicConfig->keymapTable + delta);
    Audio_Seq_activeMusicConfig->pendingKeyOn = Audio_Seq_activeMusicConfig->activeNotes;

    pChannel = Audio_Seq_musicChannels;
    for (count = 0x20, channelBit = 1; count != 0;
        count--, pChannel++, channelBit <<= 1) {
        if (mask & channelBit) {
            pChannel->programCounter += delta;
            pChannel->keymap += delta;
            pChannel->loopStarts[0] += delta;
            pChannel->loopStarts[1] += delta;
            pChannel->loopStarts[2] += delta;
            pChannel->loopStarts[3] += delta;
            pChannel->noteLength += 2;
            pChannel->gateLength += 2;
            pChannel->voice.dirtyFlags |= 0x1FF93;
            Audio_Samples_MapInstrumentToAlternateBank(
                Audio_Seq_activeMusicConfig->statusFlags, pChannel);
        } else {
            pChannel->noteLength = 4;
            pChannel->gateLength = 2;
            pChannel->programCounter = (char*)&Audio_Seq_kIdleProgram;
        }
        pChannel->voice.assignedVoice = VOICE_COUNT;
    }

    if (Audio_Seq_secondaryMusicConfig != NULL) {
        mask = Audio_Seq_AssignedVoiceMask(
            Audio_Seq_secondaryMusicChannels, Audio_Seq_secondaryMusicConfig->activeChannels);
    } else {
        mask = 0;
    }
    pConfig = Audio_Seq_activeMusicConfig;
    pConfig->pendingKeyOff = 0;
    Audio_Seq_savedMusicConfig.musicId = 0;
    Audio_Mixer_voiceScheduler.pendingKeyOff |=
        ~mask & 0xFFFFFF & ~(Audio_Mixer_voiceScheduler.activeVoices | Audio_Cutscene_streamVoices);
    Audio_Driver_controlLatches.updateFlags |= 0x100;

    if (in_SwapWithSavedState != 0) {
        delta = pConfig->reverbDepth + pConfig->reverbDepthSlideStep * pConfig->reverbDepthSlideLength;
        channelBit = 0x30;
        delta &= ~0xFFF;
        if (pConfig->reverbDepthSlideLength != 0) {
            channelBit = pConfig->reverbDepthSlideLength;
        }
        if (Audio_Seq_secondaryMusicConfig != NULL) {
            pConfig->reverbDepth = Audio_Seq_secondaryMusicConfig->reverbDepth;
        }
        Audio_Seq_activeMusicConfig->reverbDepth &= ~0xFFF;
        delta -= Audio_Seq_activeMusicConfig->reverbDepth;
        if (delta != 0) {
            Audio_Seq_activeMusicConfig->reverbDepthSlideStep = delta / (int)channelBit;
            Audio_Seq_activeMusicConfig->reverbDepthSlideLength = channelBit;
        } else {
            Audio_Seq_activeMusicConfig->reverbDepthSlideLength = 0;
        }
    }

    if (Audio_Cutscene_streamControlFlags & 1) {
        Audio_Seq_activeMusicConfig->pausedChannels = Audio_Seq_activeMusicConfig->activeChannels;
        Audio_Seq_activeMusicConfig->activeChannels = 0;
    }
}

// decomp: SLUS_010.40 Sound_Cmd_10_StartFieldMusic (src/SLUS_010.40/25AC.c:2958)
void Audio_Seq_StartOrResumeMusic(Audio_CommandParams* in_Params)
{
    if ((Audio_Seq_savedMusicConfig.musicId != 0)
        && (Audio_Seq_savedMusicConfig.musicId == in_Params->param3)) {
        Audio_Seq_RestoreMusicSequence((Audio_AkaoSequenceFile*)in_Params->param1, 0);
        return;
    }
    Audio_Seq_LoadMusicSequence((Audio_AkaoSequenceFile*)in_Params->param1);
    Audio_Seq_activeMusicConfig->musicId = in_Params->param3;
}

// decomp: SLUS_010.40 Sound_Cmd_40_PushMusicState (src/SLUS_010.40/25AC.c:2969)
void Audio_Seq_SaveCurrentMusic(void)
{
    u_int i;

    if (Audio_Seq_activeMusicConfig->activeChannels != 0) {
        Audio_SeqChannel* c;
        Audio_Native_SaveMusicSnapshot();
        for (i = 0, c = Audio_Seq_savedMusicChannels; i < 32; ++i, ++c) {
            Audio_Samples_MapInstrumentToBaseBank(Audio_Seq_savedMusicConfig.statusFlags, c);
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_19_Unk (src/SLUS_010.40/25AC.c:2983)
void Audio_Seq_StartMusicLayer(Audio_CommandParams* command)
{
    if ((Audio_Seq_activeMusicConfig->activeChannels != 0)
        && ((Audio_Seq_secondaryMusicConfig == NULL) || (Audio_Seq_secondaryMusicConfig->musicId == 0))) {
        Audio_Seq_secondaryMusicConfig = &Audio_Seq_savedMusicConfig;
        Audio_Seq_secondaryMusicChannels = Audio_Seq_savedMusicChannels;
        Audio_Native_SaveMusicSnapshot();
    }
    Audio_Seq_LoadMusicSequence((Audio_AkaoSequenceFile*)command->param1);
    Audio_Seq_activeMusicConfig->masterVolume = (command->extParam1 & 0x7F) << 0x10;
    Audio_Seq_activeMusicConfig->masterVolumeSlideLength = 0;
    Audio_Seq_activeMusicConfig->musicId = command->param3;
    Audio_Seq_ReleaseSavedVoiceConflicts();
    Audio_Driver_controlLatches.mixBehavior &= ~0x100;
}

// decomp: SLUS_010.40 Sound_Cmd_1A_Unk (src/SLUS_010.40/25AC.c:3001)
void Audio_Seq_CrossfadeMusicLayers(Audio_CommandParams* command)
{
    if ((Audio_Seq_savedMusicConfig.musicId != 0)
        && (Audio_Seq_savedMusicConfig.musicId == command->param3)) {
        Audio_Seq_secondaryMusicConfig = &Audio_Seq_savedMusicConfig;
        Audio_Seq_secondaryMusicChannels = Audio_Seq_savedMusicChannels;
        Audio_Seq_RestoreMusicSequence((Audio_AkaoSequenceFile*)command->param1, 1);
    } else {
        if ((Audio_Seq_activeMusicConfig->activeChannels != 0)
            && ((Audio_Seq_secondaryMusicConfig == NULL) || (Audio_Seq_secondaryMusicConfig->musicId == 0))) {
            Audio_Seq_secondaryMusicConfig = &Audio_Seq_savedMusicConfig;
            Audio_Seq_secondaryMusicChannels = Audio_Seq_savedMusicChannels;
            Audio_Native_SaveMusicSnapshot();
        }
        Audio_Seq_LoadMusicSequence((Audio_AkaoSequenceFile*)command->param1);
        Audio_Seq_activeMusicConfig->musicId = command->param3;
    }
    Audio_Driver_controlLatches.mixBehavior &= ~0x100;
    if (Audio_Seq_secondaryMusicConfig != NULL) {
        int fade_ticks = command->extParam1;
        Audio_Seq_masterFade.value = 0x7F0000;
        Audio_Driver_controlLatches.mixBehavior |= 0x100;
        Audio_Seq_masterFade.ticksRemaining = fade_ticks;
        Audio_Seq_masterFade.step = (int)0xFF810000 / fade_ticks;
    }
    Audio_Seq_activeMusicConfig->masterVolume = 0;
    Audio_Seq_activeMusicConfig->masterVolumeSlideLength = command->extParam1;
    Audio_Seq_activeMusicConfig->masterVolumeStep = ((command->extParam2 & 0x7F) << 0x10) / command->extParam1;
    Audio_Seq_ReleaseSavedVoiceConflicts();
}

// decomp: SLUS_010.40 Sound_Cmd_12_unk (src/SLUS_010.40/25AC.c:3034)
void Audio_Seq_StartMusicWithLoopCounter(Audio_CommandParams* in_Params)
{
    int LoopCounter;

    Audio_Seq_StartOrResumeMusic(in_Params);
    LoopCounter = 0;
    if (in_Params->param4 != 0) {
        LoopCounter = in_Params->param4 - 1;
    }
    Audio_Seq_musicLoopsRemaining = LoopCounter;
}

// decomp: SLUS_010.40 Sound_Cmd_C0_unk (src/SLUS_010.40/25AC.c:3123)
void Audio_Seq_SetMusicVolume(Audio_CommandParams* command)
{
    Audio_SeqChannel* channels;
    Audio_MusicConfig* music_config;
    int music_id;

    music_id = command->param1;
    if ((music_id == 0) || (music_id == Audio_Seq_activeMusicConfig->musicId)) {
        music_config = Audio_Seq_activeMusicConfig;
        channels = Audio_Seq_musicChannels;
        music_config->masterVolume = (command->param2 & 0x7F) << 0x10;
        music_config->masterVolumeSlideLength = 0;
        Audio_SoundEffects_InvalidateMusicChannelVolumes(music_config, channels);
    } else {
        music_config = Audio_Seq_secondaryMusicConfig;
        if ((music_config != NULL) && (music_id != 0) && (music_id == music_config->musicId)) {
            channels = Audio_Seq_secondaryMusicChannels;
            music_config->masterVolume = (command->param2 & 0x7F) << 0x10;
            music_config->masterVolumeSlideLength = 0;
            Audio_SoundEffects_InvalidateMusicChannelVolumes(music_config, channels);
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_C1_unk (src/SLUS_010.40/25AC.c:3147)
void Audio_Seq_FadeMusicVolume(Audio_CommandParams* command)
{
    Audio_SeqChannel* channels;
    Audio_MusicConfig* music_config;
    int music_id;
    int volume_step_work;

    int fade_ticks = 1;

    if (command->param2 != 0) {
        fade_ticks = command->param2;
    }

    music_id = command->param1;
    volume_step_work = (command->param3 & 0x7F) << 0x10;

    if ((music_id == 0) || (music_id == Audio_Seq_activeMusicConfig->musicId)) {
        music_config = Audio_Seq_activeMusicConfig;
        volume_step_work = (int)(volume_step_work - music_config->masterVolume);
        volume_step_work = volume_step_work / fade_ticks;
        channels = Audio_Seq_musicChannels;
        music_config->masterVolumeSlideLength = fade_ticks;
        music_config->masterVolumeStep = volume_step_work;
        Audio_SoundEffects_InvalidateMusicChannelVolumes(music_config, channels);
    } else {
        music_config = Audio_Seq_secondaryMusicConfig;
        if ((music_config != NULL) && (music_id != 0) && (music_id == music_config->musicId)) {
            volume_step_work = (int)(volume_step_work - music_config->masterVolume);
            volume_step_work = volume_step_work / fade_ticks;
            channels = Audio_Seq_secondaryMusicChannels;
            music_config->masterVolumeSlideLength = fade_ticks;
            music_config->masterVolumeStep = volume_step_work;
            Audio_SoundEffects_InvalidateMusicChannelVolumes(music_config, channels);
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_C2_unk (src/SLUS_010.40/25AC.c:3184)
void Audio_Seq_FadeMusicVolumeFrom(Audio_CommandParams* command)
{
    Audio_SeqChannel* channels;
    Audio_MusicConfig* music_config;
    int requested_fade_ticks;
    int music_id;
    int fade_ticks;
    u_int start_volume_q16;

    requested_fade_ticks = command->param2;
    fade_ticks = 1;
    if (requested_fade_ticks != 0) {
        fade_ticks = requested_fade_ticks;
    }
    music_id = command->param1;
    if ((music_id == 0) || (music_id == Audio_Seq_activeMusicConfig->musicId)) {
        music_config = Audio_Seq_activeMusicConfig;
        channels = Audio_Seq_musicChannels;
    } else {
        music_config = Audio_Seq_secondaryMusicConfig;
        if ((music_config == NULL) || (music_id == 0) || (music_id != music_config->musicId)) {
            return;
        }
        channels = Audio_Seq_secondaryMusicChannels;
    }
    start_volume_q16 = (command->param3 & 0x7F) << 0x10;
    music_config->masterVolume = start_volume_q16;
    music_config->masterVolumeStep = (int)(((command->param4 & 0x7F) << 0x10) - start_volume_q16) / fade_ticks;
    music_config->masterVolumeSlideLength = fade_ticks;
    Audio_SoundEffects_InvalidateMusicChannelVolumes(music_config, channels);
}

// decomp: SLUS_010.40 Sound_Cmd_C8_unk (src/SLUS_010.40/25AC.c:3216)
void Audio_Seq_SetDiscVolume(Audio_CommandParams* command)
{
    Audio_Seq_discVolumeFadeLength = 0;
    Audio_Seq_discVolume = (u_short)command->param1 << 0x10;
    Audio_Seq_WriteDiscVolume();
}

// decomp: SLUS_010.40 Sound_Cmd_C9_unk (src/SLUS_010.40/25AC.c:3223)
void Audio_Seq_FadeDiscVolume(Audio_CommandParams* command)
{
    int requested_fade_ticks;
    int fade_ticks;

    requested_fade_ticks = command->param1;
    fade_ticks = 1;
    if (requested_fade_ticks != 0) {
        fade_ticks = requested_fade_ticks;
    }
    Audio_Seq_discVolumeFadeLength = fade_ticks;
    Audio_Seq_discVolumeFadeStep = (((u_short)command->param2 << 0x10) - Audio_Seq_discVolume) / fade_ticks;
}

// decomp: SLUS_010.40 Sound_Cmd_CA_unk (src/SLUS_010.40/25AC.c:3237)
void Audio_Seq_FadeDiscVolumeFrom(Audio_CommandParams* command)
{
    unsigned int target_volume_q16;
    int fade_ticks = 1;
    if (command->param1 != 0) {
        fade_ticks = command->param1;
    }
    Audio_Seq_discVolumeFadeLength = fade_ticks;
    target_volume_q16 = ((u_short)command->param3) << 0x10;
    Audio_Seq_discVolume = (u_short)command->param2 << 0x10;
    Audio_Seq_discVolumeFadeStep = (int)(target_volume_q16 - Audio_Seq_discVolume) / fade_ticks;
}

}  // extern "C"
