#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/CatalogTable.hpp"
#include "Content/DataStore.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "Extract/WepModelConversion.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"
#include "TestSupport/AudioConversionFixture.hpp"
#include "TestSupport/WorkshopRecipeRoot.hpp"

namespace {

namespace fs = std::filesystem;
namespace content = VS::Content;
namespace extract = VS::Extract;
using content::WorkshopKind;
using content::WorkshopTablePart;
using VS::test_support::WorkshopRecipeRoot;
using VS::test_support::WorkshopResultId;

constexpr std::size_t kRecordTableWord = 8;  // the header's third offset
constexpr std::size_t kBladeShapeByte = 15;
constexpr std::uint8_t kSampleMaterial = 7;

// `kind`'s full table with every field of record 1 set, two pairs at the pair
// matrix's corners and one material rule at the material matrix's last entry.
content::WorkshopRecipeTable SampleTable(WorkshopKind kind) {
  content::WorkshopRecipeTable table =
      VS::test_support::FullWorkshopTable(kind);
  const content::WorkshopLayout& layout = content::LayoutOf(kind);
  const std::uint32_t first = layout.first_pair_id;
  const auto last = static_cast<std::uint32_t>(first + layout.pair_side - 1);
  table.pairs = {{.first = WorkshopResultId(kind, first),
                  .second = WorkshopResultId(kind, last),
                  .result = WorkshopResultId(kind, last)},
                 {.first = WorkshopResultId(kind, last),
                  .second = WorkshopResultId(kind, first),
                  .result = WorkshopResultId(kind, 1)}};
  const auto last_material = static_cast<std::uint8_t>(
      layout.material_base + layout.material_count - 1);
  const auto last_category = static_cast<std::uint8_t>(
      layout.by_category ? layout.category_base + layout.category_count - 1
                         : 0);
  table.materials = {{.first_material = last_material,
                      .first_category = last_category,
                      .second_material = last_material,
                      .second_category = last_category,
                      .material = kSampleMaterial}};
  const content::WorkshopStats stats{
      .strength = -128, .intelligence = 127, .agility = -1};
  if (kind == WorkshopKind::kBlade) {
    content::BladeResult& blade = table.blades.at(1);
    blade.model = 35;
    blade.category = 9;
    blade.damage_type = 3;
    blade.cost_type = 7;
    blade.cost = 255;
    blade.recovery_delay = 200;
    blade.stats = stats;
    blade.range = 10;
    blade.range_y = 11;
    blade.range_z = 12;
    blade.attack_shape = 5;
    blade.attack_angle = 18;
    blade.raw_byte7 = 0xAB;
    blade.raw_byte11 = 0xCD;
  } else {
    content::PieceResult& piece = table.pieces.at(1);
    piece.model = 9;
    piece.category = 7;
    piece.gem_slots = 3;
    piece.stats = stats;
    piece.raw_byte7 = 0xEF;
  }
  return table;
}

std::size_t WordAt(const std::vector<std::byte>& bytes, std::size_t offset) {
  std::size_t value = 0;
  for (std::size_t index = 0; index < 4; ++index) {
    value |= std::to_integer<std::size_t>(bytes.at(offset + index))
             << (index * 8);
  }
  return value;
}

std::string ReadText(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void WriteText(const fs::path& path, std::string_view text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  REQUIRE(output.good());
}

// `text` with its only `from` replaced by `to`.
std::string Replaced(std::string text, std::string_view from,
                     std::string_view to) {
  const std::size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  REQUIRE(text.find(from, at + 1) == std::string::npos);
  return text.replace(at, from.size(), to);
}

// The first ancestor of the working directory holding `relative`.
std::optional<fs::path> RepositoryPath(const fs::path& relative) {
  for (auto directory = fs::current_path();
       directory.has_parent_path() && directory != directory.parent_path();
       directory = directory.parent_path()) {
    if (fs::exists(directory / relative)) return directory / relative;
  }
  return std::nullopt;
}

// A table edit and the error it must give.
struct TableEdit {
  WorkshopKind kind;
  WorkshopTablePart part;
  std::string_view from;  // the text replaced; empty replaces the whole table
  std::string_view to;
  std::string_view error;
};

constexpr std::array kTableEdits{
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kResults,
              .from = "shield-002,2,0",
              .to = "shield-002,2,x",
              .error = "catalogs/menu/combination-shield-results.csv:4: column "
                       "'model': 'x' is not a decimal integer"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kResults,
              .from = "shield-002,2,0,0,0",
              .to = "shield-002,2,0,0,4",
              .error = "catalogs/menu/combination-shield-results.csv:4: column "
                       "'gem_slots': 4 is outside 0..3"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kResults,
              .from = "shield-002,",
              .to = "Shield-002,",
              .error = "catalogs/menu/combination-shield-results.csv:4: column "
                       "'id': 'Shield-002' is not an id"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kResults,
              .from = "shield-002,2,",
              .to = "shield-002,30,",
              .error = "catalogs/menu/combination-shield-results.csv: no row "
                       "has legacy_id 2; legacy ids run from 0 without gaps"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kResults,
              .from = "shield-002,2,",
              .to = "shield-002,1,",
              .error = "column 'legacy_id': 1 repeats line"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kResults,
              .from = "id,legacy_id",
              .to = "id,legacy",
              .error = "catalogs/menu/combination-shield-results.csv:1: "
                       "unknown column 'legacy'"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kPairs,
              .from = {},
              .to = "first,second\nshield-000,shield-001\n",
              .error = "catalogs/menu/combination-shield-pairs.csv:1: missing "
                       "column 'result'"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kPairs,
              .from = {},
              .to = "first,second,result\nshield-000,shield-001\n",
              .error = "catalogs/menu/combination-shield-pairs.csv:2: has 2 "
                       "fields; the header has 3"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kPairs,
              .from = {},
              .to = "first,second,result\n\"shield-000,shield-001\n",
              .error = "catalogs/menu/combination-shield-pairs.csv:2: a quoted "
                       "field is not closed"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kPairs,
              .from = {},
              .to = "first,second,result\nshield-000,shield-001,shield-099\n",
              .error = "catalogs/menu/combination-shield-pairs.csv:2: column "
                       "'result': 'shield-099' names no row of "
                       "combination-shield-results"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kPairs,
              .from = {},
              .to = "first,second,result\nshield-000,shield-001,shield-000\n",
              .error = "catalogs/menu/combination-shield-pairs.csv:2: column "
                       "'result': 'shield-000' has legacy_id 0, which reads as "
                       "no combination"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kPairs,
              .from = {},
              .to = "first,second,result\nshield-000,shield-001,shield-002\n"
                    "shield-000,shield-001,shield-003\n",
              .error = "catalogs/menu/combination-shield-pairs.csv:3: repeats "
                       "the key (first=shield-000, second=shield-001) of line "
                       "2"},
    TableEdit{.kind = WorkshopKind::kArmor,
              .part = WorkshopTablePart::kPairs,
              .from = {},
              .to = "first,second,result\narmor-003,armor-016,armor-017\n",
              .error = "catalogs/menu/combination-armor-pairs.csv:2: column "
                       "'first': 'armor-003' (legacy_id 3) is outside the "
                       "ingredients' legacy ids 16..80"},
    TableEdit{.kind = WorkshopKind::kShield,
              .part = WorkshopTablePart::kMaterials,
              .from = {},
              .to = "first_material,second_material,material\n0,7,8\n",
              .error = "catalogs/menu/combination-shield-materials.csv:2: "
                       "column 'material': 8 is outside 1..7"},
    TableEdit{.kind = WorkshopKind::kArmor,
              .part = WorkshopTablePart::kMaterials,
              .from = {},
              .to = "first_material,first_category,second_material,"
                    "second_category,material\n0,1,0,2,3\n",
              .error = "catalogs/menu/combination-armor-materials.csv:2: "
                       "column 'first_category': 1 is outside 2..5"},
};

// The shield results table of a converted data root, as generated and as
// edited (result 2's model set to 42).
struct EditedResults {
  fs::path path;
  std::string generated;
  std::string edited;
};

// Converts SampleTable's recipe files (and the images and models, so every
// required converter has run) under `root`, then edits the shield results
// table.
EditedResults ConvertAndEditShieldResults(WorkshopRecipeRoot& root) {
  root.Table(WorkshopKind::kShield) = SampleTable(WorkshopKind::kShield);
  root.WriteSources();
  CHECK(root.Convert(false).converted == 3);
  REQUIRE(extract::ConvertDiscImages(root.Root(), false).has_value());
  REQUIRE(extract::ConvertDiscModels(root.Root(), false).has_value());
  REQUIRE(extract::ConvertDiscWeapons(root.Root(), false).has_value());
  VS::test_support::RecordAudioConversions(root.Root());
  EditedResults results{.path = root.TableFile(WorkshopKind::kShield,
                                               WorkshopTablePart::kResults),
                        .generated = {},
                        .edited = {}};
  results.generated = ReadText(results.path);
  results.edited =
      Replaced(results.generated, "shield-002,2,0,", "shield-002,2,42,");
  WriteText(results.path, results.edited);
  return results;
}

// Checks the schemas written under `root` are the converter's.
void CheckPublishedSchemas(const fs::path& root) {
  for (const WorkshopKind kind : content::kWorkshopKinds) {
    for (const WorkshopTablePart part : content::kWorkshopTableParts) {
      CHECK(ReadText(root / content::WorkshopSchemaPath(kind, part)) ==
            content::FormatTableSchema(content::WorkshopTableSpec(kind, part)));
    }
  }
}

// Checks a store opened over `root` loads the shield tables with result 2's
// model edited to 42.
void CheckStoreReadsEdit(const fs::path& root) {
  content::DataStore::ScopedOverride restore;
  const auto opened = content::DataStore::Open(root);
  INFO((opened ? std::string{} : opened.error()));
  REQUIRE(opened.has_value());
  const content::DataStore* store = content::DataStore::Get();
  REQUIRE(store != nullptr);
  CHECK(store->RequireConversions().has_value());
  const content::WorkshopRecipeCatalog* recipes = store->WorkshopRecipes();
  REQUIRE(recipes != nullptr);
  CHECK(recipes->Of(WorkshopKind::kShield).pieces.at(2).model == 42);
  CHECK(recipes->Of(WorkshopKind::kShield).pairs ==
        SampleTable(WorkshopKind::kShield).pairs);
}

// Why a store opened over `root` refuses its conversions (empty if it does
// not).
std::string ConversionRefusal(const fs::path& root) {
  content::DataStore::ScopedOverride restore;
  const auto opened = content::DataStore::Open(root);
  INFO((opened ? std::string{} : opened.error()));
  REQUIRE(opened.has_value());
  const auto required = content::DataStore::Get()->RequireConversions();
  return required ? std::string{} : required.error();
}

}  // namespace

