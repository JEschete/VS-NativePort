#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ActionCatalog.hpp"
#include "Content/CatalogTable.hpp"
#include "Content/ZoneTables.hpp"
#include "TestSupport/ActionRoot.hpp"
#include "TestSupport/ModelFixtures.hpp"
#include "TestSupport/ZoneTemplates.hpp"

namespace {

namespace content = VS::Content;
using content::ZoneTable;
using VS::test_support::ErrorOf;
using VS::test_support::Required;

using Texts = std::array<std::string, content::kZoneTableCount>;

std::string& TextOf(Texts& texts, ZoneTable table) {
  return texts.at(static_cast<std::size_t>(table));
}

// Reads the synthetic action catalog and nothing else.
content::ZoneTableReader ActionReader() {
  return [](std::string_view path) -> std::expected<std::string, std::string> {
    if (path == content::kActionsPath) {
      return VS::test_support::SyntheticActionsText();
    }
    return std::unexpected(std::format("no table {}", path));
  };
}

content::ZoneTableReader ReaderOf(const Texts& texts) {
  return [&texts](
             std::string_view path) -> std::expected<std::string, std::string> {
    for (const ZoneTable table : content::kZoneTables) {
      if (content::ZoneTablePath(table) == path) {
        return texts.at(static_cast<std::size_t>(table));
      }
    }
    return ActionReader()(path);
  };
}

Texts Formatted(const std::vector<content::ZoneRecord>& zones,
                const content::ZoneTableReader& read = ActionReader()) {
  return Required(content::FormatZoneTables(zones, read));
}

// A template whose every byte differs from its neighbours, so a column at the
// wrong offset or width shows; `zeroed` clears the shield and a limb so their
// rows are left out. `seed` is also the template's slot.
content::ZndTemplate PatternTemplate(std::size_t seed, bool zeroed) {
  content::ZndTemplate data{};
  for (std::size_t at = 0; at < data.size(); ++at) {
    data.at(at) = static_cast<std::byte>((at * 7) + seed);
  }
  // Authoring enums use valid codes; other bytes retain the offset pattern.
  data[3] = static_cast<std::byte>((std::to_integer<unsigned>(data[3]) & 31U) |
                                   ((seed % 6) << 5));
  data[0x124] = static_cast<std::byte>(3 + seed % 5);
  data[0x200] = static_cast<std::byte>(seed % 8);
  for (std::size_t limb = 0; limb < 6; ++limb) {
    data[0x238 + limb * 0x5C + 0x50] =
        static_cast<std::byte>((seed + limb) % 8);
  }
  VS::test_support::UseValidPartCodes(data, seed);
  VS::test_support::UseKnownActions(data, seed);
  if (zeroed) {
    for (std::size_t at = 0x140; at < 0x170; ++at) data.at(at) = std::byte{};
    for (std::size_t at = 0x238; at < 0x258; ++at) data.at(at) = std::byte{};
    for (std::size_t at = 0x288; at < 0x294; ++at) data.at(at) = std::byte{};
  }
  VS::test_support::UseTextNames(data, seed);
  VS::test_support::UseReservedBytes(data, seed);
  return data;
}

std::vector<content::ZoneRecord> SampleZones() {
  content::ZoneRecord first{
      .id = "zone.map.zone009",
      .music_id = 54,
      .rooms = {"room.map.map009", "", "room.map.map010"},
      .textures = {"image.zone.zone009.00", "image.zone.zone009.01"},
      .enemies = {}};
  for (std::size_t slot = 0; slot < 2; ++slot) {
    first.enemies.push_back(
        {.id = content::ZoneEnemyId(first.id, slot),
         .archetype = std::format("archetype.map.z009u{:02}", slot),
         .data = PatternTemplate(slot, slot == 1)});
  }
  const content::ZoneRecord empty{.id = "zone.map.zone207",
                                  .music_id = -1,
                                  .rooms = {},
                                  .textures = {},
                                  .enemies = {}};
  return {first, empty};  // by id, as the tables read back
}

Texts SampleTexts() { return Formatted(SampleZones()); }

std::vector<content::ZoneRecord> ReadBack(const Texts& texts) {
  auto zones = content::ReadZoneTables(ReaderOf(texts));
  INFO(ErrorOf(zones));
  REQUIRE(zones.has_value());
  return *std::move(zones);
}

// Replaces the first occurrence of `from` in the table.
void Edit(Texts& texts, ZoneTable table, std::string_view from,
          std::string_view to) {
  std::string& text = TextOf(texts, table);
  const std::size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  text.replace(at, from.size(), to);
}

// The table's records and the index of `column` in its header.
std::pair<std::vector<content::CsvRecord>, std::size_t> RecordsOf(
    const Texts& texts, ZoneTable table, std::string_view column) {
  auto records = Required(content::ParseCsv(
      texts.at(static_cast<std::size_t>(table)), "table.csv"));
  const auto& header = records.front().fields;
  const auto found = std::ranges::find(header, column);
  REQUIRE(found != header.end());
  const auto at = static_cast<std::size_t>(found - header.begin());
  return {std::move(records), at};
}

// The cell under `column` in the table's `row`th record (1 is the first row).
std::string CellOf(const Texts& texts, ZoneTable table, std::size_t row,
                   std::string_view column) {
  const auto [records, at] = RecordsOf(texts, table, column);
  return records.at(row).fields.at(at);
}

void SetCell(Texts& texts, ZoneTable table, std::size_t row,
             std::string_view column, std::string_view value) {
  auto [records, at] = RecordsOf(texts, table, column);
  records.at(row).fields.at(at) = value;
  std::vector<std::vector<std::string>> rows;
  for (auto& record : records) rows.push_back(std::move(record.fields));
  TextOf(texts, table) = content::FormatCsv(rows);
}

// Reads the zone tables and no action catalog.
content::ZoneTableReader TablesOnly(const Texts& texts) {
  return [&texts](
             std::string_view path) -> std::expected<std::string, std::string> {
    for (const ZoneTable table : content::kZoneTables) {
      if (content::ZoneTablePath(table) == path) {
        return texts.at(static_cast<std::size_t>(table));
      }
    }
    return std::unexpected(std::format("cannot read {}", path));
  };
}

}  // namespace

