#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Field/ChestInteractionsLive.h"
#include "Field/MinimapScene.hpp"
#include "Field/MinimapSceneLive.h"
#include "Field/MinimapSceneLive.hpp"
#include "Field/Transitions.hpp"
#include "Render/RoomDrawLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Room.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/StateFlags.h"
}

namespace {
std::span<const std::byte> SceneBytes(const void* data, size_t size) {
  return {static_cast<const std::byte*>(data), size};
}

std::vector<u_long> Scene() {
  return {2,
          1,
          76,
          9 | (1 << 16),
          1,
          60,
          9 | (2 << 16),
          4,
          0xFFF0FFF0,
          0,
          0xFFF00010,
          0,
          0x00100010,
          0,
          0x0010FFF0,
          0,
          1,
          0x00020100,
          1,
          0x03020100,
          1,
          0x00000100,
          1,
          0x00000302,
          1,
          0,
          4,
          0xFFF0FFF0,
          0,
          0xFFF00010,
          0,
          0x00100010,
          0,
          0x0010FFF0,
          0,
          0,
          0,
          1,
          0x00000100,
          0,
          0};
}

u_long FileWord(const std::vector<unsigned char>& data, size_t offset) {
  REQUIRE(offset <= data.size());
  REQUIRE(data.size() - offset >= sizeof(u_long));
  u_long value;
  std::memcpy(&value, data.data() + offset, sizeof value);
  return value;
}

std::vector<unsigned char> FileBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.is_open());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

std::filesystem::path ExtractedRoot() {
  auto root = std::filesystem::current_path();
  while (!std::filesystem::exists(root / "Native" / "data" / "disc") &&
         root != root.parent_path()) {
    root = root.parent_path();
  }
  root /= std::filesystem::path("Native") / "data" / "disc";
  if (!std::filesystem::exists(root)) {
    SKIP("Extracted game data is not configured");
  }
  return root;
}

struct SceneDoorFixture {
  decltype(Field_roomData) saved_room = Field_roomData;
  decltype(Field_enteredDoorId) saved_id = Field_enteredDoorId;
  decltype(Field_entranceDoorRecord) saved_entrance = Field_entranceDoorRecord;
  std::vector<u_long> words = Scene();

  SceneDoorFixture() {
    Field_roomData = {};
    Field_roomData.minimapScene =
        reinterpret_cast<Field_MapScene*>(words.data());
    Field_roomData.header.minimapSceneLength =
        static_cast<std::uint32_t>(words.size() * sizeof(u_long));
  }

  ~SceneDoorFixture() {
    Field_roomData = saved_room;
    Field_enteredDoorId = saved_id;
    Field_entranceDoorRecord = saved_entrance;
  }
};

struct RecordingEntranceDoorHost final : VS::Field::EntranceDoorHost {
  std::optional<std::uint8_t> scene_id;
  std::optional<std::uint8_t> requested_id;

  std::optional<std::uint8_t> SceneDoorId(std::uint8_t door_id) override {
    requested_id = door_id;
    return scene_id;
  }
};

struct RenderFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  std::array<void*, 2> saved_buffers{Render_legacyPrimitiveBuffers[0],
                                     Render_legacyPrimitiveBuffers[1]};
  int saved_framebuffer = Render_frameBuf;
  u_short saved_ticks = Field_roomAnimationTicks;
  State_StateFlags saved_flags = Audio_Seq_unusedStateFlags;
  std::vector<u_long> packets =
      std::vector<u_long>(0x20000 / sizeof(u_long) + 1);
  std::array<u_long, 16> ordering{};

  explicit RenderFixture(int framebuffer = 0) {
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
    Core_Gte_SetGeomOffset(264, 196);
    Render_frameBuf = framebuffer;
    Render_legacyPrimitiveBuffers[framebuffer] = packets.data();
    Render_frameScratch.primitiveCursor = packets.data();
    Render_frameScratch.orderingTable = ordering.data();
    Field_roomAnimationTicks = 0;
    std::memset(&Audio_Seq_unusedStateFlags, 0,
                sizeof Audio_Seq_unusedStateFlags);
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    packets.back() = 0xDEADBEEF;
  }

  ~RenderFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffers[0];
    Render_legacyPrimitiveBuffers[1] = saved_buffers[1];
    Render_frameBuf = saved_framebuffer;
    Field_roomAnimationTicks = saved_ticks;
    Audio_Seq_unusedStateFlags = saved_flags;
    Core_Gte_Reset();
  }

  std::vector<const P_TAG*> Commands() {
    std::vector<const P_TAG*> result;
    const void* packet = &ordering.back();
    for (size_t count = 0; packet != nullptr && count < 64; ++count) {
      const auto* tag = static_cast<const P_TAG*>(packet);
      if (tag->len != 0) result.push_back(tag);
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    REQUIRE(packet == nullptr);
    return result;
  }
};
}  // namespace

