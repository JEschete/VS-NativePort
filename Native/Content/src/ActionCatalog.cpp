#include "Content/ActionCatalog.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Content/ActionCategoryCatalog.hpp"
#include "Content/GameTextCodec.hpp"
#include "Content/GameplayDomains.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
constexpr std::int64_t kByteMax = 255;
constexpr std::int64_t kFiveBitMax = 31;
constexpr std::string_view kSpacing = "{spacing:6}";

template <std::size_t N>
constexpr std::array<EnumValue, N> Numbered(
    const std::array<std::string_view, N>& names) {
  std::array<EnumValue, N> values{};
  for (std::size_t code = 0; code < N; ++code) {
    values[code] = {static_cast<std::int64_t>(code), names[code]};
  }
  return values;
}

// The action record's 3-bit type. It also selects the cost resource: the
// battle code spends MP, RISK, HP or weapon PP for kinds 1 to 4.
// decomp: vs_Action.type (src/SLUS_010.40/actions.h:29-62)
constexpr std::array<std::string_view, 8> kKindNames{
    "none",      "spell", "ability",     "break-art",
    "weapon-pp", "item",  "base-attack", "trap",
};
constexpr auto kKinds = Numbered(kKindNames);
constexpr std::array<std::string_view, 4> kZoomNames{
    "follow",
    "zoom-in",
    "zoom-out",
    "hold",
};
constexpr auto kZooms = Numbered(kZoomNames);
constexpr std::array<EnumValue, 2> kSlots{{
    {0, "first"},
    {1, "second"},
}};
// Bits 4 to 15 are the record's AI flag word; 0 to 3 are never read. Bits 8 to
// 11 are read by the AI planner's target filters; 14 and 15 by its decision
// scoring. The last three are the record's own flags.
// decomp: vs_ActionFlags (src/SLUS_010.40/actions.h:17-27)
constexpr std::array<EnumValue, 15> kBehaviors{{
    {0x10, "special-mode"},
    {0x20, "rarely-chosen"},
    {0x40, "fallback-eligible"},
    {0x80, "shaped-area"},
    {0x100, "targets-unconditionally"},
    {0x200, "targets-status-restricted"},
    {0x400, "targets-low-mp"},
    {0x800, "targets-low-hp"},
    {0x1000, "ignores-sight"},
    {0x2000, "radial-area"},
    {0x4000, "beneficial"},
    {0x8000, "harmful"},
    {kBehaviorMenuAction, "menu-action"},
    {kBehaviorHitsFirstObstruction, "hits-first-obstruction"},
    {kBehaviorStartsUnlocked, "starts-unlocked"},
}};
// The attack prerequisite table's slots. Several slots behave identically
// (never 0 to 3, always 0 to 2); the data uses distinct codes, so each keeps
// its own name.
// decomp: attackPrerequisiteFunctions (src/BATTLE/BATTLE.PRG/146C.c:301)
constexpr std::array<std::string_view, 40> kPrerequisiteNames{
    "never-0",
    "always-0",
    "always-1",
    "always-2",
    "undead",
    "not-undead",
    "never-1",
    "never-2",
    "never-3",
    "can-apply-strength-down",
    "can-apply-intelligence-down",
    "can-apply-agility-down",
    "can-apply-equipment-down",
    "can-apply-half-movement",
    "can-apply-strength-up",
    "can-apply-intelligence-up",
    "can-apply-agility-up",
    "can-apply-equipment-up",
    "can-apply-quicken",
    "can-apply-silence",
    "can-apply-magic-ward",
    "can-apply-regeneration",
    "can-apply-paralysis",
    "can-apply-poison",
    "can-apply-numbness",
    "can-apply-curse",
    "can-apply-air-attack-up",
    "can-apply-fire-attack-up",
    "can-apply-earth-attack-up",
    "can-apply-water-attack-up",
    "can-apply-air-defense-up",
    "can-apply-fire-defense-up",
    "can-apply-earth-defense-up",
    "can-apply-water-defense-up",
    "can-apply-analyze",
    "can-apply-instant-defeat-undead",
    "can-apply-instant-defeat-living",
    "unused-0",
    "unused-1",
    "unused-2",
};
constexpr auto kPrerequisites = Numbered(kPrerequisiteNames);
// The upstream names of slots 34-36, which this catalog spelled until the
// names said what they check: 34 the analyze rule, 35 and 36 the
// instant-defeat rule for undead and for living targets (Banish and Drain
// Mind use 36).
// decomp: hitPrerequisiteCanApplyBanish (src/BATTLE/BATTLE.PRG/146C.c:10429)
// decomp: hitPrerequisiteCanApplyExorcise (src/BATTLE/BATTLE.PRG/146C.c:10434)
// decomp: hitPrerequisiteCanApplyDrain (src/BATTLE/BATTLE.PRG/146C.c:10443)
constexpr std::array<EnumValue, 3> kLegacyPrerequisites{{
    {34, "can-apply-banish"},
    {35, "can-apply-exorcise"},
    {36, "can-apply-drain"},
}};
// The battle effect table's slots. apply-* and cure-* add or remove the status
// rule of that name. recover-hp-on-* heal only when the hit being answered
// matches. The mark-success slots behave identically, as do the unused
// slots, which are empty in the original and refused natively.
// decomp: actionEffectMediators (src/BATTLE/BATTLE.PRG/146C.c:302)
constexpr std::array<std::string_view, 70> kEffectNames{
    "unused-0",
    "apply-strength-down",
    "apply-intelligence-down",
    "apply-agility-down",
    "apply-equipment-down",
    "apply-legs-impaired",
    "apply-strength-up",
    "apply-intelligence-up",
    "apply-agility-up",
    "apply-equipment-up",
    "apply-quicken",
    "apply-silence",
    "apply-magic-ward",
    "apply-regeneration",
    "apply-analyze",
    "apply-paralysis",
    "apply-poison",
    "apply-numbness",
    "apply-curse",
    "apply-air-attack-up",
    "apply-fire-attack-up",
    "apply-earth-attack-up",
    "apply-water-attack-up",
    "apply-air-defense-up",
    "apply-fire-defense-up",
    "apply-earth-defense-up",
    "apply-water-defense-up",
    "cure-paralysis",
    "cure-poison",
    "cure-curse",
    "cure-numbness",
    "dispel",
    "cure-paralysis-poison-and-numbness",
    "mark-success-0",
    "instant-defeat",
    "armor-wear-from-damage",
    "mark-success-1",
    "set-script-flag-64",
    "set-script-flag-32",
    "mark-success-2",
    "recover-hp-on-spell-hit",
    "recover-hp-on-non-spell-hit",
    "recover-hp-on-physical-hit",
    "recover-hp-on-air-hit",
    "recover-hp-on-fire-hit",
    "recover-hp-on-earth-hit",
    "recover-hp-on-water-hit",
    "recover-hp-on-light-hit",
    "recover-hp-on-dark-hit",
    "recover-hp-on-damage",
    "random-strength-increase",
    "random-intelligence-increase",
    "random-agility-increase",
    "random-max-hp-increase",
    "random-max-mp-increase",
    "random-strength-decrease",
    "random-intelligence-decrease",
    "random-agility-decrease",
    "damage",
    "drain-mp",
    "drain-weapon-pp",
    "drain-shield-pp",
    "recover-hp",
    "recover-mp",
    "recover-weapon-pp",
    "recover-weapon-dp",
    "reduce-risk",
    "unused-1",
    "unused-2",
    "unused-3",
};
constexpr auto kEffects = Numbered(kEffectNames);
// decomp: hitFunctions (Native/Game/src/Battle/HitChance.hpp:25)
constexpr std::array<std::string_view, 8> kHitFunctionNames{
    "none",
    "follows-previous-hit",
    "gem-modifier",
    "agility-difference",
    "intelligence-difference",
    "agility-difference-2",
    "risk",
    "chain-evasion",
};
constexpr auto kHitFunctions = Numbered(kHitFunctionNames);
// decomp: statCalculators (src/BATTLE/BATTLE.PRG/146C.c:303)
constexpr std::array<std::string_view, 27> kStatCalculatorNames{
    "none",
    "last-value",
    "physical-damage-without-status-modifiers",
    "magic-damage",
    "previous-hit-factor-0",
    "hp-lost-factor",
    "previous-hit-factor-1",
    "current-hp-factor",
    "weapon-pp",
    "magic-damage-received-factor",
    "physical-damage-received-factor-0",
    "mp-lost-factor",
    "trap-panel-damage",
    "hp-healed",
    "heal-panel-hp-healed",
    "item-recovery",
    "recoverable-hp",
    "recoverable-mp",
    "previous-hit-factor-2",
    "magic-damage-received-of-type-factor",
    "physical-damage-received-factor-1",
    "spell-cost-factor",
    "damage-received-factor",
    "shield-pp",
    "last-value-factor",
    "physical-damage-with-status-modifiers",
    "risk",
};
constexpr auto kStatCalculators = Numbered(kStatCalculatorNames);
// Slot 23 reads the user's shield PP, not DP; the catalog spelled it as
// upstream does until it was named for what it reads.
// decomp: getEquippedShieldDp (src/BATTLE/BATTLE.PRG/146C.c:12138)
constexpr std::array<EnumValue, 1> kLegacyStatCalculators{{
    {23, "equipped-shield-dp"},
}};
// Zero follows the drawn weapon; the others are the physical and six elemental
// affinities in the order the item tables use.
constexpr std::array<std::string_view, 8> kAffinityNames{
    "weapon", "physical", "air", "fire", "earth", "water", "light", "dark",
};
constexpr auto kAffinities = Numbered(kAffinityNames);

