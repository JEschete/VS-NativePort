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
#include "Extract/WorkshopAuthoring.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"
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

void EditCell(const fs::path& root, std::string_view column, std::string value,
              std::string_view path = kEnemyPath) {
  auto records = Required(content::ParseCsv(Read(root, path), path));
  auto& header = records.front().fields;
  const auto found = std::ranges::find(header, column);
  REQUIRE(found != header.end());
  const auto index = static_cast<std::size_t>(found - header.begin());
  records.at(1).fields.at(index) = std::move(value);
  std::vector<std::vector<std::string>> rows;
  for (auto& record : records) rows.push_back(std::move(record.fields));
  Write(root, path, content::FormatCsv(rows));
}

extract::SourceFile RecipeNames(bool collision = false) {
  constexpr std::size_t kWidth = 24;
  constexpr std::size_t kCount = 512;
  constexpr std::size_t kShield = 127;
  constexpr std::array<std::uint8_t, 6> kKnife{0x14, 0x31, 0x2C,
                                               0x29, 0x28, 0xE7};
  constexpr std::array<std::uint8_t, 5> kDirk{0x0D, 0x2C, 0x35, 0x2E, 0xE7};
  constexpr std::array<std::uint8_t, 8> kBuckler{0x0B, 0x38, 0x26, 0x2E,
                                                 0x2F, 0x28, 0x35, 0xE7};
  constexpr std::array<std::uint8_t, 9> kUntitled{0x38, 0x31, 0x37, 0x2C, 0x37,
                                                  0x2F, 0x28, 0x27, 0xE7};
  std::vector<std::byte> bytes(kCount * kWidth);
  auto out = std::span(bytes);
  std::ranges::copy(std::as_bytes(std::span(kKnife)),
                    out.subspan(kWidth).begin());
  if (collision) {
    std::ranges::copy(std::as_bytes(std::span(kKnife)),
                      out.subspan(2 * kWidth).begin());
  } else {
    std::ranges::copy(std::as_bytes(std::span(kDirk)),
                      out.subspan(2 * kWidth).begin());
  }
  std::ranges::copy(std::as_bytes(std::span(kBuckler)),
                    out.subspan(kShield * kWidth).begin());
  std::ranges::copy(std::as_bytes(std::span(kUntitled)),
                    out.subspan(3 * kWidth).begin());
  return {.path = "MENU/ITEMNAME.BIN", .bytes = std::move(bytes)};
}

content::WorkshopRecipeTable SmallRecipe(content::WorkshopKind kind) {
  auto table = VS::test_support::FullWorkshopTable(kind);
  if (kind == content::WorkshopKind::kBlade) {
    table.blades.resize(3);
    table.blades.at(1).stats.strength = 17;
    table.pairs = {
        {.first = "blade-001", .second = "blade-002", .result = "blade-001"},
        {.first = "blade-002", .second = "blade-001", .result = "blade-002"}};
  } else {
    table.pieces.resize(2);
    table.pieces.at(1).category = 1;
  }
  return table;
}

content::WorkshopRecipeTable ReadRecipe(const fs::path& root,
                                        content::WorkshopKind kind) {
  content::WorkshopTexts texts;
  for (const auto part : content::kWorkshopTableParts) {
    texts.at(static_cast<std::size_t>(part)) =
        Read(root, content::WorkshopTablePath(kind, part));
  }
  return Required(content::ParseWorkshopTables(kind, texts));
}

content::WorkshopRecipeTable WithUntitledTail() {
  auto table = SmallRecipe(content::WorkshopKind::kBlade);
  content::BladeResult unused{};
  unused.id = "blade-003";
  unused.legacy_id = 3;
  unused.damage_type = 1;
  unused.cost_type = 2;
  unused.attack_shape = 1;
  table.blades.push_back(unused);
  return table;
}

// Appends a former raw.byte<N> `column` to the table at `path`, holding
// `value` in record `row` (1 is the first data row) and 0 elsewhere.
void AddRawColumn(const fs::path& root, std::string_view path,
                  std::string_view column, std::size_t row,
                  std::string_view value) {
  auto records = Required(content::ParseCsv(Read(root, path), path));
  std::vector<std::vector<std::string>> rows;
  for (std::size_t index = 0; index < records.size(); ++index) {
    auto fields = std::move(records.at(index).fields);
    fields.emplace_back(index == 0     ? column
                        : index == row ? value
                                       : std::string_view("0"));
    rows.push_back(std::move(fields));
  }
  Write(root, path, content::FormatCsv(rows));
}

