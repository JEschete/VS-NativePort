#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ZoneTables.hpp"
#include "TestSupport/ModelFixtures.hpp"
#include "TestSupport/ZoneTemplates.hpp"

namespace {

namespace content = VS::Content;
using content::ZoneTable;
using VS::test_support::ErrorOf;

using Texts = std::array<std::string, content::kZoneTableCount>;

std::string& TextOf(Texts& texts, ZoneTable table) {
  return texts.at(static_cast<std::size_t>(table));
}

content::ZoneTableReader ReaderOf(const Texts& texts) {
  return [&texts](
             std::string_view path) -> std::expected<std::string, std::string> {
    for (const ZoneTable table : content::kZoneTables) {
      if (content::ZoneTablePath(table) == path) {
        return texts.at(static_cast<std::size_t>(table));
      }
    }
    return std::unexpected(std::format("no table {}", path));
  };
}

// A template whose every byte differs from its neighbours, so a column at the
// wrong offset or width shows; `zeroed` clears the shield and a limb so their
// rows are left out.
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
  if (zeroed) {
    for (std::size_t at = 0x140; at < 0x170; ++at) data.at(at) = std::byte{};
    for (std::size_t at = 0x238; at < 0x258; ++at) data.at(at) = std::byte{};
    for (std::size_t at = 0x288; at < 0x294; ++at) data.at(at) = std::byte{};
  }
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

Texts SampleTexts() { return content::FormatZoneTables(SampleZones()); }

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
  CHECK(content::FormatZoneTables(ReadBack(texts)) == texts);
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

  // A name must be its 24 bytes in lowercase hexadecimal.
  Texts texts = SampleTexts();
  const auto zones = SampleZones();
  std::string name;
  for (std::size_t at = 0x04; at < 0x1C; ++at) {
    name += std::format("{:02x}", std::to_integer<unsigned>(
                                      zones.at(0).enemies.at(0).data.at(at)));
  }
  std::string upper = name;
  upper.front() = 'F';
  Edit(texts, ZoneTable::kEnemies, name, upper);
  CHECK(ErrorOf(content::ReadZoneTables(ReaderOf(texts)))
            .ends_with("column 'name': '" + upper +
                       "' is not 24 bytes as lowercase hexadecimal"));
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
  const auto texts = content::FormatZoneTables(zones);
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
  const auto split = content::SplitPackedZoneColumns(
      ZoneTable::kParts, "item_id,shape_bits,range\n5,41,2\n", "parts.csv");
  if (!split) FAIL(split.error());
  CHECK(split->split_columns == 1);
  CHECK(split->text == "item_id,attack_shape,attack_angle,range\n5,1,5,2\n");
  const auto again =
      content::SplitPackedZoneColumns(ZoneTable::kParts, split->text, "p.csv");
  if (!again) FAIL(again.error());
  CHECK(again->split_columns == 0);
  CHECK(again->text == split->text);
}

TEST_CASE("Packed shape bits that the split would change are refused",
          "[zone-authoring][content-tables][znd]") {
  const auto split = [](const std::string& text) {
    return ErrorOf(
        content::SplitPackedZoneColumns(ZoneTable::kParts, text, "parts.csv"));
  };
  CHECK(split("item_id,shape_bits\n1,256\n") ==
        "parts.csv:2: column 'shape_bits': 256 does not fit the columns that "
        "replace it");
  CHECK(split("item_id,shape_bits\n1,-1\n") ==
        "parts.csv:2: column 'shape_bits': '-1' is not a decimal integer");
  CHECK(split("shape_bits,attack_angle\n1,1\n") ==
        "parts.csv: has both 'shape_bits' and 'attack_angle'");
}
