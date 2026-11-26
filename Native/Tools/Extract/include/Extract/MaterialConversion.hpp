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

#include "Content/MaterialCatalog.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"

namespace VS::Extract {

inline constexpr std::size_t kMaterialSourceOffset = 0x3F5DC;
inline constexpr std::size_t kMaterialRecordBytes = 30;
inline constexpr std::size_t kMaterialImportBytes{
    Content::kMaterialIds.size() * kMaterialRecordBytes,
};
inline constexpr std::string_view kMaterialImportPath{
    "catalogs/items/import/materials.json",
};
using MaterialPadding = std::array<std::byte, 2>;
inline constexpr auto kImportedMaterialCount = Content::kMaterialIds.size();
inline constexpr std::size_t kMaterialClassCount = 6;
inline constexpr std::size_t kMaterialAffinityCount = 7;

// One original record's values no game code reads: the class, affinity and
// base-DP fields and the two padding bytes. Fields are native-width so a
// version 1 table's edited cells archive unchanged; only the offline encoder
// checks the original widths.
struct ArchivedMaterial {
  std::array<std::int32_t, kMaterialClassCount> classes{};
  std::array<std::int32_t, kMaterialAffinityCount> attack_affinities{};
  std::array<std::int32_t, kMaterialAffinityCount> defense_affinities{};
  std::uint32_t max_dp = 0;
  MaterialPadding padding{};

  bool operator==(const ArchivedMaterial&) const = default;
};
using ArchivedMaterials = std::array<ArchivedMaterial, kImportedMaterialCount>;

struct ImportedMaterials {
  Content::MaterialCatalog definitions;
  ArchivedMaterials archived{};
};

// Offline-only preservation codec. Native runtime never calls this encoder
// or constrains definition values to the original field widths.
[[nodiscard]] std::expected<ImportedMaterials, std::string> DecodeMaterials(
    std::span<const std::byte> records);
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
EncodeMaterials(const ImportedMaterials& imported);
[[nodiscard]] std::string FormatMaterialProvenance(
    const ArchivedMaterials& archived, std::string_view source_sha256);
[[nodiscard]] std::expected<ArchivedMaterials, std::string>
ParseMaterialProvenance(std::span<const std::byte> text);

struct MaterialAuthoringReport {
  std::size_t archived_columns = 0;
  std::size_t written = 0;
};

// Moves a version 1 table's class, affinity and max_dp columns, edits
// included, into the import provenance beside each record's padding bytes;
// other cells and labels are unchanged. Current trees are left as they are.
[[nodiscard]] std::expected<MaterialAuthoringReport, std::string>
UpgradeMaterialAuthoring(const std::filesystem::path& root);

[[nodiscard]] std::expected<TableConversionReport, std::string>
ConvertDiscMaterials(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
