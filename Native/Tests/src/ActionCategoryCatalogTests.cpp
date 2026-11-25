#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionCategoryCatalogLive.hpp"
#include "Content/ActionCategoryCatalog.hpp"
#include "Content/DataStore.hpp"
#include "Extract/ActionCategoryConversion.hpp"
#include "Extract/ConverterBatch.hpp"
#include "TestSupport/ActionCategoryRoot.hpp"

namespace {
namespace content = VS::Content;
namespace extract = VS::Extract;
namespace battle = VS::Battle;

constexpr std::size_t kDirectActor = 3;
constexpr std::size_t kTreasureChest = 10;
constexpr std::size_t kBasicAttackCategory = 14;
constexpr std::uint8_t kUnnamedKindBit = 0x40;

std::string ReadText(const std::filesystem::path& file) {
  std::ifstream stream(file, std::ios::binary);
  REQUIRE(stream.good());
  return {std::istreambuf_iterator<char>(stream), {}};
}

const content::ActionCategoryRule& LiveRule(std::size_t category) {
  return battle::RequireLiveActionCategory(static_cast<int>(category));
}

extract::ActionCategorySourceRecords Original() {
  const auto rules = VS::test_support::OriginalActionCategories().rules;
  extract::ActionCategorySourceRecords source{};
  for (std::size_t index = 0; index < source.size(); ++index) {
    source[index] = {
        static_cast<std::uint8_t>(rules[index].needs_target_selection),
        static_cast<std::uint8_t>(rules[index].area_target),
        static_cast<std::uint8_t>(rules[index].includes_acting_actor),
        static_cast<std::uint8_t>(rules[index].targets_body_parts),
        static_cast<std::uint8_t>(rules[index].target_kinds)};
  }
  return source;
}

void Source(VS::test_support::ActionCategoryRoot& root,
            std::string_view prefix) {
  const auto records = extract::ReconstructActionCategorySource(Original());
  std::string bytes(extract::kActionCategorySourceOffset, '\0');
  for (const auto byte : records) bytes += static_cast<char>(byte);
  root.Write(std::string(prefix) + "/disc/BATTLE/BATTLE.PRG", bytes);
  root.Write(std::string(prefix) + "/files.tsv",
             "lba\tsize\tform\tpath\n24\t" + std::to_string(bytes.size()) +
                 "\traw\tBATTLE/BATTLE.PRG\n");
}

// The category tables of the configured extracted BATTLE.PRG; empty when the
// disc is not extracted.
std::vector<std::byte> ConfiguredSection() {
  const auto path =
      std::filesystem::path("Native\\data") / "disc" / "BATTLE" / "BATTLE.PRG";
  if (!std::filesystem::is_regular_file(path)) return {};
  const auto bytes = extract::ReadFileBytes(path);
  if (!bytes || bytes->size() < extract::kActionCategorySourceOffset +
                                    extract::kActionCategorySourceBytes) {
    FAIL("configured BATTLE.PRG is shorter than the category tables");
  }
  const auto section =
      std::span(*bytes).subspan(extract::kActionCategorySourceOffset,
                                extract::kActionCategorySourceBytes);
  return {section.begin(), section.end()};
}
}  // namespace

TEST_CASE(
    "Action category source reloads offline and refuses unauthorable bytes",
    "[action-category-catalog]") {
  const auto source = Original();
  const auto bytes = extract::ReconstructActionCategorySource(source);
  CHECK(extract::DecodeActionCategorySource(bytes) == source);
  const auto text =
      extract::FormatActionCategoryProvenance(source, std::string(64, 'a'));
  const auto restored =
      extract::ParseActionCategoryProvenance(std::as_bytes(std::span(text)));
  REQUIRE(restored);
  if (!restored) return;
  CHECK(extract::ReconstructActionCategorySource(*restored) == bytes);
  CHECK(extract::DecodeActionCategories(bytes));

  auto flag = source;
  flag[kDirectActor].area_target = 2;
  CHECK_FALSE(extract::DecodeActionCategories(
      extract::ReconstructActionCategorySource(flag)));
  auto mask = source;
  mask[kDirectActor].target_actor_mask |= kUnnamedKindBit;
  CHECK_FALSE(extract::DecodeActionCategories(
      extract::ReconstructActionCategorySource(mask)));
  auto gap = bytes;
  gap[2 * content::kActionCategoryCount] = std::byte{1};
  CHECK_FALSE(extract::DecodeActionCategorySource(gap));
}

TEST_CASE(
    "Action category import names each rule from the original tables and "
    "the catalog leads with a plain-text name",
    "[action-category-catalog][content-tables]") {
  const auto imported = extract::DecodeActionCategories(
      extract::ReconstructActionCategorySource(Original()));
  REQUIRE(imported);
  if (!imported) return;
  CHECK(imported->definitions == VS::test_support::OriginalActionCategories());
  const auto text = content::FormatActionCategories(imported->definitions);
  CHECK(text.starts_with(
      "id,display_name,selection,extent,includes_user,aim,target_kinds\n"
      "action-category.environment,environment,automatic,single,no,actor,"
      "none\n"
      "action-category.direct-actor-including-user,direct actor including "
      "user,chosen,single,yes,actor,leader|neutral|hostile\n"));
  CHECK(
      text.contains("action-category.treasure-chest,treasure chest,chosen,"
                    "single,no,actor,chest\n"));
  CHECK(
      text.contains("action-category.direct-limb-or-object,direct limb or "
                    "object,chosen,single,no,limb,"
                    "leader|neutral|hostile|object\n"));
  CHECK(content::ParseActionCategoryCatalog(text) == imported->definitions);
}