TEST_CASE("Zone template columns cover every bit once",
          "[content-tables][znd]") {
  const auto covered = content::CheckZoneTemplateCoverage();
  INFO(ErrorOf(covered));
  CHECK(covered.has_value());
}

TEST_CASE("Zone ids follow their disc paths", "[content-tables][znd]") {
  CHECK(content::DiscFileId("zone", "MAP/ZONE009.ZND") == "zone.map.zone009");
  CHECK(content::DiscFileId("archetype", "MAP/Z009U00.ZUD") ==
        "archetype.map.z009u00");
  CHECK(content::DiscPathOfFileId("room", "room.map.map009", ".MPD") ==
        "MAP/MAP009.MPD");
  CHECK(ErrorOf(content::DiscFileId("zone", "ZONE009.ZND")) ==
        "ZONE009.ZND has no directory, stem and extension");
  CHECK(
      ErrorOf(content::DiscPathOfFileId("room", "zone.map.zone009", ".MPD")) ==
      "'zone.map.zone009' is not a room id");
  CHECK(ErrorOf(content::DiscPathOfFileId("room", "room.map009", ".MPD")) ==
        "'room.map009' names no directory");
  CHECK(content::ZoneEnemyId("zone.map.zone009", 3) == "enemy.map.zone009.03");
}

TEST_CASE("Zone tables round-trip every template byte",
          "[content-tables][znd]") {
  Texts texts = SampleTexts();
  CHECK(ReadBack(texts) == SampleZones());
  // The empty slot, the zeroed shield and the zeroed limb have no rows.
  const std::string& rooms = TextOf(texts, ZoneTable::kRooms);
  CHECK(rooms.find("zone.map.zone009,1,") == std::string::npos);
  CHECK(rooms.find("zone.map.zone009,2,room.map.map010") != std::string::npos);
  const std::string& parts = TextOf(texts, ZoneTable::kParts);
  CHECK(parts.find("enemy.map.zone009.00,shield.base,") != std::string::npos);
  CHECK(parts.find("enemy.map.zone009.01,shield.base,") == std::string::npos);
  const std::string& limbs = TextOf(texts, ZoneTable::kLimbs);
  CHECK(limbs.find("enemy.map.zone009.01,0,") == std::string::npos);
  CHECK(limbs.find("enemy.map.zone009.01,1,") != std::string::npos);
  // A second formatting of what was read writes the same text.
  CHECK(Formatted(ReadBack(texts)) == texts);
}

TEST_CASE("Zone table edits reach the rebuilt template",
          "[content-tables][znd]") {
  Texts texts = SampleTexts();
  // The first template's hp: the int16 at 0x1C.
  const auto sample = SampleZones();
  const auto& original = sample.at(0).enemies.at(0).data;
  const auto hp = static_cast<std::int16_t>(
      std::to_integer<unsigned>(original.at(0x1C)) |
      (std::to_integer<unsigned>(original.at(0x1D)) << 8U));
  Edit(texts, ZoneTable::kEnemies, std::format(",{},", hp), ",-5,");
  const auto zones = ReadBack(texts);
  const auto& data = zones.at(0).enemies.at(0).data;
  CHECK(data.at(0x1C) == std::byte{0xFB});
  CHECK(data.at(0x1D) == std::byte{0xFF});
}

