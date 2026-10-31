// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/LfoWaveforms.h"
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
#include "Audio/StreamPlayback.h"
#include "Audio/XaPlayback.h"
#include "Core/Memory/Copy.h"

// decomp: D_8002F54C (D_8002F54C@SLUS_010.40)
u_short Audio_Seq_tickCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F5A0 (D_8002F5A0@SLUS_010.40)
u_char Audio_Seq_kOpcodeLengths[152]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F600 (D_8002F600@SLUS_010.40)
u_char Audio_Seq_kExtendedOpcodeLengths[56]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F620 (D_8002F620@SLUS_010.40)
u_short Audio_Seq_kNoteLengths[12]; /* contents: module image (ModuleData.cpp) */

// decomp: g_Sound_ProgramCounter (g_Sound_ProgramCounter@SLUS_010.40)
u_int Audio_Seq_kIdleProgram; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F63C (D_8002F63C@SLUS_010.40)
u_int Audio_Seq_kSemitonePitches[88]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F66C (D_8002F66C@SLUS_010.40)
char Audio_Seq_kNoteRemapTables[304]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F69C (D_8002F69C@SLUS_010.40)
char Audio_Seq_kRandomPitchOffsets[256]; /* contents: module image (ModuleData.cpp) */

// decomp: g_Sound_LfoTable (g_Sound_LfoTable@SLUS_010.40)
short* Audio_Seq_kLfoWaveforms[256]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800305E0 (D_800305E0@SLUS_010.40)
void (*Audio_Seq_kOpcodeHandlers[96])(Audio_SeqChannel*, int); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_80030760 (D_80030760@SLUS_010.40)
void (*Audio_Seq_kExtendedOpcodeHandlers[32])(Audio_SeqChannel*, int); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_80037800 (D_80037800@SLUS_010.40)
Audio_CommandParams Audio_Seq_embeddedSoundEffectParams;

// decomp: g_Sound_MasterFadeTimer (g_Sound_MasterFadeTimer@SLUS_010.40)
Audio_FadeTimer Audio_Seq_masterFade;

// decomp: g_Sound_LfoPhase (g_Sound_LfoPhase@SLUS_010.40)
int Audio_Seq_randomPitchPhase;

