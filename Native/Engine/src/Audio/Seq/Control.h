// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SEQ_CONTROL_H_TYPES
#define VS_AUDIO_SEQ_CONTROL_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/sound.h:90
#define SOUND_GLOBAL_UPDATE_07 (1 << 7)

// decomp: src/include/sound.h:318
#define SOUND_LOOP_STACK_SIZE (4)

// decomp: src/include/sound.h:319
#define SOUND_LOOP_STACK_MAX_INDEX (SOUND_LOOP_STACK_SIZE - 1)

#endif // VS_AUDIO_SEQ_CONTROL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SEQ_CONTROL_H_DECLS)
#define VS_AUDIO_SEQ_CONTROL_H_DECLS
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"

void Audio_Seq_FinishChannel(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags);
void Audio_Seq_FinishChannelOnReplacement(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags);
void Audio_Seq_SetTempo(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SlideTempo(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetReverbDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SlideReverbDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_JumpRelative(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_JumpAtThreshold(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_CallRelative(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_ReturnFromCall(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetSequenceControlValue(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_IgnoreInstruction(Audio_SeqChannel* in_pChannels __attribute__((unused)),
    u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetVoiceAllocationStart(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_ResetVoiceAllocationStart(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_PushLoop(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_RepeatLoop(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_JumpOnLoopPass(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_ExitLoopOnPass(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_RepeatLoopForever(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetTimerDivisors(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetTimerCount(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_PlayEmbeddedSoundEffect(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SelectNoteRemapTable(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_MarkChannelKeyed(
    Audio_SeqChannel* in_pChannel __attribute__((unused)), u_int in_VoiceFlags);
void Audio_Seq_ClearChannelKeyed(
    Audio_SeqChannel* in_pChannel __attribute__((unused)), u_int in_VoiceFlags);
void Audio_Seq_StopOnInvalidInstruction(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags);

#endif // VS_AUDIO_SEQ_CONTROL_H_DECLS