TEST_CASE("Zone tables name the row at fault", "[content-tables][znd]") {
  struct Refused {
    ZoneTable table;
    std::string_view from;
    std::string_view to;
    std::string_view error;
  };
  const std::vector<Refused> refusals{
      {ZoneTable::kRooms, "zone.map.zone009,0,", "zone.map.zone010,0,",
       "catalogs/world/zone-rooms.csv:2: column 'zone': no zone "
       "'zone.map.zone010' in catalogs/world/zones.csv"},
      {ZoneTable::kRooms, "zone.map.zone009,2,", "zone.map.zone009,3,",
       "catalogs/world/zone-rooms.csv:3: column 'slot': 3 is past "
       "zone.map.zone009's 3 room slots"},
      {ZoneTable::kTextures, "zone.map.zone009,1,", "zone.map.zone009,2,",
       "catalogs/world/zone-textures.csv: zone.map.zone009 has no texture 1"},
      {ZoneTable::kEnemies, "zone.map.zone009,1,", "zone.map.zone009,0,",
       "catalogs/enemies/zone-enemies.csv:3: column 'slot': zone.map.zone009 "
       "already has a template in slot 0"},
      {ZoneTable::kEnemies, "zone.map.zone009,1,", "zone.map.zone009,2,",
       "catalogs/enemies/zone-enemies.csv: zone.map.zone009 has no template "
       "1"},
      {ZoneTable::kLimbs, "enemy.map.zone009.01,1,", "enemy.map.zone009.02,1,",
       "catalogs/enemies/zone-enemy-limbs.csv:8: column 'enemy': no template "
       "'enemy.map.zone009.02' in catalogs/enemies/zone-enemies.csv"},
      {ZoneTable::kParts, "enemy.map.zone009.00,weapon.blade,",
       "enemy.map.zone009.00,weapon.edge,",
       "catalogs/enemies/zone-enemy-parts.csv:2: column 'part': 'weapon.edge' "
       "is not an equipment part"},
  };
  for (const Refused& refused : refusals) {
    CAPTURE(refused.error);
    Texts texts = SampleTexts();
    Edit(texts, refused.table, refused.from, refused.to);
    CHECK(ErrorOf(content::ReadZoneTables(ReaderOf(texts))) == refused.error);
  }

  // A name must fit its slot with its E7 and be valid game text markup.
  Texts texts = SampleTexts();
  const std::string long_name(24, 'x');
  Edit(texts, ZoneTable::kEnemies, ",Aa 1,", "," + long_name + ",");
  CHECK(ErrorOf(content::ReadZoneTables(ReaderOf(texts))) ==
        "catalogs/enemies/zone-enemies.csv:2: column 'name': '" + long_name +
            "': encodes to 25 bytes with its E7; a name holds 24");
  texts = SampleTexts();
  Edit(texts, ZoneTable::kEnemies, ",Aa 1,", ",Aa {nope},");
  CHECK(ErrorOf(content::ReadZoneTables(ReaderOf(texts)))
            .ends_with("(in its markup 'Aa{spacing:6}{nope}')"));
}

TEST_CASE("Zone names are text that rebuilds their slots",
          "[zone-authoring][content-tables][znd]") {
  // 23 glyphs and the E7 fill a slot.
  auto texts = SampleTexts();
  const std::string full(23, 'x');
  Edit(texts, ZoneTable::kEnemies, ",Aa 1,", "," + full + ",");
  auto expected = SampleZones();
  auto& name = expected[0].enemies[0].data;
  std::fill(name.begin() + 4, name.begin() + 4 + 23, std::byte{59});
  name[4 + 23] = std::byte{0xE7};
  CHECK(ReadBack(texts) == expected);
  // Spaces between words are word gaps; gaps at the ends and other tokens
  // keep their markup; empty text is a zero slot.
  for (const std::string_view text :
       {"Dark Crusader", "A  B", "nothing{spacing:6}", "{spacing:6}A",
        "{{x}} {music}", "M\xC3\xBCllenkamp", ""}) {
    CAPTURE(text);
    const auto bytes = Required(content::EncodeZoneName(text, "name"));
    CHECK(Required(content::ZoneNameText(bytes, "name")) == text);
  }
  const auto gap = Required(content::EncodeZoneName("A B", "name"));
  CHECK(gap[1] == std::byte{0xFA});
  CHECK(gap[2] == std::byte{0x06});
}

TEST_CASE("Zone name bytes no text rebuilds are refused",
          "[zone-authoring][content-tables][znd]") {
  content::ZoneNameBytes bytes{};
  bytes[0] = std::byte{0x0A};
  bytes[1] = std::byte{0xE7};
  bytes[5] = std::byte{0xFF};
  CHECK(ErrorOf(content::ZoneNameText(bytes, "name")) ==
        "name: text byte 5: 0xFF after the E7 is not padding 0");
  bytes = {};
  bytes[0] = std::byte{0xE7};
  CHECK(ErrorOf(content::ZoneNameText(bytes, "name")) ==
        "name: the name is an E7 alone, which no text rebuilds");
  bytes = {};
  bytes[0] = std::byte{0x0A};
  bytes[1] = std::byte{0x8F};
  bytes[2] = std::byte{0x0B};
  bytes[3] = std::byte{0xE7};
  CHECK(ErrorOf(content::ZoneNameText(bytes, "name")) ==
        "name: the name holds the space glyph 0x8F, which its text writes as "
        "a word gap");
  CHECK(ErrorOf(content::ZoneNameText(std::span(bytes).first(23), "name")) ==
        "name: a name slot is 24 bytes, not 23");
}

