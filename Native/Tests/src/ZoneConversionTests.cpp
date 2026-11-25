#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ActionCatalog.hpp"
#include "Content/ActorArchetype.hpp"
#include "Content/CatalogTable.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/ConvertedZones.hpp"
#include "Content/DataStore.hpp"
#include "Content/GameTextCodec.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/TimFile.hpp"
#include "Content/ZndFile.hpp"
#include "Content/ZoneTables.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Extract/ZndConversion.hpp"
#include "Extract/ZoneAuthoring.hpp"
#include "TestSupport/ActionRoot.hpp"
#include "TestSupport/ModelFixtures.hpp"
#include "TestSupport/ZoneTemplates.hpp"

namespace {

namespace fs = std::filesystem;
namespace content = VS::Content;
namespace extract = VS::Extract;
using Bytes = std::vector<std::byte>;
using content::ZoneTable;
using VS::test_support::ErrorOf;
using VS::test_support::ExtractedModels;
using VS::test_support::ModelRoot;
using VS::test_support::ReadModel;

constexpr std::size_t kExtractedZoneCount = 256;

// The fixture's files from ModelRoot::kFirstLba on: two rooms, a ZUD and the
// zone, each starting on the sector after the one before.
constexpr std::int32_t kRoomSector = 100;       // 3000 bytes, 2 sectors
constexpr std::int32_t kSmallRoomSector = 102;  // 100 bytes, 1 sector
constexpr std::int32_t kZudSector = 103;        // 5000 bytes, 3 sectors
constexpr std::uint32_t kZoneSector = 106;
constexpr std::int32_t kSector = 2048;
constexpr std::string_view kZonePath = "MAP/ZONE001.ZND";
constexpr std::string_view kZudPath = "MAP/Z001U00.ZUD";
constexpr std::string_view kArchetypeId = "archetype.map.z001u00";
constexpr std::string_view kEnemyId = "enemy.map.zone001.00";
// The hp field of a template: an int16 at 0x1C.
constexpr std::size_t kHpAt = 0x1C;

// A 16-bit TIM of `width` x 1 words at (x, y), word i being first + i.
Bytes Tim(std::int16_t x, std::int16_t y, std::uint16_t width,
          std::uint16_t first) {
  content::TimFile tim{
      .id = content::kTimMagic,
      .flags = 2,
      .clut = std::nullopt,
      .image = {.rect = {.x = x, .y = y, .width = width, .height = 1},
                .words = {}}};
  for (std::uint16_t index = 0; index < width; ++index) {
    tim.image.words.push_back(static_cast<std::uint16_t>(first + index));
  }
  auto bytes = content::EncodeTim(tim);
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

content::ZndFile SampleZone() {
  content::ZndFile zone;
  zone.music_id = 7;
  zone.rooms = {{.sector = kRoomSector, .bytes = 2 * kSector},
                {.sector = 0, .bytes = 0},
                {.sector = kSmallRoomSector, .bytes = kSector}};
  content::ZndEnemy enemy;
  enemy.zud = {.sector = kZudSector, .bytes = 3 * kSector};
  for (std::size_t at = 0; at < enemy.data.size(); ++at) {
    enemy.data.at(at) = static_cast<std::byte>((at * 13) + 5);
  }
  // Valid native authoring domains, without changing the other byte patterns.
  enemy.data[3] = std::byte{0x65};
  enemy.data[0x124] = std::byte{5};
  enemy.data[0x200] = std::byte{3};
  for (std::size_t limb = 0; limb < 6; ++limb) {
    enemy.data[0x238 + limb * 0x5C + 0x50] = static_cast<std::byte>(limb + 1);
  }
  VS::test_support::UseValidPartCodes(enemy.data, 1);
  VS::test_support::UseKnownActions(enemy.data, 1);
  VS::test_support::UseReservedBytes(enemy.data);
  // Status immunities with bit 31, which version 3 wrote as a negative number,
  // and AI settings with bit 31, which version 9 did.
  enemy.data[0x2B] |= std::byte{0x80};
  enemy.data[0x2F] |= std::byte{0x80};
  zone.enemies.push_back(enemy);
  // A room palette row with STP words and a texture page.
  zone.textures =
      std::vector<Bytes>{Tim(768, 224, 16, 0x8001), Tim(320, 256, 4, 0x1234)};
  return zone;
}

Bytes Encoded(const content::ZndFile& zone) {
  auto bytes = content::EncodeZndFile(zone);
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

std::vector<extract::SourceFile> ZoneSources(const Bytes& zone) {
  return {{.path = "MAP/MAP001.MPD", .bytes = Bytes(3000, std::byte{1})},
          {.path = "MAP/MAP002.MPD", .bytes = Bytes(100, std::byte{2})},
          {.path = std::string(kZudPath), .bytes = Bytes(5000, std::byte{3})},
          {.path = std::string(kZonePath), .bytes = zone}};
}

// Records the archetype the zone names, as the zud-json converter would, and
// publishes the synthetic action catalog the template's limbs name.
void RecordArchetype(const fs::path& root) {
  VS::test_support::WriteTestFile(root, content::kActionsPath,
                                  VS::test_support::SyntheticActionsText());
  const auto bytes = ReadModel(root / "disc" / fs::path(kZudPath));
  content::ConversionManifest manifest;
  extract::ReplaceConverterOutputs(
      manifest, content::kZudArchetypeConverterName, 1,
      {{.id = std::string(kArchetypeId),
        .source = std::string(kZudPath),
        .source_sha256 =
            extract::HashOf({.path = std::string(kZudPath), .bytes = bytes}),
        .converter = std::string(content::kZudArchetypeConverterName),
        .files = {"models/zud/Z001U00.json"},
        .dependencies = {},
        .unreferenced = false,
        .note = {},
        .alias_of = {}}});
  REQUIRE(extract::WriteConversionManifest(root, manifest).has_value());
}

extract::ZoneConversionReport Convert(const ModelRoot& root,
                                      bool force = false) {
  auto report = extract::ConvertDiscZones(root.Root(), force);
  INFO(ErrorOf(report));
  REQUIRE(report.has_value());
  return *report;
}

std::expected<Bytes, std::string> Load(const ModelRoot& root) {
  content::DataStore::ScopedOverride restore;
  if (auto opened = content::DataStore::Open(root.Root()); !opened) {
    return std::unexpected(opened.error());
  }
  return content::LoadConvertedZone(*content::DataStore::Get(), kZoneSector);
}

Bytes Loaded(const ModelRoot& root) {
  auto bytes = Load(root);
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

std::string ReadText(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.good());
  return {std::istreambuf_iterator<char>(file), {}};
}

void WriteText(const fs::path& path, std::string_view text) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
  REQUIRE(file.good());
}

fs::path TableFile(const ModelRoot& root, ZoneTable table) {
  return root.Root() / fs::path(content::ZoneTablePath(table));
}

// Replaces the first `from` in the table file.
void EditTable(const ModelRoot& root, ZoneTable table, std::string_view from,
               std::string_view to) {
  const fs::path path = TableFile(root, table);
  std::string text = ReadText(path);
  const std::size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  text.replace(at, from.size(), to);
  WriteText(path, text);
}

// Packs the parts table's attack_shape and attack_angle back into the single
// shape_bits byte that version 2 tables had.
void PackShapeBits(std::vector<std::vector<std::string>>& rows,
                   const std::vector<content::TableRow>& checked) {
  auto& header = rows.front();
  const auto shape = std::ranges::find(header, "attack_shape");
  REQUIRE(shape != header.end());
  REQUIRE(std::next(shape) != header.end());
  REQUIRE(*std::next(shape) == "attack_angle");
  const auto at = static_cast<std::size_t>(shape - header.begin());
  for (std::size_t row = 0; row < rows.size(); ++row) {
    auto& fields = rows.at(row);
    if (row == 0) {
      fields.at(at) = "shape_bits";
    } else {
      const auto& values = checked.at(row - 1).values;
      fields.at(at) = std::to_string(values.at(at) + (values.at(at + 1) * 8));
    }
    fields.erase(fields.begin() + static_cast<std::ptrdiff_t>(at + 1));
  }
}

// Packs the enemies table's AI settings columns, by their bit positions, back
// into the signed ai_settings word that version 9 tables had.
void PackAiSettings(std::vector<std::vector<std::string>>& rows,
                    const std::vector<content::TableRow>& checked) {
  constexpr std::array<unsigned, 19> kShifts{
      2, 4, 7, 8, 10, 12, 13, 14, 15, 16, 18, 21, 23, 24, 25, 26, 27, 28, 30};
  auto& header = rows.front();
  const auto first = std::ranges::find(header, "facing_delay_preset");
  REQUIRE(first != header.end());
  const auto at = static_cast<std::size_t>(first - header.begin());
  REQUIRE(at + kShifts.size() <= header.size());
  REQUIRE(header.at(at + kShifts.size() - 1) == "terrain_preset");
  for (std::size_t row = 0; row < rows.size(); ++row) {
    auto& fields = rows.at(row);
    if (row == 0) {
      fields.at(at) = "ai_settings";
    } else {
      const auto& values = checked.at(row - 1).values;
      std::uint32_t word = 0;
      for (std::size_t index = 0; index < kShifts.size(); ++index) {
        word |= static_cast<std::uint32_t>(values.at(at + index))
                << kShifts.at(index);
      }
      fields.at(at) = std::to_string(static_cast<std::int32_t>(word));
    }
    fields.erase(
        fields.begin() + static_cast<std::ptrdiff_t>(at + 1),
        fields.begin() + static_cast<std::ptrdiff_t>(at + kShifts.size()));
  }
}

// Adds back the raw_<offset> columns version 4 tables had for reserved bytes,
// the action_N_index columns version 5 had for each action slot's index, the
// equipment index columns version 6 had, version 8's zud_index, which held
// the template's slot, and the class and affinity tail columns under the
// numbered names version 1 gave them.
void AddReservedColumns(std::vector<std::vector<std::string>>& rows,
                        ZoneTable table,
                        const std::vector<content::TableRow>& checked) {
  if (table == ZoneTable::kEnemies) {
    const auto slot = std::ranges::find(rows.front(), "slot");
    REQUIRE(slot != rows.front().end());
    const auto slot_at = static_cast<std::size_t>(slot - rows.front().begin());
    rows.front().insert(rows.front().end(),
                        {"raw_33", "raw_236", "raw_237", "weapon_index",
                         "shield_index", "accessory_index", "zud_index"});
    for (std::size_t row = 1; row < rows.size(); ++row) {
      const std::string template_slot = rows.at(row).at(slot_at);
      rows.at(row).insert(rows.at(row).end(),
                          {"0", "0", "0", "0", "0", "0", template_slot});
    }
  }
  if (table == ZoneTable::kParts) {
    rows.front().insert(rows.front().end(),
                        {"index", "class_7", "class_8", "affinity_8"});
    for (std::size_t row = 1; row < rows.size(); ++row) {
      rows.at(row).insert(rows.at(row).end(), {"0", "0", "0", "0"});
    }
  }
  if (table != ZoneTable::kLimbs) return;
  const auto columns = content::ZoneTableSpec(table).columns;
  const auto material = std::ranges::find(
      columns, std::string_view("armor_material"), &content::ColumnSpec::name);
  REQUIRE(material != columns.end());
  const auto at = static_cast<std::size_t>(material - columns.begin());
  rows.front().insert(
      rows.front().end(),
      {"raw_52", "raw_5a", "raw_5b", "action_1_index", "action_2_index",
       "action_3_index", "action_4_index", "armor_index", "affinity_8"});
  for (std::size_t row = 1; row < rows.size(); ++row) {
    const bool armored = checked.at(row - 1).values.at(at) != 0;
    rows.at(row).insert(rows.at(row).end(), {armored ? "1" : "0", "0", "0", "0",
                                             "1", "2", "3", "0", "0"});
  }
}

// The number version 5 wrote for the synthetic action `id`; 0 for none.
std::string LegacyActionCell(const std::string& id) {
  if (id.empty()) return "0";
  static const auto kDefinitions =
      VS::test_support::SyntheticActions().definitions;
  const auto found =
      std::ranges::find(kDefinitions.actions, id, &content::ActionRule::id);
  REQUIRE(found != kDefinitions.actions.end());
  return std::to_string(found - kDefinitions.actions.begin());
}

void LegacyTableHeaders(const ModelRoot& root) {
  for (const auto table : content::kZoneTables) {
    const auto path = TableFile(root, table);
    const auto text = ReadText(path);
    const auto& spec = content::ZoneTableSpec(table);
    auto checked = content::ReadTable(spec, text, path.string());
    if (!checked) FAIL(checked.error());
    auto records = content::ParseCsv(text, path.string());
    if (!records) FAIL(records.error());
    for (std::size_t column = 0; column < spec.columns.size(); ++column) {
      if (!spec.columns[column].legacy_name.empty()) {
        records->front().fields[column] = spec.columns[column].legacy_name;
      }
      // Version 5 wrote action ids as numbers.
      const auto type = spec.columns[column].type;
      if (spec.columns[column].references == "actions") {
        for (std::size_t row = 1; row < records->size(); ++row) {
          auto& cell = records->at(row).fields[column];
          cell = LegacyActionCell(cell);
        }
        continue;
      }
      // Version 7 wrote the limbs' signed agility bonus unsigned.
      if (spec.columns[column].name == "agility_defense_bonus") {
        for (std::size_t row = 0; row < checked->size(); ++row) {
          const std::int64_t value = checked->at(row).values[column];
          records->at(row + 1).fields[column] =
              std::to_string(static_cast<std::uint8_t>(value));
        }
        continue;
      }
      // Enum codes, and flags as the signed number version 3 wrote.
      if (type != content::ColumnType::kEnum &&
          type != content::ColumnType::kFlags) {
        continue;
      }
      for (std::size_t row = 0; row < checked->size(); ++row) {
        const std::int64_t value = checked->at(row).values[column];
        records->at(row + 1).fields[column] = std::to_string(
            type == content::ColumnType::kEnum
                ? value
                : static_cast<std::int32_t>(static_cast<std::uint32_t>(value)));
      }
    }
    std::vector<std::vector<std::string>> rows;
    for (const auto& record : *records) rows.push_back(record.fields);
    if (table == ZoneTable::kParts) PackShapeBits(rows, *checked);
    if (table == ZoneTable::kEnemies) PackAiSettings(rows, *checked);
    AddReservedColumns(rows, table, *checked);
    WriteText(path, content::FormatCsv(rows));
  }
  auto manifest = content::LoadConversionManifest(root.Root());
  if (!manifest || !*manifest) FAIL("fixture conversion manifest missing");
  (**manifest).converters[std::string(content::kZndTableConverterName)] = 1;
  REQUIRE(
      extract::WriteConversionManifest(root.Root(), **manifest).has_value());
}

// Every converted file's bytes, by path under the root.
std::map<std::string, Bytes> OutputFiles(const ModelRoot& root) {
  std::map<std::string, Bytes> files;
  for (const auto& directory : {"catalogs", "textures"}) {
    for (const auto& entry :
         fs::recursive_directory_iterator(root.Root() / directory)) {
      if (!entry.is_regular_file()) continue;
      files[fs::relative(entry.path(), root.Root()).generic_string()] =
          ReadModel(entry.path());
    }
  }
  return files;
}

std::optional<fs::path> RepositoryPath(const fs::path& relative) {
  for (auto directory = fs::current_path();
       directory.has_parent_path() && directory != directory.parent_path();
       directory = directory.parent_path()) {
    if (fs::exists(directory / relative)) return directory / relative;
  }
  return std::nullopt;
}

void CheckEditedTemplate(const ModelRoot& root, const Bytes& source) {
  content::ZndFile expected = SampleZone();
  auto& data = expected.enemies.at(0).data;
  const auto hp = static_cast<std::int16_t>(
      std::to_integer<unsigned>(data.at(kHpAt)) |
      (std::to_integer<unsigned>(data.at(kHpAt + 1)) << 8U));
  EditTable(root, ZoneTable::kEnemies, std::format(",{},", hp), ",300,");
  data.at(kHpAt) = std::byte{0x2C};
  data.at(kHpAt + 1) = std::byte{0x01};
  CHECK(Loaded(root) == Encoded(expected));
  CHECK(Loaded(root) != source);
  // An unchanged rerun keeps the edit.
  CHECK(Convert(root).unchanged == 1);
  CHECK(Loaded(root) == Encoded(expected));
}

void CheckEditedRoom(const ModelRoot& root) {
  EditTable(root, ZoneTable::kRooms, "zone.map.zone001,2,room.map.map002",
            "zone.map.zone001,2,room.map.map001");
  const auto zone = content::DecodeZndFile(Loaded(root));
  REQUIRE(zone.has_value());
  CHECK(zone->rooms.at(2) ==
        content::ZndFileEntry{.sector = kRoomSector, .bytes = 2 * kSector});
}

void CheckEditedTexture(const ModelRoot& root) {
  const fs::path directory = root.Root() / "textures" / "zone" / "ZONE001";
  auto document = content::LoadImageFiles(directory, "01");
  INFO(ErrorOf(document));
  REQUIRE(document.has_value());
  document->image.texels.at(0) = 0x7FFF;
  auto files = extract::EncodeImageFiles(*document, "01");
  REQUIRE(files.has_value());
  REQUIRE(extract::WriteImageFiles(directory, *files).has_value());
  const auto zone = content::DecodeZndFile(Loaded(root));
  REQUIRE(zone.has_value());
  Bytes expected = Tim(320, 256, 4, 0x1234);
  // The first word follows the id, flags and block header (20 bytes).
  expected.at(20) = std::byte{0xFF};
  expected.at(21) = std::byte{0x7F};
  CHECK(zone->textures->at(1) == expected);
}

void CheckRefusedLoads(const ModelRoot& root) {
  EditTable(root, ZoneTable::kRooms, "room.map.map001", "room.map.map999");
  CHECK(ErrorOf(Load(root))
            .contains("MAP/MAP999.MPD is not in the extracted "
                      "data"));
  EditTable(root, ZoneTable::kRooms, "room.map.map999", "room.map.map001");
  EditTable(root, ZoneTable::kZones, "zone.map.zone001,7,",
            "zone.map.zone001,x,");
  CHECK(ErrorOf(Load(root)) ==
        "catalogs/world/zones.csv:2: column 'music_id': 'x' is not a decimal "
        "integer");
  EditTable(root, ZoneTable::kZones, "zone.map.zone001,x,",
            "zone.map.zone001,7,");
  // An image from another conversion of the zone.
  const fs::path sidecar =
      root.Root() / "textures" / "zone" / "ZONE001" / "00.image.json";
  const std::string text = ReadText(sidecar);
  const std::size_t key = text.find("\"source_sha256\"");
  REQUIRE(key != std::string::npos);
  const std::size_t at = text.find('"', text.find(':', key)) + 1;
  std::string stale = text;
  stale.at(at) = stale.at(at) == '0' ? '1' : '0';
  WriteText(sidecar, stale);
  CHECK(ErrorOf(Load(root)) ==
        "zone.map.zone001: image.map.zone001.00 does not match its "
        "conversion manifest");
  WriteText(sidecar, text);
}

// A live read fills the block when the rebuilt zone fits it and zeros it
// otherwise.
void CheckLiveReads(const ModelRoot& root, const Bytes& source) {
  content::DataStore::ScopedOverride restore;
  REQUIRE(content::DataStore::Open(root.Root()).has_value());
  const content::DiscFileExtent read{
      .sector = static_cast<std::int32_t>(kZoneSector),
      .bytes = static_cast<std::int32_t>(source.size()),
      .asset = {}};
  CHECK(content::IsLiveConvertedZone(read));
  Bytes block(source.size(), std::byte{0xCD});
  CHECK(content::ReadLiveConvertedZone(read, block) == true);
  CHECK(block == source);
  Bytes small(source.size() - 1, std::byte{0xCD});
  CHECK(content::ReadLiveConvertedZone(read, small) == false);
  CHECK(small == Bytes(source.size() - 1, std::byte{}));
  const content::DiscFileExtent room{
      .sector = kRoomSector, .bytes = 3000, .asset = {}};
  CHECK_FALSE(content::IsLiveConvertedZone(room));
  CHECK_FALSE(content::ReadLiveConvertedZone(room, block).has_value());
}

bool CheckNameSlot(std::span<const std::byte> source, std::string_view id) {
  // Empty fixed slots are import records, not unterminated strings to invent.
  if (std::ranges::all_of(
          source, [](std::byte value) { return value == std::byte{}; })) {
    return false;
  }
  const auto decoded = content::DecodeGameText(source, id);
  if (!decoded) FAIL(decoded.error());
  auto encoded = content::EncodeGameText(*decoded, id);
  if (!encoded) FAIL(encoded.error());
  CHECK(encoded->size() == decoded->consumed);
  const auto suffix = source.subspan(decoded->consumed);
  encoded->insert(encoded->end(), suffix.begin(), suffix.end());
  CHECK(*encoded == Bytes(source.begin(), source.end()));
  return true;
}

std::size_t CheckTextTable(const fs::path& path) {
  const auto source = ReadModel(path);
  REQUIRE(source.size() >= 2);
  const auto halfword = [&source](std::size_t index) {
    return std::to_integer<unsigned>(source.at(index * 2)) |
           (std::to_integer<unsigned>(source.at(index * 2 + 1)) << 8);
  };
  const std::size_t count = halfword(0);
  REQUIRE(count * 2 <= source.size());
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t begin = halfword(index) * 2;
    const std::size_t end =
        index + 1 < count ? halfword(index + 1) * 2 : source.size();
    REQUIRE(begin >= count * 2);
    REQUIRE(end > begin);
    REQUIRE(end <= source.size());
    const auto id = std::format("{} entry {}", path.filename().string(), index);
    CHECK(CheckNameSlot(std::span(source).subspan(begin, end - begin), id));
  }
  return count;
}

}  // namespace

