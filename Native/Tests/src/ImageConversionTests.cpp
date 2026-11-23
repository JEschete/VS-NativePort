#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <initializer_list>
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
#include "Extract/EffectTextureConversion.hpp"
#include "Extract/EsqConversion.hpp"
#include "Extract/EtmConversion.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "Extract/WepModelConversion.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"
#include "Extract/ZndConversion.hpp"
#include "Extract/ZudConversion.hpp"
#include "Field/BackgroundLive.hpp"
#include "TestSupport/AudioConversionFixture.hpp"

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
    // The sequence keeps roots made within one clock tick apart.
    static unsigned sequence = 0;
    root_ = fs::temp_directory_path() /
            ("vs-image-conversion-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(sequence++));
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
    auto tim = VS::Content::TimFromImage(image->image, *image->provenance.tim);
    REQUIRE(tim.has_value());
    auto rebuilt = VS::Content::EncodeTim(*tim);
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

// Converts root's images and records its workshop recipe tables, character
// models, weapons, animation sets, actor textures, archetypes, sound banks, XA
// tracks and movies (none in these stores), so the store's conversion check
// covers the images alone.
Extract::ImageConversionReport RequiredConversion(const fs::path& root,
                                                  bool force) {
  auto report = Extract::ConvertDiscImages(root, force);
  REQUIRE(report.has_value());
  const auto tables = Extract::ConvertDiscTables(root, force);
  INFO((tables ? std::string{} : tables.error()));
  REQUIRE(tables.has_value());
  const auto models = Extract::ConvertDiscModels(root, force);
  INFO((models ? std::string{} : models.error()));
  REQUIRE(models.has_value());
  const auto weapons = Extract::ConvertDiscWeapons(root, force);
  INFO((weapons ? std::string{} : weapons.error()));
  REQUIRE(weapons.has_value());
  const auto sets = Extract::ConvertDiscAnimationSets(root, force);
  INFO((sets ? std::string{} : sets.error()));
  REQUIRE(sets.has_value());
  const auto textures = Extract::ConvertDiscActorTextures(root, force);
  INFO((textures ? std::string{} : textures.error()));
  REQUIRE(textures.has_value());
  const auto archetypes = Extract::ConvertDiscArchetypes(root, force);
  INFO((archetypes ? std::string{} : archetypes.error()));
  REQUIRE(archetypes.has_value());
  const auto zones = Extract::ConvertDiscZones(root, force);
  INFO((zones ? std::string{} : zones.error()));
  REQUIRE(zones.has_value());
  VS::test_support::RecordAudioConversions(root);
  return *report;
}

// The little-endian halfwords of an FBC or FBT file.
std::vector<std::uint16_t> WordsOf(const Bytes& source) {
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
  CHECK(Content::PackedWords(decoded.image) == WordsOf(source));
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
                              .note = {},
                              .alias_of = {}});
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
  const auto with_schema = [&encoded](std::string_view version) {
    std::string text = *encoded;
    const std::string current = "\"schema_version\": 2";
    text.replace(text.find(current), current.size(),
                 std::format("\"schema_version\": {}", version));
    return text;
  };
  // Schema 1 manifests have no aliases and still load; later ones do not.
  const std::string older = with_schema("1");
  CHECK(
      Content::ParseConversionManifest(
          std::as_bytes(std::span(older.data(), older.size())), "manifest.json")
          .has_value());
  const std::string future = with_schema("3");
  CHECK_FALSE(Content::ParseConversionManifest(
                  std::as_bytes(std::span(future.data(), future.size())),
                  "manifest.json")
                  .has_value());
}

TEST_CASE("conversion manifests reject invalid dependencies and hashes",
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
                              .note = {},
                              .alias_of = {}});
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
                                     .note = {},
                                     .alias_of = {}}});
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
    REQUIRE(Content::PackedWords(decoded->image) == WordsOf(source));
  }
}

// Whether converting `source` as an FBC fails with an error naming `reason`.
bool FbcRejected(const Bytes& source, std::string_view reason) {
  const auto converted = Extract::ConvertFbcPalette(
      {.path = "EFFECT/E001_1.FBC", .bytes = source});
  return !converted.has_value() &&
         converted.error().find(reason) != std::string::npos;
}

