// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"

// decomp: src/SLUS_010.40/25AC.c:173
#define SPU_PAN_TABLE_SIZE (0x100)
// decomp: g_Sound_StereoPanGainTableQ15 (g_Sound_StereoPanGainTableQ15@SLUS_010.40)
short Audio_Mixer_kPanGains[SPU_PAN_TABLE_SIZE]; /* contents: module image (ModuleData.c) */
#undef SPU_PAN_TABLE_SIZE

// decomp: D_8002F89C (D_8002F89C@SLUS_010.40)
short Audio_Mixer_kCenterPanGain; /* contents: module image (ModuleData.c) */

// decomp: D_800301E0 (D_800301E0@SLUS_010.40)
Audio_CommandHandler Audio_Mixer_kCommandHandlers[392]; /* contents: module image (ModuleData.c) */

// decomp: g_SpuVoiceInfo (g_SpuVoiceInfo@SLUS_010.40)
Audio_VoiceEnvelope Audio_Mixer_voiceEnvelopes[VOICE_COUNT];

// decomp: D_80036778 (D_80036778@SLUS_010.40)
int Audio_Mixer_musicPitchScaleStep;

// decomp: D_8003677C (D_8003677C@SLUS_010.40)
int Audio_Mixer_tempoScaleStep;

// decomp: D_80036780 (D_80036780@SLUS_010.40)
short Audio_Mixer_musicPitchScaleSlideLength;

// decomp: D_80036782 (D_80036782@SLUS_010.40)
short Audio_Mixer_tempoScaleSlideLength;

// decomp: D_80037790 (D_80037790@SLUS_010.40)
Audio_CommandParams Audio_Mixer_commandParams;

// decomp: g_Sound_VoiceSchedulerState (g_Sound_VoiceSchedulerState@SLUS_010.40)
Audio_VoiceScheduler Audio_Mixer_voiceScheduler;

// decomp: D_80037894 (D_80037894@SLUS_010.40)
int Audio_Mixer_pendingKeyOnVoices;

// decomp: D_80037898 (D_80037898@SLUS_010.40)
int Audio_Mixer_keyedVoices;

// decomp: D_8003789C (D_8003789C@SLUS_010.40)
int Audio_Mixer_pendingKeyOffVoices;

// decomp: D_800378A0 (D_800378A0@SLUS_010.40)
int Audio_Mixer_pausedSoundEffectVoices;

// decomp: D_800378E4 (D_800378E4@SLUS_010.40)
int Audio_Mixer_mutedMusicChannels;

// decomp: g_Sound_MasterPitchScaleQ16_16 (g_Sound_MasterPitchScaleQ16_16@SLUS_010.40)
int Audio_Mixer_musicPitchScale;

// decomp: g_Sound_TempoMultiplier (g_Sound_TempoMultiplier@SLUS_010.40)
int Audio_Mixer_tempoScale;

// decomp: D_80039AF2 (D_80039AF2@SLUS_010.40)
char Audio_Mixer_tempoScaleLevel;

// decomp: g_Sound_VoiceChannelConfigs (g_Sound_VoiceChannelConfigs@SLUS_010.40)
Audio_MusicConfig* Audio_Mixer_voiceOwners[VOICE_COUNT];

// decomp: g_Sound_VoiceModeFlags (g_Sound_VoiceModeFlags@SLUS_010.40)
Audio_VoiceModeMasks Audio_Mixer_voiceModes;

