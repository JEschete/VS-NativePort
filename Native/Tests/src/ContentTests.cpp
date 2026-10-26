#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Content/DataAccess.h"
#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Content/ModuleData.h"
#include "Legacy/Psx.h"
#include "Movie/MoviePlayer.h"
#include "Render/PageSpace.hpp"

extern "C" {
#include "ActorAssets.h"
#include "Audio/Seq/Sequencer.h"
#include "Effects/Plugins/Plg044.h"
#include "Effects/Plugins/Plg049.h"
#include "Effects/Plugins/Plg069.h"
#include "Effects/Plugins/Plg073.h"
#include "Effects/Plugins/Plg077.h"
#include "Effects/Plugins/Plg085.h"
#include "Effects/Plugins/Plg129.h"
#include "Effects/Plugins/Plg137.h"
#include "Effects/Plugins/Plg138.h"
#include "Effects/Plugins/Plg143.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg148.h"
#include "Effects/Plugins/Plg150.h"
#include "Effects/Plugins/Plg153.h"
#include "Effects/Plugins/Plg154.h"
#include "Effects/Plugins/Plg160.h"
#include "Effects/Plugins/Plg242.h"
#include "Effects/Plugins/Plg260.h"
#include "Effects/Plugins/Plg265.h"
#include "Effects/Templates/RadialSurge.h"
}

using VS::u16;
using VS::u32;
using VS::u8;
using VS::usize;
namespace Content = VS::Content;
namespace Render = VS::Render;

extern "C" int Tests_ActionDefinitionsLoaded(void);
extern "C" int Tests_DefaultEffectPackageLoaded(void);
extern "C" int TestsRoomSfxBankBinding(void);
extern "C" int TestsDiscReadSizeContract(void);
extern "C" int CheckMenuDiscStringsLoaded(void);

namespace {
constexpr auto kMesh10Data = std::to_array<Content_ModuleDataEntry>({
    {"EFFECT/PLG069.BIN", 0x3C24, 80, Effects_Plg069_mesh10Vertices, 80},
    {"EFFECT/PLG069.BIN", 0x3C74, 96, Effects_Plg069_mesh10Faces, 96},
    {"EFFECT/PLG077.BIN", 0x4108, 80, Effects_Plg077_mesh10Vertices, 80},
    {"EFFECT/PLG077.BIN", 0x4158, 96, Effects_Plg077_mesh10Faces, 96},
    {"EFFECT/PLG129.BIN", 0x3DE8, 80, Effects_Plg129_mesh10Vertices, 80},
    {"EFFECT/PLG129.BIN", 0x3E38, 96, Effects_Plg129_mesh10Faces, 96},
    {"EFFECT/PLG137.BIN", 0x478C, 80, Effects_Plg137_mesh10Vertices, 80},
    {"EFFECT/PLG137.BIN", 0x47DC, 96, Effects_Plg137_mesh10Faces, 96},
    {"EFFECT/PLG153.BIN", 0x3504, 80, Effects_Plg153_mesh10Vertices, 80},
    {"EFFECT/PLG153.BIN", 0x3554, 96, Effects_Plg153_mesh10Faces, 96},
    {"EFFECT/PLG154.BIN", 0x1A9C, 80, Effects_Plg154_mesh10Vertices, 80},
    {"EFFECT/PLG154.BIN", 0x1AEC, 96, Effects_Plg154_mesh10Faces, 96},
    {"EFFECT/PLG260.BIN", 0xB2C, 80, Effects_Plg260_mesh10Vertices, 80},
    {"EFFECT/PLG260.BIN", 0xB7C, 96, Effects_Plg260_mesh10Faces, 96},
});

constexpr auto kTiledRingData = std::to_array<Content_ModuleDataEntry>({
    {"EFFECT/PLG049.BIN", 19448, 32, Effects_Plg049_tiledRingColorFactors, 32},
    {"EFFECT/PLG069.BIN", 15576, 32, Effects_Plg069_tiledRingColorFactors, 32},
    {"EFFECT/PLG073.BIN", 22332, 32, Effects_Plg073_tiledRingColorFactors, 32},
    {"EFFECT/PLG085.BIN", 6752, 32, Effects_Plg085_tiledRingColorFactors, 32},
    {"EFFECT/PLG137.BIN", 18496, 32, Effects_Plg137_tiledRingColorFactors, 32},
    {"EFFECT/PLG138.BIN", 13660, 32, Effects_Plg138_tiledRingColorFactors, 32},
    {"EFFECT/PLG143.BIN", 7780, 32, Effects_Plg143_tiledRingColorFactors, 32},
    {"EFFECT/PLG146.BIN", 24700, 32, Effects_Plg146_tiledRingColorFactors, 32},
    {"EFFECT/PLG148.BIN", 12540, 32, Effects_Plg148_tiledRingColorFactors, 32},
    {"EFFECT/PLG150.BIN", 12828, 32, Effects_Plg150_tiledRingColorFactors, 32},
    {"EFFECT/PLG160.BIN", 6940, 32, Effects_Plg160_tiledRingColorFactors, 32},
    {"EFFECT/PLG242.BIN", 15456, 32, Effects_Plg242_tiledRingColorFactors, 32},
    {"EFFECT/PLG265.BIN", 26008, 32, Effects_Plg265_tiledRingColorFactors, 32},
});

void CheckTemplateReloads();

std::filesystem::path FindRepoDisc() {
  for (auto dir = std::filesystem::current_path();
       dir.has_parent_path() && dir != dir.parent_path();
       dir = dir.parent_path()) {
    if (auto cue = dir / "disks" / "SLUS-01040.cue";
        std::filesystem::exists(cue)) {
      return cue;
    }
  }
  return {};
}
}  // namespace