TEST_CASE("Workshop recipe tables round-trip through SYD bytes and CSV text",
          "[content-tables][workshop]") {
  const WorkshopKind kind = GENERATE(
      WorkshopKind::kBlade, WorkshopKind::kShield, WorkshopKind::kArmor);
  const content::WorkshopLayout& layout = content::LayoutOf(kind);
  CAPTURE(layout.name);
  const content::WorkshopRecipeTable table = SampleTable(kind);

  const auto bytes = extract::EncodeSydRecipes(table);
  REQUIRE(bytes.has_value());
  // The original files' sizes: the layouts cover every byte.
  const std::size_t size = kind == WorkshopKind::kBlade    ? 13264
                           : kind == WorkshopKind::kShield ? 504
                                                           : 5912;
  CHECK(bytes->size() == size);
  const std::size_t record =
      WordAt(*bytes, kRecordTableWord) + layout.record_bytes;  // record 1
  CHECK(std::to_integer<int>(bytes->at(record)) == 1);
  if (kind == WorkshopKind::kBlade) {
    // shape 5 in bits 0-2, angle 18 in bits 3-7
    CHECK(std::to_integer<int>(bytes->at(record + kBladeShapeByte)) ==
          ((18 << 3) | 5));
  }
  const auto decoded = extract::DecodeSydRecipes(kind, *bytes);
  REQUIRE(decoded.has_value());
  CHECK(*decoded == table);

  const content::WorkshopTexts texts = content::FormatWorkshopTables(table);
  const auto parsed = content::ParseWorkshopTables(kind, texts);
  INFO((parsed ? std::string{} : parsed.error()));
  REQUIRE(parsed.has_value());
  CHECK(*parsed == table);
}