// decomp: SLUS_010.40 Sound_UpdateSlidesAndDelays (src/SLUS_010.40/25AC.c:1400)
void Audio_Mixer_UpdateChannelSlidesAndDelays(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags, int in_Arg2)
{

    short* Wave;
    int temp;

    if (in_Arg2 == 0) {
        if (in_pChannel->volumeBalanceSlideLength != 0) {
            in_pChannel->volumeBalanceSlideLength--;
            temp = in_pChannel->volumeBalance + in_pChannel->volumeBalanceSlideStep;
            if ((temp & 0x7F00) != (in_pChannel->volumeBalance & 0x7F00)) {
                in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
            }
            in_pChannel->volumeBalance = temp;
        }
    }

    if (in_pChannel->volumeSlideLength != 0) {
        int Volume;
        in_pChannel->volumeSlideLength--;
        Volume = in_pChannel->volume;
        temp = Volume + in_pChannel->volumeSlideStep;
        if ((temp & 0xFFE00000) != (Volume & 0xFFE00000)) {
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
        }
        in_pChannel->volume = temp;
    }

    if (in_pChannel->panSlideLength != 0) {
        u_short Pan;
        in_pChannel->panSlideLength--;
        Pan = in_pChannel->pan;
        temp = Pan + in_pChannel->panSlideStep;
        if ((temp & 0xFF00) != (Pan & 0xFF00)) {
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
        }
        in_pChannel->pan = temp;
    }

    if (in_pChannel->vibratoDelayLeft != 0) {
        in_pChannel->vibratoDelayLeft--;
    }

    if (in_pChannel->tremoloDelayLeft != 0) {
        in_pChannel->tremoloDelayLeft--;
    }

    if (in_pChannel->noiseTimer != 0) {
        in_pChannel->noiseTimer--;
        if (!(in_pChannel->noiseTimer & 0xFFFF)) {
            if (in_Arg2 == 0) {
                Audio_Seq_activeMusicConfig->noiseChannels ^= in_VoiceFlags;
            } else {
                Audio_Mixer_voiceScheduler.noiseVoices ^= in_VoiceFlags;
            }
            Audio_Driver_controlLatches.updateFlags |=
                (SOUND_GLOBAL_UPDATE_04 | SOUND_GLOBAL_UPDATE_08);
        }
    }

    if (in_pChannel->fmTimer != 0) {
        in_pChannel->fmTimer--;
        if (!(in_pChannel->fmTimer & 0xFFFF)) {
            if (in_Arg2 == 0) {
                Audio_Seq_activeMusicConfig->fmChannels ^= in_VoiceFlags;
            } else {
                Audio_Mixer_voiceScheduler.fmVoices ^= in_VoiceFlags;
            }
            Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_08;
        }
    }

    if (in_pChannel->vibratoDepthSlideLength != 0) {
        u_int var_lo;
        u_short VibratoDepth;
        int NewVibratoDepth;

        in_pChannel->vibratoDepthSlideLength--;
        VibratoDepth = in_pChannel->vibratoDepth + in_pChannel->vibratoDepthSlideStep;
        in_pChannel->vibratoDepth = VibratoDepth;
        NewVibratoDepth = (u_int)(VibratoDepth & 0x7F00) >> 8;
        if (VibratoDepth & 0x8000) {
            var_lo = (u_int)(NewVibratoDepth * in_pChannel->pitchBase) >> 7;
        } else {
            var_lo = (NewVibratoDepth * ((u_int)(in_pChannel->pitchBase * 15) >> 8)) >> 7;
        }

        in_pChannel->vibratoBase = var_lo;

        if ((in_pChannel->vibratoDelayLeft == 0) && (in_pChannel->vibratoRateCounter != 1)) {
            Wave = in_pChannel->vibratoWave;
            if (Wave[0] == 0 && Wave[1] == 0) {
                Wave += Wave[2];
            }

            temp = (in_pChannel->vibratoBase * Wave[0]) >> 16;
            if (temp != in_pChannel->vibratoPitch) {
                in_pChannel->vibratoPitch = temp;
                in_pChannel->voice.dirtyFlags |= VOICE_PARAM_SAMPLE_RATE;
                if (temp >= 0) {
                    in_pChannel->vibratoPitch = temp * 2;
                }
            }
        }
    }

    if (in_pChannel->tremoloDepthSlideLength != 0) {

        in_pChannel->tremoloDepthSlideLength--;
        in_pChannel->tremoloDepth += (u_short)in_pChannel->tremoloDepthSlideStep;
        if (((u_short)in_pChannel->tremoloDelayLeft == 0)
            && ((u_short)in_pChannel->tremoloRateCounter != 1)) {
            int FinalVolume;
            int TremeloDepthHi8;
            int VolumeBalanceHigh8;
            int VolumeHigh16;

            Wave = in_pChannel->tremoloWave;
            if (Wave[0] == 0 && Wave[1] == 0) {
                Wave += Wave[2];
            }

            VolumeBalanceHigh8 = (u_short)in_pChannel->volumeBalance >> 8;
            VolumeHigh16 = in_pChannel->volume >> 16;
            FinalVolume = (VolumeHigh16 * VolumeBalanceHigh8) >> 7;
            TremeloDepthHi8 = in_pChannel->tremoloDepth >> 8;
            temp = ((FinalVolume * TremeloDepthHi8) << 9) >> 16;
            temp = (temp * *Wave) >> 15;
            if (temp != in_pChannel->tremoloVolume) {
                in_pChannel->tremoloVolume = temp;
                in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
            }
        }
    }

    if (in_pChannel->autoPanDepthSlideLength != 0) {
        in_pChannel->autoPanDepthSlideLength--;
        in_pChannel->autoPanDepth += in_pChannel->autoPanDepthSlideStep;
        if (in_pChannel->autoPanRateCounter != 1) {
            Wave = in_pChannel->autoPanWave;
            if ((Wave[0] == 0) && (Wave[1] == 0)) {
                Wave += Wave[2];
            }

            temp = ((in_pChannel->autoPanDepth >> 8) * *Wave) >> 15;
            if (temp != in_pChannel->autoPanOffset) {
                in_pChannel->autoPanOffset = temp;
                in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
            }
        }
    }

    if (in_pChannel->pitchSlideTicksLeft != 0) {
        int PitchSlide;

        in_pChannel->pitchSlideTicksLeft--;
        PitchSlide = in_pChannel->pitchSlide;
        temp = PitchSlide + in_pChannel->pitchSlideStep;
        if ((temp & 0xFFFF0000) != (PitchSlide & 0xFFFF0000)) {
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_SAMPLE_RATE;
        }
        in_pChannel->pitchSlide = temp;
    }
}