TEST_CASE("Hexadecimal zone names migrate to text once",
          "[zone-authoring][content-tables][znd]") {
  // "Aa 1": A, a, a word gap (FA 06), 1 and the E7, then zero padding.
  const std::string hex = "0a24fa0601e7" + std::string(36, '0');
  const std::string none(48, '0');
  const auto migrated = content::MigrateZoneColumns(
      ZoneTable::kEnemies,
      std::format("id,name,weapon_name\na,{},{}\nb,Text,\n", hex, none),
      "enemies.csv");
  if (!migrated) FAIL(migrated.error());
  CHECK(migrated->decoded_texts == 2);
  CHECK(migrated->text == "id,name,weapon_name\na,Aa 1,\nb,Text,\n");
  const auto again =
      content::MigrateZoneColumns(ZoneTable::kEnemies, migrated->text, "e.csv");
  if (!again) FAIL(again.error());
  CHECK(again->decoded_texts == 0);
  CHECK(again->text == migrated->text);
  std::string tail = hex;
  tail.replace(12, 2, "ff");
  CHECK(ErrorOf(content::MigrateZoneColumns(
            ZoneTable::kEnemies, std::format("id,name\na,{}\n", tail),
            "enemies.csv")) ==
        "enemies.csv:2: column 'name': its bytes: text byte 6: 0xFF after the "
        "E7 is not padding 0");
}

TEST_CASE("Zone named field aliases preserve every template byte",
          "[zone-authoring][content-tables][znd]") {
  auto texts = SampleTexts();
  Edit(texts, ZoneTable::kEnemies, "movement_class,", "material,");
  Edit(texts, ZoneTable::kParts, "class_undead,", "class_3,");
  Edit(texts, ZoneTable::kLimbs, "affinity_fire,", "affinity_3,");
  CHECK(ReadBack(texts) == SampleZones());
  const auto normalized = content::NormalizeTableText(
      content::ZoneTableSpec(ZoneTable::kParts),
      TextOf(texts, ZoneTable::kParts), "parts.csv");
  if (!normalized) FAIL(normalized.error());
  CHECK(normalized->renamed_columns == 1);
  CHECK(normalized->text.contains("class_undead,"));
  const auto again = content::NormalizeTableText(
      content::ZoneTableSpec(ZoneTable::kParts), normalized->text, "parts.csv");
  if (!again) FAIL(again.error());
  CHECK(again->renamed_columns == 0);
  CHECK(again->text == normalized->text);
}

TEST_CASE("Named affinity edits reach the correct equipment byte",
          "[zone-authoring][content-tables][znd]") {
  auto texts = SampleTexts();
  auto rows = content::ParseCsv(TextOf(texts, ZoneTable::kParts), "parts.csv");
  if (!rows) FAIL(rows.error());
  auto& header = rows->front().fields;
  const auto found = std::ranges::find(header, "affinity_fire");
  REQUIRE(found != header.end());
  rows->at(1).fields[static_cast<std::size_t>(found - header.begin())] = "-25";
  std::vector<std::vector<std::string>> records;
  for (const auto& row : *rows) records.push_back(row.fields);
  TextOf(texts, ZoneTable::kParts) = content::FormatCsv(records);
  auto expected = SampleZones();
  expected[0].enemies[0].data[0x34 + 0x28 + 2] = std::byte{0xE7};
  CHECK(ReadBack(texts) == expected);
}

TEST_CASE(
    "Unsupported enemy classes remain raw-codec data, not authoring values",
    "[zone-authoring][content-tables][znd]") {
  auto zones = SampleZones();
  zones[0].enemies[0].data[3] = std::byte{0xFF};
  const auto texts = Formatted(zones);
  const auto read = content::ReadZoneTables(ReaderOf(texts));
  REQUIRE_FALSE(read.has_value());
  CHECK(read.error().contains("column 'enemy_class'"));
  content::ZndFile source;
  source.enemies.push_back(
      {.zud = {.sector = 1, .bytes = 2048}, .data = zones[0].enemies[0].data});
  const auto encoded = content::EncodeZndFile(source);
  if (!encoded) FAIL(encoded.error());
  const auto decoded = content::DecodeZndFile(*encoded);
  if (!decoded) FAIL(decoded.error());
  CHECK(decoded->enemies[0].data == source.enemies[0].data);
}

TEST_CASE("Packed shape bits split into shape and angle columns",
          "[zone-authoring][content-tables][znd]") {
  // 41 is shape 1 (bits 0-2) and angle 5 (bits 3-7).
  const auto split = content::MigrateZoneColumns(
      ZoneTable::kParts, "item_id,shape_bits,range\n5,41,2\n", "parts.csv");
  if (!split) FAIL(split.error());
  CHECK(split->split_columns == 1);
  CHECK(split->text == "item_id,attack_shape,attack_angle,range\n5,1,5,2\n");
  const auto again =
      content::MigrateZoneColumns(ZoneTable::kParts, split->text, "p.csv");
  if (!again) FAIL(again.error());
  CHECK(again->split_columns == 0);
  CHECK(again->text == split->text);
}

