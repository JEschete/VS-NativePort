// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
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
#include "Core/Memory/Copy.h"

// decomp: SLUS_010.40 func_80015BAC (src/SLUS_010.40/25AC.c:2326) [INCLUDE_ASM: not yet ported]
void Audio_Seq_ReleaseSavedVoiceConflicts(void)
{
    VS_UNPORTED("Audio::Seq::ReleaseSavedVoiceConflicts");
}

// decomp: SLUS_010.40 func_80015C9C (src/SLUS_010.40/25AC.c:2330)
void Audio_Seq_ResetSoundEffectChannel(Audio_SeqChannel* arg0, char* arg1)
{
    arg0->volumeBalance = 0x6E00;
    arg0->volume = 0x32000000;
    arg0->programCounter = arg1;
    arg0->transpose = 0;
    arg0->fineTune = 0;
    arg0->portamentoSteps = 0;
    arg0->pitchSlide = 0;
    arg0->pitchBendTarget = 0;
    arg0->pitchSlideTicksLeft = 0;
    arg0->fixedNoteLength = 0;
    arg0->storedNoteLength = 0;
    arg0->volumeSlideLength = 0;
    arg0->finePitchDelta = 0;
    arg0->randomPitchDepth = 0;
    arg0->loopDepth = 0;
    arg0->updateFlags = 0;
    arg0->autoPanOffset = 0;
    arg0->playModes = 0;
    arg0->opcodeStepCounter = 0xFFFF;
    arg0->voice.volumeScale = 0;
    arg0->autoPanDepth = 0;
    arg0->tremoloDepth = 0;
    arg0->vibratoDepth = 0;
    arg0->autoPanDepthSlideLength = 0;
    arg0->tremoloDepthSlideLength = 0;
    arg0->vibratoDepthSlideLength = 0;
    arg0->fmTimer = 0;
    arg0->noiseTimer = 0;
    Audio_Seq_SelectChannelInstrument(arg0, 0);
}

