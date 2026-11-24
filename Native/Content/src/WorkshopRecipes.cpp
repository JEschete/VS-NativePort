// The workshop combination recipe tables (WorkshopRecipes.hpp).
#include "Content/WorkshopRecipes.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
namespace fs = std::filesystem;

// Matrix entries are bytes; 0 reads as no combination.
constexpr std::int64_t kMaxLegacyId = 255;
constexpr std::int64_t kByteMax = 255;
constexpr std::int64_t kStatMin = -128;
constexpr std::int64_t kStatMax = 127;
constexpr std::int64_t kBladeCategoryMax = 9;
constexpr std::int64_t kDamageTypeMax = 3;
constexpr std::int64_t kCostTypeMax = 7;
constexpr std::int64_t kAttackShapeMax = 7;
constexpr std::int64_t kAttackAngleMax = 31;
constexpr std::int64_t kPieceCategoryMax = 7;
constexpr std::int64_t kGemSlotsMax = 3;
constexpr std::int64_t kFirstMaterial = 1;
constexpr std::int64_t kLastMaterial = 7;

constexpr std::array kLayouts{
    WorkshopLayout{.name = "blade",
                   .source = "MENU/BLADE.SYD",
                   .pair_side = 96,
                   .first_pair_id = 0,
                   .material_entries = 2500,
                   .material_base = 3,
                   .material_count = 5,
                   .by_category = true,
                   .category_base = 0,
                   .category_count = 10,
                   .first_material_stride = 500,
                   .first_category_stride = 50,
                   .second_material_stride = 10,
                   .record_bytes = 16},
    WorkshopLayout{.name = "shield",
                   .source = "MENU/SHIELD.SYD",
                   .pair_side = 17,
                   .first_pair_id = 0,
                   .material_entries = 64,
                   .material_base = 0,
                   .material_count = 8,
                   .by_category = false,
                   .category_base = 0,
                   .category_count = 0,
                   .first_material_stride = 8,
                   .first_category_stride = 0,
                   .second_material_stride = 1,
                   .record_bytes = 8},
    WorkshopLayout{.name = "armor",
                   .source = "MENU/ARMOR.SYD",
                   .pair_side = 65,
                   .first_pair_id = 16,
                   .material_entries = 1024,
                   .material_base = 0,
                   .material_count = 8,
                   .by_category = true,
                   .category_base = 2,
                   .category_count = 4,
                   .first_material_stride = 128,
                   .first_category_stride = 32,
                   .second_material_stride = 4,
                   .record_bytes = 8},
};
static_assert(kLayouts.size() == kWorkshopKindCount);

constexpr ColumnSpec Id(std::string_view name, std::string_view description) {
  return {.name = name,
          .type = ColumnType::kId,
          .minimum = 0,
          .maximum = 0,
          .references = {},
          .description = description};
}

constexpr ColumnSpec Integer(std::string_view name, std::int64_t minimum,
                             std::int64_t maximum,
                             std::string_view description) {
  return {.name = name,
          .type = ColumnType::kInteger,
          .minimum = minimum,
          .maximum = maximum,
          .references = {},
          .description = description};
}

constexpr ColumnSpec Reference(std::string_view name, std::string_view table,
                               std::string_view description) {
  return {.name = name,
          .type = ColumnType::kReference,
          .minimum = 0,
          .maximum = 0,
          .references = table,
          .description = description};
}

constexpr std::string_view kBladeResults = "combination-blade-results";
constexpr std::string_view kShieldResults = "combination-shield-results";
constexpr std::string_view kArmorResults = "combination-armor-results";

constexpr std::string_view kIdText = "Semantic result id; pairs name it.";
constexpr std::string_view kLegacyIdText{
    "The original record index: the combined item's sub id (and, for blades, "
    "its id; shields and armor add 126). Runs from 0 without gaps.",
};
constexpr std::string_view kModelText = "The combined item's model.";
constexpr std::string_view kStrengthText{
    "Base strength, before the material's bonus.",
};
constexpr std::string_view kIntelligenceText{
    "Base intelligence, before the material's bonus.",
};
constexpr std::string_view kAgilityText{
    "Base agility, before the material's bonus.",
};
constexpr std::string_view kRawByte7Text{
    "Record byte 7, not yet named; kept so the record rebuilds exactly.",
};