TEST_CASE("Packed shape bits that the split would change are refused",
          "[zone-authoring][content-tables][znd]") {
  const auto split = [](const std::string& text) {
    return ErrorOf(
        content::MigrateZoneColumns(ZoneTable::kParts, text, "parts.csv"));
  };
  CHECK(split("item_id,shape_bits\n1,256\n") ==
        "parts.csv:2: column 'shape_bits': 256 does not fit the columns that "
        "replace it");
  CHECK(split("item_id,shape_bits\n1,-1\n") ==
        "parts.csv:2: column 'shape_bits': '-1' is not a decimal integer");
  CHECK(split("shape_bits,attack_angle\n1,1\n") ==
        "parts.csv: has both 'shape_bits' and 'attack_angle'");
}

TEST_CASE("Signed AI settings words split into their named columns",
          "[zone-authoring][content-tables][znd]") {
  constexpr std::string_view kColumns =
      "facing_delay_preset,wander_preset,approaches_target_point,"
      "step_height_preset,tile_step_limit,walks_to_waypoints,"
      "chases_lost_target,area_lock,ignores_line_of_sight,view_angle_preset,"
      "perception_range_preset,relation_category,decays_hostility,"
      "shares_ally_hostility,hostile_on_harm,hostile_on_unprovoked_harm,"
      "can_ally_with_ashley,flee_rule,terrain_preset";
  // 0xC11D2A1C: facing 3, wander 1, step height 2, tile limit 2, bit 13,
  // view 1, range 7, bit 24 and terrain 3; its unsigned form splits the same.
  constexpr std::string_view kCells = "3,1,0,2,2,0,1,0,0,1,7,0,0,1,0,0,0,0,3";
  const auto split = content::MigrateZoneColumns(
      ZoneTable::kEnemies,
      "id,ai_settings,hp\na,-1055053284,9\nb,3239914012,8\nc,0,7\n",
      "enemies.csv");
  if (!split) FAIL(split.error());
  CHECK(split->split_columns == 1);
  CHECK(split->text ==
        std::format("id,{},hp\na,{},9\nb,{},8\nc,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
                    "0,0,0,0,0,7\n",
                    kColumns, kCells, kCells));
  const auto refused = [](const std::string& text) {
    return ErrorOf(
        content::MigrateZoneColumns(ZoneTable::kEnemies, text, "enemies.csv"));
  };
  // Bits 0-1 are padding no column holds.
  CHECK(refused("id,ai_settings\na,-1\n") ==
        "enemies.csv:2: column 'ai_settings': -1 does not fit the columns "
        "that replace it: bits 0x3 have no column");
  CHECK(refused("id,ai_settings\na,4294967296\n") ==
        "enemies.csv:2: column 'ai_settings': 4294967296 does not fit the "
        "columns that replace it");
  CHECK(refused("id,ai_settings\na,-2147483652\n") ==
        "enemies.csv:2: column 'ai_settings': -2147483652 does not fit the "
        "columns that replace it");
}

TEST_CASE("Numeric status immunities migrate to status names",
          "[zone-authoring][content-tables][znd]") {
  // 147456 is bits 14 and 17; version 3 wrote bit 31 alone as -2147483648.
  const auto migrated = content::MigrateZoneColumns(
      ZoneTable::kEnemies,
      "id,status_immunities\na,147456\nb,-2147483648\nc,0\nd,poison\n",
      "enemies.csv");
  if (!migrated) FAIL(migrated.error());
  CHECK(migrated->named_values == 3);
  CHECK(migrated->text ==
        "id,status_immunities\na,poison|regeneration\nb,spell-immunity\n"
        "c,none\nd,poison\n");
  const auto again =
      content::MigrateZoneColumns(ZoneTable::kEnemies, migrated->text, "e.csv");
  if (!again) FAIL(again.error());
  CHECK(again->named_values == 0);
  CHECK(again->text == migrated->text);
}

TEST_CASE("Signed and unsigned status immunity numbers name the same flags",
          "[zone-authoring][content-tables][znd]") {
  const auto all = content::MigrateZoneColumns(
      ZoneTable::kEnemies, "status_immunities\n-1\n4294967295\n", "e.csv");
  if (!all) FAIL(all.error());
  const auto rows = content::ParseCsv(all->text, "e.csv");
  if (!rows) FAIL(rows.error());
  REQUIRE(rows->size() == 3);
  CHECK(rows->at(1).fields == rows->at(2).fields);
  CHECK(rows->at(1).fields.at(0).starts_with("head-impaired|"));
  CHECK(rows->at(1).fields.at(0).ends_with("|instant-defeat|spell-immunity"));
}

TEST_CASE("Status immunity numbers their bytes cannot hold are refused",
          "[zone-authoring][content-tables][znd]") {
  const auto migrate = [](const std::string& text) {
    return ErrorOf(
        content::MigrateZoneColumns(ZoneTable::kEnemies, text, "enemies.csv"));
  };
  CHECK(migrate("status_immunities\n4294967296\n") ==
        "enemies.csv:2: column 'status_immunities': 4294967296 is outside "
        "-2147483648..4294967295");
  CHECK(migrate("status_immunities\n-2147483649\n") ==
        "enemies.csv:2: column 'status_immunities': -2147483649 is outside "
        "-2147483648..4294967295");
  CHECK(migrate("status_immunities\n99999999999999999999\n") ==
        "enemies.csv:2: column 'status_immunities': 99999999999999999999 is "
        "outside -2147483648..4294967295");
}

