#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Content/CatalogDatabase.hpp"
#include "Content/ConversionManifest.hpp"

namespace VS::Content {

inline constexpr std::array kLearningConverters{
    ConverterVersion{.name = "learning-csv", .version = 1},
};
inline constexpr auto kLearningConverter = kLearningConverters.front();
inline constexpr std::string_view kLearningSource = "SLUS_010.40";
inline constexpr std::string_view kLearningOutputId = "catalog.battle.learning";
inline constexpr std::string_view kBreakArtLearningPath{
    "catalogs/battle/break-art-learning.csv",
};
inline constexpr std::string_view kAbilityLearningPath{
    "catalogs/battle/battle-ability-learning.csv",
};

// Explicit adapters to existing equipment categories, action definitions and
// saved progress. These are not arbitrary CSV row numbers.
struct BreakArtLearningDefinition {
  std::string id;
  int weapon_category = 0;
  int tier = 1;
  int action = 0;
  std::int32_t required_weapon_kills = 0;
  bool operator==(const BreakArtLearningDefinition&) const = default;
};

struct AbilityLearningDefinition {
  std::string id;
  int choice_order = 1;
  std::int32_t required_ability_points = 0;
  bool operator==(const AbilityLearningDefinition&) const = default;
};

struct LearningCatalog {
  std::vector<BreakArtLearningDefinition> break_arts;
  std::vector<AbilityLearningDefinition> battle_abilities;

  [[nodiscard]] std::optional<int> BreakArtRequirement(int category,
                                                       int learned) const;
  [[nodiscard]] std::optional<int> AbilityRequirement(int unlocked) const;
  bool operator==(const LearningCatalog&) const = default;
};

[[nodiscard]] const TableSpec& BreakArtLearningSpec();
[[nodiscard]] const TableSpec& AbilityLearningSpec();
[[nodiscard]] std::vector<CatalogTableSource> LearningCatalogSources();
[[nodiscard]] std::string BreakArtLearningId(int category, int tier);
[[nodiscard]] std::string AbilityLearningId(int choice_order);
[[nodiscard]] int BreakArtLearningAction(int category, int tier);
[[nodiscard]] std::string FormatBreakArtLearning(
    const LearningCatalog& catalog);
[[nodiscard]] std::string FormatAbilityLearning(const LearningCatalog& catalog);
[[nodiscard]] std::expected<LearningCatalog, std::string> ParseLearning(
    std::string_view arts, std::string_view abilities);
[[nodiscard]] std::expected<LearningCatalog, std::string> LoadLearning(
    const CatalogDatabase& database);

}  // namespace VS::Content
