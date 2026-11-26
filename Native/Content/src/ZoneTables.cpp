// The zone and enemy tables (ZoneTables.hpp).
#include "Content/ZoneTables.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/ActionCatalog.hpp"
#include "Content/CatalogTable.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/GameTextCodec.hpp"
#include "Content/GameplayDomains.hpp"
#include "Content/StatusCatalog.hpp"

namespace VS::Content {
namespace {

constexpr std::uint32_t kByteBits = 8;
constexpr unsigned kByteMask = 0xFF;
constexpr std::string_view kWorldDirectory = "catalogs/world";
constexpr std::string_view kEnemyDirectory = "catalogs/enemies";
// The enemy template column that holds its position in its zone.
constexpr std::string_view kSlotColumn = "slot";
// The tables a field reference names: the action catalog's actions, and the
// enemy kinds templates name by the template column `kind` (version 12's
// numeric sub_type). A kind number is the actor's 16-bit sub_type.
constexpr std::string_view kActionsTable = "actions";
constexpr std::string_view kKindsTable = "enemy-kinds";
constexpr std::string_view kKindIdPrefix = "enemy-kind.";
constexpr std::string_view kKindColumn = "kind";
constexpr std::string_view kLegacyKindColumn = "sub_type";
constexpr std::string_view kUntitled = "untitled";
constexpr std::int64_t kMaxKind = 0xFFFF;
// A zone's room slots, textures and enemies. An enemy placement names its
// template by a byte.
constexpr std::int64_t kMaxRoomSlots = 0xFFFF;
constexpr std::int64_t kMaxTextures = 0xFFFF;
constexpr std::int64_t kMaxEnemies = 0xFF;
constexpr std::int64_t kLimbCount = 6;
// The ZUD index is a template's last word.
constexpr std::size_t kZudIndexBytes = 4;
constexpr std::size_t kZudIndexAt = kZndTemplateBytes - kZudIndexBytes;
// A word gap: the FA control that advances 6 pixels, which names hold
// between words in place of the space glyph.
constexpr std::string_view kWordGap = "{spacing:6}";
constexpr std::string_view kSpace = " ";
// A name slot written as hexadecimal (version 11) holds two digits a byte.
constexpr std::size_t kHexNameDigits = 2 * kZoneNameBytes;
constexpr unsigned kHexDigitBits = 4U;
constexpr std::array<std::string_view, 4> kTypeFields{
    "none",
    "blunt",
    "edged",
    "piercing",
};
// EquipmentStats.cpp uses classes[0..5] and affinities[0..6]; the arrays'
// tail bytes are reserved (AddPartFields).
constexpr std::array<std::string_view, 6> kClassFields{
    "human", "beast", "undead", "phantom", "dragon", "evil",
};
constexpr std::array<std::string_view, 7> kAffinityFields{
    "physical", "air", "fire", "earth", "water", "light", "dark",
};
constexpr std::array<std::string_view, 6> kSplashFields{
    "limb_0_percent", "limb_1_percent", "limb_2_percent",
    "limb_3_percent", "limb_4_percent", "limb_5_percent",
};
// The AI settings' domains; AddEnemyFields' descriptions give each meaning.
constexpr std::array<EnumValue, 4> kRelationCategories{{
    {0, "hostile"},
    {1, "neutral"},
    {2, "passive"},
    {3, "ashley-ally"},
}};
constexpr std::array<EnumValue, 4> kFleeRules{{
    {0, "never"},
    {1, "below-quarter-hp"},
    {2, "below-half-hp"},
    {3, "on-sight-of-hostile"},
}};
constexpr std::array<EnumValue, 4> kTileStepLimits{{
    {0, "one-tile"},
    {1, "two-tiles"},
    {2, "three-tiles"},
    {3, "four-tiles"},
}};

// decomp: vs_CharInitData (src/BATTLE/BATTLE.PRG/charInit.h:24-51) and the
// records it holds; see Actors/CharacterInit.hpp and Items/ItemRecords.hpp.
constexpr std::size_t kLimbsAt = 0x238;
constexpr std::size_t kLimbBytes = 0x5C;
constexpr std::size_t kLimbArmorAt = 0x20;
constexpr std::size_t kPartBytes = 0x30;
constexpr std::size_t kLimbActionSlots = 4;

// One column's bytes in its record: a little-endian integer of `size` bytes,
// `bits` bits of one byte from `bit` up, or a name slot of `size` bytes as
// text (ZoneNameText).
struct Field {
  std::string_view column;
  std::size_t offset = 0;
  std::size_t size = 1;
  bool is_signed = false;
  bool text = false;
  std::uint8_t bit = 0;
  std::uint8_t bits = 0;  // 0: the whole value
};

// A former column of `size` little-endian bytes from `offset`, signed or not,
// now split into bit-field columns.
struct PackedColumn {
  std::string_view legacy;
  std::size_t offset = 0;
  std::size_t size = 1;
  bool is_signed = false;
  std::vector<std::string_view> columns;
};

// A byte no column holds (§4.1): zero padding, a constant, 1 exactly when
// the column `set_by` is not zero, or byte `slot_byte` (little-endian) of the
// template's slot. `legacy` is the column that held it before version 5
// (raw_<offset>), 6 (action_N_index), 7 (the equipment indices), 9
// (zud_index), 10 (ai_settings) or 11 (the class and affinity tail bytes),
// and `numbered` the one version 1 gave it, when that differs. It holds the
// bits `mask` of the byte; columns hold the rest.
struct ReservedByte {
  std::string_view legacy;
  std::size_t offset = 0;
  std::string_view set_by;
  std::optional<std::uint8_t> constant;
  std::optional<std::uint8_t> slot_byte;
  std::uint8_t mask = kByteMask;
  std::string_view numbered;
};

// A table's columns: the leading key and reference columns, then the fields.
struct TableLayout {
  std::deque<std::string> names;  // storage for generated column names
  std::vector<ColumnSpec> columns;
  std::vector<Field> fields;
  std::vector<std::string_view> key;
  std::vector<PackedColumn> packed;
  std::vector<ReservedByte> reserved;
  // Byte columns version 8 made signed; version 7 wrote 128-255 for the
  // negative values.
  std::vector<std::string_view> resigned;
  TableSpec spec;
  std::string directory;

  void Lead(ColumnSpec column) { columns.push_back(column); }

  void Reserve(std::string_view legacy, std::size_t offset,
               std::string_view set_by = {}) {
    reserved.push_back({.legacy = legacy,
                        .offset = offset,
                        .set_by = set_by,
                        .constant = std::nullopt,
                        .slot_byte = std::nullopt,
                        .mask = kByteMask,
                        .numbered = {}});
  }

  void Constant(std::string_view legacy, std::size_t offset,
                std::uint8_t value) {
    reserved.push_back({.legacy = legacy,
                        .offset = offset,
                        .set_by = {},
                        .constant = value,
                        .slot_byte = std::nullopt,
                        .mask = kByteMask,
                        .numbered = {}});
  }

  // Bits `mask` of a byte whose other bits are in columns, always zero.
  void Padding(std::string_view legacy, std::size_t offset, std::uint8_t mask) {
    reserved.push_back({.legacy = legacy,
                        .offset = offset,
                        .set_by = {},
                        .constant = 0,
                        .slot_byte = std::nullopt,
                        .mask = mask,
                        .numbered = {}});
  }

  // A little-endian field of `size` bytes that holds the template's slot.
  void Slot(std::string_view legacy, std::size_t offset, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
      reserved.push_back({.legacy = legacy,
                          .offset = offset + index,
                          .set_by = {},
                          .constant = std::nullopt,
                          .slot_byte = static_cast<std::uint8_t>(index),
                          .mask = kByteMask,
                          .numbered = {}});
    }
  }

  // A field of `size` bytes that always holds zero; each byte names the
  // field's former column.
  void Zeros(std::string_view legacy, std::size_t offset, std::size_t size) {
    for (std::size_t at = offset; at < offset + size; ++at) {
      Constant(legacy, at, 0);
    }
  }

  // An array's tail byte that always holds zero, in column `legacy` before
  // version 11 and `numbered` in version 1.
  void Tail(std::string_view legacy, std::string_view numbered,
            std::size_t offset) {
    Constant(legacy, offset, 0);
    reserved.back().numbered = numbered;
  }

  // A reference stored as the row's original index; 0 is none when
  // `optional`.
  void Reference(std::string_view name, std::size_t offset, std::size_t size,
                 std::string_view table, std::string_view description,
                 std::string_view legacy_name, bool optional = true) {
    fields.push_back({.column = name,
                      .offset = offset,
                      .size = size,
                      .is_signed = false,
                      .text = false,
                      .bit = 0,
                      .bits = 0});
    columns.push_back({.name = name,
                       .type = ColumnType::kReference,
                       .minimum = 0,
                       .maximum = 0,
                       .references = table,
                       .description = description,
                       .enumeration = {},
                       .legacy_name = legacy_name,
                       .optional = optional});
  }

  void Add(std::string_view name, std::size_t offset, std::size_t size,
           bool is_signed, std::string_view description,
           std::span<const EnumValue> domain = {},
           std::string_view legacy_name = {}) {
    Field field{.column = name,
                .offset = offset,
                .size = size,
                .is_signed = is_signed,
                .text = false,
                .bit = 0,
                .bits = 0};
    fields.push_back(field);
    const std::uint32_t width = static_cast<std::uint32_t>(size) * kByteBits;
    const std::int64_t span = std::int64_t{1} << width;
    columns.push_back(
        {.name = name,
         .type = domain.empty() ? ColumnType::kInteger : ColumnType::kEnum,
         .minimum = is_signed ? -(span / 2) : 0,
         .maximum = is_signed ? (span / 2) - 1 : span - 1,
         .references = {},
         .description = description,
         .enumeration = domain,
         .legacy_name = legacy_name});
  }

  void Bits(std::string_view name, std::size_t offset, std::uint8_t bit,
            std::uint8_t bits, std::string_view description,
            std::span<const EnumValue> domain = {}) {
    fields.push_back({.column = name,
                      .offset = offset,
                      .size = 1,
                      .is_signed = false,
                      .text = false,
                      .bit = bit,
                      .bits = bits});
    columns.push_back(
        {.name = name,
         .type = domain.empty() ? ColumnType::kInteger : ColumnType::kEnum,
         .minimum = 0,
         .maximum = (std::int64_t{1} << bits) - 1,
         .references = {},
         .description = description,
         .enumeration = domain});
  }

  // An unsigned set of named bits; `domain` must name every bit.
  void Flags(std::string_view name, std::size_t offset, std::size_t size,
             std::string_view description, std::span<const EnumValue> domain) {
    fields.push_back({.column = name,
                      .offset = offset,
                      .size = size,
                      .is_signed = false,
                      .text = false,
                      .bit = 0,
                      .bits = 0});
    const std::uint32_t width = static_cast<std::uint32_t>(size) * kByteBits;
    columns.push_back({.name = name,
                       .type = ColumnType::kFlags,
                       .minimum = 0,
                       .maximum = (std::int64_t{1} << width) - 1,
                       .references = {},
                       .description = description,
                       .enumeration = domain});
  }

  // A name slot as optional text (ZoneNameText).
  void Text(std::string_view name, std::size_t offset,
            std::string_view description) {
    fields.push_back({.column = name,
                      .offset = offset,
                      .size = kZoneNameBytes,
                      .is_signed = false,
                      .text = true,
                      .bit = 0,
                      .bits = 0});
    columns.push_back({.name = name,
                       .type = ColumnType::kText,
                       .minimum = 0,
                       .maximum = 0,
                       .references = {},
                       .description = description,
                       .enumeration = {},
                       .legacy_name = {},
                       .optional = true});
  }

  void Run(std::string_view stem, std::size_t offset,
           std::span<const std::string_view> meanings, std::size_t size,
           bool is_signed, std::string_view description) {
    for (std::size_t index = 0; index < meanings.size(); ++index) {
      const std::string& name =
          names.emplace_back(std::format("{}_{}", stem, meanings[index]));
      const std::string& legacy =
          names.emplace_back(std::format("{}_{}", stem, index + 1));
      Add(name, offset + (index * size), size, is_signed, description, {},
          legacy);
    }
  }

  std::string_view Name(std::string text) {
    return names.emplace_back(std::move(text));
  }