enum BladeColumn : std::uint8_t {
  kBladeId,
  kBladeLegacyId,
  kBladeModel,
  kBladeCategory,
  kBladeDamageType,
  kBladeCostType,
  kBladeCost,
  kBladeRecoveryDelay,
  kBladeStrength,
  kBladeIntelligence,
  kBladeAgility,
  kBladeRange,
  kBladeRangeY,
  kBladeRangeZ,
  kBladeAttackShape,
  kBladeAttackAngle,
  kBladeRawByte7,
  kBladeRawByte11,
};

constexpr std::array kBladeResultColumns{
    Id("id", kIdText),
    Integer("legacy_id", 0, kMaxLegacyId, kLegacyIdText),
    Integer("model", 0, kByteMax, kModelText),
    Integer("category", 0, kBladeCategoryMax,
            "The blade category (0-9); the material table is by category."),
    Integer("damage_type", 0, kDamageTypeMax, "The damage type (0-3)."),
    Integer("cost_type", 0, kCostTypeMax,
            "What the blade's attacks cost (0-7)."),
    Integer("cost", 0, kByteMax, "How much each attack costs."),
    Integer("recovery_delay", 0, kByteMax,
            "The recovery delay after an attack (record byte 6)."),
    Integer("strength", kStatMin, kStatMax, kStrengthText),
    Integer("intelligence", kStatMin, kStatMax, kIntelligenceText),
    Integer("agility", kStatMin, kStatMax, kAgilityText),
    Integer("range", 0, kByteMax, "The attack's reach."),
    Integer("range_y", 0, kByteMax, "The attack area's second extent."),
    Integer("range_z", 0, kByteMax, "The attack area's third extent."),
    Integer("attack_shape", 0, kAttackShapeMax,
            "The attack area's shape (the shape byte's low three bits)."),
    Integer("attack_angle", 0, kAttackAngleMax,
            "The attack area's angle (the shape byte's high five bits)."),
    Integer("raw.byte7", 0, kByteMax, kRawByte7Text),
    Integer("raw.byte11", 0, kByteMax,
            "Record byte 11, not yet named; kept so the record rebuilds "
            "exactly."),
};
static_assert(kBladeResultColumns.size() == kBladeRawByte11 + 1);

enum PieceColumn : std::uint8_t {
  kPieceId,
  kPieceLegacyId,
  kPieceModel,
  kPieceCategory,
  kPieceGemSlots,
  kPieceStrength,
  kPieceIntelligence,
  kPieceAgility,
  kPieceRawByte7,
};

constexpr std::array kPieceResultColumns{
    Id("id", kIdText),
    Integer("legacy_id", 0, kMaxLegacyId, kLegacyIdText),
    Integer("model", 0, kByteMax, kModelText),
    Integer("category", 0, kPieceCategoryMax,
            "The item category: shields 0-1, armor 2-5 (the armor material "
            "table is by category 2-5)."),
    Integer("gem_slots", 0, kGemSlotsMax, "How many gems the shield holds."),
    Integer("strength", kStatMin, kStatMax, kStrengthText),
    Integer("intelligence", kStatMin, kStatMax, kIntelligenceText),
    Integer("agility", kStatMin, kStatMax, kAgilityText),
    Integer("raw.byte7", 0, kByteMax, kRawByte7Text),
};
static_assert(kPieceResultColumns.size() == kPieceRawByte7 + 1);

enum PairColumn : std::uint8_t {
  kPairFirst,
  kPairSecond,
  kPairResult,
  kPairColumnCount
};

constexpr std::string_view kFirstText = "The first ingredient's result id.";
constexpr std::string_view kSecondText = "The second ingredient's result id.";
constexpr std::string_view kResultText = "The result the two make.";

constexpr std::array kBladePairColumns{
    Reference("first", kBladeResults, kFirstText),
    Reference("second", kBladeResults, kSecondText),
    Reference("result", kBladeResults, kResultText),
};
constexpr std::array kShieldPairColumns{
    Reference("first", kShieldResults, kFirstText),
    Reference("second", kShieldResults, kSecondText),
    Reference("result", kShieldResults, kResultText),
};
constexpr std::array kArmorPairColumns{
    Reference("first", kArmorResults, kFirstText),
    Reference("second", kArmorResults, kSecondText),
    Reference("result", kArmorResults, kResultText),
};

