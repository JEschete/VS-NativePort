#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/SourceStampCache.hpp"
#include "Core/Sha256.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Platform/AtomicFile.hpp"
#include "TestSupport/ModelFixtures.hpp"

namespace {
namespace content = VS::Content;
namespace fs = std::filesystem;
using VS::test_support::ModelRoot;
constexpr std::string_view kSource = "TEST.TIM";

std::vector<VS::Extract::SourceFile> Sources() {
  return {{.path = std::string(kSource),
           .bytes = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}}}};
}

void Write(const fs::path& path, std::string_view text) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
  file.close();
  REQUIRE(file.good());
}

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.is_open());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

void Prepare(const ModelRoot& root) {
  content::ConversionManifest manifest;
  for (const auto& converter : content::kRequiredConverters) {
    manifest.converters.emplace(converter.name, converter.version);
  }
  const auto source = Sources().front();
  manifest.outputs.push_back(
      {.id = "image.test",
       .source = std::string(kSource),
       .source_sha256 = VS::Core::Sha256Hex(source.bytes),
       .converter = "tim-png",
       .files = {"test.png"},
       .dependencies = {},
       .unreferenced = false,
       .note = {},
       .alias_of = {}});
  Write(root.Root() / "test.png", "declared output");
  const auto written =
      VS::Extract::WriteConversionManifest(root.Root(), manifest);
  if (!written) FAIL(written.error());
  const auto opened = content::DataStore::Open(root.Root());
  if (!opened) FAIL(opened.error());
}

fs::path CacheFile(const ModelRoot& root) {
  return root.Root() / ".cache" / "source-verification.json";
}

content::SourceVerificationStats Verify(content::SourceVerification mode) {
  content::SourceVerificationStats statistics;
  const auto checked =
      content::DataStore::Get()->RequireConversions(mode, &statistics);
  if (!checked) FAIL(checked.error());
  return statistics;
}
}  // namespace

TEST_CASE("Unchanged original sources reuse validated stamps without hashing",
          "[source-verification]") {
  content::DataStore::ScopedOverride restore;
  const ModelRoot root(Sources());
  Prepare(root);
  const auto first = Verify(content::SourceVerification::kCached);
  CHECK(first.files_hashed == 1);
  CHECK(first.bytes_hashed == 4);
  const auto before = fs::last_write_time(CacheFile(root));
  const auto second = Verify(content::SourceVerification::kCached);
  CHECK(second.files_hashed == 0);
  CHECK(second.cache_hits == 1);
  CHECK(fs::last_write_time(CacheFile(root)) == before);
  CHECK(Verify(content::SourceVerification::kFull).files_hashed == 1);
}

TEST_CASE("Source verification rejects appended and truncated physical files",
          "[source-verification][source-integrity]") {
  const std::string replacement = GENERATE("ABC", "ABCDE");
  content::DataStore::ScopedOverride restore;
  const ModelRoot root(Sources());
  Prepare(root);
  Verify(content::SourceVerification::kFull);
  const auto cache = Read(CacheFile(root));
  Write(root.Root() / "disc" / kSource, replacement);
  const auto rejected = content::DataStore::Get()->RequireConversions();
  REQUIRE_FALSE(rejected.has_value());
  CHECK(rejected.error().contains("TEST.TIM: source size is "));
  CHECK(rejected.error().contains("expected 4 from files.tsv"));
  CHECK(Read(CacheFile(root)) == cache);
}

TEST_CASE(
    "Full verification detects same-size timestamp-preserving source edits",
    "[source-verification][source-integrity]") {
  content::DataStore::ScopedOverride restore;
  const ModelRoot root(Sources());
  Prepare(root);
  Verify(content::SourceVerification::kFull);
  const auto file = root.Root() / "disc" / kSource;
  const auto stamp = fs::last_write_time(file);
  const auto cache = Read(CacheFile(root));
  Write(file, "EDIT");
  fs::last_write_time(file, stamp);
  CHECK(Verify(content::SourceVerification::kCached).cache_hits == 1);
  const auto rejected = content::DataStore::Get()->RequireConversions(
      content::SourceVerification::kFull);
  REQUIRE_FALSE(rejected.has_value());
  CHECK(rejected.error().contains("stale tim-png source hash"));
  CHECK(Read(CacheFile(root)) == cache);
}

