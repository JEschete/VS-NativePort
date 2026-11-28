// Hand-maintained tests for the typed collision height sampler (#247).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "Content/DataStore.hpp"
#include "Content/ModuleData.hpp"
#include "Field/CollisionMap.hpp"
#include "Field/CollisionSampling.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Field/RoomLoading.hpp"
#include "Legacy/Psx.hpp"

namespace {
// A 1x1 map: header, floor and ceiling descriptors, then two 4x4 patterns.
struct OneTileMap {
  static constexpr std::size_t kPatterns = 8 + 2 * 2;
  std::array<std::byte, kPatterns + std::size_t{2} * 16> bytes{};

  OneTileMap() {
    const std::array<std::int16_t, 4> header{1, 1, 0, 1};
    std::memcpy(bytes.data(), header.data(), sizeof header);
    const std::uint16_t ceiling = 0x1001;
    std::memcpy(bytes.data() + 10, &ceiling, sizeof ceiling);
    bytes[kPatterns + 5] = std::byte{0x83};
    bytes[kPatterns + 16] = std::byte{0x05};
  }
};

struct PaddedTileMap {
  static constexpr std::size_t kPatterns = 8 + std::size_t{3} * 2 * 2;
  std::array<std::byte, kPatterns + std::size_t{2} * 16> bytes{};

  PaddedTileMap() {
    const std::array<std::int16_t, 4> header{2, 1, -20, 3};
    const std::array<std::uint16_t, 6> descriptors{0xF800, 1,      0x7FF,
                                                   0x7801, 0x0800, 0x7FF};
    std::memcpy(bytes.data(), header.data(), sizeof(header));
    std::memcpy(bytes.data() + 8, descriptors.data(), sizeof(descriptors));
    for (std::size_t index = 0; index < 32; ++index) {
      bytes[kPatterns + index] =
          static_cast<std::byte>(static_cast<std::uint8_t>(index * 17));
    }
  }
};

VS::Field::CollisionHeightMap Import(std::span<const std::byte> source) {
  auto map = VS::Field::CollisionHeightMap::ImportLegacy(source);
  if (!map) FAIL(map.error());
  return std::move(*map);
}

void CheckSamples(const VS::Field::CollisionHeightMap& decoded,
                  std::span<const std::byte> source, int width, int height) {
  for (int z = -1; z <= height * 4; ++z) {
    for (std::uint32_t x = 0; x <= static_cast<std::uint32_t>(width * 4); ++x) {
      CHECK(decoded.Sample(x, z, VS::Field::kCollisionFloorLayer) ==
            VS::Field::SampleCollisionHeightCode(
                source, x, z, VS::Field::kCollisionFloorLayer));
      CHECK(decoded.Sample(x, z, VS::Field::kCollisionCeilingLayer) ==
            VS::Field::SampleCollisionHeightCode(
                source, x, z, VS::Field::kCollisionCeilingLayer));
    }
  }
}

std::span<std::byte> CollisionSection(std::span<std::byte> file) {
  const auto layout = VS::Field::DecodeRoomFile(file);
  if (!layout) FAIL("Room file must have bounded section ranges");
  std::uint32_t declared_bytes = 0;
  std::memcpy(&declared_bytes, file.data() + sizeof(std::uint32_t),
              sizeof(declared_bytes));
  if (declared_bytes > layout->room_sections.bytes) {
    FAIL("Declared room section must fit its file range");
  }
  const auto room = file.subspan(layout->room_sections.offset, declared_bytes);
  if (room.size() < VS::Field::kRoomSectionHeaderBytes) {
    CHECK(std::ranges::all_of(
        room, [](std::byte value) { return value == std::byte{}; }));
    return {};
  }
  const auto sections = VS::Field::DecodeRoomSectionLayout(room);
  if (!sections) FAIL("Room sections must be bounded");
  constexpr auto kCollision =
      static_cast<std::size_t>(VS::Field::RoomSectionId::kCollisionMap);
  return room.subspan(
      VS::Field::kRoomSectionHeaderBytes + sections->offsets[kCollision],
      sections->lengths[kCollision]);
}

bool CheckRoom(const VS::Content::DataStore& store,
               const VS::Content::DataStore::Entry& entry) {
  INFO(entry.path);
  auto bytes = store.ReadAll(entry);
  if (!bytes) FAIL(bytes.error());
  const auto section =
      CollisionSection(std::as_writable_bytes(std::span(*bytes)));
  if (section.empty()) return false;
  REQUIRE(VS::Field::ResolveCollisionCellCount(section));
  const auto decoded = Import(section);
  const auto& header = decoded.Header();
  if (!header) FAIL("Nonempty collision section must have a header");
  CheckSamples(decoded, section, header->width, header->height);
  return true;
}
}  // namespace

