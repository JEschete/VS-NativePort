#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionTable.hpp"
#include "Content/ActionCatalog.hpp"
#include "Content/DataStore.hpp"
#include "Content/LearningCatalog.hpp"
#include "Content/ModuleData.hpp"
#include "Extract/ActionConversion.hpp"
#include "Extract/ConverterBatch.hpp"
#include "TestSupport/ActionRoot.hpp"

namespace {
namespace content = VS::Content;
namespace extract = VS::Extract;
namespace battle = VS::Battle;
namespace support = VS::test_support;

constexpr std::size_t kRecord = extract::kActionRecordBytes;
constexpr std::size_t kHandBuilt = 9;
constexpr std::size_t kFirstWithHit = 1;
// Byte offsets inside a source record (G01904).
constexpr std::size_t kIdAt = 0;
constexpr std::size_t kPackedAt = 2;
constexpr std::size_t kFlagsAt = 13;
constexpr std::size_t kAiAt = 14;
constexpr std::size_t kReservedAt = 16;
constexpr std::size_t kHitAt = 20;
constexpr std::size_t kNameAt = 28;
constexpr std::byte kTerminator{0xE7};

std::string ReadText(const std::filesystem::path& file) {
  std::ifstream stream(file, std::ios::binary);
  REQUIRE(stream.good());
  return {std::istreambuf_iterator<char>(stream), {}};
}

std::vector<std::byte> SyntheticSection() {
  const auto bytes =
      extract::ReconstructActionSource(support::SyntheticActions());
  REQUIRE(bytes);
  return bytes.value_or(std::vector<std::byte>{});
}

void Source(support::ActionRoot& root, std::string_view prefix) {
  const auto bytes = support::SyntheticActionExecutable();
  root.Write(std::string(prefix) + "/disc/SLUS_010.40", bytes);
  root.Write(std::string(prefix) + "/files.tsv",
             "lba\tsize\tform\tpath\n24\t" + std::to_string(bytes.size()) +
                 "\traw\tSLUS_010.40\n");
}

// The action table of the configured extracted executable; empty when the
// executable is not extracted.
std::vector<std::byte> ConfiguredSection() {
  const auto path =
      std::filesystem::path("Native\\data") / "disc" / content::kActionSource;
  if (!std::filesystem::is_regular_file(path)) return {};
  const auto bytes = extract::ReadFileBytes(path);
  if (!bytes || bytes->size() < extract::kActionSourceOffset +
                                    extract::kActionSourceBytes) {
    FAIL("configured executable is shorter than the action table");
  }
  const auto section = std::span(*bytes).subspan(extract::kActionSourceOffset,
                                                 extract::kActionSourceBytes);
  return {section.begin(), section.end()};
}

// Restores the shared table, which every live battle test reads.
struct TableScope {
  battle::ActionDefinitionTable saved = battle::actionDefinitions;
  TableScope() = default;
  TableScope(const TableScope&) = delete;
  TableScope& operator=(const TableScope&) = delete;
  ~TableScope() { battle::actionDefinitions = saved; }
};

// The record as the original lays it out, minus the bits the catalog drops:
// the reserved flag bits and word, the unread AI bits and the name's tail.
std::vector<std::byte> MaskedRecord(std::span<const std::byte> record) {
  std::vector<std::byte> masked(record.begin(), record.end());
  masked[kFlagsAt] &= std::byte{0xF0};
  masked[kAiAt] &= std::byte{0xF0};
  std::fill_n(masked.begin() + kReservedAt, 4, std::byte{0});
  const auto name = masked.begin() + kNameAt;
  const auto end = std::find(name, masked.end(), kTerminator);
  if (end != masked.end()) std::fill(end + 1, masked.end(), std::byte{0});
  return masked;
}

std::vector<std::byte> TableBytes() {
  std::vector<std::byte> bytes(sizeof battle::actionDefinitions);
  std::memcpy(bytes.data(), battle::actionDefinitions.data(), bytes.size());
  return bytes;
}

// How many records of the live table differ from `source`'s, ignoring the
// bits the catalog drops.
std::size_t DifferingRecords(std::span<const std::byte> source) {
  const auto table = TableBytes();
  std::size_t different = 0;
  for (std::size_t action = 0; action < content::kActionCount; ++action) {
    const auto masked = MaskedRecord(source.subspan(action * kRecord, kRecord));
    if (!std::equal(
            masked.begin(), masked.end(),
            table.begin() + static_cast<std::ptrdiff_t>(action * kRecord))) {
      ++different;
    }
  }
  return different;
}

// The synthetic source with the hand-built record's fields set as the tests
// describe, decoded.
std::optional<extract::ImportedActions> DecodeHandBuilt() {
  auto bytes = SyntheticSection();
  const auto record = std::span(bytes).subspan(kHandBuilt * kRecord, kRecord);
  // Menu action, break art, category 14.
  record[kPackedAt] = std::byte{0x01 | (3 << 1) | (14 << 4)};
  // Reserved bits 0xA, camera zoom 2, hits first obstruction.
  record[kFlagsAt] = std::byte{0x0A | (2 << 4) | 0x40};
  record[kAiAt] = std::byte{0x23};
  record[kAiAt + 1] = std::byte{0x81};
  const std::uint32_t hit = 42 | (9 << 7) | (3 << 13) | (2 << 16) | (7 << 22) |
                            (2U << 27) | (3U << 29);
  for (std::size_t byte = 0; byte < 4; ++byte) {
    record[kHitAt + byte] = static_cast<std::byte>(hit >> (8 * byte));
  }
  auto decoded = extract::DecodeActions(bytes);
  REQUIRE(decoded);
  if (!decoded) return std::nullopt;
  return std::move(*decoded);
}

// The source of the synthetic actions after setting one field of the first
// hit of action 4, which the synthetic actions give a hit.
std::vector<std::byte> SourceWithHitField(
    std::uint8_t content::ActionHit::* field, std::uint8_t value) {
  constexpr std::size_t kActionWithHit = 4;
  auto actions = support::SyntheticActions();
  auto& hit = actions.definitions.actions[kActionWithHit].hits[0];
  REQUIRE(hit.has_value());
  if (!hit) return {};
  (*hit).*field = value;
  const auto bytes = extract::ReconstructActionSource(actions);
  REQUIRE(bytes);
  return bytes.value_or(std::vector<std::byte>{});
}

// The synthetic tables as the authoring files spell them.
struct ActionTables {
  content::ActionCatalog catalog = support::SyntheticActions().definitions;
  std::string actions = content::FormatActions(catalog);
  std::string hits = content::FormatActionHits(catalog);
};

bool Parses(const std::string& actions, const std::string& hits) {
  return content::ParseActionCatalog(actions, hits).has_value();
}

std::string Replaced(std::string text, std::string_view from,
                     std::string_view to) {
  const auto at = text.find(from);
  REQUIRE(at != std::string::npos);
  text.replace(at, from.size(), to);
  return text;
}
}  // namespace

