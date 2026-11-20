#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Actors/SequencePlaybackLive.hpp"
#include "Audio/Seq/Sequencer.hpp"
#include "Battle/Damage.h"
#include "Content/ActorAssetsLive.hpp"
#include "Content/DataAccess.hpp"
#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Content/DiscLayout.h"
#include "Content/ModuleData.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Content/PFile.hpp"
#include "Core/Log.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/Package.hpp"
#include "Effects/Plugins/Plg044.hpp"
#include "Effects/Plugins/Plg049.hpp"
#include "Effects/Plugins/Plg050.hpp"
#include "Effects/Plugins/Plg053.hpp"
#include "Effects/Plugins/Plg061.hpp"
#include "Effects/Plugins/Plg065.hpp"
#include "Effects/Plugins/Plg069.hpp"
#include "Effects/Plugins/Plg073.hpp"
#include "Effects/Plugins/Plg077.hpp"
#include "Effects/Plugins/Plg081.hpp"
#include "Effects/Plugins/Plg082.hpp"
#include "Effects/Plugins/Plg085.hpp"
#include "Effects/Plugins/Plg086.hpp"
#include "Effects/Plugins/Plg087.hpp"
#include "Effects/Plugins/Plg128.hpp"
#include "Effects/Plugins/Plg129.hpp"
#include "Effects/Plugins/Plg134.hpp"
#include "Effects/Plugins/Plg135.hpp"
#include "Effects/Plugins/Plg137.hpp"
#include "Effects/Plugins/Plg138.hpp"
#include "Effects/Plugins/Plg142.hpp"
#include "Effects/Plugins/Plg143.hpp"
#include "Effects/Plugins/Plg146.hpp"
#include "Effects/Plugins/Plg147.hpp"
#include "Effects/Plugins/Plg148.hpp"
#include "Effects/Plugins/Plg149.hpp"
#include "Effects/Plugins/Plg150.hpp"
#include "Effects/Plugins/Plg153.hpp"
#include "Effects/Plugins/Plg154.hpp"
#include "Effects/Plugins/Plg158.hpp"
#include "Effects/Plugins/Plg160.hpp"
#include "Effects/Plugins/Plg161.hpp"
#include "Effects/Plugins/Plg162.hpp"
#include "Effects/Plugins/Plg166.hpp"
#include "Effects/Plugins/Plg167.hpp"
#include "Effects/Plugins/Plg169.hpp"
#include "Effects/Plugins/Plg170.hpp"
#include "Effects/Plugins/Plg236.hpp"
#include "Effects/Plugins/Plg242.hpp"
#include "Effects/Plugins/Plg250.hpp"
#include "Effects/Plugins/Plg260.hpp"
#include "Effects/Plugins/Plg263.hpp"
#include "Effects/Plugins/Plg265.hpp"
#include "Effects/Plugins/Plg272.hpp"
#include "Effects/Plugins/Plg291.hpp"
#include "Effects/Plugins/Plg297.hpp"
#include "Effects/Plugins/Plg300.hpp"
#include "Effects/Plugins/Plg301.hpp"
#include "Effects/PurgeTables.hpp"
#include "Effects/TrailEmitterLive.hpp"
#include "Effects/Transforms.hpp"
#include "Field/MapProgress.hpp"
#include "Field/RoomLoading.hpp"
#include "Legacy/Psx.h"
#include "ModelLoading.h"
#include "Movie/MoviePlayer.hpp"
#include "Render/PageSpace.hpp"

using VS::u16;
using VS::u32;
using VS::u8;
using VS::usize;
namespace Content = VS::Content;
namespace Render = VS::Render;

int Tests_ActionDefinitionsLoaded(void);
int Tests_DefaultEffectPackageLoaded(void);
int TestsRoomSfxBankBinding(void);
int TestsDiscReadSizeContract(void);
int CheckMenuDiscStringsLoaded(void);
// Reviewed source blob, defined with the generated module data (#280).
extern unsigned int Content_battleTriggerMarkerBytes[29];

