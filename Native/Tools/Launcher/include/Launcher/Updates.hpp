#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "Launcher/Setup.hpp"

namespace VS::Launcher {

inline constexpr auto kUpdateArchive = "update.zip";
inline constexpr auto kUpdateStagingDirectory = "update-staging";
inline constexpr auto kUpdateBackupDirectory = "update-backup";
inline constexpr auto kUpdateDiscardDirectory = "update-discard";
// Inside the backup directory. Present only while files are being swapped.
inline constexpr auto kUpdateMarker = "update-applying.json";

struct LatestRelease {
  std::string version;
  std::string url;
  // Empty unless the release has a package that can be verified and installed.
  std::string asset_url;
  std::uint64_t asset_bytes = 0;
  std::string asset_sha256;
};

enum class UpdateStatus { kCurrent, kAvailable, kUnknownInstalled };
using UpdateProgress = std::function<void(const std::string&, int)>;

// The packaged version from release.json; empty for a development build.
std::string InstalledVersion(const std::filesystem::path& installation);
std::expected<LatestRelease, std::string> ParseLatestRelease(
    std::string_view json);
UpdateStatus CompareRelease(std::string_view installed,
                            std::string_view latest);
std::expected<LatestRelease, std::string> FetchLatestRelease();

// Saves the package and checks it against the checksum GitHub published.
Result DownloadReleasePackage(const LatestRelease& release,
                              const std::filesystem::path& file,
                              const UpdateProgress& progress);
Result ExtractRelease(const std::filesystem::path& archive,
                      const std::filesystem::path& stage);
Result VerifyStagedRelease(const std::filesystem::path& stage,
                           std::string_view version);
// Swaps the staged files in, restoring every original file on failure.
Result ApplyStagedRelease(const std::filesystem::path& stage,
                          const std::filesystem::path& installation);
// Finishes the rollback of a swap that was interrupted, if there was one.
Result RecoverInterruptedUpdate(const std::filesystem::path& installation);
void CleanUpdateLeftovers(const std::filesystem::path& installation);
// Downloads, verifies and applies. The running launcher must restart after.
Result InstallRelease(const LatestRelease& release,
                      const std::filesystem::path& installation,
                      const UpdateProgress& progress);

}  // namespace VS::Launcher