TEST_CASE(
    "Map scenes retain packed records and resolve bounded relative offsets",
    "[content][minimap][layout]") {
  auto bytes = Scene();
  REQUIRE(VS::Field::IsValidMapScene(
      SceneBytes(bytes.data(), bytes.size() * sizeof(u_long))));
  auto* end = VS::Field::ResolveLiveSceneRoomData(
      bytes.data(), bytes.size() * sizeof(u_long));
  CHECK(reinterpret_cast<void*>(end) ==
        reinterpret_cast<unsigned char*>(bytes.data()) +
            bytes.size() * sizeof(u_long));
  CHECK(bytes[2] == 24);
  CHECK(bytes[5] == 88);
  auto* scene = reinterpret_cast<Field_MapScene*>(bytes.data());
  CHECK(scene->rooms[0].zoneId == 9);
  CHECK(scene->rooms[1].mapId == 2);
  CHECK(VS_ONDISC(&scene->rooms[0], dataAddress, void*) == bytes.data() + 7);
  CHECK(VS_ONDISC(&scene->rooms[1], dataAddress, void*) == bytes.data() + 26);
}

TEST_CASE("Malformed map counts, payloads and indices fail without mutation",
          "[content][minimap][bounds]") {
  const auto valid = Scene();
  for (size_t length = 0; length < valid.size() * sizeof(u_long); ++length) {
    CHECK_FALSE(VS::Field::IsValidMapScene(SceneBytes(valid.data(), length)));
  }
  for (const size_t index : {0, 2, 7, 16, 17}) {
    auto bad = valid;
    bad[index] = UINT32_MAX;
    const auto saved = bad;
    CHECK_FALSE(VS::Field::IsValidMapScene(
        SceneBytes(bad.data(), bad.size() * sizeof(u_long))));
    CHECK(bad == saved);
    CHECK_THROWS_AS(VS::Field::ResolveLiveSceneRoomData(
                        bad.data(), bad.size() * sizeof(u_long)),
                    std::runtime_error);
    CHECK(bad == saved);
  }
  CHECK_FALSE(VS::Field::IsValidMapScene({}));
}

TEST_CASE("Current scene room address preserves every signed halfword",
          "[scene-room-address][minimap]") {
  std::array<std::uint32_t, 4> scene{1, 0, 0, 0};
  for (std::uint32_t value = 0; value <= UINT16_MAX; ++value) {
    const auto other = UINT16_MAX - value;
    scene[3] = value | (other << 16);
    const auto room =
        VS::Field::CurrentSceneRoomAddress(std::as_bytes(std::span(scene)));
    REQUIRE(room);
    if (!room) throw std::bad_optional_access();
    CHECK(room->zone_id ==
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value)));
    CHECK(room->map_id ==
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(other)));
  }
}

TEST_CASE(
    "Current scene room address rejects missing and malformed record extents",
    "[scene-room-address][minimap][bounds]") {
  std::array<std::uint32_t, 4> scene{1, 0, 0, 0x8000FFFF};
  const auto bytes = std::as_bytes(std::span(scene));
  for (std::size_t length = 0; length < bytes.size(); ++length) {
    CHECK_FALSE(VS::Field::CurrentSceneRoomAddress(bytes.first(length)));
  }
  CHECK(VS::Field::CurrentSceneRoomAddress(bytes) ==
        VS::Field::SceneRoomAddress{-1, INT16_MIN});
  scene[0] = 0;
  CHECK_FALSE(VS::Field::CurrentSceneRoomAddress(bytes));
  scene[0] = 2;
  CHECK_FALSE(VS::Field::CurrentSceneRoomAddress(bytes));
  scene[0] = UINT32_MAX;
  CHECK_FALSE(VS::Field::CurrentSceneRoomAddress(bytes));
}