enum MaterialColumn : std::uint8_t {
  kFirstMaterialColumn,
  kFirstCategoryColumn,
  kSecondMaterialColumn,
  kSecondCategoryColumn,
  kMaterialColumn,
};

constexpr std::string_view kFirstMaterialText{
    "The first ingredient's material (1 wood, 2 leather, 3 bronze, 4 iron, 5 "
    "hagane, 6 silver, 7 damascus).",
};
constexpr std::string_view kSecondMaterialText{
    "The second ingredient's material.",
};
constexpr std::string_view kFirstCategoryText{
    "The first ingredient's category.",
};
constexpr std::string_view kSecondCategoryText{
    "The second ingredient's category.",
};
constexpr std::string_view kMaterialText = "The combined item's material.";

constexpr std::array kBladeMaterialColumns{
    Integer("first_material", 3, kLastMaterial, kFirstMaterialText),
    Integer("first_category", 0, kBladeCategoryMax, kFirstCategoryText),
    Integer("second_material", 3, kLastMaterial, kSecondMaterialText),
    Integer("second_category", 0, kBladeCategoryMax, kSecondCategoryText),
    Integer("material", kFirstMaterial, kLastMaterial, kMaterialText),
};
constexpr std::array kShieldMaterialColumns{
    Integer("first_material", 0, kLastMaterial, kFirstMaterialText),
    Integer("second_material", 0, kLastMaterial, kSecondMaterialText),
    Integer("material", kFirstMaterial, kLastMaterial, kMaterialText),
};
constexpr std::array kArmorMaterialColumns{
    Integer("first_material", 0, kLastMaterial, kFirstMaterialText),
    Integer("first_category", 2, 5, kFirstCategoryText),
    Integer("second_material", 0, kLastMaterial, kSecondMaterialText),
    Integer("second_category", 2, 5, kSecondCategoryText),
    Integer("material", kFirstMaterial, kLastMaterial, kMaterialText),
};

constexpr std::array<std::string_view, 1> kResultKey{"id"};
constexpr std::array<std::string_view, 2> kPairKey{"first", "second"};
constexpr std::array<std::string_view, 4> kCategoryMaterialKey{
    "first_material",
    "first_category",
    "second_material",
    "second_category",
};
constexpr std::array<std::string_view, 2> kMaterialKey{
    "first_material",
    "second_material",
};

constexpr std::string_view kResultsText{
    "Workshop combination results: the item each combination makes.",
};
constexpr std::string_view kPairsText{
    "Workshop combinations: the result two ingredients make. Pairs not "
    "listed do not combine.",
};
constexpr std::string_view kMaterialsText{
    "Workshop combinations: the material two ingredients' materials give. "
    "Pairs not listed give no material.",
};

using KindSpecs = std::array<TableSpec, kWorkshopTablePartCount>;
constexpr std::array<KindSpecs, kWorkshopKindCount> kSpecs{{
    {{
        {.name = kBladeResults,
         .description = kResultsText,
         .columns = kBladeResultColumns,
         .key = kResultKey,
         .legacy_alias_prefix = kLayouts[0].name},
        {.name = "combination-blade-pairs",
         .description = kPairsText,
         .columns = kBladePairColumns,
         .key = kPairKey},
        {.name = "combination-blade-materials",
         .description = kMaterialsText,
         .columns = kBladeMaterialColumns,
         .key = kCategoryMaterialKey},
    }},
    {{
        {.name = kShieldResults,
         .description = kResultsText,
         .columns = kPieceResultColumns,
         .key = kResultKey,
         .legacy_alias_prefix = kLayouts[1].name},
        {.name = "combination-shield-pairs",
         .description = kPairsText,
         .columns = kShieldPairColumns,
         .key = kPairKey},
        {.name = "combination-shield-materials",
         .description = kMaterialsText,
         .columns = kShieldMaterialColumns,
         .key = kMaterialKey},
    }},
    {{
        {.name = kArmorResults,
         .description = kResultsText,
         .columns = kPieceResultColumns,
         .key = kResultKey,
         .legacy_alias_prefix = kLayouts[2].name},
        {.name = "combination-armor-pairs",
         .description = kPairsText,
         .columns = kArmorPairColumns,
         .key = kPairKey},
        {.name = "combination-armor-materials",
         .description = kMaterialsText,
         .columns = kArmorMaterialColumns,
         .key = kCategoryMaterialKey},
    }},
}};