TEST_CASE("Workshop recipe tables read rows and columns in any order",
          "[content-tables][workshop]") {
  const content::WorkshopRecipeTable table = SampleTable(WorkshopKind::kShield);
  content::WorkshopTexts texts = content::FormatWorkshopTables(table);
  // Swap the last two result rows and the pairs' columns; add a BOM, CRLF
  // line ends and a blank line.
  std::string& results = texts[0];
  results = Replaced(results,
                     "shield-015,15,0,0,0,0,0,0,0\n"
                     "shield-016,16,0,0,0,0,0,0,0\n",
                     "shield-016,16,0,0,0,0,0,0,0\n"
                     "shield-015,15,0,0,0,0,0,0,0\n");
  texts[1] =
      "\xEF\xBB\xBF"
      "result,first,second\r\n"
      "shield-016,shield-000,shield-016\r\n\r\n"
      "shield-001,shield-016,shield-000\r\n";
  const auto parsed =
      content::ParseWorkshopTables(WorkshopKind::kShield, texts);
  INFO((parsed ? std::string{} : parsed.error()));
  REQUIRE(parsed.has_value());
  CHECK(*parsed == table);
}

TEST_CASE("Workshop recipe tables report the file, line and column of errors",
          "[content-tables][workshop]") {
  for (const TableEdit& edit : kTableEdits) {
    CAPTURE(edit.error);
    content::WorkshopTexts texts =
        content::FormatWorkshopTables(SampleTable(edit.kind));
    std::string& text = texts.at(static_cast<std::size_t>(edit.part));
    text = edit.from.empty() ? std::string(edit.to)
                             : Replaced(text, edit.from, edit.to);
    const auto parsed = content::ParseWorkshopTables(edit.kind, texts);
    REQUIRE_FALSE(parsed.has_value());
    INFO(parsed.error());
    CHECK(parsed.error().find(edit.error) != std::string::npos);
  }
}

