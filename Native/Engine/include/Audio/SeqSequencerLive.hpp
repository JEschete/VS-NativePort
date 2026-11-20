#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

namespace VS::Audio {
struct SeqChannel;
}  // namespace VS::Audio

// Binds the typed sequencer (SequencerCore.cpp in src\Audio\Seq) to the live
// driver: the music and sound-effect channel tables, the active, primary and
// secondary music layers, the voice scheduler and the driver's latches, slides
// and tick costs. A channel step reads its program only from the memory the
// program lies in (LiveChannelProgram); a step that would read outside it is
// refused, reported once per power of two, and finishes the channel.
namespace VS::Audio::Seq {
// func_8001A8D8: runs the channel's commands up to its next note and starts
// it. `channel_bit` is the channel's bit in its layer's or the voice
// scheduler's masks.
void LiveExecuteChannelStep(VS::Audio::SeqChannel* channel, int channel_bit);
// func_80019A58: one driver tick, the tick timer's event handler. Returns the
// tick's cost in timer counts.
std::int32_t LiveTickPlayback();
// func_8001924C: the slides the driver runs every fourth tick.
void LiveUpdateGlobalSlides();
// Sound_ClearVoiceFromSchedulerState: releases `voice_bit` from the voice
// scheduler and clears the channel's sound-effect identity.
void LiveReleaseSoundEffectVoice(VS::Audio::SeqChannel* channel, int voice_bit);
}  // namespace VS::Audio::Seq
