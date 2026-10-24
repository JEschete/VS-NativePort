// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/LfoWaveforms.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Modulation.h"

// decomp: SLUS_010.40 SoundVM_B4_Vibrato (src/SLUS_010.40/25AC.c:5462)
void Audio_Seq_EnableVibrato(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short PitchBase;
    int DepthHigh;
    u_int VibratoBase;
    int VibratoDepth;
    int VibratoRatePhase;
    int new_var;

    short* nativeWave = Audio_Seq_LfoWaveform((u_char)in_pChannel->programCounter[2]);
    if (nativeWave == NULL) {
        in_pChannel->programCounter += 3;
        return;
    }
    in_pChannel->updateFlags |= 1;

    if (in_pChannel->type != 0) {
        in_pChannel->vibratoDelay = 0;
        VibratoDepth = (u_char)*in_pChannel->programCounter++;
        if (VibratoDepth != 0) {
            in_pChannel->vibratoDepth = VibratoDepth << 8;
        }
    } else {
        in_pChannel->vibratoDelay = (u_char)*in_pChannel->programCounter++;
    }

    VibratoRatePhase = (u_char)*in_pChannel->programCounter++;
    in_pChannel->vibratoRate = VibratoRatePhase;

    if (VibratoRatePhase == 0) {
        in_pChannel->vibratoRate = 0x100;
    }

    in_pChannel->vibratoShape = (u_char)*in_pChannel->programCounter++;
    PitchBase = in_pChannel->pitchBase;
    new_var = PitchBase;
    DepthHigh = (in_pChannel->vibratoDepth & 0x7F00) >> 8;

    if (!(in_pChannel->vibratoDepth & 0x8000)) {
        VibratoBase = DepthHigh * ((new_var * 0xF) >> 8);
    } else {
        VibratoBase = DepthHigh * new_var;
    }
    in_pChannel->vibratoBase = VibratoBase >> 7;
    in_pChannel->vibratoWave = nativeWave;
    in_pChannel->vibratoDelayLeft = in_pChannel->vibratoDelay;
    in_pChannel->vibratoRateCounter = 1;
}

// decomp: SLUS_010.40 SoundVM_B5_VibratoDepth (src/SLUS_010.40/25AC.c:5507)
void Audio_Seq_SetVibratoDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int PitchBase; // Base pitch around which the vibrato will oscillate
    u_int DepthAmount; // This is just the magnitude part of the vibrato depth parameter,
                       // 0-127 depth scalar
    u_int VibratoProduct; // Intermediate scaled value

    PitchBase = in_pChannel->pitchBase;

    // Read vibrato depth parameter from the sequence.
    // Stored in the high byte so bit 15 can act as a mode flag.
    in_pChannel->vibratoDepth = *in_pChannel->programCounter++ << 8;

    // Extract the depth magnitude (bits 8–14)
    DepthAmount = (in_pChannel->vibratoDepth & 0x7F00) >> 8;
    if (!(in_pChannel->vibratoDepth & VIBRATO_FLAG_ABSOLUTE)) {
        // Relative mode:
        // Vibrato depth is scaled down relative to pitch so higher notes
        // don’t produce excessively wide vibrato.
        // (PitchBase * 15) >> 8 ≈ PitchBase * 0.0586
        VibratoProduct = DepthAmount * ((PitchBase * 15) >> 8);
    } else {
        // Absolute mode:
        // Vibrato depth is directly proportional to pitch.
        VibratoProduct = DepthAmount * PitchBase;
    }

    // Final vibrato amplitude in pitch units.
    // The >> 7 normalizes the multiplication into a usable range.
    in_pChannel->vibratoBase = VibratoProduct >> 7;
}

// decomp: SLUS_010.40 SoundVM_DD_VibratoDepthSlide (src/SLUS_010.40/25AC.c:5540)
void Audio_Seq_SlideVibratoDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short Length;
    u_short Prev;
    u_short Dest;
    int Delta;

    Length = *in_pChannel->programCounter++;
    if (Length == 0) {
        Length = 0x100;
    }
    Dest = *in_pChannel->programCounter++;
    Dest = Dest << 8;
    Prev = in_pChannel->vibratoDepth;
    Delta = Dest - Prev;
    in_pChannel->vibratoDepthSlideStep = Delta / Length;
    in_pChannel->vibratoDepthSlideLength = Length;
}

// decomp: SLUS_010.40 SoundVM_B6_DisableVibrato (src/SLUS_010.40/25AC.c:5560)
void Audio_Seq_DisableVibrato(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->vibratoPitch = 0;
    in_pChannel->updateFlags &= ~SOUND_UPDATE_VIBRATO;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_SAMPLE_RATE;
}