// decomp: SLUS_010.40 func_8001436C (src/SLUS_010.40/25AC.c:1566)
void Audio_Mixer_UpdateMusicChannelModulation(Audio_SeqChannel* in_pChannel, int in_VoiceFlags __attribute__((unused)))
{
    short* Wave;
    int temp;
    int Volume = ((in_pChannel->volume >> 16) * (in_pChannel->volumeBalance >> 8)) >> 7;
    int Sample;
    u_int UpdateFlags = in_pChannel->updateFlags;

    if ((UpdateFlags & SOUND_UPDATE_VIBRATO) && (in_pChannel->vibratoDelayLeft == 0)
        && (--in_pChannel->vibratoRateCounter == 0)) {
        in_pChannel->vibratoRateCounter = in_pChannel->vibratoRate;
        Wave = in_pChannel->vibratoWave;
        if (Wave[0] == 0 && Wave[1] == 0) {
            in_pChannel->vibratoWave = Wave + Wave[2];
        }
        Sample = *in_pChannel->vibratoWave++;
        temp = (in_pChannel->vibratoBase * Sample) >> 16;
        if (temp != in_pChannel->vibratoPitch) {
            in_pChannel->vibratoPitch = temp;
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_SAMPLE_RATE;
            if (temp >= 0) {
                in_pChannel->vibratoPitch = temp * 2;
            }
        }
    }

    if ((UpdateFlags & SOUND_UPDATE_TREMOLO) && (in_pChannel->tremoloDelayLeft == 0)
        && !(--in_pChannel->tremoloRateCounter & 0xFFFF)) {
        in_pChannel->tremoloRateCounter = in_pChannel->tremoloRate;
        Wave = in_pChannel->tremoloWave;
        if (Wave[0] == 0 && Wave[1] == 0) {
            in_pChannel->tremoloWave = Wave + Wave[2];
        }
        temp = ((Volume * (in_pChannel->tremoloDepth >> 8)) << 9) >> 16;
        Sample = *in_pChannel->tremoloWave++;
        temp = (temp * Sample) >> 15;
        if (temp != in_pChannel->tremoloVolume) {
            in_pChannel->tremoloVolume = temp;
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
        }
    }

    if ((UpdateFlags & SOUND_UPDATE_PAN_LFO)
        && (--in_pChannel->autoPanRateCounter == 0)) {
        in_pChannel->autoPanRateCounter = in_pChannel->autoPanRate;
        Wave = in_pChannel->autoPanWave;
        if (Wave[0] == 0 && Wave[1] == 0) {
            in_pChannel->autoPanWave = Wave + Wave[2];
        }
        Sample = *in_pChannel->autoPanWave++;
        temp = ((in_pChannel->autoPanDepth >> 8) * Sample) >> 15;
        if (temp != in_pChannel->autoPanOffset) {
            in_pChannel->autoPanOffset = temp;
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
        }
    }

    if (UpdateFlags & SOUND_UPDATE_SIDE_CHAIN_VOL) {
        Volume = ((short)((in_pChannel - 1)->voice.sampleRate * 2)
                     * (in_pChannel->volumeBalance >> 8))
              >> 7;
        in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
    }

    if (in_pChannel->voice.dirtyFlags & VOICE_PARAM_VOLUME) {
        Volume += in_pChannel->tremoloVolume;
        Volume =
            (Volume * (int)(((u_int)Audio_Seq_activeMusicConfig->masterVolume >> 16) & 0x7F)) >> 7;
        temp = ((in_pChannel->pan >> 8) + in_pChannel->autoPanOffset) & 0xFF;
        if (Audio_Driver_mixBehavior & 2) {
            in_pChannel->voice.volume.right = (Volume * Audio_Mixer_kCenterPanGain) >> 15;
            in_pChannel->voice.volume.left = in_pChannel->voice.volume.right;
        } else {
            in_pChannel->voice.volume.left =
                (Volume * Audio_Mixer_kPanGains[temp]) >> 15;
            in_pChannel->voice.volume.right =
                (Volume * Audio_Mixer_kPanGains[temp ^ 0xFF]) >> 15;
        }
    }

    if (UpdateFlags & SOUND_UPDATE_SIDE_CHAIN_PITCH) {
        Sample = (in_pChannel - 1)->voice.sampleRate + in_pChannel->vibratoPitch
               + (in_pChannel->pitchSlide >> 16);
        temp = Audio_Mixer_musicPitchScale & 0xFF0000;
        if (temp != 0) {
            temp >>= 16;
            if (temp < 0x80) {
                Sample += (Sample * temp) >> 7;
            } else {
                Sample = (Sample * temp) >> 8;
            }
        }
        in_pChannel->voice.sampleRate =
            (in_pChannel->finePitchDelta + Sample) & 0x3FFF;
        in_pChannel->voice.dirtyFlags |= VOICE_PARAM_SAMPLE_RATE;
    } else if (in_pChannel->voice.dirtyFlags & VOICE_PARAM_SAMPLE_RATE) {
        Sample = in_pChannel->pitchBase + in_pChannel->vibratoPitch
               + (in_pChannel->pitchSlide >> 16);
        temp = Audio_Mixer_musicPitchScale & 0xFF0000;
        if (temp != 0) {
            temp >>= 16;
            if (temp < 0x80) {
                Sample += (Sample * temp) >> 7;
            } else {
                Sample = (Sample * temp) >> 8;
            }
        }
        in_pChannel->voice.sampleRate =
            (in_pChannel->finePitchDelta + Sample) & 0x3FFF;
    }
}