// Writes a blade recipe whose results table still has the former raw.byte7
// (171 in legacy record 1) and raw.byte11 (all 0) columns and a stale schema.
content::WorkshopRecipeTable WriteRawColumns(const fs::path& root) {
  const auto table = SmallRecipe(content::WorkshopKind::kBlade);
  WriteWorkshop(root, table);
  const std::string path = content::WorkshopTablePath(
      table.kind, content::WorkshopTablePart::kResults);
  AddRawColumn(root, path, "raw.byte7", 2, "171");
  AddRawColumn(root, path, "raw.byte11", 2, "0");
  Write(root,
        content::WorkshopSchemaPath(table.kind,
                                    content::WorkshopTablePart::kResults),
        "{}\n");
  return table;
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

TEST_CASE("Material rule captions name categories and replace retired ones",
          "[asset-labels]") {
  ModelRoot root({RecipeNames()});
  auto table =
      VS::test_support::FullWorkshopTable(content::WorkshopKind::kArmor);
  table.materials = {{.first_material = 3,
                      .first_category = 2,
                      .second_material = 4,
                      .second_category = 5,
                      .material = 5}};
  WriteWorkshop(root.Root(), table);
  const std::string path = content::WorkshopTablePath(
      table.kind, content::WorkshopTablePart::kMaterials);
  const auto caption = [&root, &path] {
    return Required(content::ParseCsv(Read(root.Root(), path), path))
        .at(1)
        .fields.at(1);
  };
  constexpr std::string_view kCaption =
      "Bronze + Iron -> Hagane (helm + glove)";
  REQUIRE(extract::LabelNativeAssets(root.Root()));
  CHECK(caption() == kCaption);
  EditCell(root.Root(), "display_name",
           "Bronze + Iron -> Hagane (categories 2 + 5)", path);
  CHECK(Required(extract::LabelNativeAssets(root.Root())).written == 1);
  CHECK(caption() == kCaption);
  EditCell(root.Root(), "display_name", "Owner's rule", path);
  CHECK(Required(extract::LabelNativeAssets(root.Root())).written == 0);
  CHECK(caption() == "Owner's rule");
  const std::string results = content::WorkshopTablePath(
      table.kind, content::WorkshopTablePart::kResults);
  EditCell(root.Root(), "display_name", "", results);
  CHECK(Required(extract::LabelNativeAssets(root.Root())).written == 0);
  CHECK(Required(content::ParseCsv(Read(root.Root(), results), results))
            .at(1)
            .fields.at(1)
            .empty());
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

TEST_CASE(
    "Native-owned text font and import records bypass generic asset labels",
    "[asset-labels][learning-catalog]") {
  const auto schema = GENERATE("vs.strings", "vs.font", "vs.text-contributions",
                               "vs.save-text-import", "vs.bestiary-import",
                               "vs.material-import", "vs.learning-import",
                               "vs.status-import", "vs.action-category-import");
  const bool refresh = GENERATE(false, true);
  const ModelRoot root({});
  const std::string text = std::string("{\"schema\":\"") + schema +
                           "\", \"native_owner\":\"preserve my formatting\"}\n";
  Write(root.Root(), "native-owned.json", text);
  content::ConversionManifest manifest;
  manifest.converters = {{"owner-json", 1}};
  manifest.outputs = {{.id = "owner.test",
                       .source = "SLUS_010.40",
                       .source_sha256 = std::string(64, 'a'),
                       .converter = "owner-json",
                       .files = {"native-owned.json"},
                       .dependencies = {},
                       .note = {},
                       .alias_of = {}}};
  REQUIRE(extract::WriteConversionManifest(root.Root(), manifest));
  const auto stamp = fs::last_write_time(root.Root() / "native-owned.json");
  const auto result =
      Required(extract::LabelNativeAssets(root.Root(), refresh));
  CHECK(result.written == 0);
  CHECK(Read(root.Root(), "native-owned.json") == text);
  CHECK(fs::last_write_time(root.Root() / "native-owned.json") == stamp);
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

TEST_CASE("Semantic recipe IDs preserve directed combinations and every byte",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  const auto before = SmallRecipe(content::WorkshopKind::kBlade);
  WriteWorkshop(root.Root(), before);
  const auto report = Required(extract::UpgradeWorkshopAuthoring(root.Root()));
  CHECK(report.renamed == 3);
  CHECK(report.references == 6);
  const auto after = ReadRecipe(root.Root(), before.kind);
  CHECK(after.blades.at(1).id == "blade.knife");
  CHECK(after.blades.at(2).id == "blade.dirk");
  CHECK(after.pairs.at(0).result == "blade.knife");
  CHECK(after.pairs.at(1).result == "blade.dirk");
  CHECK(Required(extract::EncodeSydRecipes(after)) ==
        Required(extract::EncodeSydRecipes(before)));
  CHECK(Required(extract::UpgradeWorkshopAuthoring(root.Root())).written == 0);
}

TEST_CASE("Recipe migration preserves custom identities labels and stat edits",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  auto before = SmallRecipe(content::WorkshopKind::kBlade);
  before.blades.at(1).id = "blade.owner-choice";
  before.pairs.clear();
  WriteWorkshop(root.Root(), before);
  REQUIRE(extract::LabelNativeAssets(root.Root()));
  const std::string path = content::WorkshopTablePath(
      before.kind, content::WorkshopTablePart::kResults);
  EditCell(root.Root(), "display_name", "Owner's caption", path);
  REQUIRE(extract::UpgradeWorkshopAuthoring(root.Root()));
  const auto after = ReadRecipe(root.Root(), before.kind);
  CHECK(after.blades.at(1).id == "blade.owner-choice");
  CHECK(after.blades.at(1).stats == before.blades.at(1).stats);
  CHECK(Read(root.Root(), path).find("Owner's caption") != std::string::npos);
}

TEST_CASE("Semantic ID collisions fail before any recipe file changes",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames(true)});
  const auto table = SmallRecipe(content::WorkshopKind::kBlade);
  WriteWorkshop(root.Root(), table);
  const std::string path = content::WorkshopTablePath(
      table.kind, content::WorkshopTablePart::kResults);
  const auto before = Read(root.Root(), path);
  const auto migrated = extract::UpgradeWorkshopAuthoring(root.Root());
  REQUIRE_FALSE(migrated.has_value());
  CHECK(migrated.error().find("collides") != std::string::npos);
  CHECK(Read(root.Root(), path) == before);
}

TEST_CASE("Legacy recipe aliases normalize without duplicating directed pairs",
          "[workshop-authoring]") {
  auto table = SmallRecipe(content::WorkshopKind::kBlade);
  table.blades.at(0).id = "blade.none";
  table.blades.at(1).id = "blade.knife";
  table.blades.at(2).id = "blade.dirk";
  const auto parsed = Required(content::ParseWorkshopTables(
      table.kind, content::FormatWorkshopTables(table)));
  CHECK(parsed.pairs.at(0).first == "blade.knife");
  CHECK(parsed.pairs.at(1).first == "blade.dirk");
  table.pairs.push_back({.first = "blade.knife",
                         .second = "blade.dirk",
                         .result = "blade.knife"});
  const auto duplicate = content::ParseWorkshopTables(
      table.kind, content::FormatWorkshopTables(table));
  REQUIRE_FALSE(duplicate.has_value());
  CHECK(duplicate.error().find("duplicate ingredients") != std::string::npos);
}

TEST_CASE("Known categories disambiguate names without numeric ID suffixes",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames(true)});
  auto table = SmallRecipe(content::WorkshopKind::kBlade);
  table.blades.at(1).category = 4;
  table.blades.at(2).category = 7;
  WriteWorkshop(root.Root(), table);
  REQUIRE(extract::UpgradeWorkshopAuthoring(root.Root()));
  const auto named = ReadRecipe(root.Root(), table.kind);
  CHECK(named.blades.at(1).id == "blade.knife.axe-mace");
  CHECK(named.blades.at(2).id == "blade.knife.heavy-mace");
  CHECK(Required(extract::EncodeSydRecipes(named)) ==
        Required(extract::EncodeSydRecipes(table)));
}

