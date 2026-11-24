#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/BestiaryCatalog.hpp"
#include "Content/GameTextCodec.hpp"
#include "Core/Sha256.hpp"
#include "Extract/BestiaryConversion.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Menu/Score/GazetteEncyclopedia.hpp"

namespace {
namespace content = VS::Content;
namespace extract = VS::Extract;
void Word(std::vector<std::byte>& bytes, std::size_t at, unsigned value) {
  bytes.at(at) = static_cast<std::byte>(value);
  bytes.at(at + 1) = static_cast<std::byte>(value >> 8);
}
std::vector<std::byte> Original() {
  std::vector<std::byte> bytes(6900);
  for (unsigned i = 0; i < 150; ++i) {
    const auto start = i * 44;
    if (i < 78) {
      Word(bytes, start, i);
      Word(bytes, start + 2, i % 6);
      Word(bytes, start + 4, i);
      Word(bytes, start + 6, 1);
      auto name = content::EncodeGameText(
          "{spacing:14}Sample{spacing:6}" + std::to_string(i), "name");
      REQUIRE(name);
      std::ranges::copy(*name, bytes.begin() + start + 16);
      bytes[start + 43] = std::byte{0xBC};  // Suffix, not displayed text.
    }
    Word(bytes, 6600 + i * 2, static_cast<unsigned>((bytes.size() - 6600) / 2));
    auto description = content::EncodeGameText(
        i < 78 ? "A foe.\nObserved." : "", "description");
    REQUIRE(description);
    bytes.insert(bytes.end(), description->begin(), description->end());
    if (bytes.size() % 2 != 0) bytes.push_back(std::byte{0xAA});
  }
  return bytes;
}
extract::ImportedBestiary Import(const std::vector<std::byte>& bytes) {
  auto imported = extract::DecodeBestiary(bytes, VS::Core::Sha256Hex(bytes));
  REQUIRE(imported);
  if (!imported) throw std::runtime_error(imported.error());
  return std::move(*imported);
}
}  // namespace

TEST_CASE("Bestiary semantic definitions preserve every original byte offline",
          "[bestiary][catalog][game-text-codec]") {
  const auto bytes = Original();
  const auto imported = Import(bytes);
  CHECK(imported.definitions.size() == 78);
  CHECK(imported.provenance["records"].size() == 150);
  CHECK(imported.definitions[2].id == "bestiary.sample-2");
  CHECK(imported.definitions[2].name == "text.bestiary.sample-2.name");
  CHECK(imported.strings.at("text.bestiary.sample-2.name") == "Sample 2");
  auto reconstructed = extract::ReconstructBestiary(imported);
  REQUIRE(reconstructed);
  REQUIRE(reconstructed->size() == bytes.size());
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    CHECK((*reconstructed)[i] == bytes[i]);
  }
}

TEST_CASE(
    "Bestiary validates source widths ranges offsets and semantic collisions",
    "[bestiary][catalog]") {
  const auto original = Original();
  auto truncated = original;
  truncated.resize(6899);
  CHECK_FALSE(extract::DecodeBestiary(truncated, "hash"));
  for (const auto offset : {std::size_t{0}, std::size_t{2}, std::size_t{4},
                            std::size_t{6}, std::size_t{6600}}) {
    auto bytes = original;
    Word(bytes, offset, 0xFFFF);
    CHECK_FALSE(extract::DecodeBestiary(bytes, "hash"));
  }
  auto duplicate = original;
  std::ranges::copy(std::span(original).subspan(16, 28),
                    duplicate.begin() + 60);
  CHECK_FALSE(extract::DecodeBestiary(duplicate, "hash"));
}