TEST_CASE("Action source fields decode from their original bytes",
          "[action-catalog]") {
  const auto decoded = DecodeHandBuilt();
  if (!decoded) return;
  const auto& rule = decoded->definitions.actions[kHandBuilt];
  CHECK(rule.kind == 3);
  CHECK(rule.category == 14);
  CHECK(rule.camera_zoom == 2);
  CHECK(rule.behavior == (0x8120 | content::kBehaviorMenuAction |
                          content::kBehaviorHitsFirstObstruction));
  CHECK(decoded->originals[kHandBuilt].unused_ai_bits == 3);
  CHECK(decoded->originals[kHandBuilt].reserved_d_bit0 == 0x0A);
}

TEST_CASE("Action hit words decode into their named fields",
          "[action-catalog]") {
  const auto decoded = DecodeHandBuilt();
  if (!decoded) return;
  CHECK(decoded->definitions.actions[kHandBuilt].hits[0] ==
        content::ActionHit{.effect = 42,
                           .prerequisite = 9,
                           .hit_function = 3,
                           .stat_calculator = 2,
                           .stat_factor = 7,
                           .damage_type = 2,
                           .affinity = 3});
}

TEST_CASE("Action source reloads offline from its provenance",
          "[action-catalog]") {
  const auto synthetic = support::SyntheticActions();
  const auto bytes = SyntheticSection();
  const auto decoded = extract::DecodeActions(bytes);
  REQUIRE(decoded);
  if (!decoded) return;
  CHECK(decoded->definitions == synthetic.definitions);
  CHECK(decoded->originals == synthetic.originals);
  const auto text =
      extract::FormatActionProvenance(*decoded, std::string(64, 'a'));
  const auto restored =
      extract::ParseActionProvenance(std::as_bytes(std::span(text)));
  REQUIRE(restored);
  if (!restored) return;
  CHECK(*restored == synthetic.originals);
  CHECK(extract::ReconstructActionSource({*restored, decoded->definitions}) ==
        bytes);
}

