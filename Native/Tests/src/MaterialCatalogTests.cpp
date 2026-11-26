#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/CatalogTable.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Content/MaterialCatalog.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/MaterialConversion.hpp"
#include "Menu/Setup/Combination.hpp"
#include "Menu/Setup/SetupLive.hpp"
#include "Platform/Paths.hpp"
#include "TestSupport/WorkshopRecipeRoot.hpp"

namespace {
namespace content = VS::Content;
namespace extract = VS::Extract;
namespace setup = VS::Menu::Setup;
namespace fs = std::filesystem;
using VS::test_support::WorkshopRecipeRoot;

template <typename Value>
Value RequireValue(std::expected<Value, std::string> result) {
  if (!result) {
    FAIL(result.error());
  }
  return std::move(*result);
}

const content::MaterialCatalog& Materials() {
  const auto* store = content::DataStore::Get();
  if (store == nullptr || store->Materials() == nullptr) {
    FAIL("no material definitions");
  }
  return *store->Materials();
}

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.is_open());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

void Write(const fs::path& path, std::string_view text) {
  fs::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
  REQUIRE(file.good());
}

extract::ImportedMaterials Synthetic() {
  std::array<std::byte, extract::kMaterialImportBytes> bytes{};
  auto imported = extract::DecodeMaterials(bytes);
  if (!imported) {
    FAIL(imported.error());
  }
  for (std::size_t slot = 0; slot < imported->definitions.size(); ++slot) {
    auto& definition = imported->definitions[slot];
    definition.strength = static_cast<std::int32_t>(slot);
    definition.intelligence = -2;
    definition.agility = 1;
    imported->archived[slot].max_dp = 3500;
    imported->archived[slot].padding = {std::byte{0xA5},
                                        static_cast<std::byte>(slot)};
  }
  return *imported;
}

void Prepare(WorkshopRecipeRoot& root) {
  constexpr auto kShield = content::WorkshopKind::kShield;
  auto& table = root.Table(kShield);
  table.pieces.at(3).stats = {
      .strength = -40, .intelligence = 10, .agility = -5};
  table.pairs = {{"shield-001", "shield-002", "shield-003"}};
  table.materials = {{.first_material = 3,
                      .first_category = 0,
                      .second_material = 4,
                      .second_category = 0,
                      .material = 3}};
  root.WriteSources();
  REQUIRE(root.Convert(false).converted == 3);
  const auto records = extract::EncodeMaterials(Synthetic());
  if (!records) {
    FAIL(records.error());
  }
  std::string source(extract::kMaterialSourceOffset, '\0');
  source.append(reinterpret_cast<const char*>(records->data()),
                records->size());
  Write(root.Root() / "disc" / content::kMaterialSource, source);
  std::ofstream manifest(root.Root() / content::DataStore::ManifestName,
                         std::ios::app);
  manifest << "200\t" << source.size() << "\traw\t" << content::kMaterialSource
           << '\n';
  REQUIRE(manifest.good());
  manifest.close();
  const auto converted = extract::ConvertDiscMaterials(root.Root(), false);
  if (!converted) {
    FAIL(converted.error());
  }
  CHECK(converted->converted == 1);
}

void Open(const WorkshopRecipeRoot& root) {
  auto opened = content::DataStore::Open(root.Root());
  if (!opened) {
    FAIL(opened.error());
  }
}

const content::DataStore& Store() {
  const auto* store = content::DataStore::Get();
  if (store == nullptr) {
    FAIL("no material store");
  }
  return *store;
}

std::expected<setup::MaterialBonuses, std::string> LiveMaterials() {
  std::expected<setup::MaterialBonuses, std::string> result =
      std::unexpected("host was not called");
  setup::RunLiveSetup([&](setup::Context& context) {
    result = context.host.Materials();
    return 0;
  });
  return result;
}

std::expected<VS::Items::InventoryArmor, std::string> LiveShield() {
  auto bonuses = LiveMaterials();
  if (!bonuses) {
    return std::unexpected(bonuses.error());
  }
  std::expected<VS::Items::InventoryArmor, std::string> result =
      std::unexpected("recipe host was not called");
  setup::RunLiveSetup([&](setup::Context& context) {
    context.host.StartRecipeLoad(setup::RecipeKind::kShield);
    context.host.ReleaseRecipeRequest();
    const auto* recipes = context.host.Recipes();
    if (recipes == nullptr) {
      return 0;
    }
    VS::Items::InventoryArmor first{};
    VS::Items::InventoryArmor second{};
    first.sub_id = 1;
    first.material = 3;
    second.sub_id = 2;
    second.material = 4;
    result = setup::BuildCombinedShield(first, second, *recipes, *bonuses);
    return 0;
  });
  setup::ResetLiveSetupStatics();
  return result;
}