TEST_CASE(
    "Reserved limb byte columns retire when they hold their bytes' values",
    "[zone-authoring][content-tables][znd]") {
  // Version 1 wrote materials as numbers, later versions as names.
  const auto limbs = content::MigrateZoneColumns(
      ZoneTable::kLimbs,
      "limb,armor_material,raw_52,raw_5a,raw_5b\n0,none,0,0,0\n1,3,1,0,0\n"
      "2,leather,1,0,0\n",
      "limbs.csv");
  if (!limbs) FAIL(limbs.error());
  CHECK(limbs->retired_columns == 3);
  CHECK(limbs->text == "limb,armor_material\n0,none\n1,3\n2,leather\n");
  const auto again =
      content::MigrateZoneColumns(ZoneTable::kLimbs, limbs->text, "l.csv");
  if (!again) FAIL(again.error());
  CHECK(again->retired_columns == 0);
  CHECK(again->text == limbs->text);
}

TEST_CASE("Reserved enemy byte columns retire when they hold zero",
          "[zone-authoring][content-tables][znd]") {
  const auto enemies = content::MigrateZoneColumns(
      ZoneTable::kEnemies, "id,raw_33,hp,raw_236,raw_237\na,0,5,0,0\n",
      "enemies.csv");
  if (!enemies) FAIL(enemies.error());
  CHECK(enemies->retired_columns == 3);
  CHECK(enemies->text == "id,hp\na,5\n");
}

TEST_CASE("Equipment index columns retire when they hold zero",
          "[zone-authoring][content-tables][znd]") {
  const auto enemies = content::MigrateZoneColumns(
      ZoneTable::kEnemies,
      "id,weapon_index,hp,shield_index,accessory_index\na,0,5,0,0\n",
      "enemies.csv");
  if (!enemies) FAIL(enemies.error());
  CHECK(enemies->retired_columns == 3);
  CHECK(enemies->text == "id,hp\na,5\n");
  const auto limbs = content::MigrateZoneColumns(
      ZoneTable::kLimbs, "limb,armor_index\n0,0\n", "limbs.csv");
  if (!limbs) FAIL(limbs.error());
  CHECK(limbs->retired_columns == 1);
  CHECK(limbs->text == "limb\n0\n");
}

TEST_CASE("The ZUD index column retires when it holds each row's slot",
          "[zone-authoring][content-tables][znd]") {
  const auto enemies = content::MigrateZoneColumns(
      ZoneTable::kEnemies, "id,slot,zud_index,hp\na,0,0,5\nb,1,1,6\n",
      "enemies.csv");
  if (!enemies) FAIL(enemies.error());
  CHECK(enemies->retired_columns == 1);
  CHECK(enemies->text == "id,slot,hp\na,0,5\nb,1,6\n");
}

TEST_CASE("Part index columns retire when they hold zero",
          "[zone-authoring][content-tables][znd]") {
  const auto parts = content::MigrateZoneColumns(
      ZoneTable::kParts, "item_id,index,range\n5,0,2\n", "parts.csv");
  if (!parts) FAIL(parts.error());
  CHECK(parts->retired_columns == 1);
  CHECK(parts->text == "item_id,range\n5,2\n");
  const auto again =
      content::MigrateZoneColumns(ZoneTable::kParts, parts->text, "p.csv");
  if (!again) FAIL(again.error());
  CHECK(again->retired_columns == 0);
  CHECK(again->text == parts->text);
}

TEST_CASE("Class and affinity tail columns retire when they hold zero",
          "[zone-authoring][content-tables][znd]") {
  // Versions 2-10 named the tails storage_N; version 1 numbered them.
  const auto parts = content::MigrateZoneColumns(
      ZoneTable::kParts,
      "item_id,class_evil,class_storage_6,class_storage_7,affinity_dark,"
      "affinity_storage_7\n5,-3,0,0,2,0\n",
      "parts.csv");
  if (!parts) FAIL(parts.error());
  CHECK(parts->retired_columns == 3);
  CHECK(parts->text == "item_id,class_evil,affinity_dark\n5,-3,2\n");
  const auto again =
      content::MigrateZoneColumns(ZoneTable::kParts, parts->text, "p.csv");
  if (!again) FAIL(again.error());
  CHECK(again->retired_columns == 0);
  CHECK(again->text == parts->text);
  const auto numbered = content::MigrateZoneColumns(
      ZoneTable::kParts, "item_id,class_7,class_8,affinity_8\n5,0,0,0\n",
      "parts.csv");
  if (!numbered) FAIL(numbered.error());
  CHECK(numbered->retired_columns == 3);
  CHECK(numbered->text == "item_id\n5\n");
  constexpr std::array<std::string_view, 2> kLimbTails{"affinity_storage_7",
                                                       "affinity_8"};
  for (const std::string_view column : kLimbTails) {
    CAPTURE(column);
    const auto limbs = content::MigrateZoneColumns(
        ZoneTable::kLimbs, std::format("limb,{}\n0,0\n", column), "limbs.csv");
    if (!limbs) FAIL(limbs.error());
    CHECK(limbs->retired_columns == 1);
    CHECK(limbs->text == "limb\n0\n");
  }
}

