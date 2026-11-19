#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Audio {
struct SeqChannel;
}  // namespace VS::Audio

// Bindings from the sequencer's opcode handler tables
// (VS::Audio::Seq::kOpcodeHandlers and VS::Audio::Seq::kExtendedOpcodeHandlers,
// stored in the module image) to the typed flow-control commands (Control.cpp
// in src\Audio\Seq). Each handler keeps the table's signature. It runs the
// command over the channel's program, bounded by the memory the program lies
// in (LiveChannelProgram), and its music layer. A command whose operands or
// target lie outside that memory, or that needs a music layer when none is
// active, is refused and changes nothing. `channel_bit` is the channel's bit
// in the layer's and the voice scheduler's masks.
namespace VS::Audio::Seq {
void LiveFinishChannel(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveFinishChannelOnReplacement(VS::Audio::SeqChannel* channel,
                                    int channel_bit);
void LiveStopOnInvalidInstruction(VS::Audio::SeqChannel* channel,
                                  int channel_bit);
void LiveIgnoreInstruction(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetTempo(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSlideTempo(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetReverbDepth(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSlideReverbDepth(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveJumpRelative(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveJumpAtThreshold(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveCallRelative(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveReturnFromCall(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetSequenceControlValue(VS::Audio::SeqChannel* channel,
                                 int channel_bit);
void LiveSetVoiceAllocationStart(VS::Audio::SeqChannel* channel,
                                 int channel_bit);
void LiveResetVoiceAllocationStart(VS::Audio::SeqChannel* channel,
                                   int channel_bit);
void LivePushLoop(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveRepeatLoop(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveJumpOnLoopPass(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveExitLoopOnPass(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveRepeatLoopForever(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetTimerDivisors(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetTimerCount(VS::Audio::SeqChannel* channel, int channel_bit);
// Also starts the two sound-effect programs FE0B names
// (SoundEffectProgramsLive.hpp LivePlayProgramPair) and steps past its
// operands.
void LivePlayEmbeddedSoundEffect(VS::Audio::SeqChannel* channel,
                                 int channel_bit);
void LiveSelectNoteRemapTable(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveMarkChannelKeyed(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveClearChannelKeyed(VS::Audio::SeqChannel* channel, int channel_bit);
}  // namespace VS::Audio::Seq