TEST_CASE("Item help and workshop text preserve controls and import provenance",
          "[game-text-codec][extract]") {
  const auto data = RepositoryPath(fs::path("Native") / "data");
  if (!data) SKIP("Extracted Native/data is unavailable");
  std::size_t strings = 0;
  for (const auto* name : {"ITEMHELP.BIN", "MENU12.BIN"}) {
    strings += CheckTextTable(*data / "disc" / "MENU" / name);
  }
  CHECK(strings == 766);
}

TEST_CASE(
    "Item and enemy names round-trip through readable text with provenance",
    "[game-text-codec][extract]") {
  const auto data = RepositoryPath(fs::path("Native") / "data");
  if (!data) SKIP("Extracted Native/data is unavailable");
  const auto items = ReadModel(*data / "disc" / "MENU" / "ITEMNAME.BIN");
  constexpr std::size_t kNameBytes = 24;
  REQUIRE(items.size() % kNameBytes == 0);
  std::size_t names = 0;
  std::size_t empty = 0;
  for (std::size_t at = 0; at < items.size(); at += kNameBytes) {
    const bool present =
        CheckNameSlot(std::span(items).subspan(at, kNameBytes),
                      std::format("ITEMNAME.BIN slot {}", at / kNameBytes));
    ++(present ? names : empty);
  }
  const auto zones = content::ReadZoneTables(
      [&](std::string_view path) -> std::expected<std::string, std::string> {
        return ReadText(*data / fs::path(path));
      });
  if (!zones) FAIL(zones.error());
  for (const auto& zone : *zones) {
    for (const auto& enemy : zone.enemies) {
      for (const std::size_t at : {std::size_t{0x04}, std::size_t{0x128}}) {
        const bool present = CheckNameSlot(
            std::span(enemy.data).subspan(at, kNameBytes), enemy.id);
        ++(present ? names : empty);
      }
    }
  }
  CHECK(names == 1419);
  CHECK(empty == 3);
}