// decomp: SLUS_010.40 func_800147CC (src/SLUS_010.40/25AC.c:1678)
void Audio_Mixer_UpdateSoundEffectModulation(Audio_SeqChannel* in_pChannel, int in_VoiceFlags __attribute__((unused)))
{
    short* Wave;
    int temp;
    int Volume = ((in_pChannel->volume >> 16) * (in_pChannel->volumeBalance >> 8)) >> 7;
    int Sample;
    u_int UpdateFlags = in_pChannel->updateFlags;

    if ((UpdateFlags & SOUND_UPDATE_VIBRATO) && (--in_pChannel->vibratoRateCounter == 0)) {
        in_pChannel->vibratoRateCounter = in_pChannel->vibratoRate;
        Wave = in_pChannel->vibratoWave;
        if (Wave[0] == 0 && Wave[1] == 0) {
            in_pChannel->vibratoWave = Wave + Wave[2];
        }
        Sample = *in_pChannel->vibratoWave++;
        temp = (in_pChannel->vibratoBase * Sample) >> 16;
        if (temp != in_pChannel->vibratoPitch) {
            in_pChannel->vibratoPitch = temp;
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_SAMPLE_RATE;
            if (temp >= 0) {
                in_pChannel->vibratoPitch = temp * 2;
            }
        }
    }

    if ((UpdateFlags & SOUND_UPDATE_TREMOLO) && !(--in_pChannel->tremoloRateCounter & 0xFFFF)) {
        in_pChannel->tremoloRateCounter = in_pChannel->tremoloRate;
        Wave = in_pChannel->tremoloWave;
        if (Wave[0] == 0 && Wave[1] == 0) {
            in_pChannel->tremoloWave = Wave + Wave[2];
        }
        temp = ((Volume * (in_pChannel->tremoloDepth >> 8)) << 9) >> 16;
        Sample = *in_pChannel->tremoloWave++;
        temp = (temp * Sample) >> 15;
        if (temp != in_pChannel->tremoloVolume) {
            in_pChannel->tremoloVolume = temp;
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
        }
    }

    if ((UpdateFlags & SOUND_UPDATE_PAN_LFO)
        && (--in_pChannel->autoPanRateCounter == 0)) {
        in_pChannel->autoPanRateCounter = in_pChannel->autoPanRate;
        Wave = in_pChannel->autoPanWave;
        if (Wave[0] == 0 && Wave[1] == 0) {
            in_pChannel->autoPanWave = Wave + Wave[2];
        }
        Sample = *in_pChannel->autoPanWave++;
        temp = ((in_pChannel->autoPanDepth >> 8) * Sample) >> 15;
        if (temp != in_pChannel->autoPanOffset) {
            in_pChannel->autoPanOffset = temp;
            in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
        }
    }

    if (UpdateFlags & SOUND_UPDATE_SIDE_CHAIN_VOL) {
        Volume = ((short)((in_pChannel - 1)->voice.sampleRate * 2)
                     * (in_pChannel->volumeBalance >> 8))
              >> 7;
        in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
    }

    if (in_pChannel->voice.dirtyFlags & VOICE_PARAM_VOLUME) {
        Volume += in_pChannel->tremoloVolume;
        if (!(in_pChannel->effectTags & 0x2000000)) {
            Volume = (Volume * (in_pChannel->effectVolume >> 8)) >> 7;
            temp = ((in_pChannel->pan + in_pChannel->effectPan) >> 8)
                 + in_pChannel->autoPanOffset + 0x80;
            temp &= 0xFF;
        } else {
            temp = 0x80;
        }
        if (Audio_Driver_mixBehavior & 2) {
            in_pChannel->voice.volume.right = (Volume * Audio_Mixer_kCenterPanGain) >> 15;
            in_pChannel->voice.volume.left = in_pChannel->voice.volume.right;
        } else {
            in_pChannel->voice.volume.left =
                (Volume * Audio_Mixer_kPanGains[temp]) >> 15;
            in_pChannel->voice.volume.right =
                (Volume * Audio_Mixer_kPanGains[temp ^ 0xFF]) >> 15;
        }
    }

    if (UpdateFlags & SOUND_UPDATE_SIDE_CHAIN_PITCH) {
        Sample = (in_pChannel - 1)->voice.sampleRate + in_pChannel->vibratoPitch
               + (in_pChannel->pitchSlide >> 16);
        if (!(in_pChannel->effectTags & 0x2000000)) {
            temp = in_pChannel->effectPitchScale & 0xFF00;
            if (temp != 0) {
                temp >>= 8;
                if (temp < 0x80) {
                    Sample += (Sample * temp) >> 7;
                } else {
                    Sample = (Sample * temp) >> 8;
                }
            }
        }
        in_pChannel->voice.sampleRate =
            (in_pChannel->finePitchDelta + Sample) & 0x3FFF;
        in_pChannel->voice.dirtyFlags |= VOICE_PARAM_SAMPLE_RATE;
    } else if (in_pChannel->voice.dirtyFlags & VOICE_PARAM_SAMPLE_RATE) {
        Sample = in_pChannel->pitchBase + in_pChannel->vibratoPitch
               + (in_pChannel->pitchSlide >> 16);
        if (!(in_pChannel->effectTags & 0x2000000)) {
            temp = in_pChannel->effectPitchScale & 0xFF00;
            if (temp != 0) {
                temp >>= 8;
                if (temp < 0x80) {
                    Sample += (Sample * temp) >> 7;
                } else {
                    Sample = (Sample * temp) >> 8;
                }
            }
        }
        in_pChannel->voice.sampleRate =
            (in_pChannel->finePitchDelta + Sample) & 0x3FFF;
    }
}

// decomp: SLUS_010.40 Sound_StealQuietestVoice (src/SLUS_010.40/25AC.c:1799)
int Audio_Mixer_StealQuietestVoice(int in_bForceFullScan)
{
    Audio_VoiceEnvelope* pVoiceInfo;
    short EnvX;
    int i;
    int out_VoiceIndex;

    if (in_bForceFullScan) {
        i = 0;
    } else {
        i = Audio_Seq_activeMusicConfig->voiceSearchStart;
    }

    EnvX = 0x7FFF;
    out_VoiceIndex = VOICE_COUNT;
    pVoiceInfo = &Audio_Mixer_voiceEnvelopes[i];

    do {
        if (pVoiceInfo->envelopeLevel < EnvX) {
            EnvX = pVoiceInfo->envelopeLevel;
            out_VoiceIndex = i;
        }
        i++;
        pVoiceInfo++;
    } while (i < VOICE_COUNT);

    if (EnvX == 0x7FFF) {
        return VOICE_COUNT;
    }
    Audio_Mixer_UnassignVoice(Audio_Seq_musicChannels, out_VoiceIndex);
    return out_VoiceIndex;
}