TEST_CASE("Action source refuses malformed and unrebuildable bytes",
          "[action-catalog]") {
  const auto bytes = SyntheticSection();
  auto truncated = bytes;
  truncated.pop_back();
  CHECK_FALSE(extract::DecodeActions(truncated));
  auto misnumbered = bytes;
  misnumbered[5 * kRecord + kIdAt] = std::byte{6};
  CHECK_FALSE(extract::DecodeActions(misnumbered));
  auto unterminated = bytes;
  std::fill(unterminated.begin() + 3 * kRecord + kNameAt,
            unterminated.begin() + 4 * kRecord, std::byte{0x21});
  CHECK_FALSE(extract::DecodeActions(unterminated));
  auto lost_tail = extract::DecodeActions(bytes);
  REQUIRE(lost_tail);
  if (!lost_tail) return;
  lost_tail->originals[3].name_tail.pop_back();
  CHECK_FALSE(extract::ReconstructActionSource(*lost_tail));
}

TEST_CASE("Action source refuses codes the schema cannot name",
          "[action-catalog]") {
  constexpr std::uint8_t kUnnamed = 63;
  const auto prerequisite =
      SourceWithHitField(&content::ActionHit::prerequisite, kUnnamed);
  const auto refused = extract::DecodeActions(prerequisite);
  REQUIRE_FALSE(refused);
  CHECK(refused.error().contains("prerequisite"));
  const auto calculator =
      SourceWithHitField(&content::ActionHit::stat_calculator, kUnnamed);
  const auto unnamed = extract::DecodeActions(calculator);
  REQUIRE_FALSE(unnamed);
  CHECK(unnamed.error().contains("stat_calculator"));
}

TEST_CASE(
    "Action tables lead with a plain-text name and spell every value in words",
    "[action-catalog][content-tables]") {
  const auto synthetic = support::SyntheticActions().definitions;
  const auto actions = content::FormatActions(synthetic);
  const auto hits = content::FormatActionHits(synthetic);
  CHECK(actions.starts_with(
      "id,name,legacy_id,kind,category,cost,recovery_delay,camera_zoom,"
      "effect_id,range_x,"));
  CHECK(actions.contains(
      "action.untitled-000,untitled,0,none,action-category.environment,"));
  CHECK(actions.contains("action.rend,Rend {level},7,trap,"));
  CHECK(hits.starts_with(
      "action,slot,effect,prerequisite,hit_function,stat_calculator,"
      "stat_factor,damage_type,affinity\n"));
  CHECK(hits.contains("action.move-ab,first,"));
  CHECK(content::ParseActionCatalog(actions, hits) == synthetic);
}

TEST_CASE("Committed action schemas match the authoring contract",
          "[action-catalog][content-tables]") {
  const std::filesystem::path folder{
      "Native\\Content\\schemas\\catalogs\\battle"};
  auto actions = ReadText(folder / "actions.schema.json");
  auto hits = ReadText(folder / "action-hits.schema.json");
  std::erase(actions, '\r');
  std::erase(hits, '\r');
  CHECK(actions == content::FormatTableSchema(content::ActionsSpec()));
  CHECK(hits == content::FormatTableSchema(content::ActionHitsSpec()));
}

TEST_CASE("Action schema refuses unknown duplicate and unfit values",
          "[action-catalog]") {
  const ActionTables tables;
  const auto& [synthetic, actions, hits] = tables;
  CHECK(Parses(actions, hits));
  CHECK_FALSE(Parses(Replaced(actions, ",trap,", ",imaginary,"), hits));
  const auto numeric =
      content::ParseActionCatalog(Replaced(actions, ",trap,", ",7,"), hits);
  REQUIRE(numeric);
  if (numeric) CHECK(*numeric == synthetic);
  CHECK_FALSE(Parses(Replaced(actions, ",trap,", ",8,"), hits));
  CHECK_FALSE(Parses(Replaced(actions, "action-category.environment",
                              "action-category.imaginary"),
                     hits));
  CHECK_FALSE(Parses(Replaced(actions, "harmful", "bit15"), hits));
  CHECK_FALSE(Parses(
      Replaced(actions, "menu-action", "menu-action|menu-action"), hits));
}