// decomp: SLUS_010.40 func_8001924C (src/SLUS_010.40/25AC.c:4024)
void Audio_Seq_UpdateGlobalSlides(void)
{
    int next_slide_value;
    int channel_count_or_voice_bit;
    Audio_SeqChannel* effect_channel;
    int effects_to_update;
    Audio_SeqChannel* music_channel;

    Audio_Seq_randomPitchPhase = (Audio_Seq_randomPitchPhase + 1) & 0xFF;
    if (Audio_Seq_discVolumeFadeLength != 0) {
        --Audio_Seq_discVolumeFadeLength;
        Audio_Seq_discVolume += Audio_Seq_discVolumeFadeStep;
        Audio_Seq_WriteDiscVolume();
    }
    Audio_Native_UpdateCutsceneStream();
    if (Audio_Mixer_tempoScaleSlideLength != 0) {
        --Audio_Mixer_tempoScaleSlideLength;
        Audio_Mixer_tempoScale += Audio_Mixer_tempoScaleStep;
    }
    if (Audio_Mixer_musicPitchScaleSlideLength != 0) {
        --Audio_Mixer_musicPitchScaleSlideLength;
        next_slide_value = Audio_Mixer_musicPitchScale + Audio_Mixer_musicPitchScaleStep;

        if ((next_slide_value & 0xFF0000) != (Audio_Mixer_musicPitchScale & 0xFF0000)) {
            music_channel = Audio_Seq_musicChannels;
            for (channel_count_or_voice_bit = 0x20; channel_count_or_voice_bit != 0; --channel_count_or_voice_bit, ++music_channel) {
                music_channel->voice.dirtyFlags |= 0x10;
            }
        }
        Audio_Mixer_musicPitchScale = next_slide_value;
    }
    if ((Audio_Seq_activeMusicConfig->activeChannels != 0)
        && (Audio_Seq_activeMusicConfig->masterVolumeSlideLength != 0)) {
        --Audio_Seq_activeMusicConfig->masterVolumeSlideLength;
        next_slide_value = Audio_Seq_activeMusicConfig->masterVolume + Audio_Seq_activeMusicConfig->masterVolumeStep;
        if ((next_slide_value & 0x7F0000) != (Audio_Seq_activeMusicConfig->masterVolume & 0x7F0000)) {
            Audio_SoundEffects_InvalidateMusicChannelVolumes(
                Audio_Seq_activeMusicConfig, Audio_Seq_musicChannels);
        }
        Audio_Seq_activeMusicConfig->masterVolume = next_slide_value;
    }
    if ((Audio_Seq_secondaryMusicConfig != NULL)
        && (Audio_Seq_secondaryMusicConfig->activeChannels != 0)) {
        if (Audio_Seq_secondaryMusicConfig->masterVolumeSlideLength != 0) {
            --Audio_Seq_secondaryMusicConfig->masterVolumeSlideLength;
            next_slide_value = Audio_Seq_secondaryMusicConfig->masterVolume + Audio_Seq_secondaryMusicConfig->masterVolumeStep;
            if ((next_slide_value & 0x7F0000) != (Audio_Seq_secondaryMusicConfig->masterVolume & 0x7F0000)) {
                Audio_SoundEffects_InvalidateMusicChannelVolumes(
                    Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels);
            }
            Audio_Seq_secondaryMusicConfig->masterVolume = next_slide_value;
        }
    }
    if (Audio_Mixer_voiceScheduler.activeVoices != 0) {
        effects_to_update = Audio_Mixer_voiceScheduler.activeVoices;
        effect_channel = Audio_SoundEffects_channels;
        do {
        } while (0);
        for (channel_count_or_voice_bit = 0x1000; effects_to_update != 0; channel_count_or_voice_bit <<= 1, ++effect_channel) {
            if (!(effects_to_update & channel_count_or_voice_bit)) {
                continue;
            }

            if (effect_channel->effectVolumeSlideLength != 0) {
                --effect_channel->effectVolumeSlideLength;
                next_slide_value = effect_channel->effectVolume + effect_channel->effectVolumeSlideStep;
                if ((next_slide_value & 0xFF00) != (effect_channel->effectVolume & 0xFF00)) {
                    effect_channel->voice.dirtyFlags |= 3;
                }
                effect_channel->effectVolume = next_slide_value;
            }

            if (effect_channel->effectPanSlideLength != 0) {
                --effect_channel->effectPanSlideLength;
                next_slide_value = effect_channel->effectPan + effect_channel->effectPanSlideStep;
                if ((next_slide_value & 0xFF00) != (effect_channel->effectPan & 0xFF00)) {
                    effect_channel->voice.dirtyFlags |= 3;
                }
                effect_channel->effectPan = next_slide_value;
            }

            if (effect_channel->effectPitchScaleSlideLength != 0) {
                --effect_channel->effectPitchScaleSlideLength;
                next_slide_value = effect_channel->effectPitchScale + effect_channel->effectPitchScaleSlideStep;
                if ((next_slide_value & 0xFF00) != (effect_channel->effectPitchScale & 0xFF00)) {
                    effect_channel->voice.dirtyFlags |= 0x10;
                }
                effect_channel->effectPitchScale = next_slide_value;
            }
            effects_to_update ^= channel_count_or_voice_bit;
        }
    }
}

