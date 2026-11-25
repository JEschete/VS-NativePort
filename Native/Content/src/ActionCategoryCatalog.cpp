#include "Content/ActionCategoryCatalog.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>

#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
// Names for the sixteen four-bit compatibility indexes, from the target
// collector each rule combination selects: CollectDirectActionTargets,
// CollectGroundAreaActionTargets, CollectSourceAreaActionTargets or
// CommitEnvironmentAction. Cloudstone, treasure chest and trap are the
// original's own target-info labels for 9, 10 and 13; 14 is the one direct
// category whose mask also names objects; 15 is unreferenced.
// decomp: D_800E8184 (D_800E8184@BATTLE/BATTLE.PRG)
constexpr std::array<std::string_view, kActionCategoryCount> kNames{
    "environment",
    "direct-actor-including-user",
    "ground-area-including-user",
    "direct-actor",
    "ground-area",
    "direct-limb-including-user",
    "ground-area-limb-including-user",
    "direct-limb",
    "ground-area-limb",
    "cloudstone",
    "treasure-chest",
    "source-area-limb-including-user",
    "source-area-limb",
    "trap",
    "direct-limb-or-object",
    "unused",
};
constexpr std::array<EnumValue, 7> kTargetKinds{{
    {0x01, "leader"},
    {0x02, "neutral"},
    {0x04, "hostile"},
    {0x08, "peaceful"},
    {0x10, "object"},
    {0x20, "cloudstone"},
    {0x80, "chest"},
}};
constexpr std::array<EnumValue, 2> kSelection{{
    {0, "automatic"},
    {1, "chosen"},
}};
constexpr std::array<EnumValue, 2> kExtent{{
    {0, "single"},
    {1, "area"},
}};
constexpr std::array<EnumValue, 2> kIncludesUser{{
    {0, "no"},
    {1, "yes"},
}};
constexpr std::array<EnumValue, 2> kAim{{
    {0, "actor"},
    {1, "limb"},
}};
constexpr ColumnSpec Choice(std::string_view name, std::string_view description,
                            std::span<const EnumValue> values) {
  return {.name = name,
          .type = ColumnType::kEnum,
          .minimum = 0,
          .maximum = 1,
          .references = {},
          .description = description,
          .enumeration = values};
}
constexpr std::array kColumns{
    ColumnSpec{
        .name = "id",
        .type = ColumnType::kId,
        .references = {},
        .description = "Semantic identity.",
    },
    Choice("selection",
           "Whether the player or AI picks the target. Automatic categories "
           "resolve without a target list: an environment action, or the "
           "area around the user.",
           kSelection),
    Choice("extent",
           "A single target, or every target inside the action's area: around "
           "a chosen ground point (chosen) or around the user (automatic).",
           kExtent),
    Choice("includes_user",
           "Whether the acting actor may appear in the target list.",
           kIncludesUser),
    Choice("aim",
           "Whether each target is a whole actor or one of its limbs. Limb "
           "categories list every limb of each actor.",
           kAim),
    ColumnSpec{
        .name = "target_kinds",
        .type = ColumnType::kFlags,
        .minimum = 0,
        .maximum = UINT32_MAX,
        .references = {},
        .description =
            "Actor kinds the category may target; 'none' targets no actor. "
            "Chest also adds the room's treasure chests to the target list.",
        .enumeration = kTargetKinds,
    },
};
constexpr std::array<std::string_view, 1> kKey{"id"};
// Value indexes in a parsed row; column 0 is the id.
constexpr std::size_t kSelectionColumn = 1;
constexpr std::size_t kExtentColumn = 2;
constexpr std::size_t kIncludesUserColumn = 3;
constexpr std::size_t kAimColumn = 4;
constexpr std::size_t kTargetKindsColumn = 5;
constexpr TableSpec kSpec = {
    .name = "action-categories",
    .description =
        "How each action category selects and filters its targets; the "
        "actions table references a category by its id.",
    .columns = kColumns,
    .key = kKey,
};

std::expected<ActionCategoryCatalog, std::string> FromRows(
    std::span<const TableRow> rows) {
  ActionCategoryCatalog catalog;
  std::set<std::size_t> seen;
  for (const auto& row : rows) {
    std::size_t index = 0;
    while (index < kNames.size() &&
           ActionCategoryIdentity(index) != row.cells.front()) {
      ++index;
    }
    if (index == kNames.size() || !seen.insert(index).second) {
      return std::unexpected(CellError(row.source, row.line, "id",
                                       "unknown or duplicate action category"));
    }
    catalog.rules[index] = {
        .needs_target_selection = row.values[kSelectionColumn] != 0,
        .area_target = row.values[kExtentColumn] != 0,
        .includes_acting_actor = row.values[kIncludesUserColumn] != 0,
        .targets_body_parts = row.values[kAimColumn] != 0,
        .target_kinds =
            static_cast<std::uint32_t>(row.values[kTargetKindsColumn]),
    };
  }
  if (seen.size() != kNames.size()) {
    return std::unexpected(CellError(kActionCategoriesPath, 1, "id",
                                     "missing required action category"));
  }
  return catalog;
}

std::string_view Name(std::span<const EnumValue> values, bool set) {
  return values[set ? 1 : 0].name;
}
}  // namespace

std::span<const EnumValue> ActionTargetKinds() { return kTargetKinds; }
std::string ActionCategoryIdentity(std::size_t legacy_category) {
  return "action-category." + std::string(kNames.at(legacy_category));
}
const ActionCategoryRule* ActionCategoryCatalog::Rule(
    int legacy_category) const {
  return legacy_category >= 0 &&
                 static_cast<std::size_t>(legacy_category) < rules.size()
             ? &rules[static_cast<std::size_t>(legacy_category)]
             : nullptr;
}
const TableSpec& ActionCategoriesSpec() { return kSpec; }
std::vector<CatalogTableSource> ActionCategoryCatalogSources() {
  return {{kSpec, Platform::Utf8Path(kActionCategoriesPath)}};
}
std::string FormatActionCategories(const ActionCategoryCatalog& catalog) {
  std::vector<std::vector<std::string>> rows{{"id", "display_name", "selection",
                                              "extent", "includes_user", "aim",
                                              "target_kinds"}};
  for (std::size_t index = 0; index < catalog.rules.size(); ++index) {
    const auto& rule = catalog.rules[index];
    std::string label(kNames[index]);
    std::ranges::replace(label, '-', ' ');
    rows.push_back(
        {ActionCategoryIdentity(index), label,
         std::string(Name(kSelection, rule.needs_target_selection)),
         std::string(Name(kExtent, rule.area_target)),
         std::string(Name(kIncludesUser, rule.includes_acting_actor)),
         std::string(Name(kAim, rule.targets_body_parts)),
         FormatFlags(kTargetKinds, rule.target_kinds)});
  }
  return FormatCsv(rows);
}
std::expected<ActionCategoryCatalog, std::string> ParseActionCategoryCatalog(
    std::string_view text) {
  auto rows = ReadTable(kSpec, text, kActionCategoriesPath);
  if (!rows) return std::unexpected(rows.error());
  return FromRows(*rows);
}
std::expected<ActionCategoryCatalog, std::string> LoadActionCategoryCatalog(
    const CatalogDatabase& database) {
  auto rows = database.Read(kSpec);
  if (!rows) return std::unexpected(rows.error());
  return FromRows(*rows);
}
}  // namespace VS::Content