std::size_t IndexOf(WorkshopKind kind) {
  return static_cast<std::size_t>(kind);
}
std::size_t IndexOf(WorkshopTablePart part) {
  return static_cast<std::size_t>(part);
}

template <typename Value>
std::string Text(Value value) {
  return std::to_string(static_cast<int>(value));
}

std::vector<std::string> RowOf(const BladeResult& blade) {
  return {blade.id,
          std::to_string(blade.legacy_id),
          Text(blade.model),
          Text(blade.category),
          Text(blade.damage_type),
          Text(blade.cost_type),
          Text(blade.cost),
          Text(blade.recovery_delay),
          Text(blade.stats.strength),
          Text(blade.stats.intelligence),
          Text(blade.stats.agility),
          Text(blade.range),
          Text(blade.range_y),
          Text(blade.range_z),
          Text(blade.attack_shape),
          Text(blade.attack_angle),
          Text(blade.raw_byte7),
          Text(blade.raw_byte11)};
}

std::vector<std::string> RowOf(const PieceResult& piece) {
  return {piece.id,
          std::to_string(piece.legacy_id),
          Text(piece.model),
          Text(piece.category),
          Text(piece.gem_slots),
          Text(piece.stats.strength),
          Text(piece.stats.intelligence),
          Text(piece.stats.agility),
          Text(piece.raw_byte7)};
}

std::vector<std::string> RowOf(const MaterialRule& rule, bool by_category) {
  if (!by_category) {
    return {Text(rule.first_material), Text(rule.second_material),
            Text(rule.material)};
  }
  return {Text(rule.first_material), Text(rule.first_category),
          Text(rule.second_material), Text(rule.second_category),
          Text(rule.material)};
}

std::uint8_t Byte(const TableRow& row, std::size_t column) {
  return static_cast<std::uint8_t>(row.values[column]);
}

std::int8_t Signed(const TableRow& row, std::size_t column) {
  return static_cast<std::int8_t>(row.values[column]);
}

BladeResult BladeOf(const TableRow& row) {
  return {.id = row.cells[kBladeId],
          .legacy_id = static_cast<std::uint32_t>(row.values[kBladeLegacyId]),
          .model = Byte(row, kBladeModel),
          .category = Byte(row, kBladeCategory),
          .damage_type = Byte(row, kBladeDamageType),
          .cost_type = Byte(row, kBladeCostType),
          .cost = Byte(row, kBladeCost),
          .recovery_delay = Byte(row, kBladeRecoveryDelay),
          .stats = {.strength = Signed(row, kBladeStrength),
                    .intelligence = Signed(row, kBladeIntelligence),
                    .agility = Signed(row, kBladeAgility)},
          .range = Byte(row, kBladeRange),
          .range_y = Byte(row, kBladeRangeY),
          .range_z = Byte(row, kBladeRangeZ),
          .attack_shape = Byte(row, kBladeAttackShape),
          .attack_angle = Byte(row, kBladeAttackAngle),
          .raw_byte7 = Byte(row, kBladeRawByte7),
          .raw_byte11 = Byte(row, kBladeRawByte11)};
}

PieceResult PieceOf(const TableRow& row) {
  return {.id = row.cells[kPieceId],
          .legacy_id = static_cast<std::uint32_t>(row.values[kPieceLegacyId]),
          .model = Byte(row, kPieceModel),
          .category = Byte(row, kPieceCategory),
          .gem_slots = Byte(row, kPieceGemSlots),
          .stats = {.strength = Signed(row, kPieceStrength),
                    .intelligence = Signed(row, kPieceIntelligence),
                    .agility = Signed(row, kPieceAgility)},
          .raw_byte7 = Byte(row, kPieceRawByte7)};
}