constexpr ColumnSpec Integer(std::string_view name, std::int64_t maximum,
                             std::string_view description) {
  return {.name = name,
          .type = ColumnType::kInteger,
          .minimum = 0,
          .maximum = maximum,
          .references = {},
          .description = description};
}
constexpr ColumnSpec Enumeration(std::string_view name,
                                 std::span<const EnumValue> values,
                                 std::string_view description) {
  return {.name = name,
          .type = ColumnType::kEnum,
          .minimum = 0,
          .maximum = static_cast<std::int64_t>(values.size()) - 1,
          .references = {},
          .description = description,
          .enumeration = values};
}
constexpr ColumnSpec Renamed(ColumnSpec column,
                             std::span<const EnumValue> legacy_values) {
  column.legacy_values = legacy_values;
  return column;
}

// Volume extents are 32 world units a step (a room tile is 128); see the zone
// part ranges for each axis. range_x 255 takes the attacker's weapon extents
// and angle but keeps the action's shape.
// decomp: func_80071F68 (src/BATTLE/BATTLE.PRG/146C.c:3898)
constexpr std::string_view kRangeText{
    "The targeting volume's extent, 32 world units a step; range_x 255, on "
    "the base attacks, takes the attacker's weapon extents (blade range plus "
    "reach) and angle instead, keeping range_shape.",
};
constexpr std::string_view kAreaText{
    "The area of effect's extent around its target, 32 world units a step.",
};
constexpr std::string_view kAngleText{
    "The volume's pitch, in 11.25-degree steps (1/32 turn).",
};
constexpr std::array kActionColumns{
    ColumnSpec{
        .name = "id",
        .type = ColumnType::kId,
        .references = {},
        .description = "Semantic identity: action.<name>, numbered when "
                       "names repeat.",
    },
    ColumnSpec{
        .name = "name",
        .type = ColumnType::kText,
        .references = {},
        .description =
            "The name as the game shows it: ordinary spaces, {level} for the "
            "level glyph. It must encode to at most 23 game-text bytes.",
    },
    ColumnSpec{
        .name = "legacy_id",
        .type = ColumnType::kInteger,
        .minimum = 0,
        .maximum = kByteMax,
        .references = {},
        .description = "The original table index, which saves, scripts and "
                       "the learning catalog still use. Runs from 0 to 255 "
                       "without gaps.",
    },
    Enumeration("kind", kKinds,
                "What the action is. Spells spend MP, abilities RISK, break "
                "arts HP and weapon-pp actions the equipped weapon's PP; "
                "items and traps spend nothing here, and a base attack's "
                "cost comes from the weapon. No shipped action is "
                "weapon-pp."),
    ColumnSpec{
        .name = "category",
        .type = ColumnType::kReference,
        .references = "action-categories",
        .description = "How the action picks and filters its targets.",
    },
    Integer("cost", kByteMax,
            "How much of the kind's resource one use spends; 255 on the base "
            "attack means the weapon decides."),
    Integer("recovery_delay", kByteMax,
            "The delay before the actor acts again: the actor waits (this + "
            "its recovery bonus) * 256 / 35 battle updates. 255 means the "
            "equipped weapon's delay; actions 22-53 add no delay."),
    Enumeration("camera_zoom", kZooms,
                "How the combat camera frames the action."),
    Integer("effect_id", kByteMax,
            "The original effect table slot that presents the action; not "
            "yet named."),
    Integer("range_x", kByteMax, kRangeText),
    Integer("range_y", kByteMax, kRangeText),
    Integer("range_z", kByteMax, kRangeText),
    Enumeration("range_shape", kVolumeShapes,
                "The targeting volume's shape; none means no volume."),
    Integer("range_angle", kFiveBitMax, kAngleText),
    Integer("area_range_x", kByteMax, kAreaText),
    Integer("area_range_y", kByteMax, kAreaText),
    Integer("area_range_z", kByteMax, kAreaText),
    Enumeration("area_shape", kVolumeShapes,
                "The area of effect's shape; none means a single target."),
    Integer("area_angle", kFiveBitMax, kAngleText),
    ColumnSpec{
        .name = "behavior",
        .type = ColumnType::kFlags,
        .minimum = 0,
        .maximum = UINT32_MAX,
        .references = {},
        .description =
            "Flags the battle and AI code read. menu-action actions are "
            "chosen from a menu (cancelling targeting returns to it) and "
            "are presented with their effect rather than a melee "
            "animation; starts-unlocked is the state a new game begins in.",
        .enumeration = kBehaviors,
    },
};
enum ActionColumn : std::size_t {
  kIdColumn,
  kNameColumn,
  kLegacyIdColumn,
  kKindColumn,
  kCategoryColumn,
  kCostColumn,
  kRecoveryDelayColumn,
  kCameraZoomColumn,
  kEffectIdColumn,
  kRangeXColumn,
  kRangeYColumn,
  kRangeZColumn,
  kRangeShapeColumn,
  kRangeAngleColumn,
  kAreaXColumn,
  kAreaYColumn,
  kAreaZColumn,
  kAreaShapeColumn,
  kAreaAngleColumn,
  kBehaviorColumn,
  kActionColumnCount,
};
static_assert(kActionColumns.size() == kActionColumnCount);

