#pragma once

#include <cstddef>
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

struct BestiaryAuthoringReport {
  std::size_t renamed_columns = 0;
  std::size_t named_values = 0;
  std::size_t written = 0;
};
// Names the authored bestiary's preview codes by archetype and refreshes its
// schema, refusing a rewrite that would change any definition. A root without
// bestiary.csv is left alone.
[[nodiscard]] std::expected<BestiaryAuthoringReport, std::string>
UpgradeBestiaryAuthoring(const std::filesystem::path& root);
}  // namespace VS::Extract
