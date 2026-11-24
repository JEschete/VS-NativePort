#include "Content/StatusCatalog.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <set>
#include <string>
#include <vector>

#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
// decomp: vs_main_statusEffectParams (src/SLUS_010.40/main.c:524)
// Bits 29..31 are verified through analyze application, instant-defeat
// prerequisites/removal, and spawn initialization plus spell hit/dispatch.
constexpr std::array<std::string_view, kStatusRuleCount> kNames{
    "head-impaired",
    "right-arm-impaired",
    "left-arm-impaired",
    "body-impaired",
    "legs-impaired",
    "strength-down",
    "strength-up",
    "intelligence-down",
    "intelligence-up",
    "agility-down",
    "agility-up",
    "quicken",
    "silence",
    "paralysis",
    "poison",
    "numbness",
    "curse",
    "regeneration",
    "magic-ward",
    "equipment-down",
    "equipment-up",
    "air-attack-up",
    "fire-attack-up",
    "earth-attack-up",
    "water-attack-up",
    "air-defense-up",
    "fire-defense-up",
    "earth-defense-up",
    "water-defense-up",
    "analyze",
    "instant-defeat",
    "spell-immunity",
};
constexpr auto kFlags = [] {
  std::array<EnumValue, kStatusRuleCount> values{};
  for (std::size_t bit = 0; bit < values.size(); ++bit) {
    values[bit] = {std::int64_t{1} << bit, kNames[bit]};
  }
  return values;
}();
constexpr std::array<std::string_view, kStatusTimingCount> kTimingNames{
    "timing.attribute-modifiers",    "timing.movement-modifiers",
    "timing.equipment-and-elements", "timing.paralysis-and-numbness",
    "timing.poison-pulse",           "timing.regeneration-pulse",
};
constexpr ColumnSpec kId = {
    .name = "id",
    .type = ColumnType::kId,
    .references = {},
    .description = "Semantic identity.",
};
constexpr ColumnSpec Flags(std::string_view name,
                           std::string_view description) {
  return {.name = name,
          .type = ColumnType::kFlags,
          .minimum = 1,
          .maximum = UINT32_MAX,
          .references = {},
          .description = description,
          .enumeration = kFlags};
}
constexpr std::array kRuleColumns{
    kId,
    Flags("effect_channels",
          "Channels to apply/remove and test against target resistance. "
          "Instant-defeat is a prerequisite/removal channel, not a timed "
          "ailment. Spell-immunity is also initialized from enemy resistance."),
    Flags("replaces",
          "Active flags to remove, also permitting reapplication of those "
          "flags. Evaluated even if resistance prevents the new flag."),
    Flags("blocked_by",
          "Any listed active flag prevents both application and replacement.")};
constexpr std::array kTimingColumns{
    kId,
    ColumnSpec{
        .name = "status_updates",
        .minimum = 0,
        .maximum = UINT8_MAX,
        .references = {},
        .description =
            "Eligible status updates, once per vitals counter multiple "
            "of 60 while the actor is active and alive. Shared timer "
            "groups are engine contracts, not independent per-rule "
            "durations. Paralysis and numbness use separate actor "
            "timers with the same authored duration. Poison and "
            "regeneration reset the same HP pulse timer; AI uses "
            "these same intervals. Zero duration disables expiry; "
            "HP pulse intervals must be positive. Actor/save capacity 255.",
    },
};
constexpr std::array<std::string_view, 1> kKey{"id"};
// Value indexes in a parsed rule row; column 0 is the id.
constexpr std::size_t kEffectChannelsColumn = 1;
constexpr std::size_t kReplacesColumn = 2;
constexpr std::size_t kBlockedByColumn = 3;
// The last two timing roles are HP pulse intervals, which must be positive.
constexpr std::size_t kFirstPulseRole = 4;
constexpr TableSpec kRulesSpec = {
    .name = "status-rules",
    .description = "Immutable status effect rules.",
    .columns = kRuleColumns,
    .key = kKey,
};
constexpr TableSpec kTimingsSpec = {
    .name = "status-timings",
    .description =
        "Shared durations/pulse intervals, separate from actor state.",
    .columns = kTimingColumns,
    .key = kKey,
};