TEST_CASE("Reserved byte columns unlike their bytes' values are refused",
          "[zone-authoring][content-tables][znd]") {
  const auto retire = [](ZoneTable table, const std::string& text) {
    return ErrorOf(content::MigrateZoneColumns(table, text, "t.csv"));
  };
  CHECK(retire(ZoneTable::kEnemies, "id,raw_33\na,5\n") ==
        "t.csv:2: column 'raw_33': '5' is not 0; byte 0x33 is padding the "
        "tables keep zero");
  CHECK(retire(ZoneTable::kEnemies, "id,weapon_index\na,1\n") ==
        "t.csv:2: column 'weapon_index': '1' is not 0; byte 0x126 always "
        "holds 0 now");
  CHECK(retire(ZoneTable::kParts, "item_id,index\n5,3\n") ==
        "t.csv:2: column 'index': '3' is not 0; byte 0x17 always holds 0 "
        "now");
  CHECK(retire(ZoneTable::kParts, "item_id,class_storage_6\n5,4\n") ==
        "t.csv:2: column 'class_storage_6': '4' is not 0; byte 0x26 always "
        "holds 0 now");
  CHECK(retire(ZoneTable::kLimbs, "limb,affinity_8\n0,-1\n") ==
        "t.csv:2: column 'affinity_8': '-1' is not 0; byte 0x0F always "
        "holds 0 now");
  CHECK(retire(ZoneTable::kLimbs, "armor_material,raw_52\niron,0\n") ==
        "t.csv:2: column 'raw_52': '0' is not 1; armor_material sets byte "
        "0x52 now");
  CHECK(retire(ZoneTable::kLimbs, "armor_material,raw_52\nnone,1\n") ==
        "t.csv:2: column 'raw_52': '1' is not 0; armor_material sets byte "
        "0x52 now");
  CHECK(retire(ZoneTable::kLimbs, "armor_material,raw_52\ntin,1\n") ==
        "t.csv:2: column 'armor_material': 'tin' is not a name or number");
  CHECK(retire(ZoneTable::kLimbs, "raw_52\n0\n") ==
        "t.csv: has 'raw_52' but not 'armor_material', which sets it");
  CHECK(retire(ZoneTable::kEnemies, "id,slot,zud_index\na,0,0\nb,1,0\n") ==
        "t.csv:3: column 'zud_index': '0' is not 1; the template's slot sets "
        "it now");
  CHECK(retire(ZoneTable::kEnemies, "id,slot,zud_index\na,x,0\n") ==
        "t.csv:2: column 'slot': 'x' is not a number");
  CHECK(retire(ZoneTable::kEnemies, "id,zud_index\na,0\n") ==
        "t.csv: has 'zud_index' but not 'slot', which sets it");
}

TEST_CASE("A limb's armour material sets its reserved byte",
          "[zone-authoring][content-tables][znd]") {
  auto texts = SampleTexts();
  auto rows = content::ParseCsv(TextOf(texts, ZoneTable::kLimbs), "limbs.csv");
  if (!rows) FAIL(rows.error());
  const auto& header = rows->front().fields;
  const auto found = std::ranges::find(header, "armor_material");
  REQUIRE(found != header.end());
  const auto at = static_cast<std::size_t>(found - header.begin());
  // The first template's limb 0 has no material and limb 1 wood.
  REQUIRE(rows->at(1).fields.at(at) == "none");
  REQUIRE(rows->at(2).fields.at(at) == "wood");
  rows->at(1).fields.at(at) = "iron";
  rows->at(2).fields.at(at) = "none";
  std::vector<std::vector<std::string>> records;
  for (const auto& row : *rows) records.push_back(row.fields);
  TextOf(texts, ZoneTable::kLimbs) = content::FormatCsv(records);
  auto expected = SampleZones();
  auto& data = expected[0].enemies[0].data;
  data[0x238 + 0x50] = std::byte{4};
  data[0x238 + 0x52] = std::byte{1};
  data[0x294 + 0x50] = std::byte{};
  data[0x294 + 0x52] = std::byte{};
  CHECK(ReadBack(texts) == expected);
}

