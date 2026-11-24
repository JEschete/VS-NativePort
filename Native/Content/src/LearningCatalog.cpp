#include "Content/LearningCatalog.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Content/GameplayDomains.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
constexpr int kArtTiers = 4;
constexpr int kAbilityChoices = 22;
constexpr int kFirstArt = 184;
constexpr int kBareHandsArt = 220;
// Equipment kind 0 means no blade, but kill-credit category 0 is bare hands
// (including equipped category 10 modulo 10). All other names share the
// established item/menu domain rather than inventing another weapon taxonomy.
constexpr auto kLearningWeaponCategories = [] {
  auto categories = kBladeCategories;
  categories.front().name = "bare-hands";
  return categories;
}();
// decomp: artsActionIds (src/SLUS_010.40/main.c:735)
// Names verified from vs_main_actions, SLUS offset 0x3C1DC + action * 52 + 28.
constexpr std::array<EnumValue, 40> kArts{{
    {184, "whistle-sting"}, {185, "shadoweave"},     {186, "double-fang"},
    {187, "wyrm-scorn"},    {188, "rending-gale"},   {189, "vile-scar"},
    {190, "cherry-ronde"},  {191, "papillon-reel"},  {192, "sunder"},
    {193, "thunderwave"},   {194, "swallow-slash"},  {195, "advent-sign"},
    {196, "mistral-edge"},  {197, "glacial-gale"},   {198, "killer-mantis"},
    {199, "black-nebula"},  {200, "bear-claw"},      {201, "accursed-umbra"},
    {202, "iron-ripper"},   {203, "emetic-bomb"},    {204, "sirocco"},
    {205, "riskbreak"},     {206, "gravis-aether"},  {207, "trinity-pulse"},
    {208, "bonecrusher"},   {209, "quickshock"},     {210, "ignis-wheel"},
    {211, "hex-flux"},      {212, "ruination"},      {213, "scythe-wind"},
    {214, "giga-tempest"},  {215, "spiral-scourge"}, {216, "brimstone-hail"},
    {217, "heavens-scorn"}, {218, "death-wail"},     {219, "sanctus-flare"},
    {220, "lotus-palm"},    {221, "vertigo"},        {222, "vermillion-aura"},
    {223, "retribution"},
}};
constexpr std::array<EnumValue, 4> kTiers{{
    {1, "first"},
    {2, "second"},
    {3, "third"},
    {4, "fourth"},
}};
constexpr std::array<std::string_view, 22> kChoiceNames{
    "first",        "second",        "third",      "fourth",     "fifth",
    "sixth",        "seventh",       "eighth",     "ninth",      "tenth",
    "eleventh",     "twelfth",       "thirteenth", "fourteenth", "fifteenth",
    "sixteenth",    "seventeenth",   "eighteenth", "nineteenth", "twentieth",
    "twenty-first", "twenty-second",
};
constexpr ColumnSpec kId = {
    .name = "id",
    .type = ColumnType::kId,
    .references = {},
    .description = "Stable learning definition identity.",
};
constexpr ColumnSpec Quantity(std::string_view name,
                              std::string_view description) {
  return {.name = name,
          .minimum = 0,
          .maximum = std::numeric_limits<std::int32_t>::max(),
          .references = {},
          .description = description};
}
// decomp: vs_main_artsPointsRequirements (src/SLUS_010.40/main.c:722)
constexpr std::array kArtColumns{
    kId,
    ColumnSpec{.name = "weapon_category",
               .type = ColumnType::kEnum,
               .minimum = 0,
               .maximum = 9,
               .references = {},
               .description = "Kill-credit category; bare-hands is category 0, "
                              "including equipment category 10 modulo 10.",
               .enumeration = kLearningWeaponCategories},
    ColumnSpec{.name = "tier",
               .type = ColumnType::kEnum,
               .minimum = 1,
               .maximum = 4,
               .references = {},
               .description = "Next art in this category's four-step learning "
                              "sequence; learned count plus one.",
               .enumeration = kTiers},
    ColumnSpec{.name = "action",
               .type = ColumnType::kEnum,
               .minimum = 184,
               .maximum = 223,
               .references = {},
               .description =
                   "Semantic action reference; enum codes explicitly "
                   "adapt the current action table and saved flags.",
               .enumeration = kArts},
    Quantity("required_weapon_kills",
             "Cumulative qualifying kills using this weapon category, not a "
             "per-tier increment. Current persisted counter is uint16.")};