  void Finish(std::string_view name, std::string_view description,
              std::string_view in) {
    directory = in;
    spec = TableSpec{.name = name,
                     .description = description,
                     .columns = columns,
                     .key = key};
  }
};

// The equipment parts a template holds, each 0x30 bytes from its offset.
struct PartSlot {
  std::string_view id;
  std::size_t offset;
};
constexpr std::array<PartSlot, 16> kPartSlots{{
    {"weapon.blade", 0x34},
    {"weapon.grip", 0x64},
    {"weapon.gem1", 0x94},
    {"weapon.gem2", 0xC4},
    {"weapon.gem3", 0xF4},
    {"shield.base", 0x140},
    {"shield.gem1", 0x170},
    {"shield.gem2", 0x1A0},
    {"shield.gem3", 0x1D0},
    {"accessory", 0x204},
    {"limb0.armor", kLimbsAt + kLimbArmorAt},
    {"limb1.armor", kLimbsAt + kLimbBytes + kLimbArmorAt},
    {"limb2.armor", kLimbsAt + (2 * kLimbBytes) + kLimbArmorAt},
    {"limb3.armor", kLimbsAt + (3 * kLimbBytes) + kLimbArmorAt},
    {"limb4.armor", kLimbsAt + (4 * kLimbBytes) + kLimbArmorAt},
    {"limb5.armor", kLimbsAt + (5 * kLimbBytes) + kLimbArmorAt},
}};

void AddEnemyFields(TableLayout& table) {
  // decomp: updateEnemyKills (src/BATTLE/BATTLE.PRG/146C.c:1414) sets the
  // kind's kill bit, which the bestiary's first_defeated_subtype ranges test.
  // decomp: func_800E5A9C (src/BATTLE/BATTLE.PRG/7CD68.c:205)
  // decomp: func_800DE3E4 (src/BATTLE/BATTLE.PRG/74088.c:718)
  table.Reference(kKindColumn, 0x00, 2, kKindsTable,
                  "the enemy kind (the actor's sub_type): a defeat sets its "
                  "bestiary kill bit (kinds 0-255; the disc's 'nothing' rows "
                  "use 256-268), and kinds 172 and 173 take the orbiting boss "
                  "AI",
                  kLegacyKindColumn, false);
  // decomp: func_800775C0 (src/BATTLE/BATTLE.PRG/146C.c:6393) gives the actor
  // this id, loads id + 1 as a second body for the ids vs_main_actorExtraShp
  // (src/SLUS_010.40/main.c:755) marks, and loads the body alone for 127.
  // decomp: func_8007CF18 (src/BATTLE/BATTLE.PRG/146C.c:9093)
  table.Add("model_id", 0x02, 1, false,
            "the actor's model id, by which scripts find it; ids 26, 71, 79 "
            "and 175 also load a second body model, and 127 loads the body "
            "without animations, weapon or shield. The body itself comes "
            "from the archetype");
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941)
  // decomp: vs_battle_equipWeapon (src/BATTLE/BATTLE.PRG/146C.c:1035)
  table.Bits("reach", 0x03, 0, 5,
             "added to each of the weapon's three range extents (32 world "
             "units a step)");
  table.Bits("enemy_class", 0x03, 5, 3, "the class (human .. evil)",
             kEnemyClasses);
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941) copies the name
  // into the actor's stats.
  table.Text("name", 0x04,
             "the actor's name: text of at most 23 bytes in the game's glyph "
             "set (one byte a glyph; a {token} one or more), a space being a "
             "word gap ({spacing:6}); empty for none");
  table.Add("hp", 0x1C, 2, true, "hit points");
  table.Add("mp", 0x1E, 2, true, "magic points");
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941) varies a room
  // enemy's HP, MP (unless 0) and attributes by -10..+10, keeps the recovery
  // delay bonus's low six bits and gives the enemy 9/10 of each speed.
  table.Add("strength", 0x20, 1, false,
            "base strength; a room enemy spawns with up to 10 more or less");
  table.Add("intelligence", 0x21, 1, false,
            "base intelligence; a room enemy spawns with up to 10 more or "
            "less");
  table.Add("agility", 0x22, 1, false,
            "base agility; a room enemy spawns with up to 10 more or less");
  // decomp: func_800859B4 (src/BATTLE/BATTLE.PRG/146C.c:13010)
  table.Add("recovery_delay_bonus", 0x23, 1, false,
            "added to the recovery delay of each action the enemy takes but "
            "actions 22-53; the runtime keeps its low six bits");
  table.Add("movement_class", 0x24, 1, false,
            "movement/body-load selector, not equipment material; packed "
            "semantics remain under investigation",
            {}, "material");
  table.Add("walk_speed", 0x25, 1, false,
            "walking speed; the enemy walks at 9/10 of it");
  // decomp: func_800E6178 (src/BATTLE/BATTLE.PRG/7CD68.c:342) packs this
  // byte above movement_class in the AI's locomotion flags.
  table.Add("gait_class", 0x26, 1, false,
            "locomotion flags beside movement_class: 8 makes the AI's moves "
            "one run step long; scripted moves run, not walk, at 2 or 64 (to "
            "a tile), 4 or 128 (facing), 16 or 32 (the two flights); packed "
            "semantics remain under investigation");
  table.Add("run_speed", 0x27, 1, false,
            "running speed; the enemy runs at 9/10 of it");
  // decomp: applyStatus (src/BATTLE/BATTLE.PRG/146C.c:11129) masks the
  // status-rule bits with these.
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941) starts an enemy
  // immune to spell-immunity (bit 31) with that status.
  table.Flags("status_immunities", 0x28, 4,
              "statuses the enemy cannot receive; immunity to spell-immunity "
              "also starts it with that status",
              StatusFlags());
  // The AI settings word. The presets index tables loaded with the battle
  // program (Battle/Ai/ActorState.hpp), so the descriptions name them.
  // decomp: func_800E5A9C (src/BATTLE/BATTLE.PRG/7CD68.c:205) decodes it
  // (Battle/Ai/ActorSettings.hpp); no code reads bits 0-1, which every disc
  // template holds zero.
  // decomp: func_800E5A74 (src/BATTLE/BATTLE.PRG/7CD68.c:200) replaces bits
  // 21-31 with a room placement's saved flags when the placement sets them.
  table.Padding("ai_settings", 0x2C, 0x03);
  table.Bits("facing_delay_preset", 0x2C, 2, 2,
             "picks a scale from Battle::Ai::facingDelayScales: the delay "
             "before the enemy turns to face its target is the scale times 8 "
             "(times 2 above a collision radius of 128); at 0 it never turns "
             "to face it");
  // BATTLE.PRG's kWanderTimingPresets (move, rest, rate): (40, 5400, 6),
  // (40, 30, 6), (240, 60, 7), (90, 15, 10), (300, 300, 7), (0, 0, 0),
  // (70, 71, 72), (0, 1, 70); disc templates use 0-3 and 7.
  // decomp: func_800E1EAC (src/BATTLE/BATTLE.PRG/770A4.c:1092)
  table.Bits("wander_preset", 0x2C, 4, 3,
             "how it wanders while idle, from "
             "Battle::Ai::kWanderTimingPresets: it walks for the move period "
             "and rests for the rest period, counted in AI updates, and at "
             "the end of each has a 44 in 128 chance of repeating it; while "
             "walking, each update has a 1 in 2^rate chance of starting a new "
             "walk. 0: moves 40, rests 5400, rate 6 (hardly wanders); 1: 40, "
             "30, 6; 2: 240, 60, 7; 3: 90, 15, 10; 4: 300, 300, 7; 5 and 7 "
             "never walk; 6: 70, 71, 72 (read as rate 8)");
  table.Bits("approaches_target_point", 0x2C, 7, 1,
             "1: closing on a near target steps to the point where it stands "
             "rather than following the target");
  table.Bits("step_height_preset", 0x2D, 0, 2,
             "picks the highest rise or drop one step may take from "
             "Battle::Ai::stepHeights");
  table.Bits("tile_step_limit", 0x2D, 2, 2,
             "how many tiles (x plus z) one move step spans, which is also how "
             "far it reaches across a gap",
             kTileStepLimits);
  table.Bits("walks_to_waypoints", 0x2D, 4, 1,
             "1: in a room with an active waypoint, a best action whose "
             "candidate flags have bit 0 walks to the nearest active waypoint "
             "instead of moving in place, unless it runs the patrol combat "
             "behavior");
  table.Bits("chases_lost_target", 0x2D, 5, 1,
             "1: chases a lost target to its last tile instead of looking "
             "around for it");
  table.Bits("area_lock", 0x2D, 6, 1,
             "1: keeps its ground moves in its current area unless it is "
             "stuck");
  table.Bits("ignores_line_of_sight", 0x2D, 7, 1,
             "1: perceives actors in range and in view without a clear line "
             "to them at body height");
  table.Bits("view_angle_preset", 0x2E, 0, 2,
             "picks the field of view, in degrees, from "
             "Battle::Ai::viewAngles");
  table.Bits(
      "perception_range_preset", 0x2E, 2, 3,
      "picks the squared perception range from "
      "Battle::Ai::kPerceptionRangesSquared; the near range is the same, "
      "capped at 1536 squared");
  // decomp: func_800E7370 (src/BATTLE/BATTLE.PRG/7CD68.c:62) classifies the
  // room's actors by category; Ashley and category 3 are not combatants.
  table.Bits("relation_category", 0x2E, 5, 2,
             "how it holds the room's actors: hostile is hostile to Ashley "
             "and ashley-ally enemies and allied with the rest, neutral is "
             "neutral to all, passive is neutral to Ashley and ashley-ally "
             "enemies and allied with the rest, ashley-ally is allied with "
             "Ashley and ashley-ally enemies and hostile to the rest; a room "
             "placement can override it",
             kRelationCategories);
  table.Bits("decays_hostility", 0x2E, 7, 1,
             "1: an actor it holds hostile turns neutral once it has gone 121 "
             "perception updates unperceived; a room placement can override "
             "it");
  table.Bits("shares_ally_hostility", 0x2F, 0, 1,
             "1: takes on its allies' hostilities; a room placement can "
             "override it");
  table.Bits("hostile_on_harm", 0x2F, 1, 1,
             "1: turns hostile to an actor that harms it (only to Ashley when "
             "hostile_on_unprovoked_harm is also 1); a room placement can "
             "override it");
  table.Bits("hostile_on_unprovoked_harm", 0x2F, 2, 1,
             "1: turns hostile to an actor that harms it other than in return "
             "for its own earlier harm; a room placement can override it");
  table.Bits("can_ally_with_ashley", 0x2F, 3, 1,
             "1: a beneficial action from Ashley may make it her ally, and "
             "harm from Ashley ends the alliance; a room placement can "
             "override it");
  // decomp: func_800D8EF4 (src/BATTLE/BATTLE.PRG/6FC00.c:324)
  table.Bits("flee_rule", 0x2F, 4, 2,
             "when it flees; on-sight-of-hostile also flees while its flee "
             "timer runs, and any rule but never also flees while its action "
             "status is fleeing; a room placement can override it",
             kFleeRules);
  // BATTLE.PRG's kTerrainRejectMasks are 4, 5, 5, 7, tested against the AI
  // tile flags shifted down a byte. The room AI map sets 0x100 << the trap
  // record's state word (0 or 1; disc traps also hold 2), and 0x400 marks a
  // trap disabled once it has gone off or by DisableTrapAtPosition.
  // decomp: func_800E6F9C (src/BATTLE/BATTLE.PRG/7CD68.c:338)
  // decomp: func_800E68EC (src/BATTLE/BATTLE.PRG/7DDDC.s:226)
  // decomp: BATTLE.PRG func_80074314 (src/BATTLE/BATTLE.PRG/146C.c:4810)
  table.Bits("terrain_preset", 0x2F, 6, 2,
             "the floor traps it will not step on, from "
             "Battle::Ai::kTerrainRejectMasks: 0 avoids traps that have gone "
             "off or been disabled; 1 and 2 also avoid live traps whose room "
             "record holds state word 0; 3 also avoids those holding 1 (live "
             "traps holding any other value are walked on). A movement_class "
             "or gait_class with 64 or 128 keeps its existing mask instead; a "
             "room placement can override it");
  table.packed.push_back(
      {.legacy = "ai_settings",
       .offset = 0x2C,
       .size = 4,
       .is_signed = true,
       .columns = {
           "facing_delay_preset", "wander_preset", "approaches_target_point",
           "step_height_preset", "tile_step_limit", "walks_to_waypoints",
           "chases_lost_target", "area_lock", "ignores_line_of_sight",
           "view_angle_preset", "perception_range_preset", "relation_category",
           "decays_hostility", "shares_ally_hostility", "hostile_on_harm",
           "hostile_on_unprovoked_harm", "can_ally_with_ashley", "flee_rule",
           "terrain_preset"}});
  // decomp: func_80085008 (src/BATTLE/BATTLE.PRG/146C.c:12767)
  table.Add("mobility_limb_1", 0x30, 1, false,
            "a limb (1-5 in the limbs table; 0 for none) the enemy moves "
            "on: while no listed limb has 2 HP or more it is legs-impaired");
  table.Add("mobility_limb_2", 0x31, 1, false,
            "a second mobility limb, as mobility_limb_1");
  table.Add("default_target_limb", 0x32, 1, false,
            "the limb (0-5 in the limbs table) a hit aimed at the whole "
            "enemy strikes");
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941) and
  // applyAccessoryStats (src/BATTLE/BATTLE.PRG/146C.c:734) read neither this
  // byte nor the accessory's last two; every disc template holds zero there.
  table.Reserve("raw_33", 0x33);
  table.Add("weapon_material", 0x124, 1, false, "weapon material", kMaterials);
  // A drop rolls a number from 0 to 254 and drops when it is below the rate.
  // Each drop rolls on its own.
  // decomp: dropWeaponRand (src/BATTLE/BATTLE.PRG/146C.c:1251)
  // decomp: dropLoot (src/BATTLE/BATTLE.PRG/146C.c:1363)
  table.Add("weapon_drop_rate", 0x125, 1, false,
            "the chance that a defeat drops the weapon (blade, grip, gems "
            "and name): rate / 255, rolled apart from the other drops; 0 "
            "never, 16 about 6%, 128 about 50%, 255 always");
  // An equipment index is the 1-based inventory slot the player's equipment
  // came from. The apply*Stats copy each into the actor's stats, and only
  // vs_mainMenu_initInventory reads them, from the player's. Every disc
  // template holds 0 (not from the inventory) in each, as in its parts' and
  // limb armours' indices.
  // decomp: vs_battle_applyWeaponStats (src/BATTLE/BATTLE.PRG/146C.c:624)
  // decomp: vs_battle_applyShieldStats (src/BATTLE/BATTLE.PRG/146C.c:683)
  // decomp: vs_mainMenu_initInventory (src/MENU/MAINMENU.PRG/C48.c:51)
  table.Zeros("weapon_index", 0x126, 2);
  // vs_battle_applyWeaponStats gives the actor's weapon this name, and
  // dropWeaponRand gives it to the weapon a defeat drops.
  // decomp: dropWeaponRand (src/BATTLE/BATTLE.PRG/146C.c:1251)
  table.Text("weapon_name", 0x128,
             "the weapon's name, which a weapon a defeat drops keeps: text "
             "as name");
  table.Add("shield_material", 0x200, 1, false, "shield material", kMaterials);
  // decomp: dropShieldRand (src/BATTLE/BATTLE.PRG/146C.c:1275)
  table.Add("shield_drop_rate", 0x201, 1, false,
            "the chance that a defeat drops the shield with its gems: rate "
            "/ 255, as weapon_drop_rate");
  table.Zeros("shield_index", 0x202, 2);
  // decomp: dropAccessoryRand (src/BATTLE/BATTLE.PRG/146C.c:1295)
  table.Add("accessory_drop_rate", 0x234, 1, false,
            "the chance that a defeat drops the accessory: rate / 255, as "
            "weapon_drop_rate");
  table.Zeros("accessory_index", 0x235, 1);
  table.Reserve("raw_236", 0x236);
  table.Reserve("raw_237", 0x237);
  // The ZUD index picks the zone's ZUD entry the enemy loads. The zone holds
  // one entry per template, and every disc template picks its own, which the
  // row's archetype names; so the tables give it the template's slot.
  // decomp: loadZndEnemies (src/BATTLE/BATTLE.PRG/146C.c:14654)
  // decomp: func_800775C0 (src/BATTLE/BATTLE.PRG/146C.c:6393)
  table.Slot("zud_index", kZudIndexAt, kZudIndexBytes);
}

