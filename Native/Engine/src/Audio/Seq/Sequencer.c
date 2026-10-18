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
#include "Audio/Stream.h"
#include "Core/Memory/Copy.h"

// decomp: D_8002F54C (D_8002F54C@SLUS_010.40)
u_short Audio_Seq_tickCounter; /* contents: module image (ModuleData.c) */

// decomp: D_8002F5A0 (D_8002F5A0@SLUS_010.40)
u_char Audio_Seq_kOpcodeLengths[152]; /* contents: module image (ModuleData.c) */

// decomp: D_8002F600 (D_8002F600@SLUS_010.40)
u_char Audio_Seq_kExtendedOpcodeLengths[56]; /* contents: module image (ModuleData.c) */

// decomp: D_8002F620 (D_8002F620@SLUS_010.40)
u_short Audio_Seq_kNoteLengths[12]; /* contents: module image (ModuleData.c) */

// decomp: g_Sound_ProgramCounter (g_Sound_ProgramCounter@SLUS_010.40)
u_int Audio_Seq_kIdleProgram; /* contents: module image (ModuleData.c) */

// decomp: D_8002F63C (D_8002F63C@SLUS_010.40)
u_int Audio_Seq_kSemitonePitches[88]; /* contents: module image (ModuleData.c) */

// decomp: D_8002F66C (D_8002F66C@SLUS_010.40)
char Audio_Seq_kNoteRemapTables[304]; /* contents: module image (ModuleData.c) */

// decomp: D_8002F69C (D_8002F69C@SLUS_010.40)
char Audio_Seq_kRandomPitchOffsets[256]; /* contents: module image (ModuleData.c) */

// decomp: g_Sound_LfoTable (g_Sound_LfoTable@SLUS_010.40)
short* Audio_Seq_kLfoWaveforms[16]; /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_800305E0 (D_800305E0@SLUS_010.40)
void (*Audio_Seq_kOpcodeHandlers[96])(Audio_SeqChannel*, int); /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_80030760 (D_80030760@SLUS_010.40)
void (*Audio_Seq_kExtendedOpcodeHandlers[32])(Audio_SeqChannel*, int); /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_80037800 (D_80037800@SLUS_010.40)
Audio_CommandParams Audio_Seq_embeddedSoundEffectParams;

// decomp: g_Sound_MasterFadeTimer (g_Sound_MasterFadeTimer@SLUS_010.40)
Audio_FadeTimer Audio_Seq_masterFade;

// decomp: g_Sound_LfoPhase (g_Sound_LfoPhase@SLUS_010.40)
int Audio_Seq_randomPitchPhase;

// decomp: SLUS_010.40 UpdateCdVolume (src/SLUS_010.40/25AC.c:3958)
void Audio_Seq_WriteDiscVolume(void)
{
    int v = Audio_Seq_discVolumeLevel;
    *CD_VOL_L = v;
    *CD_VOL_R = v;
}