MaterialRule RuleOf(const TableRow& row, bool by_category) {
  if (!by_category) {
    return {.first_material = Byte(row, 0),
            .first_category = 0,
            .second_material = Byte(row, 1),
            .second_category = 0,
            .material = Byte(row, 2)};
  }
  return {.first_material = Byte(row, kFirstMaterialColumn),
          .first_category = Byte(row, kFirstCategoryColumn),
          .second_material = Byte(row, kSecondMaterialColumn),
          .second_category = Byte(row, kSecondCategoryColumn),
          .material = Byte(row, kMaterialColumn)};
}

// Each result's legacy id by its id.
using ResultIds = std::unordered_map<std::string, std::uint32_t>;

// Records each result row's legacy id, which must be unique and, over the
// table, run from 0 without gaps.
std::expected<ResultIds, std::string> ResultIdsOf(
    const WorkshopLayout& layout, const std::vector<TableRow>& rows,
    std::size_t legacy_column, std::string_view file) {
  ResultIds ids;
  std::unordered_map<std::uint32_t, std::size_t> lines;
  for (const TableRow& row : rows) {
    const auto legacy = static_cast<std::uint32_t>(row.values[legacy_column]);
    const auto [found, added] = lines.emplace(legacy, row.line);
    if (!added) {
      return std::unexpected(CellError(
          file, row.line, "legacy_id",
          std::format("{} repeats line {}'s", legacy, found->second)));
    }
    ids.emplace(row.cells[0], legacy);
  }
  for (std::uint32_t legacy = 0; legacy < rows.size(); ++legacy) {
    if (!lines.contains(legacy)) {
      return std::unexpected(std::format(
          "{}: no row has legacy_id {}; legacy ids run from 0 without gaps",
          file, legacy));
    }
  }
  for (std::uint32_t legacy = 0; legacy < rows.size(); ++legacy) {
    const std::string alias = LegacyWorkshopResultId(layout, legacy);
    const auto [found, added] = ids.emplace(alias, legacy);
    if (!added && found->second != legacy) {
      return std::unexpected(
          std::format("{}: legacy alias '{}' conflicts with another result id",
                      file, alias));
    }
  }
  return ids;
}

// The legacy id `row`'s reference in `column` names.
std::expected<std::uint32_t, std::string> Resolve(const ResultIds& ids,
                                                  const TableSpec& spec,
                                                  const TableRow& row,
                                                  std::size_t column,
                                                  std::string_view file) {
  const std::string& id = row.cells[column];
  const auto found = ids.find(id);
  if (found == ids.end()) {
    return std::unexpected(
        CellError(file, row.line, spec.columns[column].name,
                  std::format("'{}' names no row of {}", id,
                              spec.columns[column].references)));
  }
  return found->second;
}

// Why `legacy` cannot be an ingredient of `layout`'s pair matrix, if it
// cannot.
std::optional<std::string> IngredientProblem(const WorkshopLayout& layout,
                                             std::string_view id,
                                             std::uint32_t legacy) {
  if (legacy >= layout.first_pair_id &&
      legacy - layout.first_pair_id < layout.pair_side) {
    return std::nullopt;
  }
  return std::format(
      "'{}' (legacy_id {}) is outside the ingredients' legacy ids {}..{}", id,
      legacy, layout.first_pair_id,
      layout.first_pair_id + layout.pair_side - 1);
}

std::expected<std::vector<WorkshopPair>, std::string> PairsOf(
    const WorkshopLayout& layout, const TableSpec& spec,
    const std::vector<TableRow>& rows, const ResultIds& ids,
    const std::vector<TableRow>& results, std::string_view file) {
  std::vector<WorkshopPair> pairs;
  pairs.reserve(rows.size());
  std::set<std::pair<std::uint32_t, std::uint32_t>> ingredients;
  for (const TableRow& row : rows) {
    std::array<std::uint32_t, kPairColumnCount> resolved{};
    for (const std::size_t column : {kPairFirst, kPairSecond, kPairResult}) {
      auto legacy = Resolve(ids, spec, row, column, file);
      if (!legacy) return std::unexpected(legacy.error());
      resolved.at(column) = *legacy;
      if (column != kPairResult) {
        if (auto problem =
                IngredientProblem(layout, row.cells[column], *legacy)) {
          return std::unexpected(
              CellError(file, row.line, spec.columns[column].name, *problem));
        }
      }
    }
    if (resolved[kPairResult] == 0) {
      return std::unexpected(CellError(
          file, row.line, "result",
          std::format("'{}' has legacy_id 0, which reads as no combination",
                      row.cells[kPairResult])));
    }
    if (!ingredients.emplace(resolved[kPairFirst], resolved[kPairSecond])
             .second) {
      return std::unexpected(
          CellError(file, row.line, "first",
                    "duplicate ingredients after legacy alias resolution"));
    }
    pairs.push_back({.first = results.at(resolved[kPairFirst]).cells.at(0),
                     .second = results.at(resolved[kPairSecond]).cells.at(0),
                     .result = results.at(resolved[kPairResult]).cells.at(0)});
  }
  return pairs;
}

