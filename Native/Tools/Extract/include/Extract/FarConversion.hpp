#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>

namespace VS::Extract {
struct FarConversionReport {
  std::size_t converted = 0;
  std::size_t unchanged = 0;
  std::size_t removed = 0;
};

// Images must already be converted. No writes occur until every FAR and
// paired image validates. Unchanged native edits are validated and retained.
[[nodiscard]] std::expected<FarConversionReport, std::string>
ConvertDiscBackgrounds(const std::filesystem::path& root, bool force);
}  // namespace VS::Extract
