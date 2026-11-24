#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/BestiaryCatalog.hpp"
#include "Content/CatalogDatabase.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Content/NativeText.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Menu/Setup/Combination.hpp"
#include "Menu/Setup/SetupLive.hpp"
#include "Platform/Paths.hpp"
#include "TestSupport/WorkshopRecipeRoot.hpp"

namespace {
namespace content = VS::Content;
namespace setup = VS::Menu::Setup;
namespace fs = std::filesystem;
using content::WorkshopKind;
using content::WorkshopTablePart;
using VS::test_support::WorkshopRecipeRoot;

void Write(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
  REQUIRE(file.good());
}

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.is_open());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

void Prepare(WorkshopRecipeRoot& root) {
  root.WriteSources();
  REQUIRE(root.Convert(false).converted == 3);
  auto& blade = root.Table(WorkshopKind::kBlade);
  blade.blades[1].id = "blade.battle-knife";
  blade.blades[2].id = "blade.scramasax";
  blade.blades[1].stats.strength = 10;
  blade.pairs = {{"blade.battle-knife", "blade.scramasax", "blade.scramasax"}};
  const auto texts = content::FormatWorkshopTables(blade);
  for (const auto part : content::kWorkshopTableParts) {
    Write(root.TableFile(WorkshopKind::kBlade, part),
          texts.at(static_cast<std::size_t>(part)));
  }
}

void Layers(const WorkshopRecipeRoot& root) {
  fs::create_directories(root.Root() / "patch");
  fs::create_directories(root.Root() / "mod");
  Write(
      root.Root() / "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["patch","mod"]})");
}

void Patch(const WorkshopRecipeRoot& root, std::string_view layer,
           WorkshopTablePart part, const std::string& text) {
  Write(root.Root() / layer /
            content::WorkshopTablePath(WorkshopKind::kBlade, part),
        text);
}

content::CatalogDatabase Load(const WorkshopRecipeRoot& root) {
  auto database = content::CatalogDatabase::Load(
      root.Root(), content::WorkshopCatalogSources());
  if (!database) FAIL(database.error());
  return std::move(*database);
}

content::WorkshopRecipeCatalog Recipes(
    const content::CatalogDatabase& database) {
  auto recipes = content::LoadWorkshopRecipes(database);
  if (!recipes) FAIL(recipes.error());
  return std::move(*recipes);
}

void Persist(const content::CatalogDatabase& database) {
  const auto validated = content::LoadWorkshopRecipes(database);
  if (!validated) FAIL(validated.error());
  const auto published = database.PublishCache();
  if (!published) FAIL(published.error());
}

void EditCache(const fs::path& path, const char* sql) {
  sqlite3* raw = nullptr;
  const auto opened = sqlite3_open_v2(VS::Platform::PathText(path).c_str(),
                                      &raw, SQLITE_OPEN_READWRITE, nullptr);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> database(
      raw, sqlite3_close_v2);
  REQUIRE(opened == SQLITE_OK);
  REQUIRE(sqlite3_exec(database.get(), sql, nullptr, nullptr, nullptr) ==
          SQLITE_OK);
}

struct LiveRecipes {
  ~LiveRecipes() { setup::ResetLiveSetupStatics(); }
};
}  // namespace

TEST_CASE(
    "Bestiary text references resolve shared IDs and identify bad layer cells",
    "[catalog-layers][bestiary]") {
  WorkshopRecipeRoot root;
  content::BestiaryCatalog catalog;
  for (unsigned i = 0; i < 8; ++i) {
    catalog.push_back({"bestiary.notes-" + std::to_string(i), -1, i + 1,
                       content::BestiaryClass::kHuman, 0,
                       content::BestiaryUnlock::kAlways, 0, 0,
                       "text.catalog.note", "text.catalog.note"});
  }
  const auto csv = content::FormatBestiary(catalog);
  const auto path = root.Root() / content::kBestiaryPath;
  Write(path, csv);
  Write(root.Root() / content::kBestiarySchemaPath,
        content::FormatTableSchema(content::BestiaryTableSpec()));
  const std::array sources{content::BestiaryCatalogSource()};
  auto database = content::CatalogDatabase::Load(root.Root(), sources);
  INFO((database ? "" : database.error()));
  REQUIRE(database);
  content::NativeTextCatalog text{"font.test", 1, {{"text.catalog.note", {}}}};
  CHECK(content::ValidateBestiaryTextReferences(*database, text));
  Write(root.Root() / "mod" / content::kBestiaryPath,
        "id,name\nbestiary.notes-0,text.catalog.missing\n");
  Write(
      root.Root() / "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mod"]})");
  database = content::CatalogDatabase::Load(root.Root(), sources);
  INFO((database ? "" : database.error()));
  REQUIRE(database);
  const auto checked = content::ValidateBestiaryTextReferences(*database, text);
  REQUIRE_FALSE(checked);
  CHECK(checked.error().find(":2: column 'name'") != std::string::npos);
  CHECK(checked.error().find("mod") != std::string::npos);
  CHECK(Read(path) == csv);
}

