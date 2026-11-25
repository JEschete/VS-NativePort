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

inline constexpr auto kActionCategoryConverters =
    // NOLINTNEXTLINE(whitespace/indent_namespace): wrapped constant alias.
    kActionCategoryCatalogConverters;
inline constexpr auto kActionCategoryConverter =
    // NOLINTNEXTLINE(whitespace/indent_namespace): wrapped constant alias.
    kActionCategoryConverters.front();
inline constexpr std::string_view kActionCategorySource = "BATTLE/BATTLE.PRG";
inline constexpr std::string_view kActionCategoryOutputId{
    "catalog.battle.action-categories",
};
inline constexpr std::string_view kActionCategoriesPath{
    "catalogs/battle/action-categories.csv",
};
inline constexpr std::size_t kActionCategoryCount = 16;

// Actor kinds an action category may target. The first five are the actor
// kind bits (Actors::kLeaderActorKind and friends); cloudstone and chest are
// the category-only members the original text labels "Cloudstone" and
// "Treasure Chest" (chest also adds the room's chests to the target list).
// decomp: D_800E81D4 (D_800E81D4@BATTLE/BATTLE.PRG)
[[nodiscard]] std::span<const EnumValue> ActionTargetKinds();
[[nodiscard]] std::string ActionCategoryIdentity(std::size_t legacy_category);

// How an action category picks and filters its targets. The rules are the
// five original per-category tables, not their sixteen-byte spans.
struct ActionCategoryRule {
  bool needs_target_selection = false;
  bool area_target = false;
  bool includes_acting_actor = false;
  bool targets_body_parts = false;
  std::uint32_t target_kinds = 0;
  bool operator==(const ActionCategoryRule&) const = default;
};

struct ActionCategoryCatalog {
  // The action record keeps its four-bit category, so the catalog covers
  // exactly the sixteen compatibility indexes it can name.
  std::array<ActionCategoryRule, kActionCategoryCount> rules{};

  [[nodiscard]] const ActionCategoryRule* Rule(int legacy_category) const;
  bool operator==(const ActionCategoryCatalog&) const = default;
};

[[nodiscard]] const TableSpec& ActionCategoriesSpec();
[[nodiscard]] std::vector<CatalogTableSource> ActionCategoryCatalogSources();
[[nodiscard]] std::string FormatActionCategories(
    const ActionCategoryCatalog& catalog);
[[nodiscard]] std::expected<ActionCategoryCatalog, std::string>
ParseActionCategoryCatalog(std::string_view text);
[[nodiscard]] std::expected<ActionCategoryCatalog, std::string>
LoadActionCategoryCatalog(const CatalogDatabase& database);

}  // namespace VS::Content