// Offsets from the limb's start.
void AddLimbFields(TableLayout& table) {
  table.Add("hp", 0x00, 2, false, "the limb's hit points");
  // The runtime limb's byte is signed (vs_UiEquipment_limb); version 7 wrote
  // it unsigned, so 251 was -5.
  // decomp: getAgilityDifference (src/BATTLE/BATTLE.PRG/146C.c:9876)
  // decomp: getIntelligenceDifference (src/BATTLE/BATTLE.PRG/146C.c:9927)
  table.Add("agility_defense_bonus", 0x02, 1, true,
            "signed; added to the enemy's agility (intelligence for "
            "Exorcism and Banish) when an action rolls to hit this limb, "
            "then scaled by (100 - the enemy's risk) percent: at 0 risk each "
            "point lowers the chance by 1, and a negative value makes the "
            "limb easier to hit. It counts for the hit functions "
            "agility-difference (attacks, break arts, most enemy abilities), "
            "agility-difference-2 (spells) and intelligence-difference; see "
            "ASSET_PIPELINE 4.7.1");
  table.resigned.push_back("agility_defense_bonus");
  // decomp: getChainEvasionModifier (src/BATTLE/BATTLE.PRG/146C.c:10042)
  table.Add("chain_evasion", 0x03, 1, false,
            "how well the limb evades Ashley's chain abilities (hit_function "
            "chain-evasion): they land (255 - value) * 100 / 255 percent of "
            "the time, plus her hidden 10, so 0 always lands, 128 lands 59% "
            "and 255 lands 10%");
  // Defense = (strength, or intelligence for a spell, plus the accessory's,
  // less 20) * (100 + the limb's type and affinity values + the accessory's
  // ratings) / 100, plus the shield and armour terms, all over risk / 2 + 100.
  // Types count for physical hits and typed spells.
  // decomp: getPhysicalAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11202)
  // decomp: getMagicAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11404)
  table.Run("type", 0x04, kTypeFields, 1, true,
            "signed; each point adds 1% to the defense the enemy's own "
            "strength (intelligence for spells) gives this limb against hits "
            "of this type");
  table.Run("affinity", 0x08, kAffinityFields, 1, true,
            "signed; each point adds 1% to the defense the enemy's own "
            "strength (intelligence for spells) gives this limb against hits "
            "of this affinity");
  // The affinities' tail byte, zero on the disc, as in the parts (see
  // AddPartFields): the limb's init copies only affinities 0-6.
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941)
  table.Tail("affinity_storage_7", "affinity_8", 0x0F);
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941) copies each
  // slot's id, sequence and index into the actor's stats; no code reads the
  // index, and every disc limb holds the slot's position there.
  // decomp: func_8007138C (src/BATTLE/BATTLE.PRG/146C.c:3618) loads the
  // sequence for actions 141-223.
  for (std::size_t action = 0; action < kLimbActionSlots; ++action) {
    const std::size_t at = 0x10 + (action * 4);
    table.Reference(table.Name(std::format("action_{}", action + 1)), at, 2,
                    kActionsTable, "the action in this slot; empty for none",
                    table.Name(std::format("action_{}_id", action + 1)));
    table.Add(table.Name(std::format("action_{}_special_sequence", action + 1)),
              at + 2, 1, false,
              "the special animation sequence (1-4, 0 for none) the enemy "
              "plays for a break art or special action (141-223); the "
              "runtime keeps its low four bits",
              {},
              table.Name(std::format("action_{}_effect_model", action + 1)));
    table.Constant(table.Name(std::format("action_{}_index", action + 1)),
                   at + 3, static_cast<std::uint8_t>(action));
  }
  table.Add("armor_material", 0x50, 1, false, "limb armor material",
            kMaterials);
  // Limbs with armour roll in limb order into the loot's two armour slots.
  // decomp: dropArmorRand (src/BATTLE/BATTLE.PRG/146C.c:1308)
  table.Add("armor_drop_rate", 0x51, 1, false,
            "the chance that a defeat drops this limb's armour: rate / 255, "
            "rolled for each limb with armour (0 never, 128 about 50%, 255 "
            "always). Only the first two pieces that drop, in limb order, "
            "are kept");
  // decomp: applyArmorStats (src/BATTLE/BATTLE.PRG/146C.c:759) copies this
  // byte to vs_UiArmor.unk99, which no code reads. Every disc limb holds 1
  // exactly when it has an armour material, as INITBTL.PRG's init of Ashley
  // does.
  table.Reserve("raw_52", 0x52, "armor_material");
  // An equipment index (AddEnemyFields), zero on the disc.
  // decomp: applyArmorStats (src/BATTLE/BATTLE.PRG/146C.c:759)
  table.Zeros("armor_index", 0x53, 1);
  // Actions of categories 5, 7 and 14 spread a hit on this limb by these
  // shares; categories 0-4 spread it evenly over the limbs with HP, and 6, 8,
  // 11 and 12 put it all on the limb hit. The enemy's own HP still loses the
  // whole hit once (func_80084B70); each share lowers only the limb's HP and
  // its armour's DP. Every disc value is 0-100, and the limb hit holds 100 in
  // 1946 of the rows with HP.
  // decomp: func_80080000 (src/BATTLE/BATTLE.PRG/146C.c:10547)
  // decomp: adjustDpPp (src/BATTLE/BATTLE.PRG/146C.c:10624)
  // decomp: func_80084B70 (src/BATTLE/BATTLE.PRG/146C.c:12641)
  table.Run("dp_splash", 0x54, kSplashFields, 1, false,
            "when an action aimed at one limb (categories direct-limb, "
            "direct-limb-including-user and direct-limb-or-object) hits this "
            "row's limb, the percent of the hit that this column's limb "
            "(limb_N) loses from its own HP and from its armour's DP. The "
            "enemy's HP still loses the hit once. The limb hit usually holds "
            "100, so other shares add limb damage on top; above 100 a limb "
            "loses more than the hit. Area actions aimed at limbs put the "
            "whole hit on the limb they strike, and actions aimed at the "
            "whole enemy split it evenly over the limbs with HP. limb_N "
            "numbers limbs as the limb column does");
  // decomp: vs_CharInitData (src/BATTLE/BATTLE.PRG/charInit.h:24-51):
  // LimbInit's unk5A padding, zero on the disc and read by no code.
  table.Reserve("raw_5a", 0x5A);
  table.Reserve("raw_5b", 0x5B);
}

// Offsets from the part's start (vs_battle_uiEquipment, 0x30 bytes).
// decomp: vs_battle_applyWeaponStats (src/BATTLE/BATTLE.PRG/146C.c:624)
// decomp: vs_battle_applyShieldStats (src/BATTLE/BATTLE.PRG/146C.c:683)
// decomp: applyArmorStats (src/BATTLE/BATTLE.PRG/146C.c:759)
void AddPartFields(TableLayout& table) {
  table.Add("item_id", 0x00, 2, false,
            "the global item id, which names the part");
  table.Add("sub_id", 0x02, 1, true,
            "the item's number within its own list, counted from 1: item_id "
            "for a blade, item_id - 95 for a grip, item_id - 126 for a "
            "shield, armour or accessory, item_id - 260 for a gem");
  table.Add("weapon_model_id", 0x03, 1, true,
            "a blade's or shield's WEP model (OBJ/<two hex digits>.WEP, "
            "converted to models/weapons/<two hex digits>.gltf; the file "
            "names the number in hex, so 22 is models/weapons/16.gltf); 0 "
            "for other parts");
  table.Add("category", 0x04, 1, false,
            "a blade's category (1 dagger .. 9 crossbow), which with a model "
            "also picks the enemy's attack animations; a grip's type (1-4); "
            "an armour piece's (1 shield, 2 helm, 3 breast armour, 4 "
            "leggings, 5 glove, 7 accessory); 0 for a gem");
  table.Add("strength", 0x05, 1, true,
            "strength the part adds; a weapon sums its blade, grip and gems, "
            "a shield its base and gems");
  table.Add("intelligence", 0x06, 1, true,
            "intelligence the part adds; summed as strength");
  table.Add("agility", 0x07, 1, true,
            "agility the part adds; summed as strength");
  table.Add("current_dp", 0x08, 2, false,
            "current DP (equal to max_dp on the disc)");
  table.Add("max_dp", 0x0A, 2, false,
            "maximum DP of a blade, shield or armour piece");
  table.Add("current_pp", 0x0C, 2, false, "current PP (zero on the disc)");
  table.Add("max_pp", 0x0E, 2, false, "maximum PP of a blade or shield");
  table.Add("damage_type", 0x10, 1, false, "the blade's damage type",
            kDamageTypes);
  table.Add("cost_type", 0x11, 1, false,
            "the resource the blade's base attack costs", kCostResources);
  table.Add("cost", 0x12, 1, false,
            "how much each of the blade's attacks costs");
  table.Add("material", 0x13, 1, false,
            "the part's material (zero on the disc: templates keep weapon, "
            "shield and armour materials in their own columns)",
            kMaterials);
  // The recovery timer gains (delay + bonus) * 256 and loses 35 a battle
  // update.
  // decomp: func_800859B4 (src/BATTLE/BATTLE.PRG/146C.c:13010)
  // decomp: func_80088554 (src/BATTLE/BATTLE.PRG/146C.c:14560)
  table.Add("recovery_delay", 0x14, 1, false,
            "the blade's recovery delay: after an action that takes the "
            "weapon's delay, the wielder waits (this + the enemy's "
            "recovery_delay_bonus) * 256 / 35 battle updates to act again");
  table.Add("gem_slots", 0x15, 1, false,
            "how many gems a grip or shield holds");
  // Hit-chance points add to or take from the hit's percent chance.
  // decomp: getAttackGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9702)
  // decomp: getDefenseGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9731)
  // decomp: func_8007FE5C (src/BATTLE/BATTLE.PRG/146C.c:10469)
  // decomp: func_800803A4 (src/BATTLE/BATTLE.PRG/146C.c:10656)
  table.Add("gem_effects", 0x16, 1, false,
            "a gem's effect (0 none). In a weapon: 1/2 +20 points of "
            "physical/spell hit chance, 3/4 +5% physical/spell damage. In a "
            "shield: 5/6 -20 points to physical/spell hits on the holder, 7/8 "
            "-5% physical/spell damage taken, and 11, 12, 13, 14, 16, 18, 19 "
            "and 21 -20 points to hits that apply strength-down, "
            "intelligence-down, agility-down, legs-impaired, silence, "
            "paralysis, poison and numbness. 9: a weapon gains 5% more PP a "
            "physical hit, a shield 5% less. Hit-chance gems need the weapon "
            "drawn");
  // The part's inventory slot, zero on the disc: an equipment index
  // (AddEnemyFields). Loot copies of parts skip it.
  // decomp: copyUiEquipmentStats (src/BATTLE/BATTLE.PRG/146C.c:1156)
  table.Zeros("index", 0x17, 1);
  // Extents are 32 world units a step (a room tile is 128): x and z are
  // horizontal radii, half-widths or half-diagonals; y is the vertical radius
  // of an ellipsoid or centered cylinder and the height above the origin of
  // the other shapes. A direct attack keeps the action's range_shape and, for
  // range_x 255, takes these extents and angle; each equip sets the actor's
  // own shape to ellipsoid.
  // decomp: func_80071F68 (src/BATTLE/BATTLE.PRG/146C.c:3898)
  // decomp: func_800C1564 (src/BATTLE/BATTLE.PRG/58578.c:360)
  table.Add("range", 0x18, 1, false,
            "the blade's horizontal x extent, 32 world units a step; the "
            "enemy's reach is added to it and to the other two extents");
  table.Add("range_y", 0x19, 1, false,
            "the blade's vertical extent, 32 world units a step (the "
            "vertical radius of the ellipsoid the base attacks use)");
  table.Add("range_z", 0x1A, 1, false,
            "the blade's horizontal z extent, 32 world units a step");
  // decomp: vs_Range (src/SLUS_010.40/inventory.h:4)
  table.Bits("attack_shape", 0x1B, 0, 3,
             "the volume shape stored with the blade; targeting ignores it "
             "(an attack takes the action's range_shape, ellipsoid for the "
             "base attacks), and every disc blade holds ellipsoid",
             kVolumeShapes);
  table.Bits("attack_angle", 0x1B, 3, 5,
             "the attack volume's pitch, in 11.25-degree steps (1/32 turn)");
  table.packed.push_back({.legacy = "shape_bits",
                          .offset = 0x1B,
                          .size = 1,
                          .is_signed = false,
                          .columns = {"attack_shape", "attack_angle"}});
  // Attack = stat * (100 + (weapon affinity + grip type + weapon class - DP% +
  // PP bonus + accessory ratings) / 4) / 100: 4 points are 1%. In defense
  // (AddLimbFields) armour and shield values scale the piece's own strength
  // (intelligence for spells), the accessory's the wearer's; the shield
  // counts while the weapon is drawn. Classes count for physical hits only.
  // decomp: getPhysicalAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11202)
  // decomp: getMagicAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11404)
  table.Run("type", 0x1C, kTypeFields, 1, true,
            "signed rating against hits of this type: a grip's, for its "
            "blade's type, and an accessory's add 1% attack per 4 points; "
            "armour's, a shield's and an accessory's add 1% defense per "
            "point");
  table.Run("class", 0x20, kClassFields, 1, true,
            "signed rating against the opponent's class, physical hits "
            "only: a weapon's and an accessory's add 1% attack per 4 points; "
            "armour's, a shield's and an accessory's add 1% defense per "
            "point");
  table.Run("affinity", 0x28, kAffinityFields, 1, true,
            "signed rating for hits of this affinity (a plain attack takes "
            "the weapon's highest): a weapon's and an accessory's add 1% "
            "attack per 4 points; armour's, a shield's and an accessory's "
            "add 1% defense per point");
  // The class array's last two bytes and the affinity array's last, zero in
  // every disc template, are tails no code reads. The apply*Stats copy
  // classes 0-5 and affinities 0-6 into the actor's stats, as loot copies do
  // into the inventory; damage indexes classes by a 0-5 enemy class and
  // affinities by an action's 3-bit affinity less one, and the menus show,
  // sort and combine classes 0-5 and affinities 0-6.
  // decomp: vs_battle_applyWeaponStats (src/BATTLE/BATTLE.PRG/146C.c:624)
  // decomp: vs_battle_copyUiBladeStats (src/BATTLE/BATTLE.PRG/146C.c:1084)
  // decomp: setClassAffinities (src/MENU/MENUC.PRG/168.c:4763)
  table.Tail("class_storage_6", "class_7", 0x26);
  table.Tail("class_storage_7", "class_8", 0x27);
  table.Tail("affinity_storage_7", "affinity_8", 0x2F);
}