std::expected<std::string, std::string> ReadText(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::unexpected(
        std::format("cannot open {}", Platform::PathText(path)));
  }
  std::string text{std::istreambuf_iterator<char>(input),
                   std::istreambuf_iterator<char>()};
  if (input.bad()) {
    return std::unexpected(
        std::format("cannot read {}", Platform::PathText(path)));
  }
  return text;
}

// The legacy ids `table`'s results hold, which must run from 0 in order.
std::expected<ResultIds, std::string> DenseIds(
    const WorkshopRecipeTable& table) {
  ResultIds ids;
  const auto add = [&ids](
                       const std::string& id, std::uint32_t legacy,
                       std::size_t index) -> std::expected<void, std::string> {
    if (legacy != index) {
      return std::unexpected(std::format(
          "result {} has legacy_id {} at index {}; results run from 0 in "
          "order",
          id, legacy, index));
    }
    if (legacy > kMaxLegacyId || !ids.emplace(id, legacy).second) {
      return std::unexpected(std::format(
          "result {} repeats an id or exceeds legacy id {}", id, kMaxLegacyId));
    }
    return {};
  };
  for (std::size_t index = 0; index < table.blades.size(); ++index) {
    const BladeResult& blade = table.blades[index];
    if (auto added = add(blade.id, blade.legacy_id, index); !added) {
      return std::unexpected(added.error());
    }
  }
  for (std::size_t index = 0; index < table.pieces.size(); ++index) {
    const PieceResult& piece = table.pieces[index];
    if (auto added = add(piece.id, piece.legacy_id, index); !added) {
      return std::unexpected(added.error());
    }
  }
  return ids;
}

std::expected<std::uint32_t, std::string> DenseResolve(const ResultIds& ids,
                                                       const std::string& id) {
  const auto found = ids.find(id);
  if (found == ids.end()) {
    return std::unexpected(std::format("pair names no result '{}'", id));
  }
  return found->second;
}

std::expected<std::vector<std::uint8_t>, std::string> DenseResults(
    const WorkshopRecipeTable& table, const ResultIds& ids) {
  const WorkshopLayout& layout = LayoutOf(table.kind);
  std::vector<std::uint8_t> results(layout.pair_side * layout.pair_side);
  for (const WorkshopPair& pair : table.pairs) {
    auto first = DenseResolve(ids, pair.first);
    if (!first) return std::unexpected(first.error());
    auto second = DenseResolve(ids, pair.second);
    if (!second) return std::unexpected(second.error());
    auto result = DenseResolve(ids, pair.result);
    if (!result) return std::unexpected(result.error());
    for (const auto& [id, legacy] :
         {std::pair{&pair.first, *first}, std::pair{&pair.second, *second}}) {
      if (auto problem = IngredientProblem(layout, *id, legacy)) {
        return std::unexpected(*problem);
      }
    }
    const std::size_t index =
        ((*first - layout.first_pair_id) * layout.pair_side) +
        (*second - layout.first_pair_id);
    if (*result == 0 || results[index] != 0) {
      return std::unexpected(
          std::format("pair ({}, {}) repeats or makes legacy id 0", pair.first,
                      pair.second));
    }
    results[index] = static_cast<std::uint8_t>(*result);
  }
  return results;
}

bool InRange(std::uint8_t value, std::uint8_t base, std::uint8_t count) {
  return value >= base && value - base < count;
}

