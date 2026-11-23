#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace VS::Content {

// The str-mkv converter (ASSET_PIPELINE.md §6.7, #504): each movie becomes
// disc\<path>.mkv (AV1 video, FLAC audio), and files.tsv lists it in form
// "mkv". Movies are interleaved, so the manifest hashes their raw 2352-byte
// sectors.
inline constexpr std::string_view kStrMkvConverterName = "str-mkv";
inline constexpr std::uint32_t kStrMkvConverterVersion = 1;
inline constexpr std::string_view kMovieExtension = ".STR";
inline constexpr std::string_view kMovieForm = "mkv";

// The FFmpeg AV1 encoder that converts a movie, with its quality settings.
struct Av1Encoder {
  std::string name;
  std::vector<std::string> options;
};

// Picks an encoder from the output of `ffmpeg -encoders`. libsvtav1 is much
// faster; builds such as Gyan's "essentials" only have libaom-av1.
std::optional<Av1Encoder> ChooseAv1Encoder(std::string_view encoder_listing);

// Asks this FFmpeg which encoders it has. `scratch` holds its answer briefly.
// The error names the problem, for showing to the player.
std::expected<Av1Encoder, std::string> FindAv1Encoder(
    const std::filesystem::path& ffmpeg, const std::filesystem::path& scratch);

}  // namespace VS::Content