TEST_CASE("FBC palette conversion rejects incomplete or oversized rows",
          "[content-images]") {
  SECTION("partial row") {
    CHECK(FbcRejected(Bytes(kFbcRowBytes - 1, std::byte{0}),
                      "whole 512-byte rows"));
  }
  SECTION("empty") {
    CHECK(FbcRejected({}, "whole 512-byte rows; it has 0 bytes"));
  }
  SECTION("truncated second row") {
    CHECK(FbcRejected(Bytes(kFbcRowBytes + kFbcRowBytes / 2, std::byte{0}),
                      "it has 768 bytes"));
  }
  SECTION("too many rows") { CHECK(FbcRejected(FbcBytes(6), "at most 5")); }
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
  REQUIRE(RequiredConversion(data.Root(), false).converted == 1);
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

// The same two FBC files, converted into two separate data roots.
struct TwinFbcRoots {
  DataFiles files{{"EFFECT/E001_1.FBC", FbcBytes(2)},
                  {"EFFECT/E002_1.FBC", FbcBytes(1)}};
  TemporaryData first;
  TemporaryData second;

  TwinFbcRoots() {
    first.WriteManifest(files);
    second.WriteManifest(files);
    REQUIRE(RequiredConversion(first.Root(), false).converted == 2);
    REQUIRE(RequiredConversion(second.Root(), false).converted == 2);
  }
};

TEST_CASE("FBC conversion writes identical bytes in every data root",
          "[content-images]") {
  const TwinFbcRoots roots;
  CHECK(ReadBytes(roots.first.Root() / "manifest.json") ==
        ReadBytes(roots.second.Root() / "manifest.json"));
  for (const auto& output : RequiredManifest(roots.first.Root()).outputs) {
    for (const std::string& file : output.files) {
      INFO(file);
      const Bytes bytes = ReadBytes(roots.first.Root() / fs::path(file));
      REQUIRE_FALSE(bytes.empty());
      CHECK(bytes == ReadBytes(roots.second.Root() / fs::path(file)));
    }
  }
}

TEST_CASE("A changed FBC reconverts its own palette and no other",
          "[content-images]") {
  TwinFbcRoots roots;
  DataFiles changed = roots.files;
  changed["EFFECT/E001_1.FBC"].front() ^= std::byte{1};
  roots.first.WriteManifest(changed);
  const auto report = RequiredConversion(roots.first.Root(), false);
  CHECK(report.converted == 1);
  CHECK(report.unchanged == 1);
  const fs::path palettes = fs::path("textures") / "effect" / "palettes";
  CHECK(ReadBytes(roots.first.Root() / palettes / "E001_1.png") !=
        ReadBytes(roots.second.Root() / palettes / "E001_1.png"));
  CHECK(ReadBytes(roots.first.Root() / palettes / "E002_1.png") ==
        ReadBytes(roots.second.Root() / palettes / "E002_1.png"));
}

TEST_CASE("A malformed FBC fails the run before any palette publishes",
          "[content-images]") {
  TemporaryData data;
  data.WriteManifest({{"EFFECT/E001_1.FBC", FbcBytes(2)}});
  REQUIRE(RequiredConversion(data.Root(), false).converted == 1);
  const fs::path manifest = data.Root() / "manifest.json";
  const fs::path palettes = data.Root() / "textures" / "effect" / "palettes";
  const Bytes published = ReadBytes(manifest);
  const Bytes palette = ReadBytes(palettes / "E001_1.png");
  REQUIRE_FALSE(published.empty());

  Bytes changed = FbcBytes(2);
  changed.front() ^= std::byte{1};
  data.WriteManifest(
      {{"EFFECT/E001_1.FBC", changed},
       {"EFFECT/E002_1.FBC", Bytes(kFbcRowBytes - 1, std::byte{0})}});
  const auto failed = Extract::ConvertDiscImages(data.Root(), false);
  REQUIRE_FALSE(failed.has_value());
  CHECK(failed.error().find("EFFECT/E002_1.FBC") != std::string::npos);
  // The changed palette converted in memory only: nothing was written.
  CHECK(ReadBytes(manifest) == published);
  CHECK(ReadBytes(palettes / "E001_1.png") == palette);
  CHECK_FALSE(fs::exists(palettes / "E002_1.png"));
  // The store then refuses the stale palette instead of loading it.
  CHECK(ConversionError(data).find(
            "stale fbc-palette-png source hash for EFFECT/E001_1.FBC") !=
        std::string::npos);
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
  CHECK(*loaded == WordsOf(store.fbc));

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

TEST_CASE("Live effect palettes refuse an edit that is not 256 entries wide",
          "[content-images]") {
  ConvertedFbcStore store;
  const fs::path directory =
      store.data.Root() / "textures" / "effect" / "palettes";
  auto authored = Content::LoadImageFiles(directory, "E001_1");
  REQUIRE(authored.has_value());
  // The same 512 colours, as four 128-entry rows.
  authored->image.width /= 2;
  authored->image.height *= 2;
  auto files = Extract::EncodeImageFiles(*authored, "E001_1");
  REQUIRE(files.has_value());
  REQUIRE(Extract::WriteImageFiles(directory, *files).has_value());
  const auto refused = VS::Effects::LoadLiveEffectPalette(kConvertedFbcRead);
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().find("is not a 256-entry effect palette") !=
        std::string::npos);
}

// A synthetic effect disc for the fbt-texture-png converter: EFFPURGE.BIN's
// tables at their file offsets (EffectTextureConversion.cpp), and per effect
// a package, a two-row FBC in palette slot 0 and texture pages.
constexpr std::size_t kEffPurgeBytes = 17264;
constexpr std::size_t kPurgeClutRows = 0x164C;
constexpr std::size_t kPurgePaletteFiles = 0x196C;
constexpr std::size_t kPurgeTextureCounts = 0x1FAC;
constexpr std::size_t kPurgeTextureSlots = 0x213C;
constexpr std::size_t kPurgeTextureFiles = 0x2430;
constexpr std::size_t kPurgePackageFiles = 0x2FFC;
constexpr std::size_t kPurgeExtentBytes = 4;
constexpr std::string_view kPurgePath = "EFFECT/EFFPURGE.BIN";
constexpr std::size_t kFbtBytes = std::size_t{64} * 256 * sizeof(std::uint16_t);
constexpr std::uint32_t kFirstTestLba = 100;  // TemporaryData::WriteManifest
// The v byte of a package's first descriptor, which no converter reads.
constexpr std::size_t kFirstDescriptorV = 13;

std::uint32_t SectorsOf(std::size_t bytes) {
  return static_cast<std::uint32_t>((bytes + 2047) / 2048);
}

// The files.tsv entries TemporaryData::WriteManifest lists for `files`.
std::vector<Content::DataStore::Entry> EntriesOf(const DataFiles& files) {
  std::vector<Content::DataStore::Entry> entries;
  std::uint32_t lba = kFirstTestLba;
  for (const auto& [path, bytes] : files) {
    entries.push_back({.path = path,
                       .lba = lba,
                       .size = static_cast<std::uint32_t>(bytes.size()),
                       .form = "raw"});
    lba += SectorsOf(bytes.size());
  }
  return entries;
}

std::uint32_t LbaOf(const DataFiles& files, std::string_view path) {
  const auto entries = EntriesOf(files);
  const auto found =
      std::ranges::find(entries, path, &Content::DataStore::Entry::path);
  REQUIRE(found != entries.end());
  return found->lba;
}

VS::Effects::DiscRead FbtRead(const DataFiles& files, std::string_view path) {
  return {.lba = static_cast<std::int32_t>(LbaOf(files, path)),
          .size = static_cast<std::int32_t>(kFbtBytes)};
}

void Poke16(Bytes& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::byte>(value & 0xFFU);
  bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xFFU);
}