TEST_CASE("Action schema refuses unfit names and inconsistent tables",
          "[action-catalog]") {
  const ActionTables tables;
  const auto& [synthetic, actions, hits] = tables;
  CHECK_FALSE(
      Parses(Replaced(actions, "Rend {level},7,", "Rend {level},6,"), hits));
  CHECK_FALSE(Parses(Replaced(actions, "Rend {level}", ""), hits));
  CHECK_FALSE(Parses(Replaced(actions, "Rend {level}", "Rend\tlevel"), hits));
  CHECK(Parses(Replaced(actions, "Rend {level}", "Abcdefghijklmnopqrstuvw"),
               hits));
  CHECK_FALSE(Parses(
      Replaced(actions, "Rend {level}", "Abcdefghijklmnopqrstuvwx"), hits));
  CHECK_FALSE(Parses(actions.substr(0, actions.rfind("\naction.") + 1), hits));
  CHECK_FALSE(Parses(
      actions, Replaced(hits, "action.move-ab,first", "action.move,first")));
  auto empty = synthetic;
  empty.actions[kFirstWithHit].hits[0] = content::ActionHit{};
  CHECK_FALSE(Parses(actions, content::FormatActionHits(empty)));
}

TEST_CASE("Action layer reaches the live table and a missing catalog",
          "[action-catalog][live]") {
  const TableScope scope;
  support::ActionRoot root;
  const auto* published = content::DataStore::Get()->Actions();
  REQUIRE(published != nullptr);
  CHECK(*published == root.definitions);
  CHECK(content::LoadCatalogBackedGlobals(*content::DataStore::Get()) == 0);
  CHECK(battle::actionDefinitions[kFirstWithHit].cost ==
        root.definitions.actions[kFirstWithHit].cost);

  root.Write(
      "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mods/tweak"]})");
  root.Write("mods/tweak/catalogs/battle/actions.csv",
             "id,cost\naction.move-ab,99\n");
  root.Write("mods/tweak/catalogs/battle/action-hits.csv",
             "action,slot,stat_factor\naction.move-ab,first,31\n");
  root.Open();
  CHECK(content::LoadCatalogBackedGlobals(*content::DataStore::Get()) == 0);
  CHECK(battle::actionDefinitions[kFirstWithHit].cost == 99);
  CHECK(battle::actionDefinitions[kFirstWithHit].hits[0].stat_factor == 31);
  CHECK(battle::actionDefinitions[2].cost == root.definitions.actions[2].cost);
}

TEST_CASE("Action table is left alone when the catalog is not published",
          "[action-catalog][live]") {
  const TableScope scope;
  support::ActionCategoryRoot root;
  battle::actionDefinitions[kFirstWithHit].cost = 77;
  CHECK(content::DataStore::Get()->Actions() == nullptr);
  CHECK(content::LoadCatalogBackedGlobals(*content::DataStore::Get()) == 1);
  CHECK(battle::actionDefinitions[kFirstWithHit].cost == 77);
}

TEST_CASE("Action catalog without action categories is refused with the fix",
          "[action-catalog]") {
  support::ScratchDataRoot root("test-action");
  root.Write(content::DataStore::ManifestName, "lba\tsize\tform\tpath\n");
  content::ConversionManifest manifest;
  support::AddActionCatalog(root.Root(),
                            support::SyntheticActions().definitions, manifest);
  const auto written = extract::WriteConversionManifest(root.Root(), manifest);
  REQUIRE(written);
  const auto opened = content::DataStore::Open(root.Root());
  REQUIRE_FALSE(opened);
  CHECK(opened.error().contains("action categories"));
}

TEST_CASE("Action table bridge reproduces the original record layout",
          "[action-catalog]") {
  const TableScope scope;
  const auto synthetic = support::SyntheticActions();
  REQUIRE(battle::BuildActionTable(synthetic.definitions,
                                   battle::actionDefinitions));
  const auto source = SyntheticSection();
  REQUIRE(TableBytes().size() == source.size());
  CHECK(DifferingRecords(source) == 0);
  auto unfit = synthetic.definitions;
  unfit.actions[3].name = "Abcdefghijklmnopqrstuvwx";
  battle::actionDefinitions[kFirstWithHit].cost = 77;
  CHECK_FALSE(battle::BuildActionTable(unfit, battle::actionDefinitions));
  CHECK(battle::actionDefinitions[kFirstWithHit].cost !=
        unfit.actions[kFirstWithHit].cost);
}

TEST_CASE(
    "Action converter preserves ownership edits and repairs missing schemas",
    "[action-catalog]") {
  support::ActionRoot root;
  Source(root, "source");
  const auto data = root.Root() / "source";
  REQUIRE(extract::ConvertDiscActions(data, false));
  const auto csv = data / content::kActionsPath;
  const auto text = ReadText(csv);
  const auto stamp = std::filesystem::last_write_time(csv);
  std::filesystem::remove(data / "catalogs/battle/actions.schema.json");
  REQUIRE(extract::ConvertDiscActions(data, false));
  CHECK(ReadText(csv) == text);
  CHECK(std::filesystem::last_write_time(csv) == stamp);
  const auto repeat = extract::ConvertDiscActions(data, false);
  REQUIRE(repeat);
  if (repeat) CHECK(repeat->unchanged == 1);
  Source(root, "foreign");
  root.Write("foreign/catalogs/battle/actions.csv", "not owned");
  CHECK_FALSE(extract::ConvertDiscActions(root.Root() / "foreign", true));
}

