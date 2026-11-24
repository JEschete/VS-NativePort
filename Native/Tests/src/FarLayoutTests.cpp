#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/BackgroundLayout.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Core/Sha256.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/FarConversion.hpp"
#include "Extract/ImageConversion.hpp"
#include "Field/BackgroundLive.hpp"
#include "Platform/Paths.hpp"

namespace {
namespace Content = VS::Content;
namespace Extract = VS::Extract;
namespace fs = std::filesystem;
using Bytes = std::vector<std::byte>;
constexpr std::string_view kLayoutFile = "backgrounds/001OP01A.background.json";

template <typename T>
T Required(std::expected<T, std::string> value) {
  if (!value) throw std::runtime_error(value.error());
  return std::move(*value);
}

std::span<const std::byte> BytesOf(const std::string& text) {
  return std::as_bytes(std::span(text));
}

void Put(Bytes& bytes, std::uint32_t value, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    bytes.push_back(static_cast<std::byte>((value >> (8 * i)) & 255));
  }
}

Bytes FarBytes() {
  Bytes bytes;
  for (const auto value : {1U, 1U, 0xCD000000U}) Put(bytes, value, 4);
  for (const auto value : {5, 7, -4, -6}) {
    Put(bytes, static_cast<std::uint32_t>(value), 2);
  }
  const std::array<std::uint8_t, 4> codes{13, 1, 17, 19};
  for (std::size_t i = 0; i < 4; ++i) {
    Put(bytes, static_cast<std::uint32_t>(i * 32), 1);
    Put(bytes, 128, 1);
    Put(bytes, 255, 1);
    Put(bytes, codes.at(i), 1);
  }
  return bytes;
}

Bytes TimBytes() {
  Bytes bytes;
  for (const auto value : {0x10U, 0x9U, 524U}) Put(bytes, value, 4);
  for (const auto value : {0U, 480U, 256U, 1U}) Put(bytes, value, 2);
  for (std::uint32_t i = 0; i < 256; ++i) Put(bytes, i, 2);
  Put(bytes, 76, 4);
  for (const auto value : {512U, 0U, 4U, 8U}) Put(bytes, value, 2);
  for (std::uint32_t i = 0; i < 32; ++i) Put(bytes, i * 257, 2);
  return bytes;
}

Content::BackgroundLayoutDocument Identity() {
  return {.id = "background.test.mirrored-gradient",
          .image_id = "image.bg.001op01a",
          .source = "BG/001OP01A.FAR",
          .source_sha256 = VS::Core::Sha256Hex(FarBytes()),
          .image_source = "BG/001OP01A.TIM",
          .image_source_sha256 = VS::Core::Sha256Hex(TimBytes())};
}

class FarData {
 public:
  FarData()
      : root(fs::current_path() / "build" /
             ("far-fixture-" + std::to_string(std::chrono::steady_clock::now()
                                                  .time_since_epoch()
                                                  .count()))) {
    Write("disc/BG/001OP01A.FAR", FarBytes());
    Write("disc/BG/001OP01A.TIM", TimBytes());
    const std::string manifest =
        "lba\tsize\tform\tpath\n100\t36\traw\tBG/001OP01A.FAR\n"
        "101\t608\traw\tBG/001OP01A.TIM\n";
    Write("files.tsv", BytesOf(manifest));
    Required(Extract::ConvertDiscImages(root, false));
  }
  FarData(const FarData&) = delete;
  FarData& operator=(const FarData&) = delete;
  // NOLINTNEXTLINE(bugprone-exception-escape): cleanup uses the error_code API.
  ~FarData() {
    store_scope.Restore();
    std::error_code error;
    fs::remove_all(root, error);
  }
  void Write(std::string_view name, std::span<const std::byte> bytes) const {
    const auto path = root / VS::Platform::Utf8Path(name);
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("cannot write FAR fixture");
  }
  Bytes Read(std::string_view name) const {
    return Required(
        Extract::ReadFileBytes(root / VS::Platform::Utf8Path(name)));
  }
  fs::path root;
  Content::DataStore::ScopedOverride store_scope;
};

VS::Field::TiledBackgroundSection Section(
    const Content::DataStore::Entry& far,
    const Content::DataStore::Entry& tim) {
  return {
      .texture_page_count = 1,
      .texture = {.lba = static_cast<std::int32_t>(tim.lba),
                  .size = static_cast<std::int32_t>(tim.SectorCount() * 2048)},
      .layout = {.lba = static_cast<std::int32_t>(far.lba),
                 .size = static_cast<std::int32_t>(far.SectorCount() * 2048)}};
}