TEST_CASE("NormalizeDiscPath", "[content]") {
  CHECK(Content::NormalizeDiscPath("\\map\\zone009.znd;1") ==
        "MAP/ZONE009.ZND");
  CHECK(Content::NormalizeDiscPath("/SLUS_010.40") == "SLUS_010.40");
}

TEST_CASE("Module data copies validate both source and destination extents",
          "[content][module]") {
  const std::array<unsigned char, 8> source{0, 1, 2, 3, 4, 5, 6, 7};
  std::array<unsigned char, 4> destination{9, 9, 9, 9};
  Content_ModuleDataEntry entry{"fixture", 2, 4, destination.data(), 4};
  REQUIRE(Content::CopyModuleDataEntry(entry, source));
  CHECK(destination == std::array<unsigned char, 4>{2, 3, 4, 5});
  const auto saved = destination;
  entry.size = 5;
  CHECK_FALSE(Content::CopyModuleDataEntry(entry, source));
  entry.size = 4;
  entry.offset = 6;
  CHECK_FALSE(Content::CopyModuleDataEntry(entry, source));
  entry.offset = UINT32_MAX;
  CHECK_FALSE(Content::CopyModuleDataEntry(entry, source));
  CHECK(destination == saved);
  const std::array<unsigned char, 5> odd_source{1, 2, 3, 4, 5};
  std::array<unsigned char, 6> padded_destination{9, 9, 9, 9, 9, 9};
  Content_ModuleDataEntry odd_entry{"fixture", 0, 5, padded_destination.data(),
                                    6};
  REQUIRE(Content::CopyModuleDataEntry(odd_entry, odd_source));
  CHECK(padded_destination == std::array<unsigned char, 6>{1, 2, 3, 4, 5, 9});
}

TEST_CASE("Module relocation maps native strides and rejects unsafe interiors",
          "[content][module]") {
  std::array<std::uintptr_t, 3> pointers{};
  std::array<unsigned char, 8> bytes{};
  std::array<unsigned char, 16> expanded{};
  const std::array<Content_ModuleSymbol, 4> symbols{{
      {"fixture", 0x1000, 12, pointers.data(), sizeof pointers, 4,
       sizeof pointers[0]},
      {"fixture", 0x2000, 8, expanded.data(), sizeof expanded, 0, 0},
      {"SLUS_010.40", 0x3000, 8, bytes.data(), sizeof bytes, 1, 1},
      {"fixture", 0x4000, 0, expanded.data(), 0, 0, 0},
  }};
  CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x1004) ==
        &pointers[1]);
  CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x1002) == nullptr);
  CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x100C) == nullptr);
  CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x2000) ==
        expanded.data());
  CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x2004) == nullptr);
  CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x3007) == &bytes[7]);
  CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x3008) == nullptr);
  CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x4000) ==
        expanded.data());
  CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x4004) == nullptr);
  CHECK(Content::ResolveModuleAddress(symbols, "other", 0x1000) == nullptr);
}

