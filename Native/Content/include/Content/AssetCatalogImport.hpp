#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "Content/DataStore.hpp"

// The import boundary for the asset catalog (AssetCatalog.hpp): VS.Extract
// turns the extracted manifest's sectors and the original executable's file
// tables into asset ids here, so the game never needs either.
namespace VS::Content {

// The original executable's disc path under disc\.
inline constexpr std::string_view kOriginalExecutablePath = "SLUS_010.40";

struct AssetCatalogTexts {
  std::string assets;  // catalogs\assets.json
  std::string audio;   // catalogs\audio.json
};

// Every raw manifest file becomes an asset; the audio catalog names the
// assets the executable's _musicLBAs/_musicFileSizes, _seLBAs/_seFileSizes
// and _soundLBAs/_soundFileSizes tables and _loadMenuSound address. Fails on
// a table entry with no file at its sector, in another directory, or whose
// table size is not the file's whole-sector size.
[[nodiscard]] std::expected<AssetCatalogTexts, std::string> ImportAssetCatalogs(
    std::span<const DataStore::Entry> entries,
    std::span<const std::uint8_t> executable);

// Reads root's manifest and disc\SLUS_010.40, checks the imported catalogs
// load, then replaces root\catalogs\. Returns the asset count.
[[nodiscard]] std::expected<std::size_t, std::string> WriteAssetCatalogs(
    const std::filesystem::path& root);

}  // namespace VS::Content