// A texture descriptor reading `width` texels from `u` of VRAM page 24 at
// `depth_code` (0 4-bit, 1 8-bit, 2 direct colour), with its CLUT at `entry`
// of the effect FBC's `row`.
struct FbtSample {
  std::uint32_t depth_code = 0;
  std::uint32_t row = 0;
  std::uint32_t entry = 0;
  std::uint32_t u = 0;
  std::uint32_t width = 0;
};

FbtSample Sample(std::uint32_t depth_code, std::uint32_t row = 0,
                 std::uint32_t entry = 0) {
  return {.depth_code = depth_code,
          .row = row,
          .entry = entry,
          .u = 0,
          .width = 16};
}

// A package whose texture block holds `samples`' descriptors, then its end
// block.
Bytes PackageBytes(std::span<const FbtSample> samples) {
  constexpr std::size_t kDescriptorBytes = 0x18;
  constexpr std::uint32_t kPage24 = 0x18;  // column 8 of the second row
  Bytes block;
  Put16(block, 1);  // table count
  Put16(block, 4);  // where the descriptors start
  for (const FbtSample& sample : samples) {
    const std::size_t start = block.size();
    Put16(block, static_cast<std::uint16_t>(((240 + sample.row) << 6U) |
                                            ((768 + sample.entry) / 16)));
    Put16(block,
          static_cast<std::uint16_t>(kPage24 | (sample.depth_code << 7U)));
    block.push_back(static_cast<std::byte>(sample.u));
    block.push_back(std::byte{0});  // v
    block.push_back(static_cast<std::byte>(sample.width));
    block.resize(start + kDescriptorBytes);
  }
  Bytes package;
  Put32(package, static_cast<std::uint32_t>(block.size() + 4) | (1U << 16U));
  package.insert(package.end(), block.begin(), block.end());
  Put32(package, 0);  // the end block
  return package;
}

Bytes PackageBytes(std::initializer_list<FbtSample> samples) {
  return PackageBytes(std::span(samples.begin(), samples.size()));
}

// A 64 x 256-halfword effect texture page whose bytes start at `seed`.
Bytes FbtBytes(std::uint8_t seed = 0) {
  Bytes bytes(kFbtBytes);
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<std::byte>(((index * 7U) + seed) & 0xFFU);
  }
  return bytes;
}

struct FbtPage {
  std::uint32_t slot = 0;
  Bytes bytes;
};

struct FbtEffect {
  std::uint32_t effect = 0;
  std::vector<FbtSample> samples;
  std::vector<FbtPage> pages;
};

// `effect` loading one page into slot 0, which `samples` read.
FbtEffect Effect(std::uint32_t effect, std::vector<FbtSample> samples,
                 Bytes page = FbtBytes()) {
  return {.effect = effect,
          .samples = std::move(samples),
          .pages = {{.slot = 0, .bytes = std::move(page)}}};
}

std::string EffectPath(std::uint32_t effect, std::string_view suffix) {
  return std::format("EFFECT/E{:03}{}", effect, suffix);
}

std::string PagePath(std::uint32_t effect, const FbtPage& page) {
  return EffectPath(effect, std::format("_{}.FBT", page.slot));
}

// EFFPURGE.BIN binding `effects` (in ascending order) to their files, by
// sectors from E000.P's.
Bytes PurgeBytes(const DataFiles& files, std::span<const FbtEffect> effects) {
  const std::uint32_t base = LbaOf(files, "EFFECT/E000.P");
  Bytes purge(kEffPurgeBytes);
  const auto put_extent = [&](std::size_t offset, const std::string& path) {
    Poke16(purge, offset, LbaOf(files, path) - base);
    Poke16(purge, offset + 2, SectorsOf(files.at(path).size()) * 2048);
  };
  std::size_t texture = 0;
  for (const FbtEffect& effect : effects) {
    const std::size_t index = effect.effect;
    purge[kPurgeClutRows + index] = std::byte{2};
    put_extent(kPurgePaletteFiles + (index * kPurgeExtentBytes),
               EffectPath(effect.effect, "_0.FBC"));
    put_extent(kPurgePackageFiles + (index * kPurgeExtentBytes),
               EffectPath(effect.effect, ".P"));
    purge[kPurgeTextureCounts + index] =
        static_cast<std::byte>(effect.pages.size());
    for (const FbtPage& page : effect.pages) {
      purge[kPurgeTextureSlots + texture] = static_cast<std::byte>(page.slot);
      put_extent(kPurgeTextureFiles + (texture * kPurgeExtentBytes),
                 PagePath(effect.effect, page));
      ++texture;
    }
  }
  return purge;
}

