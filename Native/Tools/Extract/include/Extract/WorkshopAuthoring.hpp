#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/WorkshopRecipes.hpp"

namespace VS::Extract {

struct WorkshopAuthoringReport {
  std::size_t renamed = 0;
  std::size_t references = 0;
  std::size_t named_values = 0;
  std::size_t quarantined = 0;
  std::size_t written = 0;
};

// Migrates generated positional recipe keys to semantic item names, preserving
// custom IDs, labels, values and directed pairs. Existing legacy references
// normalize through the results' explicit legacy_id mapping.
[[nodiscard]] std::expected<WorkshopAuthoringReport, std::string>
UpgradeWorkshopAuthoring(const std::filesystem::path& root);

[[nodiscard]] std::string WorkshopImportPath(Content::WorkshopKind kind);
// Offline preservation export only: the game does not load import-only rows.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
EncodeWorkshopAuthoring(const std::filesystem::path& root,
                        const Content::WorkshopRecipeTable& table);

[[nodiscard]] std::expected<void, std::string> CheckWorkshopImports(
    const std::filesystem::path& root,
    const Content::ConversionManifest& manifest);

}  // namespace VS::Extract