// decomp: SLUS_010.40 func_80019614 (src/SLUS_010.40/25AC.c:4138)
void Audio_Seq_AdvanceMusicTransition(void)
{
    int packed_music_timer;
    if (Audio_Driver_controlLatches.controlLatches & 0x6600) {
        packed_music_timer = Audio_Seq_activeMusicConfig->timerLowerCount
                + (Audio_Seq_activeMusicConfig->timerUpperCount << 0x10);
        Audio_Seq_musicTransitionTimer = packed_music_timer;
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
int Audio_Seq_UpdateMusicSequence(Audio_SeqChannel* channels, int secondary_music)
{
    Audio_SeqChannel* channel_cursor;
    int channels_to_update;
    u_int channel_bit;

    int tempo_increment = (u_short)(Audio_Seq_activeMusicConfig->tempo >> 16);
    u_int tempoScale = Audio_Mixer_tempoScaleLevel;

    if (tempoScale != 0) {
        if (tempoScale < 0x80) {
            tempo_increment += (tempo_increment * tempoScale) >> 7;
        } else {
            tempo_increment = (tempo_increment * tempoScale) >> 8;
        }
    }

    Audio_Seq_activeMusicConfig->tickAccumulator += tempo_increment;

    if ((Audio_Seq_activeMusicConfig->tickAccumulator & 0xFFFF0000) || (Audio_Cutscene_streamControlFlags & 4)) {
        Audio_Seq_activeMusicConfig->tickAccumulator = Audio_Seq_activeMusicConfig->tickAccumulator & 0xFFFF;

        do {
            channel_cursor = channels;
            channel_bit = 1;
            channels_to_update = Audio_Seq_activeMusicConfig->activeChannels;

            do {
                if (channels_to_update & channel_bit) {
                    --channel_cursor->noteLength;
                    --channel_cursor->gateLength;
                    if (channel_cursor->noteLength == 0) {
                        Audio_Seq_ExecuteChannelStep(channel_cursor, channel_bit);
                    } else if (channel_cursor->gateLength == 0) {
                        Audio_Seq_activeMusicConfig->pendingKeyOff |= channel_bit;
                    }
                    Audio_Mixer_UpdateChannelSlidesAndDelays(channel_cursor, channel_bit, 0);
                    channels_to_update &= ~channel_bit;
                }
                ++channel_cursor;
                channel_bit *= 2;
            } while (channels_to_update != 0);

            if (Audio_Seq_activeMusicConfig->tempoSlideLength != 0) {
                --Audio_Seq_activeMusicConfig->tempoSlideLength;
                Audio_Seq_activeMusicConfig->tempo += Audio_Seq_activeMusicConfig->tempoSlideStep;
            }

            if (Audio_Seq_activeMusicConfig->reverbDepthSlideLength != 0) {
                --Audio_Seq_activeMusicConfig->reverbDepthSlideLength;
                Audio_Seq_activeMusicConfig->reverbDepth += Audio_Seq_activeMusicConfig->reverbDepthSlideStep;
                Audio_Driver_controlLatches.updateFlags |= (secondary_music == 0) ? 0x80 : 0;
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

            if ((secondary_music == 0) && (Audio_Seq_musicLoopsRemaining != 0)) {
                --Audio_Seq_musicLoopsRemaining;
            }

            if (Audio_Seq_activeMusicConfig == &Audio_Seq_primaryMusicConfig) {
                Audio_Seq_AdvanceMusicTransition();
            }
        } while (secondary_music == 0 && Audio_Seq_musicLoopsRemaining != 0);
    }
    while (0)
        ;
    return Audio_Seq_activeMusicConfig->activeChannels;
}

// decomp: SLUS_010.40 func_800199C4 (src/SLUS_010.40/25AC.c:4264)
void Audio_Seq_UpdateSavedMusicFade(Audio_MusicConfig* music_config, Audio_SeqChannel* channels)
{
    int next_fade_value;

    if (Audio_Seq_masterFade.ticksRemaining != 0) {
        --Audio_Seq_masterFade.ticksRemaining;
        if (Audio_Seq_masterFade.ticksRemaining == 0) {
            int channels_to_release = music_config->activeChannels;
            music_config->musicId = 0;
            music_config->activeChannels = 0;
            music_config->pendingKeyOn = 0;
            music_config->activeNotes = 0;
            music_config->pendingKeyOff = channels_to_release;
            return;
        }
        next_fade_value = Audio_Seq_masterFade.value + Audio_Seq_masterFade.step;
        if ((next_fade_value & 0xFFFF0000) != (Audio_Seq_masterFade.value & 0xFFFF0000)) {
            Audio_SoundEffects_InvalidateMusicChannelVolumes(music_config, channels);
        }
        Audio_Seq_masterFade.value = next_fade_value;
    }
}

// decomp: SLUS_010.40 func_80019A58 (src/SLUS_010.40/25AC.c:4287)
long Audio_Seq_TickPlayback(void)
{
    Audio_SeqChannel* channel_cursor;
    Audio_MusicConfig* promoted_saved_config;
    Audio_MusicConfig* transition_config;
    int tick_counter_or_cost;
    int effect_voice_bit;
    int i;
    u_short* p;

    tick_counter_or_cost = GetRCnt(RCntCNT2);
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
            transition_config = Audio_Seq_activeMusicConfig;
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
            transition_config = Audio_Seq_secondaryMusicConfig;
        }
        transition_config->pendingKeyOn &= ~transition_config->resumeKeyOnMask;
        transition_config->activeNotes &= ~transition_config->resumeKeyOnMask;
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
            promoted_saved_config = Audio_Seq_secondaryMusicConfig;
            Audio_Seq_secondaryMusicConfig = NULL;
            promoted_saved_config->musicId = 0;
            promoted_saved_config->activeChannels = 0;
        }
    }
    p = (u_short*)Audio_Seq_pendingMusicData.param1;
    if (p != 0) {
        if (Audio_Seq_activeMusicConfig->activeChannels == 0) {
            Audio_Seq_embeddedSoundEffectParams.param1 = (uintptr_t)p;
            Audio_Seq_embeddedSoundEffectParams.param3 = p[2];
            Audio_Seq_StartOrResumeMusic(&Audio_Seq_embeddedSoundEffectParams);

            channel_cursor = Audio_Seq_musicChannels;
            for (i = 32; i != 0; --i) {
                channel_cursor->noteLength = 1;
                ++channel_cursor;
            }

            Audio_Seq_pendingMusicData.param1 = 0;
        }
    }
    if (((Audio_Driver_pendingUpdates | Audio_Seq_activeMusicConfig->activeNotes | Audio_Mixer_keyedVoices) != 0)
        || ((Audio_Seq_secondaryMusicConfig != NULL)
            && (Audio_Seq_secondaryMusicConfig->activeNotes != 0))) {
        ((void (*)(...))Audio_Mixer_CommitPlaybackVoices)();
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
            effect_voice_bit = 0x1000;
            channel_cursor = Audio_SoundEffects_channels;
            do {
                if (i & effect_voice_bit) {
                    if (!(Audio_Cutscene_streamControlFlags & 2) || (channel_cursor->effectTags & 0x02000000)) {
                        ++channel_cursor->priority;
                        --channel_cursor->noteLength;
                        --channel_cursor->gateLength;
                        if (channel_cursor->noteLength == 0) {
                            Audio_Seq_ExecuteChannelStep(channel_cursor, effect_voice_bit);
                        } else if (channel_cursor->gateLength == 0) {
                            Audio_Mixer_voiceScheduler.pendingKeyOff |= effect_voice_bit;
                            Audio_Mixer_voiceScheduler.keyedVoices &= ~effect_voice_bit;
                        }
                        Audio_Mixer_UpdateChannelSlidesAndDelays(channel_cursor, effect_voice_bit, 1);
                    }
                    i ^= effect_voice_bit;
                }
                ++channel_cursor;
                effect_voice_bit *= 2;
            } while (i != 0);
        }
    }
    if (!(Audio_Seq_tickCounter & 3)) {
        Audio_Seq_UpdateGlobalSlides();
    }
    tick_counter_or_cost = GetRCnt(RCntCNT2) - tick_counter_or_cost;
    if (tick_counter_or_cost <= 0) {
        tick_counter_or_cost += 0x44E8;
    }

    Audio_Driver_recentTickCosts[0] = Audio_Driver_recentTickCosts[1];
    Audio_Driver_recentTickCosts[1] = Audio_Driver_recentTickCosts[2];
    Audio_Driver_recentTickCosts[2] = Audio_Driver_recentTickCosts[3];
    Audio_Driver_recentTickCosts[3] = tick_counter_or_cost;
    Audio_Driver_recentTickCostTotal = Audio_Driver_recentTickCosts[0] + Audio_Driver_recentTickCosts[1] + Audio_Driver_recentTickCosts[2] + Audio_Driver_recentTickCosts[3];
    Audio_Native_Tick();
    return tick_counter_or_cost;
}