std::expected<std::vector<std::uint8_t>, std::string> DenseMaterials(
    const WorkshopRecipeTable& table) {
  const WorkshopLayout& layout = LayoutOf(table.kind);
  std::vector<std::uint8_t> materials(layout.material_entries);
  for (const MaterialRule& rule : table.materials) {
    const bool valid = InRange(rule.first_material, layout.material_base,
                               layout.material_count) &&
                       InRange(rule.second_material, layout.material_base,
                               layout.material_count) &&
                       (!layout.by_category ||
                        (InRange(rule.first_category, layout.category_base,
                                 layout.category_count) &&
                         InRange(rule.second_category, layout.category_base,
                                 layout.category_count))) &&
                       rule.material >= kFirstMaterial &&
                       rule.material <= kLastMaterial;
    const std::size_t index = valid ? MaterialIndex(layout, rule) : 0;
    if (!valid || materials[index] != 0) {
      return std::unexpected(std::format(
          "material rule ({}, {}, {}, {}) is out of range or repeats",
          rule.first_material, rule.first_category, rule.second_material,
          rule.second_category));
    }
    materials[index] = rule.material;
  }
  return materials;
}

}  // namespace

const WorkshopLayout& LayoutOf(WorkshopKind kind) {
  return kLayouts.at(IndexOf(kind));
}

const TableSpec& WorkshopTableSpec(WorkshopKind kind, WorkshopTablePart part) {
  return kSpecs.at(IndexOf(kind)).at(IndexOf(part));
}

std::string LegacyWorkshopResultId(const WorkshopLayout& layout,
                                   std::size_t legacy_id) {
  return std::format("{}-{:03}", layout.name, legacy_id);
}

std::string WorkshopTablePath(WorkshopKind kind, WorkshopTablePart part) {
  return std::format("{}/{}{}", kWorkshopTableDirectory,
                     WorkshopTableSpec(kind, part).name, kCsvSuffix);
}

std::string WorkshopSchemaPath(WorkshopKind kind, WorkshopTablePart part) {
  return std::format("{}/{}{}", kWorkshopTableDirectory,
                     WorkshopTableSpec(kind, part).name, kTableSchemaSuffix);
}

WorkshopTexts FormatWorkshopTables(const WorkshopRecipeTable& table) {
  const WorkshopLayout& layout = LayoutOf(table.kind);
  std::vector<std::vector<std::string>> results;
  for (const BladeResult& blade : table.blades) results.push_back(RowOf(blade));
  for (const PieceResult& piece : table.pieces) results.push_back(RowOf(piece));
  std::vector<std::vector<std::string>> pairs;
  pairs.reserve(table.pairs.size());
  for (const WorkshopPair& pair : table.pairs) {
    pairs.push_back({pair.first, pair.second, pair.result});
  }
  std::vector<std::vector<std::string>> materials;
  materials.reserve(table.materials.size());
  for (const MaterialRule& rule : table.materials) {
    materials.push_back(RowOf(rule, layout.by_category));
  }
  return {
      FormatTable(WorkshopTableSpec(table.kind, WorkshopTablePart::kResults),
                  results),
      FormatTable(WorkshopTableSpec(table.kind, WorkshopTablePart::kPairs),
                  pairs),
      FormatTable(WorkshopTableSpec(table.kind, WorkshopTablePart::kMaterials),
                  materials)};
}