ColumnSpec IdColumn(std::string_view name, std::string_view description) {
  return {.name = name,
          .type = ColumnType::kId,
          .minimum = 0,
          .maximum = 0,
          .references = {},
          .description = description};
}

ColumnSpec ReferenceColumn(std::string_view name, std::string_view table,
                           std::string_view description) {
  return {.name = name,
          .type = ColumnType::kReference,
          .minimum = 0,
          .maximum = 0,
          .references = table,
          .description = description};
}

ColumnSpec IntegerColumn(std::string_view name, std::int64_t minimum,
                         std::int64_t maximum, std::string_view description) {
  return {.name = name,
          .type = ColumnType::kInteger,
          .minimum = minimum,
          .maximum = maximum,
          .references = {},
          .description = description};
}

// The enemy kinds: no record holds them; a template names one (its kind).
// decomp: updateEnemyKills (src/BATTLE/BATTLE.PRG/146C.c:1414)
void BuildKindLayout(TableLayout& kinds) {
  kinds.Lead(IdColumn("id",
                      "the kind, named after its first named template "
                      "(enemy-kind.zombie-3)"));
  kinds.Lead(IntegerColumn("legacy_id", 0, kMaxKind,
                           "the number the game stores (the actor's "
                           "sub_type), which the bestiary's kill bits and "
                           "the orbiting boss AI (172 and 173) test"));
  kinds.key = {"id"};
  kinds.Finish("enemy-kinds",
               "The enemy kinds zone templates name; each kind's number is "
               "the actor's sub_type.",
               kEnemyDirectory);
}

// Fills `layouts` in place: their specs point into their own columns.
void BuildLayouts(std::array<TableLayout, kZoneTableCount>& layouts) {
  TableLayout& zones = layouts.at(static_cast<std::size_t>(ZoneTable::kZones));
  zones.Lead(IdColumn("id", "the zone, from its file (zone.map.zone009)"));
  zones.Lead(IntegerColumn("music_id", std::numeric_limits<std::int32_t>::min(),
                           std::numeric_limits<std::int32_t>::max(),
                           "the music the zone plays"));
  zones.Lead(IntegerColumn("room_slots", 0, kMaxRoomSlots,
                           "the room table's length, used slots or not"));
  zones.key = {"id"};
  zones.Finish("zones", "Zones (MAP/ZONE*.ZND): music and room slots.",
               kWorldDirectory);

  TableLayout& rooms = layouts.at(static_cast<std::size_t>(ZoneTable::kRooms));
  rooms.Lead(ReferenceColumn("zone", "zones", "the zone"));
  rooms.Lead(IntegerColumn("slot", 0, kMaxRoomSlots - 1,
                           "the slot in the zone's room table"));
  rooms.Lead(IdColumn("room", "the room file (room.map.map009)"));
  rooms.key = {"zone", "slot"};
  rooms.Finish("zone-rooms",
               "The rooms in each zone's room table; unlisted slots are "
               "empty.",
               kWorldDirectory);

  TableLayout& textures =
      layouts.at(static_cast<std::size_t>(ZoneTable::kTextures));
  textures.Lead(ReferenceColumn("zone", "zones", "the zone"));
  textures.Lead(
      IntegerColumn("index", 0, kMaxTextures - 1, "the position in the table"));
  textures.Lead(IdColumn("image", "the converted image"));
  textures.key = {"zone", "index"};
  textures.Finish("zone-textures",
                  "The images in each zone's texture table, in upload order.",
                  kWorldDirectory);

  TableLayout& enemies =
      layouts.at(static_cast<std::size_t>(ZoneTable::kEnemies));
  enemies.Lead(IdColumn("id", "the template (enemy.map.zone009.00)"));
  enemies.Lead(ReferenceColumn("zone", "zones", "the zone"));
  enemies.Lead(IntegerColumn(kSlotColumn, 0, kMaxEnemies - 1,
                             "the template's position"));
  enemies.Lead(IdColumn("archetype", "the ZUD the template loads"));
  AddEnemyFields(enemies);
  enemies.key = {"id"};
  enemies.Finish("zone-enemies",
                 "Each zone's enemy templates (vs_CharInitData): the record's "
                 "fields but its limbs and equipment parts.",
                 kEnemyDirectory);

  TableLayout& limbs = layouts.at(static_cast<std::size_t>(ZoneTable::kLimbs));
  limbs.Lead(ReferenceColumn("enemy", "zone-enemies", "the template"));
  // The spawn names a limb from the body model's bone whose low limb nibble
  // is the limb plus 1: that bone's tag indexes vs_battle_limbNames.
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941)
  limbs.Lead(IntegerColumn(
      "limb", 0, kLimbCount - 1,
      "the limb's number, which mobility_limb_1, default_target_limb and "
      "the dp_splash columns use; the enemy's body model tags the bone that "
      "names it (HEAD, R.ARM, BODY and so on)"));
  AddLimbFields(limbs);
  limbs.key = {"enemy", "limb"};
  limbs.Finish("zone-enemy-limbs",
               "Each template's limbs but their armour parts; a limb with "
               "no row has zero in every column.",
               kEnemyDirectory);

  TableLayout& parts = layouts.at(static_cast<std::size_t>(ZoneTable::kParts));
  parts.Lead(ReferenceColumn("enemy", "zone-enemies", "the template"));
  parts.Lead(IdColumn("part",
                      "weapon.blade, weapon.grip, weapon.gem1-3, "
                      "shield.base, shield.gem1-3, accessory or "
                      "limb0-5.armor"));
  AddPartFields(parts);
  parts.key = {"enemy", "part"};
  parts.Finish("zone-enemy-parts",
               "Each template's equipment parts; a part with no row is all "
               "zeros.",
               kEnemyDirectory);

  BuildKindLayout(layouts.at(static_cast<std::size_t>(ZoneTable::kKinds)));
}

const std::array<TableLayout, kZoneTableCount>& Layouts() {
  static const std::unique_ptr<const std::array<TableLayout, kZoneTableCount>>
      layouts = [] {
        auto built =
            std::make_unique<std::array<TableLayout, kZoneTableCount>>();
        BuildLayouts(*built);
        return built;
      }();
  return *layouts;
}

const TableLayout& LayoutOf(ZoneTable table) {
  return Layouts().at(static_cast<std::size_t>(table));
}

// --- Field values

std::int64_t ReadField(std::span<const std::byte> record, const Field& field) {
  if (field.bits != 0) {
    const auto byte = std::to_integer<unsigned>(record[field.offset]);
    return (byte >> field.bit) & ((1U << field.bits) - 1);
  }
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < field.size; ++index) {
    value |= std::to_integer<std::uint64_t>(record[field.offset + index])
             << (kByteBits * index);
  }
  if (field.is_signed) {
    const std::uint32_t width = static_cast<std::uint32_t>(field.size) * 8;
    const std::uint64_t sign = std::uint64_t{1} << (width - 1);
    return static_cast<std::int64_t>(value ^ sign) -
           static_cast<std::int64_t>(sign);
  }
  return static_cast<std::int64_t>(value);
}

void WriteField(std::span<std::byte> record, const Field& field,
                std::int64_t value) {
  if (field.bits != 0) {
    const unsigned mask = ((1U << field.bits) - 1) << field.bit;
    const unsigned byte = std::to_integer<unsigned>(record[field.offset]);
    record[field.offset] = static_cast<std::byte>(
        (byte & ~mask) | ((static_cast<unsigned>(value) << field.bit) & mask));
    return;
  }
  const auto bits = static_cast<std::uint64_t>(value);
  for (std::size_t index = 0; index < field.size; ++index) {
    record[field.offset + index] =
        static_cast<std::byte>((bits >> (kByteBits * index)) & kByteMask);
  }
}

// The markup's units: a {{ or }} brace, a {...} token or one byte. An
// unclosed token runs to the end, for the codec to refuse.
std::vector<std::string_view> MarkupUnits(std::string_view markup) {
  std::vector<std::string_view> units;
  while (!markup.empty()) {
    std::size_t size = 1;
    if (markup.starts_with("{{") || markup.starts_with("}}")) {
      size = 2;
    } else if (markup.front() == '{') {
      size = std::min(markup.find('}'), markup.size() - 1) + 1;
    }
    units.push_back(markup.substr(0, size));
    markup.remove_prefix(size);
  }
  return units;
}

// The authoring text of name markup: each word gap between two words as a
// space.
std::string NameTextOf(std::string_view markup) {
  const auto units = MarkupUnits(markup);
  std::size_t first = units.size();
  std::size_t last = 0;
  for (std::size_t index = 0; index < units.size(); ++index) {
    if (units[index] == kWordGap) continue;
    first = std::min(first, index);
    last = index;
  }
  std::string text;
  for (std::size_t index = 0; index < units.size(); ++index) {
    const bool between = index > first && index < last;
    text += between && units[index] == kWordGap ? kSpace : units[index];
  }
  return text;
}

unsigned HexDigit(char digit) {
  return digit <= '9' ? static_cast<unsigned>(digit - '0')
                      : static_cast<unsigned>(digit - 'a') + 10;
}

// `name` with each Latin-1 letter (UTF-8 C3 80-BF) as its base ASCII letter;
// the two signs among them (multiplication and division) become a space.
std::string FoldLatin1(std::string_view name) {
  constexpr std::string_view kBaseLetters =
      "AAAAAAACEEEEIIIIDNOOOOO OUUUUY s"
      "aaaaaaaceeeeiiiidnooooo ouuuuy y";
  constexpr unsigned char kLatin1Lead = 0xC3;
  constexpr std::size_t kFirstTrail = 0x80;
  std::string folded;
  for (std::size_t at = 0; at < name.size(); ++at) {
    if (static_cast<unsigned char>(name[at]) == kLatin1Lead &&
        at + 1 < name.size()) {
      const std::size_t trail = static_cast<unsigned char>(name[at + 1]);
      if (trail >= kFirstTrail && trail - kFirstTrail < kBaseLetters.size()) {
        folded += kBaseLetters[trail - kFirstTrail];
        ++at;
        continue;
      }
    }
    folded += name[at];
  }
  return folded;
}

// The slug of a kind's name; "untitled" for none.
std::string KindSlug(std::string_view name) {
  auto slug = ActionSlug(FoldLatin1(name));
  if (slug.empty()) slug = kUntitled;
  return slug;
}

// Whether `slug`'s last dash-separated word is a number, as a numbered id's
// is.
bool EndsInNumber(std::string_view slug) {
  const auto word = slug.substr(slug.rfind('-') + 1);
  return !word.empty() && std::ranges::all_of(word, [](char glyph) {
    return glyph >= '0' && glyph <= '9';
  });
}

// The enemy kinds' ids by number, and numbers by id. Built in place, never
// moved: the maps' moves may allocate.
class KindNames {
 public:
  KindNames() = default;

  // The kinds `names` (each kind's name by number) holds, by the ids
  // EnemyKindIds gives them.
  explicit KindNames(const std::map<std::int64_t, std::string>& names) {
    for (auto& [legacy, id] : EnemyKindIds(names)) {
      Add(legacy, std::move(id));  // EnemyKindIds' ids are distinct
    }
  }

  KindNames(const KindNames&) = delete;
  KindNames& operator=(const KindNames&) = delete;
  KindNames(KindNames&&) = delete;
  KindNames& operator=(KindNames&&) = delete;
  ~KindNames() = default;

  // Adds kind `legacy` as `id`; false when either is taken.
  bool Add(std::int64_t legacy, std::string id) {
    if (ids_.contains(legacy) || legacy_.contains(id)) return false;
    legacy_.emplace(id, legacy);
    ids_.emplace(legacy, std::move(id));
    return true;
  }

  [[nodiscard]] std::optional<std::string_view> IdOf(
      std::int64_t legacy) const {
    const auto found = ids_.find(legacy);
    if (found == ids_.end()) return std::nullopt;
    return found->second;
  }