// decomp: SLUS_010.40 func_80015D38 (src/SLUS_010.40/25AC.c:2363)
u_int Audio_Seq_AssignedVoiceMask(Audio_SeqChannel* arg0, u_int arg1)
{
    u_int i;
    u_int var_a3;

    for (i = 0, var_a3 = 0; i < 32; ++i) {
        if (arg1 & (1 << i)) {
            if (arg0->voice.assignedVoice < 0x18) {
                var_a3 |= 1 << arg0->voice.assignedVoice;
            }
        }
        ++arg0;
    }
    return var_a3;
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
    Audio_Seq_activeMusicConfig->sequenceBase = (uintptr_t)in_Sequence;
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
    Audio_MusicConfig* in_Config, Audio_SeqChannel* in_pChannel, u_int arg2)
{
    Audio_SeqChannel* pChannel;
    Audio_MusicConfig** ppThisChannelConfig;
    u_int Count;

    pChannel = in_pChannel;
    if ((in_Config->activeChannels != 0)
        && ((arg2 == 0) || (arg2 == in_Config->musicId))) {
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
    uintptr_t delta;
    u_int mask;
    u_int channelBit;
    int count;
    Audio_SeqChannel* pChannel;
    Audio_MusicConfig* pConfig;

    if (in_SwapWithSavedState == 0) {
        Core_Memory_CopyWords(&Audio_Seq_savedMusicConfig, &Audio_Seq_primaryMusicConfig, 0x80);
        Core_Memory_CopyWords(Audio_Seq_savedMusicChannels, Audio_Seq_musicChannels, 0x2200);
    } else {
        Core_Memory_SwapWords(&Audio_Seq_savedMusicConfig, &Audio_Seq_primaryMusicConfig, 0x80);
        Core_Memory_SwapWords(Audio_Seq_savedMusicChannels, Audio_Seq_musicChannels, 0x2200);
    }

    Audio_Seq_activeMusicConfig->statusFlags &= ~0x60;
    Audio_Seq_activeMusicConfig->statusFlags |=
        in_Sequence->sampleBankId == Audio_Samples_musicSampleBankIds.alternateBankId ? 0x40 : 0x20;

    delta = (uintptr_t)in_Sequence - Audio_Seq_primaryMusicConfig.sequenceBase;
    Audio_Seq_activeMusicConfig->sequenceBase = (uintptr_t)in_Sequence;
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
        Core_Memory_CopyWords(&Audio_Seq_primaryMusicConfig, &Audio_Seq_savedMusicConfig, 0x80);
        Core_Memory_CopyWords(Audio_Seq_musicChannels, (Audio_MusicConfig*)Audio_Seq_savedMusicChannels, 0x2200);
        for (i = 0, c = Audio_Seq_savedMusicChannels; i < 32; ++i, ++c) {
            Audio_Samples_MapInstrumentToBaseBank(Audio_Seq_savedMusicConfig.statusFlags, c);
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_19_Unk (src/SLUS_010.40/25AC.c:2983)
void Audio_Seq_StartMusicLayer(Audio_CommandParams* arg0)
{
    if ((Audio_Seq_activeMusicConfig->activeChannels != 0)
        && ((Audio_Seq_secondaryMusicConfig == NULL) || (Audio_Seq_secondaryMusicConfig->musicId == 0))) {
        Audio_Seq_secondaryMusicConfig = &Audio_Seq_savedMusicConfig;
        Audio_Seq_secondaryMusicChannels = Audio_Seq_savedMusicChannels;
        Core_Memory_CopyWords(&Audio_Seq_primaryMusicConfig, &Audio_Seq_savedMusicConfig, 0x80);
        Core_Memory_CopyWords(Audio_Seq_musicChannels,
            (Audio_MusicConfig*)Audio_Seq_secondaryMusicChannels, 0x2200);
    }
    Audio_Seq_LoadMusicSequence((Audio_AkaoSequenceFile*)arg0->param1);
    Audio_Seq_activeMusicConfig->masterVolume = (arg0->extParam1 & 0x7F) << 0x10;
    Audio_Seq_activeMusicConfig->masterVolumeSlideLength = 0;
    Audio_Seq_activeMusicConfig->musicId = arg0->param3;
    Audio_Seq_ReleaseSavedVoiceConflicts();
    Audio_Driver_controlLatches.mixBehavior &= ~0x100;
}

// decomp: SLUS_010.40 Sound_Cmd_1A_Unk (src/SLUS_010.40/25AC.c:3001)
void Audio_Seq_CrossfadeMusicLayers(Audio_CommandParams* arg0)
{
    if ((Audio_Seq_savedMusicConfig.musicId != 0)
        && (Audio_Seq_savedMusicConfig.musicId == arg0->param3)) {
        Audio_Seq_secondaryMusicConfig = &Audio_Seq_savedMusicConfig;
        Audio_Seq_secondaryMusicChannels = Audio_Seq_savedMusicChannels;
        Audio_Seq_RestoreMusicSequence((Audio_AkaoSequenceFile*)arg0->param1, 1);
    } else {
        if ((Audio_Seq_activeMusicConfig->activeChannels != 0)
            && ((Audio_Seq_secondaryMusicConfig == NULL) || (Audio_Seq_secondaryMusicConfig->musicId == 0))) {
            Audio_Seq_secondaryMusicConfig = &Audio_Seq_savedMusicConfig;
            Audio_Seq_secondaryMusicChannels = Audio_Seq_savedMusicChannels;
            Core_Memory_CopyWords(&Audio_Seq_primaryMusicConfig, &Audio_Seq_savedMusicConfig, 0x80);
            Core_Memory_CopyWords(Audio_Seq_musicChannels,
                (Audio_MusicConfig*)Audio_Seq_secondaryMusicChannels, 0x2200);
        }
        Audio_Seq_LoadMusicSequence((Audio_AkaoSequenceFile*)arg0->param1);
        Audio_Seq_activeMusicConfig->musicId = arg0->param3;
    }
    Audio_Driver_controlLatches.mixBehavior &= ~0x100;
    if (Audio_Seq_secondaryMusicConfig != NULL) {
        int temp_a1 = arg0->extParam1;
        Audio_Seq_masterFade.value = 0x7F0000;
        Audio_Driver_controlLatches.mixBehavior |= 0x100;
        Audio_Seq_masterFade.ticksRemaining = temp_a1;
        Audio_Seq_masterFade.step = (int)0xFF810000 / temp_a1;
    }
    Audio_Seq_activeMusicConfig->masterVolume = 0;
    Audio_Seq_activeMusicConfig->masterVolumeSlideLength = arg0->extParam1;
    Audio_Seq_activeMusicConfig->masterVolumeStep = ((arg0->extParam2 & 0x7F) << 0x10) / arg0->extParam1;
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
void Audio_Seq_SetMusicVolume(Audio_CommandParams* arg0)
{
    Audio_SeqChannel* var_a1;
    Audio_MusicConfig* var_a0;
    int temp_v1;

    temp_v1 = arg0->param1;
    if ((temp_v1 == 0) || (temp_v1 == Audio_Seq_activeMusicConfig->musicId)) {
        var_a0 = Audio_Seq_activeMusicConfig;
        var_a1 = Audio_Seq_musicChannels;
        var_a0->masterVolume = (arg0->param2 & 0x7F) << 0x10;
        var_a0->masterVolumeSlideLength = 0;
        Audio_SoundEffects_InvalidateMusicChannelVolumes(var_a0, var_a1);
    } else {
        var_a0 = Audio_Seq_secondaryMusicConfig;
        if ((var_a0 != NULL) && (temp_v1 != 0) && (temp_v1 == var_a0->musicId)) {
            var_a1 = Audio_Seq_secondaryMusicChannels;
            var_a0->masterVolume = (arg0->param2 & 0x7F) << 0x10;
            var_a0->masterVolumeSlideLength = 0;
            Audio_SoundEffects_InvalidateMusicChannelVolumes(var_a0, var_a1);
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_C1_unk (src/SLUS_010.40/25AC.c:3147)
void Audio_Seq_FadeMusicVolume(Audio_CommandParams* arg0)
{
    Audio_SeqChannel* var_a1;
    Audio_MusicConfig* var_a0;
    int temp_a1;
    int var_v1;

    int var_a2 = 1;

    if (arg0->param2 != 0) {
        var_a2 = arg0->param2;
    }

    temp_a1 = arg0->param1;
    var_v1 = (arg0->param3 & 0x7F) << 0x10;

    if ((temp_a1 == 0) || (temp_a1 == Audio_Seq_activeMusicConfig->musicId)) {
        var_a0 = Audio_Seq_activeMusicConfig;
        var_v1 = (int)(var_v1 - var_a0->masterVolume);
        var_v1 = var_v1 / var_a2;
        var_a1 = Audio_Seq_musicChannels;
        var_a0->masterVolumeSlideLength = var_a2;
        var_a0->masterVolumeStep = var_v1;
        Audio_SoundEffects_InvalidateMusicChannelVolumes(var_a0, var_a1);
    } else {
        var_a0 = Audio_Seq_secondaryMusicConfig;
        if ((var_a0 != NULL) && (temp_a1 != 0) && (temp_a1 == var_a0->musicId)) {
            var_v1 = (int)(var_v1 - var_a0->masterVolume);
            var_v1 = var_v1 / var_a2;
            var_a1 = Audio_Seq_secondaryMusicChannels;
            var_a0->masterVolumeSlideLength = var_a2;
            var_a0->masterVolumeStep = var_v1;
            Audio_SoundEffects_InvalidateMusicChannelVolumes(var_a0, var_a1);
        }
    }
}

// decomp: SLUS_010.40 Sound_Cmd_C2_unk (src/SLUS_010.40/25AC.c:3184)
void Audio_Seq_FadeMusicVolumeFrom(Audio_CommandParams* arg0)
{
    Audio_SeqChannel* var_a1;
    Audio_MusicConfig* var_a0;
    int temp_v0;
    int temp_v1;
    int var_a2;
    u_int temp_v1_2;

    temp_v0 = arg0->param2;
    var_a2 = 1;
    if (temp_v0 != 0) {
        var_a2 = temp_v0;
    }
    temp_v1 = arg0->param1;
    if ((temp_v1 == 0) || (temp_v1 == Audio_Seq_activeMusicConfig->musicId)) {
        var_a0 = Audio_Seq_activeMusicConfig;
        var_a1 = Audio_Seq_musicChannels;
    } else {
        var_a0 = Audio_Seq_secondaryMusicConfig;
        if ((var_a0 == NULL) || (temp_v1 == 0) || (temp_v1 != var_a0->musicId)) {
            return;
        }
        var_a1 = Audio_Seq_secondaryMusicChannels;
    }
    temp_v1_2 = (arg0->param3 & 0x7F) << 0x10;
    var_a0->masterVolume = temp_v1_2;
    var_a0->masterVolumeStep = (int)(((arg0->param4 & 0x7F) << 0x10) - temp_v1_2) / var_a2;
    var_a0->masterVolumeSlideLength = var_a2;
    Audio_SoundEffects_InvalidateMusicChannelVolumes(var_a0, var_a1);
}

// decomp: SLUS_010.40 Sound_Cmd_C8_unk (src/SLUS_010.40/25AC.c:3216)
void Audio_Seq_SetDiscVolume(Audio_CommandParams* arg0)
{
    Audio_Seq_discVolumeFadeLength = 0;
    Audio_Seq_discVolume = (u_short)arg0->param1 << 0x10;
    Audio_Seq_WriteDiscVolume();
}

// decomp: SLUS_010.40 Sound_Cmd_C9_unk (src/SLUS_010.40/25AC.c:3223)
void Audio_Seq_FadeDiscVolume(Audio_CommandParams* arg0)
{
    int temp_v0;
    int var_a1;

    temp_v0 = arg0->param1;
    var_a1 = 1;
    if (temp_v0 != 0) {
        var_a1 = temp_v0;
    }
    Audio_Seq_discVolumeFadeLength = var_a1;
    Audio_Seq_discVolumeFadeStep = (((u_short)arg0->param2 << 0x10) - Audio_Seq_discVolume) / var_a1;
}

// decomp: SLUS_010.40 Sound_Cmd_CA_unk (src/SLUS_010.40/25AC.c:3237)
void Audio_Seq_FadeDiscVolumeFrom(Audio_CommandParams* arg0)
{
    unsigned int new_var;
    int var_a1 = 1;
    if (arg0->param1 != 0) {
        var_a1 = arg0->param1;
    }
    Audio_Seq_discVolumeFadeLength = var_a1;
    new_var = ((u_short)arg0->param3) << 0x10;
    Audio_Seq_discVolume = (u_short)arg0->param2 << 0x10;
    Audio_Seq_discVolumeFadeStep = (int)(new_var - Audio_Seq_discVolume) / var_a1;
}