// decomp: SLUS_010.40 func_8001924C (src/SLUS_010.40/25AC.c:4024)
void Audio_Seq_UpdateGlobalSlides(void)
{
    int temp_s1;
    int var_a2;
    Audio_SeqChannel* var_a1_2;
    int var_a3;
    Audio_SeqChannel* var_v1;

    Audio_Seq_randomPitchPhase = (Audio_Seq_randomPitchPhase + 1) & 0xFF;
    if (Audio_Seq_discVolumeFadeLength != 0) {
        --Audio_Seq_discVolumeFadeLength;
        Audio_Seq_discVolume += Audio_Seq_discVolumeFadeStep;
        Audio_Seq_WriteDiscVolume();
    }
    if ((Audio_Cutscene_streamState.voiceMask != 0)
        && (Audio_Cutscene_streamState.volumeFadeTicksLeft != 0)) {
        --Audio_Cutscene_streamState.volumeFadeTicksLeft;
        temp_s1 = Audio_Cutscene_streamState.volume
                + Audio_Cutscene_streamState.volumeFadeStep;
        if ((temp_s1 & 0xFF00) != (Audio_Cutscene_streamState.volume & 0xFF00)) {
            if (Audio_Driver_mixBehavior & 2) {
                int temp_s0 = (Audio_Cutscene_streamState.volume * Audio_Mixer_kCenterPanGain) >> 0x10;
                Audio_Driver_SetVoiceVolume(
                    Audio_Cutscene_streamState.voiceIndex, temp_s0, temp_s0, 0);
                Audio_Driver_SetVoiceVolume(
                    Audio_Cutscene_streamState.voiceIndex + 1, temp_s0, temp_s0, 0);
            } else {
                int temp_s0 = (temp_s1 << 0xF) >> 0x10;
                Audio_Driver_SetVoiceVolume(Audio_Cutscene_streamState.voiceIndex, temp_s0, 0, 0);
                Audio_Driver_SetVoiceVolume(
                    Audio_Cutscene_streamState.voiceIndex + 1, 0, temp_s0, 0);
            }
        }
        Audio_Cutscene_streamVolume = temp_s1 & 0xFFFF;
    }
    if (Audio_Mixer_tempoScaleSlideLength != 0) {
        --Audio_Mixer_tempoScaleSlideLength;
        Audio_Mixer_tempoScale += Audio_Mixer_tempoScaleStep;
    }
    if (Audio_Mixer_musicPitchScaleSlideLength != 0) {
        --Audio_Mixer_musicPitchScaleSlideLength;
        temp_s1 = Audio_Mixer_musicPitchScale + Audio_Mixer_musicPitchScaleStep;

        if ((temp_s1 & 0xFF0000) != (Audio_Mixer_musicPitchScale & 0xFF0000)) {
            var_v1 = Audio_Seq_musicChannels;
            for (var_a2 = 0x20; var_a2 != 0; --var_a2, ++var_v1) {
                var_v1->voice.dirtyFlags |= 0x10;
            }
        }
        Audio_Mixer_musicPitchScale = temp_s1;
    }
    if ((Audio_Seq_activeMusicConfig->activeChannels != 0)
        && (Audio_Seq_activeMusicConfig->masterVolumeSlideLength != 0)) {
        --Audio_Seq_activeMusicConfig->masterVolumeSlideLength;
        temp_s1 = Audio_Seq_activeMusicConfig->masterVolume + Audio_Seq_activeMusicConfig->masterVolumeStep;
        if ((temp_s1 & 0x7F0000) != (Audio_Seq_activeMusicConfig->masterVolume & 0x7F0000)) {
            Audio_SoundEffects_InvalidateMusicChannelVolumes(
                Audio_Seq_activeMusicConfig, Audio_Seq_musicChannels);
        }
        Audio_Seq_activeMusicConfig->masterVolume = temp_s1;
    }
    if ((Audio_Seq_secondaryMusicConfig != NULL)
        && (Audio_Seq_secondaryMusicConfig->activeChannels != 0)) {
        if (Audio_Seq_secondaryMusicConfig->masterVolumeSlideLength != 0) {
            --Audio_Seq_secondaryMusicConfig->masterVolumeSlideLength;
            temp_s1 = Audio_Seq_secondaryMusicConfig->masterVolume + Audio_Seq_secondaryMusicConfig->masterVolumeStep;
            if ((temp_s1 & 0x7F0000) != (Audio_Seq_secondaryMusicConfig->masterVolume & 0x7F0000)) {
                Audio_SoundEffects_InvalidateMusicChannelVolumes(
                    Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels);
            }
            Audio_Seq_secondaryMusicConfig->masterVolume = temp_s1;
        }
    }
    if (Audio_Mixer_voiceScheduler.activeVoices != 0) {
        var_a3 = Audio_Mixer_voiceScheduler.activeVoices;
        var_a1_2 = Audio_SoundEffects_channels;
        do {
        } while (0);
        for (var_a2 = 0x1000; var_a3 != 0; var_a2 <<= 1, ++var_a1_2) {
            if (!(var_a3 & var_a2)) {
                continue;
            }

            if (var_a1_2->effectVolumeSlideLength != 0) {
                --var_a1_2->effectVolumeSlideLength;
                temp_s1 = var_a1_2->effectVolume + var_a1_2->effectVolumeSlideStep;
                if ((temp_s1 & 0xFF00) != (var_a1_2->effectVolume & 0xFF00)) {
                    var_a1_2->voice.dirtyFlags |= 3;
                }
                var_a1_2->effectVolume = temp_s1;
            }

            if (var_a1_2->effectPanSlideLength != 0) {
                --var_a1_2->effectPanSlideLength;
                temp_s1 = var_a1_2->effectPan + var_a1_2->effectPanSlideStep;
                if ((temp_s1 & 0xFF00) != (var_a1_2->effectPan & 0xFF00)) {
                    var_a1_2->voice.dirtyFlags |= 3;
                }
                var_a1_2->effectPan = temp_s1;
            }

            if (var_a1_2->effectPitchScaleSlideLength != 0) {
                --var_a1_2->effectPitchScaleSlideLength;
                temp_s1 = var_a1_2->effectPitchScale + var_a1_2->effectPitchScaleSlideStep;
                if ((temp_s1 & 0xFF00) != (var_a1_2->effectPitchScale & 0xFF00)) {
                    var_a1_2->voice.dirtyFlags |= 0x10;
                }
                var_a1_2->effectPitchScale = temp_s1;
            }
            var_a3 ^= var_a2;
        }
    }
}

// decomp: SLUS_010.40 func_80019614 (src/SLUS_010.40/25AC.c:4138)
void Audio_Seq_AdvanceMusicTransition(void)
{
    int temp_v1;
    if (Audio_Driver_controlLatches.controlLatches & 0x6600) {
        temp_v1 = Audio_Seq_activeMusicConfig->timerLowerCount
                + (Audio_Seq_activeMusicConfig->timerUpperCount << 0x10);
        Audio_Seq_musicTransitionTimer = temp_v1;
        if (Audio_Seq_musicTransitionTimer == 0) {
            if (Audio_Driver_controlLatches.controlLatches & 0x100) {
                if (Audio_Driver_controlLatches.controlLatches & 0x4000) {
                    Audio_Driver_controlLatches.controlLatches =
                        (Audio_Driver_controlLatches.controlLatches & ~0x4000) | 0x2000;
                    return;
                }
                Audio_Driver_controlLatches.controlLatches =
                    (Audio_Driver_controlLatches.controlLatches & ~0x2100) | 0x1000;
                Audio_Seq_activeMusicConfig->pendingKeyOff |=
                    Audio_Seq_activeMusicConfig->activeChannels
                    & ~Audio_Seq_activeMusicConfig->pushReleaseMask;
                Audio_Seq_activeMusicConfig->activeNotes &= Audio_Seq_activeMusicConfig->pushReleaseMask;
                return;
            }
            if (Audio_Driver_controlLatches.controlLatches & 0x400) {
                Audio_Driver_controlLatches.controlLatches =
                    (Audio_Driver_controlLatches.controlLatches & ~0x400) | 0x200;
                return;
            }
            Audio_Driver_controlLatches.controlLatches =
                (Audio_Driver_controlLatches.controlLatches & ~0x1200) | 0x100;
            Audio_Seq_secondaryMusicConfig->activeNotes = 0;
            Audio_Seq_secondaryMusicConfig->pendingKeyOff |=
                Audio_Seq_secondaryMusicConfig->activeChannels;
        }
    }
}