TEST_CASE("Zone texture ids name their image files", "[content-tables][znd]") {
  CHECK(content::ZoneTextureId("zone.map.zone009", 3) ==
        "image.map.zone009.03");
  const auto location = content::ZoneTextureLocationOf("image.map.zone009.03");
  REQUIRE(location.has_value());
  CHECK(*location ==
        content::ZoneTextureLocation{.directory = "textures/zone/ZONE009",
                                     .stem = "03",
                                     .source = "MAP/ZONE009.ZND"});
  CHECK(ErrorOf(content::ZoneTextureLocationOf("image.map.zone009.x")) ==
        "'image.map.zone009.x' is not a zone texture id");
}

TEST_CASE("ZND zones convert to tables that load as their bytes",
          "[content-tables][znd]") {
  const Bytes source = Encoded(SampleZone());
  const ModelRoot root(ZoneSources(source));
  REQUIRE(root.Read(3).lba == kZoneSector);
  RecordArchetype(root.Root());
  const auto first = Convert(root);
  CHECK(first.converted == 1);
  CHECK(first.unchanged == 0);
  CHECK(Loaded(root) == source);
  const std::string enemies = ReadText(TableFile(root, ZoneTable::kEnemies));
  CHECK(enemies.contains(
      std::format("{},zone.map.zone001,0,{},", kEnemyId, kArchetypeId)));
  CHECK(fs::exists(root.Root() / "textures/zone/ZONE001/00.png"));
  // Two forced conversions write the same files.
  const auto before = OutputFiles(root);
  CHECK(Convert(root, true).converted == 1);
  CHECK(OutputFiles(root) == before);
  CheckEditedTemplate(root, source);
  CheckEditedRoom(root);
  CheckEditedTexture(root);
  // --force replaces the edited tables and images.
  CHECK(Convert(root, true).converted == 1);
  CHECK(Loaded(root) == source);
  CheckRefusedLoads(root);
  CheckLiveReads(root, source);
}

