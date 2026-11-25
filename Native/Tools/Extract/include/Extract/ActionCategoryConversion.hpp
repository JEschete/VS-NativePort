#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "Content/ActionCategoryCatalog.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"

namespace VS::Extract {
inline constexpr std::size_t kActionCategorySourceOffset = 0x7F984;
inline constexpr std::size_t kActionCategorySourceBytes = 96;
inline constexpr std::string_view kActionCategoryImportPath{
    "catalogs/battle/import/action-categories.json",
};

// Offline source record only: one category's byte from each of the five
// original tables. The runtime never sees or reconstructs this layout.
struct ActionCategorySourceRecord {
  std::uint8_t needs_target_selection = 0;
  std::uint8_t area_target = 0;
  std::uint8_t includes_acting_actor = 0;
  std::uint8_t targets_body_parts = 0;
  std::uint8_t target_actor_mask = 0;
  bool operator==(const ActionCategorySourceRecord&) const = default;
};
inline constexpr std::size_t kActionCategorySourceCount =
    // NOLINTNEXTLINE(whitespace/indent_namespace): wrapped constant alias.
    Content::kActionCategoryCount;
using ActionCategorySourceRecords =
    // NOLINTNEXTLINE(whitespace/indent_namespace): wrapped type alias.
    std::array<ActionCategorySourceRecord, kActionCategorySourceCount>;
struct ImportedActionCategories {
  ActionCategorySourceRecords originals;
  Content::ActionCategoryCatalog definitions;
};
[[nodiscard]] std::expected<ActionCategorySourceRecords, std::string>
DecodeActionCategorySource(std::span<const std::byte> bytes);
[[nodiscard]] std::vector<std::byte> ReconstructActionCategorySource(
    const ActionCategorySourceRecords& records);
[[nodiscard]] std::expected<ImportedActionCategories, std::string>
DecodeActionCategories(std::span<const std::byte> bytes);
[[nodiscard]] std::string FormatActionCategoryProvenance(
    const ActionCategorySourceRecords& records, std::string_view source_sha256);
[[nodiscard]] std::expected<ActionCategorySourceRecords, std::string>
ParseActionCategoryProvenance(std::span<const std::byte> bytes);
[[nodiscard]] std::expected<TableConversionReport, std::string>
ConvertDiscActionCategories(const std::filesystem::path& root, bool force);
}  // namespace VS::Extract