// decomp: SLUS_010.40 func_80019FC4 (src/SLUS_010.40/25AC.c:4430)
char Audio_Seq_PeekNextNote(Audio_SeqChannel* channel)
{
    u_char* pc = (u_char *)(channel->programCounter);
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
                pc = (u_char *)(channel->returnAddress);
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
                pc = (u_char *)(channel->loopStarts[loopTop]);
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
                pc = (u_char *)(channel->loopStarts[loopTop]);
                break;
            }
        default:
            channel->playModes &= ~5;
            return 0xA0;
        }
    }
}

// decomp: SLUS_010.40 func_8001A1F4 (src/SLUS_010.40/25AC.c:4550)
int Audio_Seq_ResolveSoundEffectInstrument(int sample_bank_flags, int instrument_index)
{
    if ((instrument_index - 0x80u) < 0x20) {
        if (sample_bank_flags & 2) {
            return instrument_index + 0x20;
        }
        if (sample_bank_flags & 4) {
            return instrument_index + 0x40;
        }
    }
    return instrument_index;
}

// decomp: SLUS_010.40 func_8001A22C (src/SLUS_010.40/25AC.c:4563)
int Audio_Seq_ResolveMusicInstrument(int music_status_flags, u_int instrument_index)
{
    if ((music_status_flags & 0x40) && (instrument_index >= 0x40) && (instrument_index < 0x80)) {
        return instrument_index + 0x20;
    }
    return instrument_index;
}