TEST_CASE("Layer headers resolve legacy column aliases without ambiguity",
          "[catalog-layers]") {
  const bool duplicate = GENERATE(false, true);
  WorkshopRecipeRoot root;
  Prepare(root);
  Layers(root);
  auto sources = content::WorkshopCatalogSources();
  std::vector<content::ColumnSpec> columns(sources.front().spec.columns.begin(),
                                           sources.front().spec.columns.end());
  for (auto& column : columns) {
    if (column.name == "strength") column.legacy_name = "str";
  }
  sources.front().spec.columns = columns;
  Patch(root, "mod", WorkshopTablePart::kResults,
        duplicate ? "id,str,strength\nblade.battle-knife,25,26\n"
                  : "id,str\nblade.battle-knife,25\n");
  auto database = content::CatalogDatabase::Load(root.Root(), sources);
  if (duplicate) {
    REQUIRE_FALSE(database.has_value());
    CHECK(database.error().contains("duplicate aliased column"));
  } else {
    if (!database) FAIL(database.error());
    CHECK(Recipes(*database)
              .Of(WorkshopKind::kBlade)
              .blades.at(1)
              .stats.strength == 25);
  }
}

TEST_CASE("SQLite catalogs preserve base records and directed recipes",
          "[catalog-layers][workshop]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  const auto base = content::LoadWorkshopRecipes(root.Root());
  if (!base) FAIL(base.error());
  const auto database = Load(root);
  CHECK(Recipes(database).tables == base->tables);
  CHECK(database.Conflicts().empty());
  const auto rows = database.Read(content::WorkshopTableSpec(
      WorkshopKind::kBlade, WorkshopTablePart::kResults));
  if (!rows) FAIL(rows.error());
  CHECK(rows->at(1).cells.at(0) == "blade.battle-knife");
  CHECK(rows->at(1).values.at(8) == 10);
}

TEST_CASE(
    "Validated catalogs persist as readable SQLite and reopen without "
    "compilation",
    "[catalog-cache]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  const auto first = Load(root);
  CHECK_FALSE(first.CacheHit());
  Persist(first);
  const auto file = root.Root() / ".cache" / "catalog.sqlite";
  CHECK(Read(file).starts_with("SQLite format 3"));
  const auto stamp = fs::last_write_time(file);
  const auto second = Load(root);
  CHECK(second.CacheHit());
  CHECK(Recipes(second).tables == Recipes(first).tables);
  Persist(second);
  CHECK(fs::last_write_time(file) == stamp);
}

TEST_CASE(
    "Catalog fingerprints use CSV contents even when size and time are "
    "preserved",
    "[catalog-cache]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Persist(Load(root));
  const auto path =
      root.TableFile(WorkshopKind::kBlade, WorkshopTablePart::kResults);
  const auto stamp = fs::last_write_time(path);
  const auto size = fs::file_size(path);
  root.Table(WorkshopKind::kBlade).blades[1].stats.strength = 11;
  Write(path,
        content::FormatWorkshopTables(root.Table(WorkshopKind::kBlade))[0]);
  fs::last_write_time(path, stamp);
  REQUIRE(fs::file_size(path) == size);
  const auto changed = Load(root);
  CHECK_FALSE(changed.CacheHit());
  CHECK(Recipes(changed).Of(WorkshopKind::kBlade).blades[1].stats.strength ==
        11);
  Persist(changed);
  CHECK(Load(root).CacheHit());
}