TEST_CASE("The zone converter names the read it cannot resolve",
          "[content-tables][znd]") {
  struct Refused {
    content::ZndFile zone;
    bool archetype = true;
    std::string_view error;
  };
  content::ZndFile mid_room = SampleZone();
  mid_room.rooms.at(0).sector = kRoomSector + 1;
  content::ZndFile short_room = SampleZone();
  short_room.rooms.at(0).bytes = kSector;
  content::ZndFile no_tims = SampleZone();
  no_tims.textures.emplace();
  // A disc template whose reserved bytes the tables would not rebuild.
  content::ZndFile padded = SampleZone();
  padded.enemies.at(0).data.at(0x33) = std::byte{5};
  content::ZndFile unflagged = SampleZone();
  unflagged.enemies.at(0).data.at(0x238 + 0x52) = std::byte{};
  // Limb 0's second action slot (+0x14) holding the first slot's index.
  content::ZndFile misindexed = SampleZone();
  misindexed.enemies.at(0).data.at(0x238 + 0x17) = std::byte{};
  // A blade (at 0x34) naming the inventory slot it came from (+0x17).
  content::ZndFile slotted = SampleZone();
  slotted.enemies.at(0).data.at(0x34 + 0x17) = std::byte{2};
  // Template 0 loading ZUD entry 1 (0x460), not its own.
  content::ZndFile borrowed = SampleZone();
  borrowed.enemies.at(0).data.at(0x460) = std::byte{1};
  // The AI settings' unread low bits (0x2C bits 0-1) set.
  content::ZndFile low_bits = SampleZone();
  low_bits.enemies.at(0).data.at(0x2C) |= std::byte{2};
  // The blade's (0x34) class array tail (+0x26) set.
  content::ZndFile class_tail = SampleZone();
  class_tail.enemies.at(0).data.at(0x34 + 0x26) = std::byte{4};
  const std::vector<Refused> refusals{
      {mid_room, true,
       "MAP/ZONE001.ZND: room slot 0: sector 101 (4096 bytes) is no "
       "extracted .MPD file's read"},
      {short_room, true,
       "MAP/ZONE001.ZND: room slot 0: sector 100 (2048 bytes) is no "
       "extracted .MPD file's read"},
      {no_tims, true,
       "MAP/ZONE001.ZND: an empty texture table has no native form"},
      {padded, true,
       "MAP/ZONE001.ZND: enemy.map.zone001.00: reserved byte 0x033 is 5, not "
       "padding 0"},
      {unflagged, true,
       "MAP/ZONE001.ZND: enemy.map.zone001.00: reserved byte 0x28A is 0, not "
       "the 1 its armor_material gives"},
      {misindexed, true,
       "MAP/ZONE001.ZND: enemy.map.zone001.00: reserved byte 0x24F is 0, not "
       "the 1 it always holds"},
      {slotted, true,
       "MAP/ZONE001.ZND: enemy.map.zone001.00: reserved byte 0x04B is 2, not "
       "the 0 it always holds"},
      {borrowed, true,
       "MAP/ZONE001.ZND: enemy.map.zone001.00: reserved byte 0x460 is 1, not "
       "the 0 its slot 0 gives"},
      {low_bits, true,
       "MAP/ZONE001.ZND: enemy.map.zone001.00: bits 0x03 of reserved byte "
       "0x02C are 2, not padding 0"},
      {class_tail, true,
       "MAP/ZONE001.ZND: enemy.map.zone001.00: reserved byte 0x05A is 4, not "
       "the 0 it always holds"},
      {SampleZone(), false,
       "MAP/ZONE001.ZND: enemy.map.zone001.00 loads archetype.map.z001u00, "
       "which the zud-json converter has not recorded (convert the models "
       "first)"},
  };
  for (const Refused& refused : refusals) {
    CAPTURE(refused.error);
    const ModelRoot root(ZoneSources(Encoded(refused.zone)));
    if (refused.archetype) RecordArchetype(root.Root());
    CHECK(ErrorOf(extract::ConvertDiscZones(root.Root(), false)) ==
          refused.error);
  }
}