TEST_CASE(
    "Unreferenced stock untitled slots move to lossless import provenance",
    "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  const auto table = WithUntitledTail();
  WriteWorkshop(root.Root(), table);
  const auto report = Required(extract::UpgradeWorkshopAuthoring(root.Root()));
  CHECK(report.quarantined == 1);
  const auto named = ReadRecipe(root.Root(), table.kind);
  CHECK(named.blades.size() == 3);
  const auto imported =
      ReadJson(root.Root(), extract::WorkshopImportPath(table.kind));
  CHECK(imported.at("unused_results").at(0).at("legacy_id") == 3);
  CHECK(Required(extract::EncodeWorkshopAuthoring(root.Root(), named)) ==
        Required(extract::EncodeSydRecipes(table)));
  CHECK(Required(extract::UpgradeWorkshopAuthoring(root.Root())).written == 0);
}

TEST_CASE("Former raw record-byte columns move to import provenance",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  const auto table = WriteRawColumns(root.Root());
  const auto report = Required(extract::UpgradeWorkshopAuthoring(root.Root()));
  CHECK(report.reserved == 1);
  constexpr auto kResults = content::WorkshopTablePart::kResults;
  CHECK(Read(root.Root(), content::WorkshopTablePath(table.kind, kResults))
            .find("raw.") == std::string::npos);
  CHECK(Read(root.Root(), content::WorkshopSchemaPath(table.kind, kResults)) ==
        content::FormatTableSchema(
            content::WorkshopTableSpec(table.kind, kResults)));
  const std::array reserved{
      extract::ReservedRecordByte{.legacy_id = 1, .offset = 7, .value = 171}};
  CHECK(Required(extract::EncodeWorkshopAuthoring(
            root.Root(), ReadRecipe(root.Root(), table.kind))) ==
        Required(extract::EncodeSydRecipes(table, reserved)));
  CHECK(Required(extract::UpgradeWorkshopAuthoring(root.Root())).written == 0);
}

