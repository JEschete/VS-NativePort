// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Envelope.h"

// decomp: SLUS_010.40 SoundVM_B3_ResetAdsr (src/SLUS_010.40/25AC.c:5366)
void Audio_Seq_ResetEnvelope(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    Audio_Instrument* InstrumentInfo =
        &Audio_Samples_instruments[in_pChannel->instrumentIndex];
    in_pChannel->voice.adsrLower = InstrumentInfo->adsrLower;
    in_pChannel->voice.adsrUpper = InstrumentInfo->adsrUpper;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_ADSR_FULL;
    in_pChannel->updateFlags &=
        ~(SOUND_UPDATE_UNKNOWN_24 | SOUND_UPDATE_UNKNOWN_27 | SOUND_UPDATE_UNKNOWN_28);
}

// decomp: SLUS_010.40 SoundVM_AD_AttackRate (src/SLUS_010.40/25AC.c:5796)
void Audio_Seq_SetAttackRate(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short AttackRate = *in_pChannel->programCounter++;
    in_pChannel->voice.adsrLower &= ~SOUND_ADSR_ATTACK_RATE_MASK;
    in_pChannel->voice.adsrLower |= AttackRate << SOUND_ADSR_ATTACK_RATE_SHIFT;
    in_pChannel->voice.dirtyFlags |=
        (VOICE_PARAM_ADSR_AMODE | VOICE_PARAM_ADSR_AR);
    in_pChannel->updateFlags |= SOUND_UPDATE_UNKNOWN_24;
}

// decomp: SLUS_010.40 SoundVM_AE_DecayRate (src/SLUS_010.40/25AC.c:5807)
void Audio_Seq_SetDecayRate(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short DecayRate = *in_pChannel->programCounter++;
    in_pChannel->voice.adsrLower &= ~SOUND_ADSR_DECAY_RATE_MASK;
    in_pChannel->voice.adsrLower |= DecayRate << SOUND_ADSR_DECAY_RATE_SHIFT;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_ADSR_DR;
}

// decomp: SLUS_010.40 SoundVM_AF_SustainLevel (src/SLUS_010.40/25AC.c:5816)
void Audio_Seq_SetSustainLevel(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short SustainLevel = *in_pChannel->programCounter++;
    in_pChannel->voice.adsrLower &= ~SOUND_ADSR_SUS_LEVEL_MASK;
    in_pChannel->voice.adsrLower |= SustainLevel;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_ADSR_SL;
}

// decomp: SLUS_010.40 SoundVM_B1_SustainRate (src/SLUS_010.40/25AC.c:5825)
void Audio_Seq_SetSustainRate(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short SustainRate = *in_pChannel->programCounter++;
    in_pChannel->voice.adsrUpper &= ~SOUND_ADSR_SUS_RATE_MASK;
    in_pChannel->voice.adsrUpper |= SustainRate << SOUND_ADSR_SUS_RATE_SHIFT;
    in_pChannel->voice.dirtyFlags |=
        (VOICE_PARAM_ADSR_SR | VOICE_PARAM_ADSR_SMODE);
    in_pChannel->updateFlags |= SOUND_UPDATE_UNKNOWN_27;
}

// decomp: SLUS_010.40 SoundVM_B2_ReleaseRate (src/SLUS_010.40/25AC.c:5836)
void Audio_Seq_SetReleaseRate(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short ReleaseRate = *in_pChannel->programCounter++;
    in_pChannel->voice.adsrUpper &= ~SOUND_ADSR_RELEASE_RATE_MASK;
    in_pChannel->voice.adsrUpper |= ReleaseRate << SOUND_ADSR_RELEASE_RATE_SHIFT;
    in_pChannel->voice.dirtyFlags |=
        (VOICE_PARAM_ADSR_RR | VOICE_PARAM_ADSR_RMODE);
    in_pChannel->updateFlags |= SOUND_UPDATE_UNKNOWN_28;
}

// decomp: SLUS_010.40 SoundVM_B7_AttackMode (src/SLUS_010.40/25AC.c:5847)
void Audio_Seq_SetAttackMode(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short Mode = *in_pChannel->programCounter++;

    in_pChannel->voice.adsrLower &= ~SOUND_ADSR_ATTACK_MODE_MASK;

    if (Mode == SOUND_AMODE_5) {
        in_pChannel->voice.adsrLower |= SOUND_ADSR_ATTACK_MODE_MASK;
    }
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_ADSR_AMODE;
}

// decomp: SLUS_010.40 SoundVM_BB_SustainMode (src/SLUS_010.40/25AC.c:5860)
void Audio_Seq_SetSustainMode(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short mode = *(in_pChannel->programCounter++);
    in_pChannel->voice.adsrUpper &= ~((1 << 14) | (1 << 15));

    switch (mode) {
    case 3:
        in_pChannel->voice.adsrUpper |= 0x4000;
        break;
    case 5:
        in_pChannel->voice.adsrUpper |= 0x8000;
        break;
    case 7:
        in_pChannel->voice.adsrUpper |= 0xC000;
        break;
    }
    in_pChannel->voice.dirtyFlags |= 0x200;
}

// decomp: SLUS_010.40 SoundVM_BF_ReleaseMode (src/SLUS_010.40/25AC.c:5880)
void Audio_Seq_SetReleaseMode(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short Value = *in_pChannel->programCounter++;

    in_pChannel->voice.adsrUpper &= ~SOUND_ADSR_RELEASE_MODE_MASK;
    if (Value == SOUND_RMODE_7) {
        in_pChannel->voice.adsrUpper |= SOUND_ADSR_RELEASE_MODE_MASK;
    }
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_ADSR_RMODE;
}

// decomp: SLUS_010.40 SoundVM_B0_DecayRateAndSustainLevel (src/SLUS_010.40/25AC.c:6066)
void Audio_Seq_SetDecayAndSustain(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags)
{
    Audio_Seq_SetDecayRate(in_pChannel, in_VoiceFlags);
    Audio_Seq_SetSustainLevel(in_pChannel, in_VoiceFlags);
}

}  // extern "C"