// decomp: SLUS_010.40 func_80019704 (src/SLUS_010.40/25AC.c:4174)
int Audio_Seq_UpdateMusicSequence(Audio_SeqChannel* arg0, int arg1)
{
    Audio_SeqChannel* var_s2;
    int var_s3;
    u_int var_s1;

    int var_a0 = (u_short)(Audio_Seq_activeMusicConfig->tempo >> 16);
    u_int tempoScale = Audio_Mixer_tempoScaleLevel;

    if (tempoScale != 0) {
        if (tempoScale < 0x80) {
            var_a0 += (var_a0 * tempoScale) >> 7;
        } else {
            var_a0 = (var_a0 * tempoScale) >> 8;
        }
    }

    Audio_Seq_activeMusicConfig->tickAccumulator += var_a0;

    if ((Audio_Seq_activeMusicConfig->tickAccumulator & 0xFFFF0000) || (Audio_Cutscene_streamControlFlags & 4)) {
        Audio_Seq_activeMusicConfig->tickAccumulator = Audio_Seq_activeMusicConfig->tickAccumulator & 0xFFFF;

        do {
            var_s2 = arg0;
            var_s1 = 1;
            var_s3 = Audio_Seq_activeMusicConfig->activeChannels;

            do {
                if (var_s3 & var_s1) {
                    --var_s2->noteLength;
                    --var_s2->gateLength;
                    if (var_s2->noteLength == 0) {
                        Audio_Seq_ExecuteChannelStep(var_s2, var_s1);
                    } else if (var_s2->gateLength == 0) {
                        Audio_Seq_activeMusicConfig->pendingKeyOff |= var_s1;
                    }
                    Audio_Mixer_UpdateChannelSlidesAndDelays(var_s2, var_s1, 0);
                    var_s3 &= ~var_s1;
                }
                ++var_s2;
                var_s1 *= 2;
            } while (var_s3 != 0);

            if (Audio_Seq_activeMusicConfig->tempoSlideLength != 0) {
                --Audio_Seq_activeMusicConfig->tempoSlideLength;
                Audio_Seq_activeMusicConfig->tempo += Audio_Seq_activeMusicConfig->tempoSlideStep;
            }

            if (Audio_Seq_activeMusicConfig->reverbDepthSlideLength != 0) {
                --Audio_Seq_activeMusicConfig->reverbDepthSlideLength;
                Audio_Seq_activeMusicConfig->reverbDepth += Audio_Seq_activeMusicConfig->reverbDepthSlideStep;
                Audio_Driver_controlLatches.updateFlags |= (arg1 == 0) ? 0x80 : 0;
            }

            if (Audio_Seq_activeMusicConfig->timerLower == 0) {
                continue;
            }

            ++Audio_Seq_activeMusicConfig->timerLowerCount;

            if (Audio_Seq_activeMusicConfig->timerLowerCount
                != Audio_Seq_activeMusicConfig->timerLower) {
                continue;
            }

            Audio_Seq_activeMusicConfig->timerLowerCount = 0;
            ++Audio_Seq_activeMusicConfig->timerUpperCount;

            if (Audio_Seq_activeMusicConfig->timerUpperCount
                != Audio_Seq_activeMusicConfig->timerUpper) {
                continue;
            }

            Audio_Seq_activeMusicConfig->timerUpperCount = 0;
            ++Audio_Seq_activeMusicConfig->timerTop;

            if ((arg1 == 0) && (Audio_Seq_musicLoopsRemaining != 0)) {
                --Audio_Seq_musicLoopsRemaining;
            }

            if (Audio_Seq_activeMusicConfig == &Audio_Seq_primaryMusicConfig) {
                Audio_Seq_AdvanceMusicTransition();
            }
        } while (arg1 == 0 && Audio_Seq_musicLoopsRemaining != 0);
    }
    while (0)
        ;
    return Audio_Seq_activeMusicConfig->activeChannels;
}

// decomp: SLUS_010.40 func_800199C4 (src/SLUS_010.40/25AC.c:4264)
void Audio_Seq_UpdateSavedMusicFade(Audio_MusicConfig* arg0, Audio_SeqChannel* arg1)
{
    int temp_s0;

    if (Audio_Seq_masterFade.ticksRemaining != 0) {
        --Audio_Seq_masterFade.ticksRemaining;
        if (Audio_Seq_masterFade.ticksRemaining == 0) {
            int temp_v0 = arg0->activeChannels;
            arg0->musicId = 0;
            arg0->activeChannels = 0;
            arg0->pendingKeyOn = 0;
            arg0->activeNotes = 0;
            arg0->pendingKeyOff = temp_v0;
            return;
        }
        temp_s0 = Audio_Seq_masterFade.value + Audio_Seq_masterFade.step;
        if ((temp_s0 & 0xFFFF0000) != (Audio_Seq_masterFade.value & 0xFFFF0000)) {
            Audio_SoundEffects_InvalidateMusicChannelVolumes(arg0, arg1);
        }
        Audio_Seq_masterFade.value = temp_s0;
    }
}

