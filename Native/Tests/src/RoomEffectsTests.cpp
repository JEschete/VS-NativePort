#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Core/Memory/HeapLive.hpp"
#include "Field/RoomEffects.hpp"
#include "Field/RoomEffectsCore.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Legacy/Psx.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Room.h"
}

namespace {
namespace fx = VS::Field;
namespace layout = VS::Field::room_effects;
using Bytes = std::vector<std::byte>;
using fx::RoomEffectCommandResult;
using fx::RoomEffectOpcode;
using VS::test_support::ChannelWarnings;

constexpr const char* kChannel = "field.effects";

template <class Value>
void Put(Bytes& bytes, std::size_t offset, Value value) {
  REQUIRE(offset + sizeof(Value) <= bytes.size());
  std::memcpy(bytes.data() + offset, &value, sizeof(Value));
}

template <class Value>
Value Get(const Bytes& bytes, std::size_t offset) {
  REQUIRE(offset + sizeof(Value) <= bytes.size());
  Value value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(Value));
  return value;
}

constexpr std::size_t Group(std::size_t index) { return 4 + (index * 64); }

Bytes MakeGeometry(std::int32_t groups, std::size_t extra) {
  Bytes bytes(Group(static_cast<std::size_t>(groups)) + extra);
  Put(bytes, 0, groups);
  return bytes;
}

void SetGroupWord(Bytes& geometry, std::size_t group, std::size_t word,
                  std::int32_t value) {
  Put(geometry, Group(group) + (word * 4), value);
}

std::int32_t GroupWord(const Bytes& geometry, std::size_t group,
                       std::size_t word) {
  return Get<std::int32_t>(geometry, Group(group) + (word * 4));
}

// The words of a group from `first`, in order.
std::vector<std::int32_t> GroupWords(const Bytes& geometry, std::size_t group,
                                     std::size_t first, std::size_t count) {
  std::vector<std::int32_t> words;
  for (std::size_t word = first; word < first + count; ++word) {
    words.push_back(GroupWord(geometry, group, word));
  }
  return words;
}

// RoomEffectStorageId is an opaque handle; any value names a reservation.
fx::RoomEffectStorageId StorageAt(std::uintptr_t value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<fx::RoomEffectStorageId>(value);
}

std::int16_t Low(std::int32_t value) {
  return static_cast<std::int16_t>(static_cast<std::uint32_t>(value) & 0xFFFFU);
}

std::int16_t High(std::int32_t value) {
  return static_cast<std::int16_t>(static_cast<std::uint32_t>(value) >> 16U);
}

// Builds an effect script section: each script's header and group ids,
// then its commands, then the zero-size terminator.
struct Script {
  Bytes bytes;

  std::size_t Begin(std::initializer_list<std::int8_t> ids,
                    std::uint8_t active) {
    const std::size_t start = bytes.size();
    const auto count = static_cast<std::uint8_t>(ids.size());
    bytes.resize(start + layout::ScriptPreambleBytes(count));
    Put(bytes, start, count);
    Put(bytes, start + 2, active);
    std::size_t at = start + 4;
    for (const std::int8_t id : ids) {
      Put(bytes, at++, id);
    }
    return start;
  }

  std::size_t Raw(std::int16_t size, std::int16_t opcode,
                  std::initializer_list<std::int16_t> operands) {
    const std::size_t at = bytes.size();
    bytes.resize(at + 4 + (operands.size() * 2));
    Put(bytes, at, size);
    Put(bytes, at + 2, opcode);
    std::size_t operand_at = at + 4;
    for (const std::int16_t operand : operands) {
      Put(bytes, operand_at, operand);
      operand_at += 2;
    }
    return at;
  }

  std::size_t Add(RoomEffectOpcode opcode,
                  std::initializer_list<std::int16_t> operands) {
    return Raw(static_cast<std::int16_t>(4 + (operands.size() * 2)),
               static_cast<std::int16_t>(opcode), operands);
  }

  std::size_t Jump(std::int16_t distance) {
    return Raw(distance, static_cast<std::int16_t>(RoomEffectOpcode::kJump),
               {});
  }

  std::size_t End() { return Raw(0, 0, {}); }
};

fx::RoomEffectCommand MakeCommand(
    RoomEffectOpcode opcode, std::initializer_list<std::int16_t> operands) {
  Script script;
  script.Add(opcode, operands);
  const auto decoded = layout::DecodeCommand(script.bytes, 0);
  REQUIRE(decoded.status == layout::CommandStatus::kDecoded);
  return decoded.command;
}

struct SoundCall {
  char kind = ' ';
  std::int32_t sound = 0;
  std::int32_t first = 0;
  std::int32_t second = 0;

  friend bool operator==(const SoundCall&, const SoundCall&) = default;
};

struct ImageMove {
  fx::RoomImageRect source;
  std::int32_t x = 0;
  std::int32_t y = 0;

  friend bool operator==(const ImageMove&, const ImageMove&) = default;
};

// Records every call the room effects make into the rest of the game.
struct FakeHost final : fx::RoomEffectHost {
  Bytes geometry;
  Bytes textures;
  Bytes scripts;
  Bytes entry;
  bool sound_bank = true;
  bool fast_forward = false;
  bool refuse_storage = false;
  bool entry_result = true;
  std::uint16_t floor_code = 0;
  std::vector<SoundCall> sounds;
  std::vector<ImageMove> moves;
  std::vector<std::size_t> reserved;
  std::vector<fx::RoomEffectStorageId> released;
  std::vector<std::pair<int, int>> floor_queries;
  std::size_t entry_code_bytes = 0;
  std::uintptr_t next_storage = 1;

  std::span<std::byte> Geometry() override { return geometry; }
  std::span<std::byte> TextureEffects() override { return textures; }
  std::span<const std::byte> EffectScripts() override { return scripts; }
  std::span<const std::byte> EntryPointSection() override { return entry; }
  bool HasSoundBank() override { return sound_bank; }
  void PlayCenteredSound(std::int32_t sound_index) override {
    sounds.push_back({'c', sound_index, 0, 0});
  }
  void PlaySound(std::int32_t sound_index, std::int32_t pan,
                 std::int32_t volume) override {
    sounds.push_back({'p', sound_index, pan, volume});
  }
  void StopSound(std::int32_t sound_index) override {
    sounds.push_back({'s', sound_index, 0, 0});
  }
  void FadeSound(std::int32_t sound_index, std::int32_t volume,
                 std::int32_t fade_ticks) override {
    sounds.push_back({'f', sound_index, volume, fade_ticks});
  }
  void MoveImage(const fx::RoomImageRect& source, std::int32_t x,
                 std::int32_t y) override {
    moves.push_back({source, x, y});
  }
  std::optional<fx::RoomEffectStorageId> ReserveObjectStorage(
      std::size_t bytes) override {
    reserved.push_back(bytes);
    if (refuse_storage) {
      return std::nullopt;
    }
    return StorageAt(next_storage++);
  }
  void ReleaseObjectStorage(fx::RoomEffectStorageId storage) override {
    released.push_back(storage);
  }
  std::uint16_t FloorHeightCode(int world_x, int world_z) override {
    floor_queries.emplace_back(world_x, world_z);
    return floor_code;
  }
  bool FastForwarding() override { return fast_forward; }
  bool RunRoomEntryPoint(std::span<const std::byte> code) override {
    entry_code_bytes = code.size();
    return entry_result;
  }
};

// Two geometry groups and a polygon after them; a frame effect on the
// polygon and a swing on group 1 that starts inactive; one script that
// starts with two objects and one that waits to be started.
struct StandardRoom {
  static constexpr std::size_t kPolygon = Group(2);
  static constexpr std::size_t kSwingRecord = 24;
  FakeHost host;
  fx::RoomEffectState state;

  StandardRoom() {
    host.geometry = MakeGeometry(2, 0x28);
    SetGroupWord(host.geometry, 0, 1, 5);
    SetGroupWord(host.geometry, 0, 2, -6);
    SetGroupWord(host.geometry, 0, 3, 7);
    SetGroupWord(host.geometry, 0, 4, 0x100);
    SetGroupWord(host.geometry, 0, 5, 0x200);
    SetGroupWord(host.geometry, 0, 6, 0x300);
    host.textures.resize(68);
    Put(host.textures, 0, static_cast<std::int32_t>(kPolygon));
    Put<std::uint8_t>(host.textures, 0xA, 2);
    Put<std::uint8_t>(host.textures, 0xB, 1);
    Put<std::uint8_t>(host.textures, 0xD, 8);
    Put(host.textures, kSwingRecord, static_cast<std::int32_t>(Group(1)));
    Put<std::uint8_t>(host.textures, kSwingRecord + 5, 1);
    Put<std::uint8_t>(host.textures, kSwingRecord + 6, 6);
    Put<std::uint8_t>(host.textures, 60 + 6, 3);
    Script script;
    script.Begin({0, -1}, 1);
    script.Add(RoomEffectOpcode::kSkip, {});
    script.End();
    script.Begin({1}, 0);
    script.End();
    host.scripts = script.bytes;
  }