// decomp: SLUS_010.40 func_8001A258 (src/SLUS_010.40/25AC.c:4571)
void Audio_Seq_SelectNotePatch(Audio_SeqChannel* channel, u_int note_key)
{
    Audio_Instrument* instrument;
    char* patch_entry;
    int instrument_index;
    int update_flags;

    if ((channel->key < note_key) || (channel->key == 0xFF)) {
        patch_entry = channel->keymap;
        while (patch_entry[13] != 0 && patch_entry[2] < note_key) {
            patch_entry += 8;
        }
    } else {
        if (note_key >= channel->key) {
            return;
        }
        patch_entry = channel->keymap;
        while (patch_entry[13] != 0 && note_key >= patch_entry[9]) {
            patch_entry += 8;
        }
    }

    update_flags = channel->updateFlags;

    instrument_index = Audio_Seq_ResolveMusicInstrument(Audio_Seq_activeMusicConfig->statusFlags, patch_entry[0]);
    channel->instrumentIndex = instrument_index;
    instrument = &Audio_Samples_instruments[instrument_index];
    channel->voice.sampleStart = instrument->sampleStart;
    channel->voice.loopStart = instrument->loopStart;

    if (!(update_flags & 0x01000000)) {
        channel->voice.adsrLower = patch_entry[3] << 8;
    } else {
        channel->voice.adsrLower &= 0x7F00;
    }

    channel->voice.adsrLower |= instrument->adsrLower & 0x80FF;

    if (!(update_flags & 0x08000000)) {
        channel->voice.adsrUpper &= 0x201F;
        channel->voice.adsrUpper |= patch_entry[4] << 6;
    } else {
        channel->voice.adsrUpper &= 0x3FDF;
    }

    switch (patch_entry[5]) {
    case 3:
        channel->voice.adsrUpper |= 0x4000;
        break;
    case 5:
        channel->voice.adsrUpper |= 0x8000;
        break;
    case 7:
        channel->voice.adsrUpper |= 0xC000;
        break;
    }

    if ((update_flags & 0x10000000) == 0) {
        channel->voice.adsrUpper &= 0xFFE0;
        channel->voice.adsrUpper |= patch_entry[6];
    }

    channel->voice.adsrUpper |= instrument->adsrUpper & 0x20;
    channel->voice.volumeScale = patch_entry[7];
}

// decomp: SLUS_010.40 func_8001A4BC (src/SLUS_010.40/25AC.c:4637)
int Audio_Seq_CalculateNotePitch(Audio_Instrument* instrument, int note_cursor, u_int fine_tune, int* output_fine_pitch_delta)
{
    int semitone_or_octave_offset;
    int pitch_work;

    semitone_or_octave_offset = note_cursor - instrument->rootNote;

    while (semitone_or_octave_offset < 0) {
        semitone_or_octave_offset += 0xC;
    }

    semitone_or_octave_offset %= 12;

    if (instrument->fineTune == 0) {
        pitch_work = Audio_Seq_kSemitonePitches[semitone_or_octave_offset] << 8;
    } else if (instrument->fineTune < 0) {
        pitch_work = (Audio_Seq_kSemitonePitches[semitone_or_octave_offset] * (u_short)instrument->fineTune) >> 8;
    } else {
        pitch_work = (Audio_Seq_kSemitonePitches[semitone_or_octave_offset] * instrument->fineTune) >> 7;
        pitch_work += Audio_Seq_kSemitonePitches[semitone_or_octave_offset] << 8;
    }

    fine_tune &= 0xFF;

    if (fine_tune != 0) {
        if (fine_tune < 0x80) {
            *output_fine_pitch_delta = (pitch_work * fine_tune) >> 7;
        } else {
            *output_fine_pitch_delta = ((pitch_work * fine_tune) >> 8) - pitch_work;
        }
    }

    if (note_cursor < instrument->rootNote) {
        for (; note_cursor < instrument->rootNote; note_cursor += 0xC) {
            *output_fine_pitch_delta = *output_fine_pitch_delta >> 1;
            pitch_work = (pitch_work >> 1);
        }
    } else {
        semitone_or_octave_offset = (note_cursor - instrument->rootNote) / 12;
        if (semitone_or_octave_offset != 0) {
            pitch_work <<= semitone_or_octave_offset;
            *output_fine_pitch_delta <<= semitone_or_octave_offset;
        }
    }
    pitch_work >>= 8;
    *output_fine_pitch_delta >>= 8;
    return (u_short)pitch_work;
}