TEST_CASE("Catalog cache invalidates on external and compiled schema changes",
          "[catalog-cache]") {
  const bool compiled_contract = GENERATE(false, true);
  WorkshopRecipeRoot root;
  Prepare(root);
  Persist(Load(root));
  auto sources = content::WorkshopCatalogSources();
  std::vector<content::ColumnSpec> columns(sources.front().spec.columns.begin(),
                                           sources.front().spec.columns.end());
  if (compiled_contract) {
    columns.front().description = "A changed compiler contract";
    sources.front().spec.columns = columns;
  } else {
    const auto schema =
        root.Root() / content::WorkshopSchemaPath(WorkshopKind::kBlade,
                                                  WorkshopTablePart::kResults);
    const auto stamp = fs::last_write_time(schema);
    auto text = Read(schema);
    const auto at = text.find("Semantic result id");
    REQUIRE(at != std::string::npos);
    text.replace(at, std::string_view("Semantic result id").size(),
                 "Semantic result ID");
    Write(schema, text);
    fs::last_write_time(schema, stamp);
  }
  auto changed = content::CatalogDatabase::Load(root.Root(), sources);
  if (!changed) FAIL(changed.error());
  CHECK_FALSE(changed->CacheHit());
}

TEST_CASE(
    "Catalog cache tracks layer additions deletion order and conflict reports",
    "[catalog-cache]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Persist(Load(root));
  Layers(root);
  Patch(root, "patch", WorkshopTablePart::kResults,
        "id,strength\nblade.battle-knife,20\n");
  Patch(root, "mod", WorkshopTablePart::kResults,
        "id,strength\nblade.battle-knife,30\n");
  const auto layered = Load(root);
  CHECK_FALSE(layered.CacheHit());
  Persist(layered);
  const auto hit = Load(root);
  CHECK(hit.CacheHit());
  REQUIRE(hit.Conflicts().size() == 1);
  CHECK(hit.Conflicts()[0].winning_source ==
        layered.Conflicts()[0].winning_source);
  Write(
      root.Root() / "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mod","patch"]})");
  const auto reordered = Load(root);
  CHECK_FALSE(reordered.CacheHit());
  CHECK(Recipes(reordered).Of(WorkshopKind::kBlade).blades[1].stats.strength ==
        20);
  Persist(reordered);
  const auto file = root.Root() / "patch" /
                    content::WorkshopTablePath(WorkshopKind::kBlade,
                                               WorkshopTablePart::kResults);
  REQUIRE(fs::remove(file));
  CHECK_FALSE(Load(root).CacheHit());
}

TEST_CASE("Malformed compiled databases rebuild from validated CSV sources",
          "[catalog-cache]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Persist(Load(root));
  const auto file = root.Root() / ".cache" / "catalog.sqlite";
  Write(file, "broken database");
  const auto recovered = Load(root);
  CHECK_FALSE(recovered.CacheHit());
  CHECK(Recipes(recovered).Of(WorkshopKind::kBlade).blades[1].stats.strength ==
        10);
  Persist(recovered);
  CHECK(Load(root).CacheHit());
}

TEST_CASE("Failed typed catalog validation cannot replace the persistent cache",
          "[catalog-cache]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  REQUIRE(content::DataStore::Open(root.Root()).has_value());
  const auto file = root.Root() / ".cache" / "catalog.sqlite";
  const auto before = Read(file);
  const auto* previous = content::DataStore::Get();
  Layers(root);
  Patch(root, "mod", WorkshopTablePart::kResults,
        "id,cost\nblade.battle-knife,5\n");
  const auto rejected = content::DataStore::Open(root.Root());
  REQUIRE_FALSE(rejected.has_value());
  CHECK(rejected.error().contains("none cost_type requires zero cost"));
  CHECK(Read(file) == before);
  CHECK(content::DataStore::Get() == previous);
}

TEST_CASE("Catalog publication refuses inputs changed after compilation",
          "[catalog-cache]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Persist(Load(root));
  const auto file = root.Root() / ".cache" / "catalog.sqlite";
  const auto before = Read(file);
  auto pending = content::CatalogDatabase::Load(
      root.Root(), content::WorkshopCatalogSources(), false);
  if (!pending) FAIL(pending.error());
  Layers(root);
  const auto published = pending->PublishCache();
  REQUIRE_FALSE(published.has_value());
  CHECK(published.error().contains("inputs changed"));
  CHECK(Read(file) == before);
}

TEST_CASE(
    "Obsolete cache compiler versions rebuild without touching source files",
    "[catalog-cache]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Persist(Load(root));
  EditCache(root.Root() / ".cache" / "catalog.sqlite",
            "UPDATE __catalog_cache SET version=999");
  const auto rebuilt = Load(root);
  CHECK_FALSE(rebuilt.CacheHit());
  CHECK(Recipes(rebuilt).Of(WorkshopKind::kBlade).blades[1].stats.strength ==
        10);
}