void Layers(const WorkshopRecipeRoot& root, std::string_view patch) {
  fs::create_directories(root.Root() / "patch");
  fs::create_directories(root.Root() / "mod");
  Write(
      root.Root() / "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["patch","mod"]})");
  Write(root.Root() / "mod" / content::kMaterialTablePath, patch);
}

std::string Edit(std::string_view text, std::size_t row,
                 std::string_view column, std::string value) {
  auto csv = content::ParseCsv(text, content::kMaterialTablePath);
  if (!csv) {
    FAIL(csv.error());
  }
  auto found = std::ranges::find(csv->front().fields, column);
  if (found == csv->front().fields.end()) {
    FAIL("missing test column");
  }
  const auto index =
      static_cast<std::size_t>(found - csv->front().fields.begin());
  csv->at(row).fields.at(index) = std::move(value);
  std::vector<std::vector<std::string>> rows;
  for (auto& record : *csv) {
    rows.push_back(std::move(record.fields));
  }
  return content::FormatCsv(rows);
}

content::ConversionManifest Manifest(const WorkshopRecipeRoot& root) {
  auto loaded = RequireValue(content::LoadConversionManifest(root.Root()));
  if (!loaded.has_value()) {
    throw std::runtime_error("no conversion manifest");
  }
  return std::move(*loaded);
}

// Rewrites a converted root as a version 1 material tree: the class,
// affinity and max_dp columns in the table, the paddings alone in the
// provenance. Bronze's class_beast cell is `beast` and its strength 40000.
void Version1(const WorkshopRecipeRoot& root, std::string_view beast) {
  std::vector<std::vector<std::string>> rows{{"id",
                                              "legacy_id",
                                              "class_human",
                                              "class_beast",
                                              "class_undead",
                                              "class_phantom",
                                              "class_dragon",
                                              "class_evil",
                                              "attack_affinity_physical",
                                              "attack_affinity_air",
                                              "attack_affinity_fire",
                                              "attack_affinity_earth",
                                              "attack_affinity_water",
                                              "attack_affinity_light",
                                              "attack_affinity_dark",
                                              "defense_affinity_physical",
                                              "defense_affinity_air",
                                              "defense_affinity_fire",
                                              "defense_affinity_earth",
                                              "defense_affinity_water",
                                              "defense_affinity_light",
                                              "defense_affinity_dark",
                                              "max_dp",
                                              "strength",
                                              "intelligence",
                                              "agility",
                                              "display_name"}};
  constexpr std::size_t kBronze = 3;
  constexpr std::size_t kModifierColumns = 20;
  auto reserved = content::JsonDocument::array();
  for (std::size_t slot = 0; slot < content::kMaterialIds.size(); ++slot) {
    std::vector<std::string> row{std::string(content::kMaterialIds[slot]),
                                 std::to_string(slot)};
    for (std::size_t column = 0; column < kModifierColumns; ++column) {
      row.emplace_back(slot == kBronze && column == 1 ? beast : "0");
    }
    row.emplace_back("3500");
    row.emplace_back(slot == kBronze ? "40000" : std::to_string(slot));
    row.emplace_back("-2");
    row.emplace_back("1");
    row.push_back("Material " + std::to_string(slot) + ", labelled");
    rows.push_back(std::move(row));
    reserved.push_back({{"id", content::kMaterialIds[slot]},
                        {"attack_padding", 0xA5},
                        {"defense_padding", slot}});
  }
  Write(root.Root() / content::kMaterialTablePath, content::FormatCsv(rows));
  const auto import = root.Root() / extract::kMaterialImportPath;
  const auto current = Read(import);
  const auto json = RequireValue(content::ParseJsonText(
      std::as_bytes(std::span(current)), extract::kMaterialImportPath));
  Write(import, content::FormatJsonText(
                    {{"display_name", "Old label"},
                     {"schema", "vs.material-import"},
                     {"schema_version", 1},
                     {"source", content::kMaterialSource},
                     {"source_sha256", json.at("source_sha256")},
                     {"byte_offset", extract::kMaterialSourceOffset},
                     {"record_bytes", extract::kMaterialRecordBytes},
                     {"reserved", std::move(reserved)}}) +
                    "\n");
  auto manifest = Manifest(root);
  manifest.converters[std::string(content::kMaterialConverterName)] = 1;
  REQUIRE(extract::WriteConversionManifest(root.Root(), manifest));
}

