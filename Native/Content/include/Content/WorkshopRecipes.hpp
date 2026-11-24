#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Content/CatalogDatabase.hpp"
#include "Content/CatalogTable.hpp"

// The workshop combination recipes (MENU\BLADE.SYD, SHIELD.SYD and ARMOR.SYD;
// ASSET_PIPELINE.md §6.1, #499) as CSV tables under catalogs\menu\. Each kind
// has three tables: its result records (combination-<kind>-results), the
// ingredient pairs that make each result (-pairs) and the material each pair
// of ingredient materials (and categories) gives (-materials). Pairs name
// results by id; each result keeps its legacy_id, the index the game's item
// records and the original's matrices use. The combination screens look
// results up in dense matrices, which DenseWorkshopTables rebuilds.
namespace VS::Content {

inline constexpr std::string_view kSydRecipeConverterName = "syd-recipe-csv";
inline constexpr std::uint32_t kSydRecipeConverterVersion = 1;
inline constexpr std::string_view kWorkshopTableDirectory = "catalogs/menu";

enum class WorkshopKind : std::uint8_t { kBlade, kShield, kArmor };
inline constexpr std::size_t kWorkshopKindCount = 3;
inline constexpr std::array kWorkshopKinds{
    WorkshopKind::kBlade,
    WorkshopKind::kShield,
    WorkshopKind::kArmor,
};

enum class WorkshopTablePart : std::uint8_t { kResults, kPairs, kMaterials };
inline constexpr std::size_t kWorkshopTablePartCount = 3;
inline constexpr std::array kWorkshopTableParts{
    WorkshopTablePart::kResults,
    WorkshopTablePart::kPairs,
    WorkshopTablePart::kMaterials,
};

// How the original indexes a kind's tables (Menu::Setup BuildCombinedBlade,
// BuildCombinedShield and BuildCombinedArmor, Combination.cpp).
struct WorkshopLayout {
  std::string_view name;    // blade, shield or armor
  std::string_view source;  // its disc file, e.g. MENU/BLADE.SYD
  // The pair matrix is pair_side x pair_side, by both ingredients' legacy
  // ids less first_pair_id.
  std::size_t pair_side = 0;
  std::uint32_t first_pair_id = 0;
  // The material table: ((first material - material_base) *
  // first_material_stride) + ((first category - category_base) *
  // first_category_stride) + ((second material - material_base) *
  // second_material_stride) + (second category - category_base).
  std::size_t material_entries = 0;
  std::uint8_t material_base = 0;
  std::uint8_t material_count = 0;
  bool by_category = false;
  std::uint8_t category_base = 0;
  std::uint8_t category_count = 0;
  std::size_t first_material_stride = 0;
  std::size_t first_category_stride = 0;
  std::size_t second_material_stride = 0;
  std::size_t record_bytes = 0;  // a SYD result record
};
[[nodiscard]] const WorkshopLayout& LayoutOf(WorkshopKind kind);

// Compatibility alias for a result's original index, not its authoring id.
[[nodiscard]] std::string LegacyWorkshopResultId(const WorkshopLayout& layout,
                                                 std::size_t legacy_id);

struct WorkshopStats {
  std::int8_t strength = 0;
  std::int8_t intelligence = 0;
  std::int8_t agility = 0;

  bool operator==(const WorkshopStats&) const = default;
};

// A combined blade's record (BLADE.SYD, _combineBlades' info).
struct BladeResult {
  std::string id;
  std::uint32_t legacy_id = 0;
  std::uint8_t model = 0;
  std::uint8_t category = 0;
  std::uint8_t damage_type = 0;
  std::uint8_t cost_type = 0;
  std::uint8_t cost = 0;
  std::uint8_t recovery_delay = 0;
  WorkshopStats stats;
  std::uint8_t range = 0;
  std::uint8_t range_y = 0;
  std::uint8_t range_z = 0;
  std::uint8_t attack_shape = 0;
  std::uint8_t attack_angle = 0;
  std::uint8_t raw_byte7 = 0;   // unnamed, kept (§4.1)
  std::uint8_t raw_byte11 = 0;  // unnamed, kept (§4.1)