// decomp: SLUS_010.40 func_80019A58 (src/SLUS_010.40/25AC.c:4287)
long Audio_Seq_TickPlayback(void)
{
    Audio_SeqChannel* var_s2_2;
    Audio_MusicConfig* temp_v0;
    Audio_MusicConfig* var_a0;
    int var_s5;
    int var_s1;
    int i;
    u_short* p;

    var_s5 = GetRCnt(RCntCNT2);
    ++Audio_Seq_tickCounter;
    if (Audio_Driver_controlLatches.controlLatches & 0x1100) {
        if (Audio_Driver_controlLatches.controlLatches & 0x100) {
            if (Audio_Driver_controlLatches.controlLatches & 0x6000) {
                Audio_Seq_secondaryMusicConfig->pendingKeyOn &= Audio_Seq_secondaryMusicConfig->resumeKeyOnMask;
                Audio_Seq_secondaryMusicConfig->activeNotes &= Audio_Seq_secondaryMusicConfig->resumeKeyOnMask;
            } else {
                Audio_Seq_secondaryMusicConfig->pendingKeyOn = 0;
                Audio_Seq_secondaryMusicConfig->activeNotes = 0;
            }
            var_a0 = Audio_Seq_activeMusicConfig;
        } else {
            if (Audio_Driver_controlLatches.controlLatches & 0x600) {
                Audio_Seq_activeMusicConfig->pendingKeyOn &=
                    Audio_Seq_activeMusicConfig->resumeKeyOnMask | Audio_Seq_activeMusicConfig->pushReleaseMask;
                Audio_Seq_activeMusicConfig->activeNotes &=
                    Audio_Seq_activeMusicConfig->resumeKeyOnMask | Audio_Seq_activeMusicConfig->pushReleaseMask;
            } else {
                Audio_Seq_activeMusicConfig->pendingKeyOn &= Audio_Seq_activeMusicConfig->pushReleaseMask;
                Audio_Seq_activeMusicConfig->activeNotes &= Audio_Seq_activeMusicConfig->pushReleaseMask;
            }
            var_a0 = Audio_Seq_secondaryMusicConfig;
        }
        var_a0->pendingKeyOn &= ~var_a0->resumeKeyOnMask;
        var_a0->activeNotes &= ~var_a0->resumeKeyOnMask;
    }
    if ((Audio_Seq_activeMusicConfig->pendingKeyOff != 0) || (Audio_Mixer_pendingKeyOffVoices != 0)
        || (Audio_Seq_secondaryMusicConfig != NULL
            && Audio_Seq_secondaryMusicConfig->pendingKeyOff != 0)) {
        Audio_Mixer_ReleasePendingVoices();
    }

    if (Audio_Seq_secondaryMusicConfig != NULL) {
        if (Audio_Seq_secondaryMusicConfig->activeChannels == 0) {
            Audio_Seq_secondaryMusicConfig = NULL;
        } else if ((Audio_Seq_activeMusicConfig->activeChannels | Audio_Seq_activeMusicConfig->pausedChannels)
                   == 0) {
            Core_Memory_CopyWords(Audio_Seq_secondaryMusicConfig, Audio_Seq_activeMusicConfig, 0x80);
            Core_Memory_CopyWords(Audio_Seq_secondaryMusicChannels, Audio_Seq_musicChannels, 0x2200);
            temp_v0 = Audio_Seq_secondaryMusicConfig;
            Audio_Seq_secondaryMusicConfig = NULL;
            temp_v0->musicId = 0;
            temp_v0->activeChannels = 0;
        }
    }
    p = (u_short*)Audio_Seq_pendingMusicData.param1;
    if (p != 0) {
        if (Audio_Seq_activeMusicConfig->activeChannels == 0) {
            Audio_Seq_embeddedSoundEffectParams.param1 = (uintptr_t)p;
            Audio_Seq_embeddedSoundEffectParams.param3 = p[2];
            Audio_Seq_StartOrResumeMusic(&Audio_Seq_embeddedSoundEffectParams);

            var_s2_2 = Audio_Seq_musicChannels;
            for (i = 32; i != 0; --i) {
                var_s2_2->noteLength = 1;
                ++var_s2_2;
            }

            Audio_Seq_pendingMusicData.param1 = 0;
        }
    }
    if (((Audio_Driver_pendingUpdates | Audio_Seq_activeMusicConfig->activeNotes | Audio_Mixer_keyedVoices) != 0)
        || ((Audio_Seq_secondaryMusicConfig != NULL)
            && (Audio_Seq_secondaryMusicConfig->activeNotes != 0))) {
        ((void (*)())Audio_Mixer_CommitPlaybackVoices)();
    }
    if (Audio_Seq_activeMusicConfig->activeChannels != 0) {
        Audio_Seq_UpdateMusicSequence(Audio_Seq_musicChannels, 0);
    }
    if (Audio_Seq_secondaryMusicConfig != NULL) {
        if (Audio_Seq_secondaryMusicConfig->activeChannels != 0) {
            Audio_Seq_activeMusicConfig = Audio_Seq_secondaryMusicConfig;
            Audio_Seq_UpdateMusicSequence(Audio_Seq_secondaryMusicChannels, 1);
            Audio_Seq_activeMusicConfig = &Audio_Seq_primaryMusicConfig;
        }
        if (Audio_Driver_controlLatches.mixBehavior & 0x100) {
            if (!(Audio_Seq_tickCounter & 3)) {
                Audio_Seq_UpdateSavedMusicFade(Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels);
                if (Audio_Seq_secondaryMusicConfig->activeChannels == 0) {
                    Audio_Driver_controlLatches.mixBehavior &= ~0x100;
                }
            }
        }
    }
    if (Audio_Mixer_voiceScheduler.activeVoices != 0) {
        i = Audio_Mixer_voiceScheduler.activeVoices;
        Audio_Mixer_voiceScheduler.tempoAccumulator +=
            (Audio_Mixer_voiceScheduler.effectTempo >> 0x10) & 0xFFFF;
        if ((Audio_Mixer_voiceScheduler.tempoAccumulator & 0xFFFF0000)
            || (Audio_Cutscene_streamControlFlags & 4)) {
            Audio_Mixer_voiceScheduler.tempoAccumulator &= 0xFFFF;
            var_s1 = 0x1000;
            var_s2_2 = Audio_SoundEffects_channels;
            do {
                if (i & var_s1) {
                    if (!(Audio_Cutscene_streamControlFlags & 2) || (var_s2_2->effectTags & 0x02000000)) {
                        ++var_s2_2->priority;
                        --var_s2_2->noteLength;
                        --var_s2_2->gateLength;
                        if (var_s2_2->noteLength == 0) {
                            Audio_Seq_ExecuteChannelStep(var_s2_2, var_s1);
                        } else if (var_s2_2->gateLength == 0) {
                            Audio_Mixer_voiceScheduler.pendingKeyOff |= var_s1;
                            Audio_Mixer_voiceScheduler.keyedVoices &= ~var_s1;
                        }
                        Audio_Mixer_UpdateChannelSlidesAndDelays(var_s2_2, var_s1, 1);
                    }
                    i ^= var_s1;
                }
                ++var_s2_2;
                var_s1 *= 2;
            } while (i != 0);
        }
    }
    if (!(Audio_Seq_tickCounter & 3)) {
        Audio_Seq_UpdateGlobalSlides();
    }
    var_s5 = GetRCnt(RCntCNT2) - var_s5;
    if (var_s5 <= 0) {
        var_s5 += 0x44E8;
    }

    Audio_Driver_recentTickCosts[0] = Audio_Driver_recentTickCosts[1];
    Audio_Driver_recentTickCosts[1] = Audio_Driver_recentTickCosts[2];
    Audio_Driver_recentTickCosts[2] = Audio_Driver_recentTickCosts[3];
    Audio_Driver_recentTickCosts[3] = var_s5;
    Audio_Driver_recentTickCostTotal = Audio_Driver_recentTickCosts[0] + Audio_Driver_recentTickCosts[1] + Audio_Driver_recentTickCosts[2] + Audio_Driver_recentTickCosts[3];
    return var_s5;
}