  [[nodiscard]] std::optional<std::int64_t> LegacyOf(
      std::string_view id) const {
    const auto found = legacy_.find(id);
    if (found == legacy_.end()) return std::nullopt;
    return found->second;
  }

  // The kinds table's rows (id, legacy_id), by number.
  [[nodiscard]] std::vector<std::vector<std::string>> Rows() const {
    std::vector<std::vector<std::string>> rows;
    for (const auto& [legacy, id] : ids_) {
      rows.push_back({id, std::format("{}", legacy)});
    }
    return rows;
  }

 private:
  std::map<std::int64_t, std::string> ids_;
  std::map<std::string, std::int64_t, std::less<>> legacy_;
};

// Records `name` as kind `kind`'s unless an earlier template of it was named.
void NameKind(std::map<std::int64_t, std::string>& names, std::int64_t kind,
              std::string name) {
  auto [found, added] = names.try_emplace(kind, name);
  if (!added && found->second.empty()) found->second = std::move(name);
}

// The action catalog's ids by original index, read through the reader the
// first time a limb names an action, so tables whose limbs name none need no
// catalog. It reads the base catalogs/battle/actions.csv, not catalog layers.
class ActionNames {
 public:
  explicit ActionNames(const ZoneTableReader& read) : read_(&read) {}

  std::expected<void, std::string> Load() {
    if (loaded_) return {};
    if (!*read_) {
      return std::unexpected(
          std::format("naming limb actions needs {}", kActionsPath));
    }
    auto text = (*read_)(kActionsPath);
    if (!text) {
      return std::unexpected(
          std::format("naming limb actions: {}", text.error()));
    }
    auto rows = ReadTable(ActionsSpec(), *text, kActionsPath);
    if (!rows) return std::unexpected(rows.error());
    const auto columns = ActionsSpec().columns;
    const auto column = [&columns](std::string_view name) {
      return static_cast<std::size_t>(
          std::ranges::find(columns, name, &ColumnSpec::name) -
          columns.begin());
    };
    const std::size_t id = column("id");
    const std::size_t legacy = column("legacy_id");
    for (const TableRow& row : *rows) {
      const std::int64_t index = row.values.at(legacy);
      std::string& name = ids_.at(static_cast<std::size_t>(index));
      if (!name.empty()) {
        return std::unexpected(
            CellError(row.source, row.line, "legacy_id",
                      std::format("{} also names action {}", name, index)));
      }
      name = row.cells.at(id);
      legacy_.emplace(name, index);
    }
    loaded_ = true;
    return {};
  }

  // After Load: the id of action `legacy`, or nothing.
  [[nodiscard]] std::optional<std::string_view> IdOf(
      std::int64_t legacy) const {
    if (legacy < 0 || legacy >= static_cast<std::int64_t>(kActionCount) ||
        ids_.at(static_cast<std::size_t>(legacy)).empty()) {
      return std::nullopt;
    }
    return ids_.at(static_cast<std::size_t>(legacy));
  }

  // After Load: the original index of action `id`, or nothing.
  [[nodiscard]] std::optional<std::int64_t> LegacyOf(
      std::string_view id) const {
    const auto found = legacy_.find(id);
    if (found == legacy_.end()) return std::nullopt;
    return found->second;
  }

 private:
  const ZoneTableReader* read_;
  bool loaded_ = false;
  std::array<std::string, kActionCount> ids_;
  std::map<std::string, std::int64_t, std::less<>> legacy_;
};

std::string UnheldAction(std::int64_t value) {
  return std::format("names action {}, which {} does not hold", value,
                     kActionsPath);
}

// The tables a field reference names.
struct ZoneReferences {
  ActionNames& actions;
  const KindNames& kinds;
};

// A reference field's cell: the id of the row `value` names, empty for an
// optional reference's 0.
std::expected<std::string, std::string> ReferenceCell(const ColumnSpec& column,
                                                      std::int64_t value,
                                                      ZoneReferences refs) {
  if (value == 0 && column.optional) return std::string{};
  if (column.references == kKindsTable) {
    const auto id = refs.kinds.IdOf(value);
    if (!id) {
      return std::unexpected(
          std::format("{} names kind {}, which {} does not hold", column.name,
                      value, ZoneTablePath(ZoneTable::kKinds)));
    }
    return std::string(*id);
  }
  if (auto loaded = refs.actions.Load(); !loaded) {
    return std::unexpected(loaded.error());
  }
  const auto id = refs.actions.IdOf(value);
  if (!id) {
    return std::unexpected(
        std::format("{} {}", column.name, UnheldAction(value)));
  }
  return std::string(*id);
}

// The number of the row a reference cell of `row` names; 0 for an empty
// cell.
std::expected<std::int64_t, std::string> ReferenceValue(
    const TableRow& row, const ColumnSpec& column, const std::string& cell,
    ZoneReferences refs) {
  if (cell.empty()) return 0;
  if (column.references == kKindsTable) {
    const auto legacy = refs.kinds.LegacyOf(cell);
    if (!legacy) {
      return std::unexpected(
          CellError(row.source, row.line, column.name,
                    std::format("no kind '{}' in {}", cell,
                                ZoneTablePath(ZoneTable::kKinds))));
    }
    return *legacy;
  }
  if (auto loaded = refs.actions.Load(); !loaded) {
    return std::unexpected(loaded.error());
  }
  const auto legacy = refs.actions.LegacyOf(cell);
  if (!legacy) {
    return std::unexpected(
        CellError(row.source, row.line, column.name,
                  std::format("no action '{}' in {}", cell, kActionsPath)));
  }
  return *legacy;
}

// The record's fields as cells, in the layout's field order; a reference as
// the id of the row it names, empty for none.
std::expected<void, std::string> AppendCells(std::vector<std::string>& cells,
                                             const TableLayout& layout,
                                             std::span<const std::byte> record,
                                             ZoneReferences refs) {
  for (const Field& field : layout.fields) {
    if (field.text) {
      const auto& column = layout.columns.at(cells.size());
      auto text =
          ZoneNameText(record.subspan(field.offset, field.size), column.name);
      if (!text) return std::unexpected(text.error());
      cells.push_back(std::move(*text));
      continue;
    }
    const auto value = ReadField(record, field);
    const auto& column = layout.columns.at(cells.size());
    if (column.type == ColumnType::kReference) {
      auto cell = ReferenceCell(column, value, refs);
      if (!cell) return std::unexpected(cell.error());
      cells.push_back(std::move(*cell));
      continue;
    }
    if (column.type == ColumnType::kFlags) {
      cells.push_back(
          FormatFlags(column.enumeration, static_cast<std::uint32_t>(value)));
      continue;
    }
    const auto named =
        std::ranges::find(column.enumeration, value, &EnumValue::code);
    cells.push_back(named == column.enumeration.end()
                        ? std::format("{}", value)
                        : std::string(named->name));
  }
  return {};
}

// Writes a row's field cells (after the lead columns) into `record`.
std::expected<void, std::string> ApplyRow(std::span<std::byte> record,
                                          const TableLayout& layout,
                                          const TableRow& row,
                                          ZoneReferences refs) {
  for (std::size_t index = 0; index < layout.fields.size(); ++index) {
    const Field& field = layout.fields[index];
    const std::size_t column =
        layout.columns.size() - layout.fields.size() + index;
    const std::string& cell = row.cells.at(column);
    if (field.text) {
      const auto name = EncodeZoneName(cell, std::format("'{}'", cell));
      if (!name) {
        return std::unexpected(CellError(row.source, row.line,
                                         layout.columns.at(column).name,
                                         name.error()));
      }
      std::ranges::copy(*name, record.subspan(field.offset).begin());
    } else if (layout.columns.at(column).type == ColumnType::kReference) {
      const auto value =
          ReferenceValue(row, layout.columns.at(column), cell, refs);
      if (!value) return std::unexpected(value.error());
      WriteField(record, field, *value);
    } else {
      WriteField(record, field, row.values.at(column));
    }
  }
  return {};
}

bool AllZero(std::span<const std::byte> bytes) {
  return std::ranges::all_of(
      bytes, [](std::byte value) { return value == std::byte{}; });
}

std::span<const std::byte> LimbOf(const ZndTemplate& data, std::size_t limb) {
  return std::span(data).subspan(kLimbsAt + (limb * kLimbBytes), kLimbBytes);
}

// The limb's bytes its row holds: all but its armour part and the bytes the
// tables derive.
bool LimbFieldsZero(std::span<const std::byte> limb) {
  const auto& reserved = LayoutOf(ZoneTable::kLimbs).reserved;
  for (std::size_t at = 0; at < limb.size(); ++at) {
    if (at >= kLimbArmorAt && at < kLimbArmorAt + kPartBytes) continue;
    if (std::ranges::find(reserved, at, &ReservedByte::offset) !=
        reserved.end()) {
      continue;
    }
    if (limb[at] != std::byte{}) return false;
  }
  return true;
}

std::optional<std::size_t> PartOffset(std::string_view id) {
  const auto found = std::ranges::find(kPartSlots, id, &PartSlot::id);
  if (found == kPartSlots.end()) return std::nullopt;
  return found->offset;
}

// The value the tables give a reserved byte of `record`, a record of the
// template in `slot`.
unsigned ReservedValue(const TableLayout& layout, const ReservedByte& reserved,
                       std::span<const std::byte> record, std::size_t slot) {
  if (reserved.slot_byte) {
    return static_cast<unsigned>(slot >> (kByteBits * *reserved.slot_byte)) &
           kByteMask;
  }
  if (reserved.set_by.empty()) return reserved.constant.value_or(0);
  const auto field =
      std::ranges::find(layout.fields, reserved.set_by, &Field::column);
  return field != layout.fields.end() && ReadField(record, *field) != 0 ? 1U
                                                                        : 0U;
}

// Each reserved byte of a template's record, its limbs and its equipment
// parts, with the record it belongs to and that record's offset in the
// template.
template <typename Data, typename Visit>
void ForEachReserved(Data& data, Visit visit) {
  const TableLayout& enemy = LayoutOf(ZoneTable::kEnemies);
  for (const ReservedByte& reserved : enemy.reserved) {
    visit(enemy, reserved, std::span(data), std::size_t{0});
  }
  const TableLayout& limbs = LayoutOf(ZoneTable::kLimbs);
  for (std::size_t limb = 0; limb < static_cast<std::size_t>(kLimbCount);
       ++limb) {
    const std::size_t base = kLimbsAt + (limb * kLimbBytes);
    for (const ReservedByte& reserved : limbs.reserved) {
      visit(limbs, reserved, std::span(data).subspan(base, kLimbBytes), base);
    }
  }
  const TableLayout& parts = LayoutOf(ZoneTable::kParts);
  for (const PartSlot& part : kPartSlots) {
    for (const ReservedByte& reserved : parts.reserved) {
      visit(parts, reserved, std::span(data).subspan(part.offset, kPartBytes),
            part.offset);
    }
  }
}

void DeriveReservedBytes(ZndTemplate& data, std::size_t slot) {
  ForEachReserved(
      data, [slot](const TableLayout& layout, const ReservedByte& reserved,
                   std::span<std::byte> record, std::size_t /*base*/) {
        const unsigned mask = reserved.mask;
        const auto kept =
            std::to_integer<unsigned>(record[reserved.offset]) & ~mask;
        record[reserved.offset] = static_cast<std::byte>(
            kept | (ReservedValue(layout, reserved, record, slot) & mask));
      });
}

// --- Reading

struct ReadTables {
  std::array<std::vector<TableRow>, kZoneTableCount> rows;
  std::array<std::string, kZoneTableCount> paths;

  const std::vector<TableRow>& Rows(ZoneTable table) const {
    return rows.at(static_cast<std::size_t>(table));
  }
  const std::string& Path(ZoneTable table) const {
    return paths.at(static_cast<std::size_t>(table));
  }
};

std::expected<ReadTables, std::string> ReadAll(const ZoneTableReader& read) {
  ReadTables tables;
  for (const ZoneTable table : kZoneTables) {
    const std::string path = ZoneTablePath(table);
    auto text = read(path);
    if (!text) return std::unexpected(text.error());
    auto rows = ReadTable(ZoneTableSpec(table), *text, path);
    if (!rows) return std::unexpected(rows.error());
    tables.rows.at(static_cast<std::size_t>(table)) = std::move(*rows);
    tables.paths.at(static_cast<std::size_t>(table)) = path;
  }
  return tables;
}

// Adds the kinds the kinds table holds to `kinds`; no two may share a number.
std::expected<void, std::string> ReadKinds(const ReadTables& tables,
                                           KindNames& kinds) {
  for (const TableRow& row : tables.Rows(ZoneTable::kKinds)) {
    const std::int64_t legacy = row.values.at(1);
    if (const auto taken = kinds.IdOf(legacy)) {
      return std::unexpected(
          CellError(tables.Path(ZoneTable::kKinds), row.line, "legacy_id",
                    std::format("{} also names kind {}", *taken, legacy)));
    }
    kinds.Add(legacy, row.cells.at(0));  // ReadTable refuses a repeated id
  }
  return {};
}

const Field& FieldOf(const TableLayout& layout, std::string_view column) {
  return *std::ranges::find(layout.fields, column, &Field::column);
}

// Each kind the zones' templates hold, by number, with the first name a
// template of it holds in table order (empty for none).
std::expected<std::map<std::int64_t, std::string>, std::string> KindNamesOf(
    std::span<const ZoneRecord* const> zones) {
  const TableLayout& layout = LayoutOf(ZoneTable::kEnemies);
  const Field& kind = FieldOf(layout, kKindColumn);
  const Field& name = FieldOf(layout, "name");
  std::map<std::int64_t, std::string> names;
  for (const ZoneRecord* zone : zones) {
    for (const ZoneEnemyRecord& enemy : zone->enemies) {
      auto text = ZoneNameText(
          std::span(enemy.data).subspan(name.offset, name.size), enemy.id);
      if (!text) return std::unexpected(text.error());
      NameKind(names, ReadField(enemy.data, kind), std::move(*text));
    }
  }
  return names;
}

