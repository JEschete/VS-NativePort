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