std::uint32_t ConverterVersion(const WorkshopRecipeRoot& root) {
  const auto manifest = Manifest(root);
  const auto found =
      manifest.converters.find(std::string(content::kMaterialConverterName));
  if (found == manifest.converters.end()) {
    throw std::runtime_error("no material converter version");
  }
  return found->second;
}
}  // namespace

TEST_CASE("Material import preserves signed fields and both reserved bytes",
          "[material-catalog][content-tables]") {
  std::array<std::byte, extract::kMaterialImportBytes> bytes{};
  for (std::size_t at = 0; at < bytes.size(); ++at) {
    bytes[at] = static_cast<std::byte>((at * 37) & 255);
  }
  const auto imported = RequireValue(extract::DecodeMaterials(bytes));
  const auto csv = content::FormatMaterials(imported.definitions);
  auto parsed = RequireValue(content::ParseMaterials(csv));
  const auto provenance = extract::FormatMaterialProvenance(
      imported.archived, std::string(64, 'a'));
  auto archived = RequireValue(
      extract::ParseMaterialProvenance(std::as_bytes(std::span(provenance))));
  auto encoded = RequireValue(extract::EncodeMaterials({parsed, archived}));
  CHECK(std::ranges::equal(encoded, bytes));
  CHECK(csv.contains("material.damascus"));
  CHECK(csv.contains("display_name"));
  CHECK_FALSE(csv.contains("padding"));
  CHECK_FALSE(csv.contains("class_"));
  CHECK_FALSE(csv.contains("affinity"));
  CHECK_FALSE(csv.contains("max_dp"));
}

TEST_CASE("Native material definitions do not inherit import field widths",
          "[material-catalog]") {
  auto imported = Synthetic();
  imported.definitions[3].strength = std::numeric_limits<std::int32_t>::max();
  auto parsed =
      content::ParseMaterials(content::FormatMaterials(imported.definitions));
  if (!parsed) {
    FAIL(parsed.error());
  }
  CHECK(*parsed == imported.definitions);
  CHECK_FALSE(extract::EncodeMaterials(imported).has_value());
  auto archived = Synthetic();
  archived.archived[3].classes[0] = 1000;
  CHECK_FALSE(extract::EncodeMaterials(archived).has_value());
  archived = Synthetic();
  archived.archived[3].max_dp = 100000;
  CHECK_FALSE(extract::EncodeMaterials(archived).has_value());
}

TEST_CASE(
    "Material definitions reject malformed values and unsupported identities",
    "[material-catalog]") {
  const auto change =
      GENERATE(std::pair{"strength", "2147483648"},
               std::pair{"intelligence", "-2147483649"},
               std::pair{"id", "material.mithril"}, std::pair{"legacy_id", "8"},
               std::pair{"strength", "strong"});
  const auto csv = Edit(content::FormatMaterials(Synthetic().definitions), 4,
                        change.first, change.second);
  const auto parsed = content::ParseMaterials(csv);
  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().contains("materials.csv:"));
  CHECK(parsed.error().contains("column"));
}

TEST_CASE("Material catalogs refuse duplicate keys and missing kind references",
          "[material-catalog]") {
  auto imported = Synthetic();
  imported.definitions[4].id = imported.definitions[3].id;
  CHECK_FALSE(
      content::ParseMaterials(content::FormatMaterials(imported.definitions)));
  auto csv = content::FormatMaterials(Synthetic().definitions);
  csv.erase(csv.rfind("material.damascus"));
  const auto parsed = content::ParseMaterials(csv);
  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().contains(
      "missing required material reference material.damascus"));
}

TEST_CASE(
    "Material imports reject truncated records and stale provenance identities",
    "[material-catalog]") {
  std::array<std::byte, extract::kMaterialImportBytes - 1> short_records{};
  CHECK_FALSE(extract::DecodeMaterials(short_records));
  auto provenance = extract::FormatMaterialProvenance(Synthetic().archived,
                                                      std::string(64, 'a'));
  provenance.replace(provenance.find("material.wood"),
                     std::string("material.wood").size(), "material.none");
  CHECK_FALSE(
      extract::ParseMaterialProvenance(std::as_bytes(std::span(provenance))));
}