// A disc of `effects`' files and the EFFPURGE.BIN binding them.
DataFiles FbtDiscFiles(const std::vector<FbtEffect>& effects) {
  DataFiles files;
  for (const FbtEffect& effect : effects) {
    files.emplace(EffectPath(effect.effect, ".P"),
                  PackageBytes(effect.samples));
    files.emplace(EffectPath(effect.effect, "_0.FBC"), FbcBytes(2));
    for (const FbtPage& page : effect.pages) {
      files.emplace(PagePath(effect.effect, page), page.bytes);
    }
  }
  files.emplace(std::string(kPurgePath), PurgeBytes(files, effects));
  return files;
}

Extract::SourceReader ReaderOf(const DataFiles& files) {
  return [&files](std::string_view path)
             -> std::expected<Extract::SourceFile, std::string> {
    const auto found = files.find(path);
    if (found == files.end()) {
      return std::unexpected(std::format("{} is not on the disc", path));
    }
    return Extract::SourceFile{.path = found->first, .bytes = found->second};
  };
}

std::expected<Extract::FbtConversionPlan, std::string> PlanFbt(
    const DataFiles& files) {
  return Extract::PlanFbtConversions(EntriesOf(files), ReaderOf(files));
}

std::string FbtPlanError(const DataFiles& files) {
  const auto plan = PlanFbt(files);
  return plan ? std::string{} : plan.error();
}

Extract::FbtConversion RequiredFbtConversion(const DataFiles& files) {
  auto plan = PlanFbt(files);
  INFO((plan ? std::string{} : plan.error()));
  REQUIRE(plan.has_value());
  REQUIRE(plan->conversions.size() == 1);
  return std::move(plan->conversions.front());
}

// `count` colours of FbcBytes(2) from `entry` of `row`.
std::vector<std::uint16_t> PreviewWords(std::uint32_t row, std::uint32_t entry,
                                        std::size_t count) {
  const std::vector<std::uint16_t> words = WordsOf(FbcBytes(2));
  const auto first = words.begin() + static_cast<std::ptrdiff_t>(
                                         (row * kFbcPaletteEntries) + entry);
  return {first, first + static_cast<std::ptrdiff_t>(count)};
}

struct FbtDepthCase {
  std::uint32_t code = 0;
  Content::ImageDepth depth = Content::ImageDepth::kIndexed4;
  std::uint32_t entry = 0;
  std::uint32_t width = 0;
  std::size_t colours = 0;
};

void CheckFbtDepthBinding(const FbtDepthCase& depth,
                          const Extract::FbtConversion& conversion) {
  CHECK(conversion.texture.path == "EFFECT/E000_0.FBT");
  CHECK(conversion.depth == depth.depth);
  CHECK(conversion.palette.path == "EFFECT/E000_0.FBC");
  CHECK(conversion.palette_row == 1);
  CHECK(conversion.palette_entry == depth.entry);
}

void CheckFbtDepthProvenance(const Content::ImageDocument& decoded,
                             const FbtDepthCase& depth) {
  const Content::ImageProvenance& provenance = decoded.provenance;
  const Content::TexturePagePlacement placement{
      .effect = 0, .slot = 0, .page = 24, .vram = Content::TexturePageRect(24)};
  CHECK(provenance.texture_page == placement);
  const Content::PreviewPalette preview{
      .image = "image.effect.e000_0.palette", .row = 1, .entry = depth.entry};
  CHECK(provenance.preview_palette == preview);
  CHECK(provenance.note.empty());
  std::vector<std::string> dependencies;
  for (const Content::ImageDependency& dependency : provenance.dependencies) {
    dependencies.push_back(dependency.source + " " + dependency.role);
  }
  CHECK(dependencies ==
        std::vector<std::string>{"EFFECT/E000.P sampler",
                                 "EFFECT/E000_0.FBC preview palette",
                                 "EFFECT/EFFPURGE.BIN binding"});
}

void CheckFbtDepthPixels(const FbtDepthCase& depth, const DataFiles& files,
                         const Content::ImageDocument& decoded) {
  CHECK(decoded.image.depth == depth.depth);
  CHECK(decoded.image.width == depth.width);
  CHECK(decoded.image.height == 256);
  CHECK(Content::PackedWords(decoded.image) ==
        WordsOf(files.at("EFFECT/E000_0.FBT")));
  REQUIRE(decoded.image.palettes.size() == 1);
  CHECK(decoded.image.palettes.front() ==
        PreviewWords(1, depth.entry, depth.colours));
}

void CheckFbtDepthImage(const FbtDepthCase& depth, const DataFiles& files,
                        const Extract::FbtConversion& conversion) {
  const auto converted = Extract::ConvertFbtTexture(conversion);
  REQUIRE(converted.has_value());
  CHECK(converted->output.id == "image.effect.e000_0");
  CHECK(converted->output.converter == Extract::kFbtTextureConverterName);
  CHECK(converted->location.directory == "textures/effect");
  const auto decoded =
      Content::DecodeImageFiles("E000_0", ReaderFor(converted->files));
  REQUIRE(decoded.has_value());
  CheckFbtDepthPixels(depth, files, *decoded);
  CheckFbtDepthProvenance(*decoded, depth);
}

void CheckFbtDepthCase(const FbtDepthCase& depth) {
  INFO("depth code " << depth.code);
  const DataFiles files = FbtDiscFiles(
      {Effect(0, {Sample(depth.code), Sample(depth.code, 1, depth.entry),
                  Sample(depth.code, 1, depth.entry)})});
  const Extract::FbtConversion conversion = RequiredFbtConversion(files);
  CheckFbtDepthBinding(depth, conversion);
  CheckFbtDepthImage(depth, files, conversion);
}