TEST_CASE("Limb action slots name actions from the action catalog",
          "[content-tables][znd]") {
  Texts texts = SampleTexts();
  // The first template's limb 0 holds actions 0-3 and limb 1 actions 4-7.
  CHECK(CellOf(texts, ZoneTable::kLimbs, 1, "action_1").empty());
  CHECK(CellOf(texts, ZoneTable::kLimbs, 1, "action_2") == "action.move-ab");
  CHECK(CellOf(texts, ZoneTable::kLimbs, 2, "action_4") == "action.rend");
  // The slot index bytes are rebuilt, not columns.
  CHECK_FALSE(TextOf(texts, ZoneTable::kLimbs).contains("action_1_index"));
  CHECK(content::FormatTableSchema(content::ZoneTableSpec(ZoneTable::kLimbs))
            .contains("\"optional\": true"));
  SetCell(texts, ZoneTable::kLimbs, 1, "action_1", "action.rend");
  auto expected = SampleZones();
  expected[0].enemies[0].data[0x238 + 0x10] = std::byte{7};
  CHECK(ReadBack(texts) == expected);
  SetCell(texts, ZoneTable::kLimbs, 1, "action_1", "action.nothing");
  CHECK(ErrorOf(content::ReadZoneTables(ReaderOf(texts))) ==
        "catalogs/enemies/zone-enemy-limbs.csv:2: column 'action_1': no "
        "action 'action.nothing' in catalogs/battle/actions.csv");
}

TEST_CASE("Limb actions the action catalog cannot name are refused",
          "[content-tables][znd]") {
  const Texts texts = SampleTexts();
  CHECK(ErrorOf(content::ReadZoneTables(TablesOnly(texts))) ==
        "naming limb actions: cannot read catalogs/battle/actions.csv");
  CHECK(ErrorOf(content::FormatZoneTables(SampleZones(),
                                          content::ZoneTableReader{})) ==
        "enemy.map.zone009.00 limb 0: naming limb actions needs "
        "catalogs/battle/actions.csv");
  auto zones = SampleZones();
  zones[0].enemies[0].data[0x238 + 0x11] = std::byte{1};
  CHECK(ErrorOf(content::FormatZoneTables(zones, ActionReader())) ==
        "enemy.map.zone009.00 limb 0: action_1 names action 256, which "
        "catalogs/battle/actions.csv does not hold");
}

TEST_CASE("Limb agility bonuses are signed bytes", "[content-tables][znd]") {
  Texts texts = SampleTexts();
  // The first template's limb 0 holds 150 at +0x02.
  CHECK(CellOf(texts, ZoneTable::kLimbs, 1, "agility_defense_bonus") == "-106");
  SetCell(texts, ZoneTable::kLimbs, 1, "agility_defense_bonus", "-5");
  auto expected = SampleZones();
  expected[0].enemies[0].data[0x238 + 0x02] = std::byte{0xFB};
  CHECK(ReadBack(texts) == expected);
}

TEST_CASE("Unsigned limb agility bonuses migrate to signed values",
          "[zone-authoring][content-tables][znd]") {
  // Version 7 wrote the byte unsigned; the schema refuses 256 as before.
  const auto migrated = content::MigrateZoneColumns(
      ZoneTable::kLimbs,
      "limb,agility_defense_bonus\n0,251\n1,5\n2,-3\n3,128\n4,255\n5,256\n",
      "limbs.csv");
  if (!migrated) FAIL(migrated.error());
  CHECK(migrated->resigned_values == 3);
  CHECK(migrated->text ==
        "limb,agility_defense_bonus\n0,-5\n1,5\n2,-3\n3,-128\n4,-1\n5,256\n");
  const auto again =
      content::MigrateZoneColumns(ZoneTable::kLimbs, migrated->text, "l.csv");
  if (!again) FAIL(again.error());
  CHECK(again->resigned_values == 0);
  CHECK(again->text == migrated->text);
}

TEST_CASE("Numeric limb action ids migrate to action names",
          "[zone-authoring][content-tables][znd]") {
  const auto migrated = content::MigrateZoneColumns(
      ZoneTable::kLimbs,
      "limb,action_1_id,action_1_index,action_2_id,action_2_index\n"
      "0,0,0,7,1\n1,2,0,action.rend,1\n",
      "limbs.csv", ActionReader());
  if (!migrated) FAIL(migrated.error());
  CHECK(migrated->retired_columns == 2);
  CHECK(migrated->named_values == 3);
  CHECK(migrated->text ==
        "limb,action_1_id,action_2_id\n0,,action.rend\n"
        "1,action.move-ac,action.rend\n");
  const auto again = content::MigrateZoneColumns(
      ZoneTable::kLimbs, migrated->text, "l.csv", ActionReader());
  if (!again) FAIL(again.error());
  CHECK(again->retired_columns == 0);
  CHECK(again->named_values == 0);
  CHECK(again->text == migrated->text);
}

TEST_CASE("Limb action columns their bytes cannot hold are refused",
          "[zone-authoring][content-tables][znd]") {
  const auto migrate = [](const std::string& text,
                          const content::ZoneTableReader& read) {
    return ErrorOf(
        content::MigrateZoneColumns(ZoneTable::kLimbs, text, "t.csv", read));
  };
  CHECK(migrate("action_2_index\n0\n", ActionReader()) ==
        "t.csv:2: column 'action_2_index': '0' is not 1; byte 0x17 always "
        "holds 1 now");
  CHECK(migrate("action_1_id\n300\n", ActionReader()) ==
        "t.csv:2: column 'action_1_id': names action 300, which "
        "catalogs/battle/actions.csv does not hold");
  CHECK(migrate("action_1_id\n5\n", {}) ==
        "naming limb actions needs catalogs/battle/actions.csv");
}