TEST_CASE("Old zone authoring upgrades without overwriting native edits",
          "[zone-authoring][content-tables][znd]") {
  const auto source = Encoded(SampleZone());
  const ModelRoot root(ZoneSources(source));
  RecordArchetype(root.Root());
  REQUIRE(Convert(root).converted == 1);
  CheckEditedTemplate(root, source);
  const auto edited = Loaded(root);
  LegacyTableHeaders(root);
  CHECK(Convert(root).unchanged == 1);
  CHECK(Loaded(root) == edited);
  // The negative immunity number became its status names.
  const auto enemies = ReadText(TableFile(root, ZoneTable::kEnemies));
  CHECK(enemies.contains("movement_class,"));
  CHECK(enemies.contains(",head-impaired|left-arm-impaired|body-impaired|"));
  CHECK(enemies.contains("|analyze|spell-immunity,"));
  // The blade's packed shape byte split, and its codes became names.
  const auto parts = ReadText(TableFile(root, ZoneTable::kParts));
  CHECK(parts.contains(",attack_shape,attack_angle,"));
  CHECK_FALSE(parts.contains("shape_bits"));
  CHECK(parts.contains(",blunt,hp,"));
  // Version 4's reserved byte columns are gone.
  CHECK_FALSE(enemies.contains("raw_"));
  const auto limbs = ReadText(TableFile(root, ZoneTable::kLimbs));
  CHECK_FALSE(limbs.contains("raw_"));
  // Version 5's action numbers became action ids, and its index columns went.
  CHECK(limbs.contains(",action_1,action_1_special_sequence,"));
  CHECK(limbs.contains(",action.rend,"));
  CHECK_FALSE(limbs.contains("action_1_index"));
  // Version 6's equipment index columns went.
  CHECK_FALSE(enemies.contains("weapon_index"));
  CHECK_FALSE(limbs.contains("armor_index"));
  CHECK_FALSE(parts.contains(",index"));
  // Version 8's zud_index, which held each template's slot, went.
  CHECK_FALSE(enemies.contains("zud_index"));
  // Version 9's signed ai_settings word split into its named columns.
  CHECK_FALSE(enemies.contains("ai_settings"));
  CHECK(enemies.contains(",facing_delay_preset,wander_preset,"));
  // The class and affinity tail columns went.
  CHECK_FALSE(parts.contains("class_8"));
  CHECK_FALSE(parts.contains("affinity_8"));
  CHECK_FALSE(limbs.contains("affinity_8"));
  CHECK(parts.contains(",class_evil,affinity_physical,"));
  const auto repeat = extract::UpgradeZoneAuthoring(root.Root());
  if (!repeat) FAIL(repeat.error());
  CHECK(repeat->written == 0);
}