// decomp: SLUS_010.40 func_8001A64C (src/SLUS_010.40/25AC.c:4686)
int Audio_Seq_StartDrumNote(Audio_SeqChannel* channel, int channel_bit, int drum_key)
{
    Audio_Instrument* instrument;
    int note_pitch;
    int instrument_index;
    int update_flags;
    char* drum_entry;

    drum_entry = (char*)Audio_Seq_activeMusicConfig->keymapTable;
    drum_entry += drum_key * 8;
    Audio_Seq_activeMusicConfig->pendingKeyOn |= channel_bit;

    if (Audio_Seq_activeMusicConfig->activeNotes & channel_bit) {
        Audio_Seq_activeMusicConfig->pendingKeyOff |= channel_bit;
    }

    update_flags = channel->updateFlags;
    instrument_index = Audio_Seq_ResolveMusicInstrument(Audio_Seq_activeMusicConfig->statusFlags, (u_int)drum_entry[0]);
    channel->instrumentIndex = instrument_index;
    instrument = &Audio_Samples_instruments[instrument_index];
    channel->voice.sampleStart = instrument->sampleStart;
    channel->voice.loopStart = instrument->loopStart;

    if (!(update_flags & 0x01000000)) {
        channel->voice.adsrLower = drum_entry[2] << 8;
    } else {
        channel->voice.adsrLower &= 0x7F00;
    }

    channel->voice.adsrLower |= instrument->adsrLower & 0x80FF;

    if (!(update_flags & 0x08000000)) {
        channel->voice.adsrUpper &= 0x201F;
        channel->voice.adsrUpper |= drum_entry[3] << 6;
    } else {
        channel->voice.adsrUpper &= 0x3FDF;
    }

    switch (drum_entry[4]) {
    case 3:
        channel->voice.adsrUpper |= 0x4000;
        break;
    case 5:
        channel->voice.adsrUpper |= 0x8000;
        break;
    case 7:
        channel->voice.adsrUpper |= 0xC000;
        break;
    }

    if ((update_flags & 0x10000000) == 0) {
        channel->voice.adsrUpper &= 0xFFE0;
        channel->voice.adsrUpper |= drum_entry[5];
    }

    channel->voice.adsrUpper |= instrument->adsrUpper & 0x20;
    note_pitch = Audio_Seq_CalculateNotePitch(instrument, drum_entry[1], channel->fineTune, &channel->finePitchDelta);
    channel->voice.volumeScale = drum_entry[6];
    channel->pan = ((drum_entry[7] & 0x7F) + 0x40) << 8;

    if (drum_entry[7] & 0x80) {
        Audio_Seq_activeMusicConfig->reverbChannels |= channel_bit;
    } else {
        Audio_Seq_activeMusicConfig->reverbChannels &= ~channel_bit;
    }
    Audio_Driver_controlLatches.updateFlags |= 0x100;
    return note_pitch;
}