TEST_CASE("Committed action category schema matches the authoring contract",
          "[action-category-catalog][content-tables]") {
  auto schema = ReadText(
      std::filesystem::path("Native\\Content\\schemas\\catalogs\\battle\\"
                            "action-categories.schema.json"));
  std::erase(schema, '\r');
  CHECK(schema == content::FormatTableSchema(content::ActionCategoriesSpec()));
}

TEST_CASE(
    "Action category schema refuses unknown duplicate and numeric spellings",
    "[action-category-catalog]") {
  const auto text = content::FormatActionCategories(
      VS::test_support::OriginalActionCategories());
  const auto replaced = [&](std::string_view from, std::string_view to) {
    auto changed = text;
    const auto at = changed.find(from);
    REQUIRE(at != std::string::npos);
    changed.replace(at, from.size(), to);
    return changed;
  };
  CHECK(content::ParseActionCategoryCatalog(text));
  CHECK_FALSE(content::ParseActionCategoryCatalog(
      replaced("action-category.direct-actor,", "action-category.imaginary,")));
  CHECK_FALSE(content::ParseActionCategoryCatalog(
      replaced("action-category.trap,", "action-category.cloudstone,")));
  CHECK_FALSE(
      content::ParseActionCategoryCatalog(replaced(",chest\n", ",bit7\n")));
  CHECK_FALSE(content::ParseActionCategoryCatalog(
      replaced(",chest\n", ",chest|chest\n")));
  CHECK_FALSE(content::ParseActionCategoryCatalog(replaced(
      "chosen,single,no,actor,chest", "chosen,multiple,no,actor,chest")));
  CHECK_FALSE(content::ParseActionCategoryCatalog(
      text.substr(0, text.rfind("action-category.unused"))));
}

TEST_CASE("Action category layer reaches live consumers and a missing catalog",
          "[action-category-catalog][live]") {
  VS::test_support::ActionCategoryRoot root;
  const auto& basic = LiveRule(kBasicAttackCategory);
  CHECK(basic.needs_target_selection);
  CHECK(basic.targets_body_parts);
  CHECK(basic.target_kinds == 0x17);
  CHECK_FALSE(LiveRule(kDirectActor).area_target);
  root.Write(
      "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mods/area"]})");
  root.Write("mods/area/catalogs/battle/action-categories.csv",
             "id,extent,target_kinds\n"
             "action-category.direct-actor,area,leader|hostile\n");
  root.Open();
  const auto& changed = LiveRule(kDirectActor);
  CHECK(changed.area_target);
  CHECK(changed.target_kinds == 0x05);
  CHECK(LiveRule(kTreasureChest).target_kinds == 0x80);
  CHECK(battle::LiveActionCategoryCatalog()->Rule(16) == nullptr);
  content::DataStore::ScopedOverride absent;
  CHECK(battle::LiveActionCategoryCatalog() == nullptr);
}

TEST_CASE(
    "Action category converter preserves ownership edits and repairs missing "
    "schemas",
    "[action-category-catalog]") {
  VS::test_support::ActionCategoryRoot root;
  Source(root, "source");
  const auto data = root.Root() / "source";
  REQUIRE(extract::ConvertDiscActionCategories(data, false));
  const auto csv = data / content::kActionCategoriesPath;
  const auto text = ReadText(csv);
  const auto stamp = std::filesystem::last_write_time(csv);
  std::filesystem::remove(data /
                          "catalogs/battle/action-categories.schema.json");
  REQUIRE(extract::ConvertDiscActionCategories(data, false));
  CHECK(ReadText(csv) == text);
  CHECK(std::filesystem::last_write_time(csv) == stamp);
  const auto repeat = extract::ConvertDiscActionCategories(data, false);
  REQUIRE(repeat);
  if (repeat) CHECK(repeat->unchanged == 1);
  Source(root, "foreign");
  root.Write("foreign/catalogs/battle/action-categories.csv", "not owned");
  CHECK_FALSE(
      extract::ConvertDiscActionCategories(root.Root() / "foreign", true));
}

TEST_CASE(
    "Action category converter refuses stale sources without replacing "
    "authored data",
    "[action-category-catalog]") {
  VS::test_support::ActionCategoryRoot root;
  Source(root, "source");
  const auto data = root.Root() / "source";
  REQUIRE(extract::ConvertDiscActionCategories(data, false));
  const auto csv = data / content::kActionCategoriesPath;
  const auto original = ReadText(csv);
  auto source = ReadText(data / "disc/BATTLE/BATTLE.PRG");
  source.front() = 'x';
  root.Write("source/disc/BATTLE/BATTLE.PRG", source);
  CHECK_FALSE(extract::ConvertDiscActionCategories(data, false));
  CHECK(ReadText(csv) == original);
}

TEST_CASE(
    "Action category configured disc and persisted provenance reload all 16 "
    "categories exactly",
    "[action-category-catalog][content-tables]") {
  const auto section = ConfiguredSection();
  if (section.empty()) SKIP("configured extracted BATTLE.PRG is unavailable");
  const auto imported = extract::DecodeActionCategories(section);
  REQUIRE(imported);
  if (!imported) return;
  CHECK(std::ranges::equal(
      extract::ReconstructActionCategorySource(imported->originals), section));
  CHECK(imported->definitions == VS::test_support::OriginalActionCategories());
  const auto provenance =
      extract::ReadFileBytes(std::filesystem::path("Native\\data") /
                             extract::kActionCategoryImportPath);
  REQUIRE(provenance);
  if (!provenance) return;
  const auto restored = extract::ParseActionCategoryProvenance(*provenance);
  REQUIRE(restored);
  if (!restored) return;
  CHECK(std::ranges::equal(extract::ReconstructActionCategorySource(*restored),
                           section));
}