  void Initialize() { fx::InitializeRoomEffects(state, host); }
};
}  // namespace

TEST_CASE("Room geometry groups are found inside the group count",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  const Bytes geometry = MakeGeometry(2, 0);
  CHECK(fx::FindRoomGeometryGroup(geometry, 0) ==
        std::optional<std::size_t>{4});
  CHECK(fx::FindRoomGeometryGroup(geometry, 1) ==
        std::optional<std::size_t>{68});
  CHECK_FALSE(fx::FindRoomGeometryGroup(geometry, 2));
  CHECK_FALSE(fx::FindRoomGeometryGroup(geometry, -1));
  CHECK_FALSE(fx::FindRoomGeometryGroup({}, 0));
  CHECK(warnings.messages.empty());
}

TEST_CASE("Room geometry group records past the geometry bytes are refused",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  const Bytes short_geometry(2);
  CHECK_FALSE(fx::FindRoomGeometryGroup(short_geometry, 0));
  Bytes geometry = MakeGeometry(2, 0);
  Put<std::int32_t>(geometry, 0, 5);
  CHECK_FALSE(fx::FindRoomGeometryGroup(geometry, 4));
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Room geometry of 2 bytes has no group count",
            "Geometry group 4 refused: its record at 260 leaves the 132 byte "
            "geometry"});
}

TEST_CASE("Showing hiding and disabling a geometry group edit only its flags",
          "[field][field-room-effects]") {
  Bytes geometry = MakeGeometry(2, 0);
  SetGroupWord(geometry, 1, 0, 0x00FF0101);
  CHECK(fx::ShowRoomGeometryGroup(geometry, 1));
  CHECK(GroupWord(geometry, 1, 0) == 0x00FF0001);
  CHECK(fx::HideRoomGeometryGroup(geometry, 1));
  CHECK(GroupWord(geometry, 1, 0) == 0x00FF0101);
  CHECK(fx::DisableRoomGeometryGroup(geometry, 1));
  CHECK(GroupWord(geometry, 1, 0) == 0x00FF1101);
  CHECK_FALSE(fx::HideRoomGeometryGroup(geometry, 2));
  CHECK_FALSE(fx::ShowRoomGeometryGroup(geometry, -1));
}

TEST_CASE("Geometry group flags swap and depth biases wrap in their halfword",
          "[field][field-room-effects]") {
  Bytes geometry = MakeGeometry(2, 0);
  SetGroupWord(geometry, 0, 0, 0x11);
  SetGroupWord(geometry, 1, 0, 0x22);
  CHECK(fx::SwapRoomGeometryGroupFlags(geometry, 0, 1));
  CHECK(GroupWord(geometry, 0, 0) == 0x22);
  CHECK(GroupWord(geometry, 1, 0) == 0x11);
  CHECK_FALSE(fx::SwapRoomGeometryGroupFlags(geometry, 0, 2));
  Put<std::uint16_t>(geometry, Group(1) + 2, 2);
  CHECK(fx::AddRoomGeometryDepthBias(geometry, 1, -3));
  CHECK(GroupWord(geometry, 1, 0) == static_cast<std::int32_t>(0xFFFF0011U));
  CHECK_FALSE(fx::AddRoomGeometryDepthBias(geometry, 5, 1));
}

namespace {
// One group whose polygon block holds a triangle then a quad.
struct ColorBlock {
  static constexpr std::size_t kBlock = Group(1);
  static constexpr std::size_t kTriangle = kBlock + 8;
  static constexpr std::size_t kQuad = kTriangle + 32;
  Bytes geometry = MakeGeometry(1, 80);

  ColorBlock() {
    SetGroupWord(geometry, 0, 7, static_cast<std::int32_t>(kBlock));
    Put<std::int32_t>(geometry, kBlock, 1);
    Put<std::int32_t>(geometry, kBlock + 4, 1);
    for (std::size_t index = kTriangle; index < geometry.size(); ++index) {
      geometry[index] = static_cast<std::byte>(index);
    }
  }
};

void SwapPair(Bytes& bytes, std::size_t offset) {
  std::swap(bytes.at(offset), bytes.at(offset + 1));
}
}  // namespace

TEST_CASE("Red-green swaps exchange each corner's colour bytes",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  ColorBlock block;
  Bytes expected = block.geometry;
  for (const std::size_t corner : {0xCU, 0x10U, 0x14U}) {
    SwapPair(expected, ColorBlock::kTriangle + corner);
  }
  for (const std::size_t corner : {0xCU, 0x10U, 0x14U, 0x24U}) {
    SwapPair(expected, ColorBlock::kQuad + corner);
  }
  fx::SwapRoomGeometryRedGreen(block.geometry, 0);
  CHECK(block.geometry == expected);
  fx::SwapRoomGeometryRedGreen(block.geometry, 1);
  CHECK(block.geometry == expected);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Red-green swaps refuse polygon blocks outside the geometry",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  ColorBlock block;
  SetGroupWord(block.geometry, 0, 7, 200);
  fx::SwapRoomGeometryRedGreen(block.geometry, 0);
  SetGroupWord(block.geometry, 0, 7, -4);
  fx::SwapRoomGeometryRedGreen(block.geometry, 0);
  SetGroupWord(block.geometry, 0, 7,
               static_cast<std::int32_t>(ColorBlock::kBlock));
  Put<std::int32_t>(block.geometry, ColorBlock::kBlock + 4, 2);
  const Bytes expected = block.geometry;
  fx::SwapRoomGeometryRedGreen(block.geometry, 0);
  CHECK(block.geometry == expected);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Red-green swap of group 0 refused: its polygon block at 200 "
            "leaves the 148 byte geometry",
            "Red-green swap of group 0 refused: its polygon block at -4 "
            "leaves the 148 byte geometry",
            "Red-green swap of group 0 refused: 1 triangles and 2 quads at 76 "
            "leave the 148 byte geometry"});
}
namespace {
using Corners = std::array<std::uint8_t, fx::kRoomEffectUvCorners>;

Corners PolygonCorners(const Bytes& geometry, std::size_t polygon,
                       const Corners& unused,
                       const std::array<std::size_t, 4>& offsets) {
  Corners corners = unused;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    corners.at(corner) =
        Get<std::uint8_t>(geometry, polygon + offsets.at(corner));
  }
  return corners;
}

Corners PolygonU(const Bytes& geometry) {
  return PolygonCorners(geometry, 4, {}, layout::kPolygonUOffsets);
}

Corners PolygonV(const Bytes& geometry) {
  return PolygonCorners(geometry, 4, {}, layout::kPolygonVOffsets);
}

// A two-column sheet of three 8x4 frames at (16, 32) on the quad at 4.
fx::TextureFrameEffect MakeFrameEffect() {
  fx::TextureFrameEffect effect;
  effect.geometry_offset = 4;
  effect.delay = 1;
  effect.base_u = 16;
  effect.base_v = 32;
  effect.frame_count = 3;
  effect.columns = 2;
  effect.frame_width = 8;
  effect.frame_height = 4;
  effect.u = {0, 7, 0, 7};
  effect.v = {0, 0, 3, 3};
  return effect;
}

// An 8x4 window scrolling right inside a 20x8 rectangle at (10, 0).
fx::TextureScrollEffect MakeScrollEffect() {
  fx::TextureScrollEffect effect;
  effect.geometry_offset = 4;
  effect.base_u = 10;
  effect.width = 20;
  effect.height = 8;
  effect.current_u = 20;
  effect.window_width = 8;
  effect.window_height = 4;
  effect.step_u = 3;
  effect.u = {0, 8, 0, 8};
  effect.v = {0, 0, 4, 4};
  return effect;
}

fx::TextureImageEffect MakeImageEffect(std::int32_t scroll_step) {
  fx::TextureImageEffect effect;
  effect.frame_count = 2;
  effect.columns = 2;
  effect.scroll_step = scroll_step;
  effect.rect = {448, 0, 16, 32};
  return effect;
}

fx::SwingEffect MakeSwing(std::uint8_t axis) {
  fx::SwingEffect effect;
  effect.geometry_offset = 4;
  effect.needs_reset = 1;
  effect.axis = axis;
  effect.damping_shift = 3;
  effect.rest_angle = 300;
  effect.initial_velocity = 100;
  return effect;
}
}  // namespace

TEST_CASE("Texture frames wait out their delay before the next sheet cell",
          "[field][field-room-effects]") {
  Bytes geometry(44);
  fx::TextureFrameEffect effect = MakeFrameEffect();
  CHECK(fx::AdvanceRoomTextureFrames(effect, geometry));
  CHECK(PolygonU(geometry) == Corners{});
  CHECK(fx::AdvanceRoomTextureFrames(effect, geometry));
  CHECK(effect.frame == 1);
  CHECK(PolygonU(geometry) == Corners{24, 31, 24, 31});
  CHECK(PolygonV(geometry) == Corners{32, 32, 35, 35});
}