// decomp: SLUS_010.40 func_80019FC4 (src/SLUS_010.40/25AC.c:4430)
char Audio_Seq_PeekNextNote(Audio_SeqChannel* channel)
{
    u_char* pc = channel->programCounter;
    u_int loopTop = channel->loopDepth;
    Audio_MusicConfig* config = Audio_Seq_activeMusicConfig;
    int length;
    int jumpOffset;

    for (;;) {
        if (*pc < 0x9A) {
            if (*pc >= 0x8F) {
                channel->playModes &= ~5;
            }
            return *pc;
        }
        if (*pc < 0xA0) {
            return 0xA0;
        }
        length = Audio_Seq_kOpcodeLengths[*pc - 0xA0];
        if (length != 0) {
            pc += length;
            continue;
        }
        switch (*pc) {
        case 0xF0:
        case 0xF1:
        case 0xF2:
        case 0xF3:
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
            return 0x83;
        case 0xFC:
            return 0x84;
        case 0xFD:
            return 0x8F;
        case 0xFE:
            pc++;
            length = Audio_Seq_kExtendedOpcodeLengths[*pc];
            if (length != 0) {
                pc += length;
                break;
            }
            switch (*pc) {
            case 0x08:
            case 0x09:
                pc++;
                if (*pc == channel->loopIterations[loopTop] + 1) {
                    pc++;
                    loopTop--;
                    loopTop &= 3;
                    jumpOffset = pc[0];
                    jumpOffset += pc[1] << 8;
                    pc += (short)jumpOffset;
                } else {
                    pc += 3;
                }
                break;
            case 0x06:
            case 0x0E:
                pc++;
                jumpOffset = pc[0];
                jumpOffset += pc[1] << 8;
                pc += (short)jumpOffset;
                break;
            case 0x07:
                pc++;
                if (*pc++ <= config->jumpThreshold) {
                    jumpOffset = pc[0];
                    jumpOffset += pc[1] << 8;
                    pc += (short)jumpOffset;
                } else {
                    pc += 2;
                }
                break;
            case 0x0F:
                pc = channel->returnAddress;
                break;
            case 0x1F:
                pc++;
                if (config->statusFlags & 0x10000) {
                    return 0xA0;
                }
                break;
            }
            break;
        case 0xC9:
            pc++;
            if (*pc == channel->loopIterations[loopTop] + 1) {
                pc++;
                loopTop--;
                loopTop &= 3;
            } else {
                pc = channel->loopStarts[loopTop];
            }
            break;
        case 0xCB:
        case 0xCD:
        case 0xD1:
        case 0xDB:
            channel->playModes &= ~5;
            pc++;
            break;
        case 0xCA:
            if (!(channel->updateFlags & 0x200000)) {
                pc = channel->loopStarts[loopTop];
                break;
            }
        default:
            channel->playModes &= ~5;
            return 0xA0;
        }
    }
}

// decomp: SLUS_010.40 func_8001A1F4 (src/SLUS_010.40/25AC.c:4550)
int Audio_Seq_ResolveSoundEffectInstrument(int arg0, int arg1)
{
    if ((arg1 - 0x80u) < 0x20) {
        if (arg0 & 2) {
            return arg1 + 0x20;
        }
        if (arg0 & 4) {
            return arg1 + 0x40;
        }
    }
    return arg1;
}

// decomp: SLUS_010.40 func_8001A22C (src/SLUS_010.40/25AC.c:4563)
int Audio_Seq_ResolveMusicInstrument(int arg0, u_int arg1)
{
    if ((arg0 & 0x40) && (arg1 >= 0x40) && (arg1 < 0x80)) {
        return arg1 + 0x20;
    }
    return arg1;
}