// The hit columns describe what the battle code does with each value; the
// formulas are in Docs/pipeline/ACTION_HITS.md (ASSET_PIPELINE §4.7.1).
// decomp: canPerformAttack (src/BATTLE/BATTLE.PRG/146C.c:10458)
// decomp: doesAttackHit (src/BATTLE/BATTLE.PRG/146C.c:10059)
// decomp: getPhysicalAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11202)
// decomp: damageFromTrapPanel (src/BATTLE/BATTLE.PRG/146C.c:11655)
// decomp: itemRecovery (src/BATTLE/BATTLE.PRG/146C.c:12032)
constexpr std::array kHitColumns{
    ColumnSpec{
        .name = "action",
        .type = ColumnType::kReference,
        .references = "actions",
        .description = "The action the hit belongs to.",
    },
    Enumeration("slot", kSlots,
                "Which of the action's two hits. An absent hit is an all-zero "
                "word, so only hits that do something have a row."),
    Enumeration(
        "effect", kEffects,
        "What the hit does to each target it lands on, using the stat "
        "calculator's amount where it needs one. apply-X and cure-X add or "
        "remove status X; cure-paralysis-poison-and-numbness removes all three "
        "and dispel removes every stat, element, quicken, silence, "
        "regeneration and magic-ward status. damage takes the amount from HP "
        "and wears the equipment. drain-mp takes the amount from MP, at most "
        "what the target has; drain-weapon-pp empties the target's weapon PP "
        "and drain-shield-pp its shield PP (only when the answered hit did "
        "damage). recover-hp, recover-mp, recover-weapon-pp and "
        "recover-weapon-dp add the amount; reduce-risk lowers risk by it. "
        "recover-hp-on-X heals the amount only when the hit being answered "
        "matches X: a spell, a non-spell, that affinity or any damage. "
        "random-X-increase and -decrease change that stat by 1-4. "
        "instant-defeat clears the statuses the instant-defeat rule lists and "
        "takes all remaining HP. armor-wear-from-damage wears the hit limb's "
        "armor DP by twice the HP damage already queued. mark-success-0 to "
        "-2 mark the user's action as succeeded for the field abilities that "
        "check it. set-script-flag-32 and -64 set that script flag to 1 when "
        "executed. unused-0 to -3 are refused."),
    Renamed(
        Enumeration(
            "prerequisite", kPrerequisites,
            "Which targets the hit applies to, checked per target before the "
            "roll; a target that fails is skipped, and on the first hit it "
            "also counts as a miss. never-0 disables the hit. never-1, never-2 "
            "and never-3 make it direct: it applies once to the first target "
            "with no check and no roll, so hit_function is unread. always-2 "
            "applies it to the user instead. always-0, always-1 and unused-0 "
            "to -2 accept every target, except that always-0 on the second hit "
            "skips a target whose first hit was cancelled. undead and "
            "not-undead accept only that enemy class. can-apply-X accepts a "
            "target that can receive status X: the rule exists, the target is "
            "not immune and no blocking status is active. Equipment up and "
            "down also need armor, a shield or a blade; the attack-ups a "
            "blade; the defense-ups armor or a shield. "
            "can-apply-instant-defeat-undead and -living also accept only "
            "undead or only living targets. Earlier names: can-apply-banish "
            "(now -analyze), can-apply-exorcise (-instant-defeat-undead) and "
            "can-apply-drain (-instant-defeat-living)."),
        kLegacyPrerequisites),
    Enumeration(
        "hit_function", kHitFunctions,
        "How the percent chance to hit is worked out when the hit rolls. "
        "follows-previous-hit: 100 if the action's last roll landed (for one "
        "target, its first hit), else a miss. gem-modifier: 100, +20 with the "
        "matching accuracy gem on the user's drawn weapon, -20 for each "
        "matching protective gem on the target's drawn shield. "
        "agility-difference: 100 + the user's agility - the target's agility "
        "and limb defense, each scaled by (100 - its risk)%. "
        "intelligence-difference: the same with intelligence and no 100, the "
        "target's side also scaled by its HP fraction. agility-difference-2: "
        "130 - the action's cost in place of 100. These three vary by up to "
        "5 either way when executed and add the gem bonuses. risk: (user's "
        "risk + 150) x 100 / 256. chain-evasion: (255 - the target limb's "
        "chain_evasion) x 100 / 255. none: refused if rolled. A spell starts "
        "at 0 against a spell-immune target. The chance doubles against a "
        "left-arm-impaired target and is capped at 100; the player's rolls "
        "then get a hidden +10."),
    Renamed(
        Enumeration(
            "stat_calculator", kStatCalculators,
            "How the amount the effect uses is worked out. "
            "physical-damage-with-status-modifiers and -without- use strength "
            "(with halves it while the user is right-arm-impaired), "
            "magic-damage intelligence and hp-healed intelligence for "
            "healing: the user's stat and gear rating scaled by stat_factor, "
            "less the target's stat, armor and shield defense, with random "
            "variation. item-recovery: 5 x stat_factor, varying by up to 5. "
            "recoverable-hp and -mp: the target's missing HP or MP. risk: the "
            "target's risk. weapon-pp and shield-pp: the user's weapon or "
            "shield PP (shield-pp was equipped-shield-dp). hp-lost-factor and "
            "mp-lost-factor: the user's missing HP or MP x stat_factor / 10. "
            "current-hp-factor: the target's HP x stat_factor / 100. "
            "previous-hit-factor-0 to -2: the HP and MP damage the chained "
            "attack dealt its first target x stat_factor / 10. magic-, "
            "physical- and damage-received factors: the answered hit's damage "
            "x stat_factor / 10 when it did damage and was a spell, not a "
            "spell, or either. spell-cost-factor: the answered spell's MP "
            "cost x stat_factor / 10. last-value: the previous calculation's "
            "amount; last-value-factor: it x stat_factor / 10. "
            "trap-panel-damage and heal-panel-hp-healed: the target's max HP / "
            "10 x stat_factor + 10 per trap level, less its defense. "
            "weapon-pp and the hp-lost, mp-lost, current-hp and previous-hit "
            "calculators add 1 for each earlier ability in a chain. none: no "
            "amount, for status effects."),
        kLegacyStatCalculators),
    Integer("stat_factor", kFiveBitMax,
            "The stat calculator's multiplier: +10% attack per point for the "
            "damage and healing calculators, 5 HP or MP per point for "
            "item-recovery, tenths for the *-factor calculators (hundredths "
            "for current-hp-factor) and the max-HP multiplier for the trap "
            "panels. The other calculators ignore it."),
    Enumeration("damage_type", kDamageTypes,
                "The hit's damage type; none lets a physical hit use the "
                "user's weapon's, or blunt without a blade."),
    Enumeration("affinity", kAffinities,
                "The hit's element. weapon takes the user's drawn blade's "
                "highest affinity, or physical when the weapon is sheathed, "
                "absent or has none above zero. hp-healed refuses weapon, and "
                "physical while the user's weapon is drawn."),
};
enum HitColumn : std::size_t {
  kHitActionColumn,
  kHitSlotColumn,
  kHitEffectColumn,
  kHitPrerequisiteColumn,
  kHitFunctionColumn,
  kHitStatCalculatorColumn,
  kHitStatFactorColumn,
  kHitDamageTypeColumn,
  kHitAffinityColumn,
  kHitColumnCount,
};
static_assert(kHitColumns.size() == kHitColumnCount);