TEST_CASE("Texture frames step down the sheet rows and wrap to the first",
          "[field][field-room-effects]") {
  Bytes geometry(44);
  fx::TextureFrameEffect effect = MakeFrameEffect();
  effect.delay = 0;
  effect.frame = 1;
  CHECK(fx::AdvanceRoomTextureFrames(effect, geometry));
  CHECK(PolygonU(geometry) == Corners{16, 23, 16, 23});
  CHECK(PolygonV(geometry) == Corners{36, 36, 39, 39});
  CHECK(fx::AdvanceRoomTextureFrames(effect, geometry));
  CHECK(effect.frame == 0);
  CHECK(PolygonV(geometry) == Corners{32, 32, 35, 35});
}

TEST_CASE("Texture frames on a triangle leave its fourth corner alone",
          "[field][field-room-effects]") {
  Bytes geometry(44);
  Put<std::uint8_t>(geometry, 4 + 0xF, 0x34);
  Put<std::uint8_t>(geometry, 4 + 0x23, 0xAA);
  Put<std::uint8_t>(geometry, 4 + 0x27, 0xBB);
  fx::TextureFrameEffect effect = MakeFrameEffect();
  effect.delay = 0;
  CHECK(fx::AdvanceRoomTextureFrames(effect, geometry));
  CHECK(PolygonU(geometry) == Corners{24, 31, 24, 0xAA});
  CHECK(PolygonV(geometry) == Corners{32, 32, 35, 0xBB});
}

TEST_CASE("Texture frames with no columns use the divide-by-zero result",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  Bytes geometry(44);
  fx::TextureFrameEffect effect = MakeFrameEffect();
  effect.delay = 0;
  effect.columns = 0;
  CHECK(fx::AdvanceRoomTextureFrames(effect, geometry));
  CHECK(PolygonU(geometry) == Corners{24, 31, 24, 31});
  CHECK(PolygonV(geometry) == Corners{28, 28, 31, 31});
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Texture frame effect has no columns; frame 1 uses the hardware's "
            "divide-by-zero result"});
}

TEST_CASE(
    "Texture frame and scroll effects stop on polygons outside the "
    "geometry",
    "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  Bytes geometry(44);
  fx::TextureFrameEffect frames = MakeFrameEffect();
  for (const std::int32_t offset : {30, 16, -1}) {
    frames.geometry_offset = offset;
    CHECK_FALSE(fx::AdvanceRoomTextureFrames(frames, geometry));
  }
  fx::TextureScrollEffect scroll = MakeScrollEffect();
  scroll.geometry_offset = 40;
  CHECK_FALSE(fx::ScrollRoomTextureCoordinates(scroll, geometry));
  CHECK(frames.tick == 0);
  CHECK(geometry == Bytes(44));
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Texture frame effect stopped: its polygon at 30 leaves the 44 "
            "byte geometry",
            "Texture frame effect stopped: its polygon's UVs at 16 leave the "
            "44 byte geometry",
            "Texture frame effect stopped: its polygon at -1 leaves the 44 "
            "byte geometry",
            "Texture scroll effect stopped: its polygon at 40 leaves the 44 "
            "byte geometry"});
}

TEST_CASE("Texture scrolls wrap their window inside the base rectangle",
          "[field][field-room-effects]") {
  Bytes geometry(44);
  fx::TextureScrollEffect effect = MakeScrollEffect();
  CHECK(fx::ScrollRoomTextureCoordinates(effect, geometry));
  CHECK(PolygonU(geometry) == Corners{10, 18, 10, 18});
  CHECK(fx::ScrollRoomTextureCoordinates(effect, geometry));
  CHECK(effect.current_u == 13);
  effect.step_u = -5;
  effect.step_v = -1;
  CHECK(fx::ScrollRoomTextureCoordinates(effect, geometry));
  CHECK(effect.current_u == 22);
  CHECK(PolygonV(geometry) == Corners{4, 4, 8, 8});
}

TEST_CASE("Texture images copy the next sheet cell and its half-size copy",
          "[field][field-room-effects]") {
  FakeHost host;
  fx::TextureImageEffect effect = MakeImageEffect(0);
  CHECK(fx::AnimateRoomTextureImages(effect, host));
  CHECK(host.moves == std::vector<ImageMove>{{{144, 256, 16, 32}, 128, 256},
                                             {{72, 256, 8, 16}, 64, 256}});
  CHECK(fx::AnimateRoomTextureImages(effect, host));
  CHECK(fx::AnimateRoomTextureImages(effect, host));
  CHECK(effect.frame == 1);
  REQUIRE(host.moves.size() == 6);
  CHECK(host.moves.at(2) == ImageMove{{128, 288, 16, 32}, 128, 256});
  CHECK(host.moves.at(3) == ImageMove{{64, 272, 8, 16}, 64, 256});
}

TEST_CASE("Texture image bands scroll and wrap their top rows to the foot",
          "[field][field-room-effects]") {
  FakeHost host;
  fx::TextureImageEffect effect = MakeImageEffect(4);
  CHECK(fx::AnimateRoomTextureImages(effect, host));
  CHECK(effect.frame == 4);
  CHECK(host.moves == std::vector<ImageMove>{{{136, 260, 8, 28}, 128, 256},
                                             {{68, 258, 4, 14}, 64, 256},
                                             {{136, 256, 8, 4}, 128, 284},
                                             {{68, 256, 4, 2}, 64, 270}});
  host.moves.clear();
  effect.frame = 28;
  CHECK(fx::AnimateRoomTextureImages(effect, host));
  CHECK(effect.frame == 0);
  CHECK(host.moves == std::vector<ImageMove>{{{136, 256, 8, 32}, 128, 256},
                                             {{68, 256, 4, 16}, 64, 256}});
}

TEST_CASE("Swing effects settle at their rest angle and then reverse",
          "[field][field-room-effects]") {
  Bytes geometry = MakeGeometry(1, 0);
  fx::SwingEffect effect = MakeSwing(1);
  CHECK(fx::UpdateRoomRotationEffect(effect, geometry));
  CHECK(GroupWord(geometry, 0, 5) == 100);
  CHECK(fx::UpdateRoomRotationEffect(effect, geometry));
  CHECK_FALSE(fx::UpdateRoomRotationEffect(effect, geometry));
  CHECK(GroupWord(geometry, 0, 5) == 300);
  CHECK(GroupWord(geometry, 0, 0) == 0x6000);
  fx::SwingEffect reversed = MakeSwing(1);
  reversed.needs_reset = 0;
  reversed.angle = 300;
  reversed.angular_velocity = -100;
  reversed.start_angle = 300;
  reversed.rest_angle = 0;
  reversed.initial_velocity = -100;
  CHECK(effect == reversed);
}

TEST_CASE("Swing effects write the rotation word of their axis",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  Bytes geometry = MakeGeometry(1, 0);
  fx::SwingEffect about_x = MakeSwing(2);
  CHECK(fx::UpdateRoomRotationEffect(about_x, geometry));
  fx::SwingEffect about_z = MakeSwing(0);
  CHECK(fx::UpdateRoomRotationEffect(about_z, geometry));
  CHECK(GroupWord(geometry, 0, 4) == 100);
  CHECK(GroupWord(geometry, 0, 6) == 100);
  CHECK(GroupWord(geometry, 0, 0) == 0);
  fx::SwingEffect outside = MakeSwing(1);
  outside.geometry_offset = 200;
  CHECK_FALSE(fx::UpdateRoomRotationEffect(outside, geometry));
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Swing effect refused: its group at 200 leaves the 68 byte "
            "geometry"});
}
namespace {
using Storage = fx::RoomEffectStorageId;
using Messages = std::vector<std::string>;
}  // namespace

TEST_CASE("Room set-up fills the texture effect slots from their records",
          "[field][field-room-effects]") {
  StandardRoom room;
  room.Initialize();
  const auto& slots = room.state.instances;
  CHECK(room.state.texture_effect_count == 2);
  CHECK(slots.at(0).active == 1);
  CHECK(slots.at(1).active == 0);
  CHECK(slots.at(1).kind_or_object_count == 6);
  CHECK(slots.at(1).data_offset == StandardRoom::kSwingRecord);
  CHECK(slots.at(1).update_enabled == 1);
  CHECK(Get<std::uint8_t>(room.host.textures, 4) == 1);
  CHECK(Get<std::uint8_t>(room.host.textures, StandardRoom::kSwingRecord + 4) ==
        1);
}