TEST_CASE(
    "Material layers reach live workshop arithmetic without changing base "
    "sources",
    "[material-catalog][catalog-layers][workshop]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  const auto source = Read(root.Root() / "disc" / content::kMaterialSource);
  const auto csv = Read(root.Root() / content::kMaterialTablePath);
  Layers(root, "id,strength\nmaterial.bronze,150\n");
  Write(root.Root() / "patch" / content::kMaterialTablePath,
        "id,strength\nmaterial.bronze,140\n");
  Open(root);
  const auto result = RequireValue(LiveShield());
  CHECK(result.strength == 110);
  CHECK(result.intelligence == 8);
  CHECK(result.agility == -4);
  CHECK(Materials().at(3).strength == 150);
  CHECK(Read(root.Root() / content::kMaterialTablePath) == csv);
  CHECK(Read(root.Root() / "disc" / content::kMaterialSource) == source);
}

TEST_CASE("Material layers refuse the archived fields no game code reads",
          "[material-catalog][catalog-layers]") {
  const auto column = GENERATE("max_dp", "class_beast", "attack_affinity_air");
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  Layers(root, std::string("id,") + column + "\nmaterial.bronze,100\n");
  const auto opened = content::DataStore::Open(root.Root());
  REQUIRE_FALSE(opened.has_value());
  CHECK(opened.error().contains(column));
}

TEST_CASE("Live material arithmetic refuses unrepresentable inventory stats",
          "[material-catalog][workshop]") {
  const auto value = GENERATE("2147483647", "-2147483648");
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  Layers(root, std::string("id,strength\nmaterial.bronze,") + value + "\n");
  Open(root);
  const auto result = LiveShield();
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().contains("retained inventory stat storage"));
}

TEST_CASE(
    "Material cache reopens and invalidates on content with unchanged "
    "timestamps",
    "[material-catalog][catalog-cache]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  Open(root);
  REQUIRE(Store().CatalogTables() != nullptr);
  CHECK_FALSE(Store().CatalogTables()->CacheHit());
  Open(root);
  CHECK(Store().CatalogTables()->CacheHit());
  const auto file = root.Root() / content::kMaterialTablePath;
  const auto stamp = fs::last_write_time(file);
  Write(file, Edit(Read(file), 4, "strength", "9"));
  fs::last_write_time(file, stamp);
  Open(root);
  CHECK_FALSE(Store().CatalogTables()->CacheHit());
  const auto bonuses = LiveMaterials();
  if (!bonuses) {
    FAIL(bonuses.error());
  }
  CHECK(bonuses->at(3).strength == 9);
}

TEST_CASE("Failed material layer opens preserve the prior store and cache",
          "[material-catalog][catalog-cache]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  Open(root);
  const auto* previous = &Store();
  const auto cache_file = root.Root() / ".cache" / "catalog.sqlite";
  const auto cache = Read(cache_file);
  Layers(root, "_op,id\nremove,material.bronze\n");
  const auto failed = content::DataStore::Open(root.Root());
  REQUIRE_FALSE(failed.has_value());
  CHECK(failed.error().contains("material.bronze"));
  CHECK(&Store() == previous);
  CHECK(Read(cache_file) == cache);
}

TEST_CASE(
    "Material conversion preserves edits and unchanged outputs on repeat runs",
    "[material-catalog]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  const auto file = root.Root() / content::kMaterialTablePath;
  const auto edited = Edit(Read(file), 4, "strength", "40000");
  Write(file, edited);
  const auto stamp = fs::last_write_time(file);
  const auto manifest = Read(root.Root() / "manifest.json");
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto result = extract::ConvertDiscMaterials(root.Root(), false);
    if (!result) {
      FAIL(result.error());
    }
    CHECK(result->unchanged == 1);
  }
  CHECK(Read(file) == edited);
  CHECK(fs::last_write_time(file) == stamp);
  CHECK(Read(root.Root() / "manifest.json") == manifest);
}

TEST_CASE(
    "Material authoring archives version 1 columns with their edits and keeps "
    "labels",
    "[material-catalog]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Version1(root, "1000");
  const auto converted =
      RequireValue(extract::ConvertDiscMaterials(root.Root(), false));
  CHECK(converted.converted == 0);
  CHECK(converted.unchanged == 1);
  CHECK(ConverterVersion(root) == content::kMaterialConverterVersion);
  const auto table = Read(root.Root() / content::kMaterialTablePath);
  CHECK(table.starts_with(
      "id,legacy_id,strength,intelligence,agility,display_name\n"));
  CHECK(
      table.contains("material.bronze,3,40000,-2,1,\"Material 3, labelled\""));
  const auto import = Read(root.Root() / extract::kMaterialImportPath);
  CHECK(import.starts_with("{\n  \"display_name\": \"Old label\",\n"));
  const auto archived = RequireValue(
      extract::ParseMaterialProvenance(std::as_bytes(std::span(import))));
  CHECK(archived[3].classes[1] == 1000);
  CHECK(archived[3].max_dp == 3500);
  CHECK(archived[5].padding ==
        extract::MaterialPadding{std::byte{0xA5}, std::byte{5}});
}