TEST_CASE("Scene door identifiers preserve every unsigned byte",
          "[scene-door-core][minimap]") {
  auto words = Scene();
  auto bytes = std::as_writable_bytes(std::span(words));
  VS::Field::ResolveSceneRoomData(bytes);
  for (std::uint32_t id = 0; id <= UINT8_MAX; ++id) {
    words[25] = id << 8;
    CHECK(VS::Field::SceneDoorId(bytes, 0) == id);
  }
  CHECK_FALSE(VS::Field::SceneDoorId(bytes, -1));
  CHECK_FALSE(VS::Field::SceneDoorId(bytes, 1));
  CHECK_FALSE(VS::Field::SceneDoorId(bytes, INT32_MAX));
}

TEST_CASE("Scene door bounds reject truncated and overflowing tables",
          "[scene-door-core][minimap][bounds]") {
  const auto words = Scene();
  const auto bytes = std::as_bytes(std::span(words));
  for (std::size_t length = 0; length < 26 * sizeof(u_long); ++length) {
    CHECK_FALSE(VS::Field::SceneDoorId(bytes.first(length), 0));
  }
  for (const std::size_t count : {0, 7, 16, 18, 20, 22, 24}) {
    auto bad = words;
    bad[count] = UINT32_MAX;
    auto mutable_bytes = std::as_writable_bytes(std::span(bad));
    const auto saved = bad;
    CHECK_FALSE(VS::Field::SceneDoorId(mutable_bytes, 0));
    CHECK_FALSE(VS::Field::MarkEntranceDoorOnSceneMap(mutable_bytes, 0));
    CHECK(bad == saved);
  }
}

TEST_CASE("Scene entrance marking changes only the selected flag byte",
          "[scene-door-core][minimap]") {
  auto words = Scene();
  auto bytes = std::as_writable_bytes(std::span(words));
  VS::Field::ResolveSceneRoomData(bytes);
  words[25] = 0xAB04FE00;
  auto expected = words;
  expected[25] |= 0x00100000;
  CHECK(VS::Field::MarkEntranceDoorOnSceneMap(bytes, 0));
  CHECK(words == expected);
  CHECK(VS::Field::SceneDoorId(bytes, 0) == 254);
  CHECK(VS::Field::MarkEntranceDoorOnSceneMap(bytes, 0));
  CHECK(words == expected);
  CHECK_FALSE(VS::Field::MarkEntranceDoorOnSceneMap(bytes, -1));
  CHECK_FALSE(VS::Field::MarkEntranceDoorOnSceneMap(bytes, 1));
  CHECK(words == expected);
}

TEST_CASE("Scene room address reads bounded signed original halfwords",
          "[sigil-door-live][minimap]") {
  const std::array<std::uint32_t, 4> words{1, 1, 0, 0x8000FFFF};
  const auto bytes = std::as_bytes(std::span(words));
  const auto room = VS::Field::CurrentSceneRoomAddress(bytes);
  REQUIRE(room);
  if (!room) return;
  CHECK(room->zone_id == -1);
  CHECK(room->map_id == -32768);
  for (std::size_t length = 0; length < bytes.size(); ++length) {
    CHECK_FALSE(VS::Field::CurrentSceneRoomAddress(bytes.first(length)));
  }
  auto bad = words;
  bad[0] = 0;
  CHECK_FALSE(
      VS::Field::CurrentSceneRoomAddress(std::as_bytes(std::span(bad))));
  bad[0] = UINT32_MAX;
  CHECK_FALSE(
      VS::Field::CurrentSceneRoomAddress(std::as_bytes(std::span(bad))));
}

TEST_CASE("Entrance state copies complete door and unsigned scene identifier",
          "[entrance-door-core][field]") {
  RecordingEntranceDoorHost host;
  host.scene_id = 254;
  VS::Field::EntranceDoorState state;
  const VS::Field::DoorRecord door{.destination_zone_id = 9,
                                   .destination_room_id = 3,
                                   .required_flag_value = 7,
                                   .required_flag_id = 321,
                                   .door_id = 128,
                                   .reserved = {1, 2, 3}};
  VS::Field::SetEntranceDoorRecord(state, door, host);
  CHECK(state.door == door);
  CHECK(state.entered_door_id == 254);
  CHECK(host.requested_id == 128);
}

TEST_CASE("Entrance state clears absent records without scene lookup",
          "[entrance-door-core][field]") {
  RecordingEntranceDoorHost host;
  VS::Field::EntranceDoorState state;
  state.door.door_id = 128;
  state.entered_door_id = 254;
  VS::Field::SetEntranceDoorRecord(state, std::nullopt, host);
  CHECK(state.door == VS::Field::DoorRecord{});
  CHECK(state.entered_door_id == -1);
  CHECK_FALSE(host.requested_id);
}

