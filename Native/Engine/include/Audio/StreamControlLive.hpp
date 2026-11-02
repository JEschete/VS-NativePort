#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>

struct Audio_CommandParams;

// Bindings from the sound command table and the effect package loader to the
// typed cutscene stream commands (StreamControl.cpp) over the driver's stream
// state, the sequencer's voice scheduler and the native voice engine. The
// command handlers keep the table's callback signature; a handler that reads
// its parameters refuses a null block with a diagnostic.
namespace VS::Audio::Cutscene {
void PauseLiveStream(Audio_CommandParams* params);
void ResumeLiveStream(Audio_CommandParams* params);
void ReplaceLiveStream(Audio_CommandParams* params);
void StopLiveStream(Audio_CommandParams* params);
void SetLiveStreamVolume(Audio_CommandParams* params);
void SlideLiveStreamVolume(Audio_CommandParams* params);
void SetLiveStreamPan(Audio_CommandParams* params);
}  // namespace VS::Audio::Cutscene

namespace VS::Audio {
// Binds the AKAO stream occupying `byte_count` bytes at `data` to the room
// stream slot; returns its handle, or 0 when the slot is taken or the stream
// or its exported assets are invalid. The package keeps owning the bytes.
int BindLiveRoomCutsceneStream(void* data, std::size_t byte_count);
int UnbindLiveRoomCutsceneStream();
}  // namespace VS::Audio