// decomp: SLUS_010.40 func_8001A258 (src/SLUS_010.40/25AC.c:4571)
void Audio_Seq_SelectNotePatch(Audio_SeqChannel* arg0, u_int arg1)
{
    Audio_Instrument* temp_a0;
    char* var_s0;
    int temp_v0;
    int temp_s2;

    if ((arg0->key < arg1) || (arg0->key == 0xFF)) {
        var_s0 = arg0->keymap;
        while (var_s0[13] != 0 && var_s0[2] < arg1) {
            var_s0 += 8;
        }
    } else {
        if (arg1 >= arg0->key) {
            return;
        }
        var_s0 = arg0->keymap;
        while (var_s0[13] != 0 && arg1 >= var_s0[9]) {
            var_s0 += 8;
        }
    }

    temp_s2 = arg0->updateFlags;

    temp_v0 = Audio_Seq_ResolveMusicInstrument(Audio_Seq_activeMusicConfig->statusFlags, var_s0[0]);
    arg0->instrumentIndex = temp_v0;
    temp_a0 = &Audio_Samples_instruments[temp_v0];
    arg0->voice.sampleStart = temp_a0->sampleStart;
    arg0->voice.loopStart = temp_a0->loopStart;

    if (!(temp_s2 & 0x01000000)) {
        arg0->voice.adsrLower = var_s0[3] << 8;
    } else {
        arg0->voice.adsrLower &= 0x7F00;
    }

    arg0->voice.adsrLower |= temp_a0->adsrLower & 0x80FF;

    if (!(temp_s2 & 0x08000000)) {
        arg0->voice.adsrUpper &= 0x201F;
        arg0->voice.adsrUpper |= var_s0[4] << 6;
    } else {
        arg0->voice.adsrUpper &= 0x3FDF;
    }

    switch (var_s0[5]) {
    case 3:
        arg0->voice.adsrUpper |= 0x4000;
        break;
    case 5:
        arg0->voice.adsrUpper |= 0x8000;
        break;
    case 7:
        arg0->voice.adsrUpper |= 0xC000;
        break;
    }

    if ((temp_s2 & 0x10000000) == 0) {
        arg0->voice.adsrUpper &= 0xFFE0;
        arg0->voice.adsrUpper |= var_s0[6];
    }

    arg0->voice.adsrUpper |= temp_a0->adsrUpper & 0x20;
    arg0->voice.volumeScale = var_s0[7];
}

// decomp: SLUS_010.40 func_8001A4BC (src/SLUS_010.40/25AC.c:4637)
int Audio_Seq_CalculateNotePitch(Audio_Instrument* arg0, int arg1, u_int arg2, int* arg3)
{
    int var_a0;
    int var_t0;

    var_a0 = arg1 - arg0->rootNote;

    while (var_a0 < 0) {
        var_a0 += 0xC;
    }

    var_a0 %= 12;

    if (arg0->fineTune == 0) {
        var_t0 = Audio_Seq_kSemitonePitches[var_a0] << 8;
    } else if (arg0->fineTune < 0) {
        var_t0 = (Audio_Seq_kSemitonePitches[var_a0] * (u_short)arg0->fineTune) >> 8;
    } else {
        var_t0 = (Audio_Seq_kSemitonePitches[var_a0] * arg0->fineTune) >> 7;
        var_t0 += Audio_Seq_kSemitonePitches[var_a0] << 8;
    }

    arg2 &= 0xFF;

    if (arg2 != 0) {
        if (arg2 < 0x80) {
            *arg3 = (var_t0 * arg2) >> 7;
        } else {
            *arg3 = ((var_t0 * arg2) >> 8) - var_t0;
        }
    }

    if (arg1 < arg0->rootNote) {
        for (; arg1 < arg0->rootNote; arg1 += 0xC) {
            *arg3 = *arg3 >> 1;
            var_t0 = (var_t0 >> 1);
        }
    } else {
        var_a0 = (arg1 - arg0->rootNote) / 12;
        if (var_a0 != 0) {
            var_t0 <<= var_a0;
            *arg3 <<= var_a0;
        }
    }
    var_t0 >>= 8;
    *arg3 >>= 8;
    return (u_short)var_t0;
}

// decomp: SLUS_010.40 func_8001A64C (src/SLUS_010.40/25AC.c:4686)
int Audio_Seq_StartDrumNote(Audio_SeqChannel* arg0, int arg1, int arg2)
{
    Audio_Instrument* temp_a0;
    int temp_v0;
    int temp_v0_2;
    int temp_s2;
    char* temp_s1;

    temp_s1 = (char*)Audio_Seq_activeMusicConfig->keymapTable;
    temp_s1 += arg2 * 8;
    Audio_Seq_activeMusicConfig->pendingKeyOn |= arg1;

    if (Audio_Seq_activeMusicConfig->activeNotes & arg1) {
        Audio_Seq_activeMusicConfig->pendingKeyOff |= arg1;
    }

    temp_s2 = arg0->updateFlags;
    temp_v0_2 = Audio_Seq_ResolveMusicInstrument(Audio_Seq_activeMusicConfig->statusFlags, (u_int)temp_s1[0]);
    arg0->instrumentIndex = temp_v0_2;
    temp_a0 = &Audio_Samples_instruments[temp_v0_2];
    arg0->voice.sampleStart = temp_a0->sampleStart;
    arg0->voice.loopStart = temp_a0->loopStart;

    if (!(temp_s2 & 0x01000000)) {
        arg0->voice.adsrLower = temp_s1[2] << 8;
    } else {
        arg0->voice.adsrLower &= 0x7F00;
    }

    arg0->voice.adsrLower |= temp_a0->adsrLower & 0x80FF;

    if (!(temp_s2 & 0x08000000)) {
        arg0->voice.adsrUpper &= 0x201F;
        arg0->voice.adsrUpper |= temp_s1[3] << 6;
    } else {
        arg0->voice.adsrUpper &= 0x3FDF;
    }

    switch (temp_s1[4]) {
    case 3:
        arg0->voice.adsrUpper |= 0x4000;
        break;
    case 5:
        arg0->voice.adsrUpper |= 0x8000;
        break;
    case 7:
        arg0->voice.adsrUpper |= 0xC000;
        break;
    }

    if ((temp_s2 & 0x10000000) == 0) {
        arg0->voice.adsrUpper &= 0xFFE0;
        arg0->voice.adsrUpper |= temp_s1[5];
    }

    arg0->voice.adsrUpper |= temp_a0->adsrUpper & 0x20;
    temp_v0 = Audio_Seq_CalculateNotePitch(temp_a0, temp_s1[1], arg0->fineTune, &arg0->finePitchDelta);
    arg0->voice.volumeScale = temp_s1[6];
    arg0->pan = ((temp_s1[7] & 0x7F) + 0x40) << 8;

    if (temp_s1[7] & 0x80) {
        Audio_Seq_activeMusicConfig->reverbChannels |= arg1;
    } else {
        Audio_Seq_activeMusicConfig->reverbChannels &= ~arg1;
    }
    Audio_Driver_controlLatches.updateFlags |= 0x100;
    return temp_v0;
}