TEST_CASE("FBT pages convert at the depth their effect samples",
          "[content-images]") {
  CheckFbtDepthCase({.code = 0,
                     .depth = Content::ImageDepth::kIndexed4,
                     .entry = 16,
                     .width = 256,
                     .colours = 16});
  CheckFbtDepthCase({.code = 1,
                     .depth = Content::ImageDepth::kIndexed8,
                     .entry = 0,
                     .width = 128,
                     .colours = 256});
}

TEST_CASE("An FBT page no descriptor samples converts at 8 bits with a note",
          "[content-images]") {
  const DataFiles files = FbtDiscFiles({Effect(0, {})});
  const Extract::FbtConversion conversion = RequiredFbtConversion(files);
  CHECK_FALSE(conversion.depth.has_value());
  const auto converted = Extract::ConvertFbtTexture(conversion);
  REQUIRE(converted.has_value());
  CHECK(converted->output.note.find("converts at 8 bits") != std::string::npos);
  const auto decoded =
      Content::DecodeImageFiles("E000_0", ReaderFor(converted->files));
  REQUIRE(decoded.has_value());
  CHECK(decoded->image.depth == Content::ImageDepth::kIndexed8);
  CHECK(decoded->provenance.note == converted->output.note);
  CHECK(Content::PackedWords(decoded->image) ==
        WordsOf(files.at("EFFECT/E000_0.FBT")));
}

void CheckFbtAlias(const Content::ConversionOutput& alias) {
  CHECK(alias.id == "image.effect.e001_0");
  CHECK(alias.source == "EFFECT/E001_0.FBT");
  CHECK(alias.converter == Extract::kFbtTextureConverterName);
  CHECK(alias.alias_of == "image.effect.e000_0");
  CHECK(alias.files.empty());
  REQUIRE_FALSE(alias.dependencies.empty());
  CHECK(alias.dependencies.front().source == "EFFECT/E001.P");
}

TEST_CASE("Identical FBT pages at one depth alias the later page",
          "[content-images]") {
  const auto plan =
      PlanFbt(FbtDiscFiles({Effect(0, {Sample(0)}), Effect(1, {Sample(0)})}));
  REQUIRE(plan.has_value());
  REQUIRE(plan->conversions.size() == 1);
  CHECK(plan->conversions.front().texture.path == "EFFECT/E000_0.FBT");
  REQUIRE(plan->aliases.size() == 1);
  CheckFbtAlias(plan->aliases.front());
}

TEST_CASE("Identical FBT pages at different depths do not alias",
          "[content-images]") {
  const auto plan =
      PlanFbt(FbtDiscFiles({Effect(0, {Sample(0)}), Effect(1, {Sample(1)})}));
  REQUIRE(plan.has_value());
  CHECK(plan->conversions.size() == 2);
  CHECK(plan->aliases.empty());
}

TEST_CASE("FBT planning rejects pages it cannot place or sample",
          "[content-images]") {
  DataFiles files = FbtDiscFiles({Effect(0, {Sample(0)})});
  Bytes& purge = files.at(std::string(kPurgePath));
  Bytes& package = files.at("EFFECT/E000.P");
  std::string reason;
  SECTION("purge tables of another size") {
    purge.pop_back();
    reason = "the purge tables take 17264 bytes; it has 17263";
  }
  SECTION("no FBC palette") {
    purge[kPurgeClutRows] = std::byte{0};
    reason = "effect 0 lists texture pages but loads no FBC palette";
  }
  SECTION("an extent that names no file") {
    Poke16(purge, kPurgePaletteFiles + 2, 4096);
    reason = "names no listed .FBC file";
  }
  SECTION("a slot past the effect pages") {
    purge[kPurgeTextureSlots] = std::byte{8};
    reason = "slots are 0-7";
  }
  SECTION("a page no effect loads") {
    files.emplace("EFFECT/E000_7.FBT", FbtBytes());
    reason =
        "EFFECT/E000_7.FBT: no effect in EFFECT/EFFPURGE.BIN loads this "
        "texture page";
  }
  SECTION("a package without its end block") {
    package.resize(package.size() - 4);
    reason = "without its end block";
  }
  SECTION("texels past the page") {
    FbtSample past = Sample(0);
    past.u = 250;
    package = PackageBytes({past});
    reason = "reads texels 250-265, past its page's 256";
  }
  SECTION("two depths for one page") {
    package = PackageBytes({Sample(0), Sample(1)});
    reason = "an earlier one reads it at";
  }
  SECTION("direct colour") {
    package = PackageBytes({Sample(2)});
    reason = "as direct colour; effect pages hold palette indices";
  }
  SECTION("a CLUT row the FBC lacks") {
    package = PackageBytes({Sample(0, 2)});
    reason = "which EFFECT/E000_0.FBC's 2 rows at row 240 do not hold";
  }
  const std::string error = FbtPlanError(files);
  INFO(error);
  CHECK(error.find(reason) != std::string::npos);
}

TEST_CASE("FBT planning rejects a page two effects or none can load",
          "[content-images]") {
  DataFiles files =
      FbtDiscFiles({Effect(0, {Sample(0)}), Effect(1, {Sample(0)})});
  Bytes& purge = files.at(std::string(kPurgePath));
  std::string reason;
  SECTION("a page two effects load") {
    const auto at = static_cast<std::ptrdiff_t>(kPurgeTextureFiles);
    const auto extent = static_cast<std::ptrdiff_t>(kPurgeExtentBytes);
    std::copy_n(purge.begin() + at, extent, purge.begin() + at + extent);
    reason = "EFFECT/E000_0.FBT is loaded by effects 0 and 1";
  }
  SECTION("an effect without its package") {
    Poke16(purge, kPurgePackageFiles + kPurgeExtentBytes, 0);
    reason = "effect 1 lists texture pages but has no package";
  }
  const std::string error = FbtPlanError(files);
  INFO(error);
  CHECK(error.find(reason) != std::string::npos);
}