TEST_CASE("Changed source timestamps require a fresh matching hash",
          "[source-verification]") {
  content::DataStore::ScopedOverride restore;
  const ModelRoot root(Sources());
  Prepare(root);
  Verify(content::SourceVerification::kCached);
  const auto file = root.Root() / "disc" / kSource;
  const auto stamp = fs::last_write_time(file);
  fs::last_write_time(file, stamp + fs::file_time_type::duration{1});
  const auto checked = Verify(content::SourceVerification::kCached);
  CHECK(checked.files_hashed == 1);
  CHECK(checked.cache_hits == 0);
}

TEST_CASE("Malformed caches are revalidated and rebuilt rather than trusted",
          "[source-verification]") {
  content::DataStore::ScopedOverride restore;
  const ModelRoot root(Sources());
  Prepare(root);
  Verify(content::SourceVerification::kCached);
  Write(CacheFile(root), "{broken");
  CHECK(Verify(content::SourceVerification::kCached).files_hashed == 1);
  CHECK(Verify(content::SourceVerification::kCached).cache_hits == 1);
}

TEST_CASE(
    "A changed expected source hash invalidates an otherwise matching stamp",
    "[source-verification]") {
  content::DataStore::ScopedOverride restore;
  const ModelRoot root(Sources());
  Prepare(root);
  Verify(content::SourceVerification::kCached);
  const auto cache = Read(CacheFile(root));
  auto manifest = content::LoadConversionManifest(root.Root());
  if (!manifest || !manifest->has_value()) FAIL("missing fixture manifest");
  (**manifest).outputs.front().source_sha256 = std::string(64, '0');
  REQUIRE(VS::Extract::WriteConversionManifest(root.Root(), **manifest)
              .has_value());
  REQUIRE(content::DataStore::Open(root.Root()).has_value());
  content::SourceVerificationStats statistics;
  CHECK_FALSE(content::DataStore::Get()
                  ->RequireConversions(content::SourceVerification::kCached,
                                       &statistics)
                  .has_value());
  CHECK(statistics.files_hashed == 1);
  CHECK(statistics.cache_hits == 0);
  CHECK(Read(CacheFile(root)) == cache);
}

TEST_CASE("A cache cannot conceal missing outputs or missing original files",
          "[source-verification]") {
  const bool remove_source = GENERATE(false, true);
  content::DataStore::ScopedOverride restore;
  const ModelRoot root(Sources());
  Prepare(root);
  Verify(content::SourceVerification::kCached);
  const auto before = Read(CacheFile(root));
  const auto missing =
      remove_source ? root.Root() / "disc" / kSource : root.Root() / "test.png";
  REQUIRE(fs::remove(missing));
  CHECK_FALSE(content::DataStore::Get()->RequireConversions().has_value());
  CHECK(Read(CacheFile(root)) == before);
}

TEST_CASE(
    "Cache write failure does not turn verified source data into a failure",
    "[source-verification]") {
  content::DataStore::ScopedOverride restore;
  const ModelRoot root(Sources());
  Prepare(root);
  Write(root.Root() / ".cache", "blocked cache directory");
  CHECK(Verify(content::SourceVerification::kCached).files_hashed == 1);
  CHECK(Read(root.Root() / ".cache") == "blocked cache directory");
  CHECK(Verify(content::SourceVerification::kCached).files_hashed == 1);
}

TEST_CASE("Atomic file replacement preserves the old target on commit failure",
          "[source-verification][atomic-file]") {
  const ModelRoot root(Sources());
  const auto target = root.Root() / "cache.json";
  REQUIRE(VS::Platform::WriteFileAtomically(target, "old").has_value());
  REQUIRE(VS::Platform::WriteFileAtomically(target, "new").has_value());
  CHECK(Read(target) == "new");
  const auto directory = root.Root() / "existing-directory";
  REQUIRE(fs::create_directory(directory));
  CHECK_FALSE(
      VS::Platform::WriteFileAtomically(directory, "invalid").has_value());
  CHECK(fs::is_directory(directory));
  for (const auto& entry : fs::directory_iterator(root.Root())) {
    CHECK(entry.path().extension() != ".tmp");
  }
}