TEST_CASE(
    "Action converter refuses stale sources without replacing authored data",
    "[action-catalog]") {
  support::ActionRoot root;
  Source(root, "source");
  const auto data = root.Root() / "source";
  REQUIRE(extract::ConvertDiscActions(data, false));
  const auto csv = data / content::kActionsPath;
  const auto original = ReadText(csv);
  auto source = ReadText(data / "disc/SLUS_010.40");
  source.front() = 'x';
  root.Write("source/disc/SLUS_010.40", source);
  CHECK_FALSE(extract::ConvertDiscActions(data, false));
  CHECK(ReadText(csv) == original);
}

TEST_CASE("Action configured executable reloads all 256 actions exactly",
          "[action-catalog][content-tables]") {
  const auto section = ConfiguredSection();
  if (section.empty()) SKIP("configured extracted executable is unavailable");
  const auto imported = extract::DecodeActions(section);
  REQUIRE(imported);
  if (!imported) return;
  CHECK(extract::ReconstructActionSource(*imported) == section);
}

TEST_CASE(
    "Action persisted provenance and tables reload the configured executable",
    "[action-catalog][content-tables]") {
  const auto section = ConfiguredSection();
  if (section.empty()) SKIP("configured extracted executable is unavailable");
  const auto imported = extract::DecodeActions(section);
  REQUIRE(imported);
  if (!imported) return;
  const auto data = std::filesystem::path("Native\\data");
  const auto provenance =
      extract::ReadFileBytes(data / extract::kActionImportPath);
  REQUIRE(provenance);
  if (!provenance) return;
  const auto restored = extract::ParseActionProvenance(*provenance);
  REQUIRE(restored);
  if (!restored) return;
  CHECK(extract::ReconstructActionSource({*restored, imported->definitions}) ==
        section);
  const auto published =
      content::ParseActionCatalog(ReadText(data / content::kActionsPath),
                                  ReadText(data / content::kActionHitsPath));
  REQUIRE(published);
  if (published) CHECK(*published == imported->definitions);
}

TEST_CASE("Action catalog names the original actions of the configured disc",
          "[action-catalog][content-tables]") {
  const auto section = ConfiguredSection();
  if (section.empty()) SKIP("configured extracted executable is unavailable");
  const auto imported = extract::DecodeActions(section);
  REQUIRE(imported);
  if (!imported) return;
  const auto& actions = imported->definitions.actions;
  constexpr std::size_t kNormal = 1;
  constexpr std::size_t kWhistleSting = 184;
  CHECK(actions[kNormal].id == "action.normal");
  CHECK(actions[kNormal].name == "Normal");
  CHECK(actions[0].id == "action.untitled-000");
  CHECK(actions[kWhistleSting].id == "action.whistle-sting");
}

TEST_CASE("Action ids match the break art slugs of the learning catalog",
          "[action-catalog][content-tables]") {
  const auto section = ConfiguredSection();
  if (section.empty()) SKIP("configured extracted executable is unavailable");
  const auto imported = extract::DecodeActions(section);
  REQUIRE(imported);
  if (!imported) return;
  const auto& actions = imported->definitions.actions;
  // The learning catalog names every break art by the slug the action table
  // derives from its in-game name.
  const auto& learning = content::BreakArtLearningSpec().columns;
  const auto column =
      std::ranges::find(learning, "action", &content::ColumnSpec::name);
  REQUIRE(column != learning.end());
  if (column == learning.end()) return;
  for (const auto& art : column->enumeration) {
    CHECK(actions[static_cast<std::size_t>(art.code)].id ==
          "action." + std::string(art.name));
  }
}

TEST_CASE("Action bridge fills the live table from the configured disc",
          "[action-catalog][content-tables]") {
  const auto section = ConfiguredSection();
  if (section.empty()) SKIP("configured extracted executable is unavailable");
  const auto imported = extract::DecodeActions(section);
  REQUIRE(imported);
  if (!imported) return;
  const TableScope scope;
  REQUIRE(battle::BuildActionTable(imported->definitions,
                                   battle::actionDefinitions));
  CHECK(DifferingRecords(section) == 0);
}