// decomp: SLUS_010.40 func_8001A8D8 (src/SLUS_010.40/25AC.c:4760)
void Audio_Seq_ExecuteChannelStep(Audio_SeqChannel* channel, int channel_bit)
{
    u_int pitch_delta_work;
    u_int opcode_or_pitch_work;
    u_int note_or_update_flags;
    u_int octave_semitones;
    u_char* program_cursor;
    int gate_ticks;
    u_int note_update_flags;
    u_int explicit_note_opcode_index;
    u_int vibrato_depth_magnitude;
    u_int step_opcode;

    do {
        program_cursor = (u_char *)(channel->programCounter);
        step_opcode = program_cursor[0];
        channel->programCounter = (char *)(++program_cursor);

        if (step_opcode >= 0xA0) {
            explicit_note_opcode_index = step_opcode - 0xF0;
            if (step_opcode == 0xFE) {
                opcode_or_pitch_work = program_cursor[0];
                channel->programCounter = (char *)(program_cursor + 1);
                Audio_Seq_kExtendedOpcodeHandlers[opcode_or_pitch_work](channel, channel_bit);
            } else {
                if (explicit_note_opcode_index < 0xE) {
                    step_opcode = explicit_note_opcode_index * 0xB;
                    channel->noteLength = program_cursor[0];
                    channel->programCounter = (char *)(program_cursor + 1);
                } else {
                    if (step_opcode == 0xFF) {
                        step_opcode = 0xA0;
                    } else if ((step_opcode == 0xCA) && (channel->updateFlags & 0x200000)) {
                        step_opcode = 0xA0;
                        Audio_Mixer_voiceScheduler.pendingKeyOff |= channel_bit;
                    }
                    Audio_Seq_kOpcodeHandlers[step_opcode - 0xA0](channel, channel_bit);
                }
            }
        }
        ++channel->opcodeStepCounter;
    } while (step_opcode > 0xA0);

    if (step_opcode == 0xA0) {
        if (channel->type == 0) {
            Audio_Seq_activeMusicConfig->pendingKeyOff |= channel_bit;
        }
    } else {
        opcode_or_pitch_work = Audio_Seq_PeekNextNote(channel);
        if (channel->fixedNoteLength != 0) {
            channel->noteLength = channel->gateLength = channel->fixedNoteLength;
        }
        if (channel->noteLength != 0) {
            if ((opcode_or_pitch_work >= 0x8F) || ((opcode_or_pitch_work < 0x84) && !(channel->playModes & 5))) {
                channel->gateLength -= 2;
            }
        } else {
            int tmp = Audio_Seq_kNoteLengths[step_opcode % 11];
            channel->noteLength = tmp;
            gate_ticks = tmp;
            if (((opcode_or_pitch_work - 0x84) >= 0xB) && !(channel->playModes & 5)) {
                gate_ticks -= 2;
            }
            channel->gateLength = gate_ticks;
        }
        if ((channel->type == 0) && (channel->updateFlags & 0x40)) {
            channel->gateLength = channel->noteLength;
        }

        channel->storedNoteLength = channel->noteLength;
        channel->voice.dirtyFlags |= 0x4000;

        if (step_opcode >= 0x8F) {
            if (channel->type == 0) {
                Audio_Seq_activeMusicConfig->activeNotes &= ~channel_bit;
                if (channel->voice.assignedVoice < 0x18) {
                    Audio_Seq_activeMusicConfig->pendingKeyOff |= channel_bit;
                }
            }
            channel->portamentoSteps = 0;
            channel->vibratoPitch = 0;
            channel->tremoloVolume = 0;
            channel->playModes &= 0xFFFD;
            return;
        }
        if (step_opcode < 0x84) {
            note_update_flags = channel->updateFlags;
            octave_semitones = channel->octave * 0xC;
            note_or_update_flags = step_opcode / 11;
            note_or_update_flags += octave_semitones;
            if (note_update_flags & 8) {
                opcode_or_pitch_work = Audio_Seq_StartDrumNote(channel, channel_bit, note_or_update_flags);
            } else {
                if (!(channel->playModes & 2)) {
                    if (channel->type == 0) {
                        if (Audio_Driver_mixBehavior & 0x10) {
                            note_or_update_flags += *((signed char*)Audio_Seq_activeMusicConfig->noteRemapTable
                                         + ((note_or_update_flags) % 12));
                        }
                        if (note_update_flags & 0x1000) {
                            Audio_Seq_SelectNotePatch(channel, note_or_update_flags);
                        }
                        Audio_Seq_activeMusicConfig->pendingKeyOn |= channel_bit;
                        if ((Audio_Seq_activeMusicConfig->activeNotes & channel_bit)
                            && (channel->voice.assignedVoice < 0x18)) {
                            Audio_Seq_activeMusicConfig->pendingKeyOff |= channel_bit;
                        }

                        if (channel->keyOnVolumeSlideLength != 0) {
                            channel->volumeSlideLength = channel->keyOnVolumeSlideLength;
                            channel->volume = channel->keyOnVolume;
                            channel->volumeSlideStep = channel->keyOnVolumeSlideStep;
                        }
                    } else {
                        Audio_Mixer_voiceScheduler.pendingKeyOn |= channel_bit;
                    }
                    channel->pitchSlideTicksLeft = 0;
                }

                if ((channel->portamentoSteps != 0) && (channel->portamentoKey != 0)) {
                    channel->pitchBendSlideLength = channel->portamentoSteps;
                    channel->pitchBendTarget =
                        ((channel->transpose + note_or_update_flags) - channel->portamentoKey)
                        - channel->portamentoTranspose;
                    channel->key =
                        channel->portamentoKey - (channel->transpose - channel->portamentoTranspose);
                    note_or_update_flags = channel->portamentoKey + channel->portamentoTranspose;
                } else {
                    channel->key = note_or_update_flags;
                    note_or_update_flags = note_or_update_flags + channel->transpose;
                }

                opcode_or_pitch_work = Audio_Seq_CalculateNotePitch(&Audio_Samples_instruments[channel->instrumentIndex],
                    note_or_update_flags, channel->fineTune, &channel->finePitchDelta);
                if (channel->randomPitchDepth != 0) {
                    pitch_delta_work = (opcode_or_pitch_work * channel->randomPitchDepth) >> 8;
                    pitch_delta_work *= Audio_Seq_kRandomPitchOffsets[Audio_Seq_randomPitchPhase];
                    if (Audio_Seq_kRandomPitchOffsets[Audio_Seq_randomPitchPhase] & 0x80) {
                        pitch_delta_work >>= 9;
                        opcode_or_pitch_work -= pitch_delta_work;
                    } else {
                        pitch_delta_work >>= 7;
                        opcode_or_pitch_work += pitch_delta_work;
                    }
                }
            }
            channel->pitchBase = opcode_or_pitch_work;
            if (channel->type == 0) {
                Audio_Seq_activeMusicConfig->activeNotes |= channel_bit;
            } else {
                Audio_Mixer_voiceScheduler.keyedVoices |= channel_bit;
            }
            note_or_update_flags = channel->updateFlags;
            channel->voice.dirtyFlags |= 0x13;
            if (note_or_update_flags & 1) {
                vibrato_depth_magnitude = ((channel->vibratoDepth & 0x7F00) >> 8);
                channel->vibratoBase = (!(channel->vibratoDepth & 0x8000)
                                            ? (vibrato_depth_magnitude * ((opcode_or_pitch_work * 0xF) >> 8))
                                            : (vibrato_depth_magnitude * opcode_or_pitch_work))
                                 >> 7;
                if (!(channel->playModes & 2)) {
                    short* vibratoProgram = Audio_Seq_LfoWaveform(channel->vibratoShape);
                    if (vibratoProgram == NULL) return;
                    channel->vibratoWave = vibratoProgram;
                    channel->vibratoDelayLeft = channel->vibratoDelay;
                    channel->vibratoRateCounter = 1;
                }
            }
            if ((note_or_update_flags & 2) && !(channel->playModes & 2)) {
                short* tremoloProgram = Audio_Seq_LfoWaveform(channel->tremoloShape);
                if (tremoloProgram == NULL) return;
                channel->tremoloWave = tremoloProgram;
                channel->tremoloDelayLeft = channel->tremoloDelay;
                channel->tremoloRateCounter = 1;
            }
            channel->vibratoPitch = 0;
            channel->tremoloVolume = 0;
            channel->pitchSlide = 0;
        }

        channel->playModes = (channel->playModes & 0xFFFD) | ((channel->playModes & 1) * 2);
        if (channel->pitchBendTarget != 0) {
            channel->key += channel->pitchBendTarget;
            opcode_or_pitch_work = Audio_Seq_CalculateNotePitch(&Audio_Samples_instruments[channel->instrumentIndex],
                channel->key + channel->transpose, channel->fineTune, (int*)&pitch_delta_work);
            opcode_or_pitch_work <<= 0x10;
            channel->pitchSlideTicksLeft = channel->pitchBendSlideLength;
            channel->pitchBendTarget = 0;
            channel->pitchSlideStep =
                ((int)opcode_or_pitch_work - ((channel->pitchBase << 0x10) + channel->pitchSlide))
                / channel->pitchSlideTicksLeft;
        }
        channel->portamentoKey = channel->key;
        channel->portamentoTranspose = channel->transpose;
    }
}