// decomp: vs_main_battleAbilitiesPointsRequirements
// (src/SLUS_010.40/main.c:751)
constexpr std::array kAbilityColumns{
    kId,
    ColumnSpec{.name = "choice_order",
               .minimum = 1,
               .maximum = kAbilityChoices,
               .references = {},
               .description =
                   "Ordinal opportunity to choose an unlearned chain "
                   "or defense ability; not a specific action ID."},
    Quantity("required_ability_points",
             "Cumulative successful offensive/defensive timing credit. Legacy "
             "save field total_kills is NOT an enemy kill count. #183 banks "
             "credit up to 65535; a qualifying death triggers learning.")};
constexpr std::array<std::string_view, 1> kKey{"id"};
constexpr TableSpec kArtSpec = {
    .name = "break-art-learning",
    .description =
        "Break Art cumulative learning thresholds; immutable "
        "definitions, separate from kills and learned-action state.",
    .columns = kArtColumns,
    .key = kKey,
};
constexpr TableSpec kAbilitySpec = {
    .name = "battle-ability-learning",
    .description =
        "Battle ability choice thresholds in unlock order. Values "
        "need not increase: the original checks only the next tier.",
    .columns = kAbilityColumns,
    .key = kKey,
};

std::expected<void, std::string> ReadArts(LearningCatalog& catalog,
                                          std::span<const TableRow> rows) {
  std::set<std::pair<int, int>> seen;
  for (const auto& row : rows) {
    const int category = static_cast<int>(row.values.at(1));
    const int tier = static_cast<int>(row.values.at(2));
    const int action = static_cast<int>(row.values.at(3));
    if (row.cells.at(0) != BreakArtLearningId(category, tier)) {
      return std::unexpected(CellError(row.source, row.line, "id",
                                       "unsupported learning identity/tier"));
    }
    if (action != BreakArtLearningAction(category, tier)) {
      return std::unexpected(CellError(row.source, row.line, "action",
                                       "action does not match category/tier"));
    }
    if (!seen.emplace(category, tier).second) {
      return std::unexpected(
          CellError(row.source, row.line, "tier", "duplicate category/tier"));
    }
    catalog.break_arts.push_back(
        {row.cells.at(0), category, tier, action,
         static_cast<std::int32_t>(row.values.back())});
  }
  if (seen.size() != kArts.size()) {
    return std::unexpected(CellError(kBreakArtLearningPath, 1, "id",
                                     "missing required Break Art reference"));
  }
  return {};
}

std::expected<LearningCatalog, std::string> FromRows(
    std::span<const TableRow> arts, std::span<const TableRow> abilities) {
  LearningCatalog catalog;
  if (auto result = ReadArts(catalog, arts); !result) {
    return std::unexpected(result.error());
  }
  std::set<int> seen;
  for (const auto& row : abilities) {
    const int order = static_cast<int>(row.values.at(1));
    if (row.cells.at(0) != AbilityLearningId(order)) {
      return std::unexpected(CellError(row.source, row.line, "id",
                                       "unsupported ability choice reference"));
    }
    if (!seen.insert(order).second) {
      return std::unexpected(CellError(row.source, row.line, "choice_order",
                                       "duplicate ability choice order"));
    }
    catalog.battle_abilities.push_back(
        {row.cells.at(0), order, static_cast<std::int32_t>(row.values.at(2))});
  }
  if (seen.size() != kAbilityChoices) {
    return std::unexpected(
        CellError(kAbilityLearningPath, 1, "id",
                  "missing required ability choice reference"));
  }
  std::ranges::sort(catalog.break_arts, {}, &BreakArtLearningDefinition::id);
  std::ranges::sort(catalog.battle_abilities, {},
                    &AbilityLearningDefinition::choice_order);
  return catalog;
}