TEST_CASE("FBT conversion rejects a page it cannot rebuild",
          "[content-images]") {
  Extract::FbtConversion conversion =
      RequiredFbtConversion(FbtDiscFiles({Effect(0, {Sample(0)})}));
  std::string reason;
  SECTION("a short page") {
    conversion.texture.bytes.pop_back();
    reason = "is a 32768-byte .FBT file; it has 32767 bytes";
  }
  SECTION("a slot past the effect pages") {
    conversion.slot = 8;
    reason = "slot 8 is not one of 0-7";
  }
  SECTION("a preview row the FBC lacks") {
    conversion.palette_row = 2;
    reason = "has no 16-entry palette at row 2, entry 0";
  }
  SECTION("an 8-bit preview past the row's end") {
    conversion.depth = Content::ImageDepth::kIndexed8;
    conversion.palette_entry = 16;
    reason = "has no 256-entry palette at row 0, entry 16";
  }
  const auto converted = Extract::ConvertFbtTexture(conversion);
  REQUIRE_FALSE(converted.has_value());
  INFO(converted.error());
  CHECK(converted.error().find(reason) != std::string::npos);
}

TEST_CASE("FBT conversion keeps unchanged pages and edits until forced",
          "[content-images]") {
  TemporaryData data;
  data.WriteManifest(FbtDiscFiles({Effect(0, {Sample(0)})}));
  CHECK(RequiredConversion(data.Root(), false).converted == 2);
  const fs::path page = data.Root() / "textures" / "effect" / "E000_0.png";
  const Bytes generated = ReadBytes(page);
  REQUIRE_FALSE(generated.empty());
  const auto second = RequiredConversion(data.Root(), false);
  CHECK(second.converted == 0);
  CHECK(second.unchanged == 2);

  const Bytes edited{std::byte{0x01}, std::byte{0x02}};
  OverwriteFile(page, edited);
  CHECK(RequiredConversion(data.Root(), false).unchanged == 2);
  CHECK(ReadBytes(page) == edited);
  CHECK(RequiredConversion(data.Root(), true).converted == 2);
  CHECK(ReadBytes(page) == generated);
}

TEST_CASE("A changed package, palette or binding reconverts its FBT pages",
          "[content-images]") {
  TemporaryData data;
  DataFiles files = FbtDiscFiles({Effect(0, {Sample(0)})});
  data.WriteManifest(files);
  REQUIRE(RequiredConversion(data.Root(), false).converted == 2);
  std::string stale;
  std::size_t reconverted = 1;
  SECTION("the package") {
    files.at("EFFECT/E000.P")[kFirstDescriptorV] ^= std::byte{1};
    stale =
        "stale fbt-texture-png dependency EFFECT/E000.P for "
        "EFFECT/E000_0.FBT";
  }
  SECTION("the palette") {
    files.at("EFFECT/E000_0.FBC").front() ^= std::byte{1};
    stale = "stale fbc-palette-png source hash for EFFECT/E000_0.FBC";
    reconverted = 2;  // the palette and the page that previews it
  }
  SECTION("the binding") {
    files.at(std::string(kPurgePath)).back() ^= std::byte{1};
    stale =
        "stale fbt-texture-png dependency EFFECT/EFFPURGE.BIN for "
        "EFFECT/E000_0.FBT";
  }
  data.WriteManifest(files);
  const std::string error = ConversionError(data);
  INFO(error);
  CHECK(error.find(stale) != std::string::npos);
  CHECK(RequiredConversion(data.Root(), false).converted == reconverted);
  CHECK(ConversionError(data).empty());
}

// `data` with effect 0's page and its converted image, opened.
struct ConvertedFbtStore {
  TemporaryData data;
  DataFiles files = FbtDiscFiles({Effect(0, {Sample(0)})});
  VS::Effects::DiscRead read = FbtRead(files, "EFFECT/E000_0.FBT");
  Content::DataStore::ScopedOverride restore;

  ConvertedFbtStore() {
    data.WriteManifest(files);
    REQUIRE(Extract::ConvertDiscImages(data.Root(), false).has_value());
    REQUIRE(Content::DataStore::Open(data.Root()).has_value());
  }

  fs::path Directory() const { return data.Root() / "textures" / "effect"; }
};

TEST_CASE("Live effect textures load the converted page by sector",
          "[content-images]") {
  {
    Content::DataStore::ScopedOverride none;
    const auto unopened =
        VS::Effects::LoadLiveEffectTexture({.lba = 102, .size = 32768});
    REQUIRE_FALSE(unopened.has_value());
    CHECK(unopened.error().find("no extracted-data store") !=
          std::string::npos);
  }
  const ConvertedFbtStore store;
  const auto loaded = VS::Effects::LoadLiveEffectTexture(store.read);
  INFO((loaded ? std::string{} : loaded.error()));
  REQUIRE(loaded.has_value());
  CHECK(*loaded == WordsOf(store.files.at("EFFECT/E000_0.FBT")));
  CHECK_FALSE(VS::Effects::LoadLiveEffectTexture(
                  {.lba = store.read.lba + 1, .size = store.read.size})
                  .has_value());
  CHECK_FALSE(
      VS::Effects::LoadLiveEffectTexture({.lba = store.read.lba, .size = 2048})
          .has_value());
}