std::expected<WorkshopRecipeTable, std::string> ParseWorkshopTables(
    WorkshopKind kind, const WorkshopTexts& texts) {
  const WorkshopLayout& layout = LayoutOf(kind);
  std::array<std::vector<TableRow>, kWorkshopTablePartCount> rows;
  for (const WorkshopTablePart part : kWorkshopTableParts) {
    auto read =
        ReadTable(WorkshopTableSpec(kind, part), texts.at(IndexOf(part)),
                  WorkshopTablePath(kind, part));
    if (!read) return std::unexpected(read.error());
    rows.at(IndexOf(part)) = std::move(*read);
  }
  std::vector<TableRow>& results = rows[IndexOf(WorkshopTablePart::kResults)];
  const std::size_t legacy_column = kind == WorkshopKind::kBlade
                                        ? std::size_t{kBladeLegacyId}
                                        : std::size_t{kPieceLegacyId};
  std::ranges::sort(results, {}, [legacy_column](const TableRow& row) {
    return row.values[legacy_column];
  });
  auto ids = ResultIdsOf(layout, results, legacy_column,
                         WorkshopTablePath(kind, WorkshopTablePart::kResults));
  if (!ids) return std::unexpected(ids.error());
  WorkshopRecipeTable table{};
  table.kind = kind;
  for (const TableRow& row : results) {
    if (kind == WorkshopKind::kBlade) {
      table.blades.push_back(BladeOf(row));
    } else {
      table.pieces.push_back(PieceOf(row));
    }
  }
  auto pairs =
      PairsOf(layout, WorkshopTableSpec(kind, WorkshopTablePart::kPairs),
              rows[IndexOf(WorkshopTablePart::kPairs)], *ids, results,
              WorkshopTablePath(kind, WorkshopTablePart::kPairs));
  if (!pairs) return std::unexpected(pairs.error());
  table.pairs = std::move(*pairs);
  for (const TableRow& row : rows[IndexOf(WorkshopTablePart::kMaterials)]) {
    table.materials.push_back(RuleOf(row, layout.by_category));
  }
  return table;
}

std::size_t MaterialIndex(const WorkshopLayout& layout,
                          const MaterialRule& rule) {
  std::size_t index =
      (static_cast<std::size_t>(rule.first_material - layout.material_base) *
       layout.first_material_stride) +
      (static_cast<std::size_t>(rule.second_material - layout.material_base) *
       layout.second_material_stride);
  if (layout.by_category) {
    index +=
        (static_cast<std::size_t>(rule.first_category - layout.category_base) *
         layout.first_category_stride) +
        static_cast<std::size_t>(rule.second_category - layout.category_base);
  }
  return index;
}

std::expected<WorkshopMatrices, std::string> DenseWorkshopTables(
    const WorkshopRecipeTable& table) {
  const bool blades = table.kind == WorkshopKind::kBlade;
  if ((blades && !table.pieces.empty()) || (!blades && !table.blades.empty())) {
    return std::unexpected(std::format("{} tables hold another kind's results",
                                       LayoutOf(table.kind).name));
  }
  auto ids = DenseIds(table);
  if (!ids) return std::unexpected(ids.error());
  auto results = DenseResults(table, *ids);
  if (!results) return std::unexpected(results.error());
  auto materials = DenseMaterials(table);
  if (!materials) return std::unexpected(materials.error());
  return WorkshopMatrices{.results = std::move(*results),
                          .materials = std::move(*materials)};
}

std::expected<void, std::string> CheckSharedWorkshopResults(
    const WorkshopRecipeCatalog& catalog) {
  std::unordered_map<std::string, const PieceResult*> definitions;
  for (const auto& table : catalog.tables) {
    for (const auto& piece : table.pieces) {
      const auto [found, added] = definitions.emplace(piece.id, &piece);
      if (!added && (found->second->legacy_id != piece.legacy_id ||
                     found->second->category != piece.category ||
                     found->second->stats != piece.stats)) {
        return std::unexpected(std::format(
            "shared result '{}' differs between recipe tables; reconcile "
            "the definition or assign an explicit variant id",
            piece.id));
      }
    }
  }
  return {};
}

std::expected<WorkshopRecipeCatalog, std::string> LoadWorkshopRecipes(
    const fs::path& root) {
  WorkshopRecipeCatalog catalog;
  for (const WorkshopKind kind : kWorkshopKinds) {
    WorkshopTexts texts;
    for (const WorkshopTablePart part : kWorkshopTableParts) {
      auto text =
          ReadText(root / Platform::Utf8Path(WorkshopTablePath(kind, part)));
      if (!text) return std::unexpected(text.error());
      texts.at(IndexOf(part)) = std::move(*text);
    }
    auto table = ParseWorkshopTables(kind, texts);
    if (!table) return std::unexpected(table.error());
    catalog.tables.at(IndexOf(kind)) = std::move(*table);
  }
  if (auto shared = CheckSharedWorkshopResults(catalog); !shared) {
    return std::unexpected(shared.error());
  }
  return catalog;
}

}  // namespace VS::Content