constexpr std::array<std::string_view, 1> kActionKey{"id"};
constexpr std::array<std::string_view, 2> kHitKey{"action", "slot"};
constexpr TableSpec kActionsSpec = {
    .name = "actions",
    .description =
        "Every battle action: spells, abilities, break arts, items, traps and "
        "the base attacks. Hits are in action-hits.",
    .columns = kActionColumns,
    .key = kActionKey,
};
constexpr TableSpec kActionHitsSpec = {
    .name = "action-hits",
    .description = "Each action's hits; an action has at most two.",
    .columns = kHitColumns,
    .key = kHitKey,
};

std::string_view LabelOf(std::span<const EnumValue> domain, std::int64_t code) {
  const auto found = std::ranges::find(domain, code, &EnumValue::code);
  return found == domain.end() ? std::string_view{} : found->name;
}

std::string Label(std::span<const EnumValue> domain, std::int64_t code) {
  const auto name = LabelOf(domain, code);
  return name.empty() ? std::to_string(code) : std::string(name);
}

std::expected<ActionCatalog, std::string> FromRows(
    std::span<const TableRow> actions, std::span<const TableRow> hits) {
  ActionCatalog catalog;
  std::array<bool, kActionCount> seen{};
  std::map<std::string, std::size_t> by_id;
  for (const auto& row : actions) {
    const auto legacy = static_cast<std::size_t>(row.values[kLegacyIdColumn]);
    if (seen[legacy]) {
      return std::unexpected(CellError(row.source, row.line, "legacy_id",
                                       "duplicate action index"));
    }
    seen[legacy] = true;
    by_id.emplace(row.cells[kIdColumn], legacy);
    std::size_t category = 0;
    while (category < kActionCategoryCount &&
           ActionCategoryIdentity(category) != row.cells[kCategoryColumn]) {
      ++category;
    }
    if (category == kActionCategoryCount) {
      return std::unexpected(CellError(row.source, row.line, "category",
                                       "unknown action category"));
    }
    if (const auto encoded = EncodeActionName(row.cells[kNameColumn]);
        !encoded) {
      return std::unexpected(
          CellError(row.source, row.line, "name", encoded.error()));
    }
    const auto byte = [&row](std::size_t column) {
      return static_cast<std::uint8_t>(row.values[column]);
    };
    catalog.actions[legacy] = {
        .id = row.cells[kIdColumn],
        .name = row.cells[kNameColumn],
        .kind = byte(kKindColumn),
        .category = static_cast<std::uint8_t>(category),
        .cost = byte(kCostColumn),
        .recovery_delay = byte(kRecoveryDelayColumn),
        .camera_zoom = byte(kCameraZoomColumn),
        .effect_id = byte(kEffectIdColumn),
        .behavior = static_cast<std::uint32_t>(row.values[kBehaviorColumn]),
        .range = {{byte(kRangeXColumn), byte(kRangeYColumn),
                   byte(kRangeZColumn)},
                  byte(kRangeShapeColumn),
                  byte(kRangeAngleColumn)},
        .area = {{byte(kAreaXColumn), byte(kAreaYColumn), byte(kAreaZColumn)},
                 byte(kAreaShapeColumn),
                 byte(kAreaAngleColumn)},
        .hits = {},
    };
  }
  if (std::ranges::find(seen, false) != seen.end()) {
    return std::unexpected(CellError(kActionsPath, 1, "legacy_id",
                                     "missing required action index"));
  }
  for (const auto& row : hits) {
    const auto owner = by_id.find(row.cells[kHitActionColumn]);
    if (owner == by_id.end()) {
      return std::unexpected(CellError(row.source, row.line, "action",
                                       "names no action in actions.csv"));
    }
    const auto byte = [&row](std::size_t column) {
      return static_cast<std::uint8_t>(row.values[column]);
    };
    const ActionHit hit{
        .effect = byte(kHitEffectColumn),
        .prerequisite = byte(kHitPrerequisiteColumn),
        .hit_function = byte(kHitFunctionColumn),
        .stat_calculator = byte(kHitStatCalculatorColumn),
        .stat_factor = byte(kHitStatFactorColumn),
        .damage_type = byte(kHitDamageTypeColumn),
        .affinity = byte(kHitAffinityColumn),
    };
    if (hit == ActionHit{}) {
      return std::unexpected(
          CellError(row.source, row.line, "effect",
                    "an all-zero hit is no hit; remove the row instead"));
    }
    catalog.actions[owner->second]
        .hits[static_cast<std::size_t>(row.values[kHitSlotColumn])] = hit;
  }
  return catalog;
}
}  // namespace