// decomp: SLUS_010.40 Sound_FindFreeVoice (src/SLUS_010.40/25AC.c:1832)
int Audio_Mixer_FindFreeVoice(int arg0)
{
    Audio_VoiceEnvelope* var_v1;

    if (arg0 != 0) {
        arg0 = 0;
    } else {
        arg0 = Audio_Seq_activeMusicConfig->voiceSearchStart;
    }

    var_v1 = &Audio_Mixer_voiceEnvelopes[arg0];

    if (var_v1->envelopeLevel != 0) {
        ++arg0;
        while (1) {
            if (arg0 >= 0x18) {
                break;
            }
            ++var_v1;
            ++arg0;
            if (var_v1->envelopeLevel == 0) {
                --arg0;
                break;
            }
        }
    }
    return arg0;
}

// decomp: SLUS_010.40 func_80014D70 (src/SLUS_010.40/25AC.c:1863)
void Audio_Mixer_ApplyMusicChannelVoices(
    Audio_SeqChannel* in_pChannel, u_int in_Flags1, u_int in_Flags2, u_int* out_KeyOnFlags)
{
    int var_s2 = 1;
    int var_s5 = 0;
    int sp10 = in_Flags1 & Audio_Seq_activeMusicConfig->pendingKeyOn;

    do {
        if (in_Flags1 & var_s2) {
            Audio_Mixer_UpdateMusicChannelModulation(in_pChannel, var_s2);
            if (in_pChannel->voice.dirtyFlags != 0) {
                if (Audio_Mixer_mutedMusicChannels & var_s2) {
                    in_pChannel->voice.volume.right = 0;
                    in_pChannel->voice.volume.left = 0;
                }
                if (sp10 & var_s2) {
                    if (in_Flags2 & var_s2) {
                        *out_KeyOnFlags |= 1 << var_s5;
                        in_pChannel->voice.assignedVoice = var_s5;
                    } else {
                        int temp_s0 = Audio_Seq_activeMusicConfig->keyedVoices & var_s2;
                        int var_a0 = Audio_Mixer_FindFreeVoice(temp_s0);
                        if (var_a0 == 0x18) {
                            Audio_Seq_activeMusicConfig->statusFlags |= 2;
                            var_a0 = Audio_Mixer_StealQuietestVoice(temp_s0);
                            if (var_a0 == 0x18) {
                                in_pChannel->voice.assignedVoice = var_a0;
                                Audio_Seq_activeMusicConfig->statusFlags |= 1;
                            } else {
                                *out_KeyOnFlags |= 1 << var_a0;
                                in_pChannel->voice.assignedVoice = var_a0;
                                Audio_Mixer_voiceEnvelopes[var_a0].envelopeLevel = 0x7FFF;
                            }
                        } else {
                            *out_KeyOnFlags |= 1 << var_a0;
                            in_pChannel->voice.assignedVoice = var_a0;
                            Audio_Mixer_voiceEnvelopes[var_a0].envelopeLevel = 0x7FFF;
                        }
                    }
                    if (in_pChannel->voice.assignedVoice < 0x18) {
                        Audio_Driver_WriteVoiceParameters(in_pChannel->voice.assignedVoice,
                            &in_pChannel->voice,
                            in_pChannel->voice.volumeScale);
                        Audio_Mixer_voiceOwners[in_pChannel->voice
                                .assignedVoice] = Audio_Seq_activeMusicConfig;
                        Audio_Driver_controlLatches.updateFlags |= 0x100;
                    }
                } else {
                    if (in_pChannel->voice.assignedVoice < 0x18) {
                        Audio_Driver_WriteDirtyVoiceParameters(
                            in_pChannel->voice.assignedVoice,
                            &in_pChannel->voice, in_pChannel->updateFlags);
                    }
                }
            }
            in_Flags1 &= ~var_s2;
        }
        var_s2 *= 2;
        ++in_pChannel;
        ++var_s5;
    } while (in_Flags1 != 0);
}

// decomp: SLUS_010.40 UnassignVoicesFromChannels (src/SLUS_010.40/25AC.c:1926)
void Audio_Mixer_UnassignVoice(Audio_SeqChannel* in_pChannel, int arg1)
{
    Audio_SeqChannel* pChannel;
    u_int Count;

    Count = 0;
    pChannel = in_pChannel;

    do {
        if (arg1 == pChannel->voice.assignedVoice) {
            pChannel->voice.assignedVoice = VOICE_COUNT;
        }
        Count++;
        pChannel++;
    } while (Count < SOUND_CHANNEL_COUNT);

    Count = 0;

    if (Audio_Seq_secondaryMusicConfig != NULL) {
        pChannel = Audio_Seq_secondaryMusicChannels;
        do {
            if (arg1 == pChannel->voice.assignedVoice) {
                pChannel->voice.assignedVoice = VOICE_COUNT;
            }
            Count++;
            pChannel++;
        } while (Count < SOUND_CHANNEL_COUNT);
    }
}

// decomp: SLUS_010.40 func_8001503C (src/SLUS_010.40/25AC.c:1956)
int Audio_Mixer_IsVoiceAssignedToChannels(Audio_SeqChannel* channels, int mask, int voiceNumber)
{
    u_int i;
    for (i = 0; i < 32; i++) {
        int v = 1;
        if (voiceNumber == channels[i].voice.assignedVoice) {
            if (mask & (v << i)) {
                return 1;
            }
        }
    }
    return 0;
}