TEST_CASE("Entrance state preserves record when scene mapping is absent",
          "[entrance-door-core][field]") {
  RecordingEntranceDoorHost host;
  VS::Field::EntranceDoorState state;
  const VS::Field::DoorRecord door{.door_id = 128};
  VS::Field::SetEntranceDoorRecord(state, door, host);
  CHECK(state.door == door);
  CHECK(state.entered_door_id == -1);
  CHECK(host.requested_id == 128);
}

TEST_CASE("Entrance lookup preserves file order and one-tile boundaries",
          "[entrance-door-core][field]") {
  const std::array<VS::Field::DoorRecord, 3> doors{
      VS::Field::DoorRecord{.tile_x = 3, .tile_z = 3, .floor_layer = 1},
      VS::Field::DoorRecord{.tile_x = 2, .tile_z = 3},
      VS::Field::DoorRecord{.tile_x = 3, .tile_z = 3}};
  CHECK(VS::Field::FindEntranceDoorNearTile(doors, 3, 3, 0) == doors[1]);
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile(doors, 5, 3, 0));
  CHECK(VS::Field::FindEntranceDoorNearTile(doors, 4, 4, 0) == doors[2]);
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile({}, 3, 3, 0));
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile(doors, 3, 3, -1));
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile(
      doors, std::numeric_limits<std::int32_t>::min(), 3, 0));
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile(
      doors, std::numeric_limits<std::int32_t>::max(), 3, 0));
}

TEST_CASE("Entrance lookup upper layers ignore coordinates but keep layer",
          "[entrance-door-core][field]") {
  const std::array<VS::Field::DoorRecord, 2> doors{
      VS::Field::DoorRecord{.tile_x = 3, .tile_z = 3, .floor_layer = 3},
      VS::Field::DoorRecord{.tile_x = 7, .tile_z = 7, .floor_layer = 2}};
  CHECK(VS::Field::FindEntranceDoorNearTile(doors, 100, 100, 2) == doors[1]);
  CHECK(VS::Field::FindEntranceDoorNearTile(doors, -100, -100, 3) == doors[0]);
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile(doors, 3, 3, 4));
}

TEST_CASE("Native entrance lookup accepts bounded scene index 128",
          "[entrance-door-live][scene-door-live][field]") {
  SceneDoorFixture fixture;
  fixture.words.assign(141, 0);
  fixture.words[0] = 1;
  fixture.words[1] = 1;
  fixture.words[2] = (141 - 4) * sizeof(u_long);
  fixture.words[4] = 1;
  fixture.words[11] = 129;
  fixture.words[140] = 0xFE00;
  Field_roomData.minimapScene =
      reinterpret_cast<Field_MapScene*>(fixture.words.data());
  Field_roomData.header.minimapSceneLength =
      static_cast<std::uint32_t>(fixture.words.size() * sizeof(u_long));
  CHECK(VS::Field::IsValidMapScene(std::as_bytes(std::span(fixture.words))));
  CHECK(Field_SceneDoorIdEntry(128) == 254);
  Field_MpdDoorRecord door{};
  door.doorId = 128;
  Field_SetEntranceDoorRecordEntry(reinterpret_cast<char (*)[12]>(&door));
  CHECK(Field_enteredDoorId == 254);
  CHECK(Field_entranceDoorRecord.doorId == 128);
  CHECK(Field_SceneDoorIdEntry(129) == -1);
}

TEST_CASE("Typed entrance live lookup keeps order upper layers and byte bounds",
          "[entrance-door-live][field]") {
  SceneDoorFixture fixture;
  fixture.words[25] = 0xFE00;
  std::array<Field_MpdDoorRecord, 2> doors{};
  doors[0].tileLocation.tileX = 2;
  doors[0].tileLocation.tileZ = 3;
  doors[1].tileLocation.tileX = 7;
  doors[1].tileLocation.tileZ = 7;
  doors[1].tileLocation.floorLayer = 2;
  Field_roomData.doors = doors.data();
  Field_roomData.header.doorsLength = sizeof doors;
  Field_FindEntranceDoorNearTileEntry(3, 3, 0);
  CHECK(Field_enteredDoorId == 254);
  CHECK(Field_entranceDoorRecord.tileLocation.tileX == 2);
  Field_FindEntranceDoorNearTileEntry(50, 50, 2);
  CHECK(Field_enteredDoorId == 254);
  CHECK(Field_entranceDoorRecord.tileLocation.tileX == 7);
  Field_roomData.header.doorsLength = sizeof doors - 1;
  Field_FindEntranceDoorNearTileEntry(3, 3, 0);
  CHECK(Field_enteredDoorId == -1);
  CHECK(Field_entranceDoorRecord.doorId == 0);
}

