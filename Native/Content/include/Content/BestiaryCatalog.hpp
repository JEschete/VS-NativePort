#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "Content/CatalogDatabase.hpp"
#include "Content/ConversionManifest.hpp"

namespace VS::Content {
inline constexpr std::string_view kBestiarySource = "SMALL/MON.BIN";
inline constexpr std::string_view kBestiaryId = "catalog.enemies.bestiary";
inline constexpr std::string_view kBestiaryConverter =
    // NOLINTNEXTLINE(whitespace/indent_namespace): wrapped constant alias.
    kBestiaryCatalogConverters.front().name;
inline constexpr std::uint32_t kBestiaryConverterVersion =
    // NOLINTNEXTLINE(whitespace/indent_namespace): wrapped constant alias.
    kBestiaryCatalogConverters.front().version;
inline constexpr std::string_view kBestiaryPath{
    "catalogs/enemies/bestiary.csv",
};
inline constexpr std::string_view kBestiarySchemaPath{
    "catalogs/enemies/bestiary.schema.json",
};
inline constexpr std::size_t kBestiaryMaximumEntries = 999;
inline constexpr std::uint32_t kBestiarySubtypeCount = 256;

enum class BestiaryClass { kHuman, kBeast, kUndead, kPhantom, kDragon, kEvil };
enum class BestiaryUnlock { kDefeatedSubtypeRange, kAlways };

struct BestiaryDefinition {
  std::string id;
  std::int32_t legacy_id = -1;
  std::uint32_t display_order = 0;
  BestiaryClass enemy_class = BestiaryClass::kHuman;
  // MENU9 preview code of the preview_archetype column (EnemyPreviews.hpp);
  // the Gazette's preview adapter, not an enemy/spawn identity.
  std::uint32_t preview_table_index = 0;
  BestiaryUnlock unlock = BestiaryUnlock::kDefeatedSubtypeRange;
  std::uint32_t first_defeated_subtype = 0;
  std::uint32_t defeated_subtype_count = 0;
  std::string name;
  std::string description;
  bool operator==(const BestiaryDefinition&) const = default;
};
using BestiaryCatalog = std::vector<BestiaryDefinition>;

[[nodiscard]] const TableSpec& BestiaryTableSpec();
[[nodiscard]] CatalogTableSource BestiaryCatalogSource();
[[nodiscard]] std::string FormatBestiary(const BestiaryCatalog& catalog);
[[nodiscard]] std::expected<BestiaryCatalog, std::string> ParseBestiary(
    std::string_view text);
[[nodiscard]] std::expected<BestiaryCatalog, std::string> LoadBestiary(
    const CatalogDatabase& database);
struct NativeTextCatalog;
[[nodiscard]] std::expected<void, std::string> ValidateBestiaryTextReferences(
    const CatalogDatabase& database, const NativeTextCatalog& text);
}  // namespace VS::Content