TEST_CASE("SYD decoding refuses files outside the recipe layout",
          "[content-tables][workshop]") {
  auto encoded = extract::EncodeSydRecipes(SampleTable(WorkshopKind::kShield));
  REQUIRE(encoded.has_value());
  std::vector<std::byte> bytes = *encoded;
  std::string error;
  // SHIELD.SYD: pairs at 12 (17 x 17), padding 301-303, materials at 304 (64)
  // and records at 368 (8 bytes each).
  SECTION("a short file") {
    bytes.pop_back();
    error = "MENU/SHIELD.SYD: has 503 bytes; its layout takes 504";
  }
  SECTION("a moved table") {
    bytes[4] = std::byte{0x31};
    error = "MENU/SHIELD.SYD: table 1 starts at 305; its layout puts it at 304";
  }
  SECTION("nonzero padding") {
    bytes[302] = std::byte{1};
    error = "MENU/SHIELD.SYD: padding byte 302 is 1, not 0";
  }
  SECTION("a record out of order") {
    bytes[368 + 16] = std::byte{9};
    error = "MENU/SHIELD.SYD: record 2 starts with 9";
  }
  SECTION("a pair naming no record") {
    bytes[12] = std::byte{17};
    error = "MENU/SHIELD.SYD: pair (0, 0) names record 17 of 17";
  }
  SECTION("a material past damascus") {
    bytes[304] = std::byte{8};
    error = "MENU/SHIELD.SYD: material entry 0 is 8, past 7";
  }
  const auto decoded = extract::DecodeSydRecipes(WorkshopKind::kShield, bytes);
  REQUIRE_FALSE(decoded.has_value());
  CHECK(decoded.error() == error);
}

