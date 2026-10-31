#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace VS::Launcher {

struct Settings {
  std::filesystem::path cue;
  std::filesystem::path saves;
  std::filesystem::path ffmpeg;
};

using Result = std::expected<void, std::string>;
inline constexpr std::uint64_t kSupportedDiscBytes = 750643152;
inline constexpr auto kSupportedDiscSha256 = std::string_view(
    "25221D7F8FDB2C4490AF12D9E82F0E58BDD6ACB43387F9082B3C5E79DFE090C0");
constexpr auto kSupportedDiscSha1 = "38C63DB6C49A06B91D87CD5A4E31B1EC68FE212C";
using HashProgress = std::function<void(int)>;

struct CloseDiscHandle {
  void operator()(void* handle) const;
};

struct VerifiedDisc {
  std::filesystem::path image;
  std::string sha256;
  // Denies writes/deletion until extraction and output verification finish.
  std::unique_ptr<void, CloseDiscHandle> read_lock;
};

enum class SetupStage { kDisc, kCache, kExtract, kAudio, kOutputs, kReady };
struct ProgressUpdate {
  SetupStage stage;
  std::string message;
  int percent = -1;
};
using Progress = std::function<void(const ProgressUpdate&)>;

std::expected<std::filesystem::path, std::string> ValidateCue(
    const std::filesystem::path& cue);
std::expected<std::string, std::string> Sha256File(
    const std::filesystem::path& file, const HashProgress& progress = {});
std::expected<VerifiedDisc, std::string> VerifyDisc(
    const std::filesystem::path& cue, const HashProgress& progress = {});
std::filesystem::path FindFfmpeg();
std::expected<Settings, std::string> LoadSettings(
    const std::filesystem::path& state);
Result SaveSettings(const std::filesystem::path& state,
                    const Settings& settings);
Result CheckInstallation(const std::filesystem::path& installation);
Result CheckAssets(const std::filesystem::path& data,
                   const std::filesystem::path& image);
std::expected<std::filesystem::path, std::string> Prepare(
    const Settings& settings, const std::filesystem::path& installation,
    const std::filesystem::path& state, const Progress& progress);
std::vector<std::string> GameArguments(
    const std::filesystem::path& installation,
    const std::filesystem::path& data, const Settings& settings);

}  // namespace VS::Launcher