// decomp: SLUS_010.40 SoundVM_B8_Tremelo (src/SLUS_010.40/25AC.c:5568)
void Audio_Seq_EnableTremolo(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int Delay;
    int Rate;

    short* nativeWave = Audio_Seq_LfoWaveform((u_char)in_pChannel->programCounter[2]);
    if (nativeWave == NULL) {
        in_pChannel->programCounter += 3;
        return;
    }
    in_pChannel->updateFlags |= SOUND_UPDATE_TREMOLO;
    Delay = (u_char)*in_pChannel->programCounter++;
    if (in_pChannel->type != 0) {
        in_pChannel->tremoloDelay = 0;
        if (Delay != 0) {
            in_pChannel->tremoloDepth = (Delay & 0x7F) << 8;
        }
    } else {
        in_pChannel->tremoloDelay = Delay;
    }

    Rate = (u_char)*in_pChannel->programCounter++;
    in_pChannel->tremoloRate = Rate;

    if (Rate == 0) {
        in_pChannel->tremoloRate = 0x100;
    }

    in_pChannel->tremoloShape = (u_char)*in_pChannel->programCounter++;
    in_pChannel->tremoloWave = nativeWave;
    in_pChannel->tremoloDelayLeft = in_pChannel->tremoloDelay;
    in_pChannel->tremoloRateCounter = 1;
}

// decomp: SLUS_010.40 SoundVM_B9_TremeloDepth (src/SLUS_010.40/25AC.c:5598)
void Audio_Seq_SetTremoloDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->tremoloDepth = (*in_pChannel->programCounter++ & 0x7F) << 8;
}

// decomp: SLUS_010.40 SoundVM_DE_TremeloDepthSlide (src/SLUS_010.40/25AC.c:5604)
void Audio_Seq_SlideTremoloDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int Length;
    int Prev;
    u_int Dest;
    int Delta;

    Length = *in_pChannel->programCounter++;
    if (Length == 0) {
        Length = 0x100;
    }
    Dest = (*in_pChannel->programCounter++ & 0x7f) << 8;
    Prev = in_pChannel->tremoloDepth;
    Delta = Dest - Prev;
    in_pChannel->tremoloDepthSlideStep = Delta / Length;
    in_pChannel->tremoloDepthSlideLength = Length;
}

// decomp: SLUS_010.40 SoundVM_BA_DisableTremelo (src/SLUS_010.40/25AC.c:5623)
void Audio_Seq_DisableTremolo(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->tremoloVolume = 0;
    in_pChannel->updateFlags &= ~SOUND_UPDATE_TREMOLO;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
}

// decomp: SLUS_010.40 SoundVM_BC_AutoPan (src/SLUS_010.40/25AC.c:5631)
void Audio_Seq_EnableAutoPan(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int Rate;

    short* nativeWave = Audio_Seq_LfoWaveform((u_char)in_pChannel->programCounter[1]);
    if (nativeWave == NULL) {
        in_pChannel->programCounter += 2;
        return;
    }
    in_pChannel->updateFlags |= SOUND_UPDATE_PAN_LFO;
    Rate = (u_char)*in_pChannel->programCounter++;
    in_pChannel->autoPanRate = Rate;

    if (Rate == 0) {
        in_pChannel->autoPanRate = 0x100;
    }

    in_pChannel->autoPanShape = (u_char)*in_pChannel->programCounter++;
    in_pChannel->autoPanWave = nativeWave;
    in_pChannel->autoPanRateCounter = 1;
}

// decomp: SLUS_010.40 SoundVM_BD_AutoPanDepth (src/SLUS_010.40/25AC.c:5649)
void Audio_Seq_SetAutoPanDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->autoPanDepth = *in_pChannel->programCounter++ << 7;
}

// decomp: SLUS_010.40 SoundVM_DF_AutoPanDepthSlide (src/SLUS_010.40/25AC.c:5655)
void Audio_Seq_SlideAutoPanDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short Length;
    u_short Prev;
    u_short Dest;
    int Delta;

    Length = *in_pChannel->programCounter++;
    if (Length == 0) {
        Length = 0x100;
    }
    Dest = *in_pChannel->programCounter++;
    Dest = Dest << 7;
    Prev = in_pChannel->autoPanDepth;

    Delta = Dest - Prev;
    in_pChannel->autoPanDepthSlideStep = Delta / Length;
    in_pChannel->autoPanDepthSlideLength = Length;
}

// decomp: SLUS_010.40 SoundVM_BE_DisableAutoPan (src/SLUS_010.40/25AC.c:5676)
void Audio_Seq_DisableAutoPan(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->autoPanOffset = 0;
    in_pChannel->updateFlags &= ~SOUND_UPDATE_PAN_LFO;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
}

