// Hand-maintained tests for the typed collision height sampler (#247).
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Field/CollisionSampling.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

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
  auto* saved_map = Field_roomData.collisionMap;
  const auto saved_length = Field_roomData.header.collisionMapLength;
  Field_roomData.collisionMap = nullptr;
  CHECK(VS::Field::LiveRoomCollisionMap().empty());
  Field_roomData.collisionMap =
      reinterpret_cast<Field_MpdCollisionHeader*>(map.bytes.data());
  Field_roomData.header.collisionMapLength = sizeof map.bytes;
  const auto live = VS::Field::LiveRoomCollisionMap();
  CHECK(live.data() == map.bytes.data());
  CHECK(live.size() == map.bytes.size());
  Field_roomData.collisionMap = saved_map;
  Field_roomData.header.collisionMapLength = saved_length;
}

TEST_CASE("The promoted collision sampler has no original module address",
          "[collision][room][collision-sampling]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80099514u) == nullptr);
}
