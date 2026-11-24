#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "Content/BestiaryCatalog.hpp"
#include "Content/JsonText.hpp"

namespace VS::Extract {
inline constexpr std::string_view kBestiaryImportPath{
    "catalogs/enemies/import/bestiary.json",
};
struct ImportedBestiary {
  Content::BestiaryCatalog definitions;
  Content::JsonDocument strings;
  Content::JsonDocument provenance;
};
[[nodiscard]] std::expected<ImportedBestiary, std::string> DecodeBestiary(
    std::span<const std::byte> bytes, std::string_view hash);
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
ReconstructBestiary(const ImportedBestiary& imported);
// Reloads original import provenance, independently of authored CSV/text edits.
[[nodiscard]] std::expected<ImportedBestiary, std::string>
ParseBestiaryProvenance(std::span<const std::byte> json);
[[nodiscard]] std::expected<void, std::string> ConvertBestiary(
    const std::filesystem::path& root, bool force);
}  // namespace VS::Extract