TEST_CASE("Reload door native store writes only its packed state byte",
          "[entrance-reload-live][field]") {
  SceneDoorFixture fixture;
  const auto saved = Audio_Seq_unusedStateFlags;
  struct RestoreFlags {
    decltype(Audio_Seq_unusedStateFlags) value;
    ~RestoreFlags() { Audio_Seq_unusedStateFlags = value; }
  } restore{saved};
  auto expected = saved;
  Field_enteredDoorId = 254;
  expected.doorEntered = std::bit_cast<char>(std::uint8_t{255});
  Field_StoreEntranceDoorForReloadEntry();
  using FlagBytes = std::array<std::byte, sizeof(expected)>;
  CHECK(std::bit_cast<FlagBytes>(Audio_Seq_unusedStateFlags) ==
        std::bit_cast<FlagBytes>(expected));
  CHECK(Field_enteredDoorId == 254);
  Field_enteredDoorId = -1;
  Field_StoreEntranceDoorForReloadEntry();
  CHECK(std::bit_cast<std::uint8_t>(Audio_Seq_unusedStateFlags.doorEntered) ==
        0);
}

TEST_CASE("Entrance native entries retain original module addresses",
          "[entrance-door-live][entrance-reload-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008B8F8) ==
        reinterpret_cast<const void*>(&Field_SetEntranceDoorRecordEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008B960) ==
        reinterpret_cast<const void*>(&Field_FindEntranceDoorNearTileEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008AB68) ==
        reinterpret_cast<const void*>(&Field_StoreEntranceDoorForReloadEntry));
}

TEST_CASE("Scene door native entries retain original module addresses",
          "[scene-door-live][minimap]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D880) ==
        reinterpret_cast<const void*>(&Field_SceneDoorIdEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D948) ==
        reinterpret_cast<const void*>(&Field_MarkEntranceDoorOnSceneMapEntry));
}

TEST_CASE("Native entrance lookup uses bounded unsigned scene bytes",
          "[scene-door-live][minimap]") {
  SceneDoorFixture fixture;
  Field_MpdDoorRecord door{};
  door.requiredFlagId = 321;
  for (std::uint32_t id = 0; id <= UINT8_MAX; ++id) {
    fixture.words[25] = id << 8;
    CHECK(Field_SceneDoorIdEntry(0) == id);
    Field_SetEntranceDoorRecordEntry(reinterpret_cast<char (*)[12]>(&door));
    CHECK(Field_enteredDoorId == id);
    CHECK(Field_entranceDoorRecord.requiredFlagId == 321);
  }
  Field_roomData.header.minimapSceneLength = 26 * sizeof(u_long) - 1;
  CHECK(Field_SceneDoorIdEntry(0) == -1);
  Field_SetEntranceDoorRecordEntry(reinterpret_cast<char (*)[12]>(&door));
  CHECK(Field_enteredDoorId == -1);
  Field_SetEntranceDoorRecordEntry(nullptr);
  CHECK(Field_enteredDoorId == -1);
  CHECK(Field_entranceDoorRecord.requiredFlagId == 0);
}

TEST_CASE("Scene native marking preserves scene and halfword store semantics",
          "[scene-door-live][minimap]") {
  SceneDoorFixture fixture;
  fixture.words[25] = 0xAB04FE00;
  auto expected = fixture.words;
  expected[25] |= 0x00100000;
  Field_MarkEntranceDoorOnSceneMapEntry(0);
  CHECK(Field_enteredDoorId == 0);
  CHECK(fixture.words == expected);
  Field_MarkEntranceDoorOnSceneMapEntry(-1);
  CHECK(Field_enteredDoorId == -1);
  CHECK(fixture.words == expected);
  Field_roomData.minimapScene = nullptr;
  CHECK(Field_SceneDoorIdEntry(0) == -1);
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    Field_MarkEntranceDoorOnSceneMapEntry(static_cast<std::int32_t>(bits));
    CHECK(std::bit_cast<std::uint16_t>(Field_enteredDoorId) == bits);
    CHECK(VS::Field::EnteredDoorReloadByte(Field_enteredDoorId) ==
          static_cast<std::uint8_t>((bits & UINT8_MAX) + 1));
  }
  CHECK(fixture.words == expected);
}