std::string Label(std::string_view name) {
  std::string text(name);
  std::ranges::replace(text, '-', ' ');
  return text;
}
}  // namespace

int BreakArtLearningAction(int category, int tier) {
  return (category == 0 ? kBareHandsArt
                        : kFirstArt + (category - 1) * kArtTiers) +
         tier - 1;
}
std::string BreakArtLearningId(int category, int tier) {
  const int action = BreakArtLearningAction(category, tier);
  return "learning." + std::string(kArts.at(action - kFirstArt).name);
}
std::string AbilityLearningId(int choice_order) {
  return std::format("learning.{}-battle-ability-choice",
                     kChoiceNames.at(choice_order - 1));
}
const TableSpec& BreakArtLearningSpec() { return kArtSpec; }
const TableSpec& AbilityLearningSpec() { return kAbilitySpec; }
std::vector<CatalogTableSource> LearningCatalogSources() {
  return {{kArtSpec, Platform::Utf8Path(kBreakArtLearningPath)},
          {kAbilitySpec, Platform::Utf8Path(kAbilityLearningPath)}};
}

std::optional<int> LearningCatalog::BreakArtRequirement(int category,
                                                        int learned) const {
  if (category < 0 ||
      category >= static_cast<int>(kLearningWeaponCategories.size()) ||
      learned < 0 || learned >= kArtTiers) {
    return std::nullopt;
  }
  const auto found = std::ranges::find_if(break_arts, [&](const auto& art) {
    return art.weapon_category == category && art.tier == learned + 1;
  });
  return found == break_arts.end()
             ? std::nullopt
             : std::optional<int>(found->required_weapon_kills);
}
std::optional<int> LearningCatalog::AbilityRequirement(int unlocked) const {
  if (unlocked < 0 || unlocked >= kAbilityChoices) return std::nullopt;
  const auto found = std::ranges::find(
      battle_abilities, unlocked + 1, &AbilityLearningDefinition::choice_order);
  return found == battle_abilities.end()
             ? std::nullopt
             : std::optional<int>(found->required_ability_points);
}

std::string FormatBreakArtLearning(const LearningCatalog& catalog) {
  std::vector<std::vector<std::string>> rows{
      {"id", "display_name", "weapon_category", "tier", "action",
       "required_weapon_kills"}};
  for (const auto& art : catalog.break_arts) {
    const auto name = kArts.at(art.action - kFirstArt).name;
    rows.push_back(
        {art.id, Label(name),
         std::string(kLearningWeaponCategories.at(art.weapon_category).name),
         std::string(kTiers.at(art.tier - 1).name), std::string(name),
         std::to_string(art.required_weapon_kills)});
  }
  return FormatCsv(rows);
}
std::string FormatAbilityLearning(const LearningCatalog& catalog) {
  std::vector<std::vector<std::string>> rows{
      {"id", "display_name", "choice_order", "required_ability_points"}};
  for (const auto& ability : catalog.battle_abilities) {
    rows.push_back(
        {ability.id,
         std::format("Battle ability choice {}", ability.choice_order),
         std::to_string(ability.choice_order),
         std::to_string(ability.required_ability_points)});
  }
  return FormatCsv(rows);
}
std::expected<LearningCatalog, std::string> ParseLearning(
    std::string_view arts, std::string_view abilities) {
  auto art_rows = ReadTable(kArtSpec, arts, kBreakArtLearningPath);
  auto ability_rows = ReadTable(kAbilitySpec, abilities, kAbilityLearningPath);
  if (!art_rows) return std::unexpected(art_rows.error());
  if (!ability_rows) return std::unexpected(ability_rows.error());
  return FromRows(*art_rows, *ability_rows);
}
std::expected<LearningCatalog, std::string> LoadLearning(
    const CatalogDatabase& database) {
  auto arts = database.Read(kArtSpec);
  auto abilities = database.Read(kAbilitySpec);
  if (!arts) return std::unexpected(arts.error());
  if (!abilities) return std::unexpected(abilities.error());
  return FromRows(*arts, *abilities);
}
}  // namespace VS::Content
