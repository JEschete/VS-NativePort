#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/CatalogTable.hpp"
#include "Content/ZndFile.hpp"

// The zone and enemy tables the MAP\ZONE*.ZND files convert to
// (ASSET_PIPELINE.md §6.9, #199, #517): CSV tables with schemas under
// catalogs\world\ and catalogs\enemies\, one row per zone, room slot, zone
// texture and enemy template, with join tables for each template's limbs and
// equipment parts. Rows name rooms, archetypes and images by stable content
// id, never by sector: the loader finds each file's sector and read size from
// the extracted disc files when it rebuilds the zone.
//
// Every byte of a 0x464-byte template (Actors/CharacterInit.hpp) is in
// exactly one column or is reserved: the enemy row holds the scalars and the
// weapon, shield and accessory fields around their parts; a limb row the
// limb's fields but its armour; a part row one equipment part (blade, grip,
// gem, shield, accessory or a limb's armour).
// A limb row names each action slot's action by its id in
// catalogs/battle/actions.csv, empty for none. Reserved bytes, which no code
// reads (§4.1), have no column: the template's padding at 0x033, 0x236 and
// 0x237 and each limb's at +0x5A and +0x5B read as zero, each limb's +0x52 as
// 1 exactly when the limb has an armour material, and each action slot's
// index (+0x13, +0x17, +0x1B, +0x1F) as the slot's position, as on the disc.
// The equipment indices (the weapon's at 0x126, the shield's at 0x202, the
// accessory's at 0x235, each limb armour's at +0x53 and each part's at +0x17),
// the inventory slots of the player's equipment, read as zero. The trailing
// ZUD index (0x460, 4 bytes) reads as the template's slot: it picks the
// zone's ZUD entry, which the row's archetype names. Bits 0-1 of 0x02C, below
// the AI settings' columns, read as zero, as do the class and affinity arrays'
// tail bytes no code reads: each limb's +0x0F and each part's +0x26, +0x27
// and +0x2F.
// A limb whose columns are all zero and a part whose bytes are all zero have
// no row, and a missing row reads as such, so a template rebuilds byte for
// byte.
namespace VS::Content {

inline constexpr std::string_view kZndTableConverterName = "znd-tables";
inline constexpr std::uint32_t kZndTableConverterVersion = 11;

enum class ZoneTable : std::uint8_t {
  kZones,     // catalogs/world/zones: music and room slot count
  kRooms,     // catalogs/world/zone-rooms: each used room slot
  kTextures,  // catalogs/world/zone-textures: the texture table's images
  kEnemies,   // catalogs/enemies/zone-enemies: one row per template
  kLimbs,     // catalogs/enemies/zone-enemy-limbs
  kParts,     // catalogs/enemies/zone-enemy-parts
};
inline constexpr std::size_t kZoneTableCount = 6;
inline constexpr std::array kZoneTables{
    ZoneTable::kZones,   ZoneTable::kRooms, ZoneTable::kTextures,
    ZoneTable::kEnemies, ZoneTable::kLimbs, ZoneTable::kParts,
};

[[nodiscard]] const TableSpec& ZoneTableSpec(ZoneTable table);
// "<directory>/<table>.csv" and "<directory>/<table>.schema.json" under the
// data root.
[[nodiscard]] std::string ZoneTablePath(ZoneTable table);
[[nodiscard]] std::string ZoneTableSchemaPath(ZoneTable table);

// <kind>.<directories>.<stem> in lowercase, e.g. MAP/ZONE009.ZND with kind
// "zone" is zone.map.zone009; nothing for a path without a directory and a
// stem made of id characters.
[[nodiscard]] std::expected<std::string, std::string> DiscFileId(
    std::string_view kind, std::string_view disc_path);
// The disc path DiscFileId gives `id` for, with `extension` (".MPD").
[[nodiscard]] std::expected<std::string, std::string> DiscPathOfFileId(
    std::string_view kind, std::string_view id, std::string_view extension);

inline constexpr std::string_view kZoneIdKind = "zone";
inline constexpr std::string_view kRoomIdKind = "room";
inline constexpr std::string_view kArchetypeIdKind = "archetype";
inline constexpr std::string_view kRoomExtension = ".MPD";

struct ZoneEnemyRecord {
  std::string id;         // enemy.map.zone009.00
  std::string archetype;  // the ZUD it loads, archetype.map.z009u00
  ZndTemplate data{};