namespace {
constexpr auto kMesh10Data = std::to_array<VS::Content::ModuleDataEntry>({
    {"EFFECT/PLG069.BIN", 0x3C24, 80, VS::Effects::Plg069::mesh10Vertices, 80},
    {"EFFECT/PLG069.BIN", 0x3C74, 96, VS::Effects::Plg069::mesh10Faces, 96},
    {"EFFECT/PLG077.BIN", 0x4108, 80, VS::Effects::Plg077::mesh10Vertices, 80},
    {"EFFECT/PLG077.BIN", 0x4158, 96, VS::Effects::Plg077::mesh10Faces, 96},
    {"EFFECT/PLG129.BIN", 0x3DE8, 80, VS::Effects::Plg129::mesh10Vertices, 80},
    {"EFFECT/PLG129.BIN", 0x3E38, 96, VS::Effects::Plg129::mesh10Faces, 96},
    {"EFFECT/PLG137.BIN", 0x478C, 80, VS::Effects::Plg137::mesh10Vertices, 80},
    {"EFFECT/PLG137.BIN", 0x47DC, 96, VS::Effects::Plg137::mesh10Faces, 96},
    {"EFFECT/PLG153.BIN", 0x3504, 80, VS::Effects::Plg153::mesh10Vertices, 80},
    {"EFFECT/PLG153.BIN", 0x3554, 96, VS::Effects::Plg153::mesh10Faces, 96},
    {"EFFECT/PLG154.BIN", 0x1A9C, 80, VS::Effects::Plg154::mesh10Vertices, 80},
    {"EFFECT/PLG154.BIN", 0x1AEC, 96, VS::Effects::Plg154::mesh10Faces, 96},
    {"EFFECT/PLG260.BIN", 0xB2C, 80, VS::Effects::Plg260::mesh10Vertices, 80},
    {"EFFECT/PLG260.BIN", 0xB7C, 96, VS::Effects::Plg260::mesh10Faces, 96},
});

constexpr auto kTiledRingData = std::to_array<VS::Content::ModuleDataEntry>({
    {"EFFECT/PLG049.BIN", 19448, 32, VS::Effects::Plg049::tiledRingColorFactors,
     32},
    {"EFFECT/PLG069.BIN", 15576, 32, VS::Effects::Plg069::tiledRingColorFactors,
     32},
    {"EFFECT/PLG073.BIN", 22332, 32, VS::Effects::Plg073::tiledRingColorFactors,
     32},
    {"EFFECT/PLG085.BIN", 6752, 32, VS::Effects::Plg085::tiledRingColorFactors,
     32},
    {"EFFECT/PLG137.BIN", 18496, 32, VS::Effects::Plg137::tiledRingColorFactors,
     32},
    {"EFFECT/PLG138.BIN", 13660, 32, VS::Effects::Plg138::tiledRingColorFactors,
     32},
    {"EFFECT/PLG143.BIN", 7780, 32, VS::Effects::Plg143::tiledRingColorFactors,
     32},
    {"EFFECT/PLG146.BIN", 24700, 32, VS::Effects::Plg146::tiledRingColorFactors,
     32},
    {"EFFECT/PLG148.BIN", 12540, 32, VS::Effects::Plg148::tiledRingColorFactors,
     32},
    {"EFFECT/PLG150.BIN", 12828, 32, VS::Effects::Plg150::tiledRingColorFactors,
     32},
    {"EFFECT/PLG160.BIN", 6940, 32, VS::Effects::Plg160::tiledRingColorFactors,
     32},
    {"EFFECT/PLG242.BIN", 15456, 32, VS::Effects::Plg242::tiledRingColorFactors,
     32},
    {"EFFECT/PLG265.BIN", 26008, 32, VS::Effects::Plg265::tiledRingColorFactors,
     32},
});

// Ring and closed ring palettes (#289): eight four-byte entries each.
constexpr auto kRingPaletteData = std::to_array<VS::Content::ModuleDataEntry>({
    {"EFFECT/PLG049.BIN", 19416, 32, VS::Effects::Plg049::ringColorFactors, 32},
    {"EFFECT/PLG050.BIN", 11812, 32, VS::Effects::Plg050::ringColorFactors, 32},
    {"EFFECT/PLG053.BIN", 6868, 32, VS::Effects::Plg053::closedRingColorFactors,
     32},
    {"EFFECT/PLG061.BIN", 13584, 32, VS::Effects::Plg061::ringColorFactors, 32},
    {"EFFECT/PLG061.BIN", 13616, 32,
     VS::Effects::Plg061::closedRingColorFactors, 32},
    {"EFFECT/PLG065.BIN", 16692, 32,
     VS::Effects::Plg065::closedRingColorFactors, 32},
    {"EFFECT/PLG065.BIN", 16724, 32, VS::Effects::Plg065::ringColorFactors, 32},
    {"EFFECT/PLG077.BIN", 16824, 32, VS::Effects::Plg077::ringColorFactors, 32},
    {"EFFECT/PLG077.BIN", 17496, 32, VS::Effects::Plg077::ringColorFactors2,
     32},
    {"EFFECT/PLG081.BIN", 10828, 32, VS::Effects::Plg081::ringColorFactors, 32},
    {"EFFECT/PLG081.BIN", 10860, 32,
     VS::Effects::Plg081::closedRingColorFactors, 32},
    {"EFFECT/PLG082.BIN", 14476, 32,
     VS::Effects::Plg082::closedRingColorFactors, 32},
    {"EFFECT/PLG086.BIN", 19344, 32,
     VS::Effects::Plg086::closedRingColorFactors, 32},
    {"EFFECT/PLG087.BIN", 25052, 32,
     VS::Effects::Plg087::closedRingColorFactors, 32},
    {"EFFECT/PLG128.BIN", 28528, 32,
     VS::Effects::Plg128::closedRingColorFactors, 32},
    {"EFFECT/PLG129.BIN", 15784, 32, VS::Effects::Plg129::ringColorFactors, 32},
    {"EFFECT/PLG129.BIN", 16664, 32, VS::Effects::Plg129::ringColorFactors2,
     32},
    {"EFFECT/PLG134.BIN", 12656, 32,
     VS::Effects::Plg134::closedRingColorFactors, 32},
    {"EFFECT/PLG135.BIN", 12652, 32,
     VS::Effects::Plg135::closedRingColorFactors, 32},
    {"EFFECT/PLG138.BIN", 13628, 32, VS::Effects::Plg138::ringColorFactors, 32},
    {"EFFECT/PLG142.BIN", 13752, 32,
     VS::Effects::Plg142::closedRingColorFactors, 32},
    {"EFFECT/PLG142.BIN", 13784, 32, VS::Effects::Plg142::ringColorFactors, 32},
    {"EFFECT/PLG143.BIN", 7812, 32, VS::Effects::Plg143::closedRingColorFactors,
     32},
    {"EFFECT/PLG146.BIN", 25376, 32, VS::Effects::Plg146::ringColorFactors, 32},
    {"EFFECT/PLG147.BIN", 8976, 32, VS::Effects::Plg147::ringColorFactors, 32},
    {"EFFECT/PLG149.BIN", 3108, 32, VS::Effects::Plg149::ringColorFactors, 32},
    {"EFFECT/PLG150.BIN", 13500, 32, VS::Effects::Plg150::ringColorFactors, 32},
    {"EFFECT/PLG153.BIN", 13748, 32, VS::Effects::Plg153::ringColorFactors, 32},
    {"EFFECT/PLG154.BIN", 6988, 32, VS::Effects::Plg154::closedRingColorFactors,
     32},
    {"EFFECT/PLG158.BIN", 15160, 32,
     VS::Effects::Plg158::closedRingColorFactors, 32},
    {"EFFECT/PLG160.BIN", 6972, 32, VS::Effects::Plg160::ringColorFactors, 32},
    {"EFFECT/PLG161.BIN", 21608, 32, VS::Effects::Plg161::ringColorFactors, 32},
    {"EFFECT/PLG161.BIN", 21644, 32, VS::Effects::Plg161::ringColorFactors2,
     32},
    {"EFFECT/PLG162.BIN", 14732, 32, VS::Effects::Plg162::ringColorFactors, 32},
    {"EFFECT/PLG162.BIN", 14764, 32, VS::Effects::Plg162::ringColorFactors2,
     32},
    {"EFFECT/PLG162.BIN", 14796, 32,
     VS::Effects::Plg162::closedRingColorFactors, 32},
    {"EFFECT/PLG166.BIN", 14656, 32,
     VS::Effects::Plg166::closedRingColorFactors, 32},
    {"EFFECT/PLG166.BIN", 14688, 32, VS::Effects::Plg166::ringColorFactors, 32},
    {"EFFECT/PLG167.BIN", 16420, 32,
     VS::Effects::Plg167::closedRingColorFactors, 32},
    {"EFFECT/PLG169.BIN", 15764, 32, VS::Effects::Plg169::ringColorFactors, 32},
    {"EFFECT/PLG169.BIN", 15796, 32,
     VS::Effects::Plg169::closedRingColorFactors, 32},
    {"EFFECT/PLG170.BIN", 20548, 32, VS::Effects::Plg170::ringColorFactors, 32},
    {"EFFECT/PLG170.BIN", 21220, 32,
     VS::Effects::Plg170::closedRingColorFactors, 32},
    {"EFFECT/PLG236.BIN", 14736, 32,
     VS::Effects::Plg236::closedRingColorFactors, 32},
    {"EFFECT/PLG236.BIN", 14768, 32, VS::Effects::Plg236::ringColorFactors, 32},
    {"EFFECT/PLG236.BIN", 14800, 32, VS::Effects::Plg236::ringColorFactors2,
     32},
    {"EFFECT/PLG250.BIN", 7720, 32, VS::Effects::Plg250::closedRingColorFactors,
     32},
    {"EFFECT/PLG263.BIN", 18224, 32, VS::Effects::Plg263::ringColorFactors, 32},
    {"EFFECT/PLG263.BIN", 18256, 32, VS::Effects::Plg263::ringColorFactors2,
     32},
    {"EFFECT/PLG272.BIN", 9840, 32, VS::Effects::Plg272::closedRingColorFactors,
     32},
    {"EFFECT/PLG291.BIN", 15580, 32,
     VS::Effects::Plg291::closedRingColorFactors, 32},
    {"EFFECT/PLG297.BIN", 13808, 32,
     VS::Effects::Plg297::closedRingColorFactors, 32},
    {"EFFECT/PLG297.BIN", 13840, 32, VS::Effects::Plg297::ringColorFactors, 32},
    {"EFFECT/PLG300.BIN", 19280, 32, VS::Effects::Plg300::ringColorFactors, 32},
    {"EFFECT/PLG300.BIN", 19312, 32, VS::Effects::Plg300::ringColorFactors2,
     32},
    {"EFFECT/PLG301.BIN", 11560, 32,
     VS::Effects::Plg301::closedRingColorFactors, 32},
});

void CheckTemplateReloads();

struct DiscReadLogScope {
  static constexpr unsigned kSector = 123;
  static constexpr unsigned kSize = 4;
  Content::DataStore::ScopedOverride store;
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("vs-disc-read-log-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::string asset = root.filename().string() + ".BIN";
  const std::array<u8, kSize> expected{1, 2, 3, 4};
  VS::Core::LogObserver previous_observer = VS::Core::SetLogObserver({});
  VS::Core::LogLevel previous_console =
      VS::Core::SetConsoleLogLevel(VS::Core::LogLevel::Off);

  ~DiscReadLogScope() noexcept(false) {
    VS::Core::SetLogObserver(std::move(previous_observer));
    VS::Core::SetConsoleLogLevel(previous_console);
    VS::Core::SetLogLevel(VS::Core::LogLevel::Info);
    VS::Core::SetLogChannels("*");
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }

  void Open() const {
    std::filesystem::create_directories(root / "disc");
    std::ofstream manifest(root / Content::DataStore::ManifestName);
    manifest << "lba\tsize\tform\tpath\n"
             << kSector << '\t' << kSize << "\traw\t" << asset << '\n';
    manifest.close();
    REQUIRE(manifest.good());
    std::ofstream file(root / "disc" / asset, std::ios::binary);
    file.write(reinterpret_cast<const char*>(expected.data()), kSize);
    file.close();
    REQUIRE(file.good());
    REQUIRE(Content::DataStore::Open(root).has_value());
    VS::Core::SetLogLevel(VS::Core::LogLevel::Debug);
    REQUIRE(VS::Core::SetLogChannels("content"));
  }
};

void CheckDiscReadPayload(const DiscReadLogScope& scope) {
  std::array<u8, DiscReadLogScope::kSize> buffer{};
  REQUIRE(VS::Content::ReadDiscBytes(DiscReadLogScope::kSector,
                                     DiscReadLogScope::kSize,
                                     buffer.data()) == 1);
  CHECK(buffer == scope.expected);
}

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
  VS::Content::ModuleDataEntry entry{"fixture", 2, 4, destination.data(), 4};
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
  VS::Content::ModuleDataEntry odd_entry{"fixture", 0, 5,
                                         padded_destination.data(), 6};
  REQUIRE(Content::CopyModuleDataEntry(odd_entry, odd_source));
  CHECK(padded_destination == std::array<unsigned char, 6>{1, 2, 3, 4, 5, 9});
}

TEST_CASE("Module relocation maps native strides and rejects unsafe interiors",
          "[content][module]") {
  std::array<std::uintptr_t, 3> pointers{};
  std::array<unsigned char, 8> bytes{};
  std::array<unsigned char, 16> expanded{};
  const std::array<VS::Content::ModuleSymbol, 4> symbols{{
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
  const std::array<VS::Content::ModuleSymbol, 1> symbols{{
      {"fixture", 0x1000, sizeof values, values.data(), sizeof values,
       sizeof(int), sizeof(int)},
  }};
  const std::span<const unsigned char> image{
      reinterpret_cast<const unsigned char*>(addresses.data()),
      sizeof addresses};
  VS::Content::ModulePointerTable table{"fixture",          0, 3,
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
  REQUIRE(VS::Content::LoadModuleData() == 0);
  CheckTemplateReloads();
  CHECK(Tests_ActionDefinitionsLoaded() == 1);
  CHECK(Tests_DefaultEffectPackageLoaded() == 1);
  CHECK(CheckMenuDiscStringsLoaded() == 1);
  for (const auto& table : VS::Content::ModulePointerTables()) {
    INFO(table.module << " offset " << table.offset);
    CHECK(table.count <= table.capacity);
  }
}

namespace {

constexpr std::size_t kTrapAuditTransformCapacity = 64;
constexpr std::size_t kTrapAuditRoomCapacity = 1024;
constexpr std::size_t kTrapAuditRoomBytes = std::size_t{8} * 1024 * 1024;
constexpr std::size_t kFloorTriggerRecordBytes = 12;
constexpr std::size_t kFloorTriggerKindOffset = 6;
constexpr std::uint16_t kNonTrapFloorTriggerKind = 0x10;

std::uint32_t TrapAuditWord(std::span<const std::byte> bytes,
                            std::size_t offset) {
  return std::to_integer<std::uint8_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(
              std::to_integer<std::uint8_t>(bytes[offset + 1]))
          << 8) |
         (static_cast<std::uint32_t>(
              std::to_integer<std::uint8_t>(bytes[offset + 2]))
          << 16) |
         (static_cast<std::uint32_t>(
              std::to_integer<std::uint8_t>(bytes[offset + 3]))
          << 24);
}

std::span<const std::byte> TrapAuditFloorRecords(
    std::span<const std::byte> file, std::size_t& empty_rooms) {
  const auto layout = VS::Field::DecodeRoomFile(file);
  if (!layout) {
    FAIL("Room file must have bounded section ranges");
    return {};
  }
  REQUIRE(layout);
  if (layout->room_sections.bytes == 0) {
    ++empty_rooms;
    return {};
  }
  const auto declared_bytes = TrapAuditWord(file, 4);
  REQUIRE(declared_bytes <= layout->room_sections.bytes);
  const auto room = file.subspan(layout->room_sections.offset, declared_bytes);
  if (room.size() < VS::Field::kRoomSectionHeaderBytes) {
    REQUIRE(std::ranges::all_of(
        room, [](std::byte value) { return value == std::byte{}; }));
    ++empty_rooms;
    return {};
  }
  const auto sections = VS::Field::DecodeRoomSectionLayout(room);
  if (!sections) {
    FAIL("Nonempty room sections must be bounded");
    return {};
  }
  REQUIRE(sections);
  const auto section =
      static_cast<std::size_t>(VS::Field::RoomSectionId::kFloorTriggers);
  const auto records = room.subspan(
      VS::Field::kRoomSectionHeaderBytes + sections->offsets[section],
      sections->lengths[section]);
  REQUIRE(records.size() % kFloorTriggerRecordBytes == 0);
  return records;
}

void CollectRoomTrapKinds(const Content::DataStore& store,
                          const Content::DataStore::Entry& entry,
                          std::vector<std::uint16_t>& kinds,
                          std::size_t& empty_rooms) {
  INFO(entry.path);
  REQUIRE(entry.size <= kTrapAuditRoomBytes);
  const auto bytes = store.ReadAll(entry);
  REQUIRE(bytes);
  const auto records =
      TrapAuditFloorRecords(std::as_bytes(std::span(*bytes)), empty_rooms);
  for (std::size_t offset = 0; offset < records.size();
       offset += kFloorTriggerRecordBytes) {
    const auto kind = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(
            records[offset + kFloorTriggerKindOffset]) |
        (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(
             records[offset + kFloorTriggerKindOffset + 1]))
         << 8));
    if (kind != kNonTrapFloorTriggerKind &&
        std::ranges::find(kinds, kind) == kinds.end()) {
      kinds.push_back(kind);
    }
  }
}

void CollectOriginalTrapKinds(const Content::DataStore& store,
                              std::vector<std::uint16_t>& kinds) {
  std::size_t rooms = 0;
  std::size_t empty_rooms = 0;
  for (const auto& entry : store.Entries()) {
    if (entry.path.starts_with("MAP/") && entry.path.ends_with(".MPD")) {
      REQUIRE(++rooms <= kTrapAuditRoomCapacity);
      CollectRoomTrapKinds(store, entry, kinds, empty_rooms);
    }
  }
  REQUIRE(rooms != 0);
  REQUIRE(empty_rooms < rooms);
  REQUIRE_FALSE(kinds.empty());
}

struct TrapPackageAuditHost final : VS::Effects::EffectPackageHost {
  void BindTextureTables(std::span<std::byte>) override {}
  void BindSoundBank(std::span<std::byte>) override {}
  std::int32_t BindAuxiliarySound(std::span<std::byte>) override { return 0; }
};

void CheckTrapSourcePointIndependence(
    std::span<const VS::Effects::TransformDefinition> definitions) {
  std::array<VS::Effects::TransformState, kTrapAuditTransformCapacity> first{};
  std::array<VS::Effects::TransformState, kTrapAuditTransformCapacity> second{};
  VS::Effects::TransformEndpoints endpoints{
      .emit_from = {.type = 5, .position = {123, 234, 345, 0}},
      .emit_to = {.type = 4, .actor = 0, .part = 0xFD}};
  CHECK(VS::Effects::InitializeTransformStates(definitions, endpoints, first) ==
        definitions.size());
  endpoints.emit_from.position = {-321, -432, -543, 0};
  CHECK(VS::Effects::InitializeTransformStates(definitions, endpoints,
                                               second) == definitions.size());
  for (std::size_t index = 0; index < definitions.size(); ++index) {
    INFO("definition " << index << " flags " << definitions[index].flags);
    CHECK(first[index] == second[index]);
  }
}

void CheckTrapPackageSourcePoints(const Content::DataStore& store,
                                  std::uint16_t effect_id,
                                  std::string_view path) {
  INFO(path);
  const auto* entry = store.Find(path);
  REQUIRE(entry != nullptr);
  REQUIRE(entry->size <= 65535);
  CHECK(entry->lba == VS_E000_P_LBA + VS::Effects::packageFiles[effect_id].lba);
  CHECK(entry->SectorCount() * Content::DataStore::kDataSectorBytes ==
        VS::Effects::packageFiles[effect_id].size);
  auto bytes = store.ReadAll(*entry);
  REQUIRE(bytes);
  TrapPackageAuditHost host;
  VS::Effects::EffectPackageBlocks blocks;
  const auto walk = VS::Effects::BindEffectPackageBlocks(
      std::as_writable_bytes(std::span(*bytes)), blocks, host);
  REQUIRE(walk.status == VS::Effects::PackageWalkStatus::kEnded);
  std::array<VS::Effects::TransformDefinition, kTrapAuditTransformCapacity>
      definitions{};
  const auto count = VS::Effects::ReadTransformDefinitions(
      blocks.transform_definitions, definitions);
  REQUIRE(blocks.transform_definitions.size() %
              VS::Effects::kTransformDefinitionBytes ==
          0);
  REQUIRE(count == blocks.transform_definitions.size() /
                       VS::Effects::kTransformDefinitionBytes);
  CheckTrapSourcePointIndependence(std::span(definitions).first(count));
}

}  // namespace

TEST_CASE("Initial trap packages do not select their unwritten source point",
          "[content][trap-source-audit][disc]") {
  const auto cue = FindRepoDisc();
  if (cue.empty()) {
    SKIP("Repository disc location is unavailable");
  }
  const auto root = cue.parent_path().parent_path() / "Native" / "data";
  Content::DataStore::ScopedOverride restore;
  REQUIRE(Content::DataStore::Open(root).has_value());
  const auto* store = Content::DataStore::Get();
  REQUIRE(store != nullptr);
  constexpr std::array<std::pair<std::uint16_t, std::string_view>, 3> kPackages{
      {{6, "EFFECT/E006.P"}, {7, "EFFECT/E007.P"}, {8, "EFFECT/E008.P"}}};
  for (const auto& [effect_id, path] : kPackages) {
    CheckTrapPackageSourcePoints(*store, effect_id, path);
  }
}

TEST_CASE("Original room trap kinds have bounded source-package mappings",
          "[content][trap-kind-audit][disc]") {
  const auto cue = FindRepoDisc();
  if (cue.empty()) {
    SKIP("Repository disc location is unavailable");
  }
  Content::DataStore::ScopedOverride restore;
  REQUIRE(Content::DataStore::Open(cue.parent_path().parent_path() / "Native" /
                                   "data")
              .has_value());
  const auto* store = Content::DataStore::Get();
  REQUIRE(store != nullptr);
  std::vector<std::uint16_t> kinds;
  CollectOriginalTrapKinds(*store, kinds);
  for (const auto kind : kinds) {
    INFO("trap kind " << kind);
    REQUIRE(kind < kNonTrapFloorTriggerKind);
    const auto effect = static_cast<std::uint16_t>(kind + 6);
    const auto* file = store->FindBySector(
        VS_E000_P_LBA + VS::Effects::packageFiles[effect].lba);
    REQUIRE(file != nullptr);
    CheckTrapPackageSourcePoints(*store, effect, file->path);
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
  const std::array<VS::Content::ModuleSymbol, 2> symbols{{
      {"fixture", kAddress, kExtent, nullptr, kExtent, 1, 1},
      {nullptr, kAddress, kExtent, target.data(), kExtent, 1, 1},
  }};
  CHECK(VS::Content::ResolveModuleAddress(symbols, "fixture", kAddress) ==
        nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "fixture", kAddress + 1) ==
        nullptr);
}

TEST_CASE("Generated module extents stop before the next source symbol",
          "[content][module]") {
  // #280: interior addresses resolve only inside their own source symbol.
  constexpr const char* kBattle = "BATTLE/BATTLE.PRG";
  constexpr unsigned kArmorLoss = 0x800F1A10;
  constexpr unsigned kNextAfterArmorLoss = 0x800F1A20;
  constexpr unsigned kLastLimb = 5;
  constexpr unsigned kRoomMask = 0x800E8508;
  constexpr unsigned kTriggerMarkerBytes = 0x800E8548;
  constexpr unsigned kLastRoomWord = 15;
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(Content::ResolveModuleAddress(
            symbols, kBattle,
            kArmorLoss + (kLastLimb * sizeof Battle_limbArmorDpLoss[0])) ==
        &Battle_limbArmorDpLoss[kLastLimb]);
  CHECK(Content::ResolveModuleAddress(symbols, kBattle, kNextAfterArmorLoss) ==
        nullptr);
  CHECK(Content::ResolveModuleAddress(
            symbols, kBattle,
            kRoomMask +
                (kLastRoomWord * sizeof VS::Field::countableRoomMask[0])) ==
        &VS::Field::countableRoomMask[kLastRoomWord]);
  CHECK(Content::ResolveModuleAddress(symbols, kBattle, kTriggerMarkerBytes) ==
        Content_battleTriggerMarkerBytes);
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

namespace {
struct MovieScope {
  std::vector<u16> pixels;
  std::filesystem::path temporary;
  ~MovieScope() {
    try {
      Restore();
    } catch (const std::exception& error) {
      std::fputs(error.what(), stderr);
    }
  }
  void Restore() {
    VS::Movie::PlayerClose();
    Render::GamePages().Upload(
        {0, 0, Render::PageSpace::Width, Render::PageSpace::Height},
        pixels.data());
    if (!temporary.empty()) {
      std::error_code error;
      std::filesystem::remove_all(temporary, error);
      if (error) {
        VS::Core::LogError("test", "Movie fixture cleanup: {}",
                           error.message());
      }
    }
  }
};
}  // namespace

TEST_CASE("Converted intro movie decodes without changing page ownership",
          "[content][movie][disc]") {
  const auto cue = FindRepoDisc();
  const auto root = cue.parent_path().parent_path() / "Native" / "data";
  if (cue.empty() ||
      !std::filesystem::exists(root / "disc/MOV/TITLE.STR.mkv")) {
    SKIP("Converted intro movie is unavailable");
  }
  const bool unicode_path = GENERATE(false, true);
  Content::DataStore::ScopedOverride restore;
  MovieScope scope{{Render::GamePages().Pixels().begin(),
                    Render::GamePages().Pixels().end()},
                   {}};
  auto selected_root = root;
  if (unicode_path) {
    scope.temporary =
        std::filesystem::temp_directory_path() /
        ("vs-movie-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    scope.temporary += L" spaces-\u6E2C\u8A66";
    std::filesystem::create_directories(scope.temporary / "disc" / "MOV");
    std::filesystem::copy_file(
        root / Content::DataStore::ManifestName,
        scope.temporary / Content::DataStore::ManifestName);
    std::filesystem::copy_file(
        root / "disc" / "MOV" / "TITLE.STR.mkv",
        scope.temporary / "disc" / "MOV" / "TITLE.STR.mkv");
    selected_root = scope.temporary;
  }
  REQUIRE(Content::DataStore::Open(selected_root).has_value());
  Render::GamePages().Fill(
      {0, 0, Render::PageSpace::Width, Render::PageSpace::Height}, 255, 0, 255);
  const u16 before = Render::GamePages().At(0, 0);
  REQUIRE(VS::Movie::PlayerOpen("MOV/TITLE.STR") == 1);
  REQUIRE(VS::Movie::PlayerDrawFrame(300, 0, 0) == 1);
  CHECK(std::ranges::any_of(Render::GamePages().Pixels(),
                            [before](u16 pixel) { return pixel != before; }));
  VS::Movie::PlayerClose();
  CHECK(VS::Movie::PlayerDrawFrame(301, 0, 0) == 0);
}

TEST_CASE(
    "Disc read rejects unrepresentable sizes without changing request state",
    "[content][disc]") {
  REQUIRE(TestsDiscReadSizeContract() == 1);
}

TEST_CASE("Repeated disc reads retain diagnostics without increasing Info logs",
          "[content][disc][reentry][diagnostics]") {
  DiscReadLogScope scope;
  scope.Open();
  std::vector<std::pair<VS::Core::LogLevel, std::string>> messages;
  VS::Core::SetLogObserver([&messages](const VS::Core::LogRecord& record) {
    if (record.channel == "content" &&
        record.message.starts_with("disc read: ")) {
      messages.emplace_back(record.level, record.message);
    }
  });
  for (int read = 0; read < 2; ++read) {
    CheckDiscReadPayload(scope);
  }
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].first == VS::Core::LogLevel::Info);
  CHECK(messages[1].first == VS::Core::LogLevel::Debug);
  const std::string expected_message =
      "disc read: " + scope.asset + " (sector 123, 4 bytes, raw)";
  CHECK(messages[0].second == expected_message);
  CHECK(messages[1].second == expected_message);
  VS::Core::SetLogLevel(VS::Core::LogLevel::Info);
  CheckDiscReadPayload(scope);
  CHECK(messages.size() == 2);
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
void CheckTemplateBinding(const VS::Content::ModuleDataEntry& expected) {
  INFO(expected.module << " offset " << expected.offset);
  const auto entries = VS::Content::ModuleDataEntries();
  const auto found = std::ranges::find(entries, expected.target,
                                       &VS::Content::ModuleDataEntry::target);
  REQUIRE(found != entries.end());
  CHECK(std::string_view(found->module) == expected.module);
  CHECK(found->offset == expected.offset);
  REQUIRE(found->size == expected.size);
  REQUIRE(found->capacity == expected.capacity);
}

void CheckMesh10Indices(const VS::Content::ModuleDataEntry& expected,
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
                              const VS::Content::ModuleDataEntry& expected) {
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
  const VS::Content::ModuleDataEntry initialized{
      "EFFECT/PLG044.BIN", 3832, 32, VS::Effects::Plg044::tiledRingColorFactors,
      32};
  CheckTemplateSourceBytes(store, initialized);
  const VS::Content::ModuleDataEntry shared{
      "EFFECT/PLG139.BIN", 6752, 32, VS::Effects::Plg085::tiledRingColorFactors,
      32};
  CheckTemplateSourceBytes(store, shared);
}

void CheckLoadedRingPalettes(const Content::DataStore& store) {
  for (const auto& expected : kRingPaletteData) {
    CheckTemplateBinding(expected);
    CheckTemplateSourceBytes(store, expected);
  }
}

void CheckTemplateReloads() {
  CheckLoadedMesh10Data(*Content::DataStore::Get());
  CheckLoadedTiledRingData(*Content::DataStore::Get());
  CheckLoadedRingPalettes(*Content::DataStore::Get());
  REQUIRE(VS::Content::LoadModuleData() == 0);
  CheckLoadedMesh10Data(*Content::DataStore::Get());
  CheckLoadedTiledRingData(*Content::DataStore::Get());
  CheckLoadedRingPalettes(*Content::DataStore::Get());
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

TEST_CASE("Every ring palette imports all eight transparency color modes",
          "[content][module][effects-rings]") {
  const unsigned index =
      GENERATE(Catch::Generators::range(0u, unsigned{kRingPaletteData.size()}));
  CheckTemplateBinding(kRingPaletteData[index]);
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
  CHECK(Content::CopyLiveModelGeometry(destination.model.data(), source.data(),
                                       sizeof source) ==
        destination.model.data());
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
  Content::OffsetLiveModelTextureCoordinates(bytes.data() + 4, 2, colored, 10,
                                             -7);
  CHECK(bytes == expected);
}

TEST_CASE("LFO relocation imports source data instead of host code pointers",
          "[content][module][lfo-source]") {
  std::array<std::int16_t, 5> samples{1000, -1000, 0, 0, -2};
  std::array<unsigned, 66> addresses{};
  addresses[65] = 0x1000;
  std::array<void*, 66> destination{};
  int host_code = 7;
  const VS::Content::ModuleSymbol code{"fixture", 0x1000, 0, &host_code,
                                       0,         0,      0};
  const VS::Content::ModuleSymbol source{
      "fixture", 0x1000, sizeof samples, samples.data(), sizeof samples, 2, 2};
  const std::span<const unsigned char> image{
      reinterpret_cast<const unsigned char*>(addresses.data()),
      sizeof addresses};
  VS::Content::ModulePointerTable table{"fixture",          0,  66,
                                        destination.data(), 66, &source};
  REQUIRE(
      Content::RelocateModulePointerTable(table, image, std::span(&code, 1)));
  CHECK(destination[65] == samples.data());
  CHECK(destination[0] == nullptr);
}

TEST_CASE("Native LFO catalogue covers the encoded unsigned byte domain",
          "[content][module][lfo-source]") {
  CHECK(std::size(VS::Audio::Seq::kLfoWaveforms) == 256);
}

TEST_CASE(
    "Unbounded source LFO programs remain unavailable rather than fabricated",
    "[content][module][lfo-source]") {
  std::array<std::int16_t, 8> samples{1000, -1000, 0, 0, -2, 0, 0, 32767};
  std::array<unsigned, 2> addresses{0x1000, 0x100A};
  std::array<void*, 2> destination{};
  const VS::Content::ModuleSymbol source{
      "fixture", 0x1000, sizeof samples, samples.data(), sizeof samples, 2, 2};
  const std::span<const unsigned char> image{
      reinterpret_cast<const unsigned char*>(addresses.data()),
      sizeof addresses};
  VS::Content::ModulePointerTable table{"fixture",          0, 2,
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
  VS::Content::ModuleSymbol source{
      "fixture", 0x1000, sizeof samples, samples.data(), sizeof samples, 2, 2};
  const std::span<const unsigned char> image{
      reinterpret_cast<const unsigned char*>(addresses.data()),
      sizeof addresses};
  VS::Content::ModulePointerTable table{"fixture",          0, 2,
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
  REQUIRE(VS::Content::LoadModuleData() == 0);
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

const VS::Content::ModuleSymbol& LoadedLfoOwner() {
  const auto tables = VS::Content::ModulePointerTables();
  const auto found = std::ranges::find(
      tables, reinterpret_cast<void**>(VS::Audio::Seq::kLfoWaveforms),
      &VS::Content::ModulePointerTable::target);
  REQUIRE(found != tables.end());
  REQUIRE(found->sample_source != nullptr);
  return *found->sample_source;
}

void CheckOriginalLfoOwner(const VS::Content::ModuleSymbol& owner,
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
                             const VS::Content::ModuleSymbol& owner,
                             unsigned selector) {
  const auto offset = std::size_t{0x209A0} + selector * sizeof(std::uint32_t);
  std::uint32_t address = 0;
  std::memcpy(&address, image.data() + offset, sizeof address);
  auto* cursor = VS::Audio::Seq::kLfoWaveforms[selector];
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
  REQUIRE(VS::Audio::Seq::kLfoWaveforms[65] != nullptr);
  unsigned supported = 0;
  for (unsigned selector = 0; selector < 256; ++selector) {
    if (VS::Audio::Seq::kLfoWaveforms[selector] != nullptr) {
      CheckOriginalLfoProgram(image, owner, selector);
      ++supported;
    }
  }
  CHECK(supported == 231);
}

namespace {
void CheckPlg074CursorReload(std::span<const u8> image) {
  const auto entries = VS::Content::ModuleDataEntries();
  const auto cursor = std::ranges::find(
      entries, static_cast<void*>(VS::Content::plg074TrailEmissionCounter),
      &VS::Content::ModuleDataEntry::target);
  REQUIRE(cursor != entries.end());
  CHECK(std::string_view(cursor->module) == "EFFECT/PLG074.BIN");
  CHECK(cursor->offset == 0x577C);
  CHECK(cursor->size == 4);
  CHECK(cursor->capacity == sizeof(int));
  const auto saved_first = VS::Effects::Plg073::trailEmissionCounter;
  const auto saved_second = VS::Content::plg074TrailEmissionCounter[0];
  VS::Effects::Plg073::trailEmissionCounter = 5;
  VS::Content::plg074TrailEmissionCounter[0] = 7;
  CHECK(Content::CopyModuleDataEntry(*cursor, image));
  CHECK(VS::Content::plg074TrailEmissionCounter[0] == 0);
  CHECK(VS::Effects::Plg073::trailEmissionCounter == 5);
  VS::Effects::Plg073::trailEmissionCounter = saved_first;
  VS::Content::plg074TrailEmissionCounter[0] = saved_second;
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
  const VS::Content::ModuleDataEntry expected{
      "EFFECT/PLG073.BIN", 0x575C, 32, VS::Effects::Plg073::ringColorFactors,
      32};
  CheckTemplateBinding(expected);
  LoadOriginalLfoModules();
  CheckTemplateSourceBytes(*Content::DataStore::Get(), expected);
  REQUIRE(VS::Content::LoadModuleData() == 0);
  CheckTemplateSourceBytes(*Content::DataStore::Get(), expected);
}

namespace {
struct ContentWorkspaceState {
  char saved = VS::Content::overlayWorkspaceOwner;
  ContentWorkspaceState() { VS::Content::overlayWorkspaceOwner = 0; }
  ~ContentWorkspaceState() { VS::Content::overlayWorkspaceOwner = saved; }
};

struct ActorAssetState {
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Content_legacyActorAssetRequests)) saved_requests =
      std::to_array(Content_legacyActorAssetRequests);
  decltype(std::to_array(Actors_sharedSequenceData)) saved_banks =
      std::to_array(Actors_sharedSequenceData);
  std::array<unsigned char, 3> saved_pages{};
  std::array<Actors_ActorRecord, 4> records{};
  Content_SequenceBuffer sequence{};

  ActorAssetState() {
    std::copy_n(Actors_sharedSequenceBankTexturePages, saved_pages.size(),
                saved_pages.begin());
    std::fill_n(Actors_sharedSequenceBankTexturePages, saved_pages.size(), 0);
    std::fill_n(Actors_sharedSequenceData, saved_banks.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_records.size(), nullptr);
    std::fill_n(Content_legacyActorAssetRequests, saved_requests.size(),
                Content_ActorAssetRequest{});
    for (std::size_t index = 0; index < records.size(); ++index) {
      records[index].modelId = 7;
      Actors_actorRecords[index] = &records[index];
    }
  }

  ~ActorAssetState() {
    std::copy(saved_records.begin(), saved_records.end(), Actors_actorRecords);
    std::copy(saved_requests.begin(), saved_requests.end(),
              Content_legacyActorAssetRequests);
    std::copy(saved_banks.begin(), saved_banks.end(),
              Actors_sharedSequenceData);
    std::copy(saved_pages.begin(), saved_pages.end(),
              Actors_sharedSequenceBankTexturePages);
  }
};

// A timeline section as the block stores it: its track count word, then each
// track's header halfword and block-relative data offset.
struct TimelineTrackEntryFixture {
  std::uint16_t header{};
  std::uint16_t data_offset{};
};
struct TimelineSectionFixture {
  std::int32_t track_count{};
  std::array<TimelineTrackEntryFixture, 2> tracks{};
};
static_assert(sizeof(TimelineTrackEntryFixture) ==
              Content::kPFileTimelineTrackEntryBytes);
static_assert(offsetof(TimelineSectionFixture, tracks) ==
              Content::kPFileTimelineHeaderBytes);

struct TimelineBlockFixture {
  VS::Effects::LiveTimelineTable block{};
  std::array<std::uint16_t, 2> offsets{};
  std::array<TimelineSectionFixture, 2> sections{};
  std::array<std::byte, 16> keys{};
};
}  // namespace

TEST_CASE("Weapon sequence copies retain relative offsets and exact extent",
          "[content][semantic-names][sequence]") {
  auto source = std::make_unique<Content_SequenceBuffer>();
  auto destination = std::make_unique<Content_SequenceBuffer>();
  std::memset(source.get(), 0xA5, sizeof(*source));
  std::memset(destination.get(), 0x6B, sizeof(*destination));
  source->firstSectionPointer = 0x40;
  source->secondSectionPointer = 0x200;
  VS::Actors::RegisterSequenceBank(source.get(), Content::kWeaponSequenceBytes);
  Content::CopyLiveWeaponSequence(destination.get(), source.get());
  const auto original = std::as_bytes(std::span(source.get(), 1));
  const auto copied = std::as_bytes(std::span(destination.get(), 1));
  CHECK(std::ranges::equal(copied.first(0x7800), original.first(0x7800)));
  CHECK(std::ranges::all_of(copied.subspan(0x7800), [](std::byte value) {
    return value == std::byte{0x6B};
  }));
  CHECK(destination->firstSectionPointer == 0x40);
  CHECK(destination->secondSectionPointer == 0x200);
  VS::Actors::ForgetSequenceBanks(source.get(), Content::kWeaponSequenceBytes);
}

TEST_CASE("Overlay workspace claims preserve the owning tag",
          "[content][semantic-names][workspace]") {
  ContentWorkspaceState state;
  CHECK(Content::AcquireLiveOverlayWorkspace(6) == 1);
  CHECK(VS::Content::overlayWorkspaceOwner == 6);
  CHECK(Content::AcquireLiveOverlayWorkspace(2) == 0);
  CHECK(VS::Content::overlayWorkspaceOwner == 6);
  CHECK(Content::ReleaseLiveOverlayWorkspace(2) == 0);
  CHECK(VS::Content::overlayWorkspaceOwner == 6);
}

TEST_CASE("Overlay workspace release permits a subsequent different owner",
          "[content][semantic-names][workspace]") {
  ContentWorkspaceState state;
  VS::Content::overlayWorkspaceOwner = 6;
  CHECK(Content::ReleaseLiveOverlayWorkspace(6) == 1);
  CHECK(VS::Content::overlayWorkspaceOwner == 0);
  CHECK(Content::ReleaseLiveOverlayWorkspace(6) == 0);
  CHECK(Content::AcquireLiveOverlayWorkspace(2) == 1);
  CHECK(Content::ReleaseLiveOverlayWorkspace(2) == 1);
  CHECK(VS::Content::overlayWorkspaceOwner == 0);
}

TEST_CASE("Timeline section and track indices retain block-relative offsets",
          "[content][semantic-names][timeline]") {
  TimelineBlockFixture fixture;
  fixture.block.count = 2;
  for (std::size_t section = 0; section < fixture.sections.size(); ++section) {
    const std::size_t table_at = offsetof(TimelineBlockFixture, sections) +
                                 section * sizeof(TimelineSectionFixture);
    fixture.offsets[section] = static_cast<std::uint16_t>(table_at);
    auto& table = fixture.sections[section];
    table.track_count = 2;
    for (std::size_t track = 0; track < table.tracks.size(); ++track) {
      table.tracks[track].data_offset = static_cast<std::uint16_t>(
          offsetof(TimelineBlockFixture, keys) + section * 8 + track * 4);
    }
  }
  const auto block = std::as_bytes(std::span(&fixture, 1));
  for (std::size_t section = 0; section < fixture.sections.size(); ++section) {
    CHECK(Content::PFileTimelineSection(block, section) ==
          offsetof(TimelineBlockFixture, sections) +
              section * sizeof(TimelineSectionFixture));
    for (std::size_t track = 0; track < 2; ++track) {
      CHECK(Content::PFileTimelineTrackData(block, section, track) ==
            offsetof(TimelineBlockFixture, keys) + section * 8 + track * 4);
    }
  }
}

TEST_CASE("Sequence release distinguishes actor slots from one-based banks",
          "[content][semantic-names][sequence]") {
  ActorAssetState state;
  state.records[0].battleSequence = &state.sequence;
  CHECK(Content::ReleaseLiveActorSequence(0, 0) == 0);
  CHECK(state.records[0].battleSequence == nullptr);
  state.records[2].sharedSequences[0] = &state.sequence;
  CHECK(Content::ReleaseLiveActorSequence(1, 1) == 0);
  CHECK(state.records[2].sharedSequences[0] == nullptr);
  CHECK(Content::ReleaseLiveActorSequence(1, 1) == 0);
}

TEST_CASE("Special actor model release clears only its record slot",
          "[content][semantic-names][sequence]") {
  ActorAssetState state;
  state.records[2].modelId = 0x7F;
  CHECK(Content::ReleaseLiveActorModel(2) == 0);
  CHECK(Actors_actorRecords[2] == nullptr);
  CHECK(Actors_actorRecords[3] == &state.records[3]);
  CHECK(Content::ReleaseLiveActorModel(2) == -1);
  CHECK(Content::ReleaseLiveActorModel(17) == -1);
}