TEST_CASE("Room set-up builds the objects of the scripts that start active",
          "[field][field-room-effects]") {
  StandardRoom room;
  room.Initialize();
  fx::RoomEffectObject first;
  first.object_id = 0;
  first.group_offset = Group(0);
  first.cursor = 8;
  first.position = {80, -96, 112};
  first.rotation = {0x100, 0x200, 0x300};
  fx::RoomEffectObject inert;
  inert.cursor = 8;
  const auto& slots = room.state.instances;
  CHECK(room.state.script_count == 2);
  CHECK(slots.at(64).objects ==
        std::vector<fx::RoomEffectObject>{first, inert});
  CHECK(slots.at(64).storage == StorageAt(1));
  CHECK(slots.at(65).active == 0);
  CHECK(slots.at(65).data_offset == 16);
  CHECK(slots.at(65).objects.empty());
  CHECK(room.host.reserved == std::vector<std::size_t>{104});
  CHECK(room.state.sound_id == -1);
}

TEST_CASE("Room set-up refuses effect scripts that leave their section",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  Script unterminated;
  unterminated.Begin({0}, 1);
  unterminated.Add(RoomEffectOpcode::kSkip, {});
  Script negative;
  negative.Begin({0}, 1);
  negative.Raw(-4, 2, {});
  negative.End();
  for (const Bytes& scripts : {unterminated.bytes, negative.bytes, Bytes(2)}) {
    room.host.scripts = scripts;
    room.Initialize();
    CHECK(room.state.script_count == 0);
  }
  CHECK(room.host.reserved.empty());
  CHECK(warnings.messages ==
        Messages{"Effect script at 0 refused: its command at 12 leaves the 12 "
                 "byte section",
                 "Effect script at 0 refused: its command at 8 has size -4",
                 "Effect script at 0 refused: its header leaves the 2 byte "
                 "section"});
}

TEST_CASE("Room set-up refuses texture effects that leave their section",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  room.host.scripts.clear();
  room.host.textures.resize(56);
  room.Initialize();
  CHECK(room.state.texture_effect_count == 1);
  room.host.textures.resize(30);
  room.Initialize();
  CHECK(room.state.texture_effect_count == 1);
  CHECK(warnings.messages ==
        Messages{"Texture effect at 24 refused: its kind 6 record leaves the "
                 "56 byte section",
                 "Texture effect at 24 refused: its kind leaves the 30 byte "
                 "section"});
}

TEST_CASE("A script whose objects the heap refuses ends on its first update",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  room.host.refuse_storage = true;
  room.Initialize();
  CHECK(room.state.script_count == 2);
  CHECK(room.state.instances.at(64).active == 1);
  CHECK(room.state.instances.at(64).objects.empty());
  fx::UpdateRoomEffects(room.state, room.host);
  CHECK(room.state.instances.at(64).active == 0);
  CHECK(room.host.released.empty());
  CHECK(warnings.messages ==
        Messages{"Effect script at 0: the heap refused 2 objects"});
}

TEST_CASE("Rebuilding script objects that were never released is reported",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  room.Initialize();
  fx::CreateRoomEffectObjects(room.state.instances.at(64), room.host);
  room.Initialize();
  CHECK(room.host.released == std::vector<Storage>{StorageAt(1)});
  CHECK(room.host.reserved == std::vector<std::size_t>{104, 104, 104});
  CHECK(room.state.instances.at(64).storage == StorageAt(3));
  CHECK(warnings.messages ==
        Messages{"Effect script at 0 rebuilt its objects without releasing "
                 "them; releasing them now",
                 "Effect script at 0 replaces objects that were never "
                 "released; leaving them to the heap"});
}

TEST_CASE("Starting a room effect activates a texture slot or builds a script",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  room.Initialize();
  auto& slots = room.state.instances;
  CHECK(fx::StartRoomEffect(room.state, room.host, 1));
  CHECK(slots.at(1).active == 1);
  for (const int index : {2, 63, 66, -1}) {
    CHECK_FALSE(fx::StartRoomEffect(room.state, room.host, index));
  }
  const std::uint32_t generation = slots.at(65).generation;
  CHECK(fx::StartRoomEffect(room.state, room.host, 65));
  CHECK(slots.at(65).generation == generation + 2);
  CHECK(room.host.reserved == std::vector<std::size_t>{104, 52});
  CHECK(warnings.messages ==
        Messages{"Room effect -1 refused: it is negative"});
}

TEST_CASE("Restarting a running script releases its objects first",
          "[field][field-room-effects]") {
  StandardRoom room;
  room.Initialize();
  CHECK(fx::StartRoomEffect(room.state, room.host, 64));
  CHECK(room.host.released == std::vector<Storage>{StorageAt(1)});
  CHECK(room.state.instances.at(64).storage == StorageAt(2));
  CHECK(room.state.instances.at(64).objects.size() == 2);
}

TEST_CASE("Toggling room effect updates flips only slots that exist",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  room.Initialize();
  CHECK(fx::ToggleRoomEffectUpdates(room.state, 0));
  CHECK(room.state.instances.at(0).update_enabled == 0);
  CHECK(fx::ToggleRoomEffectUpdates(room.state, 65));
  CHECK(room.state.instances.at(65).update_enabled == 0);
  for (const int index : {2, 66, -2}) {
    CHECK_FALSE(fx::ToggleRoomEffectUpdates(room.state, index));
  }
  CHECK(warnings.messages ==
        Messages{"Toggling room effect -2 refused: it is negative"});
}

TEST_CASE("Room effect activity is read only inside the hundred slots",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  room.Initialize();
  CHECK(fx::IsRoomEffectActive(room.state, 0) == 1);
  CHECK(fx::IsRoomEffectActive(room.state, 1) == 0);
  CHECK(fx::IsRoomEffectActive(room.state, 64) == 1);
  CHECK(fx::IsRoomEffectActive(room.state, 100) == 0);
  CHECK(fx::IsRoomEffectActive(room.state, -1) == 0);
  CHECK(warnings.messages ==
        Messages{"Room effect 100 refused: there are 100 slots",
                 "Room effect -1 refused: there are 100 slots"});
}

TEST_CASE("Releasing room effects frees script objects and stops the sound",
          "[field][field-room-effects]") {
  StandardRoom room;
  room.Initialize();
  room.state.sound_id = 7;
  CHECK(fx::ReleaseRoomEffects(room.state, room.host));
  CHECK(room.host.released == std::vector<Storage>{StorageAt(1)});
  CHECK(room.host.sounds == std::vector<SoundCall>{{'s', 7, 0, 0}});
  CHECK(room.state.sound_id == -1);
  CHECK(room.state.script_count == 0);
  CHECK(room.state.instances.at(64).active == 0);
  CHECK(room.state.instances.at(64).objects.empty());
  CHECK_FALSE(fx::ReleaseRoomEffects(room.state, room.host));
}

TEST_CASE("Releasing room effects without a sound bank keeps the sound id",
          "[field][field-room-effects]") {
  StandardRoom room;
  room.Initialize();
  room.host.sound_bank = false;
  room.state.sound_id = 7;
  CHECK(fx::ReleaseRoomEffects(room.state, room.host));
  CHECK(room.host.sounds.empty());
  CHECK(room.state.sound_id == 7);
}

TEST_CASE("Room effect updates run active texture effects and scripts",
          "[field][field-room-effects]") {
  StandardRoom room;
  room.Initialize();
  fx::UpdateRoomEffects(room.state, room.host);
  CHECK(Get<std::uint8_t>(room.host.geometry, StandardRoom::kPolygon + 0x19) ==
        8);
  CHECK(Get<std::uint8_t>(room.host.textures, 0xF) == 1);
  CHECK(room.state.instances.at(64).active == 1);
  CHECK(Get<std::uint8_t>(room.host.textures, StandardRoom::kSwingRecord + 4) ==
        1);
  fx::UpdateRoomEffects(room.state, room.host);
  CHECK(room.state.instances.at(64).active == 0);
  CHECK(room.state.instances.at(64).objects.empty());
  CHECK(room.host.released == std::vector<Storage>{StorageAt(1)});
}

TEST_CASE("Room effect updates skip disabled slots and stop broken records",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  room.Initialize();
  CHECK(fx::ToggleRoomEffectUpdates(room.state, 0));
  CHECK(fx::ToggleRoomEffectUpdates(room.state, 64));
  room.state.instances.at(1).active = 1;
  room.state.instances.at(1).data_offset = 60;
  fx::UpdateRoomEffects(room.state, room.host);
  CHECK(Get<std::uint8_t>(room.host.textures, 0xF) == 0);
  CHECK(room.state.instances.at(64).objects.at(0).cursor ==
        std::optional<std::size_t>{8});
  CHECK(room.state.instances.at(1).active == 0);
  CHECK(warnings.messages ==
        Messages{"Texture effect record at 60 refused: it leaves the 68 byte "
                 "section"});
}