TEST_CASE("Minimap geometry, visited connections and markers use native links",
          "[render][minimap]") {
  RenderFixture fixture(GENERATE(0, 1));
  auto bytes = Scene();
  VS::Field::ResolveLiveSceneRoomData(bytes.data(),
                                      bytes.size() * sizeof(u_long));
  VS::Field::DrawLiveMinimapScene(bytes.data(), bytes.size() * sizeof(u_long));
  const auto packets = fixture.Commands();
  REQUIRE(packets.size() == 9);
  const std::array<int, 9> commands{0xE1, 0x42, 0x42, 0x42, 0x2A,
                                    0x22, 0xE1, 0x60, 0x60};
  for (size_t index = 0; index < packets.size(); ++index) {
    CAPTURE(index);
    CHECK(packets[index]->code == commands[index]);
  }
  const auto* quad = reinterpret_cast<const POLY_F4*>(packets[4]);
  const std::array<int16_t, 6> coordinates{quad->x0, quad->y0, quad->x2,
                                           quad->y2, quad->x3, quad->y3};
  const std::array<int16_t, 6> expected{248, 180, 248, 212, 280, 212};
  const std::array<unsigned char, 3> color{quad->r0, quad->g0, quad->b0};
  const std::array<unsigned char, 3> expected_color{0, 127, 63};
  CHECK(coordinates == expected);
  CHECK(color == expected_color);
  CHECK(fixture.packets.back() == 0xDEADBEEF);
}

TEST_CASE(
    "Minimap door gates, entry blinking and visited rooms retain semantics",
    "[render][minimap][doors]") {
  const auto door = GENERATE(0u, 0x00100000u, 0x01000000u);
  const auto ticks = GENERATE(u_short{0}, u_short{32});
  const auto gate = GENERATE(char{0}, char{1});
  RenderFixture fixture;
  auto bytes = Scene();
  bytes[4] = 0;
  bytes[25] = door;
  Field_roomAnimationTicks = ticks;
  Audio_Seq_unusedStateFlags.opaque33F[1] = gate;
  VS::Field::ResolveLiveSceneRoomData(bytes.data(),
                                      bytes.size() * sizeof(u_long));
  VS::Field::DrawLiveMinimapScene(bytes.data(), bytes.size() * sizeof(u_long));
  const auto packets = fixture.Commands();
  const bool locked = door == 0x01000000u && gate == 0;
  const bool hidden = door == 0x00100000u && ticks == 0;
  CAPTURE(door, ticks, gate);
  REQUIRE(packets.size() == (hidden ? 6 : locked ? 7 : 8));
  if (!hidden) CHECK(packets.back()->code == (locked ? 0x74 : 0x60));
}

TEST_CASE("Minimap capacity errors do not overwrite packets or ordering links",
          "[render][minimap][bounds]") {
  RenderFixture fixture;
  auto bytes = Scene();
  VS::Field::ResolveLiveSceneRoomData(bytes.data(),
                                      bytes.size() * sizeof(u_long));
  Render_frameScratch.primitiveCursor =
      fixture.packets.data() + 0x20000 / 4 - 1;
  const auto saved = fixture.ordering;
  CHECK_THROWS_AS(VS::Field::DrawLiveMinimapScene(
                      bytes.data(), bytes.size() * sizeof(u_long)),
                  std::runtime_error);
  CHECK(fixture.ordering == saved);
  CHECK(fixture.packets.back() == 0xDEADBEEF);
}

TEST_CASE(
    "Projection offsets restore the original center for independent draws",
    "[render][minimap][projection]") {
  const Render_ScreenOffset original{160, 112};
  const Render_ScreenOffset map{264, 196};
  VS::Render::SetLiveProjectionScreenOffset(original);
  Render_ScreenOffset observed = VS::Render::GetLiveProjectionScreenOffset();
  CHECK(observed.x == 160);
  CHECK(observed.y == 112);
  VS::Render::SetLiveProjectionScreenOffset(map);
  observed = VS::Render::GetLiveProjectionScreenOffset();
  CHECK(observed.x == 264);
  CHECK(observed.y == 196);
  VS::Render::SetLiveProjectionScreenOffset(original);
  observed = VS::Render::GetLiveProjectionScreenOffset();
  CHECK(observed.x == 160);
  CHECK(observed.y == 112);
  Core_Gte_Reset();
}

