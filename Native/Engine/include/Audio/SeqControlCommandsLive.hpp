#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Audio_SeqChannel;

// Bindings from the sequencer's opcode handler tables
// (Audio_Seq_kOpcodeHandlers and Audio_Seq_kExtendedOpcodeHandlers, stored in
// the module image) to the typed flow-control commands (Control.cpp in
// src\Audio\Seq). Each handler keeps the table's signature. It runs the
// command over the channel's program, bounded by the memory the program lies
// in (LiveChannelProgram), and its music layer. A command whose operands or
// target lie outside that memory, or that needs a music layer when none is
// active, is refused and changes nothing. `channel_bit` is the channel's bit
// in the layer's and the voice scheduler's masks.
namespace VS::Audio::Seq {
void LiveFinishChannel(Audio_SeqChannel* channel, int channel_bit);
void LiveFinishChannelOnReplacement(Audio_SeqChannel* channel, int channel_bit);
void LiveStopOnInvalidInstruction(Audio_SeqChannel* channel, int channel_bit);
void LiveIgnoreInstruction(Audio_SeqChannel* channel, int channel_bit);
void LiveSetTempo(Audio_SeqChannel* channel, int channel_bit);
void LiveSlideTempo(Audio_SeqChannel* channel, int channel_bit);
void LiveSetReverbDepth(Audio_SeqChannel* channel, int channel_bit);
void LiveSlideReverbDepth(Audio_SeqChannel* channel, int channel_bit);
void LiveJumpRelative(Audio_SeqChannel* channel, int channel_bit);
void LiveJumpAtThreshold(Audio_SeqChannel* channel, int channel_bit);
void LiveCallRelative(Audio_SeqChannel* channel, int channel_bit);
void LiveReturnFromCall(Audio_SeqChannel* channel, int channel_bit);
void LiveSetSequenceControlValue(Audio_SeqChannel* channel, int channel_bit);
void LiveSetVoiceAllocationStart(Audio_SeqChannel* channel, int channel_bit);
void LiveResetVoiceAllocationStart(Audio_SeqChannel* channel, int channel_bit);
void LivePushLoop(Audio_SeqChannel* channel, int channel_bit);
void LiveRepeatLoop(Audio_SeqChannel* channel, int channel_bit);
void LiveJumpOnLoopPass(Audio_SeqChannel* channel, int channel_bit);
void LiveExitLoopOnPass(Audio_SeqChannel* channel, int channel_bit);
void LiveRepeatLoopForever(Audio_SeqChannel* channel, int channel_bit);
void LiveSetTimerDivisors(Audio_SeqChannel* channel, int channel_bit);
void LiveSetTimerCount(Audio_SeqChannel* channel, int channel_bit);
// Also starts the two sound-effect programs FE0B names
// (Audio_SoundEffects_PlayProgramPair) and steps past its operands.
void LivePlayEmbeddedSoundEffect(Audio_SeqChannel* channel, int channel_bit);
void LiveSelectNoteRemapTable(Audio_SeqChannel* channel, int channel_bit);
void LiveMarkChannelKeyed(Audio_SeqChannel* channel, int channel_bit);
void LiveClearChannelKeyed(Audio_SeqChannel* channel, int channel_bit);
}  // namespace VS::Audio::Seq