// decomp: SLUS_010.40 Sound_CopyInstrumentInfoToChannel (src/SLUS_010.40/25AC.c:4953)
void Audio_Seq_ApplyInstrumentParameters(
    Audio_SeqChannel* channel, Audio_Instrument* instrument, int sample_start_address)
{
    channel->voice.sampleStart = sample_start_address;
    channel->voice.loopStart = instrument->loopStart;
    channel->voice.adsrLower = instrument->adsrLower;
    channel->voice.adsrUpper = instrument->adsrUpper;
    channel->voice.dirtyFlags |= 0x1FF80;
}

// decomp: SLUS_010.40 func_8001B094 (src/SLUS_010.40/25AC.c:4963)
void Audio_Seq_SelectChannelInstrument(Audio_SeqChannel* channel, int instrument_index)
{
    channel->instrumentIndex = instrument_index;
    Audio_Seq_ApplyInstrumentParameters(
        channel, &Audio_Samples_instruments[instrument_index], Audio_Samples_instruments[instrument_index].sampleStart);
}

// decomp: SLUS_010.40 Sound_ClearVoiceFromSchedulerState (src/SLUS_010.40/25AC.c:4970)
void Audio_Seq_ReleaseSoundEffectVoice(Audio_SeqChannel* channel, int voice_bit)
{
    int keep_voice_mask = ~voice_bit;
    Audio_Mixer_voiceScheduler.activeVoices &= keep_voice_mask;
    Audio_Mixer_voiceScheduler.pausedEffectVoices &= keep_voice_mask;
    Audio_Mixer_voiceScheduler.noiseVoices &= keep_voice_mask;
    Audio_Mixer_voiceScheduler.reverbVoices &= keep_voice_mask;
    Audio_Mixer_voiceScheduler.fmVoices &= keep_voice_mask;
    Audio_Mixer_voiceScheduler.pendingKeyOn &= keep_voice_mask;
    Audio_Mixer_voiceScheduler.keyedVoices &= keep_voice_mask;
    channel->effectTags = 0;
    channel->effectId = 0;
}

}  // extern "C"