std::span<const EnumValue> ActionKinds() { return kKinds; }
std::span<const EnumValue> ActionVolumeShapes() { return kVolumeShapes; }
std::span<const EnumValue> ActionCameraZooms() { return kZooms; }
std::span<const EnumValue> ActionBehaviors() { return kBehaviors; }
std::span<const EnumValue> HitPrerequisites() { return kPrerequisites; }
std::span<const EnumValue> HitEffects() { return kEffects; }
std::span<const EnumValue> HitFunctions() { return kHitFunctions; }
std::span<const EnumValue> HitStatCalculators() { return kStatCalculators; }
std::span<const EnumValue> HitAffinities() { return kAffinities; }

const ActionRule* ActionCatalog::Rule(int legacy_action) const {
  return legacy_action >= 0 &&
                 static_cast<std::size_t>(legacy_action) < actions.size()
             ? &actions[static_cast<std::size_t>(legacy_action)]
             : nullptr;
}

std::expected<ActionNameBytes, std::string> EncodeActionName(
    std::string_view name) {
  std::string markup;
  for (const char glyph : name) {
    if (glyph == ' ') {
      markup += kSpacing;
    } else {
      markup += glyph;
    }
  }
  auto encoded = EncodeGameText(markup, "action name");
  if (!encoded) return std::unexpected(encoded.error());
  if (encoded->size() > kActionNameBytes) {
    return std::unexpected(std::format(
        "'{}' encodes to {} bytes; at most {} fit with the terminator", name,
        encoded->size(), kActionNameBytes));
  }
  ActionNameBytes bytes{};
  std::ranges::copy(*encoded, bytes.begin());
  return bytes;
}

