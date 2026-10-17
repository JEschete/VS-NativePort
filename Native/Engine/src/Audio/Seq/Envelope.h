// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SEQ_ENVELOPE_H_TYPES
#define VS_AUDIO_SEQ_ENVELOPE_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/sound.h:23
#define VOICE_PARAM_ADSR_AMODE (1 << 8) /* ADSR Attack rate mode */

// decomp: src/include/sound.h:24
#define VOICE_PARAM_ADSR_SMODE (1 << 9) /* ADSR Sustain rate mode */

// decomp: src/include/sound.h:25
#define VOICE_PARAM_ADSR_RMODE (1 << 10) /* ADSR Release rate mode */

// decomp: src/include/sound.h:26
#define VOICE_PARAM_ADSR_AR (1 << 11) /* ADSR Attack rate */

// decomp: src/include/sound.h:27
#define VOICE_PARAM_ADSR_DR (1 << 12) /* ADSR Decay rate */

// decomp: src/include/sound.h:28
#define VOICE_PARAM_ADSR_SR (1 << 13) /* ADSR Sustain rate */

// decomp: src/include/sound.h:29
#define VOICE_PARAM_ADSR_RR (1 << 14) /* ADSR Release rate */

// decomp: src/include/sound.h:30
#define VOICE_PARAM_ADSR_SL (1 << 15) /* ADSR Sustain level */

// decomp: src/include/sound.h:37
#define VOICE_PARAM_ADSR_FULL (VOICE_PARAM_ADSR_LOWER | VOICE_PARAM_ADSR_UPPER)

// decomp: src/include/sound.h:118
#define BIT_MASK(width) ((1u << (width)) - 1u)

// decomp: src/include/sound.h:127
#define SOUND_ADSR_ATTACK_MODE_MASK                                                      \
    (BIT_MASK(SOUND_ADSR_ATTACK_MODE_WIDTH)                                              \
        << SOUND_ADSR_ATTACK_MODE_SHIFT) // 0x8000  // bit 15

// decomp: src/include/sound.h:130
#define SOUND_ADSR_ATTACK_RATE_MASK                                                      \
    (BIT_MASK(SOUND_ADSR_ATTACK_RATE_WIDTH)                                              \
        << SOUND_ADSR_ATTACK_RATE_SHIFT) // 0x7F00  // bits 14–8

// decomp: src/include/sound.h:133
#define SOUND_ADSR_DECAY_RATE_MASK                                                       \
    (BIT_MASK(SOUND_ADSR_DECAY_RATE_WIDTH)                                               \
        << SOUND_ADSR_DECAY_RATE_SHIFT) // 0x00F0  // bits 7–4

// decomp: src/include/sound.h:136
#define SOUND_ADSR_SUS_LEVEL_MASK                                                        \
    (BIT_MASK(SOUND_ADSR_SUS_LEVEL_WIDTH)                                                \
        << SOUND_ADSR_SUS_LEVEL_SHIFT) // 0x000F  // bits 3–0

// decomp: src/include/sound.h:140
#define SOUND_ADSR_ATTACK_MODE_SHIFT 15

// decomp: src/include/sound.h:141
#define SOUND_ADSR_ATTACK_RATE_SHIFT 8

// decomp: src/include/sound.h:142
#define SOUND_ADSR_DECAY_RATE_SHIFT 4

// decomp: src/include/sound.h:143
#define SOUND_ADSR_SUS_LEVEL_SHIFT 0

// decomp: src/include/sound.h:145
#define SOUND_ADSR_ATTACK_MODE_WIDTH 1

// decomp: src/include/sound.h:146
#define SOUND_ADSR_ATTACK_RATE_WIDTH 7

// decomp: src/include/sound.h:147
#define SOUND_ADSR_DECAY_RATE_WIDTH 4

// decomp: src/include/sound.h:148
#define SOUND_ADSR_SUS_LEVEL_WIDTH 4

// decomp: src/include/sound.h:159
#define SOUND_ADSR_SUS_RATE_MASK                                                         \
    (BIT_MASK(SOUND_ADSR_SUS_RATE_WIDTH) << SOUND_ADSR_SUS_RATE_SHIFT) //   0x3F00

// decomp: src/include/sound.h:161
#define SOUND_ADSR_RELEASE_MODE_MASK                                                     \
    (BIT_MASK(SOUND_ADSR_RELEASE_MODE_WIDTH) << SOUND_ADSR_RELEASE_MODE_SHIFT) //   0x00E0

// decomp: src/include/sound.h:163
#define SOUND_ADSR_RELEASE_RATE_MASK                                                     \
    (BIT_MASK(SOUND_ADSR_RELEASE_RATE_WIDTH) << SOUND_ADSR_RELEASE_RATE_SHIFT) //   0x00E0

// decomp: src/include/sound.h:167
#define SOUND_ADSR_SUS_RATE_SHIFT 6

// decomp: src/include/sound.h:168
#define SOUND_ADSR_RELEASE_MODE_SHIFT 5

// decomp: src/include/sound.h:169
#define SOUND_ADSR_RELEASE_RATE_SHIFT 0

// decomp: src/include/sound.h:172
#define SOUND_ADSR_SUS_RATE_WIDTH 7

// decomp: src/include/sound.h:173
#define SOUND_ADSR_RELEASE_MODE_WIDTH 1

// decomp: src/include/sound.h:174
#define SOUND_ADSR_RELEASE_RATE_WIDTH 5

// decomp: src/include/sound.h:177
#define SOUND_AMODE_5 5

// decomp: src/include/sound.h:178
#define SOUND_RMODE_7 7

#endif // VS_AUDIO_SEQ_ENVELOPE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SEQ_ENVELOPE_H_DECLS)
#define VS_AUDIO_SEQ_ENVELOPE_H_DECLS
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"

void Audio_Seq_ResetEnvelope(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetAttackRate(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetDecayRate(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetSustainLevel(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetSustainRate(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetReleaseRate(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetAttackMode(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetSustainMode(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetReleaseMode(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetDecayAndSustain(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags);

#endif // VS_AUDIO_SEQ_ENVELOPE_H_DECLS