TEST_CASE("Texture effect kinds without a handler stop on their first update",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  FakeHost host;
  fx::RoomEffectState state;
  state.texture_effect_count = 70;
  for (std::size_t index = 0; index < 2; ++index) {
    state.instances.at(index).active = 1;
    state.instances.at(index).update_enabled = 1;
  }
  state.instances.at(0).kind_or_object_count = 4;
  state.instances.at(1).kind_or_object_count = 9;
  fx::UpdateRoomEffects(state, host);
  CHECK(state.instances.at(0).active == 0);
  CHECK(state.instances.at(1).active == 1);
  CHECK(warnings.messages ==
        Messages{"Texture effect count 70 held to the 64 slots"});
}

namespace {
// One geometry group moved by the script in slot 64.
struct ScriptRoom {
  FakeHost host;
  fx::RoomEffectState state;

  explicit ScriptRoom(const Script& script) {
    host.geometry = MakeGeometry(1, 0);
    host.scripts = script.bytes;
    fx::InitializeRoomEffects(state, host);
  }

  fx::RoomEffectObject& Object(std::size_t index) {
    return state.instances.at(64).objects.at(index);
  }

  bool Run() { return fx::ExecuteRoomEffectCommands(state, host, 64); }
};

// A one-object script holding a single raw command.
Script SingleCommandScript(std::int16_t size, std::int16_t opcode) {
  Script script;
  script.Begin({0}, 1);
  script.Raw(size, opcode, {});
  script.End();
  return script;
}

// An object on group 0 of a one-group geometry.
fx::RoomEffectObject GroupObject() {
  fx::RoomEffectObject object;
  object.object_id = 0;
  object.group_offset = Group(0);
  object.cursor = 8;
  return object;
}

// A group placed by SetTransform, given a velocity, then advanced twice.
ScriptRoom TransformThenVelocityRoom() {
  Script script;
  script.Begin({0}, 1);
  script.Add(RoomEffectOpcode::kSetTransform,
             {Low(176), High(176), Low(-48), High(-48), Low(80), High(80), 0x11,
              0, 0x22, 0, 0x33, 0});
  script.Add(RoomEffectOpcode::kVelocity, {12, -12, 24, 0, 0, 0, 0, 0});
  CHECK(script.Add(RoomEffectOpcode::kAdvanceMotion, {2, 2}) == 56);
  script.End();
  return ScriptRoom{script};
}
}  // namespace

TEST_CASE("Effect scripts place a group by its transform",
          "[field][field-room-effects]") {
  ScriptRoom room = TransformThenVelocityRoom();
  CHECK(room.Run());
  CHECK(room.Object(0).cursor == std::optional<std::size_t>{56});
  CHECK(room.Object(0).elapsed == 1);
  CHECK(room.Object(0).velocity == fx::RoomEffectVector{12, -12, 24});
  CHECK(GroupWords(room.host.geometry, 0, 1, 6) ==
        std::vector<std::int32_t>{11, -3, 6, 0x11, 0x22, 0x33});
}

TEST_CASE("Effect scripts carry a placed group on its velocity",
          "[field][field-room-effects]") {
  ScriptRoom room = TransformThenVelocityRoom();
  CHECK(room.Run());
  CHECK(room.Run());
  CHECK_FALSE(room.Object(0).cursor);
  CHECK(GroupWords(room.host.geometry, 0, 1, 3) ==
        std::vector<std::int32_t>{12, -4, 8});
  CHECK_FALSE(room.Run());
}

TEST_CASE("Damped motion keeps rotations in a turn and slows by sixteenths",
          "[field][field-room-effects]") {
  Bytes geometry = MakeGeometry(1, 0);
  fx::RoomEffectObject object = GroupObject();
  object.velocity = {32, -32, 16};
  object.velocity_damping = 4;
  object.rotation = {0xFF8, 0, 0};
  object.angular_velocity = {0x10, 0, 0};
  object.angular_damping = 8;
  CHECK_FALSE(fx::StepRoomEffectDampedMotion(object, geometry));
  CHECK(object.position == fx::RoomEffectPosition{32, -32, 16});
  CHECK(object.rotation == fx::RoomEffectVector{8, 0, 0});
  CHECK(object.velocity == fx::RoomEffectVector{24, -24, 12});
  CHECK(object.angular_velocity == fx::RoomEffectVector{8, 0, 0});
  CHECK(GroupWord(geometry, 0, 2) == -2);
  CHECK(GroupWord(geometry, 0, 4) == 8);
}

TEST_CASE("Gravity accelerates a group down to the floor and bounces it",
          "[field][field-room-effects]") {
  FakeHost host;
  host.geometry = MakeGeometry(1, 0);
  host.floor_code = 4;
  fx::RoomEffectObject object = GroupObject();
  object.position.x = 40;
  object.velocity = {0, 32, 0};
  object.velocity_damping = 2;
  CHECK(fx::StepRoomEffectGravity(object, host));
  CHECK(object.position.y == 32);
  CHECK(object.velocity.y == 48);
  CHECK(GroupWord(host.geometry, 0, 2) == 2);
  CHECK(fx::StepRoomEffectGravity(object, host));
  CHECK(object.position.y == 64);
  CHECK(object.velocity.y == 4);
  CHECK(object.bounces == 1);
  CHECK(host.floor_queries == std::vector<std::pair<int, int>>{{2, 0}, {2, 0}});
}

TEST_CASE("Gravity stops an undamped group and reads the floor's signed height",
          "[field][field-room-effects]") {
  FakeHost host;
  host.geometry = MakeGeometry(1, 0);
  fx::RoomEffectObject undamped = GroupObject();
  undamped.velocity = {0, 32, 0};
  CHECK_FALSE(fx::StepRoomEffectGravity(undamped, host));
  CHECK(undamped.bounces == 0);
  fx::RoomEffectObject high = GroupObject();
  high.position.y = 48;
  high.velocity_damping = 2;
  host.floor_code = 0x8004;
  CHECK(fx::StepRoomEffectGravity(high, host));
  CHECK(high.bounces == 0);
  host.floor_code = 0x7FFE;
  CHECK(fx::StepRoomEffectGravity(high, host));
  CHECK(high.bounces == 1);
  CHECK(high.position.y == -32);
}