TEST_CASE("Pointer table relocation preserves destinations on invalid input",
          "[content][module]") {
  std::array<int, 2> values{};
  std::array<void*, 3> destination{&values[0], &values[0], &values[0]};
  std::array<unsigned, 3> addresses{0x1000, 0, 0x1004};
  const std::array<Content_ModuleSymbol, 1> symbols{{
      {"fixture", 0x1000, sizeof values, values.data(), sizeof values,
       sizeof(int), sizeof(int)},
  }};
  const std::span<const unsigned char> image{
      reinterpret_cast<const unsigned char*>(addresses.data()),
      sizeof addresses};
  Content_ModulePointerTable table{"fixture",          0, 3,
                                   destination.data(), 3, nullptr};
  REQUIRE(Content::RelocateModulePointerTable(table, image, symbols));
  CHECK(destination == std::array<void*, 3>{&values[0], nullptr, &values[1]});
  const auto saved = destination;
  addresses[2] = 0x2000;
  CHECK_FALSE(Content::RelocateModulePointerTable(table, image, symbols));
  CHECK(destination == saved);
  addresses[2] = 0x1004;
  table.capacity = 2;
  CHECK_FALSE(Content::RelocateModulePointerTable(table, image, symbols));
  CHECK(destination == saved);
  table.capacity = 3;
  table.offset = UINT32_MAX;
  CHECK_FALSE(Content::RelocateModulePointerTable(table, image, symbols));
  CHECK(destination == saved);
}

TEST_CASE(
    "Extracted US modules initialize every required data and dispatch entry",
    "[content][module][disc]") {
  const auto cue = FindRepoDisc();
  if (cue.empty()) {
    SKIP("Repository disc location is unavailable");
  }
  const auto root = cue.parent_path().parent_path() / "Native" / "data";
  if (!std::filesystem::exists(root / Content::DataStore::ManifestName)) {
    SKIP("Extracted Native/data is unavailable");
  }
  REQUIRE(Content::DataStore::Open(root).has_value());
  REQUIRE(Content_LoadModuleData() == 0);
  CheckTemplateReloads();
  CHECK(Tests_ActionDefinitionsLoaded() == 1);
  CHECK(Tests_DefaultEffectPackageLoaded() == 1);
  CHECK(CheckMenuDiscStringsLoaded() == 1);
  for (unsigned index = 0; index < Content_modulePointerCount; ++index) {
    const auto& table = Content_modulePointers[index];
    INFO(table.module << " offset " << table.offset);
    CHECK(table.count <= table.capacity);
  }
}

TEST_CASE("Disc image: ISO9660 directory and file read", "[content][disc]") {
  const auto cue = FindRepoDisc();
  if (cue.empty()) {
    SKIP("disks\\SLUS-01040.cue not present");
  }
  auto disc = Content::Disc::Open(cue);
  REQUIRE(disc.has_value());
  CHECK((*disc)->IsImage());
  auto exe = (*disc)->Find("SLUS_010.40");
  REQUIRE(exe.has_value());
  auto data = (*disc)->ReadFile(*exe);
  REQUIRE(data.has_value());
  REQUIRE(data->size() > 8);
  CHECK(std::string(reinterpret_cast<const char*>(data->data()), 8) ==
        "PS-X EXE");
  CHECK((*disc)->Find("BATTLE/BATTLE.PRG").has_value());
}

TEST_CASE(
    "Room SFX binding preserves native pointers and adjacent music tables",
    "[content][audio][layout]") {
  REQUIRE(TestsRoomSfxBankBinding() == 1);
}

