#include "Content/BestiaryCatalog.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Content/NativeText.hpp"
#include "Content/TextCatalogRegistration.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
constexpr auto kClasses = std::to_array<EnumValue>({
    {0, "human"},
    {1, "beast"},
    {2, "undead"},
    {3, "phantom"},
    {4, "dragon"},
    {5, "evil"},
});
constexpr auto kUnlocks = std::to_array<EnumValue>({
    {0, "defeated-subtype-range"},
    {1, "always"},
});
enum Column : std::size_t {
  kIdentity,
  kLegacy,
  kOrder,
  kClass,
  kPreview,
  kUnlock,
  kFirstSubtype,
  kSubtypeCount,
  kName,
  kDescription
};
constexpr ColumnSpec Id(std::string_view name, std::string_view description) {
  return {.name = name,
          .type = ColumnType::kId,
          .references = {},
          .description = description};
}
constexpr ColumnSpec Number(std::string_view name, std::int64_t minimum,
                            std::int64_t maximum,
                            std::string_view description) {
  return {.name = name,
          .minimum = minimum,
          .maximum = maximum,
          .references = {},
          .description = description};
}
constexpr ColumnSpec TextReference(std::string_view name,
                                   std::string_view description) {
  return {.name = name,
          .type = ColumnType::kReference,
          .references = kNativeTextCatalogId,
          .description = description};
}
// decomp: MENU9.PRG initEnemyList (src/MENU/MENU9.PRG/238.c:1921)
// Defeat flags are actor subtypes, not MON rows or model indices:
// src/BATTLE/BATTLE.PRG/146C.c:1437 writes only subtypes below 256.
constexpr std::array kColumns{
    Id("id", "Stable encyclopedia definition identity, not a spawned enemy."),
    Number("legacy_id", -1, 77,
           "Original visible MON record index; -1 denotes a native-only entry. "
           "The 72 out-of-list records remain import provenance."),
    Number("display_order", 1, 999,
           "Unique list position; 999 is the current three-digit view policy."),
    ColumnSpec{.name = "class",
               .type = ColumnType::kEnum,
               .maximum = 5,
               .references = {},
               .description = "Displayed enemy class.",
               .enumeration = kClasses},
    Number("preview_table_index", 0, 255,
           "MENU9 preview-table compatibility index (#200/#55); not a spawn "
           "identity. Uses that table's model, equipment and palette policy."),
    ColumnSpec{.name = "unlock",
               .type = ColumnType::kEnum,
               .maximum = 1,
               .references = {},
               .description = "Any defeated actor subtype in the range, or "
                              "explicitly always visible (no progress write).",
               .enumeration = kUnlocks},
    Number("first_defeated_subtype", 0, 255,
           "First actor subtype tested in the saved defeat flags (#203)."),
    Number("defeated_subtype_count", 0, 256,
           "Number of consecutive subtypes; range end is exclusive."),
    TextReference(
        "name",
        "Native text ID for the readable name; no embedded list indent."),
    TextReference("description", "Native text ID for the description."),
};
constexpr std::array<std::string_view, 1> kKey{"id"};
constexpr TableSpec kSpec{
    .name = "bestiary",
    .description =
        "Encyclopedia definitions. Selection, highlights, unlocked "
        "state and navigation links belong to each UI session.",
    .columns = kColumns,
    .key = kKey,
};

std::expected<BestiaryCatalog, std::string> FromRows(
    std::span<const TableRow> rows) {
  if (rows.empty() || rows.size() > kBestiaryMaximumEntries) {
    return std::unexpected("bestiary: expected 1..999 visible definitions");
  }
  BestiaryCatalog catalog;
  std::set<std::int64_t> legacy, order;
  for (const auto& row : rows) {
    const auto& values = row.values;
    if ((values[kLegacy] >= 0 && !legacy.insert(values[kLegacy]).second) ||
        !order.insert(values[kOrder]).second) {
      return std::unexpected(CellError(row.source, row.line, "legacy_id",
                                       "duplicate compatibility ID or order"));
    }
    if (values[kFirstSubtype] + values[kSubtypeCount] > kBestiarySubtypeCount ||
        (values[kUnlock] == 0 && values[kSubtypeCount] == 0) ||
        (values[kUnlock] == 1 &&
         (values[kFirstSubtype] != 0 || values[kSubtypeCount] != 0))) {
      return std::unexpected(CellError(
          row.source, row.line, "unlock",
          "invalid subtype range; always requires both range fields zero"));
    }
    catalog.push_back({row.cells[kIdentity],
                       static_cast<std::int32_t>(values[kLegacy]),
                       static_cast<std::uint32_t>(values[kOrder]),
                       static_cast<BestiaryClass>(values[kClass]),
                       static_cast<std::uint32_t>(values[kPreview]),
                       static_cast<BestiaryUnlock>(values[kUnlock]),
                       static_cast<std::uint32_t>(values[kFirstSubtype]),
                       static_cast<std::uint32_t>(values[kSubtypeCount]),
                       row.cells[kName], row.cells[kDescription]});
  }
  std::ranges::sort(catalog, {}, &BestiaryDefinition::display_order);
  return catalog;
}
}  // namespace

const TableSpec& BestiaryTableSpec() { return kSpec; }
CatalogTableSource BestiaryCatalogSource() {
  return {kSpec, Platform::Utf8Path(kBestiaryPath)};
}
std::string FormatBestiary(const BestiaryCatalog& catalog) {
  std::vector<std::vector<std::string>> rows;
  for (const auto& entry : catalog) {
    rows.push_back(
        {entry.id, std::to_string(entry.legacy_id),
         std::to_string(entry.display_order),
         std::string(
             kClasses.at(static_cast<std::size_t>(entry.enemy_class)).name),
         std::to_string(entry.preview_table_index),
         std::string(kUnlocks.at(static_cast<std::size_t>(entry.unlock)).name),
         std::to_string(entry.first_defeated_subtype),
         std::to_string(entry.defeated_subtype_count), entry.name,
         entry.description});
  }
  return FormatTable(kSpec, rows);
}
std::expected<BestiaryCatalog, std::string> ParseBestiary(
    std::string_view text) {
  auto rows = ReadTable(kSpec, text, kBestiaryPath);
  if (!rows) return std::unexpected(rows.error());
  return FromRows(*rows);
}
std::expected<BestiaryCatalog, std::string> LoadBestiary(
    const CatalogDatabase& database) {
  auto rows = database.Read(kSpec);
  if (!rows) return std::unexpected(rows.error());
  return FromRows(*rows);
}
std::expected<void, std::string> ValidateBestiaryTextReferences(
    const CatalogDatabase& database, const NativeTextCatalog& text) {
  auto rows = database.Read(kSpec);
  if (!rows) return std::unexpected(rows.error());
  for (const auto& row : *rows) {
    for (const auto column : {kName, kDescription}) {
      if (auto found = text.Find(row.cells[column]); !found) {
        return std::unexpected(CellError(row.source, row.line,
                                         kColumns[column].name, found.error()));
      }
    }
  }
  return {};
}
}  // namespace VS::Content
