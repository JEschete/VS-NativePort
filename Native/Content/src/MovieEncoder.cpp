#include "Content/MovieEncoder.hpp"

#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "Platform/App.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {

namespace fs = std::filesystem;

std::optional<Av1Encoder> ChooseAv1Encoder(std::string_view encoder_listing) {
  std::set<std::string> video_encoders;
  std::istringstream lines{std::string(encoder_listing)};
  for (std::string line; std::getline(lines, line);) {
    std::istringstream words(line);
    std::string flags;
    std::string name;
    if (words >> flags >> name && flags.starts_with('V')) {
      video_encoders.insert(name);
    }
  }
  if (video_encoders.contains("libsvtav1")) {
    return Av1Encoder{"libsvtav1",
                      {"-preset", "6", "-svtav1-params", "lp=2", "-crf", "18"}};
  }
  if (video_encoders.contains("libaom-av1")) {
    return Av1Encoder{"libaom-av1",
                      {"-cpu-used", "6", "-row-mt", "1", "-threads", "2",
                       "-crf", "18", "-b:v", "0"}};
  }
  return std::nullopt;
}

std::expected<Av1Encoder, std::string> FindAv1Encoder(const fs::path& ffmpeg,
                                                      const fs::path& scratch) {
  const std::string name = Platform::PathText(ffmpeg);
  std::error_code ignored;
  fs::remove(scratch, ignored);  // process output is added to what is there
  const auto result = Platform::RunProcess(
      std::vector<std::string>{name, "-hide_banner", "-encoders"}, scratch);
  std::string listing;
  {
    std::ifstream input(scratch, std::ios::binary);
    listing.assign(std::istreambuf_iterator<char>(input),
                   std::istreambuf_iterator<char>());
  }
  fs::remove(scratch, ignored);
  if (!result || *result != 0) {
    return std::unexpected(
        "FFmpeg could not list its encoders: " +
        (result ? "exit code " + std::to_string(*result) : result.error()) +
        "\nFFmpeg: " + name);
  }
  auto encoder = ChooseAv1Encoder(listing);
  if (!encoder) {
    return std::unexpected(
        "This FFmpeg cannot convert the game's intro movie: it has neither the "
        "libsvtav1 nor the libaom-av1 encoder.\nFFmpeg: " +
        name + "\nUse Install with winget, or download a full FFmpeg build.");
  }
  return *encoder;
}

}  // namespace VS::Content
