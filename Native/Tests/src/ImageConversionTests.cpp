#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/TimFile.hpp"
#include "Core/Sha256.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Field/BackgroundLive.hpp"

namespace {
namespace Content = VS::Content;
namespace Extract = VS::Extract;
namespace fs = std::filesystem;
using Bytes = std::vector<std::byte>;
using DataFiles = std::map<std::string, Bytes, std::less<>>;
constexpr std::size_t kFbcPaletteEntries = 256;
constexpr std::size_t kFbcRowBytes = kFbcPaletteEntries * sizeof(std::uint16_t);

void Put16(Bytes& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::byte>(value & 0xFFU));
  bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void Put32(Bytes& bytes, std::uint32_t value) {
  Put16(bytes, static_cast<std::uint16_t>(value & 0xFFFFU));
  Put16(bytes, static_cast<std::uint16_t>(value >> 16U));
}

std::uint16_t Read16(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(
      std::to_integer<std::uint32_t>(bytes[offset]) |
      (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 8U));
}

std::uint32_t Read32(std::span<const std::byte> bytes, std::size_t offset) {
  return std::to_integer<std::uint32_t>(bytes[offset]) |
         (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 8U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 16U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 3]) << 24U);
}

Bytes TimBytes() {
  Bytes bytes;
  Put32(bytes, 0x10);
  Put32(bytes, 0x9);
  Put32(bytes, 12 + (256 * 2));
  Put16(bytes, 0);
  Put16(bytes, 480);
  Put16(bytes, 256);
  Put16(bytes, 1);
  for (std::uint16_t word = 0; word < 256; ++word) Put16(bytes, word);
  Put32(bytes, 12 + (4 * 2));
  Put16(bytes, 512);
  Put16(bytes, 0);
  Put16(bytes, 4);
  Put16(bytes, 1);
  for (std::uint16_t word = 0; word < 4; ++word) Put16(bytes, word);
  return bytes;
}

Bytes FbcBytes(std::size_t rows) {
  Bytes bytes;
  for (std::size_t index = 0; index < rows * kFbcPaletteEntries; ++index) {
    std::uint16_t word = static_cast<std::uint16_t>(index * 37U);
    if (index == 0) word = 0x0000;
    if (index == 1) word = 0x8000;
    if (index == 2) word = 0x8001;
    if (index == 3) word = 0x7FFF;
    Put16(bytes, word);
  }
  return bytes;
}

Bytes LayoutBytes(std::uint8_t depth_code = 1, std::uint16_t tile_height = 1,
                  std::uint32_t unknown = 0) {
  Bytes bytes;
  Put32(bytes, 1);
  Put32(bytes, 1);
  Put32(bytes, unknown);
  bytes.resize(36);
  bytes[16] = std::byte{1};
  bytes[18] = static_cast<std::byte>(tile_height & 0xFFU);
  bytes[19] = static_cast<std::byte>(tile_height >> 8U);
  bytes[27] = static_cast<std::byte>(depth_code);
  return bytes;
}

auto ConvertBackgroundTim(const Bytes& tim, const Bytes& layout) {
  return Extract::ConvertTim(
      {.tim = {.path = "BG/001OP01A.TIM", .bytes = tim},
       .layout =
           Extract::SourceFile{.path = "BG/001OP01A.FAR", .bytes = layout},
       .material =
           Content::ImageMaterial{
               .zero_word = Content::ZeroWord::kTransparent,
               .semi_transparency = Content::SemiTransparency::kOff},
       .unreferenced = false,
       .note = {}});
}

Bytes LayoutWithSkippedTile() {
  Bytes bytes = LayoutBytes(0, 0);
  bytes[0] = std::byte{2};
  std::ranges::fill(bytes.begin() + 16, bytes.begin() + 20, std::byte{});
  bytes.insert(bytes.end(), 24, std::byte{});
  bytes[36 + 4] = std::byte{1};
  bytes[36 + 6] = std::byte{1};
  bytes[36 + 15] = std::byte{1};
  return bytes;
}

Bytes Direct24TimBytes() {
  Bytes bytes;
  Put32(bytes, 0x10);
  Put32(bytes, 3);
  Put32(bytes, 16);
  Put16(bytes, 0);
  Put16(bytes, 0);
  Put16(bytes, 2);
  Put16(bytes, 1);
  Put16(bytes, 0x1234);
  Put16(bytes, 0x5678);
  return bytes;
}

