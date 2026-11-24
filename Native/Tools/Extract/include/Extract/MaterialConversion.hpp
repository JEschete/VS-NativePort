#pragma once

#include <array>
#include <cstddef>
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
using MaterialReserved = std::array<MaterialPadding, kImportedMaterialCount>;

struct ImportedMaterials {
  Content::MaterialCatalog definitions;
  MaterialReserved reserved{};
};

// Offline-only preservation codec. Native runtime never calls this encoder
// or constrains definition values to the original field widths.
[[nodiscard]] std::expected<ImportedMaterials, std::string> DecodeMaterials(
    std::span<const std::byte> records);
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
EncodeMaterials(const ImportedMaterials& imported);
[[nodiscard]] std::string FormatMaterialProvenance(
    const MaterialReserved& reserved, std::string_view source_sha256);
[[nodiscard]] std::expected<MaterialReserved, std::string>
ParseMaterialProvenance(std::span<const std::byte> text);

[[nodiscard]] std::expected<TableConversionReport, std::string>
ConvertDiscMaterials(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