std::string ActionSlug(std::string_view name) {
  std::string slug;
  bool separate = false;
  for (std::size_t at = 0; at < name.size(); ++at) {
    const char glyph = name[at];
    if (glyph == '{') {
      const auto close = name.find('}', at);
      if (close == std::string_view::npos) break;
      at = close;
      separate = true;
    } else if ((glyph >= 'a' && glyph <= 'z') ||
               (glyph >= '0' && glyph <= '9')) {
      if (separate && !slug.empty()) slug += '-';
      separate = false;
      slug += glyph;
    } else if (glyph >= 'A' && glyph <= 'Z') {
      if (separate && !slug.empty()) slug += '-';
      separate = false;
      slug += static_cast<char>(glyph - 'A' + 'a');
    } else if (glyph != '\'') {
      separate = true;
    }
  }
  return slug;
}

std::array<std::string, kActionCount> ActionIdentities(
    const std::array<std::string, kActionCount>& names) {
  std::array<std::string, kActionCount> identities;
  std::set<std::string> used;
  for (std::size_t index = 0; index < kActionCount; ++index) {
    auto base = ActionSlug(names[index]);
    if (base.empty() || base == "untitled") {
      base = std::format("untitled-{:03}", index);
    }
    auto candidate = "action." + base;
    for (int repeat = 2; !used.insert(candidate).second; ++repeat) {
      candidate = std::format("action.{}-{}", base, repeat);
    }
    identities[index] = std::move(candidate);
  }
  return identities;
}