// decomp: SLUS_010.40 func_8001A8D8 (src/SLUS_010.40/25AC.c:4760)
void Audio_Seq_ExecuteChannelStep(Audio_SeqChannel* arg0, int arg1)
{
    u_int sp10;
    u_int temp_a2_2;
    u_int temp_s0;
    u_int temp_v0;
    u_char* temp_a1;
    int var_v1;
    u_int temp_a1_2;
    u_int temp_v1;
    u_int temp_v1_6;
    u_int var_s0;

    do {
        temp_a1 = arg0->programCounter;
        var_s0 = temp_a1[0];
        arg0->programCounter = ++temp_a1;

        if (var_s0 >= 0xA0) {
            temp_v1 = var_s0 - 0xF0;
            if (var_s0 == 0xFE) {
                temp_a2_2 = temp_a1[0];
                arg0->programCounter = temp_a1 + 1;
                Audio_Seq_kExtendedOpcodeHandlers[temp_a2_2](arg0, arg1);
            } else {
                if (temp_v1 < 0xE) {
                    var_s0 = temp_v1 * 0xB;
                    arg0->noteLength = temp_a1[0];
                    arg0->programCounter = temp_a1 + 1;
                } else {
                    if (var_s0 == 0xFF) {
                        var_s0 = 0xA0;
                    } else if ((var_s0 == 0xCA) && (arg0->updateFlags & 0x200000)) {
                        var_s0 = 0xA0;
                        Audio_Mixer_voiceScheduler.pendingKeyOff |= arg1;
                    }
                    Audio_Seq_kOpcodeHandlers[var_s0 - 0xA0](arg0, arg1);
                }
            }
        }
        ++arg0->opcodeStepCounter;
    } while (var_s0 > 0xA0);

    if (var_s0 == 0xA0) {
        if (arg0->type == 0) {
            Audio_Seq_activeMusicConfig->pendingKeyOff |= arg1;
        }
    } else {
        temp_a2_2 = Audio_Seq_PeekNextNote(arg0);
        if (arg0->fixedNoteLength != 0) {
            arg0->noteLength = arg0->gateLength = arg0->fixedNoteLength;
        }
        if (arg0->noteLength != 0) {
            if ((temp_a2_2 >= 0x8F) || ((temp_a2_2 < 0x84) && !(arg0->playModes & 5))) {
                arg0->gateLength -= 2;
            }
        } else {
            int tmp = Audio_Seq_kNoteLengths[var_s0 % 11];
            arg0->noteLength = tmp;
            var_v1 = tmp;
            if (((temp_a2_2 - 0x84) >= 0xB) && !(arg0->playModes & 5)) {
                var_v1 -= 2;
            }
            arg0->gateLength = var_v1;
        }
        if ((arg0->type == 0) && (arg0->updateFlags & 0x40)) {
            arg0->gateLength = arg0->noteLength;
        }

        arg0->storedNoteLength = arg0->noteLength;
        arg0->voice.dirtyFlags |= 0x4000;

        if (var_s0 >= 0x8F) {
            if (arg0->type == 0) {
                Audio_Seq_activeMusicConfig->activeNotes &= ~arg1;
                if (arg0->voice.assignedVoice < 0x18) {
                    Audio_Seq_activeMusicConfig->pendingKeyOff |= arg1;
                }
            }
            arg0->portamentoSteps = 0;
            arg0->vibratoPitch = 0;
            arg0->tremoloVolume = 0;
            arg0->playModes &= 0xFFFD;
            return;
        }
        if (var_s0 < 0x84) {
            temp_a1_2 = arg0->updateFlags;
            temp_v0 = arg0->octave * 0xC;
            temp_s0 = var_s0 / 11;
            temp_s0 += temp_v0;
            if (temp_a1_2 & 8) {
                temp_a2_2 = Audio_Seq_StartDrumNote(arg0, arg1, temp_s0);
            } else {
                if (!(arg0->playModes & 2)) {
                    if (arg0->type == 0) {
                        if (Audio_Driver_mixBehavior & 0x10) {
                            temp_s0 += *((signed char*)Audio_Seq_activeMusicConfig->noteRemapTable
                                         + ((temp_s0) % 12));
                        }
                        if (temp_a1_2 & 0x1000) {
                            Audio_Seq_SelectNotePatch(arg0, temp_s0);
                        }
                        Audio_Seq_activeMusicConfig->pendingKeyOn |= arg1;
                        if ((Audio_Seq_activeMusicConfig->activeNotes & arg1)
                            && (arg0->voice.assignedVoice < 0x18)) {
                            Audio_Seq_activeMusicConfig->pendingKeyOff |= arg1;
                        }

                        if (arg0->keyOnVolumeSlideLength != 0) {
                            arg0->volumeSlideLength = arg0->keyOnVolumeSlideLength;
                            arg0->volume = arg0->keyOnVolume;
                            arg0->volumeSlideStep = arg0->keyOnVolumeSlideStep;
                        }
                    } else {
                        Audio_Mixer_voiceScheduler.pendingKeyOn |= arg1;
                    }
                    arg0->pitchSlideTicksLeft = 0;
                }

                if ((arg0->portamentoSteps != 0) && (arg0->portamentoKey != 0)) {
                    arg0->pitchBendSlideLength = arg0->portamentoSteps;
                    arg0->pitchBendTarget =
                        ((arg0->transpose + temp_s0) - arg0->portamentoKey)
                        - arg0->portamentoTranspose;
                    arg0->key =
                        arg0->portamentoKey - (arg0->transpose - arg0->portamentoTranspose);
                    temp_s0 = arg0->portamentoKey + arg0->portamentoTranspose;
                } else {
                    arg0->key = temp_s0;
                    temp_s0 = temp_s0 + arg0->transpose;
                }

                temp_a2_2 = Audio_Seq_CalculateNotePitch(&Audio_Samples_instruments[arg0->instrumentIndex],
                    temp_s0, arg0->fineTune, &arg0->finePitchDelta);
                if (arg0->randomPitchDepth != 0) {
                    sp10 = (temp_a2_2 * arg0->randomPitchDepth) >> 8;
                    sp10 *= Audio_Seq_kRandomPitchOffsets[Audio_Seq_randomPitchPhase];
                    if (Audio_Seq_kRandomPitchOffsets[Audio_Seq_randomPitchPhase] & 0x80) {
                        sp10 >>= 9;
                        temp_a2_2 -= sp10;
                    } else {
                        sp10 >>= 7;
                        temp_a2_2 += sp10;
                    }
                }
            }
            arg0->pitchBase = temp_a2_2;
            if (arg0->type == 0) {
                Audio_Seq_activeMusicConfig->activeNotes |= arg1;
            } else {
                Audio_Mixer_voiceScheduler.keyedVoices |= arg1;
            }
            temp_s0 = arg0->updateFlags;
            arg0->voice.dirtyFlags |= 0x13;
            if (temp_s0 & 1) {
                temp_v1_6 = ((arg0->vibratoDepth & 0x7F00) >> 8);
                arg0->vibratoBase = (!(arg0->vibratoDepth & 0x8000)
                                            ? (temp_v1_6 * ((temp_a2_2 * 0xF) >> 8))
                                            : (temp_v1_6 * temp_a2_2))
                                 >> 7;
                if (!(arg0->playModes & 2)) {
                    arg0->vibratoWave = Audio_Seq_kLfoWaveforms[arg0->vibratoShape];
                    arg0->vibratoDelayLeft = arg0->vibratoDelay;
                    arg0->vibratoRateCounter = 1;
                }
            }
            if ((temp_s0 & 2) && !(arg0->playModes & 2)) {
                arg0->tremoloWave = Audio_Seq_kLfoWaveforms[arg0->tremoloShape];
                arg0->tremoloDelayLeft = arg0->tremoloDelay;
                arg0->tremoloRateCounter = 1;
            }
            arg0->vibratoPitch = 0;
            arg0->tremoloVolume = 0;
            arg0->pitchSlide = 0;
        }

        arg0->playModes = (arg0->playModes & 0xFFFD) | ((arg0->playModes & 1) * 2);
        if (arg0->pitchBendTarget != 0) {
            arg0->key += arg0->pitchBendTarget;
            temp_a2_2 = Audio_Seq_CalculateNotePitch(&Audio_Samples_instruments[arg0->instrumentIndex],
                arg0->key + arg0->transpose, arg0->fineTune, (int*)&sp10);
            temp_a2_2 <<= 0x10;
            arg0->pitchSlideTicksLeft = arg0->pitchBendSlideLength;
            arg0->pitchBendTarget = 0;
            arg0->pitchSlideStep =
                ((int)temp_a2_2 - ((arg0->pitchBase << 0x10) + arg0->pitchSlide))
                / arg0->pitchSlideTicksLeft;
        }
        arg0->portamentoKey = arg0->key;
        arg0->portamentoTranspose = arg0->transpose;
    }
}