TEST_CASE("Failed zone authoring upgrades preserve files and converter version",
          "[zone-authoring][content-tables][znd]") {
  const ModelRoot root(ZoneSources(Encoded(SampleZone())));
  RecordArchetype(root.Root());
  REQUIRE(Convert(root).converted == 1);
  LegacyTableHeaders(root);
  // affinity_3 is affinity_fire's legacy name, so the column appears twice.
  EditTable(root, ZoneTable::kLimbs, "affinity_2,", "affinity_fire,");
  const auto before = OutputFiles(root);
  const auto manifest = ReadText(root.Root() / "manifest.json");
  const auto upgraded = extract::UpgradeZoneAuthoring(root.Root());
  REQUIRE_FALSE(upgraded.has_value());
  CHECK(upgraded.error().contains("appears twice"));
  CHECK(OutputFiles(root) == before);
  CHECK(ReadText(root.Root() / "manifest.json") == manifest);
}

TEST_CASE("Committed zone table schemas match the converter's",
          "[content-tables][znd]") {
  for (const ZoneTable table : content::kZoneTables) {
    const std::string path = content::ZoneTableSchemaPath(table);
    CAPTURE(path);
    const auto committed = RepositoryPath(fs::path("Native") / "Content" /
                                          "schemas" / fs::path(path));
    REQUIRE(committed.has_value());
    std::string text = ReadText(*committed);
    std::erase(text, '\r');
    CHECK(text == content::FormatTableSchema(content::ZoneTableSpec(table)));
  }
}

TEST_CASE("Extracted ZND zones load from their tables as exact bytes",
          "[content-tables][znd][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  content::DataStore::ScopedOverride restore;
  const auto opened =
      content::DataStore::OpenShared(models->parent_path().parent_path());
  INFO(ErrorOf(opened));
  REQUIRE(opened.has_value());
  const content::DataStore* store = content::DataStore::Get();
  REQUIRE(store != nullptr);
  const auto required = store->RequireConversions();
  INFO(ErrorOf(required));
  REQUIRE(required.has_value());
  std::size_t zones = 0;
  for (const content::DataStore::Entry& entry : store->Entries()) {
    if (entry.form != "raw" || !entry.path.ends_with(content::kZndExtension)) {
      continue;
    }
    CAPTURE(entry.path);
    const auto bytes = content::LoadConvertedZone(*store, entry.lba);
    INFO(ErrorOf(bytes));
    REQUIRE(bytes.has_value());
    CHECK(*bytes == ReadModel(store->FileOf(entry)));
    ++zones;
  }
  CHECK(zones == kExtractedZoneCount);
}