const TableSpec& ActionsSpec() { return kActionsSpec; }
const TableSpec& ActionHitsSpec() { return kActionHitsSpec; }
std::vector<CatalogTableSource> ActionCatalogSources() {
  return {{kActionsSpec, Platform::Utf8Path(kActionsPath)},
          {kActionHitsSpec, Platform::Utf8Path(kActionHitsPath)}};
}

std::string FormatActions(const ActionCatalog& catalog) {
  std::vector<std::vector<std::string>> rows;
  for (std::size_t index = 0; index < catalog.actions.size(); ++index) {
    const auto& rule = catalog.actions[index];
    const auto number = [](std::uint8_t value) {
      return std::to_string(value);
    };
    rows.push_back({
        rule.id,
        rule.name,
        std::to_string(index),
        Label(kKinds, rule.kind),
        ActionCategoryIdentity(rule.category),
        number(rule.cost),
        number(rule.recovery_delay),
        Label(kZooms, rule.camera_zoom),
        number(rule.effect_id),
        number(rule.range.extent[0]),
        number(rule.range.extent[1]),
        number(rule.range.extent[2]),
        Label(kVolumeShapes, rule.range.shape),
        number(rule.range.angle),
        number(rule.area.extent[0]),
        number(rule.area.extent[1]),
        number(rule.area.extent[2]),
        Label(kVolumeShapes, rule.area.shape),
        number(rule.area.angle),
        FormatFlags(kBehaviors, rule.behavior),
    });
  }
  return FormatTable(kActionsSpec, rows);
}

