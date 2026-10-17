// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SEQ_MODULATION_H_TYPES
#define VS_AUDIO_SEQ_MODULATION_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/sound.h:76
#define SOUND_UPDATE_UNKNOWN_16 (1 << 16)

// decomp: src/include/sound.h:180
#define VIBRATO_FLAG_ABSOLUTE (1 << 15)

#endif // VS_AUDIO_SEQ_MODULATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SEQ_MODULATION_H_DECLS)
#define VS_AUDIO_SEQ_MODULATION_H_DECLS
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"

void Audio_Seq_EnableVibrato(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetVibratoDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SlideVibratoDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisableVibrato(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableTremolo(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetTremoloDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SlideTremoloDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisableTremolo(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableAutoPan(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetAutoPanDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SlideAutoPanDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisableAutoPan(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableNoise(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisableNoise(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableFrequencyModulation(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisableFrequencyModulation(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableReverb(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisableReverb(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetNoiseClock(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableTimedNoise(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags);
void Audio_Seq_SetNoiseToggleDelay(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableTimedFrequencyModulation(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags);
void Audio_Seq_SetFrequencyModulationToggleDelay(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_ResetVoiceEffects(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags);
void Audio_Seq_EnablePitchSidechain(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisablePitchSidechain(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableVolumeSidechain(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisableVolumeSidechain(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetRandomPitchDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_ResetRandomPitchDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));

#endif // VS_AUDIO_SEQ_MODULATION_H_DECLS
