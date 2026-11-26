#include "Content/MaterialCatalog.hpp"

#include <format>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {

constexpr ColumnSpec Modifier(std::string_view name,
                              std::string_view description) {
  return {.name = name,
          .type = ColumnType::kInteger,
          .minimum = std::numeric_limits<std::int32_t>::min(),
          .maximum = std::numeric_limits<std::int32_t>::max(),
          .references = {},
          .description = description};
}

// decomp: vs_main_materialModifiers (src/SLUS_010.40/main.c:350)
// Field families follow MaterialModifiers in src/SLUS_010.40/modifiers.h.
// Only the workshop reads the table, and only its strength, intelligence and
// agility (the three combine functions); items carry their own class and
// affinity ratings and DP.
// decomp: MENUC.PRG combineShields (src/MENU/MENUC.PRG/168.c:5017)
constexpr std::array kColumns{
    ColumnSpec{.name = "id",
               .type = ColumnType::kId,
               .references = {},
               .description = "Stable material definition identity."},
    ColumnSpec{.name = "legacy_id",
               .minimum = 0,
               .maximum = 7,
               .references = {},
               .description =
                   "Compatibility kind for existing inventory, saves and "
                   "palettes; not a definition field-width limit."},
    Modifier("class_human", "Human class points; kept but unread."),
    Modifier("class_beast", "Beast class points; kept but unread."),
    Modifier("class_undead", "Undead class points; kept but unread."),
    Modifier("class_phantom", "Phantom class points; kept but unread."),
    Modifier("class_dragon", "Dragon class points; kept but unread."),
    Modifier("class_evil", "Evil class points; kept but unread."),
    Modifier("attack_affinity_physical",
             "Physical attack affinity points; kept but unread."),
    Modifier("attack_affinity_air",
             "Air (source wind) attack affinity points; kept but unread."),
    Modifier("attack_affinity_fire",
             "Fire attack affinity points; kept but unread."),
    Modifier("attack_affinity_earth",
             "Earth attack affinity points; kept but unread."),
    Modifier("attack_affinity_water",
             "Water attack affinity points; kept but unread."),
    Modifier("attack_affinity_light",
             "Light attack affinity points; kept but unread."),
    Modifier("attack_affinity_dark",
             "Dark attack affinity points; kept but unread."),
    Modifier("defense_affinity_physical",
             "Physical defense affinity points; kept but unread."),
    Modifier("defense_affinity_air",
             "Air (source wind) defense affinity points; kept but unread."),
    Modifier("defense_affinity_fire",
             "Fire defense affinity points; kept but unread."),
    Modifier("defense_affinity_earth",
             "Earth defense affinity points; kept but unread."),
    Modifier("defense_affinity_water",
             "Water defense affinity points; kept but unread."),
    Modifier("defense_affinity_light",
             "Light defense affinity points; kept but unread."),
    Modifier("defense_affinity_dark",
             "Dark defense affinity points; kept but unread."),
    ColumnSpec{
        .name = "max_dp",
        .minimum = 0,
        .maximum = std::numeric_limits<std::uint32_t>::max(),
        .references = {},
        .description =
            "Material base durability in internal DP units (100 units per "
            "displayed DP); kept but unread: the workshop averages the "
            "ingredients' DP instead."},
    Modifier("strength",
             "Added to a workshop result's base strength when it is made of "
             "this material."),
    Modifier("intelligence",
             "Added to a workshop result's base intelligence when it is made "
             "of this material."),
    Modifier("agility",
             "Added to a workshop result's base agility when it is made of "
             "this material."),
};
constexpr std::array<std::string_view, 1> kKey{"id"};
constexpr std::string_view kDescription{
    "Native material definitions; reserved import bytes are not editable "
    "properties. The workshop adds a result material's strength, "
    "intelligence and agility to the result's base stats. No game code reads "
    "the class, affinity or max_dp values: items carry their own ratings and "
    "DP.",
};
constexpr TableSpec kSpec = {
    .name = "materials",
    .description = kDescription,
    .columns = kColumns,
    .key = kKey,
};
constexpr std::array<std::string_view, 8> kLabels = {
    "None", "Wood", "Leather", "Bronze", "Iron", "Hagane", "Silver", "Damascus",
};