void CheckLoadedCells(const Content::BackgroundLayoutDocument& document,
                      const VS::Field::OwnedBackgroundLayout& loaded) {
  REQUIRE(loaded.tiles.size() == document.cells.size());
  CHECK(loaded.width == static_cast<std::int32_t>(document.columns));
  CHECK(loaded.height == static_cast<std::int32_t>(document.rows));
  for (std::size_t i = 0; i < document.cells.size(); ++i) {
    CAPTURE(i);
    const auto& expected = document.cells[i];
    const auto& actual = loaded.tiles[i];
    CHECK(std::array<std::int32_t, 4>{actual.texture.x, actual.texture.y,
                                      actual.texture.width,
                                      actual.texture.height} ==
          std::array<std::int32_t, 4>{expected.left, expected.top,
                                      expected.right - expected.left,
                                      expected.bottom - expected.top});
    for (std::size_t corner = 0; corner < 4; ++corner) {
      const auto& color = expected.colors.at(corner);
      const auto& drawn = actual.corners.at(corner);
      CHECK(std::array<std::uint32_t, 3>{drawn.red, drawn.green, drawn.blue} ==
            std::array<std::uint32_t, 3>{color.red, color.green, color.blue});
    }
    CHECK(actual.corners[1].code == 1);
  }
}
}  // namespace

TEST_CASE("FAR JSON preserves signed sampling and unconsumed provenance",
          "[far-layout]") {
  const auto layout = Required(Content::DecodeFar(FarBytes(), Identity()));
  CHECK(layout.cells.front().right == 1);
  CHECK(layout.cells.front().bottom == 1);
  CHECK(layout.unused_header_word == 0xCD000000U);
  CHECK(layout.cells.front().unused_corner_bytes ==
        std::array<std::uint8_t, 3>{13, 17, 19});
  const auto text = Required(Content::WriteBackgroundLayout(layout));
  const auto reloaded =
      Required(Content::ReadBackgroundLayout(BytesOf(text), "fixture"));
  CHECK(reloaded == layout);
  CHECK(Required(Content::EncodeFar(reloaded)) == FarBytes());
  CHECK(Required(Content::WriteBackgroundLayout(reloaded)) == text);
}

TEST_CASE("FAR import refuses truncated, inconsistent and unsupported inputs",
          "[far-layout]") {
  auto bytes = FarBytes();
  bytes.pop_back();
  CHECK_FALSE(Content::DecodeFar(bytes, Identity()));
  bytes = FarBytes();
  bytes[0] = std::byte{32};
  CHECK_FALSE(Content::DecodeFar(bytes, Identity()));
  bytes = FarBytes();
  bytes[27] = std::byte{2};
  CHECK_FALSE(Content::DecodeFar(bytes, Identity()));
  bytes = FarBytes();
  bytes[12] = std::byte{255};
  bytes[16] = std::byte{32};
  bytes[17] = std::byte{0};
  CHECK_FALSE(Content::DecodeFar(bytes, Identity()));
}

TEST_CASE(
    "FAR native schema refuses misspellings, invalid colors and cell counts",
    "[far-layout]") {
  const auto text = Required(Content::WriteBackgroundLayout(
      Required(Content::DecodeFar(FarBytes(), Identity()))));
  const auto original =
      Required(Content::ParseJsonText(BytesOf(text), "fixture"));
  auto json = original;
  json["cells"][0]["corner_colors"]["top_left"]["reed"] = 12;
  CHECK_FALSE(Content::ReadBackgroundLayout(BytesOf(json.dump()), "fixture"));
  json = original;
  json["cells"][0]["corner_colors"]["top_left"]["red"] = 256;
  CHECK_FALSE(Content::ReadBackgroundLayout(BytesOf(json.dump()), "fixture"));
  json = original;
  json["columns"] = 2;
  CHECK_FALSE(Content::ReadBackgroundLayout(BytesOf(json.dump()), "fixture"));
  json = original;
  json["image_depth"] = "scene-depth";
  CHECK_FALSE(Content::ReadBackgroundLayout(BytesOf(json.dump()), "fixture"));
}

TEST_CASE("FAR conversion preserves authored edits and is repeat-stable",
          "[far-layout]") {
  FarData data;
  CHECK(Required(Extract::ConvertDiscBackgrounds(data.root, false)).converted ==
        1);
  const auto before = data.Read(kLayoutFile);
  const auto manifest = data.Read("manifest.json");
  CHECK(Required(Extract::ConvertDiscBackgrounds(data.root, false)).unchanged ==
        1);
  CHECK(data.Read(kLayoutFile) == before);
  CHECK(data.Read("manifest.json") == manifest);
  auto layout = Required(Content::ReadBackgroundLayout(before, kLayoutFile));
  layout.cells.front().colors.front().red = 42;
  const auto edited = Required(Content::WriteBackgroundLayout(layout));
  data.Write(kLayoutFile, BytesOf(edited));
  CHECK(Required(Extract::ConvertDiscBackgrounds(data.root, false)).unchanged ==
        1);
  CHECK(data.Read(kLayoutFile) ==
        Bytes(BytesOf(edited).begin(), BytesOf(edited).end()));
}