// decomp: SLUS_010.40 Sound_CopyInstrumentInfoToChannel (src/SLUS_010.40/25AC.c:4953)
void Audio_Seq_ApplyInstrumentParameters(
    Audio_SeqChannel* arg0, Audio_Instrument* arg1, int arg2)
{
    arg0->voice.sampleStart = arg2;
    arg0->voice.loopStart = arg1->loopStart;
    arg0->voice.adsrLower = arg1->adsrLower;
    arg0->voice.adsrUpper = arg1->adsrUpper;
    arg0->voice.dirtyFlags |= 0x1FF80;
}

// decomp: SLUS_010.40 func_8001B094 (src/SLUS_010.40/25AC.c:4963)
void Audio_Seq_SelectChannelInstrument(Audio_SeqChannel* arg0, int arg1)
{
    arg0->instrumentIndex = arg1;
    Audio_Seq_ApplyInstrumentParameters(
        arg0, &Audio_Samples_instruments[arg1], Audio_Samples_instruments[arg1].sampleStart);
}

// decomp: SLUS_010.40 Sound_ClearVoiceFromSchedulerState (src/SLUS_010.40/25AC.c:4970)
void Audio_Seq_ReleaseSoundEffectVoice(Audio_SeqChannel* arg0, int arg1)
{
    int temp_a1 = ~arg1;
    Audio_Mixer_voiceScheduler.activeVoices &= temp_a1;
    Audio_Mixer_voiceScheduler.pausedEffectVoices &= temp_a1;
    Audio_Mixer_voiceScheduler.noiseVoices &= temp_a1;
    Audio_Mixer_voiceScheduler.reverbVoices &= temp_a1;
    Audio_Mixer_voiceScheduler.fmVoices &= temp_a1;
    Audio_Mixer_voiceScheduler.pendingKeyOn &= temp_a1;
    Audio_Mixer_voiceScheduler.keyedVoices &= temp_a1;
    arg0->effectTags = 0;
    arg0->effectId = 0;
}

