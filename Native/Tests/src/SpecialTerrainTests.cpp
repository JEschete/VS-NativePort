#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"
#include "Platform/GameFiber.hpp"

extern "C" {
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

static_assert(
    std::is_same_v<decltype(&Battle_Ai_BuildAiTerrainHeightsWithWorkspace),
                   void (*)(void)>);

namespace {
struct SpecialTerrainFixture {
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  decltype(Battle_Ai_specialTerrainHeights) saved_heights =
      Battle_Ai_specialTerrainHeights;
  decltype(std::to_array(Battle_Ai_directionDeltas)) saved_directions =
      std::to_array(Battle_Ai_directionDeltas);
  Field_RoomRecord saved_room = Field_roomData;
  u_short flags[32][32]{};
  u_short heights[32][32]{};
  alignas(Field_MpdCollisionHeader) std::array<unsigned char, 4120> collision{};

  SpecialTerrainFixture() {
    Battle_Ai_tileFlagMap = flags;
    Battle_Ai_specialTerrainHeights = heights;
    Field_roomData = {};
    const Field_MpdCollisionHeader header{32, 32, 16, 1024};
    std::memcpy(collision.data(), &header, sizeof header);
    Field_roomData.collisionMap =
        reinterpret_cast<Field_MpdCollisionHeader*>(collision.data());
    Field_roomData.header.collisionMapLength = collision.size();
    constexpr std::array<int, 8> kX{0, 1, 1, 1, 0, -1, -1, -1};
    constexpr std::array<int, 8> kZ{-1, -1, 0, 1, 1, 1, 0, -1};
    for (unsigned index = 0; index < 8; ++index) {
      Battle_Ai_directionDeltas[index].dx = static_cast<signed char>(kX[index]);
      Battle_Ai_directionDeltas[index].dz = static_cast<signed char>(kZ[index]);
    }
    for (unsigned z = 0; z < 32; ++z) {
      for (unsigned x = 0; x < 32; ++x) {
        heights[z][x] = static_cast<u_short>(0x8800 | (z * 32 + x));
      }
    }
  }

  ~SpecialTerrainFixture() {
    Battle_Ai_tileFlagMap = saved_flags;
    Battle_Ai_specialTerrainHeights = saved_heights;
    std::copy(saved_directions.begin(), saved_directions.end(),
              Battle_Ai_directionDeltas);
    Field_roomData = saved_room;
  }
};
}  // namespace

TEST_CASE(
    "Room-entry terrain envelope invokes the native special-height builder",
    "[ai][special-terrain]") {
  SpecialTerrainFixture fixture;
  fixture.flags[1][1] = fixture.flags[1][2] = fixture.flags[2][2] = 0x20;
  Battle_Ai_BuildAiTerrainHeightsWithWorkspace();
  CHECK(fixture.heights[1][1] == 0xA802);
  CHECK(fixture.heights[1][2] == 0xA802);
  CHECK(fixture.heights[2][2] == 0xA802);
  CHECK(fixture.heights[0][0] == 0x8800);
  CHECK(fixture.flags[1][1] == 0x20);
}

TEST_CASE(
    "Special terrain rebuild preserves visited cells and unrelated height "
    "flags",
    "[ai][special-terrain]") {
  SpecialTerrainFixture fixture;
  fixture.flags[1][1] = fixture.flags[1][2] = 0x20;
  fixture.heights[1][1] = 0xE955;
  Battle_Ai_BuildAiTerrainHeightsWithWorkspace();
  CHECK(fixture.heights[1][1] == 0xE955);
  CHECK(fixture.heights[1][2] == 0xA802);
  const auto before = fixture.heights[1][2];
  Battle_Ai_BuildAiTerrainHeightsWithWorkspace();
  CHECK(fixture.heights[1][2] == before);
}

TEST_CASE("The complete bounded special region builds on the native game fiber",
          "[ai][special-terrain]") {
  SpecialTerrainFixture fixture;
  for (auto& row : fixture.flags) std::fill_n(row, 32, 0x20);
  fixture.flags[0][0] = 0;
  VS::Platform::GameFiber fiber(
      +[] { Battle_Ai_BuildAiTerrainHeightsWithWorkspace(); });
  CHECK(fiber.Resume() == false);
  CHECK(fiber.Finished());
  CHECK(fixture.heights[0][0] == 0x8800);
  for (unsigned z = 0; z < 32; ++z) {
    for (unsigned x = 0; x < 32; ++x) {
      if (x != 0 || z != 0) CHECK(fixture.heights[z][x] == 0xA802);
    }
  }
}

TEST_CASE("Native terrain-build envelope retains its original typed entry",
          "[ai][special-terrain][redirect-binding]") {
  SpecialTerrainFixture fixture;
  fixture.flags[1][1] = 0x20;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* address = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x800E6B4Cu);
  REQUIRE(address != nullptr);
  const auto build = reinterpret_cast<void (*)(void)>(address);
  CHECK(build == Battle_Ai_BuildAiTerrainHeightsWithWorkspace);
  build();
  CHECK(fixture.heights[1][1] == 0xA802);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E6B4Du) == nullptr);
}

TEST_CASE("A special region without eligible adjacent ground stays unseeded",
          "[ai][special-terrain]") {
  SpecialTerrainFixture fixture;
  for (auto& row : fixture.flags) std::fill_n(row, 32, 0x20);
  std::array<unsigned char, sizeof fixture.heights> before{};
  std::memcpy(before.data(), fixture.heights, before.size());
  Battle_Ai_BuildAiTerrainHeightsWithWorkspace();
  CHECK(std::memcmp(before.data(), fixture.heights, before.size()) == 0);
  Battle_Ai_BuildAiTerrainHeightsWithWorkspace();
  CHECK(std::memcmp(before.data(), fixture.heights, before.size()) == 0);
}