// Entries keyed by position must run from 0 with no gap.
template <typename Value>
std::expected<std::vector<Value>, std::string> Dense(
    const std::map<std::int64_t, Value>& entries, std::string_view what,
    std::string_view zone) {
  std::vector<Value> values;
  for (const auto& [position, value] : entries) {
    if (position != static_cast<std::int64_t>(values.size())) {
      return std::unexpected(
          std::format("{} has no {} {}", zone, what, values.size()));
    }
    values.push_back(value);
  }
  return values;
}

// A packed cell's value as its replacement columns' cells, in their order.
std::expected<std::vector<std::string>, std::string> SplitCell(
    const TableLayout& layout, const PackedColumn& packed,
    std::string_view cell, std::string_view file, std::size_t line) {
  std::int64_t value = 0;
  const char* const first = cell.data();
  const auto parsed = std::from_chars(first, first + cell.size(), value);
  if (cell.empty() || parsed.ec != std::errc() ||
      parsed.ptr != first + cell.size() || (value < 0 && !packed.is_signed)) {
    return std::unexpected(
        CellError(file, line, packed.legacy,
                  std::format("'{}' is not a decimal integer", cell)));
  }
  const auto width = static_cast<std::uint32_t>(packed.size) * kByteBits;
  const std::int64_t span = std::int64_t{1} << width;
  const std::int64_t lowest = packed.is_signed ? -(span / 2) : 0;
  if (value < lowest || value >= span) {
    return std::unexpected(CellError(
        file, line, packed.legacy,
        std::format("{} does not fit the columns that replace it", value)));
  }
  const auto bits = static_cast<std::uint64_t>(value) &
                    (static_cast<std::uint64_t>(span) - 1);
  std::vector<std::string> cells;
  std::uint64_t rebuilt = 0;
  for (const std::string_view column : packed.columns) {
    const auto field = std::ranges::find(layout.fields, column, &Field::column);
    if (field == layout.fields.end() || field->bits == 0 ||
        field->offset < packed.offset ||
        field->offset >= packed.offset + packed.size) {
      return std::unexpected(std::format("{}: '{}' is not a bit field of '{}'",
                                         file, column, packed.legacy));
    }
    const auto shift =
        static_cast<std::uint32_t>(field->offset - packed.offset) * kByteBits +
        field->bit;
    const std::uint64_t part =
        (bits >> shift) & ((std::uint64_t{1} << field->bits) - 1);
    rebuilt |= part << shift;
    cells.push_back(std::to_string(part));
  }
  if (rebuilt != bits) {
    return std::unexpected(
        CellError(file, line, packed.legacy,
                  std::format("{} does not fit the columns that replace it: "
                              "bits 0x{:X} have no column",
                              value, bits & ~rebuilt)));
  }
  return cells;
}

// Every record must have the header's fields.
std::expected<void, std::string> CheckWidths(
    const std::vector<CsvRecord>& records, std::string_view file) {
  for (const CsvRecord& record : records) {
    if (record.fields.size() != records.front().fields.size()) {
      return std::unexpected(std::format(
          "{}:{}: has {} fields; the header has {}", file, record.line,
          record.fields.size(), records.front().fields.size()));
    }
  }
  return {};
}

// Replaces `packed`'s header field and cells with its replacement columns.
std::expected<void, std::string> SplitColumn(const TableLayout& layout,
                                             const PackedColumn& packed,
                                             std::vector<CsvRecord>& records,
                                             std::size_t at,
                                             std::string_view file) {
  if (auto widths = CheckWidths(records, file); !widths) {
    return widths;
  }
  const auto position = static_cast<std::ptrdiff_t>(at);
  std::vector<std::string> header(packed.columns.begin(), packed.columns.end());
  auto& names = records.front().fields;
  names.erase(names.begin() + position);
  names.insert(names.begin() + position, header.begin(), header.end());
  for (std::size_t row = 1; row < records.size(); ++row) {
    auto& fields = records[row].fields;
    auto cells = SplitCell(layout, packed, fields[at], file, records[row].line);
    if (!cells) return std::unexpected(cells.error());
    fields.erase(fields.begin() + position);
    fields.insert(fields.begin() + position, cells->begin(), cells->end());
  }
  return {};
}

// A flags cell written as a number (in either the signed or the unsigned form
// of its bytes) as its member names; nothing for a cell that is not a number,
// which the schema then reads as names.
std::expected<std::optional<std::string>, std::string> NamedFlagsCell(
    const ColumnSpec& column, std::string_view cell, std::string_view file,
    std::size_t line) {
  std::int64_t value = 0;
  const char* const first = cell.data();
  const auto parsed = std::from_chars(first, first + cell.size(), value);
  if (cell.empty() || parsed.ec == std::errc::invalid_argument ||
      parsed.ptr != first + cell.size()) {
    return std::nullopt;
  }
  const std::int64_t lowest = -((column.maximum + 1) / 2);
  if (parsed.ec != std::errc() || value < lowest || value > column.maximum) {
    return std::unexpected(CellError(
        file, line, column.name,
        std::format("{} is outside {}..{}", cell, lowest, column.maximum)));
  }
  const auto bits =
      static_cast<std::uint32_t>(static_cast<std::uint64_t>(value) &
                                 static_cast<std::uint64_t>(column.maximum));
  std::string names = FormatFlags(column.enumeration, bits);
  if (names.empty()) {
    return std::unexpected(
        CellError(file, line, column.name,
                  std::format("{} has bits no flag names", cell)));
  }
  return names;
}

// Names the flags cells written as numbers; the number of cells it named.
std::expected<std::size_t, std::string> NameFlagsCells(
    const TableLayout& layout, std::vector<CsvRecord>& records,
    std::string_view file) {
  std::size_t named = 0;
  const auto& header = records.front().fields;
  for (const ColumnSpec& column : layout.columns) {
    if (column.type != ColumnType::kFlags) continue;
    const auto found = std::ranges::find(header, column.name);
    if (found == header.end()) continue;
    const auto at = static_cast<std::size_t>(found - header.begin());
    for (std::size_t row = 1; row < records.size(); ++row) {
      auto& fields = records[row].fields;
      if (at >= fields.size()) continue;  // the schema refuses the short row
      auto cell = NamedFlagsCell(column, fields[at], file, records[row].line);
      if (!cell) return std::unexpected(cell.error());
      if (*cell) {
        fields[at] = *std::move(*cell);
        ++named;
      }
    }
  }
  return named;
}

// A decimal integer cell's value; nothing for any other text.
std::optional<std::int64_t> IntegerCell(std::string_view cell) {
  std::int64_t value = 0;
  const char* const first = cell.data();
  const auto parsed = std::from_chars(first, first + cell.size(), value);
  if (cell.empty() || parsed.ec != std::errc() ||
      parsed.ptr != first + cell.size()) {
    return std::nullopt;
  }
  return value;
}

// An enum cell's code: one of its names or, as version 1 wrote it, a number.
std::optional<std::int64_t> EnumCode(const ColumnSpec& column,
                                     std::string_view cell) {
  if (const auto value = IntegerCell(cell)) return value;
  const auto named =
      std::ranges::find(column.enumeration, cell, &EnumValue::name);
  if (named == column.enumeration.end()) return std::nullopt;
  return named->code;
}

// The value a retired column's cell must hold: 0 or the byte's constant; for
// a byte another column sets, 1 exactly when that column's cell (at `source`)
// is not zero; or for a field holding the template's slot, the slot cell (at
// `source`).
std::expected<std::int64_t, std::string> RetiredValue(
    const TableLayout& layout, const ReservedByte& reserved,
    const CsvRecord& record, std::optional<std::size_t> source,
    std::string_view file) {
  if (!source) return reserved.constant.value_or(0);
  const std::string& cell = record.fields[*source];
  if (reserved.slot_byte) {
    if (const auto slot = IntegerCell(cell)) return *slot;
    return std::unexpected(
        CellError(file, record.line, kSlotColumn,
                  std::format("'{}' is not a number", cell)));
  }
  const auto column =
      std::ranges::find(layout.columns, reserved.set_by, &ColumnSpec::name);
  const auto code =
      column == layout.columns.end() ? std::nullopt : EnumCode(*column, cell);
  if (!code) {
    return std::unexpected(
        CellError(file, record.line, reserved.set_by,
                  std::format("'{}' is not a name or number", cell)));
  }
  return *code != 0 ? 1 : 0;
}

// Drops the columns that held reserved bytes before version 5 (raw_<offset>),
// 6 (action_N_index), 7 (the equipment indices) or 9 (zud_index), each of
// whose cells must hold the value the tables now give its field; the number
// of columns dropped. A field of several bytes is checked whole and dropped
// with its first byte.
std::expected<std::size_t, std::string> RetireReservedColumns(
    const TableLayout& layout, std::vector<CsvRecord>& records,
    std::string_view file) {
  std::size_t retired = 0;
  for (const ReservedByte& reserved : layout.reserved) {
    const auto& header = records.front().fields;
    std::string_view name = reserved.legacy;
    auto found = std::ranges::find(header, name);
    if (found == header.end() && !reserved.numbered.empty()) {
      name = reserved.numbered;
      found = std::ranges::find(header, name);
    }
    if (found == header.end()) continue;
    if (auto widths = CheckWidths(records, file); !widths) {
      return std::unexpected(widths.error());
    }
    const auto at = static_cast<std::size_t>(found - header.begin());
    std::optional<std::size_t> source;
    const std::string_view sets =
        reserved.slot_byte ? kSlotColumn : reserved.set_by;
    if (!sets.empty()) {
      const auto column = std::ranges::find(header, sets);
      if (column == header.end()) {
        return std::unexpected(std::format(
            "{}: has '{}' but not '{}', which sets it", file, name, sets));
      }
      source = static_cast<std::size_t>(column - header.begin());
    }
    for (std::size_t row = 1; row < records.size(); ++row) {
      const CsvRecord& record = records[row];
      auto expected = RetiredValue(layout, reserved, record, source, file);
      if (!expected) return std::unexpected(expected.error());
      if (IntegerCell(record.fields[at]) == *expected) continue;
      std::string why;
      if (reserved.slot_byte) {
        why = "the template's slot sets it now";
      } else if (source) {
        why = std::format("{} sets byte 0x{:02X} now", reserved.set_by,
                          reserved.offset);
      } else if (reserved.constant) {
        why = std::format("byte 0x{:02X} always holds {} now", reserved.offset,
                          *reserved.constant);
      } else {
        why = std::format("byte 0x{:02X} is padding the tables keep zero",
                          reserved.offset);
      }
      return std::unexpected(
          CellError(file, record.line, name,
                    std::format("'{}' is not {}; {}", record.fields[at],
                                *expected, why)));
    }
    for (CsvRecord& record : records) {
      record.fields.erase(record.fields.begin() +
                          static_cast<std::ptrdiff_t>(at));
    }
    ++retired;
  }
  return retired;
}

// The field columns, after the lead key and reference columns.
std::span<const ColumnSpec> FieldColumns(const TableLayout& layout) {
  return std::span(layout.columns)
      .subspan(layout.columns.size() - layout.fields.size());
}

bool HasFieldOf(const TableLayout& layout, ColumnType type) {
  return std::ranges::any_of(
      FieldColumns(layout),
      [type](const ColumnSpec& column) { return column.type == type; });
}

// Writes each action cell that holds a number, as version 5's action_N_id
// did, as the action's id (empty for 0); the number of cells it wrote.
std::expected<std::size_t, std::string> NameActionCells(
    const TableLayout& layout, std::vector<CsvRecord>& records,
    std::string_view file, ActionNames& actions) {
  std::size_t named = 0;
  const auto& header = records.front().fields;
  for (const ColumnSpec& column : FieldColumns(layout)) {
    if (column.type != ColumnType::kReference ||
        column.references != kActionsTable) {
      continue;
    }
    auto found = std::ranges::find(header, column.name);
    if (found == header.end() && !column.legacy_name.empty()) {
      found = std::ranges::find(header, column.legacy_name);
    }
    if (found == header.end()) continue;
    const auto at = static_cast<std::size_t>(found - header.begin());
    for (std::size_t row = 1; row < records.size(); ++row) {
      auto& fields = records[row].fields;
      if (at >= fields.size()) continue;  // the schema refuses the short row
      const auto value = IntegerCell(fields[at]);
      if (!value) continue;
      ++named;
      if (*value == 0) {
        fields[at].clear();
        continue;
      }
      if (auto loaded = actions.Load(); !loaded) {
        return std::unexpected(loaded.error());
      }
      const auto id = actions.IdOf(*value);
      if (!id) {
        return std::unexpected(
            CellError(file, records[row].line, *found, UnheldAction(*value)));
      }
      fields[at] = *id;
    }
  }
  return named;
}

// Rewrites each cell of a column version 8 made signed (resigned) that holds
// 128-255, as version 7 wrote a negative byte, as the same byte's signed
// value; the number of cells it rewrote. Version 8 refuses 128-255 there, so
// a cell is never rewritten twice; the schema refuses any other number the
// byte cannot hold.
std::size_t ResignCells(const TableLayout& layout,
                        std::vector<CsvRecord>& records) {
  constexpr std::int64_t kFirstNegative = 0x80;
  constexpr std::int64_t kByteValues = 0x100;
  std::size_t resigned = 0;
  const auto& header = records.front().fields;
  for (const std::string_view column : layout.resigned) {
    const auto found = std::ranges::find(header, column);
    if (found == header.end()) continue;
    const auto at = static_cast<std::size_t>(found - header.begin());
    for (std::size_t row = 1; row < records.size(); ++row) {
      auto& fields = records[row].fields;
      if (at >= fields.size()) continue;  // the schema refuses the short row
      const auto value = IntegerCell(fields[at]);
      if (!value || *value < kFirstNegative || *value >= kByteValues) continue;
      fields[at] = std::to_string(*value - kByteValues);
      ++resigned;
    }
  }
  return resigned;
}

