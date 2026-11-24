#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Actor.hpp"
#include "Battle/AttackPrerequisitesLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hit.hpp"
#include "Battle/StatusCatalogLive.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/StatusCatalog.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/StatusConversion.hpp"
#include "TestSupport/StatusRoot.hpp"

namespace {
namespace content = VS::Content;
namespace extract = VS::Extract;
namespace battle = VS::Battle;

extract::StatusSourceRecords Original() {
  extract::StatusSourceRecords source{};
  for (std::size_t index = 0; index < source.size(); ++index) {
    source[index].effect_channels = std::uint32_t{1} << index;
  }
  source[5].duration_word = 15;
  source[11].duration_word = 20;
  source[13].duration_word = 15;
  source[19].duration_word = 15;
  source[14].interval_word = 2;
  source[17].interval_word = 5;
  return source;
}

std::string ReadText(const std::filesystem::path& file) {
  std::ifstream stream(file, std::ios::binary);
  REQUIRE(stream.good());
  return {std::istreambuf_iterator<char>(stream), {}};
}

void Source(VS::test_support::StatusRoot& root, std::string_view prefix) {
  const auto records = extract::ReconstructStatusSource(Original());
  std::string bytes(extract::kStatusSourceOffset, '\0');
  for (const auto byte : records) bytes += static_cast<char>(byte);
  root.Write(std::string(prefix) + "/disc/SLUS_010.40", bytes);
  root.Write(std::string(prefix) + "/files.tsv",
             "lba\tsize\tform\tpath\n24\t" + std::to_string(bytes.size()) +
                 "\traw\tSLUS_010.40\n");
}

// The status section of the configured extracted executable; empty when the
// executable is not extracted.
std::vector<std::byte> ConfiguredStatusSection() {
  const auto path =
      std::filesystem::path("Native\\data") / "disc" / content::kStatusSource;
  if (!std::filesystem::is_regular_file(path)) return {};
  const auto bytes = extract::ReadFileBytes(path);
  if (!bytes || bytes->size() < extract::kStatusSourceOffset +
                                    extract::kStatusSourceBytes) {
    FAIL("configured executable is shorter than the status section");
  }
  const auto section = std::span(*bytes).subspan(extract::kStatusSourceOffset,
                                                 extract::kStatusSourceBytes);
  return {section.begin(), section.end()};
}

struct Live {
  VS::test_support::StatusRoot catalog;
  battle::Actor* previous = battle::actors[0];
  battle::Actor actor{};
  battle::ActorStats stats{};
  battle::HitResult hit{};
  Live() {
    actor.stats = &stats;
    battle::actors[0] = &actor;
    stats.current_hp = stats.max_hp = 100;
    stats.strength = stats.total_strength = 100;
    catalog.definitions.rules[5] = {0x20, 0x60, 0};
    catalog.Publish();
  }
  ~Live() { battle::actors[0] = previous; }
};
}  // namespace

TEST_CASE(
    "Status source high words survive offline reload without AI narrowing",
    "[status-catalog]") {
  auto source = Original();
  source[5].duration_word = 0x12FF;
  source[11].duration_word = 0x3401;
  source[14].interval_word = 0x5602;
  source[17].interval_word = 0x7805;
  source[31].replaces = 0xFFFFFFFF;
  const auto bytes = extract::ReconstructStatusSource(source);
  CHECK(extract::DecodeStatusSource(bytes) == source);
  const auto text =
      extract::FormatStatusProvenance(source, std::string(64, 'a'));
  const auto restored =
      extract::ParseStatusProvenance(std::as_bytes(std::span(text)));
  REQUIRE(restored);
  if (!restored) return;
  CHECK(extract::ReconstructStatusSource(*restored) == bytes);
  CHECK_FALSE(extract::DecodeStatus(bytes));
}

TEST_CASE("Status import derives six shared roles rather than unused durations",
          "[status-catalog]") {
  auto source = Original();
  source[5].duration_word = 0x12FF;
  source[11].duration_word = 0x3401;
  source[15].duration_word = 99;
  source[20].duration_word = 88;
  source[21].duration_word = 77;
  const auto imported =
      extract::DecodeStatus(extract::ReconstructStatusSource(source));
  REQUIRE(imported);
  if (!imported) return;
  CHECK(imported->definitions.timings ==
        content::StatusTimings{255, 1, 15, 15, 2, 5});
  CHECK(imported->originals == source);
  const auto parsed = content::ParseStatusCatalog(
      content::FormatStatusRules(imported->definitions),
      content::FormatStatusTimings(imported->definitions));
  CHECK(parsed == imported->definitions);
}

TEST_CASE(
    "Committed status schemas match the authoring contract and both tables "
    "lead with a plain-text name",
    "[status-catalog][content-tables]") {
  const std::filesystem::path root(
      "Native\\Content\\schemas\\catalogs\\battle");
  auto rules = ReadText(root / "status-rules.schema.json");
  auto timings = ReadText(root / "status-timings.schema.json");
  std::erase(rules, '\r');
  std::erase(timings, '\r');
  CHECK(rules == content::FormatTableSchema(content::StatusRulesSpec()));
  CHECK(timings == content::FormatTableSchema(content::StatusTimingsSpec()));
  const content::StatusCatalog catalog{};
  CHECK(content::FormatStatusRules(catalog).starts_with(
      "id,display_name,effect_channels,"));
  CHECK(content::FormatStatusTimings(catalog).starts_with(
      "id,display_name,status_updates\n"
      "timing.attribute-modifiers,attribute modifiers,"));
}

