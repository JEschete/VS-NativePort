#include <algorithm>
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
#include "TestSupport/ModelFixtures.hpp"

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

// Records the archetype the zone names, as the zud-json converter would.
void RecordArchetype(const fs::path& root) {
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
      if (spec.columns[column].type == content::ColumnType::kEnum) {
        for (std::size_t row = 0; row < checked->size(); ++row) {
          records->at(row + 1).fields[column] =
              std::to_string(checked->at(row).values[column]);
        }
      }
    }
    std::vector<std::vector<std::string>> rows;
    for (const auto& record : *records) rows.push_back(record.fields);
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
  auto encoded = content::EncodeGameText(decoded->markup, id);
  if (!encoded) FAIL(encoded.error());
  CHECK(encoded->size() == decoded->consumed);
  const auto suffix = source.subspan(decoded->consumed);
  encoded->insert(encoded->end(), suffix.begin(), suffix.end());
  CHECK(*encoded == Bytes(source.begin(), source.end()));
  return true;
}

}  // namespace

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
  const std::vector<Refused> refusals{
      {mid_room, true,
       "MAP/ZONE001.ZND: room slot 0: sector 101 (4096 bytes) is no "
       "extracted .MPD file's read"},
      {short_room, true,
       "MAP/ZONE001.ZND: room slot 0: sector 100 (2048 bytes) is no "
       "extracted .MPD file's read"},
      {no_tims, true,
       "MAP/ZONE001.ZND: an empty texture table has no native form"},
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
  CHECK(ReadText(TableFile(root, ZoneTable::kEnemies))
            .contains("movement_class,"));
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
  EditTable(root, ZoneTable::kParts, "affinity_3,",
            "affinity_3,affinity_fire,");
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
      content::DataStore::Open(models->parent_path().parent_path());
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