TEST_CASE("Moved raw record bytes are version 2 import entries",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  const auto table = WriteRawColumns(root.Root());
  REQUIRE(extract::UpgradeWorkshopAuthoring(root.Root()));
  const auto imported =
      ReadJson(root.Root(), extract::WorkshopImportPath(table.kind));
  CHECK(imported.at("schema_version") == 2);
  REQUIRE(imported.at("reserved_bytes").size() == 1);
  const auto& entry = imported.at("reserved_bytes").at(0);
  CHECK(entry.at("legacy_id") == 1);
  CHECK(entry.at("byte") == 7);
  CHECK(entry.at("value") == 171);
}

TEST_CASE("Version 1 recipe imports upgrade without losing records",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  const auto table = WithUntitledTail();
  WriteWorkshop(root.Root(), table);
  REQUIRE(extract::UpgradeWorkshopAuthoring(root.Root()));
  const std::string path = extract::WorkshopImportPath(table.kind);
  auto version1 = ReadJson(root.Root(), path);
  version1["schema_version"] = 1;
  auto with_reserved = version1;
  version1.erase("reserved_bytes");
  Write(root.Root(), path, content::FormatJsonText(with_reserved) + "\n");
  const auto refused = extract::UpgradeWorkshopAuthoring(root.Root());
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().find("reserved_bytes needs schema_version 2") !=
        std::string::npos);
  Write(root.Root(), path, content::FormatJsonText(version1) + "\n");
  CHECK(Required(extract::UpgradeWorkshopAuthoring(root.Root())).written == 1);
  const auto upgraded = ReadJson(root.Root(), path);
  CHECK(upgraded.at("schema_version") == 2);
  CHECK(upgraded.at("unused_results") == version1.at("unused_results"));
  CHECK(Required(extract::EncodeWorkshopAuthoring(
            root.Root(), ReadRecipe(root.Root(), table.kind))) ==
        Required(extract::EncodeSydRecipes(table)));
}