TEST_CASE("Projection centers preserve the original high-halfword semantics",
          "[render][minimap][projection]") {
  const int x = GENERATE(-32768, -1, 0, 32767, 65535);
  const Render_ScreenOffset offset{x, 1 - x};
  VS::Render::SetLiveProjectionScreenOffset(offset);
  const Render_ScreenOffset observed =
      VS::Render::GetLiveProjectionScreenOffset();
  CHECK(observed.x == static_cast<u_short>(offset.x));
  CHECK(observed.y == static_cast<u_short>(offset.y));
  Core_Gte_Reset();
}

TEST_CASE("Every extracted ARM scene decodes packed rooms and its name table",
          "[content][minimap][disc]") {
  const auto root = ExtractedRoot();
  size_t arm_count = 0;
  for (const auto& entry :
       std::filesystem::directory_iterator(root / "SMALL")) {
    if (entry.path().extension() != ".ARM" ||
        !entry.path().stem().string().starts_with("SCEN")) {
      continue;
    }
    auto bytes = FileBytes(entry.path());
    CAPTURE(entry.path().filename().string());
    REQUIRE(VS::Field::IsValidMapScene(SceneBytes(bytes.data(), bytes.size())));
    auto* names =
        VS::Field::ResolveLiveSceneRoomData(bytes.data(), bytes.size());
    const auto name_offset = static_cast<size_t>(
        reinterpret_cast<const unsigned char*>(names) - bytes.data());
    REQUIRE(name_offset <= bytes.size());
    if (entry.path().filename() == "SCEN031.ARM") {
      CHECK(name_offset == bytes.size());
    } else {
      CHECK(FileWord(bytes, 0) <=
            (bytes.size() - name_offset) / sizeof(Field_MapRoomName));
    }
    ++arm_count;
  }
  CHECK(arm_count == 31);
}

TEST_CASE("Every extracted MPD minimap decodes and draws within packet bounds",
          "[content][minimap][disc]") {
  const auto root = ExtractedRoot();
  size_t scene_count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(root / "MAP")) {
    if (entry.path().extension() != ".MPD") continue;
    auto bytes = FileBytes(entry.path());
    if (bytes.empty()) continue;
    const size_t header = FileWord(bytes, 0);
    const size_t size = FileWord(bytes, header + 14 * sizeof(u_long));
    if (size == 0) continue;
    size_t offset = header + 24 * sizeof(u_long);
    for (size_t section = 0; section < 14; ++section) {
      offset += FileWord(bytes, header + section * sizeof(u_long));
    }
    CAPTURE(entry.path().filename().string(), offset, size);
    REQUIRE(offset <= bytes.size());
    REQUIRE(size <= bytes.size() - offset);
    REQUIRE(
        VS::Field::IsValidMapScene(SceneBytes(bytes.data() + offset, size)));
    VS::Field::ResolveLiveSceneRoomData(bytes.data() + offset, size);
    RenderFixture fixture;
    auto* scene = reinterpret_cast<Field_MapScene*>(bytes.data() + offset);
    scene->rooms[0].visited = 1;
    VS::Field::DrawLiveMinimapScene(scene, size);
    CHECK(fixture.packets.back() == 0xDEADBEEF);
    ++scene_count;
  }
  CHECK(scene_count > 0);
}

namespace {
using VS::Field::MinimapColor;
using VS::Field::MinimapPrimitiveCounts;
using VS::Field::MinimapScreenPoint;
using VS::Field::MinimapVertex;

struct RecordingMinimapRenderer final : VS::Field::MinimapRenderer {
  MinimapPrimitiveCounts counts;
  std::vector<std::string> calls;
  std::vector<MinimapColor> colors;
  std::vector<std::array<MinimapScreenPoint, 4>> quads;
  std::vector<MinimapScreenPoint> markers;
  bool locked = false;
  int begins = 0;