TEST_CASE(
    "Typed validation rejects damaged cache records and recompiles their "
    "source",
    "[catalog-cache]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  Persist(Load(root));
  EditCache(root.Root() / ".cache" / "catalog.sqlite",
            "UPDATE \"combination-blade-results\" SET cost=5 WHERE "
            "id='blade.battle-knife'");
  const auto opened = content::DataStore::Open(root.Root());
  if (!opened) FAIL(opened.error());
  const auto* store = content::DataStore::Get();
  REQUIRE(store != nullptr);
  REQUIRE(store->CatalogTables() != nullptr);
  CHECK_FALSE(store->CatalogTables()->CacheHit());
  CHECK(store->WorkshopRecipes()->Of(WorkshopKind::kBlade).blades[1].cost == 0);
}

TEST_CASE("An unavailable cache location does not reject valid catalogs",
          "[catalog-cache]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  Write(root.Root() / ".cache", "not a directory");
  const auto opened = content::DataStore::Open(root.Root());
  if (!opened) FAIL(opened.error());
  CHECK(Read(root.Root() / ".cache") == "not a directory");
  CHECK(content::DataStore::Get()
            ->WorkshopRecipes()
            ->Of(WorkshopKind::kBlade)
            .blades[1]
            .stats.strength == 10);
}

TEST_CASE("Layers merge fields by legacy aliases and report precedence",
          "[catalog-layers][workshop]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Layers(root);
  const auto path =
      root.TableFile(WorkshopKind::kBlade, WorkshopTablePart::kResults);
  const auto before = Read(path);
  Patch(root, "patch", WorkshopTablePart::kResults,
        "id,strength\nblade-001,25\n");
  Patch(root, "mod", WorkshopTablePart::kResults,
        "id,strength,damage_type\nblade.battle-knife,37,edged\n");
  const auto database = Load(root);
  const auto recipes = Recipes(database);
  const auto& blade = recipes.Of(WorkshopKind::kBlade).blades.at(1);
  CHECK(blade.id == "blade.battle-knife");
  CHECK(blade.stats.strength == 37);
  CHECK(blade.damage_type == 2);
  REQUIRE(database.Conflicts().size() == 1);
  CHECK(database.Conflicts().front().key == "blade.battle-knife");
  CHECK(Read(path) == before);
  CHECK(Recipes(Load(root)).tables == recipes.tables);
}

TEST_CASE("A layered recipe reaches the production workshop book",
          "[catalog-layers][workshop]") {
  content::DataStore::ScopedOverride restore;
  LiveRecipes live;
  setup::ResetLiveSetupStatics();
  WorkshopRecipeRoot root;
  Prepare(root);
  Layers(root);
  Patch(root, "mod", WorkshopTablePart::kResults,
        "id,strength\nblade.battle-knife,37\n");
  Patch(root, "mod", WorkshopTablePart::kPairs,
        "first,second,result,_op\nblade.scramasax,blade.battle-knife,blade."
        "battle-knife,add\n");
  const auto opened = content::DataStore::Open(root.Root());
  if (!opened) FAIL(opened.error());
  setup::RunLiveSetup([](setup::Context& context) {
    context.host.StartRecipeLoad(setup::RecipeKind::kBlade);
    context.host.ReleaseRecipeRequest();
    return 0;
  });
  const auto* book = setup::LiveSetupRecipes();
  REQUIRE(book != nullptr);
  CHECK(book->blades.at(1).strength == 37);
  const auto side = content::LayoutOf(WorkshopKind::kBlade).pair_side;
  CHECK(book->results.at(2 * side + 1) == 1);
  CHECK(book->results.at(side + 2) == 2);
}

TEST_CASE("Remove and replace require existing rows without modifying the base",
          "[catalog-layers][workshop]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Layers(root);
  const auto path =
      root.TableFile(WorkshopKind::kBlade, WorkshopTablePart::kPairs);
  const auto before = Read(path);
  Patch(root, "patch", WorkshopTablePart::kPairs,
        "first,second,result,_op\nblade-001,blade-002,blade-001,replace\n");
  Patch(root, "mod", WorkshopTablePart::kPairs,
        "first,second,_op\nblade.battle-knife,blade.scramasax,remove\n");
  const auto database = Load(root);
  CHECK(Recipes(database).Of(WorkshopKind::kBlade).pairs.empty());
  CHECK(database.Conflicts().size() == 1);
  CHECK(Read(path) == before);
}