TEST_CASE("Edited or referenced untitled records require an explicit identity",
          "[workshop-authoring]") {
  const bool referenced = GENERATE(false, true);
  ModelRoot root({RecipeNames()});
  auto table = WithUntitledTail();
  if (referenced) {
    table.pairs.push_back(
        {.first = "blade-001", .second = "blade-001", .result = "blade-003"});
  } else {
    table.blades.back().stats.strength = 1;
  }
  WriteWorkshop(root.Root(), table);
  const auto refused = extract::UpgradeWorkshopAuthoring(root.Root());
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().find("needs an explicit semantic id") !=
        std::string::npos);
  CHECK_FALSE(
      fs::exists(root.Root() / extract::WorkshopImportPath(table.kind)));
  table.blades.back().id = "blade.experimental";
  if (referenced) table.pairs.back().result = "blade.experimental";
  WriteWorkshop(root.Root(), table);
  CHECK(Required(extract::UpgradeWorkshopAuthoring(root.Root())).quarantined ==
        0);
}

TEST_CASE("Shared shield identities cannot hide divergent armor-table copies",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  const auto shield = SmallRecipe(content::WorkshopKind::kShield);
  auto armor = SmallRecipe(content::WorkshopKind::kArmor);
  armor.pieces.at(1).stats.strength = 1;
  WriteWorkshop(root.Root(), shield);
  WriteWorkshop(root.Root(), armor);
  const auto refused = extract::UpgradeWorkshopAuthoring(root.Root());
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().find("differs between recipe tables") !=
        std::string::npos);
  armor.pieces.at(1).id = "shield.buckler.strong";
  WriteWorkshop(root.Root(), armor);
  REQUIRE(extract::UpgradeWorkshopAuthoring(root.Root()));
  CHECK(ReadRecipe(root.Root(), shield.kind).pieces.at(1).id ==
        "shield.buckler");
  CHECK(ReadRecipe(root.Root(), armor.kind).pieces.at(1).id ==
        "shield.buckler.strong");
}

TEST_CASE("Shield-only recipe fields stay exact when armor shares the identity",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  auto shield = SmallRecipe(content::WorkshopKind::kShield);
  const auto armor = SmallRecipe(content::WorkshopKind::kArmor);
  shield.pieces.at(1).gem_slots = 3;
  shield.pieces.at(1).model = 7;
  WriteWorkshop(root.Root(), shield);
  WriteWorkshop(root.Root(), armor);
  REQUIRE(extract::UpgradeWorkshopAuthoring(root.Root()));
  const auto named_shield = ReadRecipe(root.Root(), shield.kind);
  const auto named_armor = ReadRecipe(root.Root(), armor.kind);
  CHECK(named_shield.pieces.at(1).id == named_armor.pieces.at(1).id);
  CHECK(Required(extract::EncodeSydRecipes(named_shield)) ==
        Required(extract::EncodeSydRecipes(shield)));
  CHECK(Required(extract::EncodeSydRecipes(named_armor)) ==
        Required(extract::EncodeSydRecipes(armor)));
}

TEST_CASE("Incremental recipe conversion keeps authored IDs and import records",
          "[workshop-authoring]") {
  constexpr std::size_t kNameWidth = 24;
  auto names = RecipeNames();
  auto table =
      VS::test_support::FullWorkshopTable(content::WorkshopKind::kBlade);
  for (auto& blade : table.blades) {
    blade.id = "blade.fixture-" + std::to_string(blade.legacy_id);
  }
  auto placeholder = WithUntitledTail().blades.back();
  placeholder.legacy_id = static_cast<std::uint32_t>(table.blades.size() - 1);
  placeholder.id = content::LegacyWorkshopResultId(
      content::LayoutOf(table.kind), placeholder.legacy_id);
  table.blades.back() = placeholder;
  std::ranges::copy(std::span(names.bytes).subspan(3 * kNameWidth, kNameWidth),
                    std::span(names.bytes)
                        .subspan(placeholder.legacy_id * kNameWidth)
                        .begin());
  const extract::SourceFile source{
      .path = "MENU/BLADE.SYD",
      .bytes = Required(extract::EncodeSydRecipes(table))};
  ModelRoot root({names, source});
  WriteWorkshop(root.Root(), table);
  const auto converted = Required(extract::ConvertSydRecipes(source));
  content::ConversionManifest manifest;
  manifest.converters[std::string(content::kSydRecipeConverterName)] =
      content::kSydRecipeConverterVersion;
  manifest.outputs.push_back(converted.output);
  REQUIRE(extract::WriteConversionManifest(root.Root(), manifest));
  CHECK(Required(extract::UpgradeWorkshopAuthoring(root.Root())).quarantined ==
        1);
  auto authored = ReadRecipe(root.Root(), table.kind);
  authored.blades.at(1).stats.strength = 42;
  WriteWorkshop(root.Root(), authored);
  CHECK(Required(extract::ConvertDiscTables(root.Root(), false)).unchanged ==
        1);
  CHECK(ReadRecipe(root.Root(), table.kind).blades.at(1).stats.strength == 42);
  REQUIRE(fs::remove(root.Root() / extract::WorkshopImportPath(table.kind)));
  CHECK_FALSE(extract::ConvertDiscTables(root.Root(), false));
}