TEST_CASE("Live effect textures load edits to the converted page",
          "[content-images]") {
  const ConvertedFbtStore store;
  auto authored = Content::LoadImageFiles(store.Directory(), "E000_0");
  REQUIRE(authored.has_value());
  REQUIRE_FALSE(authored->image.indices.empty());
  authored->image.indices.front() ^= 0x0FU;
  auto files = Extract::EncodeImageFiles(*authored, "E000_0");
  REQUIRE(files.has_value());
  REQUIRE(Extract::WriteImageFiles(store.Directory(), *files).has_value());
  const Bytes& source = store.files.at("EFFECT/E000_0.FBT");
  const auto edited = VS::Effects::LoadLiveEffectTexture(store.read);
  REQUIRE(edited.has_value());
  CHECK(*edited == Content::PackedWords(authored->image));
  CHECK(*edited != WordsOf(source));
  CHECK(ReadBytes(store.data.Root() / "disc" / "EFFECT" / "E000_0.FBT") ==
        source);
}

TEST_CASE("Live effect textures refuse an edit that is not one page",
          "[content-images]") {
  const ConvertedFbtStore store;
  auto authored = Content::LoadImageFiles(store.Directory(), "E000_0");
  REQUIRE(authored.has_value());
  // The same indices, as a 512 x 128 image.
  authored->image.width *= 2;
  authored->image.height /= 2;
  auto files = Extract::EncodeImageFiles(*authored, "E000_0");
  REQUIRE(files.has_value());
  REQUIRE(Extract::WriteImageFiles(store.Directory(), *files).has_value());
  const auto refused = VS::Effects::LoadLiveEffectTexture(store.read);
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().find(
            "is not an indexed 64x256-word effect texture page") !=
        std::string::npos);
}

TEST_CASE("Live effect textures load an aliased page from its target",
          "[content-images]") {
  TemporaryData data;
  DataFiles files =
      FbtDiscFiles({Effect(0, {Sample(0)}), Effect(1, {Sample(0)})});
  data.WriteManifest(files);
  const auto report = RequiredConversion(data.Root(), false);
  CHECK(report.converted == 3);  // two palettes and one page
  CHECK(report.aliased == 1);
  const fs::path copy = data.Root() / "textures" / "effect" / "E001_0.png";
  CHECK_FALSE(fs::exists(copy));
  CHECK(ConversionError(data).empty());
  {
    Content::DataStore::ScopedOverride restore;
    REQUIRE(Content::DataStore::Open(data.Root()).has_value());
    const auto loaded =
        VS::Effects::LoadLiveEffectTexture(FbtRead(files, "EFFECT/E001_0.FBT"));
    INFO((loaded ? std::string{} : loaded.error()));
    REQUIRE(loaded.has_value());
    CHECK(*loaded == WordsOf(files.at("EFFECT/E001_0.FBT")));
  }
}