TEST_CASE(
    "Bestiary rejects stale provenance instead of silently rebuilding changed "
    "originals",
    "[bestiary][catalog]") {
  const auto original = Original();
  auto imported = Import(original);
  imported.provenance["records"][0]["description_offset"] = 2;
  CHECK_FALSE(extract::ReconstructBestiary(imported));
  imported = Import(original);
  imported.provenance["records"][0]["name_markup"] = "Changed";
  CHECK_FALSE(extract::ReconstructBestiary(imported));
  imported = Import(original);
  imported.definitions[0].preview_table_index = 99;
  CHECK_FALSE(extract::ReconstructBestiary(imported));
  imported = Import(original);
  imported.provenance["records"][149]["record_hex"] = "ab";
  CHECK_FALSE(extract::ReconstructBestiary(imported));
  imported = Import(original);
  imported.provenance["records"][0]["name_markup"] = 7;
  CHECK_FALSE(extract::ReconstructBestiary(imported));
}

TEST_CASE("Bestiary CSV validates named policies identities and references",
          "[bestiary][catalog]") {
  auto catalog = Import(Original()).definitions;
  auto text = content::FormatBestiary(catalog);
  auto parsed = content::ParseBestiary(text);
  REQUIRE(parsed);
  CHECK(*parsed == catalog);
  catalog[1].id = catalog[0].id;
  CHECK_FALSE(content::ParseBestiary(content::FormatBestiary(catalog)));
  catalog[1].id = "bestiary.other";
  catalog[1].display_order = catalog[0].display_order;
  CHECK_FALSE(content::ParseBestiary(content::FormatBestiary(catalog)));
  catalog[1].display_order = 2;
  catalog[1].unlock = content::BestiaryUnlock::kAlways;
  CHECK_FALSE(content::ParseBestiary(content::FormatBestiary(catalog)));
}

TEST_CASE(
    "Bestiary native records extend beyond original record and name limits",
    "[bestiary][catalog][gazette-terminal]") {
  auto catalog = Import(Original()).definitions;
  for (unsigned i = 78; i < 300; ++i) {
    auto entry = catalog.front();
    entry.id = "bestiary.field-notes-" + std::to_string(i);
    entry.legacy_id = -1;
    entry.display_order = i + 1;
    entry.name = "text." + entry.id + ".name";
    entry.description = "text." + entry.id + ".description";
    catalog.push_back(std::move(entry));
  }
  auto parsed = content::ParseBestiary(content::FormatBestiary(catalog));
  REQUIRE(parsed);
  CHECK(parsed->size() == 300);
  std::vector<VS::Menu::Score::GazetteEnemy> session(300);
  for (auto& enemy : session) enemy.always_unlocked = true;
  const std::array<std::uint32_t, 26> flags{};
  CHECK(VS::Menu::Score::InitializeEnemyList(session, flags));
  CHECK(session[0].previous == 299);
  CHECK(session[299].next == 0);
  CHECK(std::ranges::all_of(flags, [](auto word) { return word == 0; }));
}

TEST_CASE(
    "Bestiary configured disc corpus reloads all records and descriptions "
    "exactly",
    "[bestiary][catalog]") {
  const auto* configured = std::getenv("VS_BESTIARY_DATA_ROOT");
  const auto root = configured != nullptr
                        ? std::filesystem::path(configured)
                        : std::filesystem::path(R"(Native\data)");
  const auto source = extract::ReadSource(root, content::kBestiarySource);
  REQUIRE(source);
  const auto imported = Import(source->bytes);
  const auto csv = content::FormatBestiary(imported.definitions);
  auto reloaded = content::ParseBestiary(csv);
  REQUIRE(reloaded);
  CHECK(*reloaded == imported.definitions);
  auto stored = extract::ReadFileBytes(root / extract::kBestiaryImportPath);
  REQUIRE(stored);
  auto from_files = extract::ParseBestiaryProvenance(*stored);
  REQUIRE(from_files);
  auto exact = extract::ReconstructBestiary(*from_files);
  REQUIRE(exact);
  REQUIRE(exact->size() == source->bytes.size());
  for (std::size_t i = 0; i < exact->size(); ++i) {
    CHECK((*exact)[i] == source->bytes[i]);
  }
}