std::expected<MaterialCatalog, std::string> FromRows(
    std::span<const TableRow> rows) {
  MaterialCatalog materials;
  std::array<bool, kMaterialIds.size()> seen{};
  for (const auto& row : rows) {
    const auto slot = static_cast<std::size_t>(row.values.at(1));
    const std::string& id = row.cells.at(0);
    if (slot >= materials.size() || id != kMaterialIds.at(slot)) {
      return std::unexpected(
          CellError(row.source, row.line, "id",
                    "unsupported material identity/kind; new inventory, save "
                    "and palette kinds require #58"));
    }
    if (seen[slot]) {
      return std::unexpected(CellError(row.source, row.line, "legacy_id",
                                       "duplicate compatibility kind"));
    }
    seen[slot] = true;
    auto& material = materials[slot];
    material.id = id;
    material.legacy_id = static_cast<std::uint32_t>(slot);
    std::size_t column = 2;
    for (auto& value : material.classes) {
      value = static_cast<std::int32_t>(row.values.at(column++));
    }
    for (auto& value : material.attack_affinities) {
      value = static_cast<std::int32_t>(row.values.at(column++));
    }
    for (auto& value : material.defense_affinities) {
      value = static_cast<std::int32_t>(row.values.at(column++));
    }
    material.max_dp = static_cast<std::uint32_t>(row.values.at(column++));
    material.strength = static_cast<std::int32_t>(row.values.at(column++));
    material.intelligence = static_cast<std::int32_t>(row.values.at(column++));
    material.agility = static_cast<std::int32_t>(row.values.at(column));
  }
  for (std::size_t slot = 0; slot < seen.size(); ++slot) {
    if (!seen[slot]) {
      return std::unexpected(
          CellError(kMaterialTablePath, 1, "id",
                    std::format("missing required material reference {}",
                                kMaterialIds[slot])));
    }
  }
  return materials;
}

}  // namespace

const TableSpec& MaterialTableSpec() { return kSpec; }

CatalogTableSource MaterialCatalogSource() {
  return {kSpec, Platform::Utf8Path(kMaterialTablePath)};
}

std::string FormatMaterials(const MaterialCatalog& materials) {
  std::vector<std::vector<std::string>> rows(1);
  for (const auto& column : kColumns) {
    rows.front().emplace_back(column.name);
  }
  rows.front().emplace_back("display_name");
  for (const auto& material : materials) {
    std::vector<std::string> row{material.id,
                                 std::to_string(material.legacy_id)};
    for (const auto value : material.classes) {
      row.push_back(std::to_string(value));
    }
    for (const auto value : material.attack_affinities) {
      row.push_back(std::to_string(value));
    }
    for (const auto value : material.defense_affinities) {
      row.push_back(std::to_string(value));
    }
    row.push_back(std::to_string(material.max_dp));
    row.push_back(std::to_string(material.strength));
    row.push_back(std::to_string(material.intelligence));
    row.push_back(std::to_string(material.agility));
    row.emplace_back(kLabels.at(material.legacy_id));
    rows.push_back(std::move(row));
  }
  return FormatCsv(rows);
}

std::expected<MaterialCatalog, std::string> ParseMaterials(
    std::string_view text) {
  auto rows = ReadTable(kSpec, text, kMaterialTablePath);
  if (!rows) {
    return std::unexpected(rows.error());
  }
  return FromRows(*rows);
}

std::expected<MaterialCatalog, std::string> LoadMaterials(
    const CatalogDatabase& database) {
  auto rows = database.Read(kSpec);
  if (!rows) {
    return std::unexpected(rows.error());
  }
  return FromRows(*rows);
}

}  // namespace VS::Content
