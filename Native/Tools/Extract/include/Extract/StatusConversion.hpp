#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "Content/StatusCatalog.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"

namespace VS::Extract {
inline constexpr std::size_t kStatusSourceOffset = 0x3F6CC;
inline constexpr std::size_t kStatusSourceBytes = 512;
inline constexpr std::string_view kStatusImportPath{
    "catalogs/battle/import/status.json",
};

// Offline source record only. These words preserve unused duplicate timings
// and upper bytes; the runtime never sees or reconstructs this layout.
struct StatusSourceRecord {
  std::uint32_t effect_channels = 0;
  std::uint16_t duration_word = 0;
  std::uint16_t interval_word = 0;
  std::uint32_t replaces = 0;
  std::uint32_t blocked_by = 0;
  bool operator==(const StatusSourceRecord&) const = default;
};
inline constexpr std::size_t kStatusSourceCount = Content::kStatusRuleCount;
using StatusSourceRecords = std::array<StatusSourceRecord, kStatusSourceCount>;
struct ImportedStatus {
  StatusSourceRecords originals;
  Content::StatusCatalog definitions;
};
[[nodiscard]] std::expected<StatusSourceRecords, std::string>
DecodeStatusSource(std::span<const std::byte> bytes);
[[nodiscard]] std::vector<std::byte> ReconstructStatusSource(
    const StatusSourceRecords& records);
[[nodiscard]] std::expected<ImportedStatus, std::string> DecodeStatus(
    std::span<const std::byte> bytes);
[[nodiscard]] std::string FormatStatusProvenance(
    const StatusSourceRecords& records, std::string_view source_sha256);
[[nodiscard]] std::expected<StatusSourceRecords, std::string>
ParseStatusProvenance(std::span<const std::byte> bytes);
[[nodiscard]] std::expected<TableConversionReport, std::string>
ConvertDiscStatus(const std::filesystem::path& root, bool force);
}  // namespace VS::Extract
