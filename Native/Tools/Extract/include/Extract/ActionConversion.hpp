#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ActionCatalog.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"

namespace VS::Extract {
inline constexpr std::size_t kActionSourceOffset = 0x3C1DC;
inline constexpr std::size_t kActionRecordBytes = 52;
inline constexpr std::size_t kActionSourceBytes = 13312;
static_assert(kActionSourceBytes == Content::kActionCount * kActionRecordBytes);
inline constexpr std::string_view kActionImportPath{
    "catalogs/battle/import/actions.json",
};

// Offline source bits the catalog does not keep: the AI flag bits and record
// bits no code reads, the reserved word, and whatever follows the name's
// terminator. The runtime never sees or reconstructs this layout.
struct ActionSourceRecord {
  std::uint8_t unused_ai_bits = 0;
  std::uint8_t reserved_d_bit0 = 0;
  std::int32_t reserved10 = 0;
  std::vector<std::byte> name_tail;
  bool operator==(const ActionSourceRecord&) const = default;
};
template <typename Record>
using PerAction = std::array<Record, Content::kActionCount>;
using ActionSourceRecords = PerAction<ActionSourceRecord>;
struct ImportedActions {
  ActionSourceRecords originals;
  Content::ActionCatalog definitions;
};
[[nodiscard]] std::expected<ImportedActions, std::string> DecodeActions(
    std::span<const std::byte> bytes);
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
ReconstructActionSource(const ImportedActions& imported);
[[nodiscard]] std::string FormatActionProvenance(
    const ImportedActions& imported, std::string_view source_sha256);
[[nodiscard]] std::expected<ActionSourceRecords, std::string>
ParseActionProvenance(std::span<const std::byte> bytes);
[[nodiscard]] std::expected<TableConversionReport, std::string>
ConvertDiscActions(const std::filesystem::path& root, bool force);
}  // namespace VS::Extract