// decomp: SLUS_010.40 func_80015080 (src/SLUS_010.40/25AC.c:1971)
void Audio_Mixer_RefreshVoiceAvailability(u_int in_ReservedVoices)
{
    u_int i;
    u_int reserved = (Audio_Seq_activeMusicConfig->activeChannels
                         & Audio_Seq_activeMusicConfig->allocatedVoices)
                   | in_ReservedVoices;
    Audio_VoiceEnvelope* pVoice;

    if (Audio_Seq_secondaryMusicConfig != NULL) {
        reserved |= Audio_Seq_secondaryMusicConfig->activeChannels
                  & Audio_Seq_secondaryMusicConfig->allocatedVoices;
    }

    for (i = 0, pVoice = Audio_Mixer_voiceEnvelopes; i < VOICE_COUNT; pVoice++, i++) {
        if (reserved & (1 << i)) {
            pVoice->envelopeLevel = 0x7FFF;
            continue;
        }
        SpuGetVoiceEnvelope(i, &pVoice->envelopeLevel);
        if (pVoice->envelopeLevel == 0) {
            Audio_Mixer_UnassignVoice(Audio_Seq_musicChannels, i);
        } else if (Audio_Seq_activeMusicConfig->keyedVoices != 0) {
            if (Audio_Mixer_IsVoiceAssignedToChannels(
                    Audio_Seq_musicChannels, Audio_Seq_activeMusicConfig->keyedVoices, i)) {
                pVoice->envelopeLevel = 0x7FFF;
            }
            if (Audio_Seq_secondaryMusicConfig != NULL
                && Audio_Mixer_IsVoiceAssignedToChannels(
                    Audio_Seq_secondaryMusicChannels, Audio_Seq_secondaryMusicConfig->keyedVoices, i)) {
                pVoice->envelopeLevel = 0x7FFF;
            }
        }
    }
}

// decomp: SLUS_010.40 Sound_ApplyMasterFadeToChannelVolume (src/SLUS_010.40/25AC.c:2006)
void Audio_Mixer_ApplySavedMusicMasterFade(
    Audio_MusicConfig* in_Context, Audio_SeqChannel* arg1 __attribute__((unused)))
{
    int Volume;

    Volume = in_Context->masterVolume;
    Audio_Seq_masterFade.savedValue = in_Context->masterVolume;
    Volume >>= 16;
    Volume *= Audio_Seq_masterFade.value >> 8;
    Volume /= 127;
    Volume <<= 8;
    in_Context->masterVolume = Volume;
}

// decomp: SLUS_010.40 Sound_RestoreChannelVolumeFromMasterFade (src/SLUS_010.40/25AC.c:2020)
void Audio_Mixer_RestoreSavedMusicVolume(
    Audio_MusicConfig* in_Config, Audio_SeqChannel* arg1 __attribute__((unused)))
{
    in_Config->masterVolume = Audio_Seq_masterFade.savedValue;
}