bool IsLowerHexDigit(char value) {
  return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
}

// Rewrites each text cell that holds its name slot's bytes as hexadecimal,
// as version 11 wrote names, as the name's text; the number of cells it
// rewrote.
std::expected<std::size_t, std::string> DecodeNameCells(
    const TableLayout& layout, std::vector<CsvRecord>& records,
    std::string_view file) {
  std::size_t decoded = 0;
  const auto& header = records.front().fields;
  for (const ColumnSpec& column : FieldColumns(layout)) {
    if (column.type != ColumnType::kText) continue;
    const auto found = std::ranges::find(header, column.name);
    if (found == header.end()) continue;
    const auto at = static_cast<std::size_t>(found - header.begin());
    for (std::size_t row = 1; row < records.size(); ++row) {
      auto& fields = records[row].fields;
      if (at >= fields.size()) continue;  // the schema refuses the short row
      std::string& cell = fields[at];
      if (cell.size() != kHexNameDigits ||
          !std::ranges::all_of(cell, IsLowerHexDigit)) {
        continue;
      }
      ZoneNameBytes bytes{};
      for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes.at(index) = static_cast<std::byte>(
            (HexDigit(cell.at(2 * index)) << kHexDigitBits) |
            HexDigit(cell.at((2 * index) + 1)));
      }
      auto text = ZoneNameText(bytes, "its bytes");
      if (!text) {
        return std::unexpected(
            CellError(file, records[row].line, column.name, text.error()));
      }
      cell = std::move(*text);
      ++decoded;
    }
  }
  return decoded;
}

bool HasKindField(const TableLayout& layout) {
  return std::ranges::any_of(FieldColumns(layout),
                             [](const ColumnSpec& column) {
                               return column.references == kKindsTable;
                             });
}

// A kind cell's number, when it holds one a kind can have.
std::optional<std::int64_t> KindCell(const CsvRecord& record, std::size_t at) {
  if (at >= record.fields.size()) return std::nullopt;  // the schema refuses
  const auto value = IntegerCell(record.fields[at]);
  if (!value || *value < 0 || *value > kMaxKind) return std::nullopt;
  return value;
}

// Writes each kind cell that holds a number, as version 12's sub_type did, as
// the id EnemyKindIds gives the kind after its first named template in row
// order; the number of cells it wrote. `kinds` gets the enemy-kinds table of
// those ids. The schema and reader refuse any other cell.
std::expected<std::size_t, std::string> NameKindCells(
    const TableLayout& layout, std::vector<CsvRecord>& records,
    std::string_view file, std::string& kinds) {
  const auto& header = records.front().fields;
  const auto legacy = std::ranges::find(header, kLegacyKindColumn);
  if (!HasKindField(layout) || legacy == header.end()) return 0;
  if (std::ranges::find(header, kKindColumn) != header.end()) {
    return std::unexpected(std::format("{}: has both '{}' and '{}'", file,
                                       kLegacyKindColumn, kKindColumn));
  }
  const auto at = static_cast<std::size_t>(legacy - header.begin());
  const auto name_at = static_cast<std::size_t>(
      std::ranges::find(header, "name") - header.begin());
  std::map<std::int64_t, std::string> names;
  for (std::size_t row = 1; row < records.size(); ++row) {
    const auto kind = KindCell(records[row], at);
    if (!kind) continue;
    const auto& fields = records[row].fields;
    NameKind(names, *kind,
             name_at < fields.size() ? fields[name_at] : std::string{});
  }
  if (names.empty()) return 0;
  const KindNames named(names);
  std::size_t written = 0;
  for (std::size_t row = 1; row < records.size(); ++row) {
    const auto kind = KindCell(records[row], at);
    if (!kind) continue;
    if (const auto id = named.IdOf(*kind)) {
      records[row].fields[at] = *id;
      ++written;
    }
  }
  kinds = FormatTable(ZoneTableSpec(ZoneTable::kKinds), named.Rows());
  return written;
}

// Appends a row for each of `enemy`'s limbs and equipment parts that holds a
// field.
std::expected<void, std::string> AppendLimbAndPartRows(
    const ZoneEnemyRecord& enemy, const ZoneReferences& refs,
    std::vector<std::vector<std::string>>& limb_rows,
    std::vector<std::vector<std::string>>& part_rows) {
  for (std::size_t limb = 0; limb < static_cast<std::size_t>(kLimbCount);
       ++limb) {
    const auto bytes = LimbOf(enemy.data, limb);
    if (LimbFieldsZero(bytes)) continue;
    std::vector<std::string> limb_cells{enemy.id, std::format("{}", limb)};
    if (auto done =
            AppendCells(limb_cells, LayoutOf(ZoneTable::kLimbs), bytes, refs);
        !done) {
      return std::unexpected(
          std::format("{} limb {}: {}", enemy.id, limb, done.error()));
    }
    limb_rows.push_back(std::move(limb_cells));
  }
  for (const PartSlot& part : kPartSlots) {
    const auto bytes = std::span(enemy.data).subspan(part.offset, kPartBytes);
    if (AllZero(bytes)) continue;
    std::vector<std::string> part_cells{enemy.id, std::string(part.id)};
    if (auto done =
            AppendCells(part_cells, LayoutOf(ZoneTable::kParts), bytes, refs);
        !done) {
      return std::unexpected(
          std::format("{} {}: {}", enemy.id, part.id, done.error()));
    }
    part_rows.push_back(std::move(part_cells));
  }
  return {};
}

}  // namespace

const TableSpec& ZoneTableSpec(ZoneTable table) { return LayoutOf(table).spec; }

std::string ZoneTablePath(ZoneTable table) {
  const TableLayout& layout = LayoutOf(table);
  return std::format("{}/{}{}", layout.directory, layout.spec.name, kCsvSuffix);
}

std::string ZoneTableSchemaPath(ZoneTable table) {
  const TableLayout& layout = LayoutOf(table);
  return std::format("{}/{}{}", layout.directory, layout.spec.name,
                     kTableSchemaSuffix);
}

std::expected<std::string, std::string> DiscFileId(std::string_view kind,
                                                   std::string_view disc_path) {
  const std::size_t slash = disc_path.rfind('/');
  const std::size_t dot = disc_path.rfind('.');
  if (slash == std::string_view::npos || slash == 0 ||
      dot == std::string_view::npos || dot <= slash + 1) {
    return std::unexpected(
        std::format("{} has no directory, stem and extension", disc_path));
  }
  std::string id = std::format("{}.", kind);
  for (const char value : disc_path.substr(0, dot)) {
    id += value == '/'
              ? '.'
              : static_cast<char>(
                    value >= 'A' && value <= 'Z' ? value - 'A' + 'a' : value);
  }
  if (!IsContentId(id)) {
    return std::unexpected(
        std::format("{} does not make a content id", disc_path));
  }
  return id;
}

std::expected<std::string, std::string> DiscPathOfFileId(
    std::string_view kind, std::string_view id, std::string_view extension) {
  if (!id.starts_with(kind) || id.size() <= kind.size() + 1 ||
      id[kind.size()] != '.' || !IsContentId(id)) {
    return std::unexpected(std::format("'{}' is not a {} id", id, kind));
  }
  const std::string_view rest = id.substr(kind.size() + 1);
  if (rest.find('.') == std::string_view::npos) {
    return std::unexpected(std::format("'{}' names no directory", id));
  }
  std::string path;
  for (const char value : rest) {
    path += value == '.'
                ? '/'
                : static_cast<char>(
                      value >= 'a' && value <= 'z' ? value - 'a' + 'A' : value);
  }
  return path + std::string(extension);
}

std::string ZoneEnemyId(std::string_view zone_id, std::size_t slot) {
  const std::string_view rest = zone_id.starts_with(kZoneIdKind)
                                    ? zone_id.substr(kZoneIdKind.size())
                                    : zone_id;
  return std::format("enemy{}.{:02}", rest, slot);
}

std::expected<std::string, std::string> ZoneNameText(
    std::span<const std::byte> source, std::string_view id) {
  if (source.size() != kZoneNameBytes) {
    return std::unexpected(std::format("{}: a name slot is {} bytes, not {}",
                                       id, kZoneNameBytes, source.size()));
  }
  if (AllZero(source)) return std::string{};
  const auto decoded = DecodeGameText(source, id);
  if (!decoded) return std::unexpected(decoded.error());
  if (!decoded->ignored_font_bits.empty()) {
    return std::unexpected(std::format(
        "{}: a move-down holds low bits, which its text drops", id));
  }
  const auto tail = source.subspan(decoded->consumed);
  const auto set = std::ranges::find_if(
      tail, [](std::byte value) { return value != std::byte{}; });
  if (set != tail.end()) {
    return std::unexpected(std::format(
        "{}: text byte {}: 0x{:02X} after the E7 is not padding 0", id,
        decoded->consumed + static_cast<std::size_t>(set - tail.begin()),
        std::to_integer<unsigned>(*set)));
  }
  if (decoded->markup.empty()) {
    return std::unexpected(
        std::format("{}: the name is an E7 alone, which no text rebuilds", id));
  }
  const auto units = MarkupUnits(decoded->markup);
  if (std::ranges::find(units, kSpace) != units.end()) {
    return std::unexpected(std::format(
        "{}: the name holds the space glyph 0x8F, which its text writes as a "
        "word gap",
        id));
  }
  std::string text = NameTextOf(decoded->markup);
  const auto rebuilt = EncodeZoneName(text, id);
  if (!rebuilt) return std::unexpected(rebuilt.error());
  if (!std::ranges::equal(*rebuilt, source)) {
    return std::unexpected(
        std::format("{}: the text '{}' does not rebuild the name", id, text));
  }
  return text;
}

std::expected<ZoneNameBytes, std::string> EncodeZoneName(std::string_view text,
                                                         std::string_view id) {
  ZoneNameBytes name{};
  if (text.empty()) return name;
  std::string markup;
  for (const std::string_view unit : MarkupUnits(text)) {
    markup += unit == kSpace ? kWordGap : unit;
  }
  const auto bytes = EncodeGameText(markup, id);
  if (!bytes) {
    if (markup == text) return std::unexpected(bytes.error());
    return std::unexpected(
        std::format("{} (in its markup '{}')", bytes.error(), markup));
  }
  if (bytes->size() > name.size()) {
    return std::unexpected(
        std::format("{}: encodes to {} bytes with its E7; a name holds {}", id,
                    bytes->size(), name.size()));
  }
  std::ranges::copy(*bytes, name.begin());
  return name;
}

std::map<std::int64_t, std::string> EnemyKindIds(
    const std::map<std::int64_t, std::string>& names) {
  std::map<std::int64_t, std::string> slugs;
  std::map<std::string, std::size_t, std::less<>> uses;
  for (const auto& [number, name] : names) {
    auto slug = KindSlug(name);
    ++uses[slug];
    slugs.emplace(number, std::move(slug));
  }
  // A numbered id ends in "-<number>"; no other id ends in a number, and the
  // numbers are distinct, so no two ids are equal.
  std::map<std::int64_t, std::string> ids;
  for (const auto& [number, slug] : slugs) {
    const bool numbered =
        uses.at(slug) > 1 || slug == kUntitled || EndsInNumber(slug);
    ids.emplace(number,
                numbered ? std::format("{}{}-{}", kKindIdPrefix, slug, number)
                         : std::format("{}{}", kKindIdPrefix, slug));
  }
  return ids;
}

std::expected<ZoneTableText, std::string> FormatZoneTables(
    std::span<const ZoneRecord> zones, const ZoneTableReader& read) {
  ActionNames actions(read);
  std::vector<const ZoneRecord*> sorted;
  for (const ZoneRecord& zone : zones) sorted.push_back(&zone);
  std::ranges::sort(sorted, {}, &ZoneRecord::id);
  auto kind_names = KindNamesOf(sorted);
  if (!kind_names) return std::unexpected(kind_names.error());
  const KindNames kinds(*kind_names);
  const ZoneReferences refs{.actions = actions, .kinds = kinds};
  std::array<std::vector<std::vector<std::string>>, kZoneTableCount> rows;
  rows.at(static_cast<std::size_t>(ZoneTable::kKinds)) = kinds.Rows();
  auto& zone_rows = rows.at(static_cast<std::size_t>(ZoneTable::kZones));
  auto& room_rows = rows.at(static_cast<std::size_t>(ZoneTable::kRooms));
  auto& texture_rows = rows.at(static_cast<std::size_t>(ZoneTable::kTextures));
  auto& enemy_rows = rows.at(static_cast<std::size_t>(ZoneTable::kEnemies));
  auto& limb_rows = rows.at(static_cast<std::size_t>(ZoneTable::kLimbs));
  auto& part_rows = rows.at(static_cast<std::size_t>(ZoneTable::kParts));
  for (const ZoneRecord* zone : sorted) {
    zone_rows.push_back({zone->id, std::format("{}", zone->music_id),
                         std::format("{}", zone->rooms.size())});
    for (std::size_t slot = 0; slot < zone->rooms.size(); ++slot) {
      if (zone->rooms[slot].empty()) continue;
      room_rows.push_back(
          {zone->id, std::format("{}", slot), zone->rooms[slot]});
    }
    for (std::size_t index = 0; index < zone->textures.size(); ++index) {
      texture_rows.push_back(
          {zone->id, std::format("{}", index), zone->textures[index]});
    }
    for (std::size_t slot = 0; slot < zone->enemies.size(); ++slot) {
      const ZoneEnemyRecord& enemy = zone->enemies[slot];
      std::vector<std::string> cells{enemy.id, zone->id,
                                     std::format("{}", slot), enemy.archetype};
      if (auto done = AppendCells(cells, LayoutOf(ZoneTable::kEnemies),
                                  enemy.data, refs);
          !done) {
        return std::unexpected(std::format("{}: {}", enemy.id, done.error()));
      }
      enemy_rows.push_back(std::move(cells));
      if (auto done = AppendLimbAndPartRows(enemy, refs, limb_rows, part_rows);
          !done) {
        return std::unexpected(done.error());
      }
    }
  }
  ZoneTableText texts;
  for (const ZoneTable table : kZoneTables) {
    const auto index = static_cast<std::size_t>(table);
    texts.at(index) = FormatTable(ZoneTableSpec(table), rows.at(index));
  }
  return texts;
}