TEST_CASE("FAR conversion invalidates changed or missing image dependencies",
          "[far-layout]") {
  FarData data;
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  const auto before = data.Read("manifest.json");
  auto tim = TimBytes();
  tim.back() = std::byte{7};
  data.Write("disc/BG/001OP01A.TIM", tim);
  CHECK_FALSE(Extract::ConvertDiscBackgrounds(data.root, false));
  CHECK(data.Read("manifest.json") == before);
  Required(Extract::ConvertDiscImages(data.root, false));
  CHECK(Required(Extract::ConvertDiscBackgrounds(data.root, false)).converted ==
        1);
  fs::remove(data.root / "disc" / "BG" / "001OP01A.TIM");
  CHECK_FALSE(Extract::ConvertDiscBackgrounds(data.root, false));
}

TEST_CASE(
    "FAR native reader rejects missing bindings and out-of-image rectangles",
    "[far-layout]") {
  FarData data;
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  auto layout = Required(
      Content::ReadBackgroundLayout(data.Read(kLayoutFile), kLayoutFile));
  const auto image = Required(
      Content::LoadImageFiles(data.root / "textures" / "bg", "001OP01A"));
  layout.image_id = "image.missing";
  CHECK_FALSE(Content::ValidateBackgroundImage(layout, image));
  layout.image_id = image.id;
  layout.cells.front().right = 8;
  CHECK_FALSE(Content::ValidateBackgroundImage(layout, image));
  const auto text = Required(Content::WriteBackgroundLayout(layout));
  data.Write(kLayoutFile, BytesOf(text));
  CHECK_FALSE(Extract::ConvertDiscBackgrounds(data.root, false));
}

TEST_CASE("Live FAR reader adopts native edits without FAR repacking",
          "[far-layout][field-background]") {
  FarData data;
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  auto layout = Required(
      Content::ReadBackgroundLayout(data.Read(kLayoutFile), kLayoutFile));
  layout.cells.front().colors.front().red = 42;
  const auto text = Required(Content::WriteBackgroundLayout(layout));
  data.Write(kLayoutFile, BytesOf(text));
  REQUIRE(Content::DataStore::Open(data.root));
  const auto* store = Content::DataStore::Get();
  REQUIRE(store != nullptr);
  const auto* far = store->Find("BG/001OP01A.FAR");
  const auto* tim = store->Find("BG/001OP01A.TIM");
  REQUIRE(far != nullptr);
  REQUIRE(tim != nullptr);
  VS::Field::LiveBackgroundHost host;
  const auto loaded = Required(host.LoadBackgroundLayout(Section(*far, *tim)));
  REQUIRE(loaded);
  CHECK(loaded->tiles.front().corners.front().red == 42);
  CHECK(loaded->tiles.front().texture.width == -4);
  CHECK(data.Read("disc/BG/001OP01A.FAR") == FarBytes());
}

namespace {
void VerifyCorpusEntry(const Content::DataStore& store,
                       const Content::DataStore::Entry& entry) {
  CAPTURE(entry.path);
  const auto bytes = Required(Extract::ReadSource(store.Root(), entry.path));
  const std::string tim_path =
      entry.path.substr(0, entry.path.size() - 4) + ".TIM";
  const auto* tim = store.Find(tim_path);
  REQUIRE(tim != nullptr);
  auto source = Required(Content::FindConvertedSource(
      store, {.lba = entry.lba,
              .rounded_size = std::uint64_t{entry.SectorCount()} * 2048,
              .extension = ".FAR",
              .converter = Content::kFarConverterName}));
  const auto image_source = Required(Content::FindConvertedSource(
      store, {.lba = tim->lba,
              .rounded_size = std::uint64_t{tim->SectorCount()} * 2048,
              .extension = ".TIM",
              .converter = "tim-png"}));
  const auto image = Required(Content::LoadConvertedImage(store, image_source));
  const auto document =
      Required(Content::LoadConvertedBackground(store, source, image));
  CHECK(Required(Content::EncodeFar(document)) == bytes.bytes);
  VS::Field::LiveBackgroundHost host;
  const auto loaded = Required(host.LoadBackgroundLayout(Section(entry, *tim)));
  REQUIRE(loaded);
  CheckLoadedCells(document, *loaded);
}
}  // namespace

TEST_CASE(
    "Configured FAR corpus rebuilds exactly and loads through the live reader",
    "[far-layout][extract]") {
  const fs::path root = fs::current_path() / "Native" / "data";
  Content::DataStore::ScopedOverride store_scope;
  if (!fs::exists(root / "disc" / "BG")) SKIP("configured disc data absent");
  REQUIRE(Content::DataStore::Open(root));
  const auto* store = Content::DataStore::Get();
  REQUIRE(store != nullptr);
  std::size_t count = 0;
  for (const auto& entry : store->Entries()) {
    if (!entry.path.ends_with(".FAR")) continue;
    VerifyCorpusEntry(*store, entry);
    ++count;
  }
  CHECK(count == 4);
}