// decomp: SLUS_010.40 func_80015220 (src/SLUS_010.40/25AC.c:2027)
void Audio_Mixer_CommitPlaybackVoices(void)
{
    u_int KeyOnFlags = 0;
    u_int ReservedVoices = Audio_Mixer_voiceScheduler.activeVoices
                         | Audio_Mixer_voiceScheduler.pausedEffectVoices | Audio_Cutscene_streamVoices;
    u_int Voices;
    u_int Flags;
    u_int SavedChannels;
    u_int SavedVoices;
    Audio_SeqChannel* pChannel;

    if ((Audio_Seq_activeMusicConfig->activeChannels & Audio_Seq_activeMusicConfig->pendingKeyOn)
        || ((Audio_Seq_secondaryMusicConfig != NULL)
            && (Audio_Seq_secondaryMusicConfig->activeChannels
                & Audio_Seq_secondaryMusicConfig->pendingKeyOn))) {
        Audio_Mixer_RefreshVoiceAvailability(ReservedVoices);
    }

    if (Audio_Seq_secondaryMusicConfig != NULL) {
        if (Audio_Driver_controlLatches.mixBehavior & 0x100) {
            Audio_Mixer_ApplySavedMusicMasterFade(
                Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels);
        }

        Audio_Seq_activeMusicConfig = Audio_Seq_secondaryMusicConfig;
        SavedChannels = (Audio_Seq_activeMusicConfig->activeChannels
                            & Audio_Seq_activeMusicConfig->activeNotes)
                      & ~(Audio_Seq_activeMusicConfig->allocatedVoices & ReservedVoices);
        SavedVoices =
            (SavedChannels & Audio_Seq_activeMusicConfig->allocatedVoices) & ~ReservedVoices;
        if (SavedChannels & Audio_Seq_activeMusicConfig->keyedVoices) {
            Audio_Mixer_ApplyMusicChannelVoices(Audio_Seq_secondaryMusicChannels,
                SavedChannels & Audio_Seq_activeMusicConfig->keyedVoices, SavedVoices,
                &KeyOnFlags);
            Audio_Seq_activeMusicConfig->pendingKeyOn &= ~Audio_Seq_activeMusicConfig->keyedVoices;
            SavedChannels &= ~Audio_Seq_activeMusicConfig->keyedVoices;
        }

        Audio_Seq_activeMusicConfig = &Audio_Seq_primaryMusicConfig;
        Flags = (Audio_Seq_activeMusicConfig->activeChannels
                    & Audio_Seq_activeMusicConfig->activeNotes)
              & ~(Audio_Seq_activeMusicConfig->allocatedVoices
                  & (SavedVoices | ReservedVoices));
        Voices = SavedVoices | ReservedVoices;
        Voices = (Flags & Audio_Seq_activeMusicConfig->allocatedVoices) & ~Voices;
        if (Flags & Audio_Seq_activeMusicConfig->keyedVoices) {
            Audio_Mixer_ApplyMusicChannelVoices(Audio_Seq_musicChannels, Flags & Audio_Seq_activeMusicConfig->keyedVoices,
                Voices, &KeyOnFlags);
            Audio_Seq_activeMusicConfig->pendingKeyOn &= ~Audio_Seq_activeMusicConfig->keyedVoices;
            Flags &= ~Audio_Seq_activeMusicConfig->keyedVoices;
        }

        if (SavedChannels != 0) {
            Audio_Seq_activeMusicConfig = Audio_Seq_secondaryMusicConfig;
            SavedVoices &= ~Voices;
            Audio_Mixer_ApplyMusicChannelVoices(
                Audio_Seq_secondaryMusicChannels, SavedChannels, SavedVoices, &KeyOnFlags);
            Audio_Seq_activeMusicConfig->pendingKeyOn = 0;
            Audio_Seq_activeMusicConfig = &Audio_Seq_primaryMusicConfig;
        }

        if (Flags != 0) {
            Audio_Mixer_ApplyMusicChannelVoices(Audio_Seq_musicChannels, Flags, Voices, &KeyOnFlags);
            Audio_Seq_activeMusicConfig->pendingKeyOn = 0;
        }

        if (Audio_Driver_controlLatches.mixBehavior & 0x100) {
            Audio_Mixer_RestoreSavedMusicVolume(
                Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels);
        }
    } else {
        Flags = (Audio_Seq_activeMusicConfig->activeChannels
                    & Audio_Seq_activeMusicConfig->activeNotes)
              & ~(Audio_Seq_activeMusicConfig->allocatedVoices & ReservedVoices);
        Voices = (Flags & Audio_Seq_activeMusicConfig->allocatedVoices) & ~ReservedVoices;
        if (Flags & Audio_Seq_activeMusicConfig->keyedVoices) {
            Audio_Mixer_ApplyMusicChannelVoices(Audio_Seq_musicChannels, Flags & Audio_Seq_activeMusicConfig->keyedVoices,
                Voices, &KeyOnFlags);
            Audio_Seq_activeMusicConfig->pendingKeyOn &= ~Audio_Seq_activeMusicConfig->keyedVoices;
            Flags &= ~Audio_Seq_activeMusicConfig->keyedVoices;
        }
        if (Flags != 0) {
            Audio_Mixer_ApplyMusicChannelVoices(Audio_Seq_musicChannels, Flags, Voices, &KeyOnFlags);
            Audio_Seq_activeMusicConfig->pendingKeyOn = 0;
        }
    }

    Flags = Audio_Mixer_voiceScheduler.activeVoices
          & Audio_Mixer_voiceScheduler.keyedVoices;
    if (Flags != 0) {
        KeyOnFlags |= Audio_Mixer_voiceScheduler.pendingKeyOn;
        for (Voices = 0x1000, pChannel = Audio_SoundEffects_channels; Flags != 0;
            Voices <<= 1, pChannel++) {
            if (Flags & Voices) {
                Audio_Mixer_UpdateSoundEffectModulation(pChannel, Voices);
                if (pChannel->voice.dirtyFlags != 0) {
                    Audio_Driver_WriteDirtyVoiceParameters(pChannel->voice.assignedVoice,
                        &pChannel->voice, pChannel->updateFlags);
                }
                Flags &= ~Voices;
            }
        }
        Audio_Mixer_pendingKeyOnVoices = 0;
    }

    Flags = Audio_Driver_controlLatches.updateFlags;
    if (Flags & 0x80) {
        SpuSetReverbModeDepth(
            Audio_Seq_activeMusicConfig->reverbDepth >> 12, Audio_Seq_activeMusicConfig->reverbDepth >> 12);
        Audio_Driver_controlLatches.updateFlags &= ~0x80;
    }
    if (Flags & 0x10) {
        if (Audio_Mixer_voiceScheduler.activeVoices != 0) {
            SpuSetNoiseClock(Audio_Mixer_voiceScheduler.noiseClock);
        } else {
            SpuSetNoiseClock(Audio_Seq_activeMusicConfig->noiseClock);
        }
        Audio_Driver_controlLatches.updateFlags &= ~0x10;
    }
    if (Flags & 0x100) {
        Audio_Mixer_BuildVoiceModeMask(&Audio_Mixer_voiceModes.noise,
            Audio_Seq_secondaryMusicConfig->noiseChannels,
            Audio_Seq_activeMusicConfig->noiseChannels,
            Audio_Mixer_voiceScheduler.noiseVoices);
        Audio_Mixer_BuildVoiceModeMask(&Audio_Mixer_voiceModes.reverb,
            Audio_Seq_secondaryMusicConfig->reverbChannels,
            Audio_Seq_activeMusicConfig->reverbChannels,
            Audio_Mixer_voiceScheduler.reverbVoices);
        Audio_Mixer_BuildVoiceModeMask(&Audio_Mixer_voiceModes.fm, Audio_Seq_secondaryMusicConfig->fmChannels,
            Audio_Seq_activeMusicConfig->fmChannels,
            Audio_Mixer_voiceScheduler.fmVoices);
        Audio_Driver_SetVoiceReverbMask(Audio_Mixer_voiceModes.reverb);
        Audio_Driver_SetVoiceNoiseMask(Audio_Mixer_voiceModes.noise);
        Audio_Driver_SetVoiceFrequencyModulationMask(Audio_Mixer_voiceModes.fm);
        Audio_Driver_controlLatches.updateFlags &= ~0x100;
    }
    if (KeyOnFlags != 0) {
        Audio_Driver_KeyOnVoices(KeyOnFlags);
    }
}

// decomp: SLUS_010.40 ChannelMaskToVoiceMaskFiltered (src/SLUS_010.40/25AC.c:2168)
void Audio_Mixer_AccumulateFilteredVoiceMask(Audio_SeqChannel* in_Channel, int* io_VoiceMask,
    int in_ChannelMask, int in_VoiceMaskFilter)
{
    u_int bit = 1;

    do {
        if (in_ChannelMask & bit) {
            if (in_Channel->voice.assignedVoice < VOICE_COUNT) {
                *io_VoiceMask |= 1 << in_Channel->voice.assignedVoice;
            }
        }
        in_ChannelMask &= ~bit;
        in_Channel++;
        bit <<= 1;
    } while (in_ChannelMask != 0);

    *io_VoiceMask &= in_VoiceMaskFilter;
}