std::expected<std::vector<ZoneRecord>, std::string> ReadZoneTables(
    const ZoneTableReader& read) {
  auto tables = ReadAll(read);
  if (!tables) return std::unexpected(tables.error());
  KindNames kinds;
  if (auto read_kinds = ReadKinds(*tables, kinds); !read_kinds) {
    return std::unexpected(read_kinds.error());
  }
  ActionNames actions(read);
  const ZoneReferences refs{.actions = actions, .kinds = kinds};
  std::map<std::string, ZoneRecord, std::less<>> zones;
  for (const TableRow& row : tables->Rows(ZoneTable::kZones)) {
    ZoneRecord& zone = zones[row.cells.at(0)];
    zone.id = row.cells.at(0);
    zone.music_id = static_cast<std::int32_t>(row.values.at(1));
    zone.rooms.resize(static_cast<std::size_t>(row.values.at(2)));
  }
  const auto zone_of =
      [&](const TableRow& row,
          ZoneTable table) -> std::expected<ZoneRecord*, std::string> {
    const auto found = zones.find(row.cells.at(0));
    if (found == zones.end()) {
      return std::unexpected(CellError(
          tables->Path(table), row.line, ZoneTableSpec(table).columns[0].name,
          std::format("no zone '{}' in {}", row.cells.at(0),
                      ZoneTablePath(ZoneTable::kZones))));
    }
    return &found->second;
  };
  for (const TableRow& row : tables->Rows(ZoneTable::kRooms)) {
    auto zone = zone_of(row, ZoneTable::kRooms);
    if (!zone) return std::unexpected(zone.error());
    const auto slot = static_cast<std::size_t>(row.values.at(1));
    if (slot >= (*zone)->rooms.size()) {
      return std::unexpected(
          CellError(tables->Path(ZoneTable::kRooms), row.line, "slot",
                    std::format("{} is past {}'s {} room slots", slot,
                                (*zone)->id, (*zone)->rooms.size())));
    }
    (*zone)->rooms[slot] = row.cells.at(2);
  }
  std::map<std::string, std::map<std::int64_t, std::string>, std::less<>>
      textures;
  for (const TableRow& row : tables->Rows(ZoneTable::kTextures)) {
    auto zone = zone_of(row, ZoneTable::kTextures);
    if (!zone) return std::unexpected(zone.error());
    textures[(*zone)->id][row.values.at(1)] = row.cells.at(2);
  }
  for (const auto& [id, entries] : textures) {
    auto dense = Dense(entries, "texture", id);
    if (!dense) {
      return std::unexpected(std::format(
          "{}: {}", tables->Path(ZoneTable::kTextures), dense.error()));
    }
    zones.at(id).textures = std::move(*dense);
  }
  // Templates by enemy id, built from their rows in table order.
  std::map<std::string, ZoneEnemyRecord, std::less<>> enemies;
  std::map<std::string, std::map<std::int64_t, std::string>, std::less<>> slots;
  const TableLayout& enemy_layout = LayoutOf(ZoneTable::kEnemies);
  for (const TableRow& row : tables->Rows(ZoneTable::kEnemies)) {
    const std::string& zone_id = row.cells.at(1);
    if (!zones.contains(zone_id)) {
      return std::unexpected(
          CellError(tables->Path(ZoneTable::kEnemies), row.line, "zone",
                    std::format("no zone '{}' in {}", zone_id,
                                ZoneTablePath(ZoneTable::kZones))));
    }
    auto [slot, added] =
        slots[zone_id].try_emplace(row.values.at(2), row.cells.at(0));
    if (!added) {
      return std::unexpected(
          CellError(tables->Path(ZoneTable::kEnemies), row.line, "slot",
                    std::format("{} already has a template in slot {}", zone_id,
                                row.values.at(2))));
    }
    ZoneEnemyRecord& enemy = enemies[row.cells.at(0)];
    enemy.id = row.cells.at(0);
    enemy.archetype = row.cells.at(3);
    if (auto done = ApplyRow(enemy.data, enemy_layout, row, refs); !done) {
      return std::unexpected(done.error());
    }
  }
  const auto enemy_of =
      [&](const TableRow& row,
          ZoneTable table) -> std::expected<ZoneEnemyRecord*, std::string> {
    const auto found = enemies.find(row.cells.at(0));
    if (found == enemies.end()) {
      return std::unexpected(
          CellError(tables->Path(table), row.line, "enemy",
                    std::format("no template '{}' in {}", row.cells.at(0),
                                ZoneTablePath(ZoneTable::kEnemies))));
    }
    return &found->second;
  };
  for (const TableRow& row : tables->Rows(ZoneTable::kLimbs)) {
    auto enemy = enemy_of(row, ZoneTable::kLimbs);
    if (!enemy) return std::unexpected(enemy.error());
    const auto limb = static_cast<std::size_t>(row.values.at(1));
    if (auto done =
            ApplyRow(std::span((*enemy)->data)
                         .subspan(kLimbsAt + (limb * kLimbBytes), kLimbBytes),
                     LayoutOf(ZoneTable::kLimbs), row, refs);
        !done) {
      return std::unexpected(done.error());
    }
  }
  for (const TableRow& row : tables->Rows(ZoneTable::kParts)) {
    auto enemy = enemy_of(row, ZoneTable::kParts);
    if (!enemy) return std::unexpected(enemy.error());
    const auto offset = PartOffset(row.cells.at(1));
    if (!offset) {
      return std::unexpected(CellError(
          tables->Path(ZoneTable::kParts), row.line, "part",
          std::format("'{}' is not an equipment part", row.cells.at(1))));
    }
    if (auto done =
            ApplyRow(std::span((*enemy)->data).subspan(*offset, kPartBytes),
                     LayoutOf(ZoneTable::kParts), row, refs);
        !done) {
      return std::unexpected(done.error());
    }
  }
  for (const auto& [zone_id, entries] : slots) {
    auto dense = Dense(entries, "template", zone_id);
    if (!dense) {
      return std::unexpected(std::format(
          "{}: {}", tables->Path(ZoneTable::kEnemies), dense.error()));
    }
    for (std::size_t slot = 0; slot < dense->size(); ++slot) {
      const std::string& id = dense->at(slot);
      DeriveReservedBytes(enemies.at(id).data, slot);
      zones.at(zone_id).enemies.push_back(std::move(enemies.at(id)));
    }
  }
  std::vector<ZoneRecord> records;
  for (auto& [id, zone] : zones) records.push_back(std::move(zone));
  return records;
}

std::expected<void, std::string> CheckZoneTemplateCoverage() {
  std::array<unsigned, kZndTemplateBytes> covered{};
  std::array<bool, kZndTemplateBytes> overlap{};
  const auto mark_bits = [&](std::size_t at, unsigned mask) {
    unsigned& bits = covered.at(at);
    if ((bits & mask) != 0) overlap.at(at) = true;
    bits |= mask;
  };
  const auto mark = [&](const Field& field, std::size_t base) {
    for (std::size_t index = 0; index < field.size; ++index) {
      mark_bits(
          base + field.offset + index,
          field.bits == 0 ? kByteMask : ((1U << field.bits) - 1) << field.bit);
    }
  };
  for (const Field& field : LayoutOf(ZoneTable::kEnemies).fields) {
    mark(field, 0);
  }
  for (std::size_t limb = 0; limb < static_cast<std::size_t>(kLimbCount);
       ++limb) {
    for (const Field& field : LayoutOf(ZoneTable::kLimbs).fields) {
      mark(field, kLimbsAt + (limb * kLimbBytes));
    }
  }
  for (const PartSlot& part : kPartSlots) {
    for (const Field& field : LayoutOf(ZoneTable::kParts).fields) {
      mark(field, part.offset);
    }
  }
  std::optional<std::string> unset;
  ForEachReserved(
      covered, [&](const TableLayout& layout, const ReservedByte& reserved,
                   std::span<unsigned> /*record*/, std::size_t base) {
        mark_bits(base + reserved.offset, reserved.mask);
        if (!reserved.set_by.empty() &&
            std::ranges::find(layout.fields, reserved.set_by, &Field::column) ==
                layout.fields.end()) {
          unset = std::format("reserved byte '{}' is set by no column '{}'",
                              reserved.legacy, reserved.set_by);
        }
      });
  if (unset) return std::unexpected(*unset);
  for (std::size_t at = 0; at < kZndTemplateBytes; ++at) {
    if (overlap.at(at)) {
      return std::unexpected(
          std::format("byte 0x{:03X} is in more than one column", at));
    }
    if (covered.at(at) != kByteMask) {
      return std::unexpected(std::format("byte 0x{:03X} is in no column", at));
    }
  }
  // An unnamed bit would leave FormatFlags no spelling for its cell.
  for (const ZoneTable table : kZoneTables) {
    for (const ColumnSpec& column : LayoutOf(table).columns) {
      if (column.type != ColumnType::kFlags) continue;
      std::int64_t named = 0;
      for (const EnumValue& value : column.enumeration) named |= value.code;
      if (named != column.maximum) {
        return std::unexpected(
            std::format("flags column '{}' leaves bits unnamed", column.name));
      }
    }
  }
  return {};
}

std::expected<void, std::string> CheckZoneReservedBytes(const ZndTemplate& data,
                                                        std::size_t slot) {
  std::optional<std::string> fault;
  ForEachReserved(
      data, [&](const TableLayout& layout, const ReservedByte& reserved,
                std::span<const std::byte> record, std::size_t base) {
        const unsigned mask = reserved.mask;
        const unsigned expected =
            ReservedValue(layout, reserved, record, slot) & mask;
        const auto actual =
            std::to_integer<unsigned>(record[reserved.offset]) & mask;
        if (fault || actual == expected) return;
        const std::size_t at = base + reserved.offset;
        if (mask != kByteMask) {
          fault = std::format(
              "bits 0x{:02X} of reserved byte 0x{:03X} are {}, not padding {}",
              mask, at, actual, expected);
        } else if (reserved.slot_byte) {
          fault = std::format(
              "reserved byte 0x{:03X} is {}, not the {} its slot {} gives", at,
              actual, expected, slot);
        } else if (!reserved.set_by.empty()) {
          fault = std::format(
              "reserved byte 0x{:03X} is {}, not the {} its {} "
              "gives",
              at, actual, expected, reserved.set_by);
        } else if (reserved.constant) {
          fault = std::format(
              "reserved byte 0x{:03X} is {}, not the {} it always holds", at,
              actual, expected);
        } else {
          fault = std::format("reserved byte 0x{:03X} is {}, not padding 0", at,
                              actual);
        }
      });
  if (fault) return std::unexpected(*fault);
  return {};
}

std::expected<ZoneColumnMigration, std::string> MigrateZoneColumns(
    ZoneTable table, const std::string& text, std::string_view file,
    const ZoneTableReader& read) {
  const TableLayout& layout = LayoutOf(table);
  ZoneColumnMigration migrated{.text = text, .kinds = {}};
  if (layout.packed.empty() && layout.reserved.empty() &&
      layout.resigned.empty() && !HasFieldOf(layout, ColumnType::kFlags) &&
      !HasFieldOf(layout, ColumnType::kReference) &&
      !HasFieldOf(layout, ColumnType::kText)) {
    return migrated;
  }
  auto records = ParseCsv(text, file);
  if (!records) return std::unexpected(records.error());
  if (records->empty()) return migrated;
  for (const PackedColumn& packed : layout.packed) {
    const auto& header = records->front().fields;
    const auto found = std::ranges::find(header, packed.legacy);
    if (found == header.end()) continue;
    for (const std::string_view column : packed.columns) {
      if (std::ranges::find(header, column) != header.end()) {
        return std::unexpected(std::format("{}: has both '{}' and '{}'", file,
                                           packed.legacy, column));
      }
    }
    const auto at = static_cast<std::size_t>(found - header.begin());
    if (auto done = SplitColumn(layout, packed, *records, at, file); !done) {
      return std::unexpected(done.error());
    }
    ++migrated.split_columns;
  }
  auto retired = RetireReservedColumns(layout, *records, file);
  if (!retired) return std::unexpected(retired.error());
  migrated.retired_columns = *retired;
  auto named = NameFlagsCells(layout, *records, file);
  if (!named) return std::unexpected(named.error());
  ActionNames actions(read);
  auto actions_named = NameActionCells(layout, *records, file, actions);
  if (!actions_named) return std::unexpected(actions_named.error());
  migrated.named_values = *named + *actions_named;
  migrated.resigned_values = ResignCells(layout, *records);
  auto decoded = DecodeNameCells(layout, *records, file);
  if (!decoded) return std::unexpected(decoded.error());
  migrated.decoded_texts = *decoded;
  auto kinds_named = NameKindCells(layout, *records, file, migrated.kinds);
  if (!kinds_named) return std::unexpected(kinds_named.error());
  migrated.named_values += *kinds_named;
  if (migrated.split_columns != 0 || migrated.retired_columns != 0 ||
      migrated.named_values != 0 || migrated.resigned_values != 0 ||
      migrated.decoded_texts != 0) {
    std::vector<std::vector<std::string>> rows;
    for (CsvRecord& record : *records) rows.push_back(std::move(record.fields));
    migrated.text = FormatCsv(rows);
  }
  return migrated;
}

}  // namespace VS::Content
