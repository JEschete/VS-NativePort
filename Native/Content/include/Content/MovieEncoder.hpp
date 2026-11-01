#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace VS::Content {

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