  void Begin(const MinimapPrimitiveCounts& reserved) override {
    counts = reserved;
    ++begins;
  }
  MinimapScreenPoint Project(const MinimapVertex& vertex) override {
    return {vertex.x, vertex.y};
  }
  bool IsDoorGateLocked(std::uint8_t /*gate*/) override { return locked; }
  void DrawTriangle(const std::array<MinimapScreenPoint, 3>& /*corners*/,
                    MinimapColor color) override {
    calls.emplace_back("triangle");
    colors.push_back(color);
  }
  void DrawQuad(const std::array<MinimapScreenPoint, 4>& corners,
                MinimapColor color) override {
    calls.emplace_back("quad");
    colors.push_back(color);
    quads.push_back(corners);
  }
  void DrawLine(MinimapScreenPoint /*from*/, MinimapScreenPoint /*to*/,
                MinimapColor color) override {
    calls.emplace_back("line");
    colors.push_back(color);
  }
  void DrawLockedDoor(MinimapScreenPoint position) override {
    calls.emplace_back("locked");
    markers.push_back(position);
  }
  void DrawDoorCross(MinimapScreenPoint position, MinimapColor color) override {
    calls.emplace_back("cross");
    colors.push_back(color);
    markers.push_back(position);
  }
  void Finish() override { calls.emplace_back("finish"); }
};

std::vector<std::byte> ResolvedScene(std::vector<u_long> words) {
  const auto view = std::as_bytes(std::span(words));
  std::vector<std::byte> bytes(view.begin(), view.end());
  CHECK(VS::Field::ResolveSceneRoomData(bytes) == bytes.size());
  return bytes;
}
}  // namespace

TEST_CASE("Typed minimap scenes draw rooms in order through a renderer",
          "[render][minimap][typed]") {
  const auto bytes = ResolvedScene(Scene());
  RecordingMinimapRenderer renderer;
  VS::Field::DrawMinimapScene(bytes, false, renderer);
  CHECK(renderer.begins == 1);
  CHECK(renderer.counts == MinimapPrimitiveCounts{1, 1, 3, 1});
  const std::vector<std::string> calls{"triangle", "quad", "line",  "line",
                                       "cross",    "line", "finish"};
  CHECK(renderer.calls == calls);
  const std::vector<MinimapColor> colors{{0, 127, 63},    {0, 127, 63},
                                         {0, 127, 255},   {0, 95, 47},
                                         {127, 127, 127}, {0, 127, 255}};
  CHECK(renderer.colors == colors);
  REQUIRE(renderer.quads.size() == 1);
  const std::array<MinimapScreenPoint, 4> strip{
      {{-16, -16}, {16, -16}, {-16, 16}, {16, 16}}};
  CHECK(renderer.quads[0] == strip);
}

TEST_CASE("Typed minimap doors follow gates, entry pulses and special flags",
          "[render][minimap][typed][doors]") {
  auto words = Scene();
  words[25] = 0x01000000u;
  auto bytes = ResolvedScene(words);
  RecordingMinimapRenderer locked;
  locked.locked = true;
  VS::Field::DrawMinimapScene(bytes, false, locked);
  REQUIRE(locked.markers.size() == 1);
  CHECK(locked.markers[0] == MinimapScreenPoint{-16 - 3, -16 - 4});

  words[25] = 0x00100000u;
  bytes = ResolvedScene(words);
  RecordingMinimapRenderer hidden;
  VS::Field::DrawMinimapScene(bytes, false, hidden);
  CHECK(hidden.markers.empty());
  RecordingMinimapRenderer pulsing;
  VS::Field::DrawMinimapScene(bytes, true, pulsing);
  REQUIRE(pulsing.markers.size() == 1);
  CHECK(pulsing.colors[4] == MinimapColor{255, 0, 127});

  words[25] = 0x00040000u;
  bytes = ResolvedScene(words);
  RecordingMinimapRenderer special;
  VS::Field::DrawMinimapScene(bytes, false, special);
  CHECK(special.colors[4] == MinimapColor{0, 127, 255});

  words[25] = 0x00010000u;
  bytes = ResolvedScene(words);
  RecordingMinimapRenderer other;
  VS::Field::DrawMinimapScene(bytes, false, other);
  CHECK(other.markers.empty());
}

TEST_CASE("Typed minimap draws reject unresolved scenes before rendering",
          "[render][minimap][typed][bounds]") {
  const auto words = Scene();
  RecordingMinimapRenderer renderer;
  CHECK_THROWS_AS(VS::Field::DrawMinimapScene(std::as_bytes(std::span(words)),
                                              false, renderer),
                  std::runtime_error);
  CHECK(renderer.begins == 0);
  CHECK(renderer.calls.empty());
}
