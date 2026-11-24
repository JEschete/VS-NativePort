#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "Content/CatalogDatabase.hpp"

namespace VS::Content {

inline constexpr std::string_view kMaterialConverterName = "material-csv";
inline constexpr std::uint32_t kMaterialConverterVersion = 1;
inline constexpr std::string_view kMaterialSource = "SLUS_010.40";
inline constexpr std::string_view kMaterialOutputId = "catalog.items.materials";
inline constexpr std::string_view kMaterialTablePath{
    "catalogs/items/materials.csv",
};
inline constexpr std::string_view kMaterialSchemaPath{
    "catalogs/items/materials.schema.json",
};

// Explicit compatibility policy for existing inventory, palette and save
// material kinds. New instances/appearances require #58, not another byte slot.
inline constexpr std::array<std::string_view, 8> kMaterialIds{
    "material.none", "material.wood",   "material.leather", "material.bronze",
    "material.iron", "material.hagane", "material.silver",  "material.damascus",
};
inline constexpr std::size_t kMaterialClassCount = 6;
inline constexpr std::size_t kMaterialAffinityCount = 7;

struct MaterialDefinition {
  std::string id;
  std::uint32_t legacy_id = 0;
  std::array<std::int32_t, kMaterialClassCount> classes{};
  std::array<std::int32_t, kMaterialAffinityCount> attack_affinities{};
  std::array<std::int32_t, kMaterialAffinityCount> defense_affinities{};
  std::uint32_t max_dp = 0;
  std::int32_t strength = 0;
  std::int32_t intelligence = 0;
  std::int32_t agility = 0;

  bool operator==(const MaterialDefinition&) const = default;
};

// Indexed only through the deliberate compatibility map above, not source
// record offsets. The schema and typed loader both reject unsupported kinds.
using MaterialCatalog = std::array<MaterialDefinition, kMaterialIds.size()>;

[[nodiscard]] const TableSpec& MaterialTableSpec();
[[nodiscard]] CatalogTableSource MaterialCatalogSource();
[[nodiscard]] std::string FormatMaterials(const MaterialCatalog& materials);
[[nodiscard]] std::expected<MaterialCatalog, std::string> ParseMaterials(
    std::string_view text);
[[nodiscard]] std::expected<MaterialCatalog, std::string> LoadMaterials(
    const CatalogDatabase& database);

}  // namespace VS::Content
