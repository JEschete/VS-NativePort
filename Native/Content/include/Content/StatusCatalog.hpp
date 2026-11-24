#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/CatalogDatabase.hpp"
#include "Content/ConversionManifest.hpp"

namespace VS::Content {

inline constexpr auto kStatusConverters = kStatusRuleCatalogConverters;
inline constexpr auto kStatusConverter = kStatusConverters.front();
inline constexpr std::string_view kStatusSource = "SLUS_010.40";
inline constexpr std::string_view kStatusOutputId = "catalog.battle.status";
inline constexpr std::string_view kStatusRulesPath{
    "catalogs/battle/status-rules.csv",
};
inline constexpr std::string_view kStatusTimingsPath{
    "catalogs/battle/status-timings.csv",
};
inline constexpr std::size_t kStatusRuleCount = 32;
inline constexpr std::size_t kStatusTimingCount = 6;

// These are effect-channel bits, not rule IDs. Analyze and instant defeat
// also participate in resistance checks; spell immunity is installed at spawn.
// decomp: statuses (src/BATTLE/BATTLE.PRG/actor.h:24)
[[nodiscard]] std::span<const EnumValue> StatusFlags();
[[nodiscard]] std::string StatusRuleIdentity(std::size_t legacy_rule);

struct StatusRule {
  std::uint32_t statuses = 0;
  std::uint32_t cancelled = 0;
  std::uint32_t blocking = 0;
  bool operator==(const StatusRule&) const = default;
};

// One unit is one eligible status update (the vitals counter modulo 60),
// not wall time. The actor/save stores each timer in a byte. Catalog validation
// refuses values above 255 and zero HP-pulse intervals. A zero duration has
// no expiry countdown. No narrowing or sidecar state is introduced.
struct StatusTimings {
  std::uint8_t attributes = 0;
  std::uint8_t movement = 0;
  std::uint8_t equipment = 0;
  std::uint8_t paralysis = 0;
  std::uint8_t poison_interval = 0;
  std::uint8_t regeneration_interval = 0;
  bool operator==(const StatusTimings&) const = default;
};

struct StatusCatalog {
  // Explicit compatibility lookup for the remaining action/limb rule callers.
  // This is owned typed data, not the sixteen-byte executable record layout.
  std::array<StatusRule, kStatusRuleCount> rules{};
  StatusTimings timings{};

  [[nodiscard]] const StatusRule* Rule(int legacy_rule) const;
  bool operator==(const StatusCatalog&) const = default;
};

[[nodiscard]] const TableSpec& StatusRulesSpec();
[[nodiscard]] const TableSpec& StatusTimingsSpec();
[[nodiscard]] std::vector<CatalogTableSource> StatusCatalogSources();
[[nodiscard]] std::string FormatStatusRules(const StatusCatalog& catalog);
[[nodiscard]] std::string FormatStatusTimings(const StatusCatalog& catalog);
[[nodiscard]] std::expected<StatusCatalog, std::string> ParseStatusCatalog(
    std::string_view rules, std::string_view timings);
[[nodiscard]] std::expected<StatusCatalog, std::string> LoadStatusCatalog(
    const CatalogDatabase& database);

}  // namespace VS::Content