TEST_CASE("Invalid layered definitions report the exact patch location",
          "[catalog-layers][workshop]") {
  const auto text = GENERATE(
      "id,strength\nblade.battle-knife,128\n",
      "id,strenght\nblade.battle-knife,12\n",
      "id,_op\nblade.battle-knife,add\n",
      "id,strength,_op\nblade.new,10,replace\n", "id,strength\nblade.new,10\n",
      "id,strength\nblade.battle-knife,12\nblade-001,13\n");
  WorkshopRecipeRoot root;
  Prepare(root);
  Layers(root);
  Patch(root, "mod", WorkshopTablePart::kResults, text);
  const auto database = content::CatalogDatabase::Load(
      root.Root(), content::WorkshopCatalogSources());
  REQUIRE_FALSE(database.has_value());
  CHECK(database.error().find("combination-blade-results.csv:") !=
        std::string::npos);
}

TEST_CASE("Named and numeric material keys identify the same layer operation",
          "[catalog-layers][workshop]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Layers(root);
  Patch(
      root, "patch", WorkshopTablePart::kMaterials,
      "first_material,first_category,second_material,second_category,material\n"
      "3,1,4,2,3\n");
  Patch(
      root, "mod", WorkshopTablePart::kMaterials,
      "first_material,first_category,second_material,second_category,material\n"
      "bronze,dagger,iron,sword,damascus\n");
  const auto database = Load(root);
  const auto rules = Recipes(database).Of(WorkshopKind::kBlade).materials;
  REQUIRE(rules.size() == 1);
  CHECK(rules.front().material == 7);
  CHECK(database.Conflicts().size() == 1);
}

TEST_CASE("Broken layered references cannot replace the published store",
          "[catalog-layers][workshop]") {
  content::DataStore::ScopedOverride restore;
  WorkshopRecipeRoot root;
  Prepare(root);
  const auto opened = content::DataStore::Open(root.Root());
  if (!opened) FAIL(opened.error());
  const auto* previous = content::DataStore::Get();
  Layers(root);
  Patch(root, "mod", WorkshopTablePart::kPairs,
        "first,second,result\nblade.battle-knife,blade.scramasax,blade."
        "missing\n");
  const auto refused = content::DataStore::Open(root.Root());
  REQUIRE_FALSE(refused.has_value());
  CHECK(
      refused.error().find("combination-blade-pairs.csv:2: column 'result'") !=
      std::string::npos);
  CHECK(content::DataStore::Get() == previous);
  CHECK(previous->WorkshopRecipes()
            ->Of(WorkshopKind::kBlade)
            .blades.at(1)
            .stats.strength == 10);
}

TEST_CASE("Layer roots reject missing and repeated directories",
          "[catalog-layers]") {
  const auto layers =
      GENERATE(R"(["absent"])", R"(["patch","patch"])", R"(["."])");
  WorkshopRecipeRoot root;
  Prepare(root);
  fs::create_directories(root.Root() / "patch");
  Write(root.Root() / "catalog-layers.json",
        std::string(
            R"({"schema":"vs.catalog-layers","schema_version":1,"layers":)") +
            layers + "}");
  const auto database = content::CatalogDatabase::Load(
      root.Root(), content::WorkshopCatalogSources());
  REQUIRE_FALSE(database.has_value());
  CHECK(database.error().find("catalog-layers.json") != std::string::npos);
}

TEST_CASE("Unsupported layer tables fail rather than silently losing edits",
          "[catalog-layers]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  Layers(root);
  Write(root.Root() / "mod" / "catalogs" / "items" / "unknown.csv",
        "id,strength\nblade.battle-knife,25\n");
  const auto database = content::CatalogDatabase::Load(
      root.Root(), content::WorkshopCatalogSources());
  REQUIRE_FALSE(database.has_value());
  CHECK(database.error().find("unsupported layered catalog table") !=
        std::string::npos);
}

TEST_CASE(
    "Layer paths support spaces and Unicode independently of working directory",
    "[catalog-layers]") {
  WorkshopRecipeRoot root;
  Prepare(root);
  const fs::path layer = root.Root() / fs::path(L"patch \u00e9");
  fs::create_directories(layer);
  const content::JsonDocument config{
      {"schema", "vs.catalog-layers"},
      {"schema_version", 1},
      {"layers", {VS::Platform::PathText(layer.filename())}}};
  Write(root.Root() / "catalog-layers.json", content::FormatJsonText(config));
  Write(layer / content::WorkshopTablePath(WorkshopKind::kBlade,
                                           WorkshopTablePart::kResults),
        "id,strength\nblade.battle-knife,25\n");
  const auto recipes = Recipes(Load(root));
  CHECK(recipes.Of(WorkshopKind::kBlade).blades.at(1).stats.strength == 25);
}
