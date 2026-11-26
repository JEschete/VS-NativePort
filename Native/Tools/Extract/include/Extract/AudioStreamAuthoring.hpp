#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

// Moves the effect-stream records `VS.Extract streams` writes
// (audio\streams\akao\<id>\stream.json) to schema vs.audio-stream
// (Content/AudioStreamJson.hpp). Streams are not in manifest.json, so the
// upgrade reads each stream directory.
namespace VS::Extract {

// The stream.json version that kept the stream's 64-byte AKAO header as a
// byte list and named only the values the native player derives from it.
inline constexpr std::uint32_t kNumberedStreamVersion = 1;

struct StreamAuthoringReport {
  std::size_t streams = 0;  // stream.json files read
  std::size_t written = 0;  // files moved to schema vs.audio-stream
};

// Rewrites each version 1 stream.json under `root` in schema vs.audio-stream,
// without reextracting the stream or dropping its display_name label. The
// header is read as the player reads it and must agree with the values the
// file already names; it is dropped once the named values rebuild it byte for
// byte. A file already current is checked and left as it is, so an
// interrupted run resumes.
[[nodiscard]] std::expected<StreamAuthoringReport, std::string>
UpgradeStreamAuthoring(const std::filesystem::path& root);

}  // namespace VS::Extract