TEST_CASE("Upgraded material authoring is stable and loads its edits",
          "[material-catalog]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  Version1(root, "1000");
  REQUIRE(extract::ConvertDiscMaterials(root.Root(), false).has_value());
  const auto table = Read(root.Root() / content::kMaterialTablePath);
  const auto manifest = Read(root.Root() / "manifest.json");
  const auto again =
      RequireValue(extract::UpgradeMaterialAuthoring(root.Root()));
  CHECK(again.written == 0);
  CHECK(RequireValue(extract::ConvertDiscMaterials(root.Root(), false))
            .unchanged == 1);
  CHECK(Read(root.Root() / content::kMaterialTablePath) == table);
  CHECK(Read(root.Root() / "manifest.json") == manifest);
  Open(root);
  CHECK(RequireValue(LiveMaterials()).at(3).strength == 40000);
}

TEST_CASE("Material authoring refuses a version 1 cell it cannot archive",
          "[material-catalog]") {
  const auto beast = GENERATE("strong", "2147483648", "");
  WorkshopRecipeRoot root;
  Prepare(root);
  Version1(root, beast);
  const auto table = Read(root.Root() / content::kMaterialTablePath);
  const auto upgraded = extract::UpgradeMaterialAuthoring(root.Root());
  REQUIRE_FALSE(upgraded.has_value());
  CHECK(upgraded.error().contains("column 'class_beast'"));
  CHECK(Read(root.Root() / content::kMaterialTablePath) == table);
  CHECK(ConverterVersion(root) == 1);
}

TEST_CASE(
    "Missing material catalogs refuse live reads rather than return zero "
    "bonuses",
    "[material-catalog]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  root.WriteSources();
  Open(root);
  const auto result = LiveMaterials();
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().contains("VS.Extract tables"));
}

TEST_CASE("Typed material validation recompiles a corrupted cached identity",
          "[material-catalog][catalog-cache]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  Open(root);
  const auto file = root.Root() / ".cache" / "catalog.sqlite";
  sqlite3* handle = nullptr;
  REQUIRE(sqlite3_open_v2(VS::Platform::PathText(file).c_str(), &handle,
                          SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
  {
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> database(
        handle, sqlite3_close_v2);
    REQUIRE(sqlite3_exec(database.get(),
                         "UPDATE materials SET id='material.mithril' WHERE "
                         "id='material.bronze'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
  }
  Open(root);
  REQUIRE(Store().Materials() != nullptr);
  CHECK(Store().Materials()->at(3).id == "material.bronze");
  REQUIRE(Store().CatalogTables() != nullptr);
  CHECK_FALSE(Store().CatalogTables()->CacheHit());
  Open(root);
  CHECK(Store().CatalogTables()->CacheHit());
}

TEST_CASE("Committed material schema matches the importer contract",
          "[material-catalog][content-tables]") {
  const auto path =
      fs::path("Native") / "Content" / "schemas" / content::kMaterialSchemaPath;
  auto text = Read(path);
  std::erase(text, '\r');
  CHECK(text == content::FormatTableSchema(content::MaterialTableSpec()));
}

TEST_CASE(
    "Material import matches all eight original records when disc data is "
    "present",
    "[material-catalog][content-tables]") {
  const fs::path source =
      fs::path("Native") / "data" / "disc" / content::kMaterialSource;
  if (!fs::is_regular_file(source)) {
    SKIP("optional local disc material preservation");
  }
  const auto bytes = RequireValue(extract::ReadFileBytes(source));
  REQUIRE(bytes.size() >=
          extract::kMaterialSourceOffset + extract::kMaterialImportBytes);
  const auto original = std::span(bytes).subspan(extract::kMaterialSourceOffset,
                                                 extract::kMaterialImportBytes);
  auto imported = RequireValue(extract::DecodeMaterials(original));
  const auto text = content::FormatMaterials(imported.definitions);
  auto parsed = RequireValue(content::ParseMaterials(text));
  const auto encoded =
      RequireValue(extract::EncodeMaterials({parsed, imported.archived}));
  CHECK(std::ranges::equal(encoded, original));
  CHECK(imported.archived[1].max_dp == 3500);
  CHECK(imported.definitions[7].strength == 8);
}
