#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace VS::Extract {

struct AssetLabelReport {
  std::size_t written = 0;
  std::size_t unchanged = 0;
  std::size_t rows = 0;
};

// A bounded English-edition name. Stops at E7, ignores layout controls and
// keeps unknown glyphs/controls as visible markers. Not a text re-encoder.
[[nodiscard]] std::expected<std::string, std::string> DecodeAssetName(
    std::span<const std::byte> bytes);

[[nodiscard]] std::expected<std::vector<std::string>, std::string>
ReadItemDisplayNames(const std::filesystem::path& root);

// Adds optional display_name metadata without reconverting assets or changing
// gameplay fields. Existing labels survive unless refresh is requested.
// Each changed file is replaced atomically; a failed run can be resumed.
// Native text/font owners and import-only schemas are never generically
// relabeled, including refresh runs (#526).
[[nodiscard]] std::expected<AssetLabelReport, std::string> LabelNativeAssets(
    const std::filesystem::path& root, bool refresh = false);

}  // namespace VS::Extract
