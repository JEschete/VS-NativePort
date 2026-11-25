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
// A zone's room slots, textures and enemies. An enemy placement names its
// template by a byte.
constexpr std::int64_t kMaxRoomSlots = 0xFFFF;
constexpr std::int64_t kMaxTextures = 0xFFFF;
constexpr std::int64_t kMaxEnemies = 0xFF;
constexpr std::int64_t kLimbCount = 6;
// The ZUD index is a template's last word.
constexpr std::size_t kZudIndexBytes = 4;
constexpr std::size_t kZudIndexAt = kZndTemplateBytes - kZudIndexBytes;
constexpr std::size_t kNameBytes = 24;
constexpr std::array<std::string_view, 4> kTypeFields{
    "none",
    "blunt",
    "edged",
    "piercing",
};
// EquipmentStats.cpp uses classes[0..5] and affinities[0..6].
// Tail slots remain explicit storage until the import/runtime split retires
// them.
constexpr std::array<std::string_view, 8> kClassFields{
    "human",  "beast", "undead",    "phantom",
    "dragon", "evil",  "storage_6", "storage_7",
};
constexpr std::array<std::string_view, 8> kAffinityFields{
    "physical", "air", "fire", "earth", "water", "light", "dark", "storage_7",
};
constexpr std::array<std::string_view, 6> kSplashFields{
    "limb_0_percent", "limb_1_percent", "limb_2_percent",
    "limb_3_percent", "limb_4_percent", "limb_5_percent",
};

// decomp: vs_CharInitData (src/BATTLE/BATTLE.PRG/charInit.h:24-51) and the
// records it holds; see Actors/CharacterInit.hpp and Items/ItemRecords.hpp.
constexpr std::size_t kLimbsAt = 0x238;
constexpr std::size_t kLimbBytes = 0x5C;
constexpr std::size_t kLimbArmorAt = 0x20;
constexpr std::size_t kPartBytes = 0x30;
constexpr std::size_t kLimbActionSlots = 4;

// One column's bytes in its record: a little-endian integer of `size` bytes,
// `bits` bits of one byte from `bit` up, or `size` bytes as hexadecimal.
struct Field {
  std::string_view column;
  std::size_t offset = 0;
  std::size_t size = 1;
  bool is_signed = false;
  bool bytes = false;
  std::uint8_t bit = 0;
  std::uint8_t bits = 0;  // 0: the whole value
};

// A former column that held whole bytes now split into bit-field columns.
struct PackedColumn {
  std::string_view legacy;
  std::vector<std::string_view> columns;
};

