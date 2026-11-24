#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/CatalogDatabase.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
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

struct LiveRecipes {
  ~LiveRecipes() { setup::ResetLiveSetupStatics(); }
};
}  // namespace

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