TEST_CASE("A changed FBT alias refuses stale output until converted",
          "[content-images]") {
  TemporaryData data;
  const fs::path copy = data.Root() / "textures" / "effect" / "E001_0.png";
  DataFiles files =
      FbtDiscFiles({Effect(0, {Sample(0)}), Effect(1, {Sample(0)})});
  data.WriteManifest(files);
  const auto initial = RequiredConversion(data.Root(), false);
  CHECK(initial.aliased == 1);
  files.at("EFFECT/E001_0.FBT") = FbtBytes(1);
  data.WriteManifest(files);
  CHECK(ConversionError(data).find(
            "stale fbt-texture-png source hash for EFFECT/E001_0.FBT") !=
        std::string::npos);
  const auto split = RequiredConversion(data.Root(), false);
  CHECK(split.converted == 1);
  CHECK(split.aliased == 0);
  CHECK(fs::exists(copy));
  CHECK(ConversionError(data).empty());
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

TEST_CASE("A converted background refuses its deleted layout",
          "[content-images]") {
  TemporaryData data;
  data.WriteManifest(BackgroundTimFiles());
  REQUIRE(RequiredConversion(data.Root(), false).converted == 1);
  CHECK(ConversionError(data).empty());
  data.WriteManifest({{"BG/001OP01A.TIM", TimBytes()}});
  CHECK(ConversionError(data).find(
            "missing tim-png dependency BG/001OP01A.FAR") != std::string::npos);
  CHECK_FALSE(Extract::ConvertDiscImages(data.Root(), false).has_value());
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
  const auto report = RequiredConversion(converted_data.Root(), false);
  CHECK(report.converted ==
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

// The containers whose TIMs other converters will own (#203 zones, #211 DIS,
// #498 the ending program), keyed by the tally name.
std::optional<std::string_view> EmbeddedTimContainer(std::string_view path) {
  if (path.starts_with("MAP/") && path.ends_with(".ZND")) return "MAP";
  if (path.starts_with("SMALL/") && path.ends_with(".DIS")) return "SMALL";
  if (path == "ENDING/ENDING.PRG") return "ENDING";
  return std::nullopt;
}

// Each strict TIM starting at a 4-byte-aligned word of `bytes`, by offset; a
// scan resumes after each TIM it finds.
std::vector<std::pair<std::size_t, Content::DecodedTim>> EmbeddedTims(
    std::span<const std::byte> bytes) {
  constexpr std::size_t kWord = 4;
  std::vector<std::pair<std::size_t, Content::DecodedTim>> tims;
  std::size_t at = 0;
  while (at + kWord <= bytes.size()) {
    if (Read32(bytes, at) == Content::kTimMagic) {
      auto decoded = Content::DecodeTim(bytes.subspan(at));
      if (decoded) {
        const std::size_t length = decoded->bytes;
        tims.emplace_back(at, std::move(*decoded));
        at += length;
        continue;
      }
    }
    at += kWord;
  }
  return tims;
}

// The TIM count the zone's first texture table declares (RoomLoading.cpp
// UploadZoneTextures), so the scan cannot miss or invent zone TIMs.
std::size_t ZoneTextureCount(std::span<const std::byte> zone) {
  constexpr std::size_t kTexturesOffsetAt = 0x10;
  constexpr std::size_t kTablesAt = 16;
  if (zone.size() < kTexturesOffsetAt + 4) return 0;
  const std::size_t section = Read32(zone, kTexturesOffsetAt);
  if (section > zone.size() || zone.size() - section < kTablesAt + 4) return 0;
  if (static_cast<std::int32_t>(Read32(zone, section)) <= 0) return 0;
  return Read32(zone, section + kTablesAt);
}

std::string EmbeddedTimTally(std::string_view container,
                             const Content::TimFile& tim) {
  const auto mode = tim.Mode();
  if (mode == Content::TimPixelMode::kDirect15) {
    return std::format("{} 16-bit", container);
  }
  const auto depth = mode == Content::TimPixelMode::kIndexed4
                         ? Content::ImageDepth::kIndexed4
                         : Content::ImageDepth::kIndexed8;
  const std::size_t entries = tim.clut ? tim.clut->words.size() : 0;
  return std::format("{} {}-bit x{} palettes", container,
                     Content::BitsPerPixel(depth),
                     entries / Content::PaletteCapacity(depth));
}

TEST_CASE("embedded TIM payloads convert and rebuild their exact bytes",
          "[content-images][extract]") {
  const fs::path source_root = "Native/data";
  auto entries = Content::DataStore::LoadManifest(source_root);
  if (!entries) SKIP("Extracted Native/data is unavailable");
  std::map<std::string, std::size_t, std::less<>> tallies;
  for (const Content::DataStore::Entry& entry : *entries) {
    const auto container = EmbeddedTimContainer(entry.path);
    if (entry.form != "raw" || !container) continue;
    CAPTURE(entry.path);
    const Bytes bytes = ReadBytes(source_root / "disc" / fs::path(entry.path));
    REQUIRE_FALSE(bytes.empty());
    const auto tims = EmbeddedTims(bytes);
    if (*container == "MAP") CHECK(tims.size() == ZoneTextureCount(bytes));
    const std::string_view stem =
        std::string_view(entry.path).substr(0, entry.path.rfind('.'));
    for (const auto& [offset, decoded] : tims) {
      const auto slice = std::span(bytes).subspan(offset, decoded.bytes);
      Extract::TimConversion conversion{
          .tim = {.path = std::format("{}_{:06X}.TIM", stem, offset),
                  .bytes = Bytes(slice.begin(), slice.end())},
          .layout = std::nullopt,
          .material = std::nullopt,
          .unreferenced = false,
          .note = {}};
      const auto converted = Extract::ConvertTim(conversion);
      INFO((converted ? std::string{} : converted.error()));
      CHECK(converted.has_value());
      ++tallies[EmbeddedTimTally(*container, decoded.tim)];
    }
  }
  if (tallies.empty()) SKIP("Extracted Native/data has no TIM containers");
  const std::map<std::string, std::size_t, std::less<>> expected{
      {"ENDING 4-bit x1 palettes", 6},
      {"ENDING 4-bit x2 palettes", 1},
      {"ENDING 8-bit x1 palettes", 5},
      {"MAP 16-bit", 782},
      {"SMALL 16-bit", 16},
      {"SMALL 4-bit x16 palettes", 12}};
  CHECK(tallies == expected);
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

Extract::FbtConversionPlan ExtractedFbtPlan() {
  const fs::path source_root = "Native/data";
  auto entries = Content::DataStore::LoadManifest(source_root);
  if (!entries) SKIP("Extracted Native/data is unavailable");
  const auto listed = std::ranges::count_if(
      *entries, [](const Content::DataStore::Entry& entry) {
        return entry.form == "raw" && entry.path.ends_with(".FBT");
      });
  if (listed == 0) SKIP("Extracted Native/data has no FBT files");
  const Extract::SourceReader read = [&source_root](std::string_view path)
      -> std::expected<Extract::SourceFile, std::string> {
    Bytes bytes = ReadBytes(source_root / "disc" / fs::path(path));
    if (bytes.empty()) {
      return std::unexpected(std::format("cannot read {}", path));
    }
    return Extract::SourceFile{.path = std::string(path),
                               .bytes = std::move(bytes)};
  };
  const auto plan = Extract::PlanFbtConversions(*entries, read);
  INFO((plan ? std::string{} : plan.error()));
  REQUIRE(plan.has_value());
  CHECK(plan->conversions.size() + plan->aliases.size() ==
        static_cast<std::size_t>(listed));
  return std::move(*plan);
}

void CheckExtractedFbtPlan(const Extract::FbtConversionPlan& plan) {
  std::map<std::string, std::string, std::less<>> hashes;  // id -> source
  for (const Extract::FbtConversion& conversion : plan.conversions) {
    const auto converted = Extract::ConvertFbtTexture(conversion);
    INFO((converted ? std::string{} : converted.error()));
    REQUIRE(converted.has_value());
    hashes.emplace(converted->output.id, converted->output.source_sha256);
  }
  for (const Content::ConversionOutput& alias : plan.aliases) {
    INFO(alias.source);
    const auto target = hashes.find(alias.alias_of);
    REQUIRE(target != hashes.end());
    CHECK(target->second == alias.source_sha256);
    CHECK(alias.files.empty());
  }
}

TEST_CASE("extracted FBT corpus binds every page and converts it exactly",
          "[content-images][extract]") {
  const auto plan = ExtractedFbtPlan();
  CheckExtractedFbtPlan(plan);
}

}  // namespace
