#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ActorArchetype.hpp"
#include "Content/CatalogTable.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Content/ZoneTables.hpp"
#include "Extract/AssetLabels.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/ConverterBatch.hpp"
#include "TestSupport/ModelFixtures.hpp"
#include "TestSupport/WorkshopRecipeRoot.hpp"

namespace {
namespace content = VS::Content;
namespace extract = VS::Extract;
namespace fs = std::filesystem;
using Json = content::JsonDocument;
using VS::test_support::ErrorOf;
using VS::test_support::ModelRoot;
using VS::test_support::Required;

constexpr std::array<std::uint8_t, 15> kGuildenstern{
    0x10, 0x38, 0x2C, 0x2F, 0x27, 0x28, 0x31, 0x36,
    // NOLINTNEXTLINE(whitespace/indent_namespace): array initializer.
    0x37, 0x28, 0x35, 0x31, 0xE7, 0xFF, 0xFF};
constexpr std::string_view kEnemyPath = "catalogs/enemies/zone-enemies.csv";
constexpr std::string_view kArchetypePath = "models/zud/Z009U00.json";
constexpr std::string_view kModelPath = "models/characters/4B.gltf";

void Write(const fs::path& root, std::string_view path, std::string_view text) {
  const extract::OutputBytes output{
      .name = path,
      .bytes = std::as_bytes(std::span(text.data(), text.size()))};
  const auto written = extract::WriteOutputFiles(root, std::span(&output, 1));
  INFO(ErrorOf(written));
  REQUIRE(written.has_value());
}

std::string Read(const fs::path& root, std::string_view path) {
  const auto bytes = Required(extract::ReadFileBytes(root / fs::path(path)));
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

Json ReadJson(const fs::path& root, std::string_view path) {
  const auto bytes = Required(extract::ReadFileBytes(root / fs::path(path)));
  return Required(content::ParseJsonText(bytes, path));
}

content::ZoneRecord Zone() {
  content::ZoneRecord zone{.id = "zone.map.zone009",
                           .music_id = 7,
                           .rooms = {"room.map.map009"},
                           .textures = {"image.map.zone009.00"},
                           .enemies = {}};
  content::ZoneEnemyRecord enemy{.id = "enemy.map.zone009.00",
                                 .archetype = "archetype.map.z009u00",
                                 .data = {}};
  std::ranges::copy(std::as_bytes(std::span(kGuildenstern)),
                    enemy.data.begin() + 4);
  enemy.data.at(0x34) = std::byte{1};   // blade item id
  enemy.data.at(0x238) = std::byte{9};  // first limb HP
  zone.enemies.push_back(enemy);
  return zone;
}

void WriteZone(const fs::path& root, const content::ZoneRecord& zone) {
  const auto tables = content::FormatZoneTables(std::span(&zone, 1));
  for (const auto table : content::kZoneTables) {
    Write(root, content::ZoneTablePath(table),
          tables.at(static_cast<std::size_t>(table)));
    Write(root, content::ZoneTableSchemaPath(table),
          content::FormatTableSchema(content::ZoneTableSpec(table)));
  }
}

void WriteWorkshop(const fs::path& root,
                   const content::WorkshopRecipeTable& table) {
  const auto texts = content::FormatWorkshopTables(table);
  for (const auto part : content::kWorkshopTableParts) {
    Write(root, content::WorkshopTablePath(table.kind, part),
          texts.at(static_cast<std::size_t>(part)));
    Write(root, content::WorkshopSchemaPath(table.kind, part),
          content::FormatTableSchema(
              content::WorkshopTableSpec(table.kind, part)));
  }
}

std::vector<content::ZoneRecord> ReadZones(const fs::path& root) {
  return Required(content::ReadZoneTables(
      [&](std::string_view path) -> std::expected<std::string, std::string> {
        auto bytes = extract::ReadFileBytes(root / fs::path(path));
        if (!bytes) return std::unexpected(bytes.error());
        return std::string(reinterpret_cast<const char*>(bytes->data()),
                           bytes->size());
      }));
}

void WriteModels(const fs::path& root) {
  const std::string hash(64, 'a');
  const Json archetype{{"schema", content::kActorArchetypeSchema},
                       {"schema_version", 1},
                       {"source", "MAP/Z009U00.ZUD"},
                       {"source_sha256", hash},
                       {"body", {{"model", "model.obj.4b"}}}};
  const Json model{
      {"asset",
       {{"version", "2.0"},
        {"extras",
         {{"vs", {{"id", "model.obj.4b"}, {"source", "OBJ/4B.SHP"}}}}}}}};
  Write(root, kArchetypePath, content::FormatJsonText(archetype) + "\n");
  Write(root, kModelPath, content::FormatJsonText(model) + "\n");
  content::ConversionManifest manifest;
  manifest.converters = {{std::string(content::kZudArchetypeConverterName), 1},
                         {"shp-gltf", 1}};
  manifest.outputs = {
      {.id = "archetype.map.z009u00",
       .source = "MAP/Z009U00.ZUD",
       .source_sha256 = hash,
       .converter = std::string(content::kZudArchetypeConverterName),
       .files = {std::string(kArchetypePath)},
       .dependencies = {},
       .unreferenced = false,
       .note = {},
       .alias_of = {}},
      {.id = "model.obj.4b",
       .source = "OBJ/4B.SHP",
       .source_sha256 = hash,
       .converter = "shp-gltf",
       .files = {std::string(kModelPath)},
       .dependencies = {},
       .unreferenced = false,
       .note = {},
       .alias_of = {}}};
  const auto written = extract::WriteConversionManifest(root, manifest);
  INFO(ErrorOf(written));
  REQUIRE(written.has_value());
}

void EditCell(const fs::path& root, std::string_view column,
              std::string value) {
  auto records =
      Required(content::ParseCsv(Read(root, kEnemyPath), kEnemyPath));
  auto& header = records.front().fields;
  const auto found = std::ranges::find(header, column);
  REQUIRE(found != header.end());
  const auto index = static_cast<std::size_t>(found - header.begin());
  records.at(1).fields.at(index) = std::move(value);
  std::vector<std::vector<std::string>> rows;
  for (auto& record : records) rows.push_back(std::move(record.fields));
  Write(root, kEnemyPath, content::FormatCsv(rows));
}

}  // namespace

TEST_CASE("Readable names stop before terminator padding", "[asset-labels]") {
  CHECK(Required(extract::DecodeAssetName(
            std::as_bytes(std::span(kGuildenstern)))) == "Guildenstern");
  const std::array empty{std::byte{}, std::byte{}};
  CHECK(Required(extract::DecodeAssetName(empty)).empty());
}

TEST_CASE("Readable names retain punctuation accents and visible unknowns",
          "[asset-labels]") {
  const std::array text{std::byte{0x0A}, std::byte{0x96}, std::byte{0x9F},
                        std::byte{0x91}, std::byte{0x3E}, std::byte{0x41},
                        std::byte{0x91}, std::byte{0x6B}, std::byte{0xE7}};
  CHECK(Required(extract::DecodeAssetName(text)) ==
        "A',\"\xC5\x92\xC3\x82\"[glyph 6B]");
}

TEST_CASE("Readable names consume layout parameters without displaying them",
          "[asset-labels]") {
  const std::array text{std::byte{0xFB}, std::byte{2},   std::byte{0x0A},
                        std::byte{0xFA}, std::byte{4},   std::byte{0x0B},
                        std::byte{0xE5}, std::byte{0xE7}};
  CHECK(Required(extract::DecodeAssetName(text)) == "A B");
  const std::array truncated{std::byte{0xFB}};
  CHECK_FALSE(extract::DecodeAssetName(truncated).has_value());
}

TEST_CASE("CSV labels do not shift runtime columns or weaken validation",
          "[asset-labels]") {
  const std::array<content::ColumnSpec, 2> columns{
      {{.name = "id",
        .type = content::ColumnType::kId,
        .references = {},
        .description = {}},
       {.name = "hp",
        .minimum = 0,
        .maximum = 99,
        .references = {},
        .description = {}}}};
  const std::array<std::string_view, 1> key{"id"};
  const content::TableSpec spec{
      .name = "enemies", .description = {}, .columns = columns, .key = key};
  const auto rows = Required(content::ReadTable(
      spec, "id,display_name,hp\nwolf,\"Wolf, \"\"A\"\"\",12\n", "test.csv"));
  CHECK(rows.front().cells == std::vector<std::string>{"wolf", "12"});
  CHECK(rows.front().values.at(1) == 12);
  CHECK_FALSE(content::ReadTable(
      spec, "id,display_name,display_name,hp\nwolf,A,B,12\n", "test.csv"));
  CHECK_FALSE(
      content::ReadTable(spec, "id,display_name,hp\nwolf,A,100\n", "test.csv"));
  CHECK_FALSE(
      content::ReadTable(spec, "id,display_name,hp\nwolf,12\n", "test.csv"));
  CHECK_FALSE(
      content::ReadTable(spec, "id,display_nam,hp\nwolf,A,12\n", "test.csv"));
}

TEST_CASE(
    "JSON annotations allow only text and leave other member checks strict",
    "[asset-labels]") {
  Json value{{"display_name", "Wolf"}, {"hp", 12}};
  CHECK(content::JsonMembers(value, "enemy").Only({"hp"}));
  value["display_name"] = 7;
  CHECK_FALSE(content::JsonMembers(value, "enemy").Only({"hp"}));
  value["display_name"] = "Wolf";
  value["hpp"] = 12;
  CHECK_FALSE(content::JsonMembers(value, "enemy").Only({"hp"}));
}

TEST_CASE("Zone labels preserve every source field and are stable on repeat",
          "[asset-labels]") {
  ModelRoot root({});
  const auto zone = Zone();
  WriteZone(root.Root(), zone);
  const auto first = Required(extract::LabelNativeAssets(root.Root()));
  CHECK(first.written == content::kZoneTableCount);
  CHECK(first.rows == 6);
  const auto records =
      Required(content::ParseCsv(Read(root.Root(), kEnemyPath), kEnemyPath));
  CHECK(records.front().fields.at(1) == "display_name");
  CHECK(records.at(1).fields.at(1) == "Guildenstern");
  CHECK(ReadZones(root.Root()) == std::vector{zone});
  CHECK(Required(extract::LabelNativeAssets(root.Root())).written == 0);
}

TEST_CASE("Refreshing labels never replaces native gameplay or custom edits",
          "[asset-labels]") {
  ModelRoot root({});
  auto zone = Zone();
  WriteZone(root.Root(), zone);
  REQUIRE(extract::LabelNativeAssets(root.Root()));
  EditCell(root.Root(), "display_name", "Owner's name, \"quoted\"");
  EditCell(root.Root(), "hp", "1234");
  CHECK(Required(extract::LabelNativeAssets(root.Root())).written == 0);
  CHECK(Read(root.Root(), kEnemyPath).find("Owner's name") !=
        std::string::npos);
  REQUIRE(extract::LabelNativeAssets(root.Root(), true));
  const auto records =
      Required(content::ParseCsv(Read(root.Root(), kEnemyPath), kEnemyPath));
  CHECK(records.at(1).fields.at(1) == "Guildenstern");
  zone.enemies.front().data.at(0x1C) = std::byte{0xD2};
  zone.enemies.front().data.at(0x1D) = std::byte{0x04};
  CHECK(ReadZones(root.Root()) == std::vector{zone});
}

TEST_CASE("Archetype names reach model extras without changing glTF structure",
          "[asset-labels]") {
  ModelRoot root({});
  WriteZone(root.Root(), Zone());
  WriteModels(root.Root());
  const Json original = ReadJson(root.Root(), kModelPath);
  REQUIRE(extract::LabelNativeAssets(root.Root()));
  auto model = ReadJson(root.Root(), kModelPath);
  auto& payload = model["asset"]["extras"]["vs"];
  CHECK(payload.at("display_name") == "Guildenstern");
  CHECK_FALSE(model.contains("display_name"));
  payload.erase("display_name");
  CHECK(model == original);
  const auto archetype = ReadJson(root.Root(), kArchetypePath);
  CHECK(archetype.begin().key() == "display_name");
  CHECK(Required(extract::LabelNativeAssets(root.Root())).written == 0);
}

TEST_CASE("Recipe labels use item names with the original equipment offset",
          "[asset-labels]") {
  const auto kind =
      GENERATE(content::WorkshopKind::kBlade, content::WorkshopKind::kShield);
  constexpr std::size_t kNameWidth = 24;
  constexpr std::size_t kItemCount = 512;
  constexpr std::size_t kFirstShield = 127;
  std::vector<std::byte> names(kItemCount * kNameWidth);
  names.at(kNameWidth) = std::byte{0x0A};
  names.at(kNameWidth + 1) = std::byte{0xE7};
  names.at(kFirstShield * kNameWidth) = std::byte{0x0B};
  names.at(kFirstShield * kNameWidth + 1) = std::byte{0xE7};
  ModelRoot root({{.path = "MENU/ITEMNAME.BIN", .bytes = names}});
  auto table = VS::test_support::FullWorkshopTable(kind);
  const auto id = VS::test_support::WorkshopResultId(kind, 1);
  table.pairs = {{.first = id, .second = id, .result = id}};
  WriteWorkshop(root.Root(), table);
  REQUIRE(extract::LabelNativeAssets(root.Root()));
  content::WorkshopTexts texts;
  for (const auto part : content::kWorkshopTableParts) {
    texts.at(static_cast<std::size_t>(part)) =
        Read(root.Root(), content::WorkshopTablePath(kind, part));
  }
  CHECK(Required(content::ParseWorkshopTables(kind, texts)) == table);
  const auto results = Required(content::ParseCsv(texts.at(0), "results"));
  CHECK(results.at(2).fields.at(1) ==
        (kind == content::WorkshopKind::kBlade ? "A" : "B"));
  const auto pairs = Required(content::ParseCsv(texts.at(1), "pairs"));
  CHECK(pairs.at(1).fields.at(1) ==
        (kind == content::WorkshopKind::kBlade ? "A + A -> A" : "B + B -> B"));
}

TEST_CASE("Shared model labels retain distinct enemy aliases",
          "[asset-labels]") {
  ModelRoot root({});
  auto zone = Zone();
  auto second = zone.enemies.front();
  second.id = "enemy.map.zone009.01";
  second.data.at(4) = std::byte{0x0A};
  second.data.at(5) = std::byte{0xE7};
  zone.enemies.push_back(second);
  WriteZone(root.Root(), zone);
  WriteModels(root.Root());
  REQUIRE(extract::LabelNativeAssets(root.Root()));
  const auto model = ReadJson(root.Root(), kModelPath);
  CHECK(model.at("asset").at("extras").at("vs").at("display_name") ==
        "A / Guildenstern");
  CHECK(ReadZones(root.Root()) == std::vector{zone});
}

TEST_CASE("Standalone audio metadata can be labeled without a disc inventory",
          "[asset-labels]") {
  ModelRoot root({});
  REQUIRE(fs::remove(root.Root() / content::DataStore::ManifestName));
  constexpr std::string_view kIndex = "audio/streams/index.json";
  const Json index{
      {"version", 1},
      {"streams",
       Json::array({{{"id", "stream-id"}, {"source", "EFFECT/E001.P"}}})},
      {"xa", Json::array()}};
  Write(root.Root(), kIndex, content::FormatJsonText(index) + "\n");
  REQUIRE(extract::LabelNativeAssets(root.Root()));
  const auto labeled = ReadJson(root.Root(), kIndex);
  CHECK(labeled.at("display_name") == "Audio stream index");
  CHECK(labeled.at("streams").at(0).at("display_name") ==
        "Audio stream from Effect package EFFECT/E001.P");
  CHECK(Required(extract::LabelNativeAssets(root.Root())).written == 0);
}

TEST_CASE("A malformed existing annotation is refused without replacing it",
          "[asset-labels]") {
  ModelRoot root({});
  WriteModels(root.Root());
  auto model = ReadJson(root.Root(), kModelPath);
  model["asset"]["extras"]["vs"]["display_name"] = Json::array({1, 2});
  const std::string malformed = content::FormatJsonText(model) + "\n";
  Write(root.Root(), kModelPath, malformed);
  const auto labeled = extract::LabelNativeAssets(root.Root());
  REQUIRE_FALSE(labeled.has_value());
  CHECK(labeled.error().find("display_name must be a string") !=
        std::string::npos);
  CHECK(Read(root.Root(), kModelPath) == malformed);
}