  bool operator==(const BladeResult&) const = default;
};

// A combined shield's or armor piece's record (SHIELD.SYD and ARMOR.SYD,
// _armorInfo).
struct PieceResult {
  std::string id;
  std::uint32_t legacy_id = 0;
  std::uint8_t model = 0;
  std::uint8_t category = 0;
  std::uint8_t gem_slots = 0;
  WorkshopStats stats;
  std::uint8_t raw_byte7 = 0;  // unnamed, kept (§4.1)

  bool operator==(const PieceResult&) const = default;
};

// The result two ingredients make, all by result id.
struct WorkshopPair {
  std::string first;
  std::string second;
  std::string result;

  bool operator==(const WorkshopPair&) const = default;
};

// The material two ingredients' materials (and categories, for blades and
// armor) give. Material numbers follow ITEMNAME: 1 wood to 7 damascus.
struct MaterialRule {
  std::uint8_t first_material = 0;
  std::uint8_t first_category = 0;
  std::uint8_t second_material = 0;
  std::uint8_t second_category = 0;
  std::uint8_t material = 0;

  bool operator==(const MaterialRule&) const = default;
};

// One kind's tables: results sorted by legacy_id (blades for kBlade, pieces
// otherwise), pairs and materials in their tables' order.
struct WorkshopRecipeTable {
  WorkshopKind kind = WorkshopKind::kBlade;
  std::vector<BladeResult> blades;
  std::vector<PieceResult> pieces;
  std::vector<WorkshopPair> pairs;
  std::vector<MaterialRule> materials;

  bool operator==(const WorkshopRecipeTable&) const = default;
};

[[nodiscard]] const TableSpec& WorkshopTableSpec(WorkshopKind kind,
                                                 WorkshopTablePart part);
// catalogs/menu/<table>.csv
[[nodiscard]] std::string WorkshopTablePath(WorkshopKind kind,
                                            WorkshopTablePart part);
// catalogs/menu/<table>.schema.json
[[nodiscard]] std::string WorkshopSchemaPath(WorkshopKind kind,
                                             WorkshopTablePart part);

// The text of each table, by WorkshopTablePart.
using WorkshopTexts = std::array<std::string, kWorkshopTablePartCount>;

[[nodiscard]] WorkshopTexts FormatWorkshopTables(
    const WorkshopRecipeTable& table);
// The tables `texts` hold. Every reference must name a result; ingredient
// legacy ids must fit the pair matrix and results must not be legacy id 0
// (which reads as no combination); legacy ids run from 0 without gaps. Errors
// name the file, line and column.
// Original <kind>-NNN aliases resolve through legacy_id and normalize to the
// current result id. Ambiguous aliases and duplicate resolved pairs fail.
[[nodiscard]] std::expected<WorkshopRecipeTable, std::string>
ParseWorkshopTables(WorkshopKind kind, const WorkshopTexts& texts);

// The original's matrices: the result legacy id by both ingredients' legacy
// ids, and the material by both materials (and categories); 0 is none.
struct WorkshopMatrices {
  std::vector<std::uint8_t> results;
  std::vector<std::uint8_t> materials;
};
[[nodiscard]] std::expected<WorkshopMatrices, std::string> DenseWorkshopTables(
    const WorkshopRecipeTable& table);
// A material rule's index in the material matrix.
[[nodiscard]] std::size_t MaterialIndex(const WorkshopLayout& layout,
                                        const MaterialRule& rule);

struct WorkshopRecipeCatalog {
  std::array<WorkshopRecipeTable, kWorkshopKindCount> tables;

  [[nodiscard]] const WorkshopRecipeTable& Of(WorkshopKind kind) const {
    return tables.at(static_cast<std::size_t>(kind));
  }
};
// Shared ids must agree on the fields both recipe paths consume. Armor does
// not consume its imported model/gem-slot bytes; those are preserved
// separately.
[[nodiscard]] std::expected<void, std::string> CheckSharedWorkshopResults(
    const WorkshopRecipeCatalog& catalog);
// Reads and checks every kind's tables under `root`.
[[nodiscard]] std::expected<WorkshopRecipeCatalog, std::string>
LoadWorkshopRecipes(const std::filesystem::path& root);
[[nodiscard]] std::vector<CatalogTableSource> WorkshopCatalogSources();
[[nodiscard]] std::expected<WorkshopRecipeCatalog, std::string>
LoadWorkshopRecipes(const CatalogDatabase& database);

}  // namespace VS::Content
