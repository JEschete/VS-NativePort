#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"

namespace VS::Extract {

struct WorkshopAuthoringReport {
  std::size_t renamed = 0;
  std::size_t references = 0;
  std::size_t named_values = 0;
  std::size_t quarantined = 0;
  std::size_t reserved = 0;  // non-zero bytes moved out of raw.* columns
  std::size_t written = 0;
};

// A recipe table's import-only provenance (WorkshopImportPath, schema
// workshop-import.schema.json): unreferenced stock records moved out of the
// authoring tables and the non-zero bytes no reader uses.
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct WorkshopImport {
  std::map<std::uint32_t, std::vector<std::byte>> unused_results;
  std::vector<ReservedRecordByte> reserved_bytes;
};

// Migrates generated positional recipe keys to semantic item names, preserving
// custom IDs, labels, values and directed pairs. Existing legacy references
// normalize through the results' explicit legacy_id mapping. Former raw.*
// record-byte columns move to the import file and the schemas are refreshed.
[[nodiscard]] std::expected<WorkshopAuthoringReport, std::string>
UpgradeWorkshopAuthoring(const std::filesystem::path& root);

[[nodiscard]] std::string WorkshopImportPath(Content::WorkshopKind kind);
[[nodiscard]] std::string FormatWorkshopImport(Content::WorkshopKind kind,
                                               const WorkshopImport& imported);
// Offline preservation export only: the game does not load import-only rows.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
EncodeWorkshopAuthoring(const std::filesystem::path& root,
                        const Content::WorkshopRecipeTable& table);

[[nodiscard]] std::expected<void, std::string> CheckWorkshopImports(
    const std::filesystem::path& root,
    const Content::ConversionManifest& manifest);

}  // namespace VS::Extract
