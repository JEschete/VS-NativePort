#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Audio_SeqChannel;

// Binds the typed sequencer (SequencerCore.cpp in src\Audio\Seq) to the live
// driver: the music and sound-effect channel tables, the active, primary and
// secondary music layers, the voice scheduler and the driver's latches, slides
// and tick costs. A channel step reads its program only from the memory the
// program lies in (LiveChannelProgram); a step that would read outside it is
// refused, reported once per power of two, and finishes the channel.
namespace VS::Audio::Seq {
// The result of the root counter event callbacks OpenEvent stores (the SDK's
// C long).
// NOLINTNEXTLINE(runtime/int, google-runtime-int)
using CounterEventResult = long;

// func_8001A8D8: runs the channel's commands up to its next note and starts
// it. `channel_bit` is the channel's bit in its layer's or the voice
// scheduler's masks.
void LiveExecuteChannelStep(Audio_SeqChannel* channel, int channel_bit);
// func_80019A58: one driver tick, the root counter 2 event handler. Returns
// the tick's cost in timer counts.
CounterEventResult LiveTickPlayback();
// func_8001924C: the slides the driver runs every fourth tick.
void LiveUpdateGlobalSlides();
// Sound_ClearVoiceFromSchedulerState: releases `voice_bit` from the voice
// scheduler and clears the channel's sound-effect identity.
void LiveReleaseSoundEffectVoice(Audio_SeqChannel* channel, int voice_bit);
}  // namespace VS::Audio::Seq