  bool operator==(const ZoneEnemyRecord&) const = default;
};

struct ZoneRecord {
  std::string id;  // zone.map.zone009
  std::int32_t music_id = 0;
  // Each room slot's room id; empty for an empty slot.
  std::vector<std::string> rooms;
  // The texture table's images in table order; nothing when the zone has
  // no texture table.
  std::vector<std::string> textures;
  std::vector<ZoneEnemyRecord> enemies;  // in template order

  bool operator==(const ZoneRecord&) const = default;
};

// enemy.<zone id without its kind>.<slot, two digits>.
[[nodiscard]] std::string ZoneEnemyId(std::string_view zone_id,
                                      std::size_t slot);

// Each table's text by its path (ZoneTablePath), and the action catalog
// (kActionsPath) when a limb names an action.
using ZoneTableReader = std::function<std::expected<std::string, std::string>(
    std::string_view path)>;

using ZoneTableText = std::array<std::string, kZoneTableCount>;

// The six tables' text for `zones`, in kZoneTables order. Zones are written
// by id, their rooms, textures and enemies by position. `read` serves the
// action catalog the limbs' action ids are named from; a limb action the
// catalog does not hold is refused.
[[nodiscard]] std::expected<ZoneTableText, std::string> FormatZoneTables(
    std::span<const ZoneRecord> zones, const ZoneTableReader& read);

// The zones the six tables hold, by id. Each table must pass its schema; rows
// must name zones and enemies the tables hold, and actions the action catalog
// holds; a zone's rooms must lie within its room slots; and its textures and
// enemies must be numbered from 0 with no gap.
[[nodiscard]] std::expected<std::vector<ZoneRecord>, std::string>
ReadZoneTables(const ZoneTableReader& read);

// Whether the enemy, limb and part columns and the reserved bytes cover every
// bit of a template exactly once, and every flags column names each bit it
// holds; the error names the first byte or column at fault.
[[nodiscard]] std::expected<void, std::string> CheckZoneTemplateCoverage();

// Whether `data`, the template in `slot`, holds in its reserved bytes what
// the tables rebuild them as, so its rows load as its bytes; the error names
// the first byte at fault and its value.
[[nodiscard]] std::expected<void, std::string> CheckZoneReservedBytes(
    const ZndTemplate& data, std::size_t slot);

struct ZoneColumnMigration {
  std::string text;
  std::size_t split_columns = 0;
  std::size_t retired_columns = 0;
  std::size_t named_values = 0;
  std::size_t resigned_values = 0;
};

// Rewrites older authoring forms the current schema no longer reads, so it
// loads; the text is unchanged when none remain. Splits the former columns
// that packed several bit fields (the parts table's shape_bits and version 9's
// signed ai_settings) into the columns that replaced them, refusing a value
// they cannot hold and a table with both forms. Drops version 4's raw_<offset>
// columns for reserved bytes and version 5's action_N_index columns, refusing a
// cell that differs from the value the tables now give its byte. Names a flags
// cell written as a number (version 3's status_immunities), refusing a number
// its bytes cannot hold, and an action cell written as a number (version 5's
// action_N_id) by the id `read` serves for it in the action catalog, refusing
// one the catalog does not hold; 0 becomes an empty cell. Rewrites a limb's
// agility_defense_bonus of 128-255, which version 7 wrote unsigned, as the
// same byte's signed value (251 is -5). Column renames are left to
// NormalizeTableText.
[[nodiscard]] std::expected<ZoneColumnMigration, std::string>
MigrateZoneColumns(ZoneTable table, const std::string& text,
                   std::string_view file, const ZoneTableReader& read = {});

}  // namespace VS::Content