TEST_CASE("Collision samples decode floor and ceiling patterns from bytes",
          "[collision][room][collision-sampling]") {
  const OneTileMap map;
  using VS::Field::SampleCollisionHeightCode;
  CHECK(SampleCollisionHeightCode(map.bytes, 1, 1,
                                  VS::Field::kCollisionFloorLayer) == 0x83);
  CHECK(SampleCollisionHeightCode(map.bytes, 0, 0,
                                  VS::Field::kCollisionCeilingLayer) == 13);
  CHECK(SampleCollisionHeightCode(map.bytes, 4, 0, 0) ==
        VS::Field::kCollisionHeightInvalid);
  CHECK(SampleCollisionHeightCode(map.bytes, 0, 0, -1) ==
        VS::Field::kCollisionHeightInvalid);
  CHECK(SampleCollisionHeightCode(std::span(map.bytes).first(20), 0, 0, 1) ==
        VS::Field::kCollisionHeightInvalid);
}

TEST_CASE("The live room binding exposes the room's collision bytes",
          "[collision][room][collision-sampling]") {
  OneTileMap map;
  auto* saved_map = VS::Field::roomData.collision_map;
  const auto saved_length = VS::Field::roomData.header.collision_map_length;
  VS::Field::roomData.collision_map = nullptr;
  CHECK(VS::Field::LiveRoomCollisionMap().empty());
  VS::Field::roomData.collision_map =
      reinterpret_cast<VS::Field::MpdCollisionHeader*>(map.bytes.data());
  VS::Field::roomData.header.collision_map_length = sizeof map.bytes;
  const auto live = VS::Field::LiveRoomCollisionMap();
  CHECK(live.data() == map.bytes.data());
  CHECK(live.size() == map.bytes.size());
  VS::Field::roomData.collision_map = saved_map;
  VS::Field::roomData.header.collision_map_length = saved_length;
}

TEST_CASE("The promoted collision sampler has no original module address",
          "[collision][room][collision-sampling]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80099514u) == nullptr);
}

TEST_CASE("Owned collision heights keep independent decoded inputs",
          "[collision][collision-sampling][query-context][issue-57]") {
  OneTileMap source;
  const auto first = Import(source.bytes);
  source.bytes[OneTileMap::kPatterns + 5] = std::byte{7};
  const auto second = Import(source.bytes);
  source.bytes.fill(std::byte{});
  CHECK(first.Sample(1, 1, 0) == 0x83);
  CHECK(second.Sample(1, 1, 0) == 7);
  CHECK(first.Sample(0, 0, 1) == 13);
  CHECK(VS::Field::FloorHeightAt(first, 32, 32) == -48);
  CHECK(VS::Field::FloorHeightAt(second, 32, 32) == 32656);
  CHECK(first.Sample(1, 1, 0) == 0x83);
}

TEST_CASE(
    "Owned collision import preserves descriptor strides and signed samples",
    "[collision][collision-sampling][query-context][issue-57]") {
  const PaddedTileMap source;
  const auto decoded = Import(source.bytes);
  CheckSamples(decoded, source.bytes, 2, 1);
  CHECK(VS::Field::FloorHeightAt(decoded, 176, 48) ==
        VS::Field::FloorHeightAt(source.bytes, 176, 48));
  CHECK(decoded.Sample(0, 0, 2) == VS::Field::kCollisionHeightInvalid);
}

TEST_CASE("Collision height import distinguishes absent and malformed rooms",
          "[collision][collision-sampling][query-context][issue-57]") {
  using VS::Field::CollisionHeightMap;
  const auto absent =
      CollisionHeightMap::ImportLegacy(std::span<const std::byte>{});
  if (!absent) FAIL(absent.error());
  CHECK(VS::Field::FloorHeightAt(*absent, 0, 0) ==
        VS::Field::kMissingFloorHeight);
  OneTileMap source;
  CHECK_FALSE(
      CollisionHeightMap::ImportLegacy(std::span(source.bytes).first(7)));
  CHECK_FALSE(
      CollisionHeightMap::ImportLegacy(std::span(source.bytes).first(10)));
  CHECK_FALSE(
      CollisionHeightMap::ImportLegacy(std::span(source.bytes).first(20)));
  const std::int16_t negative_width = -1;
  std::memcpy(source.bytes.data(), &negative_width, sizeof(negative_width));
  CHECK_FALSE(CollisionHeightMap::ImportLegacy(source.bytes));
}

TEST_CASE("Original room collision samples survive native ownership import",
          "[collision-height-corpus][disc][issue-57]") {
  namespace Content = VS::Content;
  const std::filesystem::path root = R"(Native\data)";
  if (!std::filesystem::exists(root / Content::DataStore::ManifestName)) {
    SKIP("Extracted Native data is unavailable");
  }
  Content::DataStore::ScopedOverride restore;
  const auto opened = Content::DataStore::OpenShared(root);
  if (!opened) FAIL(opened.error());
  const auto* store = Content::DataStore::Get();
  if (store == nullptr) FAIL("Opened data store must be available");
  std::size_t rooms = 0;
  for (const auto& entry : store->Entries()) {
    if (entry.path.starts_with("MAP/") && entry.path.ends_with(".MPD")) {
      if (CheckRoom(*store, entry)) ++rooms;
    }
  }
  CHECK(rooms != 0);
}