class TemporaryData {
 public:
  TemporaryData() {
    root_ = fs::temp_directory_path() /
            ("vs-image-conversion-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root_ / "disc");
  }

  // NOLINTNEXTLINE(bugprone-exception-escape): errors use the error_code API.
  ~TemporaryData() {
    std::error_code error;
    fs::remove_all(root_, error);
    if (error) {
      std::fprintf(stderr, "image conversion fixture left files: %s\n",
                   error.message().c_str());
    }
  }

  TemporaryData(const TemporaryData&) = delete;
  TemporaryData& operator=(const TemporaryData&) = delete;

  const fs::path& Root() const { return root_; }

  void Write(std::string_view path, std::span<const std::byte> bytes) const {
    const fs::path file = root_ / "disc" / fs::path(path);
    fs::create_directories(file.parent_path());
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    REQUIRE(output.good());
  }

  void WriteManifest(const DataFiles& files) const {
    std::ofstream output(root_ / Content::DataStore::ManifestName);
    output << "lba\tsize\tform\tpath\n";
    std::uint32_t lba = 100;
    for (const auto& [path, bytes] : files) {
      output << lba << '\t' << bytes.size() << "\traw\t" << path << '\n';
      lba += static_cast<std::uint32_t>((bytes.size() + 2047) / 2048);
      Write(path, bytes);
    }
    REQUIRE(output.good());
  }

 private:
  fs::path root_;
};

Bytes ReadBytes(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  const std::vector<char> chars{std::istreambuf_iterator<char>(input),
                                std::istreambuf_iterator<char>()};
  Bytes bytes(chars.size());
  for (std::size_t index = 0; index < chars.size(); ++index) {
    bytes[index] =
        static_cast<std::byte>(static_cast<unsigned char>(chars[index]));
  }
  return bytes;
}

Content::ConversionManifest RequiredManifest(const fs::path& root) {
  auto manifest = Content::LoadConversionManifest(root);
  REQUIRE(manifest.has_value());
  REQUIRE(manifest->has_value());
  return std::move(**manifest);
}

DataFiles BackgroundTimFiles() {
  return {{"BG/001OP01A.FAR", LayoutBytes()}, {"BG/001OP01A.TIM", TimBytes()}};
}

const Content::DataStore::Entry* EntryForSector(
    const std::vector<Content::DataStore::Entry>& entries,
    std::uint32_t sector) {
  const auto found = std::ranges::find_if(entries, [sector](const auto& entry) {
    return entry.ContainsSector(sector);
  });
  return found == entries.end() ? nullptr : &*found;
}

struct BackgroundSectors {
  std::uint32_t tim;
  std::uint32_t layout;
};

std::optional<BackgroundSectors> BackgroundSectorsIn(const Bytes& mpd) {
  constexpr std::size_t kRoomSectionLengthsBytes = 0x60;
  constexpr std::size_t kTiledBackgroundSection = 15;
  constexpr std::size_t kTiledTextureFileAt = 0x40;
  constexpr std::size_t kTiledLayoutFileAt = 0x84;
  constexpr std::size_t kTiledSectionBytes = kTiledLayoutFileAt + 8;
  REQUIRE(mpd.size() >= 4);
  const std::uint32_t room_offset = Read32(mpd, 0);
  if (room_offset == 0) return std::nullopt;
  REQUIRE(room_offset <= mpd.size());
  REQUIRE(mpd.size() - room_offset >= kRoomSectionLengthsBytes);
  std::array<std::uint32_t, 24> lengths{};
  for (std::size_t index = 0; index < lengths.size(); ++index) {
    lengths[index] = Read32(mpd, room_offset + (index * sizeof(std::uint32_t)));
  }
  if (lengths[kTiledBackgroundSection] == 0) return std::nullopt;
  std::uint64_t tiled_offset = room_offset + kRoomSectionLengthsBytes;
  for (std::size_t index = 0; index < kTiledBackgroundSection; ++index) {
    tiled_offset += lengths[index];
  }
  REQUIRE(tiled_offset <= mpd.size());
  REQUIRE(mpd.size() - tiled_offset >= kTiledSectionBytes);
  return BackgroundSectors{
      .tim = Read32(mpd, tiled_offset + kTiledTextureFileAt),
      .layout = Read32(mpd, tiled_offset + kTiledLayoutFileAt)};
}

std::set<std::string> ReferencedBackgroundTims(
    const fs::path& source_root,
    const std::vector<Content::DataStore::Entry>& entries,
    const Content::ConversionManifest& manifest) {
  std::set<std::string> referenced;
  for (const auto& entry : entries) {
    if (entry.form != "raw" || !entry.path.ends_with(".MPD")) continue;
    const Bytes mpd = ReadBytes(source_root / "disc" / fs::path(entry.path));
    const auto sectors = BackgroundSectorsIn(mpd);
    if (!sectors) continue;
    const auto* tim = EntryForSector(entries, sectors->tim);
    const auto* layout = EntryForSector(entries, sectors->layout);
    REQUIRE(tim != nullptr);
    REQUIRE(layout != nullptr);
    REQUIRE(tim->path.ends_with(".TIM"));
    REQUIRE(layout->path.ends_with(".FAR"));
    const std::string expected_layout =
        tim->path.substr(0, tim->path.size() - 3) + "FAR";
    CHECK(layout->path == expected_layout);
    auto image_id = Extract::ImageIdForDiscPath(tim->path);
    REQUIRE(image_id.has_value());
    CHECK(manifest.Find(*image_id) != nullptr);
    referenced.insert(tim->path);
  }
  return referenced;
}

DataFiles TimCorpusInputs(
    const fs::path& source_root,
    const std::vector<Content::DataStore::Entry>& entries) {
  DataFiles input_files;
  for (const auto& entry : entries) {
    if (entry.form != "raw" || !entry.path.ends_with(".TIM")) continue;
    const Bytes tim = ReadBytes(source_root / "disc" / fs::path(entry.path));
    if (tim.empty()) SKIP("An extracted TIM source file is unavailable");
    input_files.emplace(entry.path, tim);
    if (!entry.path.starts_with("BG/")) continue;
    const std::string far = entry.path.substr(0, entry.path.size() - 3) + "FAR";
    const Bytes layout = ReadBytes(source_root / "disc" / fs::path(far));
    if (layout.empty()) SKIP("An extracted background layout is unavailable");
    input_files.emplace(far, layout);
  }
  if (input_files.empty()) SKIP("The extracted data has no standalone TIMs");
  return input_files;
}

DataFiles::const_iterator FirstBackgroundTim(const DataFiles& input_files) {
  return std::ranges::find_if(input_files, [](const auto& item) {
    return item.first.starts_with("BG/") && item.first.ends_with(".TIM");
  });
}

VS::Field::BackgroundFile BackgroundFileForEntry(
    const Content::DataStore::Entry& entry) {
  const auto rounded_size = static_cast<std::uint64_t>(entry.SectorCount()) *
                            Content::DataStore::kDataSectorBytes;
  return {.lba = static_cast<std::int32_t>(entry.lba),
          .size = static_cast<std::int32_t>(rounded_size)};
}

void VerifyCorpusTimRoundTrips(const fs::path& converted_root,
                               const DataFiles& input_files,
                               const Content::ConversionManifest& manifest) {
  for (const auto& [path, source] : input_files) {
    if (!path.ends_with(".TIM")) continue;
    const auto location = Extract::ImageLocationForDiscPath(path);
    REQUIRE(location.has_value());
    auto image = Content::LoadImageFiles(converted_root / location->directory,
                                         location->stem);
    REQUIRE(image.has_value());
    REQUIRE(image->provenance.tim.has_value());
    auto tim = Extract::TimFromImage(image->image, *image->provenance.tim);
    REQUIRE(tim.has_value());
    auto rebuilt = Extract::EncodeTim(*tim);
    REQUIRE(rebuilt.has_value());
    CHECK(*rebuilt == source);
    CHECK(manifest.Find(image->id) != nullptr);
  }
}

std::uint16_t EditAuthoredPalette(const fs::path& directory,
                                  std::string_view stem) {
  auto authored = Content::LoadImageFiles(directory, stem);
  REQUIRE(authored.has_value());
  REQUIRE_FALSE(authored->image.palettes.empty());
  REQUIRE_FALSE(authored->image.palettes.front().empty());
  const std::uint16_t edited_word = static_cast<std::uint16_t>(
      authored->image.palettes.front().front() ^ std::uint16_t{1});
  authored->image.palettes.front().front() = edited_word;
  auto files = Extract::EncodeImageFiles(*authored, stem);
  REQUIRE(files.has_value());
  REQUIRE(Extract::WriteImageFiles(directory, *files).has_value());
  return edited_word;
}

void VerifyBackgroundPaletteReload(const fs::path& source_root,
                                   const fs::path& converted_root,
                                   const DataFiles::value_type& background,
                                   const Content::DataStore::Entry& entry,
                                   VS::Field::LiveBackgroundHost& host) {
  auto texture = host.LoadBackgroundTexture(BackgroundFileForEntry(entry));
  REQUIRE(texture.has_value());
  CHECK(texture->pixel_words.size() ==
        (static_cast<std::size_t>(texture->source_rect.width) *
         static_cast<std::size_t>(texture->source_rect.height)));
  CHECK(texture->palette_words.size() == 256);
  const auto location = Extract::ImageLocationForDiscPath(background.first);
  REQUIRE(location.has_value());
  const fs::path directory = converted_root / location->directory;
  const std::uint16_t edited_word =
      EditAuthoredPalette(directory, location->stem);
  auto reloaded = host.LoadBackgroundTexture(BackgroundFileForEntry(entry));
  REQUIRE(reloaded.has_value());
  CHECK(reloaded->palette_words.front() == edited_word);
  CHECK(ReadBytes(source_root / "disc" / fs::path(background.first)) ==
        background.second);
  auto wrong_size = BackgroundFileForEntry(entry);
  wrong_size.size = static_cast<std::int32_t>(entry.size);
  auto rejected = host.LoadBackgroundTexture(wrong_size);
  REQUIRE_FALSE(rejected.has_value());
  CHECK(rejected.error().find("sector-rounded size") != std::string::npos);
}

void VerifyBackgroundTextureEdit(const fs::path& source_root,
                                 const TemporaryData& converted_data,
                                 const DataFiles& input_files) {
  Content::DataStore::ScopedOverride restore;
  REQUIRE(Content::DataStore::Open(converted_data.Root()).has_value());
  REQUIRE(Content::DataStore::Get() != nullptr);
  CHECK(Content::DataStore::Get()->RequireConversions().has_value());
  const auto background = FirstBackgroundTim(input_files);
  REQUIRE(background != input_files.end());
  const Content::DataStore::Entry* const entry =
      Content::DataStore::Get()->Find(background->first);
  REQUIRE(entry != nullptr);
  VS::Field::LiveBackgroundHost host;
  VerifyBackgroundPaletteReload(source_root, converted_data.Root(), *background,
                                *entry, host);
}

void VerifyStaleDependency(TemporaryData& converted_data,
                           const std::string& layout_path,
                           const Bytes& original_layout) {
  Bytes changed_layout = original_layout;
  changed_layout.front() ^= std::byte{1};
  converted_data.Write(layout_path, changed_layout);
  Content::DataStore::ScopedOverride restore;
  REQUIRE(Content::DataStore::Open(converted_data.Root()).has_value());
  const auto required = Content::DataStore::Get()->RequireConversions();
  REQUIRE_FALSE(required.has_value());
  CHECK(required.error().find("stale tim-png dependency") != std::string::npos);
}

void VerifyMissingConvertedOutput(const TemporaryData& converted_data,
                                  const Content::ConversionManifest& manifest) {
  REQUIRE_FALSE(manifest.outputs.empty());
  REQUIRE_FALSE(manifest.outputs.front().files.empty());
  fs::remove(converted_data.Root() /
             fs::path(manifest.outputs.front().files.front()));
  Content::DataStore::ScopedOverride restore;
  REQUIRE(Content::DataStore::Open(converted_data.Root()).has_value());
  const auto required = Content::DataStore::Get()->RequireConversions();
  REQUIRE_FALSE(required.has_value());
  INFO(required.error());
  CHECK(required.error().find("missing tim-png output") != std::string::npos);
}

void VerifyDependencyInvalidation(TemporaryData& converted_data,
                                  const DataFiles& input_files,
                                  const Content::ConversionManifest& manifest) {
  const auto background = FirstBackgroundTim(input_files);
  REQUIRE(background != input_files.end());
  const std::string layout_path =
      background->first.substr(0, background->first.size() - 3) + "FAR";
  const auto layout = input_files.find(layout_path);
  REQUIRE(layout != input_files.end());
  VerifyStaleDependency(converted_data, layout_path, layout->second);
  converted_data.Write(layout_path, layout->second);
  VerifyMissingConvertedOutput(converted_data, manifest);
}

Content::ImageFileReader ReaderFor(
    const std::vector<Content::ImageFile>& files) {
  return [&files](const std::string& name) {
    const auto found =
        std::ranges::find(files, name, &Content::ImageFile::name);
    return found == files.end() ? Content::ImageFileBytes{}
                                : Content::ImageFileBytes{found->bytes};
  };
}

void OverwriteFile(const fs::path& path, const Bytes& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  REQUIRE(output.good());
}

Extract::ImageConversionReport RequiredConversion(const fs::path& root,
                                                  bool force) {
  auto report = Extract::ConvertDiscImages(root, force);
  REQUIRE(report.has_value());
  return *report;
}

std::vector<std::uint16_t> FbcWords(const Bytes& source) {
  std::vector<std::uint16_t> words;
  for (std::size_t offset = 0; offset + 1 < source.size();
       offset += sizeof(std::uint16_t)) {
    words.push_back(Read16(source, offset));
  }
  return words;
}

// A converted FBC palette is one 256-entry direct-colour row per source row,
// holding exactly the source's colour words.
void CheckFbcImage(const Content::ImageDocument& decoded, const Bytes& source) {
  CHECK(decoded.image.depth == Content::ImageDepth::kDirect16);
  CHECK(decoded.image.width == kFbcPaletteEntries);
  CHECK(decoded.image.height == source.size() / kFbcRowBytes);
  CHECK(Content::PackedWords(decoded.image) == FbcWords(source));
}

DataFiles FbcCorpusInputs(
    const fs::path& source_root,
    const std::vector<Content::DataStore::Entry>& entries) {
  DataFiles inputs;
  for (const Content::DataStore::Entry& entry : entries) {
    if (entry.form != "raw" || !entry.path.ends_with(".FBC")) continue;
    const Bytes bytes = ReadBytes(source_root / "disc" / fs::path(entry.path));
    REQUIRE(bytes.size() == entry.size);
    inputs.emplace(entry.path, bytes);
  }
  return inputs;
}

void VerifyFbcCorpusOutput(const fs::path& converted_root,
                           const Content::ConversionManifest& manifest,
                           const std::string& path, const Bytes& source) {
  INFO(path);
  const auto id = Extract::ImageIdForDiscPath(path);
  REQUIRE(id.has_value());
  const auto* output = manifest.Find(*id + ".palette");
  REQUIRE(output != nullptr);
  CHECK(output->converter == Extract::kFbcPaletteConverterName);
  const auto location = Extract::ImageLocationForDiscPath(path);
  REQUIRE(location.has_value());
  const auto decoded = Content::LoadImageFiles(
      converted_root / "textures" / "effect" / "palettes", location->stem);
  REQUIRE(decoded.has_value());
  CHECK(decoded->provenance.source == path);
  CheckFbcImage(*decoded, source);
}

TEST_CASE("TIM image identifiers and paths are stable", "[content-images]") {
  CHECK(Extract::ImageIdForDiscPath("BG/001OP01A.TIM") == "image.bg.001op01a");
  CHECK(Extract::ImageIdForDiscPath("DBGFNT.TIM") == "image.root.dbgfnt");
  const Extract::ImageLocation expected_location{.directory = "textures/bg",
                                                 .stem = "001OP01A"};
  CHECK(Extract::ImageLocationForDiscPath("BG/001OP01A.TIM") ==
        expected_location);
  CHECK_FALSE(Extract::ImageIdForDiscPath("../bad.TIM").has_value());
  CHECK_FALSE(Extract::ImageLocationForDiscPath("BG/../bad.TIM").has_value());
}

TEST_CASE("background layout depth follows the rendered tiles",
          "[content-images]") {
  CHECK(Extract::BackgroundLayoutDepth(LayoutBytes()) ==
        Content::ImageDepth::kIndexed8);
  CHECK(Extract::BackgroundLayoutDepth(LayoutWithSkippedTile()) ==
        Content::ImageDepth::kIndexed8);
}

TEST_CASE("background layout rejects unsupported depth codes",
          "[content-images]") {
  CHECK_FALSE(Extract::BackgroundLayoutDepth(LayoutBytes(0)).has_value());
  CHECK_FALSE(Extract::BackgroundLayoutDepth(LayoutBytes(2)).has_value());
}

TEST_CASE("background layout rejects malformed extents", "[content-images]") {
  Bytes zero_width = LayoutBytes();
  std::ranges::fill(zero_width.begin(), zero_width.begin() + 4, std::byte{});
  CHECK_FALSE(Extract::BackgroundLayoutDepth(zero_width).has_value());
  Bytes zero_height = LayoutBytes();
  std::ranges::fill(zero_height.begin() + 4, zero_height.begin() + 8,
                    std::byte{});
  CHECK_FALSE(Extract::BackgroundLayoutDepth(zero_height).has_value());
  Bytes short_header(11);
  CHECK_FALSE(Extract::BackgroundLayoutDepth(short_header).has_value());
  Bytes no_draw = LayoutBytes(1, 0);
  no_draw[16] = std::byte{0};
  no_draw[17] = std::byte{0};
  CHECK_FALSE(Extract::BackgroundLayoutDepth(no_draw).has_value());
  Bytes wrong_size = LayoutBytes();
  wrong_size.push_back(std::byte{});
  CHECK_FALSE(Extract::BackgroundLayoutDepth(wrong_size).has_value());
}

TEST_CASE("TIM conversion records source and layout provenance",
          "[content-images]") {
  const Bytes tim = TimBytes();
  const Bytes layout = LayoutBytes();
  const std::string tim_hash = VS::Core::Sha256Hex(tim);
  const std::string layout_hash = VS::Core::Sha256Hex(layout);
  auto converted = ConvertBackgroundTim(tim, layout);
  REQUIRE(converted.has_value());
  CHECK(converted->output.id == "image.bg.001op01a");
  CHECK(converted->output.source_sha256 == tim_hash);
  REQUIRE(converted->output.dependencies.size() == 1);
  CHECK(converted->output.dependencies[0].source == "BG/001OP01A.FAR");
  CHECK(converted->output.dependencies[0].source_sha256 == layout_hash);
  const std::vector<std::string> expected_files{
      "textures/bg/001OP01A.png", "textures/bg/001OP01A.image.json"};
  CHECK(converted->output.files == expected_files);
}

TEST_CASE("TIM conversion emits native indexed image files",
          "[content-images]") {
  const Bytes tim = TimBytes();
  const Bytes layout = LayoutBytes();
  auto converted = ConvertBackgroundTim(tim, layout);
  REQUIRE(converted.has_value());
  auto image = Content::DecodeImageFiles(
      "001OP01A",
      [&converted](const std::string& name) -> std::optional<Bytes> {
        for (const auto& file : converted->files) {
          if (file.name == name) return file.bytes;
        }
        return std::nullopt;
      });
  REQUIRE(image.has_value());
  CHECK(image->image.depth == Content::ImageDepth::kIndexed8);
  CHECK(image->image.width == 8);
  const Content::ImageMaterial expected_material{
      .zero_word = Content::ZeroWord::kTransparent,
      .semi_transparency = Content::SemiTransparency::kOff};
  CHECK(image->material == expected_material);
}

TEST_CASE("TIM conversion rejects trailing and unsupported source data",
          "[content-images]") {
  Bytes trailing = TimBytes();
  const Bytes layout = LayoutBytes();
  trailing.push_back(std::byte{});
  auto trailing_result = Extract::ConvertTim(
      {.tim = {.path = "BG/001OP01A.TIM", .bytes = std::move(trailing)},
       .layout =
           Extract::SourceFile{.path = "BG/001OP01A.FAR", .bytes = layout},
       .material = std::nullopt,
       .unreferenced = false,
       .note = {}});
  CHECK_FALSE(trailing_result.has_value());
  auto direct24 = Extract::ConvertTim(
      {.tim = {.path = "ROOT/24BIT.TIM", .bytes = Direct24TimBytes()},
       .layout = std::nullopt,
       .material = std::nullopt,
       .unreferenced = false,
       .note = {}});
  CHECK_FALSE(direct24.has_value());
}

TEST_CASE("conversion manifests are strict and deterministic",
          "[content-images][conversion]") {
  const std::string hash(64, 'a');
  Content::ConversionManifest manifest;
  manifest.converters["tim-png"] = 1;
  manifest.outputs.push_back({.id = "image.test",
                              .source = "TEST/IMAGE.TIM",
                              .source_sha256 = hash,
                              .converter = "tim-png",
                              .files = {"textures/test/IMAGE.png"},
                              .dependencies = {},
                              .unreferenced = false,
                              .note = {}});
  const auto encoded = Extract::FormatConversionManifest(manifest);
  REQUIRE(encoded.has_value());
  const auto parsed = Content::ParseConversionManifest(
      std::as_bytes(std::span(encoded->data(), encoded->size())),
      "manifest.json");
  REQUIRE(parsed.has_value());
  CHECK(Extract::FormatConversionManifest(*parsed) == *encoded);

  std::string unknown = *encoded;
  unknown.insert(unknown.find('{') + 1, "\"unknown\":true,");
  CHECK_FALSE(Content::ParseConversionManifest(
                  std::as_bytes(std::span(unknown.data(), unknown.size())),
                  "manifest.json")
                  .has_value());
  std::string traversal = *encoded;
  traversal.replace(traversal.find("textures/test/IMAGE.png"),
                    std::string("textures/test/IMAGE.png").size(),
                    "../IMAGE.png");
  CHECK_FALSE(Content::ParseConversionManifest(
                  std::as_bytes(std::span(traversal.data(), traversal.size())),
                  "manifest.json")
                  .has_value());
  std::string future = *encoded;
  future.replace(future.find("\"schema_version\": 1"),
                 std::string("\"schema_version\": 1").size(),
                 "\"schema_version\": 2");
  CHECK_FALSE(Content::ParseConversionManifest(
                  std::as_bytes(std::span(future.data(), future.size())),
                  "manifest.json")
                  .has_value());

  manifest.outputs[0].dependencies = {
      {.source = "Z/FAR", .source_sha256 = hash},
      {.source = "A/FAR", .source_sha256 = hash}};
  CHECK_FALSE(Content::ValidateConversionManifest(manifest).has_value());
  manifest.outputs[0].dependencies.clear();
  manifest.outputs[0].source_sha256 = std::string(64, 'A');
  CHECK_FALSE(Content::ValidateConversionManifest(manifest).has_value());
}

TEST_CASE("required converter versions gate the loaded manifest",
          "[content-images][conversion]") {
  const std::array required{
      Content::ConverterVersion{.name = "tim-png", .version = 1}};
  Content::ConversionManifest manifest;
  CHECK_FALSE(Content::CheckRequiredConverters(manifest, required).has_value());
  manifest.converters["tim-png"] = 0;
  CHECK_FALSE(Content::CheckRequiredConverters(manifest, required).has_value());
  manifest.converters["tim-png"] = 1;
  CHECK(Content::CheckRequiredConverters(manifest, required).has_value());

  manifest.converters["other"] = 1;
  Extract::ReplaceConverterOutputs(manifest, "tim-png", 2,
                                   {{.id = "image.test",
                                     .source = "TEST/IMAGE.TIM",
                                     .source_sha256 = std::string(64, 'a'),
                                     .converter = "tim-png",
                                     .files = {"textures/test/IMAGE.png"},
                                     .dependencies = {},
                                     .unreferenced = false,
                                     .note = {}}});
  CHECK(manifest.converters["tim-png"] == 2);
  CHECK(manifest.converters.contains("other"));
  CHECK(manifest.outputs.size() == 1);
}

TEST_CASE("TIM conversion reuses unchanged outputs", "[content-images]") {
  TemporaryData data;
  const DataFiles files = BackgroundTimFiles();
  data.WriteManifest(files);

  const auto first = Extract::ConvertDiscImages(data.Root(), false);
  REQUIRE(first.has_value());
  CHECK(first->converted == 1);
  CHECK(first->unchanged == 0);
  const fs::path png = data.Root() / "textures" / "bg" / "001OP01A.png";
  const Bytes generated = ReadBytes(png);
  REQUIRE_FALSE(generated.empty());

  const auto second = Extract::ConvertDiscImages(data.Root(), false);
  REQUIRE(second.has_value());
  CHECK(second->converted == 0);
  CHECK(second->unchanged == 1);
}

TEST_CASE("FBC palettes round-trip as editable native images",
          "[content-images]") {
  const Bytes source = FbcBytes(2);
  const auto converted = Extract::ConvertFbcPalette(
      {.path = "EFFECT/E001_1.FBC", .bytes = source});
  REQUIRE(converted.has_value());
  CHECK(converted->output.id == "image.effect.e001_1.palette");
  CHECK(converted->output.converter == Extract::kFbcPaletteConverterName);
  CHECK(converted->location.directory == "textures/effect/palettes");
  CHECK(converted->location.stem == "E001_1");
  CHECK(std::ranges::find(converted->files, "E001_1.stp.png",
                          &Content::ImageFile::name) != converted->files.end());

  const auto decoded =
      Content::DecodeImageFiles("E001_1", ReaderFor(converted->files));
  REQUIRE(decoded.has_value());
  CHECK(decoded->provenance.source == "EFFECT/E001_1.FBC");
  CheckFbcImage(*decoded, source);
}

TEST_CASE("FBC conversion round-trips every 16-bit colour word",
          "[content-images]") {
  constexpr std::size_t kWordsPerFile = 5 * kFbcPaletteEntries;
  for (std::size_t first = 0; first <= 0xFFFFU; first += kWordsPerFile) {
    INFO("first word " << first);
    Bytes source;
    for (std::size_t index = 0; index < kWordsPerFile; ++index) {
      Put16(source, static_cast<std::uint16_t>((first + index) & 0xFFFFU));
    }
    const auto converted = Extract::ConvertFbcPalette(
        {.path = "EFFECT/E001_1.FBC", .bytes = source});
    REQUIRE(converted.has_value());
    const auto decoded =
        Content::DecodeImageFiles("E001_1", ReaderFor(converted->files));
    REQUIRE(decoded.has_value());
    REQUIRE(Content::PackedWords(decoded->image) == FbcWords(source));
  }
}

TEST_CASE("FBC palette conversion rejects incomplete or oversized rows",
          "[content-images]") {
  SECTION("partial row") {
    const Bytes source(kFbcRowBytes - 1, std::byte{0});
    const auto converted = Extract::ConvertFbcPalette(
        {.path = "EFFECT/E001_1.FBC", .bytes = source});
    REQUIRE_FALSE(converted.has_value());
    CHECK(converted.error().find("whole 512-byte rows") != std::string::npos);
  }
  SECTION("too many rows") {
    const Bytes source = FbcBytes(6);
    const auto converted = Extract::ConvertFbcPalette(
        {.path = "EFFECT/E001_1.FBC", .bytes = source});
    REQUIRE_FALSE(converted.has_value());
    CHECK(converted.error().find("at most 5") != std::string::npos);
  }
}

TEST_CASE("FBC conversion keeps unchanged palettes and edits until forced",
          "[content-images]") {
  TemporaryData data;
  data.WriteManifest({{"EFFECT/E001_1.FBC", FbcBytes(2)}});
  CHECK(RequiredConversion(data.Root(), false).converted == 1);
  const fs::path palette =
      data.Root() / "textures" / "effect" / "palettes" / "E001_1.png";
  const Bytes generated = ReadBytes(palette);
  REQUIRE_FALSE(generated.empty());
  const auto second = RequiredConversion(data.Root(), false);
  CHECK(second.converted == 0);
  CHECK(second.unchanged == 1);

  const Bytes edited{std::byte{0x01}, std::byte{0x02}};
  OverwriteFile(palette, edited);
  CHECK(RequiredConversion(data.Root(), false).unchanged == 1);
  CHECK(ReadBytes(palette) == edited);
  CHECK(RequiredConversion(data.Root(), true).converted == 1);
  CHECK(ReadBytes(palette) == generated);
}

TEST_CASE("FBC conversion removes the files of deleted sources",
          "[content-images]") {
  TemporaryData data;
  data.WriteManifest({{"EFFECT/E001_1.FBC", FbcBytes(2)}});
  REQUIRE(Extract::ConvertDiscImages(data.Root(), false).has_value());
  const auto manifest = RequiredManifest(data.Root());
  const auto* output = manifest.Find("image.effect.e001_1.palette");
  REQUIRE(output != nullptr);
  const fs::path palette =
      data.Root() / "textures" / "effect" / "palettes" / "E001_1.png";
  REQUIRE(fs::exists(palette));

  data.WriteManifest({});
  const auto removed = Extract::ConvertDiscImages(data.Root(), false);
  REQUIRE(removed.has_value());
  CHECK(removed->removed == output->files.size());
  CHECK_FALSE(fs::exists(palette));
}

// The startup conversion check on `data`'s store: empty, or its error.
std::string ConversionError(const TemporaryData& data) {
  Content::DataStore::ScopedOverride restore;
  REQUIRE(Content::DataStore::Open(data.Root()).has_value());
  const auto required = Content::DataStore::Get()->RequireConversions();
  return required ? std::string{} : required.error();
}

TEST_CASE("Data stores require each FBC's current converted palette",
          "[content-images]") {
  TemporaryData data;
  const Bytes fbc = FbcBytes(2);
  data.WriteManifest({{"EFFECT/E001_1.FBC", fbc}});
  REQUIRE(Extract::ConvertDiscImages(data.Root(), false).has_value());
  CHECK(ConversionError(data).empty());

  SECTION("a missing palette image") {
    fs::remove(data.Root() / "textures" / "effect" / "palettes" / "E001_1.png");
    CHECK(ConversionError(data).find("missing fbc-palette-png output") !=
          std::string::npos);
  }
  SECTION("a changed source") {
    Bytes changed = fbc;
    changed.front() ^= std::byte{1};
    data.Write("EFFECT/E001_1.FBC", changed);
    CHECK(ConversionError(data).find(
              "stale fbc-palette-png source hash for EFFECT/E001_1.FBC") !=
          std::string::npos);
  }
  SECTION("a source without a palette") {
    data.WriteManifest(
        {{"EFFECT/E001_1.FBC", fbc}, {"EFFECT/E002_1.FBC", fbc}});
    CHECK(ConversionError(data).find(
              "no fbc-palette-png output for EFFECT/E002_1.FBC") !=
          std::string::npos);
  }
}

// `data` with E001_1.FBC at sector 100 and its converted palette, opened.
struct ConvertedFbcStore {
  TemporaryData data;
  Bytes fbc = FbcBytes(2);
  Content::DataStore::ScopedOverride restore;

