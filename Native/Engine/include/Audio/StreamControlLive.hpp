#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Audio {
struct CommandParams;
}  // namespace VS::Audio

// Bindings from the sound command table and the effect package loader to the
// typed cutscene stream commands (StreamControl.cpp) over the driver's stream
// state, the sequencer's voice scheduler and the native voice engine. The
// command handlers keep the table's callback signature; a handler that reads
// its parameters refuses a null block with a diagnostic.
namespace VS::Audio::Cutscene {
void PauseLiveStream(VS::Audio::CommandParams* params);
void ResumeLiveStream(VS::Audio::CommandParams* params);
void ReplaceLiveStream(VS::Audio::CommandParams* params);
void StopLiveStream(VS::Audio::CommandParams* params);
void SetLiveStreamVolume(VS::Audio::CommandParams* params);
void SlideLiveStreamVolume(VS::Audio::CommandParams* params);
void SetLiveStreamPan(VS::Audio::CommandParams* params);
// The bytes of the stream bound to the room stream slot; empty when none is.
std::span<std::uint8_t> LiveBoundStreamBytes();
}  // namespace VS::Audio::Cutscene

namespace VS::Audio {
// Binds the AKAO stream occupying `byte_count` bytes at `data` to the room
// stream slot; returns its handle, or 0 when the slot is taken or the stream
// or its exported assets are invalid. The package keeps owning the bytes.
int BindLiveRoomCutsceneStream(void* data, std::size_t byte_count);
int UnbindLiveRoomCutsceneStream();
}  // namespace VS::Audio