TEST_CASE("Module relocation rejects null targets and unnamed symbols",
          "[content][module]") {
  constexpr unsigned kAddress = 0x1000;
  constexpr unsigned kExtent = 8;
  std::array<unsigned char, kExtent> target{};
  const std::array<Content_ModuleSymbol, 2> symbols{{
      {"fixture", kAddress, kExtent, nullptr, kExtent, 1, 1},
      {nullptr, kAddress, kExtent, target.data(), kExtent, 1, 1},
  }};
  CHECK(VS::Content::ResolveModuleAddress(symbols, "fixture", kAddress) ==
        nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "fixture", kAddress + 1) ==
        nullptr);
}

TEST_CASE(
    "ISO directory parsing rejects truncated records and overflowing sizes",
    "[content][disc]") {
  struct ImageFile {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("vs-iso-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".iso");
    ~ImageFile() {
      std::error_code error;
      std::filesystem::remove(path, error);
    }
  } image_file;
  constexpr usize kSectorBytes = Content::Disc::SectorData;
  constexpr usize kDescriptor = 16 * kSectorBytes;
  constexpr usize kRoot = kDescriptor + 156;
  constexpr usize kDirectory = 18 * kSectorBytes;
  std::vector<u8> image(19 * kSectorBytes);
  image[kDescriptor] = 1;
  const std::string signature = "CD001";
  std::copy(signature.begin(), signature.end(),
            image.begin() + kDescriptor + 1);
  image[kRoot] = 34;
  image[kRoot + 2] = 18;
  image[kRoot + 11] = 8;
  image[kRoot + 32] = 1;
  const auto open_image = [&] {
    std::ofstream stream(image_file.path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(image.data()),
                 static_cast<std::streamsize>(image.size()));
    stream.close();
    return Content::Disc::Open(image_file.path);
  };
  REQUIRE(open_image().has_value());
  SECTION("record header") { image[kDirectory] = 1; }
  SECTION("record name") {
    image[kDirectory] = 34;
    image[kDirectory + 32] = 40;
  }
  SECTION("directory extent") {
    std::fill_n(image.begin() + kRoot + 10, sizeof(u32), UINT8_MAX);
  }
  CHECK_FALSE(open_image().has_value());
}

TEST_CASE("Converted intro movie decodes without changing page ownership",
          "[content][movie][disc]") {
  const auto cue = FindRepoDisc();
  const auto root = cue.parent_path().parent_path() / "Native" / "data";
  if (cue.empty() ||
      !std::filesystem::exists(root / "disc/MOV/TITLE.STR.mkv")) {
    SKIP("Converted intro movie is unavailable");
  }
  REQUIRE(Content::DataStore::Open(root).has_value());
  struct MovieScope {
    std::vector<u16> pixels;
    ~MovieScope() {
      Movie_PlayerClose();
      Render::GamePages().Upload(
          {0, 0, Render::PageSpace::Width, Render::PageSpace::Height},
          pixels.data());
    }
  } scope{{Render::GamePages().Pixels().begin(),
           Render::GamePages().Pixels().end()}};
  Render::GamePages().Fill(
      {0, 0, Render::PageSpace::Width, Render::PageSpace::Height}, 255, 0, 255);
  const u16 before = Render::GamePages().At(0, 0);
  REQUIRE(Movie_PlayerOpen("MOV/TITLE.STR") == 1);
  REQUIRE(Movie_PlayerDrawFrame(300, 0, 0) == 1);
  CHECK(std::ranges::any_of(Render::GamePages().Pixels(),
                            [before](u16 pixel) { return pixel != before; }));
  Movie_PlayerClose();
  CHECK(Movie_PlayerDrawFrame(301, 0, 0) == 0);
}

TEST_CASE(
    "Disc read rejects unrepresentable sizes without changing request state",
    "[content][disc]") {
  REQUIRE(TestsDiscReadSizeContract() == 1);
}

TEST_CASE("Manifest sector extents do not wrap at uint32 boundaries",
          "[content][manifest]") {
  Content::DataStore::Entry entry{"fixture", 0, UINT32_MAX, "raw"};
  CHECK(entry.SectorCount() == 2097152);
  CHECK(entry.ContainsSector(2097151));
  CHECK_FALSE(entry.ContainsSector(2097152));
}

TEST_CASE("Manifest high-LBA ranges retain their upper boundary",
          "[content][manifest]") {
  Content::DataStore::Entry entry{"fixture", UINT32_MAX - 1, 4096, "raw"};
  CHECK(entry.ContainsSector(UINT32_MAX));
  CHECK_FALSE(entry.ContainsSector(0));
}

TEST_CASE("Manifest sector rounding preserves empty and partial entries",
          "[content][manifest]") {
  Content::DataStore::Entry entry{"fixture", 10, 0, "raw"};
  for (const u32 size : {0u, 1u, 2048u}) {
    entry.size = size;
    CHECK(entry.SectorCount() == 1);
    CHECK(entry.ContainsSector(10));
    CHECK_FALSE(entry.ContainsSector(11));
  }
  entry.size = 2049;
  CHECK(entry.SectorCount() == 2);
}

namespace {
void CheckTemplateBinding(const Content_ModuleDataEntry& expected) {
  INFO(expected.module << " offset " << expected.offset);
  const std::span entries(Content_moduleData, Content_moduleDataCount);
  const auto found = std::ranges::find(entries, expected.target,
                                       &Content_ModuleDataEntry::target);
  REQUIRE(found != entries.end());
  CHECK(std::string_view(found->module) == expected.module);
  CHECK(found->offset == expected.offset);
  REQUIRE(found->size == expected.size);
  REQUIRE(found->capacity == expected.capacity);
}

void CheckMesh10Indices(const Content_ModuleDataEntry& expected,
                        bool vertices) {
  if (vertices) {
    const std::span values(static_cast<const SVECTOR*>(expected.target), 10);
    for (usize index = 0; index < values.size(); ++index) {
      CHECK(values[index].pad == (index < 4 ? 0 : 1));
    }
  } else {
    const std::span values(static_cast<const u8*>(expected.target), 96);
    for (usize index = 0; index < values.size(); ++index) {
      const bool valid = index % 6 < 3
                             ? values[index] < 10
                             : values[index] <= 10 && values[index] % 2 == 0;
      CHECK(valid);
    }
  }
}

void CheckTemplateSourceBytes(const Content::DataStore& store,
                              const Content_ModuleDataEntry& expected) {
  INFO(expected.module << " offset " << expected.offset);
  const auto* file = store.Find(expected.module);
  REQUIRE(file != nullptr);
  std::vector<u8> source(expected.size);
  REQUIRE(store.Read(*file, expected.offset, source));
  const std::span native_bytes(static_cast<const u8*>(expected.target),
                               expected.size);
  CHECK(std::ranges::equal(native_bytes, source));
}

void CheckLoadedMesh10Data(const Content::DataStore& store) {
  for (usize index = 0; index < kMesh10Data.size(); ++index) {
    const auto& expected = kMesh10Data[index];
    CheckTemplateBinding(expected);
    CheckTemplateSourceBytes(store, expected);
    CheckMesh10Indices(expected, index % 2 == 0);
  }
}

void CheckLoadedTiledRingData(const Content::DataStore& store) {
  for (const auto& expected : kTiledRingData) {
    CheckTemplateBinding(expected);
    CheckTemplateSourceBytes(store, expected);
  }
  const Content_ModuleDataEntry initialized{
      "EFFECT/PLG044.BIN", 3832, 32, Effects_Plg044_tiledRingColorFactors, 32};
  CheckTemplateSourceBytes(store, initialized);
  const Content_ModuleDataEntry shared{
      "EFFECT/PLG139.BIN", 6752, 32, Effects_Plg085_tiledRingColorFactors, 32};
  CheckTemplateSourceBytes(store, shared);
}

void CheckTemplateReloads() {
  CheckLoadedMesh10Data(*Content::DataStore::Get());
  CheckLoadedTiledRingData(*Content::DataStore::Get());
  REQUIRE(Content_LoadModuleData() == 0);
  CheckLoadedMesh10Data(*Content::DataStore::Get());
  CheckLoadedTiledRingData(*Content::DataStore::Get());
}
}  // namespace

TEST_CASE("Every Mesh10 instance imports complete vertex and triangle tables",
          "[content][module][mesh10]") {
  const unsigned index = GENERATE(Catch::Generators::range(0u, 14u));
  CheckTemplateBinding(kMesh10Data[index]);
}

TEST_CASE("Every tiled ring imports all eight transparency color modes",
          "[content][module][tiled-ring]") {
  const unsigned index = GENERATE(Catch::Generators::range(0u, 13u));
  CheckTemplateBinding(kTiledRingData[index]);
}

TEST_CASE("Copied model geometry keeps serialized section offsets relative",
          "[content][model-geometry][offsets]") {
  std::array<int, 32> source{};
  for (std::size_t word = 0; word < source.size(); ++word) {
    source[word] = static_cast<int>(word * 3);
  }
  source[12] = GENERATE(0, 128);
  source[13] = 64;
  source[14] = 80;
  source[15] = 96;
  struct {
    std::array<int, 2> before{17, 19};
    std::array<int, 32> model{};
    std::array<int, 2> after{23, 29};
  } destination;
  const auto original = source;
  CHECK(Content_CopyModelGeometry(destination.model.data(), source.data(),
                                  sizeof source) == destination.model.data());
  CHECK(destination.model == source);
  CHECK(source == original);
  CHECK(destination.before == std::array<int, 2>{17, 19});
  CHECK(destination.after == std::array<int, 2>{23, 29});
}

TEST_CASE("Model UV offsets preserve primitive strides and byte wrapping",
          "[content][model-geometry][uv]") {
  const bool colored = GENERATE(false, true);
  const bool reverse = GENERATE(false, true);
  std::array<u8, 72> bytes{};
  bytes.fill(0xA5);
  auto expected = bytes;
  std::size_t start = 4;
  for (int primitive = 0; primitive < 2; ++primitive) {
    const bool triangle = (primitive == 0) != reverse;
    bytes[start + (colored ? 11 : 0)] = expected[start + (colored ? 11 : 0)] =
        static_cast<u8>(colored ? (triangle ? 0x34 : 0x3C)
                                : (triangle ? 0x24 : 0x2C));
    const auto pairs = colored ? (triangle ? std::vector<int>{6, 20, 22}
                                           : std::vector<int>{24, 26, 28, 30})
                               : (triangle ? std::vector<int>{10, 12, 14}
                                           : std::vector<int>{12, 14, 16, 18});
    for (const auto offset : pairs) {
      bytes[start + offset] = 250;
      bytes[start + offset + 1] = 3;
      expected[start + offset] = 4;
      expected[start + offset + 1] = 252;
    }
    start += colored ? (triangle ? 24 : 32) : (triangle ? 16 : 20);
  }
  Content_OffsetModelTextureCoordinates(bytes.data() + 4, 2, colored, 10, -7);
  CHECK(bytes == expected);
}

TEST_CASE("LFO relocation imports source data instead of host code pointers",
          "[content][module][lfo-source]") {
  std::array<std::int16_t, 5> samples{1000, -1000, 0, 0, -2};
  std::array<unsigned, 66> addresses{};
  addresses[65] = 0x1000;
  std::array<void*, 66> destination{};
  int host_code = 7;
  const Content_ModuleSymbol code{"fixture", 0x1000, 0, &host_code, 0, 0, 0};
  const Content_ModuleSymbol source{
      "fixture", 0x1000, sizeof samples, samples.data(), sizeof samples, 2, 2};
  const std::span<const unsigned char> image{
      reinterpret_cast<const unsigned char*>(addresses.data()),
      sizeof addresses};
  Content_ModulePointerTable table{"fixture",          0,  66,
                                   destination.data(), 66, &source};
  REQUIRE(
      Content::RelocateModulePointerTable(table, image, std::span(&code, 1)));
  CHECK(destination[65] == samples.data());
  CHECK(destination[0] == nullptr);
}

TEST_CASE("Native LFO catalogue covers the encoded unsigned byte domain",
          "[content][module][lfo-source]") {
  CHECK(std::size(Audio_Seq_kLfoWaveforms) == 256);
}

TEST_CASE(
    "Unbounded source LFO programs remain unavailable rather than fabricated",
    "[content][module][lfo-source]") {
  std::array<std::int16_t, 8> samples{1000, -1000, 0, 0, -2, 0, 0, 32767};
  std::array<unsigned, 2> addresses{0x1000, 0x100A};
  std::array<void*, 2> destination{};
  const Content_ModuleSymbol source{
      "fixture", 0x1000, sizeof samples, samples.data(), sizeof samples, 2, 2};
  const std::span<const unsigned char> image{
      reinterpret_cast<const unsigned char*>(addresses.data()),
      sizeof addresses};
  Content_ModulePointerTable table{"fixture",          0, 2,
                                   destination.data(), 2, &source};
  REQUIRE(Content::RelocateModulePointerTable(table, image, {}));
  CHECK(destination[0] == samples.data());
  CHECK(destination[1] == nullptr);
}

TEST_CASE(
    "Invalid LFO source ownership preserves the complete destination table",
    "[content][module][lfo-source]") {
  std::array<std::int16_t, 5> samples{1000, -1000, 0, 0, -2};
  std::array<unsigned, 2> addresses{0x1000, 0x1001};
  int sentinel = 7;
  std::array<void*, 2> destination{&sentinel, &sentinel};
  const auto saved = destination;
  Content_ModuleSymbol source{
      "fixture", 0x1000, sizeof samples, samples.data(), sizeof samples, 2, 2};
  const std::span<const unsigned char> image{
      reinterpret_cast<const unsigned char*>(addresses.data()),
      sizeof addresses};
  Content_ModulePointerTable table{"fixture",          0, 2,
                                   destination.data(), 2, &source};
  CHECK_FALSE(Content::RelocateModulePointerTable(table, image, {}));
  CHECK(destination == saved);
  addresses[1] = 0x1000;
  source.nativeSize = 1;
  CHECK_FALSE(Content::RelocateModulePointerTable(table, image, {}));
  CHECK(destination == saved);
}

namespace {
void LoadOriginalLfoModules() {
  const auto cue = FindRepoDisc();
  if (cue.empty()) SKIP("Repository disc location is unavailable");
  const auto root = cue.parent_path().parent_path() / "Native" / "data";
  if (!std::filesystem::exists(root / Content::DataStore::ManifestName)) {
    SKIP("Extracted Native/data is unavailable");
  }
  REQUIRE(Content::DataStore::Open(root).has_value());
  REQUIRE(Content_LoadModuleData() == 0);
}

std::vector<u8> ReadOriginalLfoImage() {
  const auto* store = Content::DataStore::Get();
  REQUIRE(store != nullptr);
  const auto* entry = store->Find("SLUS_010.40");
  REQUIRE(entry != nullptr);
  auto image = store->ReadAll(*entry);
  REQUIRE(image.has_value());
  return std::move(*image);
}

const Content_ModuleSymbol& LoadedLfoOwner() {
  const std::span tables(Content_modulePointers, Content_modulePointerCount);
  const auto found = std::ranges::find(
      tables, reinterpret_cast<void**>(Audio_Seq_kLfoWaveforms),
      &Content_ModulePointerTable::target);
  REQUIRE(found != tables.end());
  REQUIRE(found->sampleSource != nullptr);
  return *found->sampleSource;
}

void CheckOriginalLfoOwner(const Content_ModuleSymbol& owner,
                           std::span<const u8> image) {
  REQUIRE(owner.address == 0x80010000);
  REQUIRE(owner.size == 0x52000);
  REQUIRE(owner.nativeSize == owner.size);
  CHECK(std::memcmp(owner.target, image.data() + 0x800, owner.size) == 0);
}

std::int16_t ReadOriginalLfoSample(std::span<const u8> image,
                                   std::uint32_t address) {
  REQUIRE(address >= 0x80010000);
  const auto offset = std::uint64_t{0x800} + address - 0x80010000;
  REQUIRE(offset + 2 <= image.size());
  const auto word = static_cast<std::uint16_t>(
      image[offset] | (static_cast<unsigned>(image[offset + 1]) << 8));
  return std::bit_cast<std::int16_t>(word);
}

std::int16_t AdvanceOriginalLfo(std::span<const u8> image,
                                std::uint32_t& address) {
  if (ReadOriginalLfoSample(image, address) == 0 &&
      ReadOriginalLfoSample(image, address + 2) == 0) {
    const auto delta = ReadOriginalLfoSample(image, address + 4);
    address = static_cast<std::uint32_t>(static_cast<std::int64_t>(address) +
                                         static_cast<std::int64_t>(delta) * 2);
  }
  const auto value = ReadOriginalLfoSample(image, address);
  address += 2;
  return value;
}

void CheckOriginalLfoProgram(std::span<const u8> image,
                             const Content_ModuleSymbol& owner,
                             unsigned selector) {
  const auto offset = std::size_t{0x209A0} + selector * sizeof(std::uint32_t);
  std::uint32_t address = 0;
  std::memcpy(&address, image.data() + offset, sizeof address);
  auto* cursor = Audio_Seq_kLfoWaveforms[selector];
  REQUIRE(reinterpret_cast<const u8*>(cursor) ==
          static_cast<const u8*>(owner.target) + (address - owner.address));
  for (int sample = 0; sample < 1024; ++sample) {
    const auto expected = AdvanceOriginalLfo(image, address);
    if (cursor[0] == 0 && cursor[1] == 0) cursor += cursor[2];
    CHECK(*cursor++ == expected);
  }
}
}  // namespace

TEST_CASE("Original LFO source aliases retain every bounded sample and loop",
          "[content][module][lfo-source][disc]") {
  LoadOriginalLfoModules();
  const auto image = ReadOriginalLfoImage();
  const auto& owner = LoadedLfoOwner();
  CheckOriginalLfoOwner(owner, image);
  REQUIRE(Audio_Seq_kLfoWaveforms[65] != nullptr);
  unsigned supported = 0;
  for (unsigned selector = 0; selector < 256; ++selector) {
    if (Audio_Seq_kLfoWaveforms[selector] != nullptr) {
      CheckOriginalLfoProgram(image, owner, selector);
      ++supported;
    }
  }
  CHECK(supported == 231);
}

namespace {
void CheckPlg074CursorReload(std::span<const u8> image) {
  const std::span entries(Content_moduleData, Content_moduleDataCount);
  const auto cursor = std::ranges::find(
      entries, static_cast<void*>(Content_plg074TrailEmissionCounter),
      &Content_ModuleDataEntry::target);
  REQUIRE(cursor != entries.end());
  CHECK(std::string_view(cursor->module) == "EFFECT/PLG074.BIN");
  CHECK(cursor->offset == 0x577C);
  CHECK(cursor->size == 4);
  CHECK(cursor->capacity == sizeof(int));
  const auto saved_first = Effects_Plg073_trailEmissionCounter;
  const auto saved_second = Content_plg074TrailEmissionCounter[0];
  Effects_Plg073_trailEmissionCounter = 5;
  Content_plg074TrailEmissionCounter[0] = 7;
  CHECK(Content::CopyModuleDataEntry(*cursor, image));
  CHECK(Content_plg074TrailEmissionCounter[0] == 0);
  CHECK(Effects_Plg073_trailEmissionCounter == 5);
  Effects_Plg073_trailEmissionCounter = saved_first;
  Content_plg074TrailEmissionCounter[0] = saved_second;
}
}  // namespace

TEST_CASE(
    "PLG074 source sharing preserves identical code and independent state",
    "[content][module][plg074][disc]") {
  LoadOriginalLfoModules();
  const auto* store = Content::DataStore::Get();
  const auto* first = store->Find("EFFECT/PLG073.BIN");
  const auto* second = store->Find("EFFECT/PLG074.BIN");
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  const auto original = store->ReadAll(*first);
  const auto duplicate = store->ReadAll(*second);
  REQUIRE(original.has_value());
  REQUIRE(duplicate.has_value());
  REQUIRE(original->size() == 22400);
  CHECK(*original == *duplicate);
  CheckPlg074CursorReload(*duplicate);
}

TEST_CASE("PLG073 and PLG074 import all eight ring-arc color rows",
          "[content][module][plg074][ring-arc][disc]") {
  const Content_ModuleDataEntry expected{"EFFECT/PLG073.BIN", 0x575C, 32,
                                         Effects_Plg073_ringColorFactors, 32};
  CheckTemplateBinding(expected);
  LoadOriginalLfoModules();
  CheckTemplateSourceBytes(*Content::DataStore::Get(), expected);
  REQUIRE(Content_LoadModuleData() == 0);
  CheckTemplateSourceBytes(*Content::DataStore::Get(), expected);
}