std::expected<StatusCatalog, std::string> FromRows(
    std::span<const TableRow> rules, std::span<const TableRow> timings) {
  StatusCatalog catalog;
  std::set<std::size_t> seen;
  for (const auto& row : rules) {
    std::size_t index = 0;
    while (index < kNames.size() &&
           StatusRuleIdentity(index) != row.cells.front()) {
      ++index;
    }
    if (index == kNames.size() || !seen.insert(index).second) {
      return std::unexpected(CellError(row.source, row.line, "id",
                                       "unknown or duplicate status rule"));
    }
    catalog.rules[index] = {
        static_cast<std::uint32_t>(row.values[kEffectChannelsColumn]),
        static_cast<std::uint32_t>(row.values[kReplacesColumn]),
        static_cast<std::uint32_t>(row.values[kBlockedByColumn])};
  }
  if (seen.size() != kNames.size()) {
    return std::unexpected(
        CellError(kStatusRulesPath, 1, "id", "missing required status rule"));
  }
  seen.clear();
  const std::array destinations{
      &catalog.timings.attributes,      &catalog.timings.movement,
      &catalog.timings.equipment,       &catalog.timings.paralysis,
      &catalog.timings.poison_interval, &catalog.timings.regeneration_interval};
  for (const auto& row : timings) {
    const auto found = std::ranges::find(kTimingNames, row.cells.front());
    const auto index = static_cast<std::size_t>(found - kTimingNames.begin());
    if (found == kTimingNames.end() || !seen.insert(index).second) {
      return std::unexpected(CellError(row.source, row.line, "id",
                                       "unknown or duplicate timing role"));
    }
    if (index >= kFirstPulseRole && row.values[1] == 0) {
      return std::unexpected(CellError(row.source, row.line, "status_updates",
                                       "HP pulse interval must be positive"));
    }
    *destinations[index] = static_cast<std::uint8_t>(row.values[1]);
  }
  if (seen.size() != kTimingNames.size()) {
    return std::unexpected(
        CellError(kStatusTimingsPath, 1, "id", "missing required timing role"));
  }
  return catalog;
}
}  // namespace

std::span<const EnumValue> StatusFlags() { return kFlags; }
std::string StatusRuleIdentity(std::size_t legacy_rule) {
  return "status." + std::string(kNames.at(legacy_rule));
}
const StatusRule* StatusCatalog::Rule(int legacy_rule) const {
  return legacy_rule >= 0 &&
                 static_cast<std::size_t>(legacy_rule) < rules.size()
             ? &rules[static_cast<std::size_t>(legacy_rule)]
             : nullptr;
}
const TableSpec& StatusRulesSpec() { return kRulesSpec; }
const TableSpec& StatusTimingsSpec() { return kTimingsSpec; }
std::vector<CatalogTableSource> StatusCatalogSources() {
  return {{kRulesSpec, Platform::Utf8Path(kStatusRulesPath)},
          {kTimingsSpec, Platform::Utf8Path(kStatusTimingsPath)}};
}
std::string FormatStatusRules(const StatusCatalog& catalog) {
  std::vector<std::vector<std::string>> rows{
      {"id", "display_name", "effect_channels", "replaces", "blocked_by"}};
  for (std::size_t index = 0; index < catalog.rules.size(); ++index) {
    const auto& rule = catalog.rules[index];
    std::string label(kNames[index]);
    std::ranges::replace(label, '-', ' ');
    rows.push_back({StatusRuleIdentity(index), label,
                    FormatFlags(kFlags, rule.statuses),
                    FormatFlags(kFlags, rule.cancelled),
                    FormatFlags(kFlags, rule.blocking)});
  }
  return FormatCsv(rows);
}
std::string FormatStatusTimings(const StatusCatalog& catalog) {
  const auto& timing = catalog.timings;
  const std::array values{timing.attributes,      timing.movement,
                          timing.equipment,       timing.paralysis,
                          timing.poison_interval, timing.regeneration_interval};
  std::vector<std::vector<std::string>> rows{
      {"id", "display_name", "status_updates"}};
  for (std::size_t index = 0; index < values.size(); ++index) {
    std::string label(
        kTimingNames[index].substr(kTimingNames[index].find('.') + 1));
    std::ranges::replace(label, '-', ' ');
    rows.push_back({std::string(kTimingNames[index]), label,
                    std::to_string(values[index])});
  }
  return FormatCsv(rows);
}
std::expected<StatusCatalog, std::string> ParseStatusCatalog(
    std::string_view rules, std::string_view timings) {
  auto rule_rows = ReadTable(kRulesSpec, rules, kStatusRulesPath);
  auto timing_rows = ReadTable(kTimingsSpec, timings, kStatusTimingsPath);
  if (!rule_rows) return std::unexpected(rule_rows.error());
  if (!timing_rows) return std::unexpected(timing_rows.error());
  return FromRows(*rule_rows, *timing_rows);
}
std::expected<StatusCatalog, std::string> LoadStatusCatalog(
    const CatalogDatabase& database) {
  auto rules = database.Read(kRulesSpec);
  auto timings = database.Read(kTimingsSpec);
  if (!rules) return std::unexpected(rules.error());
  if (!timings) return std::unexpected(timings.error());
  return FromRows(*rules, *timings);
}
}  // namespace VS::Content