std::string FormatActionHits(const ActionCatalog& catalog) {
  std::vector<std::vector<std::string>> rows;
  for (const auto& rule : catalog.actions) {
    for (std::size_t slot = 0; slot < rule.hits.size(); ++slot) {
      if (!rule.hits[slot]) continue;
      const auto& hit = *rule.hits[slot];
      rows.push_back({
          rule.id,
          Label(kSlots, static_cast<std::int64_t>(slot)),
          Label(kEffects, hit.effect),
          Label(kPrerequisites, hit.prerequisite),
          Label(kHitFunctions, hit.hit_function),
          Label(kStatCalculators, hit.stat_calculator),
          std::to_string(hit.stat_factor),
          Label(kDamageTypes, hit.damage_type),
          Label(kAffinities, hit.affinity),
      });
    }
  }
  return FormatTable(kActionHitsSpec, rows);
}

std::expected<ActionCatalog, std::string> ParseActionCatalog(
    std::string_view actions, std::string_view hits) {
  auto action_rows = ReadTable(kActionsSpec, actions, kActionsPath);
  auto hit_rows = ReadTable(kActionHitsSpec, hits, kActionHitsPath);
  if (!action_rows) return std::unexpected(action_rows.error());
  if (!hit_rows) return std::unexpected(hit_rows.error());
  return FromRows(*action_rows, *hit_rows);
}

std::expected<ActionCatalog, std::string> LoadActionCatalog(
    const CatalogDatabase& database) {
  auto actions = database.Read(kActionsSpec);
  auto hits = database.Read(kActionHitsSpec);
  if (!actions) return std::unexpected(actions.error());
  if (!hits) return std::unexpected(hits.error());
  return FromRows(*actions, *hits);
}
}  // namespace VS::Content
