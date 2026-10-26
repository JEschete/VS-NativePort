#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Render/RoomDraw.h"
#include "Render/RoomGeometry.h"
}

namespace {
using PackedCoordinates = std::array<unsigned char, 12>;
using RoomVertices = std::array<Render_RoomVertex, 3>;
using NativeDecode = void (*)(const void*, std::size_t, unsigned,
                              Render_RoomVertex*);

PackedCoordinates Coordinates() {
  PackedCoordinates data{};
  constexpr std::array<std::int16_t, 3> kBase{32760, -32760, -123};
  std::memcpy(data.data(), kBase.data(), sizeof kBase);
  return data;
}

RoomVertices Vertices() {
  RoomVertices vertices;
  std::memset(vertices.data(), 0x5A, sizeof vertices);
  return vertices;
}

std::array<std::int16_t, 3> Position(const Render_RoomVertex& vertex) {
  return {vertex.position.vx, vertex.position.vy, vertex.position.vz};
}

std::array<std::int16_t, 3> ExpectedPosition(const PackedCoordinates& data,
                                             unsigned shift,
                                             std::size_t vertex) {
  std::array<std::int16_t, 3> expected{};
  const auto scale = std::int64_t{1} << (shift % 32);
  for (std::size_t axis = 0; axis < expected.size(); ++axis) {
    const auto word = static_cast<std::uint16_t>(
        data[axis * 2] | (unsigned{data[axis * 2 + 1]} << 8));
    const auto base = std::bit_cast<std::int16_t>(word);
    int delta = 0;
    if (vertex != 0) {
      const int value = data[6 + (vertex - 1) * 3 + axis];
      delta = value < 128 ? value : value - 256;
    }
    const auto result = static_cast<std::uint16_t>(base + delta * scale);
    expected[axis] = std::bit_cast<std::int16_t>(result);
  }
  return expected;
}

void CheckCoordinates(const PackedCoordinates& data, unsigned shift) {
  auto vertices = Vertices();
  const auto before = vertices;
  Render_DecodeRoomPolygonVertices(data.data(), data.size(), shift,
                                   vertices.data());
  for (std::size_t vertex = 0; vertex < vertices.size(); ++vertex) {
    CHECK(Position(vertices[vertex]) == ExpectedPosition(data, shift, vertex));
    CHECK(vertices[vertex].position.pad == before[vertex].position.pad);
    CHECK(vertices[vertex].color == before[vertex].color);
    CHECK(vertices[vertex].uv == before[vertex].uv);
  }
}

void CheckDelta(unsigned shift, unsigned byte) {
  for (std::size_t vertex = 1; vertex < 3; ++vertex) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
      auto data = Coordinates();
      data[6 + (vertex - 1) * 3 + axis] = static_cast<unsigned char>(byte);
      CheckCoordinates(data, shift);
    }
  }
}

void CheckAliasedCoordinates(unsigned shift, std::size_t offset) {
  auto vertices = Vertices();
  auto data = Coordinates();
  data[6] = 127;
  data[7] = 128;
  data[8] = 1;
  data[9] = 255;
  data[10] = 3;
  data[11] = 128;
  auto* bytes = reinterpret_cast<unsigned char*>(vertices.data());
  std::memcpy(bytes + offset, data.data(), data.size());
  RoomVertices expected;
  std::memcpy(expected.data(), vertices.data(), sizeof vertices);
  for (std::size_t vertex = 0; vertex < vertices.size(); ++vertex) {
    const auto position = ExpectedPosition(data, shift, vertex);
    expected[vertex].position.vx = position[0];
    expected[vertex].position.vy = position[1];
    expected[vertex].position.vz = position[2];
  }
  Render_DecodeRoomPolygonVertices(bytes + offset, data.size(), shift,
                                   vertices.data());
  std::array<unsigned char, sizeof(RoomVertices)> actual_bytes{};
  std::array<unsigned char, sizeof(RoomVertices)> expected_bytes{};
  std::memcpy(actual_bytes.data(), vertices.data(), actual_bytes.size());
  std::memcpy(expected_bytes.data(), expected.data(), expected_bytes.size());
  CHECK(actual_bytes == expected_bytes);
}
}  // namespace

TEST_CASE("Room vertex decoder exposes bounded bytes and typed native outputs",
          "[render][room-decode]") {
  CHECK((std::is_same_v<decltype(&Render_DecodeRoomPolygonVertices),
                        NativeDecode>));
}

TEST_CASE(
    "Packed room decoding covers every signed delta and masked word shift",
    "[render][room-decode]") {
  for (unsigned shift = 0; shift < 32; ++shift) {
    for (unsigned byte = 0; byte < 256; ++byte) CheckDelta(shift, byte);
  }
}

TEST_CASE("Room coordinate shift ignores high bits of the original register",
          "[render][room-decode]") {
  auto data = Coordinates();
  data[6] = 127;
  data[7] = 128;
  data[8] = 1;
  data[9] = 255;
  data[10] = 3;
  data[11] = 128;
  for (unsigned shift : {32u, 35u, 63u, 0xA5A50003u, UINT32_MAX}) {
    CheckCoordinates(data, shift);
  }
}

TEST_CASE("Room decoder captures packed words before any aliased output write",
          "[render][room-decode]") {
  for (unsigned shift : {0u, 3u}) {
    for (std::size_t offset = 0; offset <= sizeof(RoomVertices) - 12;
         ++offset) {
      CheckAliasedCoordinates(shift, offset);
    }
  }
}

TEST_CASE(
    "Room decoder handles unaligned bytes and preserves a fourth native corner",
    "[render][room-decode]") {
  auto data = Coordinates();
  data[6] = 127;
  data[10] = 127;
  std::array<unsigned char, 16> unaligned{};
  std::memcpy(unaligned.data() + 1, data.data(), data.size());
  std::array<Render_RoomVertex, 4> vertices;
  std::memset(vertices.data(), 0x5A, sizeof vertices);
  std::array<unsigned char, sizeof(Render_RoomVertex)> last{};
  std::memcpy(last.data(), &vertices.back(), last.size());
  Render_DecodeRoomPolygonVertices(unaligned.data() + 1, data.size(), 0,
                                   vertices.data());
  CHECK(Position(vertices[0]) == ExpectedPosition(data, 0, 0));
  CHECK(Position(vertices[1]) == ExpectedPosition(data, 0, 1));
  CHECK(Position(vertices[2]) == ExpectedPosition(data, 0, 2));
  std::array<unsigned char, sizeof(Render_RoomVertex)> actual_last{};
  std::memcpy(actual_last.data(), &vertices.back(), actual_last.size());
  CHECK(actual_last == last);
}

TEST_CASE(
    "Original packed room decoder resolves to its typed native implementation",
    "[render][room-decode][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* entry = VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                                  0x8009797Cu);
  REQUIRE(entry != nullptr);
  const auto decode = reinterpret_cast<NativeDecode>(entry);
  CHECK(decode == Render_DecodeRoomPolygonVertices);
  auto data = Coordinates();
  auto vertices = Vertices();
  decode(data.data(), data.size(), 3, vertices.data());
  CHECK(Position(vertices[0]) == ExpectedPosition(data, 3, 0));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8009797Du) == nullptr);
}
