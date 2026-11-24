#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>

namespace VS::Extract {

struct ZoneAuthoringReport {
  std::size_t renamed_columns = 0;
  std::size_t named_values = 0;
  std::size_t written = 0;
};

// Upgrades existing native zone tables without reconverting source assets or
// replacing authored values. The converter version is published last.
[[nodiscard]] std::expected<ZoneAuthoringReport, std::string>
UpgradeZoneAuthoring(const std::filesystem::path& root);

}  // namespace VS::Extract