TEST_CASE("Radial velocities push away from the centre within their speed",
          "[field][field-room-effects]") {
  fx::RoomEffectObject object;
  object.position = {160, 0, -160};
  CHECK(fx::SetRoomEffectRadialVelocity(
            object,
            MakeCommand(RoomEffectOpcode::kRadialVelocity, {0, 0, 0, 30, 3})) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(object.velocity == fx::RoomEffectVector{5, 7, -5});
  CHECK(object.velocity_damping == 4);
  CHECK(object.angular_velocity == fx::RoomEffectVector{-11, -25, 0});
  CHECK(object.angular_damping == 0);
  CHECK(fx::SetRoomEffectRadialVelocity(
            object,
            MakeCommand(RoomEffectOpcode::kRadialVelocity, {0, 0, 0, 5, 0})) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(object.velocity == fx::RoomEffectVector{0, 1, 0});
}

TEST_CASE("Velocity commands set both vectors and their damping",
          "[field][field-room-effects]") {
  fx::RoomEffectObject object;
  CHECK(fx::SetRoomEffectVelocity(object,
                                  MakeCommand(RoomEffectOpcode::kVelocity,
                                              {-1, 2, -3, 4, 5, -6, 7, 8})) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(object.velocity == fx::RoomEffectVector{-1, 2, -3});
  CHECK(object.velocity_damping == 4);
  CHECK(object.angular_velocity == fx::RoomEffectVector{5, -6, 7});
  CHECK(object.angular_damping == 8);
}

TEST_CASE("Motion commands wait out their duration or until the group stops",
          "[field][field-room-effects]") {
  FakeHost host;
  host.geometry = MakeGeometry(1, 0);
  fx::RoomEffectObject object = GroupObject();
  const auto timed = MakeCommand(RoomEffectOpcode::kAdvanceMotion, {2, 0});
  CHECK(fx::AdvanceRoomEffectMotion(object, timed, host) ==
        RoomEffectCommandResult::kWait);
  CHECK(fx::AdvanceRoomEffectMotion(object, timed, host) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(object.elapsed == 0);
  const auto until_still =
      MakeCommand(RoomEffectOpcode::kAdvanceMotion, {Low(0x8000), 1});
  object.velocity_damping = 2;
  CHECK(fx::AdvanceRoomEffectMotion(object, until_still, host) ==
        RoomEffectCommandResult::kWait);
  object.velocity_damping = 0;
  object.velocity.y = 64;
  CHECK(fx::AdvanceRoomEffectMotion(object, until_still, host) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(object.elapsed == 0);
}

TEST_CASE("Play sound commands start the room sound centred or silent",
          "[field][field-room-effects]") {
  FakeHost host;
  fx::RoomEffectState state;
  CHECK(fx::ExecuteRoomEffectPlaySound(
            MakeCommand(RoomEffectOpcode::kPlaySound, {0, 9}), state, host) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(state.sound_id == 9);
  CHECK(fx::ExecuteRoomEffectPlaySound(
            MakeCommand(RoomEffectOpcode::kPlaySound, {1, 9}), state, host) ==
        RoomEffectCommandResult::kAdvance);
  state.sound_restored = true;
  CHECK(fx::ExecuteRoomEffectPlaySound(
            MakeCommand(RoomEffectOpcode::kPlaySound, {1, 9}), state, host) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(fx::ExecuteRoomEffectStopSound(
            MakeCommand(RoomEffectOpcode::kStopSound, {0, 9}), state, host) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(state.sound_id == -1);
  CHECK(host.sounds ==
        std::vector<SoundCall>{
            {'c', 9, 0, 0}, {'p', 9, 0x80, 0}, {'c', 9, 0, 0}, {'s', 9, 0, 0}});
}

TEST_CASE("Sound commands do nothing in a room without a sound bank",
          "[field][field-room-effects]") {
  FakeHost host;
  host.sound_bank = false;
  fx::RoomEffectState state;
  state.sound_id = 4;
  CHECK(fx::ExecuteRoomEffectPlaySound(
            MakeCommand(RoomEffectOpcode::kPlaySound, {0, 9}), state, host) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(fx::ExecuteRoomEffectStopSound(
            MakeCommand(RoomEffectOpcode::kStopSound, {0, 9}), state, host) ==
        RoomEffectCommandResult::kAdvance);
  fx::DuckRoomEffectSound(state, host, true);
  fx::RestartRoomEffectSound(state, host);
  CHECK(state.sound_id == 4);
  CHECK(host.sounds.empty());
}

TEST_CASE("The room sound ducks restores and restarts while one plays",
          "[field][field-room-effects]") {
  FakeHost host;
  fx::RoomEffectState state;
  state.sound_id = 9;
  fx::DuckRoomEffectSound(state, host, true);
  fx::DuckRoomEffectSound(state, host, false);
  fx::RestoreRoomEffectSoundVolume(state, host);
  fx::RestartRoomEffectSound(state, host);
  CHECK(state.sound_restored);
  CHECK(host.sounds == std::vector<SoundCall>{{'f', 9, 0x20, 8},
                                              {'f', 9, 0x7F, 8},
                                              {'f', 9, 0x7F, 0},
                                              {'c', 9, 0, 0}});
  state.sound_id = -1;
  state.sound_restored = false;
  host.sounds.clear();
  fx::DuckRoomEffectSound(state, host, true);
  fx::RestoreRoomEffectSoundVolume(state, host);
  fx::RestartRoomEffectSound(state, host);
  CHECK(state.sound_restored);
  CHECK(host.sounds.empty());
}

TEST_CASE("Stop script commands release only the script slots that exist",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  room.Initialize();
  for (const std::int16_t index : std::initializer_list<std::int16_t>{36, -1}) {
    CHECK(fx::ExecuteRoomEffectStopScript(
              MakeCommand(RoomEffectOpcode::kStopScript, {index, High(index)}),
              room.state, room.host) == RoomEffectCommandResult::kAdvance);
  }
  CHECK(room.state.instances.at(64).active == 1);
  CHECK(fx::ExecuteRoomEffectStopScript(
            MakeCommand(RoomEffectOpcode::kStopScript, {0, 0}), room.state,
            room.host) == RoomEffectCommandResult::kAdvance);
  CHECK(room.state.instances.at(64).active == 0);
  CHECK(room.state.instances.at(64).objects.empty());
  CHECK(room.host.released == std::vector<Storage>{StorageAt(1)});
  CHECK(warnings.messages ==
        Messages{"Stopping effect script 36 refused: it is not a script slot",
                 "Stopping effect script -1 refused: it is not a script slot"});
}

TEST_CASE("Script commands start effects and show or hide groups",
          "[field][field-room-effects]") {
  StandardRoom room;
  room.Initialize();
  auto& geometry = room.host.geometry;
  CHECK(fx::ExecuteRoomEffectStartEffect(
            MakeCommand(RoomEffectOpcode::kStartEffect, {1, 0}), room.state,
            room.host) == RoomEffectCommandResult::kAdvance);
  CHECK(room.state.instances.at(1).active == 1);
  SetGroupWord(geometry, 1, 0, 0x2101);
  CHECK(fx::ExecuteRoomEffectShowGroup(
            MakeCommand(RoomEffectOpcode::kShowGroup, {1, 0}), geometry) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(GroupWord(geometry, 1, 0) == 0x2001);
  CHECK(fx::ExecuteRoomEffectHideGroup(
            MakeCommand(RoomEffectOpcode::kHideGroup, {1, 0}), geometry) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(GroupWord(geometry, 1, 0) == 0x2101);
}

TEST_CASE("Hiding a script's object overwrites its group flags",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  room.Initialize();
  auto& geometry = room.host.geometry;
  SetGroupWord(geometry, 0, 0, 0x2001);
  const auto& objects = room.state.instances.at(64).objects;
  CHECK(fx::HideRoomEffectObject(objects.at(0), geometry) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(GroupWord(geometry, 0, 0) == 0x100);
  const Bytes before = geometry;
  CHECK(fx::HideRoomEffectObject(objects.at(1), geometry) ==
        RoomEffectCommandResult::kAdvance);
  fx::RoomEffectObject outside = objects.at(0);
  outside.object_id = 3;
  outside.group_offset = 200;
  CHECK(fx::HideRoomEffectObject(outside, geometry) ==
        RoomEffectCommandResult::kAdvance);
  CHECK(geometry == before);
  CHECK(warnings.messages ==
        Messages{"Effect object 3 refused: its group at 200 leaves the 172 "
                 "byte geometry"});
}

TEST_CASE("A script that restarts its own slot ends the pass on new objects",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  Script script;
  script.Begin({0}, 1);
  script.Add(RoomEffectOpcode::kStartEffect, {64, 0});
  script.End();
  ScriptRoom room{script};
  CHECK(room.Run());
  CHECK(room.Object(0).cursor == std::optional<std::size_t>{8});
  CHECK(room.host.reserved == std::vector<std::size_t>{52, 52});
  CHECK(room.host.released == std::vector<Storage>{StorageAt(1)});
  CHECK(room.state.instances.at(64).storage == StorageAt(2));
  CHECK(warnings.messages ==
        Messages{"Effect script slot 64 rebuilt its own objects; ending the "
                 "pass"});
}

TEST_CASE("Fast-forwarded scripts run waits to their end and stop at jumps",
          "[field][field-room-effects]") {
  Script script;
  script.Begin({0}, 1);
  script.Add(RoomEffectOpcode::kAdvanceMotion, {3, 0});
  script.Jump(-8);
  script.End();
  ScriptRoom room{script};
  CHECK(room.Run());
  CHECK(room.Object(0).cursor == std::optional<std::size_t>{8});
  CHECK(room.Object(0).elapsed == 1);
  room.host.fast_forward = true;
  CHECK(room.Run());
  CHECK(room.Object(0).cursor == std::optional<std::size_t>{16});
  CHECK(room.Object(0).elapsed == 0);
  room.host.fast_forward = false;
  CHECK(room.Run());
  CHECK(room.Object(0).cursor == std::optional<std::size_t>{8});
  CHECK(room.Object(0).elapsed == 1);
}

TEST_CASE("A script that loops without waiting is ended at the handler cap",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  Script script;
  script.Begin({0}, 1);
  script.Add(RoomEffectOpcode::kSkip, {});
  script.Jump(-4);
  script.End();
  ScriptRoom room{script};
  CHECK(room.Run());
  CHECK_FALSE(room.Object(0).cursor);
  CHECK_FALSE(room.Run());
  CHECK(warnings.messages ==
        Messages{"Effect script slot 64 object 0 ended: 1048576 handler calls "
                 "without waiting"});
}

TEST_CASE(
    "Jumps before the script section and slots past the table are refused",
    "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  Script script;
  script.Begin({0}, 1);
  script.Jump(-100);
  script.End();
  ScriptRoom room{script};
  CHECK(room.Run());
  CHECK_FALSE(room.Object(0).cursor);
  CHECK_FALSE(fx::ExecuteRoomEffectCommands(room.state, room.host, 100));
  CHECK(warnings.messages ==
        Messages{"Effect script slot 64 object 0 ended: the command at 8 jumps "
                 "before the section",
                 "Effect script slot 100 refused: there are 100"});
}

TEST_CASE("Malformed script commands end their object with a report",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  for (const std::int16_t opcode :
       std::initializer_list<std::int16_t>{1, -1, 13}) {
    ScriptRoom room{SingleCommandScript(4, opcode)};
    CHECK(room.Run());
    CHECK_FALSE(room.Object(0).cursor);
  }
  ScriptRoom past_end{SingleCommandScript(4, 2)};
  past_end.Object(0).cursor = 14;
  CHECK(past_end.Run());
  CHECK_FALSE(past_end.Object(0).cursor);
  CHECK(warnings.messages ==
        Messages{"Effect script slot 64 object 0 ended: command at 8 (size 4, "
                 "opcode 1) refused",
                 "Effect script slot 64 object 0 ended: command at 8 (size 4, "
                 "opcode -1) refused",
                 "Effect script slot 64 object 0 ended: command at 14 (size 0, "
                 "opcode 2) refused"});
}

namespace {
// The swing record at 24 on group 1: rest 0x40, start 0x10, initial speed
// 50 and acceleration 2 on the given axis.
void SetUpSwing(StandardRoom& room, std::uint8_t axis) {
  constexpr std::size_t kSwing = StandardRoom::kSwingRecord;
  Bytes& textures = room.host.textures;
  Put<std::uint8_t>(textures, kSwing + 7, axis);
  Put<std::int32_t>(textures, kSwing + 0xC, 2);
  Put<std::int32_t>(textures, kSwing + 0x18, 0x40);
  Put<std::int32_t>(textures, kSwing + 0x1C, 0x10);
  Put<std::int32_t>(textures, kSwing + 0x20, 50);
}

fx::SwingEffect SwingRecord(const StandardRoom& room) {
  const std::span<const std::byte> textures = room.host.textures;
  return layout::DecodeSwingEffect(textures.subspan(StandardRoom::kSwingRecord)
                                       .first<layout::kSwingEffectBytes>());
}

// The record SetUpSwing describes once a rotation has finished.
fx::SwingEffect ReversedSwing(std::uint8_t axis) {
  fx::SwingEffect swing;
  swing.geometry_offset = static_cast<std::int32_t>(Group(1));
  swing.needs_reset = 1;
  swing.axis = axis;
  swing.angular_velocity = -50;
  swing.acceleration = -2;
  swing.rest_angle = 0x10;
  swing.start_angle = 0x40;
  swing.initial_velocity = -50;
  return swing;
}
}  // namespace

TEST_CASE("Finishing a door rotation rests the group and reverses the swing",
          "[field][field-room-effects]") {
  StandardRoom room;
  SetUpSwing(room, 2);
  room.Initialize();
  const Bytes geometry = room.host.geometry;
  const Bytes textures = room.host.textures;
  fx::FinishRoomDoorRotation(room.state, room.host, 1);
  CHECK(room.host.geometry == geometry);
  CHECK(room.host.textures == textures);
  CHECK(fx::StartRoomEffect(room.state, room.host, 1));
  fx::FinishRoomDoorRotation(room.state, room.host, 1);
  CHECK(GroupWord(room.host.geometry, 1, 5) == 0x40);
  CHECK(GroupWord(room.host.geometry, 1, 0) == 0x6000);
  CHECK(SwingRecord(room) == ReversedSwing(2));
  CHECK(room.state.instances.at(1).active == 0);
}

TEST_CASE("Door rotations act only on active swings in the texture slots",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  SetUpSwing(room, 2);
  room.Initialize();
  room.state.instances.at(5).kind_or_object_count = 6;
  room.state.instances.at(5).active = 1;
  const Bytes textures = room.host.textures;
  for (const int index : {0, 64, -1, 5}) {
    fx::FinishRoomDoorRotation(room.state, room.host, index);
  }
  CHECK(room.host.textures == textures);
  CHECK(room.state.instances.at(5).active == 1);
  CHECK(warnings.messages ==
        Messages{"Door rotation -1 refused: it is negative",
                 "Door rotation 5 refused: the room has 2 texture effects"});
}

TEST_CASE("Finishing a tilt rotation rests the group on the swing's axis",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  StandardRoom room;
  SetUpSwing(room, 2);
  room.Initialize();
  fx::FinishRoomTiltRotation(room.state, room.host, 1);
  CHECK(GroupWord(room.host.geometry, 1, 4) == 0x40);
  CHECK(GroupWord(room.host.geometry, 1, 0) == 0);
  CHECK(SwingRecord(room) == ReversedSwing(2));
  Put<std::uint8_t>(room.host.textures, StandardRoom::kSwingRecord + 7, 0);
  fx::FinishRoomTiltRotation(room.state, room.host, 1);
  CHECK(GroupWord(room.host.geometry, 1, 6) == 0x10);
  fx::FinishRoomTiltRotation(room.state, room.host, 0);
  fx::FinishRoomTiltRotation(room.state, room.host, 2);
  CHECK(warnings.messages ==
        Messages{"Tilt rotation 0 refused: it is a kind 0 effect",
                 "Tilt rotation 2 refused: the room has 2 texture effects"});
}

TEST_CASE("The room entry point runs only code past the section E header",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  FakeHost host;
  host.entry = Bytes(0x14);
  CHECK_FALSE(fx::CallRoomEntryPoint(host));
  CHECK(host.entry_code_bytes == 0);
  host.entry = Bytes(0x18);
  CHECK(fx::CallRoomEntryPoint(host));
  CHECK(host.entry_code_bytes == 4);
  host.entry_result = false;
  CHECK_FALSE(fx::CallRoomEntryPoint(host));
  CHECK(warnings.messages ==
        Messages{"Room entry point refused: section E has 20 bytes and no "
                 "code"});
}

TEST_CASE("Script command headers decode to an end or a refusal",
          "[field][field-room-effects]") {
  using layout::CommandStatus;
  CHECK(layout::DecodeCommand({}, 0).status ==
        CommandStatus::kHeaderOutOfRange);
  Script script;
  script.Raw(0, 1, {});
  script.Raw(4, 13, {});
  script.Raw(4, -1, {});
  script.Raw(4, 1, {});
  const std::array expected{CommandStatus::kEnd, CommandStatus::kEnd,
                            CommandStatus::kNegativeOpcode,
                            CommandStatus::kTruncated};
  for (std::size_t index = 0; index < expected.size(); ++index) {
    CAPTURE(index);
    CHECK(layout::DecodeCommand(script.bytes, index * 4).status ==
          expected.at(index));
  }
  CHECK(layout::DecodeCommand(script.bytes, 4).command.size == 4);
}

TEST_CASE("Command accessors read zero past the operands their opcode reads",
          "[field][field-room-effects]") {
  const auto skip = MakeCommand(RoomEffectOpcode::kSkip, {7});
  CHECK(skip.size == 6);
  CHECK(skip.Half(4) == 0);
  const auto velocity =
      MakeCommand(RoomEffectOpcode::kVelocity, {1, 2, 3, 4, 5, 6, 7, -8});
  CHECK(velocity.Word(4) == 0x00020001);
  CHECK(velocity.SignedHalf(0x12) == -8);
  CHECK(velocity.Half(0x12) == 0xFFF8);
  CHECK(velocity.Half(27) == 0);
  CHECK(velocity.Word(25) == 0);
}

TEST_CASE("Room effect arithmetic wraps and divides as the R3000 does",
          "[field][field-room-effects]") {
  const auto by_zero = layout::DivideUnsigned(5, 0);
  CHECK(by_zero.quotient == 0xFFFFFFFFU);
  CHECK(by_zero.remainder == 5);
  const auto halves = layout::DivideUnsigned(7, 2);
  CHECK(halves.quotient == 3);
  CHECK(halves.remainder == 1);
  CHECK(layout::WrappingAbs(INT_MIN) == INT_MIN);
  CHECK(layout::WrappingAbs(-5) == 5);
  CHECK(layout::WrappingAdd(INT_MAX, 1) == INT_MIN);
  CHECK(layout::WrappingNegate(INT_MIN) == INT_MIN);
}

TEST_CASE("Script preambles pad their group ids and commands have set extents",
          "[field][field-room-effects]") {
  CHECK(layout::ScriptPreambleBytes(0) == 4);
  CHECK(layout::ScriptPreambleBytes(1) == 8);
  CHECK(layout::ScriptPreambleBytes(4) == 8);
  CHECK(layout::ScriptPreambleBytes(5) == 12);
  CHECK(layout::CommandExtent(RoomEffectOpcode::kRadialVelocity) == 14);
  CHECK(layout::CommandExtent(RoomEffectOpcode::kVelocity) == 20);
  CHECK(layout::CommandExtent(RoomEffectOpcode::kSetTransform) == 26);
  CHECK(layout::CommandExtent(RoomEffectOpcode::kStopScript) == 8);
  CHECK(layout::CommandExtent(RoomEffectOpcode::kJump) == 4);
  CHECK_FALSE(fx::SkipRoomEffectCommand());
}

namespace {
// Bytes 0x80, 0x81, ... with the listed bytes cleared.
template <std::size_t Size>
std::array<std::byte, Size> Pattern(
    std::initializer_list<std::size_t> cleared) {
  std::array<std::byte, Size> bytes{};
  for (std::size_t index = 0; index < Size; ++index) {
    bytes.at(index) = static_cast<std::byte>(0x80U + index);
  }
  for (const std::size_t index : cleared) {
    bytes.at(index) = std::byte{0};
  }
  return bytes;
}

std::int32_t Signed(std::uint32_t value) {
  return static_cast<std::int32_t>(value);
}

std::int16_t SignedShort(std::uint16_t value) {
  return static_cast<std::int16_t>(value);
}
}  // namespace

TEST_CASE("Texture frame records round-trip through their codec",
          "[field][field-room-effects]") {
  constexpr std::size_t kBytes = layout::kTextureFrameEffectBytes;
  const auto frame = layout::DecodeTextureFrameEffect(Pattern<kBytes>({}));
  CHECK(frame.geometry_offset == Signed(0x83828180U));
  CHECK(frame.delay == 0x87);
  CHECK(frame.frame_height == 0x8D);
  CHECK(frame.frame == 0x8F);
  CHECK(frame.v == Corners{0x94, 0x95, 0x96, 0x97});
  std::array<std::byte, kBytes> encoded{};
  layout::EncodeTextureFrameEffect(frame, encoded);
  CHECK(encoded == Pattern<kBytes>({4, 5, 6}));
}

TEST_CASE("Texture scroll records round-trip through their codec",
          "[field][field-room-effects]") {
  constexpr std::size_t kBytes = layout::kTextureScrollEffectBytes;
  const auto scroll = layout::DecodeTextureScrollEffect(Pattern<kBytes>({}));
  CHECK(scroll.window_height == 0x8F);
  CHECK(scroll.step_u == -112);
  CHECK(scroll.step_v == -111);
  CHECK(scroll.tick == 0x92);
  CHECK(scroll.u == Corners{0x94, 0x95, 0x96, 0x97});
  std::array<std::byte, kBytes> encoded{};
  layout::EncodeTextureScrollEffect(scroll, encoded);
  CHECK(encoded == Pattern<kBytes>({4, 5, 6, 0x13}));
}

TEST_CASE("Texture image records round-trip through their codec",
          "[field][field-room-effects]") {
  constexpr std::size_t kBytes = layout::kTextureImageEffectBytes;
  const auto image = layout::DecodeTextureImageEffect(Pattern<kBytes>({}));
  CHECK(image.tick == 0x80);
  CHECK(image.columns == 0x83);
  CHECK(image.delay == 0x87);
  CHECK(image.scroll_step == Signed(0x8B8A8988U));
  CHECK(image.rect ==
        fx::RoomImageRect{SignedShort(0x8D8C), SignedShort(0x8F8E),
                          SignedShort(0x9190), SignedShort(0x9392)});
  std::array<std::byte, kBytes> encoded{};
  layout::EncodeTextureImageEffect(image, encoded);
  CHECK(encoded == Pattern<kBytes>({4, 5, 6}));
}

TEST_CASE("Swing records round-trip through their codec",
          "[field][field-room-effects]") {
  constexpr std::size_t kBytes = layout::kSwingEffectBytes;
  const auto swing = layout::DecodeSwingEffect(Pattern<kBytes>({}));
  CHECK(swing.needs_reset == 0x84);
  CHECK(swing.axis == 0x87);
  CHECK(swing.damping_shift == Signed(0x93929190U));
  CHECK(swing.rest_angle == Signed(0x9B9A9998U));
  CHECK(swing.initial_velocity == Signed(0xA3A2A1A0U));
  std::array<std::byte, kBytes> encoded{};
  layout::EncodeSwingEffect(swing, encoded);
  CHECK(encoded == Pattern<kBytes>({5, 6}));
}

namespace {
std::uint32_t Length(const Bytes& bytes) {
  return static_cast<std::uint32_t>(bytes.size());
}

// Points the loaded room at a StandardRoom's sections over a scratch game
// heap, and restores the room and the live effect state afterwards.
struct LiveRoom {
  Field_RoomRecord saved_room = Field_roomData;
  fx::RoomEffectState saved_state = fx::LiveRoomEffectState();
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 64> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  StandardRoom room;
  Bytes section_e;

  LiveRoom() {
    fx::LiveRoomEffectState() = fx::RoomEffectState{};
    Field_roomData = Field_RoomRecord{};
    Field_roomData.geometry = reinterpret_cast<int*>(room.host.geometry.data());
    Field_roomData.header.geometryLength = Length(room.host.geometry);
    Field_roomData.textureEffects = room.host.textures.data();
    Field_roomData.header.textureEffectsLength = Length(room.host.textures);
    Field_roomData.effectScripts = room.host.scripts.data();
    Field_roomData.header.effectScriptsLength = Length(room.host.scripts);
  }
  LiveRoom(const LiveRoom&) = delete;
  LiveRoom& operator=(const LiveRoom&) = delete;
  LiveRoom(LiveRoom&&) = delete;
  LiveRoom& operator=(LiveRoom&&) = delete;
  ~LiveRoom() {
    Field_roomData = saved_room;
    fx::LiveRoomEffectState() = std::move(saved_state);
  }

  void SetSectionE(std::size_t bytes) {
    section_e.assign(bytes, std::byte{0});
    Field_roomData.sectionE =
        reinterpret_cast<Field_MpdSectionE*>(section_e.data());
    Field_roomData.header.sectionELength = Length(section_e);
  }
};
}  // namespace

TEST_CASE("Live room effects set up over the loaded room's sections and heap",
          "[field][field-room-effects]") {
  LiveRoom live;
  fx::LiveInitializeRoomEffects();
  const fx::RoomEffectState& state = fx::LiveRoomEffectState();
  CHECK(state.texture_effect_count == 2);
  CHECK(state.script_count == 2);
  CHECK(VS::Core::Memory::GameHeap().AllocatedBlocks().size() == 1);
  CHECK(fx::LiveFindRoomGeometryGroup(1) ==
        Field_roomData.geometry + (Group(1) / sizeof(int)));
  CHECK(fx::LiveHideRoomGeometryGroup(1) == 1);
  CHECK(GroupWord(live.room.host.geometry, 1, 0) == 0x100);
  CHECK(fx::LiveReleaseRoomEffects() == 1);
}

TEST_CASE("Live room effects update the loaded room and free their objects",
          "[field][field-room-effects]") {
  LiveRoom live;
  fx::LiveInitializeRoomEffects();
  fx::LiveUpdateRoomEffects();
  CHECK(Get<std::uint8_t>(live.room.host.geometry,
                          StandardRoom::kPolygon + 0x19) == 8);
  CHECK(fx::LiveIsRoomEffectActive(0) == 1);
  CHECK(fx::LiveReleaseRoomEffects() == 1);
  CHECK(VS::Core::Memory::GameHeap().AllocatedBlocks().empty());
}

TEST_CASE("Live room effects refuse empty sections and unbound entry points",
          "[field][field-room-effects]") {
  ChannelWarnings warnings{kChannel};
  LiveRoom live;
  CHECK(fx::LiveCallRoomEntryPoint() == 0);
  live.SetSectionE(0x18);
  CHECK(fx::LiveCallRoomEntryPoint() == 0);
  Field_roomData.header.geometryLength = 0xFFFFFFFFU;
  CHECK(fx::LiveFindRoomGeometryGroup(0) == nullptr);
  Field_roomData.header.geometryLength = Length(live.room.host.geometry);
  Field_roomData.geometry = nullptr;
  CHECK(fx::LiveShowRoomGeometryGroup(0) == 0);
  CHECK(warnings.messages ==
        Messages{"Room entry point refused: section E has 0 bytes and no code",
                 "Room entry point refused: no binding for its 4 bytes (FNV-1a "
                 "4B95F515)"});
}

TEST_CASE("The room effect module symbols are retired",
          "[field][field-room-effects]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  constexpr std::array<std::uint32_t, 48> kRetired{
      0x8009195CU, 0x80091998U, 0x800919D8U, 0x80091A1CU, 0x80091A78U,
      0x80091AC0U, 0x80091B04U, 0x80091C6CU, 0x80091E10U, 0x80091FE8U,
      0x800923F8U, 0x80092540U, 0x80092548U, 0x800927ACU, 0x80092914U,
      0x8009291CU, 0x80092A18U, 0x80092B04U, 0x80092C68U, 0x80092E14U,
      0x80092E7CU, 0x80092EDCU, 0x80092F6CU, 0x80092F74U, 0x80092F98U,
      0x80092FBCU, 0x8009306CU, 0x80093364U, 0x8009352CU, 0x800935B8U,
      0x800935F8U, 0x8009361CU, 0x80093640U, 0x800936F0U, 0x800936F8U,
      0x80093764U, 0x80093788U, 0x80093824U, 0x800938ACU, 0x80093914U,
      0x80093A14U, 0x80093A70U, 0x80098B1CU, 0x800E85CCU, 0x800E85E8U,
      0x800F1D00U, 0x800F1DD8U, 0x800F2258U};
  for (const std::uint32_t address : kRetired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}
