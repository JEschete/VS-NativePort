#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live bindings for StreamBuffers.hpp over the driver's stream state
// (g_Sound_Cutscene_StreamState, _isSpuTransfer), the bound room stream's
// bytes, the sequencer's voice scheduler, the voice output and the SPU
// transfer. The playback starts and refills keep the SPU callback shape
// (void()), so an SPU transfer completion can store them: the second mono
// upload stores LiveBeginMonoPlayback. A chunk cursor outside the bound room
// stream, a voice outside the pair range or a chunk past the stream's end is
// refused with a diagnostic.
namespace VS::Audio::Cutscene {
// The first voice of the highest free stream voice pair, or -1.
int LiveFindFreeVoicePair();
void LiveUploadSecondMonoBuffer();
void LiveBeginMonoPlayback();
void LiveBeginStereoPlayback();
void LiveRefillMonoFirstBuffer();
void LiveRefillMonoSecondBuffer();
void LiveRefillStereoFirstBuffer();
void LiveRefillStereoSecondBuffer();
}  // namespace VS::Audio::Cutscene