// decomp: SLUS_010.40 SoundVM_C4_EnableNoiseVoices (src/SLUS_010.40/25AC.c:5684)
void Audio_Seq_EnableNoise(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    if (in_pChannel->type == SOUND_CHANNEL_TYPE_MUSIC) {
        Audio_Seq_activeMusicConfig->noiseChannels |= in_VoiceFlags;
    } else {
        Audio_Mixer_voiceScheduler.noiseVoices |= in_VoiceFlags;
    }
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_04 | SOUND_GLOBAL_UPDATE_08;
}

// decomp: SLUS_010.40 SoundVM_C5_DisableNoiseVoices (src/SLUS_010.40/25AC.c:5695)
void Audio_Seq_DisableNoise(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    if (in_pChannel->type == SOUND_CHANNEL_TYPE_MUSIC) {
        Audio_Seq_activeMusicConfig->noiseChannels &= ~in_VoiceFlags;
    } else {
        Audio_Mixer_voiceScheduler.noiseVoices &= ~in_VoiceFlags;
    }
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_04 | SOUND_GLOBAL_UPDATE_08;
    in_pChannel->noiseTimer = 0;
}

// decomp: SLUS_010.40 SoundVM_C6_EnableFmVoices (src/SLUS_010.40/25AC.c:5707)
void Audio_Seq_EnableFrequencyModulation(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    if (in_pChannel->type == SOUND_CHANNEL_TYPE_MUSIC) {
        Audio_Seq_activeMusicConfig->fmChannels |= in_VoiceFlags;
    } else if (in_pChannel->updateFlags & SOUND_UPDATE_UNKNOWN_16) {
        Audio_Mixer_voiceScheduler.fmVoices |= in_VoiceFlags;
    }
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_08;
}

// decomp: SLUS_010.40 SoundVM_C7_DisableFmVoices (src/SLUS_010.40/25AC.c:5718)
void Audio_Seq_DisableFrequencyModulation(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    if (in_pChannel->type == SOUND_CHANNEL_TYPE_MUSIC) {
        Audio_Seq_activeMusicConfig->fmChannels &= ~in_VoiceFlags;
    } else {
        Audio_Mixer_voiceScheduler.fmVoices &= ~in_VoiceFlags;
    }
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_08;
    in_pChannel->fmTimer = 0;
}

// decomp: SLUS_010.40 SoundVM_C2_EnableReverbVoices (src/SLUS_010.40/25AC.c:5730)
void Audio_Seq_EnableReverb(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    if (in_pChannel->type == SOUND_CHANNEL_TYPE_MUSIC) {
        Audio_Seq_activeMusicConfig->reverbChannels |= in_VoiceFlags;
    } else {
        Audio_Mixer_voiceScheduler.reverbVoices |= in_VoiceFlags;
    }
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_08;
}

// decomp: SLUS_010.40 SoundVM_C3_DisableReverbVoices (src/SLUS_010.40/25AC.c:5741)
void Audio_Seq_DisableReverb(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    if (in_pChannel->type == SOUND_CHANNEL_TYPE_MUSIC) {
        Audio_Seq_activeMusicConfig->reverbChannels &= ~in_VoiceFlags;
    } else {
        Audio_Mixer_voiceScheduler.reverbVoices &= ~in_VoiceFlags;
    }
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_08;
}

// decomp: SLUS_010.40 SoundVM_AC_NoiseClockFrequency (src/SLUS_010.40/25AC.c:5776)
void Audio_Seq_SetNoiseClock(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int Frequency = *in_pChannel->programCounter++;
    if (in_pChannel->type == SOUND_CHANNEL_TYPE_MUSIC) {
        if (Frequency & 0xC0) {
            Audio_Seq_activeMusicConfig->noiseClock =
                (Audio_Seq_activeMusicConfig->noiseClock + (Frequency & 0x3F)) & 0x3F;
        } else {
            Audio_Seq_activeMusicConfig->noiseClock = Frequency;
        }
    } else if (Frequency & 0xC0) {
        Audio_Mixer_voiceScheduler.noiseClock =
            (Audio_Mixer_voiceScheduler.noiseClock + (Frequency & 0x3F)) & 0x3F;
    } else {
        Audio_Mixer_voiceScheduler.noiseClock = Frequency;
    }
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_04;
}

// decomp: src/SLUS_010.40/25AC.c:4985
#define SOUND_DEFAULT_DELAY_TIME (0x101) // 256 + 1
// decomp: SLUS_010.40 SoundVM_CE_EnableNoiseAndDelayToggle (src/SLUS_010.40/25AC.c:6072)
void Audio_Seq_EnableTimedNoise(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags)
{
    u_short Timer;
    u_short Value;

    Value = *in_pChannel->programCounter++;
    if (Value != 0) {
        Timer = Value + 1;
    } else {
        Timer = SOUND_DEFAULT_DELAY_TIME;
    }

    in_pChannel->noiseTimer = Timer;

    Audio_Seq_EnableNoise(in_pChannel, in_VoiceFlags);
}
#undef SOUND_DEFAULT_DELAY_TIME