// A byte no column holds (§4.1): zero padding, a constant, 1 exactly when
// the column `set_by` is not zero, or byte `slot_byte` (little-endian) of the
// template's slot. `legacy` is the column that held it before version 5
// (raw_<offset>), 6 (action_N_index), 7 (the equipment indices) or 9
// (zud_index).
struct ReservedByte {
  std::string_view legacy;
  std::size_t offset = 0;
  std::string_view set_by;
  std::optional<std::uint8_t> constant;
  std::optional<std::uint8_t> slot_byte;
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
                        .slot_byte = std::nullopt});
  }

  void Constant(std::string_view legacy, std::size_t offset,
                std::uint8_t value) {
    reserved.push_back({.legacy = legacy,
                        .offset = offset,
                        .set_by = {},
                        .constant = value,
                        .slot_byte = std::nullopt});
  }

  // A little-endian field of `size` bytes that holds the template's slot.
  void Slot(std::string_view legacy, std::size_t offset, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
      reserved.push_back({.legacy = legacy,
                          .offset = offset + index,
                          .set_by = {},
                          .constant = std::nullopt,
                          .slot_byte = static_cast<std::uint8_t>(index)});
    }
  }

  // A field of `size` bytes that always holds zero; each byte names the
  // field's former column.
  void Zeros(std::string_view legacy, std::size_t offset, std::size_t size) {
    for (std::size_t at = offset; at < offset + size; ++at) {
      Constant(legacy, at, 0);
    }
  }

  // An optional reference stored as the row's original index; 0 is none.
  void Reference(std::string_view name, std::size_t offset, std::size_t size,
                 std::string_view table, std::string_view description,
                 std::string_view legacy_name) {
    fields.push_back({.column = name,
                      .offset = offset,
                      .size = size,
                      .is_signed = false,
                      .bytes = false,
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
                       .optional = true});
  }

  void Add(std::string_view name, std::size_t offset, std::size_t size,
           bool is_signed, std::string_view description,
           std::span<const EnumValue> domain = {},
           std::string_view legacy_name = {}) {
    Field field{.column = name,
                .offset = offset,
                .size = size,
                .is_signed = is_signed,
                .bytes = false,
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
                      .bytes = false,
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
                      .bytes = false,
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

  void Bytes(std::string_view name, std::size_t offset, std::size_t size,
             std::string_view description) {
    fields.push_back({.column = name,
                      .offset = offset,
                      .size = size,
                      .is_signed = false,
                      .bytes = true,
                      .bit = 0,
                      .bits = 0});
    columns.push_back({.name = name,
                       .type = ColumnType::kBytes,
                       .minimum = 0,
                       .maximum = static_cast<std::int64_t>(size),
                       .references = {},
                       .description = description});
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
  table.Add("sub_type", 0x00, 2, false, "the enemy kind (kill and bestiary)");
  table.Add("model_id", 0x02, 1, false, "the body model id");
  table.Bits("reach", 0x03, 0, 5, "attack reach");
  table.Bits("enemy_class", 0x03, 5, 3, "the class (human .. evil)",
             kEnemyClasses);
  table.Bytes("name", 0x04, kNameBytes, "the name, in the game's encoding");
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
  // decomp: func_800E5A9C (src/BATTLE/BATTLE.PRG/7CD68.c:205) decodes it
  // (Battle/Ai/ActorSettings.hpp).
  // decomp: func_800E5A74 (src/BATTLE/BATTLE.PRG/7CD68.c:200)
  table.Add("ai_settings", 0x2C, 4, true,
            "packed AI settings: bits 2-3 facing delay, 4-6 wander preset, "
            "8-9 step height, 10-11 tile step limit, 14 area lock, 16-17 view "
            "angle, 18-20 perception range, 21-22 relation category, 30-31 "
            "terrain mask; the rest set AI state flags, and an actor's saved "
            "flags can replace bits 21-31");
  table.Add("mobility_limb_1", 0x30, 1, false,
            "a limb whose loss slows the enemy");
  table.Add("mobility_limb_2", 0x31, 1, false,
            "a limb whose loss slows the enemy");
  table.Add("default_target_limb", 0x32, 1, false,
            "the limb a hit aimed at the whole enemy strikes");
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941) and
  // applyAccessoryStats (src/BATTLE/BATTLE.PRG/146C.c:734) read neither this
  // byte nor the accessory's last two; every disc template holds zero there.
  table.Reserve("raw_33", 0x33);
  table.Add("weapon_material", 0x124, 1, false, "weapon material", kMaterials);
  table.Add("weapon_drop_rate", 0x125, 1, false, "out of 255");
  // An equipment index is the 1-based inventory slot the player's equipment
  // came from. The apply*Stats copy each into the actor's stats, and only
  // vs_mainMenu_initInventory reads them, from the player's. Every disc
  // template holds 0 (not from the inventory) in each, as in its parts' and
  // limb armours' indices.
  // decomp: vs_battle_applyWeaponStats (src/BATTLE/BATTLE.PRG/146C.c:624)
  // decomp: vs_battle_applyShieldStats (src/BATTLE/BATTLE.PRG/146C.c:683)
  // decomp: vs_mainMenu_initInventory (src/MENU/MAINMENU.PRG/C48.c:51)
  table.Zeros("weapon_index", 0x126, 2);
  table.Bytes("weapon_name", 0x128, kNameBytes,
              "the weapon's name, in the game's encoding");
  table.Add("shield_material", 0x200, 1, false, "shield material", kMaterials);
  table.Add("shield_drop_rate", 0x201, 1, false, "out of 255");
  table.Zeros("shield_index", 0x202, 2);
  table.Add("accessory_drop_rate", 0x234, 1, false, "out of 255");
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
            "signed; added to the enemy's agility and intelligence totals "
            "when the chance of a hit on this limb is worked out");
  table.resigned.push_back("agility_defense_bonus");
  // decomp: getChainEvasionModifier (src/BATTLE/BATTLE.PRG/146C.c:10042)
  table.Add("chain_evasion", 0x03, 1, false,
            "how well the limb evades actions whose hit_function is "
            "chain-evasion: they land (255 - value) * 100 / 255 percent of "
            "the time");
  table.Run("type", 0x04, kTypeFields, 1, true,
            "signed physical-type resistance");
  table.Run(
      "affinity", 0x08, kAffinityFields, 1, true,
      "signed elemental affinity; storage_7 retains the original tail byte");
  // decomp: func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941) copies each
  // slot's id, sequence and index into the actor's stats; no code reads the
  // index, and every disc limb holds the slot's position there.
  // decomp: func_8007138C (src/BATTLE/BATTLE.PRG/146C.c:3618) loads the
  // sequence for actions 141-223.
  for (std::size_t action = 0; action < kLimbActionSlots; ++action) {
    const std::size_t at = 0x10 + (action * 4);
    table.Reference(table.Name(std::format("action_{}", action + 1)), at, 2,
                    "actions", "the action in this slot; empty for none",
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
  table.Add("armor_drop_rate", 0x51, 1, false, "out of 255");
  // decomp: applyArmorStats (src/BATTLE/BATTLE.PRG/146C.c:759) copies this
  // byte to vs_UiArmor.unk99, which no code reads. Every disc limb holds 1
  // exactly when it has an armour material, as INITBTL.PRG's init of Ashley
  // does.
  table.Reserve("raw_52", 0x52, "armor_material");
  // An equipment index (AddEnemyFields), zero on the disc.
  // decomp: applyArmorStats (src/BATTLE/BATTLE.PRG/146C.c:759)
  table.Zeros("armor_index", 0x53, 1);
  table.Run("dp_splash", 0x54, kSplashFields, 1, false,
            "DP damage share (%) to the zero-based destination limb; body-part "
            "names depend on the model");
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
            "the item's 1-based index in its family: item_id less 0 for a "
            "blade, 95 for a grip, 126 for a shield, armour or accessory, "
            "260 for a gem");
  table.Add("weapon_model_id", 0x03, 1, true,
            "a blade's or shield's WEP model (OBJ/<two hex digits>.WEP, "
            "converted to models/weapons/<two hex digits>.gltf); 0 for other "
            "parts");
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
  table.Add("recovery_delay", 0x14, 1, false,
            "the blade's recovery delay after an attack, for actions that "
            "take the weapon's delay");
  table.Add("gem_slots", 0x15, 1, false,
            "how many gems a grip or shield holds");
  table.Add("gem_effects", 0x16, 1, false,
            "a gem's effect (0 none): 1/2 physical/spell hit bonus, 3/4 "
            "+5% physical/spell damage, 5/6 physical/spell evasion, 7/8 -5% "
            "physical/spell damage taken, 9 PP gain; 11-14, 16, 18, 19 and 21 "
            "evade specific action effects");
  // The part's inventory slot, zero on the disc: an equipment index
  // (AddEnemyFields). Loot copies of parts skip it.
  // decomp: copyUiEquipmentStats (src/BATTLE/BATTLE.PRG/146C.c:1156)
  table.Zeros("index", 0x17, 1);
  table.Add("range", 0x18, 1, false,
            "the blade's reach; the enemy's reach is added to it and to the "
            "other two extents");
  table.Add("range_y", 0x19, 1, false, "the attack area's second extent");
  table.Add("range_z", 0x1A, 1, false, "the attack area's third extent");
  // decomp: vs_Range (src/SLUS_010.40/inventory.h:4)
  table.Bits("attack_shape", 0x1B, 0, 3,
             "the blade's targeting volume; actions use the same domain",
             kVolumeShapes);
  table.Bits("attack_angle", 0x1B, 3, 5, "the attack area's angle");
  table.packed.push_back(
      {.legacy = "shape_bits", .columns = {"attack_shape", "attack_angle"}});
  table.Run("type", 0x1C, kTypeFields, 1, true, "signed physical-type rating");
  table.Run("class", 0x20, kClassFields, 1, true,
            "signed class rating; storage_6/7 retain the original tail bytes");
  table.Run(
      "affinity", 0x28, kAffinityFields, 1, true,
      "signed elemental affinity; storage_7 retains the original tail byte");
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
  limbs.Lead(IntegerColumn("limb", 0, kLimbCount - 1, "the limb"));
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

std::string HexOf(std::span<const std::byte> bytes) {
  std::string text;
  for (const std::byte value : bytes) {
    text += std::format("{:02x}", std::to_integer<unsigned>(value));
  }
  return text;
}

unsigned HexDigit(char digit) {
  return digit <= '9' ? static_cast<unsigned>(digit - '0')
                      : static_cast<unsigned>(digit - 'a') + 10;
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

// The record's fields as cells, in the layout's field order; an action
// reference as the action's id, empty for none.
std::expected<void, std::string> AppendCells(std::vector<std::string>& cells,
                                             const TableLayout& layout,
                                             std::span<const std::byte> record,
                                             ActionNames& actions) {
  for (const Field& field : layout.fields) {
    if (field.bytes) {
      cells.push_back(HexOf(record.subspan(field.offset, field.size)));
      continue;
    }
    const auto value = ReadField(record, field);
    const auto& column = layout.columns.at(cells.size());
    if (column.type == ColumnType::kReference) {
      if (value == 0 && column.optional) {
        cells.emplace_back();
        continue;
      }
      if (auto loaded = actions.Load(); !loaded) {
        return loaded;
      }
      const auto id = actions.IdOf(value);
      if (!id) {
        return std::unexpected(
            std::format("{} {}", column.name, UnheldAction(value)));
      }
      cells.emplace_back(*id);
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
                                          ActionNames& actions) {
  for (std::size_t index = 0; index < layout.fields.size(); ++index) {
    const Field& field = layout.fields[index];
    const std::size_t column =
        layout.columns.size() - layout.fields.size() + index;
    const std::string& cell = row.cells.at(column);
    if (field.bytes) {
      for (std::size_t at = 0; at < field.size; ++at) {
        record[field.offset + at] =
            static_cast<std::byte>((HexDigit(cell.at(2 * at)) << 4U) |
                                   HexDigit(cell.at((2 * at) + 1)));
      }
    } else if (layout.columns.at(column).type == ColumnType::kReference) {
      std::int64_t value = 0;
      if (!cell.empty()) {
        if (auto loaded = actions.Load(); !loaded) {
          return loaded;
        }
        const auto legacy = actions.LegacyOf(cell);
        if (!legacy) {
          return std::unexpected(CellError(
              row.source, row.line, layout.columns.at(column).name,
              std::format("no action '{}' in {}", cell, kActionsPath)));
        }
        value = *legacy;
      }
      WriteField(record, field, value);
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
        record[reserved.offset] = static_cast<std::byte>(
            ReservedValue(layout, reserved, record, slot));
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
  unsigned value = 0;
  const char* const first = cell.data();
  const auto parsed = std::from_chars(first, first + cell.size(), value);
  if (cell.empty() || parsed.ec != std::errc() ||
      parsed.ptr != first + cell.size()) {
    return std::unexpected(
        CellError(file, line, packed.legacy,
                  std::format("'{}' is not a decimal integer", cell)));
  }
  std::vector<std::string> cells;
  unsigned rebuilt = 0;
  for (const std::string_view column : packed.columns) {
    const auto field = std::ranges::find(layout.fields, column, &Field::column);
    if (field == layout.fields.end() || field->bits == 0) {
      return std::unexpected(
          std::format("{}: '{}' is not a bit field", file, column));
    }
    const unsigned part = (value >> field->bit) & ((1U << field->bits) - 1);
    rebuilt |= part << field->bit;
    cells.push_back(std::to_string(part));
  }
  if (rebuilt != value) {
    return std::unexpected(CellError(
        file, line, packed.legacy,
        std::format("{} does not fit the columns that replace it", value)));
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
    const auto found = std::ranges::find(header, reserved.legacy);
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
        return std::unexpected(
            std::format("{}: has '{}' but not '{}', which sets it", file,
                        reserved.legacy, sets));
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
          CellError(file, record.line, reserved.legacy,
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
    if (column.type != ColumnType::kReference) continue;
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

std::expected<ZoneTableText, std::string> FormatZoneTables(
    std::span<const ZoneRecord> zones, const ZoneTableReader& read) {
  ActionNames actions(read);
  std::vector<const ZoneRecord*> sorted;
  for (const ZoneRecord& zone : zones) sorted.push_back(&zone);
  std::ranges::sort(sorted, {}, &ZoneRecord::id);
  std::array<std::vector<std::vector<std::string>>, kZoneTableCount> rows;
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
                                  enemy.data, actions);
          !done) {
        return std::unexpected(std::format("{}: {}", enemy.id, done.error()));
      }
      enemy_rows.push_back(std::move(cells));
      for (std::size_t limb = 0; limb < static_cast<std::size_t>(kLimbCount);
           ++limb) {
        const auto bytes = LimbOf(enemy.data, limb);
        if (LimbFieldsZero(bytes)) continue;
        std::vector<std::string> limb_cells{enemy.id, std::format("{}", limb)};
        if (auto done = AppendCells(limb_cells, LayoutOf(ZoneTable::kLimbs),
                                    bytes, actions);
            !done) {
          return std::unexpected(
              std::format("{} limb {}: {}", enemy.id, limb, done.error()));
        }
        limb_rows.push_back(std::move(limb_cells));
      }
      for (const PartSlot& part : kPartSlots) {
        const auto bytes =
            std::span(enemy.data).subspan(part.offset, kPartBytes);
        if (AllZero(bytes)) continue;
        std::vector<std::string> part_cells{enemy.id, std::string(part.id)};
        if (auto done = AppendCells(part_cells, LayoutOf(ZoneTable::kParts),
                                    bytes, actions);
            !done) {
          return std::unexpected(
              std::format("{} {}: {}", enemy.id, part.id, done.error()));
        }
        part_rows.push_back(std::move(part_cells));
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
  ActionNames actions(read);
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
    if (auto done = ApplyRow(enemy.data, enemy_layout, row, actions); !done) {
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
                     LayoutOf(ZoneTable::kLimbs), row, actions);
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
                     LayoutOf(ZoneTable::kParts), row, actions);
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
  const auto mark = [&](const Field& field, std::size_t base) {
    for (std::size_t index = 0; index < field.size; ++index) {
      const unsigned mask =
          field.bits == 0 ? kByteMask : ((1U << field.bits) - 1) << field.bit;
      unsigned& bits = covered.at(base + field.offset + index);
      if ((bits & mask) != 0) overlap.at(base + field.offset + index) = true;
      bits |= mask;
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
        mark({.column = reserved.legacy,
              .offset = reserved.offset,
              .size = 1,
              .is_signed = false,
              .bytes = false,
              .bit = 0,
              .bits = 0},
             base);
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
        const unsigned expected = ReservedValue(layout, reserved, record, slot);
        const auto actual = std::to_integer<unsigned>(record[reserved.offset]);
        if (fault || actual == expected) return;
        const std::size_t at = base + reserved.offset;
        if (reserved.slot_byte) {
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
  ZoneColumnMigration migrated{.text = text};
  if (layout.packed.empty() && layout.reserved.empty() &&
      layout.resigned.empty() && !HasFieldOf(layout, ColumnType::kFlags) &&
      !HasFieldOf(layout, ColumnType::kReference)) {
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
  if (migrated.split_columns != 0 || migrated.retired_columns != 0 ||
      migrated.named_values != 0 || migrated.resigned_values != 0) {
    std::vector<std::vector<std::string>> rows;
    for (CsvRecord& record : *records) rows.push_back(std::move(record.fields));
    migrated.text = FormatCsv(rows);
  }
  return migrated;
}

}  // namespace VS::Content