TEST_CASE(
    "Named recipe domains preserve legacy values and reject invalid names",
    "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  auto table = SmallRecipe(content::WorkshopKind::kBlade);
  table.blades.at(1).category = 1;
  table.blades.at(1).damage_type = 2;
  table.blades.at(1).cost_type = 2;
  table.blades.at(1).cost = 3;
  table.blades.at(1).attack_shape = 4;
  table.materials = {{.first_material = 3,
                      .first_category = 1,
                      .second_material = 4,
                      .second_category = 2,
                      .material = 5}};
  WriteWorkshop(root.Root(), table);
  REQUIRE(extract::UpgradeWorkshopAuthoring(root.Root()));
  const auto result_path = content::WorkshopTablePath(
      table.kind, content::WorkshopTablePart::kResults);
  const auto material_path = content::WorkshopTablePath(
      table.kind, content::WorkshopTablePart::kMaterials);
  CHECK(Read(root.Root(), result_path).find("dagger,edged,risk") !=
        std::string::npos);
  CHECK(Read(root.Root(), result_path).find(",cone,0") != std::string::npos);
  CHECK(Read(root.Root(), material_path)
            .find("bronze,dagger,iron,sword,hagane") != std::string::npos);
  CHECK(Required(
            extract::EncodeSydRecipes(ReadRecipe(root.Root(), table.kind))) ==
        Required(extract::EncodeSydRecipes(table)));
  EditCell(root.Root(), "cost_type", "mana", result_path);
  CHECK_FALSE(extract::UpgradeWorkshopAuthoring(root.Root()));
}

TEST_CASE("Armor recipe categories are authored by name",
          "[workshop-authoring]") {
  ModelRoot root({RecipeNames()});
  auto table = SmallRecipe(content::WorkshopKind::kArmor);
  table.materials = {{.first_material = 3,
                      .first_category = 2,
                      .second_material = 4,
                      .second_category = 5,
                      .material = 5}};
  WriteWorkshop(root.Root(), table);
  REQUIRE(extract::UpgradeWorkshopAuthoring(root.Root()));
  CHECK(Read(root.Root(), content::WorkshopTablePath(
                              table.kind, content::WorkshopTablePart::kResults))
            .find(",shield,") != std::string::npos);
  CHECK(
      Read(root.Root(), content::WorkshopTablePath(
                            table.kind, content::WorkshopTablePart::kMaterials))
          .find("bronze,helm,iron,glove,hagane") != std::string::npos);
  CHECK(Required(
            extract::EncodeSydRecipes(ReadRecipe(root.Root(), table.kind))) ==
        Required(extract::EncodeSydRecipes(table)));
}

TEST_CASE("Numeric enum aliases cannot create duplicate material rules",
          "[workshop-authoring]") {
  const auto kind = content::WorkshopKind::kShield;
  auto table = SmallRecipe(kind);
  table.materials = {{.first_material = 3,
                      .first_category = 0,
                      .second_material = 4,
                      .second_category = 0,
                      .material = 5}};
  auto texts = content::FormatWorkshopTables(table);
  texts.at(2) += "bronze,iron,hagane\n";
  const auto duplicate = content::ParseWorkshopTables(kind, texts);
  REQUIRE_FALSE(duplicate.has_value());
  CHECK(duplicate.error().find("repeats the key") != std::string::npos);
}
