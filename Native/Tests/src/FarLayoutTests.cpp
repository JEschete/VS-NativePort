#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/BackgroundAssets.hpp"
#include "Content/BackgroundLayout.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Core/Sha256.hpp"
#include "Extract/BackgroundAuthoring.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/FarConversion.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Field/BackgroundLive.hpp"
#include "Platform/Paths.hpp"

namespace {
namespace Content = VS::Content;
namespace Extract = VS::Extract;
namespace fs = std::filesystem;
using Bytes = std::vector<std::byte>;
constexpr auto kLayoutFile = "backgrounds/castle-silhouette.background.json";

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

Bytes LegacyJson(Bytes bytes, const std::string& name, const std::string& id,
                 bool layout) {
  auto json = Required(Content::ParseJsonText(bytes, name));
  json["id"] = id;
  if (layout) {
    json["schema_version"] = 1;
    json.erase("cell_size_units");
    for (const auto& known : Content::BackgroundAssets()) {
      if (json["image_id"] == Content::BackgroundImageId(known)) {
        json["image_id"] = Content::LegacyBackgroundImageId(known);
      }
    }
  }
  const auto text = Content::FormatJsonText(json) + "\n";
  return {BytesOf(text).begin(), BytesOf(text).end()};
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
  void AddSecondImage() const {
    auto image = TimBytes();
    image[22] = std::byte{31};
    image[544] = std::byte{1};
    image[545] = std::byte{1};
    Write("disc/BG/002OP01A.FAR", FarBytes());
    Write("disc/BG/002OP01A.TIM", image);
    const std::string manifest =
        "lba\tsize\tform\tpath\n100\t36\traw\tBG/001OP01A.FAR\n"
        "101\t608\traw\tBG/001OP01A.TIM\n102\t36\traw\tBG/002OP01A.FAR\n"
        "103\t608\traw\tBG/002OP01A.TIM\n";
    Write("files.tsv", BytesOf(manifest));
    Required(Extract::ConvertDiscImages(root, false));
  }
  Content::ConversionManifest Manifest() const {
    auto manifest = Required(Content::LoadConversionManifest(root));
    if (!manifest) throw std::runtime_error("fixture manifest missing");
    return std::move(*manifest);
  }
  void BindImage(const std::string& id) const {
    auto json =
        Required(Content::ParseJsonText(Read(kLayoutFile), kLayoutFile));
    json["image_id"] = id;
    const auto text = Content::FormatJsonText(json) + "\n";
    Write(kLayoutFile, BytesOf(text));
  }
  void MoveLegacyFiles(Content::ConversionOutput& output,
                       const Content::BackgroundAssetIdentity& asset,
                       bool layout) const {
    for (auto& file : output.files) {
      const auto original = file;
      const auto semantic = layout ? Content::BackgroundLayoutPath(asset)
                                   : Content::BackgroundImageStemPath(asset);
      const auto legacy =
          layout ? "backgrounds/" + std::string(asset.source_stem) +
                       ".background.json"
                 : "textures/bg/" + std::string(asset.source_stem);
      file = legacy + original.substr(semantic.size());
      auto bytes = Read(original);
      if (file.ends_with(".json")) {
        bytes = LegacyJson(std::move(bytes), original, output.id, layout);
      }
      Write(file, bytes);
      fs::remove(root / VS::Platform::Utf8Path(original));
    }
  }
  void MakeLegacyFiles() const {
    auto manifest = Manifest();
    std::erase_if(manifest.outputs,
                  [](const auto& output) { return !output.alias_of.empty(); });
    for (auto& output : manifest.outputs) {
      const auto* asset = Content::BackgroundAssetForSource(output.source);
      if (asset == nullptr) continue;
      const bool layout = output.converter == Content::kFarConverterName;
      output.id = layout ? Content::LegacyBackgroundLayoutId(*asset)
                         : Content::LegacyBackgroundImageId(*asset);
      MoveLegacyFiles(output, *asset, layout);
    }
    manifest.converters["tim-png"] = 1;
    manifest.converters["far-layout-json"] = 1;
    const auto written = Extract::WriteConversionManifest(root, manifest);
    if (!written) throw std::runtime_error(written.error());
  }
  void AddImageAlias(const std::string& id, const std::string& target) const {
    auto manifest = Manifest();
    const auto* output = manifest.Find(target);
    if (output == nullptr) throw std::runtime_error("fixture image missing");
    auto alias = *output;
    alias.id = id;
    alias.alias_of = target;
    alias.files.clear();
    manifest.outputs.push_back(alias);
    const auto written = Extract::WriteConversionManifest(root, manifest);
    if (!written) throw std::runtime_error(written.error());
  }
  void EditFirstPixel(const std::string& stem) const {
    auto image =
        Required(Content::LoadImageFiles(root / "textures" / "bg", stem));
    image.image.indices.front() = 7;
    const auto files = Required(Extract::EncodeImageFiles(image, stem));
    const auto written =
        Extract::WriteImageFiles(root / "textures" / "bg", files);
    if (!written) throw std::runtime_error(written.error());
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

using LayoutPtr = std::shared_ptr<const VS::Field::OwnedBackgroundLayout>;

std::expected<LayoutPtr, std::string> LoadFixtureBackground(
    const FarData& data) {
  if (auto opened = Content::DataStore::Open(data.root); !opened) {
    return std::unexpected(opened.error());
  }
  const auto* store = Content::DataStore::Get();
  if (store == nullptr) return std::unexpected("fixture store missing");
  const auto* far = store->Find("BG/001OP01A.FAR");
  if (far == nullptr) return std::unexpected("fixture FAR missing");
  VS::Field::LiveBackgroundHost host;
  return host.LoadBackgroundLayout(
      {.texture_page_count = 65535,
       .texture = {},
       .layout = {.lba = static_cast<std::int32_t>(far->lba), .size = 2048}});
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
  const auto image = Required(Content::LoadImageFiles(
      data.root / "textures" / "bg", "castle-silhouette"));
  layout.image_id = "image.missing";
  CHECK_FALSE(
      Content::LoadBackgroundLayoutImage(data.root, data.Manifest(), layout));
  layout.image_id = image.id;
  layout.cells.front().right = 8;
  CHECK_FALSE(Content::ValidateBackgroundImage(layout, image));
  const auto text = Required(Content::WriteBackgroundLayout(layout));
  data.Write(kLayoutFile, BytesOf(text));
  CHECK_FALSE(Extract::ConvertDiscBackgrounds(data.root, false));
}

TEST_CASE("Authored FAR image IDs select a second image without an MPD veto",
          "[far-layout][far-rebinding]") {
  FarData data;
  data.AddSecondImage();
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  data.BindImage("image.bg.002op01a");
  const auto authored = data.Read(kLayoutFile);
  CHECK(Required(Extract::ConvertDiscBackgrounds(data.root, false)).unchanged ==
        2);
  CHECK(data.Read(kLayoutFile) == authored);
  fs::remove(data.root / "disc" / "BG" / "001OP01A.TIM");
  const auto loaded = Required(LoadFixtureBackground(data));
  REQUIRE(loaded);
  CHECK(loaded->image_id == "image.bg.002op01a");
  CHECK(loaded->image.pixel_words.front() == 257);
  CHECK(loaded->image.palette_words[1] == 31);
  const auto document =
      Required(Content::ReadBackgroundLayout(authored, kLayoutFile));
  CHECK(document.image_source == "BG/001OP01A.TIM");
  CHECK(data.Read("disc/BG/001OP01A.FAR") == FarBytes());
}

TEST_CASE("FAR binding refuses missing, malformed and non-image identities",
          "[far-layout][far-rebinding]") {
  FarData data;
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  for (const auto& id :
       {"image.missing", "../invalid", "background.import.bg.001op01a"}) {
    CAPTURE(id);
    data.BindImage(id);
    CHECK_FALSE(LoadFixtureBackground(data));
  }
  data.BindImage("image.bg.001op01a");
  fs::remove(data.root / "textures" / "bg" / "castle-silhouette.png");
  CHECK_FALSE(LoadFixtureBackground(data));
}

TEST_CASE(
    "FAR bindings validate image material and retain owned reload snapshots",
    "[far-layout][far-rebinding]") {
  FarData data;
  data.AddSecondImage();
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  const auto first = Required(LoadFixtureBackground(data));
  data.BindImage("image.bg.002op01a");
  const auto second = Required(LoadFixtureBackground(data));
  REQUIRE(first);
  REQUIRE(second);
  CHECK(first->image.pixel_words.front() == 0);
  CHECK(second->image.pixel_words.front() == 257);
  auto image = Required(
      Content::LoadImageFiles(data.root / "textures" / "bg", "dark-clouds"));
  image.material.reset();
  const auto files = Required(Extract::EncodeImageFiles(image, "dark-clouds"));
  REQUIRE(Extract::WriteImageFiles(data.root / "textures" / "bg", files));
  CHECK_FALSE(LoadFixtureBackground(data));
  CHECK(second->image.palette_words[1] == 31);
}

TEST_CASE("FAR explicit image aliases can select unreferenced native artwork",
          "[far-layout][far-rebinding]") {
  FarData data;
  data.AddSecondImage();
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  auto image = Required(
      Content::LoadImageFiles(data.root / "textures" / "bg", "dark-clouds"));
  image.unreferenced = true;
  const auto files = Required(Extract::EncodeImageFiles(image, "dark-clouds"));
  REQUIRE(Extract::WriteImageFiles(data.root / "textures" / "bg", files));
  auto manifest = data.Manifest();
  auto output = std::ranges::find(manifest.outputs, image.id,
                                  &Content::ConversionOutput::id);
  REQUIRE(output != manifest.outputs.end());
  output->unreferenced = true;
  auto alias = *output;
  alias.id = "image.test.explicit-alias";
  alias.files.clear();
  alias.dependencies.clear();
  alias.alias_of = image.id;
  manifest.outputs.push_back(alias);
  REQUIRE(Extract::WriteConversionManifest(data.root, manifest));
  data.BindImage(alias.id);
  const auto loaded = Required(LoadFixtureBackground(data));
  REQUIRE(loaded);
  CHECK(loaded->image_id == alias.id);
  CHECK(loaded->image.palette_words == image.image.palettes.front());
  fs::remove(data.root / "disc" / "BG" / "002OP01A.FAR");
  CHECK_FALSE(LoadFixtureBackground(data));
}

TEST_CASE("Semantic FAR migration preserves edits and custom image references",
          "[far-layout][far-authoring]") {
  FarData data;
  data.AddSecondImage();
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  data.MakeLegacyFiles();
  data.EditFirstPixel("001OP01A");
  constexpr auto kLegacy = "backgrounds/001OP01A.background.json";
  auto json = Required(Content::ParseJsonText(data.Read(kLegacy), kLegacy));
  json["display_name"] = "My edited silhouette";
  json["cells"][0]["corner_colors"]["top_left"]["red"] = 42;
  json["image_id"] = "image.custom.cloud";
  const auto text = Content::FormatJsonText(json) + "\n";
  data.Write(kLegacy, BytesOf(text));
  data.AddImageAlias("image.custom.cloud", "image.bg.002op01a");
  const auto original_png = data.Read("textures/bg/001OP01A.png");
  CHECK(Required(Extract::UpgradeBackgroundAuthoring(data.root)).identities ==
        4);
  const auto migrated =
      Required(Content::ParseJsonText(data.Read(kLayoutFile), kLayoutFile));
  CHECK(migrated["display_name"] == "My edited silhouette");
  CHECK(migrated["cells"][0]["corner_colors"]["top_left"]["red"] == 42);
  CHECK(migrated["image_id"] == "image.custom.cloud");
  CHECK(data.Read("textures/bg/castle-silhouette.png") == original_png);
  CHECK(Required(Extract::ConvertDiscImages(data.root, false)).unchanged == 2);
  CHECK(Required(Extract::ConvertDiscBackgrounds(data.root, false)).unchanged ==
        2);
  const auto loaded = Required(LoadFixtureBackground(data));
  CHECK(loaded->image_id == "image.custom.cloud");
}

TEST_CASE("Semantic FAR aliases and repeated conversion keep one owned output",
          "[far-layout][far-authoring]") {
  FarData data;
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  data.MakeLegacyFiles();
  Required(Extract::UpgradeBackgroundAuthoring(data.root));
  const auto manifest = data.Manifest();
  const auto* old = manifest.Find("background.import.bg.001op01a");
  const auto* canonical = manifest.Find("background.castle-silhouette");
  REQUIRE(old != nullptr);
  REQUIRE(canonical != nullptr);
  CHECK(manifest.FilesOf(*old) == canonical);
  const auto before = data.Read("manifest.json");
  const auto layout = data.Read(kLayoutFile);
  CHECK(Required(Extract::ConvertDiscImages(data.root, false)).unchanged == 1);
  CHECK(Required(Extract::ConvertDiscBackgrounds(data.root, false)).unchanged ==
        1);
  CHECK(data.Read("manifest.json") == before);
  CHECK(data.Read(kLayoutFile) == layout);
  CHECK(Required(Extract::UpgradeBackgroundAuthoring(data.root)).files == 0);
}

TEST_CASE("Semantic FAR migration refuses destination and identity collisions",
          "[far-layout][far-authoring]") {
  FarData data;
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  data.MakeLegacyFiles();
  const auto before = data.Read("manifest.json");
  SECTION("different authored destination") {
    const Bytes conflict{std::byte{1}, std::byte{2}};
    data.Write("textures/bg/castle-silhouette.png", conflict);
    CHECK_FALSE(Extract::UpgradeBackgroundAuthoring(data.root));
    CHECK(data.Read("textures/bg/castle-silhouette.png") == conflict);
  }
  SECTION("semantic ID belongs to another source") {
    auto manifest = data.Manifest();
    auto conflict = manifest.outputs.front();
    conflict.id = "image.bg.castle-silhouette";
    conflict.source = "OTHER.TIM";
    manifest.outputs.push_back(conflict);
    REQUIRE(Extract::WriteConversionManifest(data.root, manifest));
    const auto collision = data.Read("manifest.json");
    CHECK_FALSE(Extract::UpgradeBackgroundAuthoring(data.root));
    CHECK(data.Read("manifest.json") == collision);
    return;
  }
  CHECK(data.Read("manifest.json") == before);
  CHECK(fs::exists(data.root / "textures" / "bg" / "001OP01A.png"));
}

TEST_CASE(
    "A native FAR layout can use cell geometry the legacy format cannot encode",
    "[far-layout][far-authoring]") {
  FarData data;
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  auto layout = Required(
      Content::ReadBackgroundLayout(data.Read(kLayoutFile), kLayoutFile));
  layout.cell_size_units = 16;
  const auto text = Required(Content::WriteBackgroundLayout(layout));
  data.Write(kLayoutFile, BytesOf(text));
  CHECK_FALSE(Content::EncodeFar(layout));
  const auto loaded = Required(LoadFixtureBackground(data));
  CHECK(loaded->cell_size_units == 16);
  CHECK(Required(Extract::ConvertDiscBackgrounds(data.root, false)).unchanged ==
        1);
  CHECK(data.Read(kLayoutFile) ==
        Bytes(BytesOf(text).begin(), BytesOf(text).end()));
}

TEST_CASE(
    "Semantic FAR conversion preserves custom filenames and explicit old "
    "aliases",
    "[far-layout][far-authoring]") {
  FarData data;
  Required(Extract::ConvertDiscBackgrounds(data.root, false));
  data.BindImage("image.bg.001op01a");
  const auto authored = data.Read(kLayoutFile);
  constexpr auto kCustom = "backgrounds/my-authored-layout.background.json";
  data.Write(kCustom, authored);
  fs::remove(data.root / VS::Platform::Utf8Path(kLayoutFile));
  auto manifest = data.Manifest();
  for (auto& output : manifest.outputs) {
    if (output.id == "background.castle-silhouette") output.files = {kCustom};
  }
  REQUIRE(Extract::WriteConversionManifest(data.root, manifest));
  CHECK(Required(Extract::ConvertDiscBackgrounds(data.root, false)).unchanged ==
        1);
  CHECK(data.Read(kCustom) == authored);
  CHECK_FALSE(fs::exists(data.root / VS::Platform::Utf8Path(kLayoutFile)));
  const auto loaded = Required(LoadFixtureBackground(data));
  CHECK(loaded->image_id == "image.bg.001op01a");
}

TEST_CASE("Native background geometry enforces explicit renderer budgets",
          "[far-layout][far-authoring]") {
  auto layout = Required(Content::DecodeFar(FarBytes(), Identity()));
  layout.columns = 64;
  layout.rows = 16;
  layout.cells.resize(1024, layout.cells.front());
  const auto text = Required(Content::WriteBackgroundLayout(layout));
  CHECK(Required(Content::ReadBackgroundLayout(BytesOf(text), "fixture")) ==
        layout);
  CHECK_FALSE(Content::EncodeFar(layout));
  SECTION("total cell budget") {
    layout.rows = 17;
    layout.cells.resize(1088, layout.cells.front());
  }
  SECTION("side budget") {
    layout.columns = 65;
    layout.rows = 1;
    layout.cells.resize(65);
  }
  SECTION("unsupported cell size") { layout.cell_size_units = 24; }
  CHECK_FALSE(Content::WriteBackgroundLayout(layout));
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
  const auto document =
      Required(Content::LoadConvertedBackground(store, source));
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
  REQUIRE(Content::DataStore::OpenShared(root));
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
