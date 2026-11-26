#include "Content/BestiaryCatalog.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Content/EnemyPreviews.hpp"
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
    {0, "defeated-kind-range"},
    {1, "always"},
});
// The unlock's name before enemy kinds were named.
constexpr std::array<EnumValue, 1> kLegacyUnlocks{{
    {0, "defeated-subtype-range"},
}};
enum Column : std::size_t {
  kIdentity,
  kLegacy,
  kOrder,
  kClass,
  kPreview,
  kUnlock,
  kFirstKind,
  kKindCount,
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
                            std::int64_t maximum, std::string_view description,
                            std::string_view legacy_name = {}) {
  return {.name = name,
          .minimum = minimum,
          .maximum = maximum,
          .references = {},
          .description = description,
          .legacy_name = legacy_name};
}
constexpr ColumnSpec TextReference(std::string_view name,
                                   std::string_view description) {
  return {.name = name,
          .type = ColumnType::kReference,
          .references = kNativeTextCatalogId,
          .description = description};
}
// decomp: MENU9.PRG initEnemyList (src/MENU/MENU9.PRG/238.c:1921)
// Defeat flags are enemy kinds, not MON rows or model indices: a spawn copies
// its template's kind into the actor, and a defeat sets that kind's flag when
// it is below 256.
// decomp: func_80076F24 (src/BATTLE/BATTLE.PRG/146C.c:6179)
// decomp: updateEnemyKills (src/BATTLE/BATTLE.PRG/146C.c:1414)
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
    ColumnSpec{
        .name = "preview_archetype",
        .type = ColumnType::kEnum,
        .maximum = kLastEnemyPreviewCode,
        .references = {},
        .description =
            "Archetype whose model the Gazette previews, one of those MENU9's "
            "preview table loads (#200 owns previewing any archetype); not a "
            "spawn identity. Its legacy numeric alias is that table's index, "
            "which also selects the entry's palette flag.",
        .enumeration = kEnemyPreviews,
        .legacy_name = "preview_table_index"},
    ColumnSpec{.name = "unlock",
               .type = ColumnType::kEnum,
               .maximum = 1,
               .references = {},
               .description =
                   "Any defeated enemy kind in the range, or explicitly "
                   "always visible (no progress write).",
               .enumeration = kUnlocks,
               .legacy_values = kLegacyUnlocks},
    Number("first_defeated_kind", 0, 255,
           "First enemy kind tested in the saved defeat flags, as numbered by "
           "enemy-kinds legacy_id (the actor's sub_type). Only kinds below "
           "256 have a flag (#203); the range may hold kinds no zone template "
           "uses.",
           "first_defeated_subtype"),
    Number("defeated_kind_count", 0, 256,
           "Number of consecutive kinds; the range end is exclusive.",
           "defeated_subtype_count"),
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

// Unlisted codes stay numeric so parsing reports them.
std::string PreviewCell(std::uint32_t code) {
  return IsEnemyPreviewCode(code) ? std::string(EnemyPreviewArchetype(code))
                                  : std::to_string(code);
}

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
    if (values[kFirstKind] + values[kKindCount] > kBestiaryFlaggedKinds ||
        (values[kUnlock] == 0 && values[kKindCount] == 0) ||
        (values[kUnlock] == 1 &&
         (values[kFirstKind] != 0 || values[kKindCount] != 0))) {
      return std::unexpected(CellError(
          row.source, row.line, "unlock",
          "invalid kind range; always requires both range fields zero"));
    }
    catalog.push_back({row.cells[kIdentity],
                       static_cast<std::int32_t>(values[kLegacy]),
                       static_cast<std::uint32_t>(values[kOrder]),
                       static_cast<BestiaryClass>(values[kClass]),
                       static_cast<std::uint32_t>(values[kPreview]),
                       static_cast<BestiaryUnlock>(values[kUnlock]),
                       static_cast<std::uint32_t>(values[kFirstKind]),
                       static_cast<std::uint32_t>(values[kKindCount]),
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
         PreviewCell(entry.preview_table_index),
         std::string(kUnlocks.at(static_cast<std::size_t>(entry.unlock)).name),
         std::to_string(entry.first_defeated_kind),
         std::to_string(entry.defeated_kind_count), entry.name,
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
