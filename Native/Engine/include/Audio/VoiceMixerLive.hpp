#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Audio/PlaybackReset.hpp"

struct Audio_SeqChannel;

// Live bindings for VoiceMixer.hpp over the music and sound-effect channel
// tables, the active, primary and secondary music layers, the voice
// scheduler, envelopes, owners and mode masks, and the voice output. No
// module image stores these functions' addresses.
//
// An LFO pointer is read as a position in the LFO shape table's waveform
// that holds it (from a shape's first sample through its loop marker); a
// pointer outside every waveform reads nothing and is refused when the LFO
// steps. A channel's side chains read the record before it only within the
// music, secondary music and sound-effect channel tables.
namespace VS::Audio::Mixer {
// Sound_UpdateSlidesAndDelays: `channel_bit` is the channel's bit in the
// active layer's (music) or the voice scheduler's (sound effects) masks.
void LiveUpdateChannelSlidesAndDelays(Audio_SeqChannel* channel,
                                      unsigned channel_bit,
                                      Driver::ChannelKind kind);
// func_8001436C over the active music layer.
void LiveUpdateMusicChannelModulation(Audio_SeqChannel* channel);
// func_800147CC.
void LiveUpdateSoundEffectModulation(Audio_SeqChannel* channel);
// func_80015220: the tick's voice allocation, modulation and key-on.
void LiveCommitPlaybackVoices();
// Sound_ProcessKeyOffRequests: the tick's key-off.
void LiveReleasePendingVoices();
}  // namespace VS::Audio::Mixer