TEST_CASE("Converted workshop tables keep edits across conversions",
          "[content-tables][workshop]") {
  WorkshopRecipeRoot root;
  const EditedResults results = ConvertAndEditShieldResults(root);
  CheckPublishedSchemas(root.Root());
  const auto kept = root.Convert(false);
  CHECK(kept.converted == 0);
  CHECK(kept.unchanged == 3);
  CHECK(ReadText(results.path) == results.edited);
  CheckStoreReadsEdit(root.Root());
}

TEST_CASE("A bad workshop table edit stops the data store opening",
          "[content-tables][workshop]") {
  WorkshopRecipeRoot root;
  const EditedResults results = ConvertAndEditShieldResults(root);
  WriteText(results.path,
            Replaced(results.edited, "shield-002,2,42,", "shield-002,2,x,"));
  content::DataStore::ScopedOverride restore;
  const auto opened = content::DataStore::Open(root.Root());
  REQUIRE_FALSE(opened.has_value());
  CHECK(opened.error().find("catalogs/menu/combination-shield-results.csv:4: "
                            "column 'model': 'x' is not a decimal integer") !=
        std::string::npos);
  CHECK(opened.error().find("re-run VS.Extract tables") != std::string::npos);
}

TEST_CASE("A forced table conversion replaces edited workshop tables",
          "[content-tables][workshop]") {
  WorkshopRecipeRoot root;
  const EditedResults results = ConvertAndEditShieldResults(root);
  CHECK(root.Convert(true).converted == 3);
  CHECK(ReadText(results.path) == results.generated);
}

TEST_CASE("A changed recipe file is stale until its tables convert again",
          "[content-tables][workshop]") {
  WorkshopRecipeRoot root;
  const EditedResults results = ConvertAndEditShieldResults(root);
  root.Table(WorkshopKind::kShield).pieces.at(3).model = 77;
  root.WriteSources();
  CHECK(ConversionRefusal(root.Root())
            .find("stale syd-recipe-csv source hash for MENU/SHIELD.SYD") !=
        std::string::npos);
  const auto changed = root.Convert(false);
  CHECK(changed.converted == 1);
  CHECK(changed.unchanged == 2);
  CHECK(ReadText(results.path) ==
        Replaced(results.generated, "shield-003,3,0,", "shield-003,3,77,"));
}

TEST_CASE("Committed workshop table schemas match the converter's",
          "[content-tables][workshop]") {
  const auto schemas = RepositoryPath(fs::path("Native") / "Content" /
                                      "schemas" / "catalogs" / "menu");
  REQUIRE(schemas.has_value());
  for (const WorkshopKind kind : content::kWorkshopKinds) {
    for (const WorkshopTablePart part : content::kWorkshopTableParts) {
      const content::TableSpec& spec = content::WorkshopTableSpec(kind, part);
      CAPTURE(spec.name);
      std::string committed =
          ReadText(*schemas / (std::string(spec.name) +
                               std::string(content::kTableSchemaSuffix)));
      std::erase(committed, '\r');
      CHECK(committed == content::FormatTableSchema(spec));
    }
  }
}

TEST_CASE("Extracted workshop recipe files convert to tables exactly",
          "[content-tables][workshop][extract]") {
  const auto data =
      RepositoryPath(fs::path("Native") / "data" / "disc" / "MENU");
  if (!data) SKIP("Extracted Native/data is unavailable");
  for (const WorkshopKind kind : content::kWorkshopKinds) {
    const content::WorkshopLayout& layout = content::LayoutOf(kind);
    CAPTURE(layout.name);
    auto bytes =
        extract::ReadFileBytes(*data / fs::path(layout.source).filename());
    REQUIRE(bytes.has_value());
    // ConvertSydRecipes requires the tables to read back to the decoded
    // recipes and those to rebuild the source bytes.
    const auto converted = extract::ConvertSydRecipes(
        {.path = std::string(layout.source), .bytes = *std::move(bytes)});
    INFO((converted ? std::string{} : converted.error()));
    REQUIRE(converted.has_value());
    CHECK(converted->files.size() == 2 * content::kWorkshopTablePartCount);
  }
}