// decomp: SLUS_010.40 Sound_ProcessKeyOffRequests (src/SLUS_010.40/25AC.c:2187)
void Audio_Mixer_ReleasePendingVoices(void)
{
    int VoiceMask;
    int SavedConfigKeyedMask;
    int ActiveKeyedMask;
    int Filter;
    int ActiveOffMask;
    int SavedOffMask;

    Filter = ~(Audio_Mixer_voiceScheduler.activeVoices
               | Audio_Mixer_voiceScheduler.pausedEffectVoices
               | Audio_Cutscene_streamState.voiceMask);
    VoiceMask = 0;
    SavedOffMask = 0;

    if (Audio_Seq_secondaryMusicConfig) {
        SavedOffMask = Audio_Seq_secondaryMusicConfig->pendingKeyOff;
        SavedConfigKeyedMask = SavedOffMask & Audio_Seq_secondaryMusicConfig->keyedVoices;

        if (SavedConfigKeyedMask != 0) {
            Audio_Mixer_AccumulateFilteredVoiceMask(
                Audio_Seq_secondaryMusicChannels, &VoiceMask, SavedConfigKeyedMask, Filter);
            Audio_Seq_secondaryMusicConfig->pendingKeyOff &= ~Audio_Seq_secondaryMusicConfig->keyedVoices;
            SavedOffMask &= ~Audio_Seq_secondaryMusicConfig->keyedVoices;
        }
    }

    ActiveOffMask = Audio_Seq_activeMusicConfig->pendingKeyOff;
    ActiveKeyedMask = ActiveOffMask & Audio_Seq_activeMusicConfig->keyedVoices;

    if (ActiveKeyedMask != 0) {
        Audio_Mixer_AccumulateFilteredVoiceMask(
            Audio_Seq_musicChannels, &VoiceMask, ActiveKeyedMask, Filter);
        Audio_Seq_activeMusicConfig->pendingKeyOff &= ~Audio_Seq_activeMusicConfig->keyedVoices;
        ActiveOffMask &= ~Audio_Seq_activeMusicConfig->keyedVoices;
    }

    if (Audio_Seq_secondaryMusicConfig && (SavedOffMask != 0)) {
        Audio_Mixer_AccumulateFilteredVoiceMask(
            Audio_Seq_secondaryMusicChannels, &VoiceMask, SavedOffMask, Filter);
        Audio_Seq_secondaryMusicConfig->pendingKeyOff = 0;
    }

    if (ActiveOffMask != 0) {
        Audio_Mixer_AccumulateFilteredVoiceMask(
            Audio_Seq_musicChannels, &VoiceMask, ActiveOffMask, Filter);
        Audio_Seq_activeMusicConfig->pendingKeyOff = 0;
    }

    VoiceMask |= Audio_Mixer_voiceScheduler.pendingKeyOff;
    Audio_Mixer_voiceScheduler.pendingKeyOff = 0;

    if (VoiceMask != 0) {
        Audio_Driver_KeyOffVoices(VoiceMask);
    }
}

// decomp: SLUS_010.40 func_80015970 (src/SLUS_010.40/25AC.c:2244)
void Audio_Mixer_BuildVoiceModeMask(int* out_VoiceMask, int in_SavedChannelMask, int in_ActiveChannelMask,
    int in_ExtraVoiceMask)
{
    int VoiceMask = 0;
    int SavedKeyedMask;
    int ActiveKeyedMask;
    int Filter = ~(Audio_Mixer_voiceScheduler.activeVoices
                   | Audio_Mixer_voiceScheduler.pausedEffectVoices | Audio_Cutscene_streamVoices);
    int ActiveMask;
    int SavedMask = 0;

    if (Audio_Seq_secondaryMusicConfig != NULL) {
        SavedMask = Audio_Seq_secondaryMusicConfig->activeChannels & in_SavedChannelMask;
        SavedKeyedMask = SavedMask & Audio_Seq_secondaryMusicConfig->keyedVoices;

        if (SavedKeyedMask != 0) {
            Audio_Mixer_AccumulateFilteredVoiceMask(
                Audio_Seq_secondaryMusicChannels, &VoiceMask, SavedKeyedMask, Filter);
            SavedMask &= ~Audio_Seq_secondaryMusicConfig->keyedVoices;
        }
    }

    ActiveMask = Audio_Seq_activeMusicConfig->activeChannels & in_ActiveChannelMask;
    ActiveKeyedMask = ActiveMask & Audio_Seq_activeMusicConfig->keyedVoices;

    if (ActiveKeyedMask != 0) {
        Audio_Mixer_AccumulateFilteredVoiceMask(
            Audio_Seq_musicChannels, &VoiceMask, ActiveKeyedMask, Filter);
        ActiveMask &= ~Audio_Seq_activeMusicConfig->keyedVoices;
    }

    if ((Audio_Seq_secondaryMusicConfig != NULL) && (SavedMask != 0)) {
        Audio_Mixer_AccumulateFilteredVoiceMask(
            Audio_Seq_secondaryMusicChannels, &VoiceMask, SavedMask, Filter);
    }

    if (ActiveMask != 0) {
        Audio_Mixer_AccumulateFilteredVoiceMask(
            Audio_Seq_musicChannels, &VoiceMask, ActiveMask, Filter);
    }

    VoiceMask |= in_ExtraVoiceMask;
    *out_VoiceMask = VoiceMask;
    Audio_Driver_controlLatches.updateFlags |= 0x100;
}