// decomp: src/SLUS_010.40/25AC.c:4985
#define SOUND_DEFAULT_DELAY_TIME (0x101) // 256 + 1
// decomp: SLUS_010.40 SoundVM_CF_ToggleNoiseOnDelay (src/SLUS_010.40/25AC.c:6089)
void Audio_Seq_SetNoiseToggleDelay(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short Delay = *in_pChannel->programCounter++;
    if (Delay != 0) {
        in_pChannel->noiseTimer = Delay + 1;
        return;
    }
    in_pChannel->noiseTimer = SOUND_DEFAULT_DELAY_TIME;
}
#undef SOUND_DEFAULT_DELAY_TIME

// decomp: src/SLUS_010.40/25AC.c:4985
#define SOUND_DEFAULT_DELAY_TIME (0x101) // 256 + 1
// decomp: SLUS_010.40 SoundVM_D2_EnableFmAndDelayToggle (src/SLUS_010.40/25AC.c:6100)
void Audio_Seq_EnableTimedFrequencyModulation(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags)
{
    u_short Timer;
    u_short Value;

    Value = *in_pChannel->programCounter++;
    if (Value != 0) {
        Timer = Value + 1;
    } else {
        Timer = SOUND_DEFAULT_DELAY_TIME;
    }

    in_pChannel->fmTimer = Timer;
    Audio_Seq_EnableFrequencyModulation(in_pChannel, in_VoiceFlags);
}
#undef SOUND_DEFAULT_DELAY_TIME

// decomp: src/SLUS_010.40/25AC.c:4985
#define SOUND_DEFAULT_DELAY_TIME (0x101) // 256 + 1
// decomp: SLUS_010.40 SoundVM_D3_ToggleFmDelay (src/SLUS_010.40/25AC.c:6116)
void Audio_Seq_SetFrequencyModulationToggleDelay(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short Delay = *in_pChannel->programCounter++;
    if (Delay != 0) {
        in_pChannel->fmTimer = Delay + 1;
        return;
    }
    in_pChannel->fmTimer = SOUND_DEFAULT_DELAY_TIME;
}
#undef SOUND_DEFAULT_DELAY_TIME

// decomp: SLUS_010.40 SoundVM_CB_DisableVoiceModes (src/SLUS_010.40/25AC.c:6127)
void Audio_Seq_ResetVoiceEffects(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags)
{
    in_pChannel->updateFlags &=
        ~(SOUND_UPDATE_SIDE_CHAIN_PITCH | SOUND_UPDATE_SIDE_CHAIN_VOL
            | SOUND_UPDATE_PAN_LFO | SOUND_UPDATE_TREMOLO | SOUND_UPDATE_VIBRATO);
    Audio_Seq_DisableNoise(in_pChannel, in_VoiceFlags);
    Audio_Seq_DisableFrequencyModulation(in_pChannel, in_VoiceFlags);
    Audio_Seq_DisableReverb(in_pChannel, in_VoiceFlags);
    in_pChannel->playModes &= ~(SOUND_SFX_LEGATO | SOUND_SFX_FULL_LENGTH);
}

// decomp: SLUS_010.40 SoundVM_D4_EnablePlaybackRateSidechain (src/SLUS_010.40/25AC.c:6138)
void Audio_Seq_EnablePitchSidechain(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->updateFlags |= SOUND_UPDATE_SIDE_CHAIN_PITCH;
}

// decomp: SLUS_010.40 SoundVM_D5_DisablePlaybackRateSidechain (src/SLUS_010.40/25AC.c:6144)
void Audio_Seq_DisablePitchSidechain(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->updateFlags &= ~SOUND_UPDATE_SIDE_CHAIN_PITCH;
}

// decomp: SLUS_010.40 SoundVM_D6_EnablePitchVolumeSidechain (src/SLUS_010.40/25AC.c:6150)
void Audio_Seq_EnableVolumeSidechain(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->updateFlags |= SOUND_UPDATE_SIDE_CHAIN_VOL;
}

// decomp: SLUS_010.40 SoundVM_D7_DisablePitchVolumeSidechain (src/SLUS_010.40/25AC.c:6156)
void Audio_Seq_DisableVolumeSidechain(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->updateFlags &= ~SOUND_UPDATE_SIDE_CHAIN_VOL;
}

// decomp: SLUS_010.40 SoundVM_E1_SetRandomPitchDepth (src/SLUS_010.40/25AC.c:6221)
void Audio_Seq_SetRandomPitchDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->randomPitchDepth = *in_pChannel->programCounter++;
}

// decomp: SLUS_010.40 SoundVM_E2_ResetRandomPitchDepth (src/SLUS_010.40/25AC.c:6227)
void Audio_Seq_ResetRandomPitchDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->randomPitchDepth = 0;
}

