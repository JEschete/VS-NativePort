#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "Content/AudioStream.hpp"
#include "Content/JsonText.hpp"

// The stream.json of an effect-package sample stream (ASSET_PIPELINE.md §6.5,
// #520), in schema vs.audio-stream (Native/Content/schemas/
// audio-stream.schema.json). It names what the stream's 64-byte AKAO header
// holds (AudioStreamInfo): channels, frames, the SPU pitch and its sample rate,
// the loop and the unread start page. It is a record of the stream, not its
// source: the runtime reads the stream from its effect package and refuses to
// bind one whose stream.json differs. The audio is the WAV and SFZ files
// beside it (AudioStreamInstrumentPath).
namespace VS::Content {

inline constexpr std::string_view kAudioStreamSchema = "vs.audio-stream";
inline constexpr std::uint32_t kAudioStreamSchemaVersion = 2;

// The document of `info`.
[[nodiscard]] JsonDocument AudioStreamJson(const AudioStreamInfo& info);
// The stream `json` describes; errors name the member at fault under `where`.
// Its values must be ones a stream header holds (ReadAudioStreamHeader).
[[nodiscard]] std::expected<AudioStreamInfo, std::string> AudioStreamInfoOf(
    const JsonDocument& json, std::string_view where);

}  // namespace VS::Content