TEST_CASE("Status schema refuses unknown duplicate and numeric flag spellings",
          "[status-catalog]") {
  VS::test_support::StatusRoot root;
  const auto timings = content::FormatStatusTimings(root.definitions);
  for (const auto name : {"bit31", "2147483648", "-2147483648", "poison|poison",
                          "poison|", "|poison"}) {
    auto text = content::FormatStatusRules(root.definitions);
    const auto at = text.find(",none,none,none");
    REQUIRE(at != std::string::npos);
    text.replace(at + 1, 4, name);
    CHECK_FALSE(content::ParseStatusCatalog(text, timings));
  }
}

TEST_CASE("Status schema refuses missing duplicate and unsupported roles",
          "[status-catalog]") {
  VS::test_support::StatusRoot root;
  const auto rules = content::FormatStatusRules(root.definitions);
  auto timing = content::FormatStatusTimings(root.definitions);
  const auto at = timing.find("timing.attribute-modifiers");
  REQUIRE(at != std::string::npos);
  CHECK_FALSE(content::ParseStatusCatalog(rules, timing.substr(0, at)));
  CHECK_FALSE(content::ParseStatusCatalog(
      rules, timing + "timing.attribute-modifiers,12\n"));
  timing.replace(at, std::string("timing.attribute-modifiers").size(),
                 "timing.imaginary");
  CHECK_FALSE(content::ParseStatusCatalog(rules, timing));
}

TEST_CASE(
    "Status layer reaches live application expiry and repeated catalog load",
    "[status-catalog][live]") {
  Live live;
  live.catalog.Write(
      "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mods/status"]})");
  live.catalog.Write("mods/status/catalogs/battle/status-rules.csv",
                     "id,effect_channels\nstatus.strength-down,strength-up\n");
  live.catalog.Write("mods/status/catalogs/battle/status-timings.csv",
                     "id,status_updates\ntiming.attribute-modifiers,255\n");
  live.catalog.Open();
  CHECK(battle::QueueLiveApplicableStatusChanges(5, &live.hit) == 0x40);
  battle::InflictLiveActorStatuses(live.hit.remove_statuses, &live.stats);
  CHECK(live.stats.strength == 110);
  CHECK(live.stats.status_timers[0] == 255);
  live.catalog.Open();
  CHECK(live.stats.status_timers[0] == 255);
  battle::RemoveLiveActorStatuses(0x40, &live.stats);
  CHECK(live.stats.strength == 100);
  CHECK(live.stats.status_timers[0] == 0);
}

TEST_CASE("Status authoring refuses values beyond retained actor capacity",
          "[status-catalog]") {
  VS::test_support::StatusRoot root;
  root.Write(
      "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mods/status"]})");
  root.Write("mods/status/catalogs/battle/status-timings.csv",
             "id,status_updates\ntiming.poison-pulse,256\n");
  const auto opened = content::DataStore::Open(root.Root());
  CHECK_FALSE(opened);
}

TEST_CASE("Missing status catalog refuses live prerequisites not a zero rule",
          "[status-catalog][live]") {
  Live live;
  content::DataStore::ScopedOverride absent;
  std::array<std::uint8_t, 4> target{};
  CHECK(battle::LiveStatusCatalog() == nullptr);
  CHECK(battle::CanApplyStrengthDownSlot(nullptr, target.data()) == 0);
}

TEST_CASE(
    "Status converter preserves ownership edits and repairs missing schemas",
    "[status-catalog]") {
  VS::test_support::StatusRoot root;
  Source(root, "source");
  const auto data = root.Root() / "source";
  REQUIRE(extract::ConvertDiscStatus(data, false));
  const auto csv = data / content::kStatusRulesPath;
  const auto text = ReadText(csv);
  const auto stamp = std::filesystem::last_write_time(csv);
  std::filesystem::remove(data / "catalogs/battle/status-rules.schema.json");
  REQUIRE(extract::ConvertDiscStatus(data, false));
  CHECK(ReadText(csv) == text);
  CHECK(std::filesystem::last_write_time(csv) == stamp);
  const auto repeat = extract::ConvertDiscStatus(data, false);
  REQUIRE(repeat);
  if (repeat) CHECK(repeat->unchanged == 1);
  Source(root, "foreign");
  root.Write("foreign/catalogs/battle/status-rules.csv", "not owned");
  CHECK_FALSE(extract::ConvertDiscStatus(root.Root() / "foreign", true));
}

TEST_CASE(
    "Status converter refuses stale sources without replacing authored data",
    "[status-catalog]") {
  VS::test_support::StatusRoot root;
  Source(root, "source");
  const auto data = root.Root() / "source";
  REQUIRE(extract::ConvertDiscStatus(data, false));
  const auto csv = data / content::kStatusRulesPath;
  const auto original = ReadText(csv);
  auto source = ReadText(data / "disc/SLUS_010.40");
  source.front() = 'x';
  root.Write("source/disc/SLUS_010.40", source);
  CHECK_FALSE(extract::ConvertDiscStatus(data, false));
  CHECK(ReadText(csv) == original);
}

TEST_CASE(
    "Status configured executable and persisted provenance reload all 32 "
    "records exactly",
    "[status-catalog][content-tables]") {
  const auto section = ConfiguredStatusSection();
  if (section.empty()) SKIP("configured extracted executable is unavailable");
  const auto imported = extract::DecodeStatus(section);
  REQUIRE(imported);
  if (!imported) return;
  CHECK(std::ranges::equal(
      extract::ReconstructStatusSource(imported->originals), section));
  const auto provenance = extract::ReadFileBytes(
      std::filesystem::path("Native\\data") / extract::kStatusImportPath);
  REQUIRE(provenance);
  if (!provenance) return;
  const auto restored = extract::ParseStatusProvenance(*provenance);
  REQUIRE(restored);
  if (!restored) return;
  CHECK(
      std::ranges::equal(extract::ReconstructStatusSource(*restored), section));
}
