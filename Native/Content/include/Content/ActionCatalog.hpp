#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/CatalogDatabase.hpp"
#include "Content/ConversionManifest.hpp"

namespace VS::Content {

inline constexpr auto kActionCatalogConverter =
    // NOLINTNEXTLINE(whitespace/indent_namespace): wrapped constant alias.
    kActionCatalogConverters.front();
inline constexpr std::string_view kActionSource = "SLUS_010.40";
inline constexpr std::string_view kActionOutputId = "catalog.battle.actions";
inline constexpr std::string_view kActionsPath = "catalogs/battle/actions.csv";
inline constexpr std::string_view kActionHitsPath{
    "catalogs/battle/action-hits.csv",
};
inline constexpr std::size_t kActionCount = 256;
inline constexpr std::size_t kActionHitSlots = 2;
// The original record keeps a name in 24 bytes, including its terminator.
inline constexpr std::size_t kActionNameBytes = 24;
inline constexpr std::size_t kActionVolumeAxes = 3;

// Names for the numeric selectors the original action record stores. Each is
// the dispatch table slot the battle code reads, so a name always maps back to
// one slot.
[[nodiscard]] std::span<const EnumValue> ActionKinds();
[[nodiscard]] std::span<const EnumValue> ActionVolumeShapes();
[[nodiscard]] std::span<const EnumValue> ActionCameraZooms();
[[nodiscard]] std::span<const EnumValue> ActionBehaviors();
[[nodiscard]] std::span<const EnumValue> HitPrerequisites();
[[nodiscard]] std::span<const EnumValue> HitEffects();
[[nodiscard]] std::span<const EnumValue> HitFunctions();
[[nodiscard]] std::span<const EnumValue> HitStatCalculators();
[[nodiscard]] std::span<const EnumValue> HitAffinities();

// ActionRule::behavior members. The low word keeps the original AI flag
// positions; the record's own flags follow it.
inline constexpr std::uint32_t kBehaviorAiMask = 0xFFF0;
inline constexpr std::uint32_t kBehaviorMenuAction = 0x10000;
inline constexpr std::uint32_t kBehaviorHitsFirstObstruction = 0x20000;
inline constexpr std::uint32_t kBehaviorStartsUnlocked = 0x40000;

// One hit of an action; an action has at most two.
struct ActionHit {
  std::uint8_t effect = 0;
  std::uint8_t prerequisite = 0;
  std::uint8_t hit_function = 0;
  std::uint8_t stat_calculator = 0;
  std::uint8_t stat_factor = 0;
  std::uint8_t damage_type = 0;
  std::uint8_t affinity = 0;
  bool operator==(const ActionHit&) const = default;
};

struct ActionVolume {
  std::array<std::uint8_t, kActionVolumeAxes> extent{};
  std::uint8_t shape = 0;
  std::uint8_t angle = 0;
  bool operator==(const ActionVolume&) const = default;
};

struct ActionRule {
  std::string id;
  // Readable text in game-text markup: ordinary spaces, {level} for the level
  // glyph. It encodes to at most kActionNameBytes including the terminator.
  std::string name;
  std::uint8_t kind = 0;
  std::uint8_t category = 0;
  std::uint8_t cost = 0;
  std::uint8_t recovery_delay = 0;
  std::uint8_t camera_zoom = 0;
  // The effect package's code (Content/EffectPackages.hpp); 0 plays none.
  std::uint8_t effect_package = 0;
  std::uint32_t behavior = 0;
  ActionVolume range;
  ActionVolume area;
  std::array<std::optional<ActionHit>, kActionHitSlots> hits;
  bool operator==(const ActionRule&) const = default;
};

struct ActionCatalog {
  // Indexed by the action's compatibility index (its id in the original
  // table, which saves and scripts still name).
  std::array<ActionRule, kActionCount> actions{};

  [[nodiscard]] const ActionRule* Rule(int legacy_action) const;
  bool operator==(const ActionCatalog&) const = default;
};

using ActionNameBytes = std::array<std::byte, kActionNameBytes>;
// The name's game-text bytes: spaces become the font's six-unit spacing, the
// result is E7-terminated and padded with zeros. Refuses a name that does not
// fit rather than truncating it.
[[nodiscard]] std::expected<ActionNameBytes, std::string> EncodeActionName(
    std::string_view name);
// `action.<slug>`: lowercase letters and digits, apostrophes dropped, other
// runs joined by '-', {markup} ignored.
[[nodiscard]] std::string ActionSlug(std::string_view name);
// The identity of every action: the slug, with -2, -3 ... after the first
// action of a repeated name and untitled-NNN for placeholder records.
[[nodiscard]] std::array<std::string, kActionCount> ActionIdentities(
    const std::array<std::string, kActionCount>& names);

[[nodiscard]] const TableSpec& ActionsSpec();
[[nodiscard]] const TableSpec& ActionHitsSpec();
[[nodiscard]] std::vector<CatalogTableSource> ActionCatalogSources();
[[nodiscard]] std::string FormatActions(const ActionCatalog& catalog);
[[nodiscard]] std::string FormatActionHits(const ActionCatalog& catalog);
[[nodiscard]] std::expected<ActionCatalog, std::string> ParseActionCatalog(
    std::string_view actions, std::string_view hits);
[[nodiscard]] std::expected<ActionCatalog, std::string> LoadActionCatalog(
    const CatalogDatabase& database);

}  // namespace VS::Content