  ConvertedFbcStore() {
    data.WriteManifest({{"EFFECT/E001_1.FBC", fbc}});
    REQUIRE(Extract::ConvertDiscImages(data.Root(), false).has_value());
    REQUIRE(Content::DataStore::Open(data.Root()).has_value());
  }
};

constexpr VS::Effects::DiscRead kConvertedFbcRead{.lba = 100, .size = 2048};

TEST_CASE("Live effect palettes load the converted rows by sector",
          "[content-images]") {
  {
    Content::DataStore::ScopedOverride none;
    const auto unopened = VS::Effects::LoadLiveEffectPalette(kConvertedFbcRead);
    REQUIRE_FALSE(unopened.has_value());
    CHECK(unopened.error().find("no extracted-data store") !=
          std::string::npos);
  }
  ConvertedFbcStore store;
  const auto loaded = VS::Effects::LoadLiveEffectPalette(kConvertedFbcRead);
  INFO((loaded ? std::string{} : loaded.error()));
  REQUIRE(loaded.has_value());
  CHECK(*loaded == FbcWords(store.fbc));

  const auto exact_size = VS::Effects::LoadLiveEffectPalette(
      {.lba = 100, .size = static_cast<std::int32_t>(store.fbc.size())});
  REQUIRE_FALSE(exact_size.has_value());
  CHECK(exact_size.error().find("sector-rounded size") != std::string::npos);
  CHECK_FALSE(VS::Effects::LoadLiveEffectPalette({.lba = 101, .size = 2048})
                  .has_value());
  CHECK_FALSE(
      VS::Effects::LoadLiveEffectPalette({.lba = 0, .size = 2048}).has_value());
}

TEST_CASE("Live effect palettes load edits to the converted image",
          "[content-images]") {
  ConvertedFbcStore store;
  const fs::path directory =
      store.data.Root() / "textures" / "effect" / "palettes";
  auto authored = Content::LoadImageFiles(directory, "E001_1");
  REQUIRE(authored.has_value());
  REQUIRE_FALSE(authored->image.texels.empty());
  authored->image.texels.front() = 0x7C1F;
  auto files = Extract::EncodeImageFiles(*authored, "E001_1");
  REQUIRE(files.has_value());
  REQUIRE(Extract::WriteImageFiles(directory, *files).has_value());
  const auto edited = VS::Effects::LoadLiveEffectPalette(kConvertedFbcRead);
  REQUIRE(edited.has_value());
  CHECK(edited->front() == 0x7C1F);
  CHECK(ReadBytes(store.data.Root() / "disc" / "EFFECT" / "E001_1.FBC") ==
        store.fbc);
}

TEST_CASE("TIM conversion preserves edits until a forced rebuild",
          "[content-images]") {
  TemporaryData data;
  data.WriteManifest(BackgroundTimFiles());
  REQUIRE(Extract::ConvertDiscImages(data.Root(), false).has_value());
  const fs::path png = data.Root() / "textures" / "bg" / "001OP01A.png";
  const Bytes generated = ReadBytes(png);
  REQUIRE_FALSE(generated.empty());
  const Bytes edited{std::byte{0x01}, std::byte{0x02}};
  {
    std::ofstream output(png, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(edited.data()),
                 static_cast<std::streamsize>(edited.size()));
    REQUIRE(output.good());
  }
  REQUIRE(Extract::ConvertDiscImages(data.Root(), false).has_value());
  CHECK(ReadBytes(png) == edited);
  REQUIRE(Extract::ConvertDiscImages(data.Root(), true).has_value());
  CHECK(ReadBytes(png) == generated);
}

TEST_CASE(
    "TIM conversion rebuilds changed dependencies and removes stale files",
    "[content-images]") {
  TemporaryData data;
  DataFiles files = BackgroundTimFiles();
  data.WriteManifest(files);
  REQUIRE(Extract::ConvertDiscImages(data.Root(), false).has_value());
  auto manifest = RequiredManifest(data.Root());
  manifest.outputs.front().files.push_back("textures/bg/obsolete.png");
  REQUIRE(Extract::WriteConversionManifest(data.Root(), manifest).has_value());
  const Bytes stale{std::byte{0x01}};
  {
    std::ofstream output(data.Root() / "textures" / "bg" / "obsolete.png",
                         std::ios::binary);
    output.write(reinterpret_cast<const char*>(stale.data()), 1);
    REQUIRE(output.good());
  }
  files["BG/001OP01A.FAR"] = LayoutBytes(1, 1, 1);
  data.WriteManifest(files);
  const auto changed = Extract::ConvertDiscImages(data.Root(), false);
  REQUIRE(changed.has_value());
  CHECK(changed->converted == 1);
  CHECK(changed->removed == 1);
  CHECK_FALSE(fs::exists(data.Root() / "textures" / "bg" / "obsolete.png"));
}

TEST_CASE("unsupported TIM inputs fail before publishing converted outputs",
          "[content-images]") {
  SECTION("unknown standalone image") {
    TemporaryData data;
    const DataFiles files{{"OTHER/UNKNOWN.TIM", TimBytes()}};
    data.WriteManifest(files);
    CHECK_FALSE(Extract::ConvertDiscImages(data.Root(), false).has_value());
    CHECK_FALSE(fs::exists(data.Root() / "manifest.json"));
    CHECK_FALSE(fs::exists(data.Root() / "textures"));
  }
  SECTION("background without its layout") {
    TemporaryData data;
    const DataFiles files{{"BG/001OP01A.TIM", TimBytes()}};
    data.WriteManifest(files);
    CHECK_FALSE(Extract::ConvertDiscImages(data.Root(), false).has_value());
    CHECK_FALSE(fs::exists(data.Root() / "manifest.json"));
    CHECK_FALSE(fs::exists(data.Root() / "textures"));
  }
}

TEST_CASE("extracted TIM corpus converts without changing its source bytes",
          "[content-images][extract]") {
  const fs::path source_root = "Native/data";
  auto entries = Content::DataStore::LoadManifest(source_root);
  if (!entries) SKIP("Extracted Native/data is unavailable");
  const DataFiles input_files = TimCorpusInputs(source_root, *entries);

  TemporaryData converted_data;
  converted_data.WriteManifest(input_files);
  const auto report = Extract::ConvertDiscImages(converted_data.Root(), false);
  REQUIRE(report.has_value());
  CHECK(report->converted ==
        std::ranges::count_if(input_files, [](const auto& item) {
          return item.first.ends_with(".TIM");
        }));

  const auto manifest = RequiredManifest(converted_data.Root());
  VerifyCorpusTimRoundTrips(converted_data.Root(), input_files, manifest);

  const auto referenced =
      ReferencedBackgroundTims(source_root, *entries, manifest);
  for (const auto& [path, source] : input_files) {
    (void)source;
    if (path.starts_with("BG/") && path.ends_with(".TIM")) {
      CHECK(referenced.contains(path));
    }
  }

  VerifyBackgroundTextureEdit(source_root, converted_data, input_files);
  VerifyDependencyInvalidation(converted_data, input_files, manifest);
}

TEST_CASE("extracted FBC palette corpus converts and reloads exact words",
          "[content-images][extract]") {
  const fs::path source_root = "Native/data";
  auto entries = Content::DataStore::LoadManifest(source_root);
  if (!entries) SKIP("Extracted Native/data is unavailable");
  const DataFiles input_files = FbcCorpusInputs(source_root, *entries);
  if (input_files.empty()) SKIP("Extracted Native/data has no FBC files");

  TemporaryData converted_data;
  converted_data.WriteManifest(input_files);
  const auto report = Extract::ConvertDiscImages(converted_data.Root(), false);
  REQUIRE(report.has_value());
  CHECK(report->converted == input_files.size());
  CHECK(report->unchanged == 0);
  const auto manifest = RequiredManifest(converted_data.Root());
  CHECK(manifest.converters.contains(
      std::string(Extract::kFbcPaletteConverterName)));
  for (const auto& [path, source] : input_files) {
    VerifyFbcCorpusOutput(converted_data.Root(), manifest, path, source);
  }
}

}  // namespace
