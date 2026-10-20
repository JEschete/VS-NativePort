#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Field/TerrainHeight.h"
}

namespace {
struct TerrainFixture {
  Battle_Ai_FrameContext saved = *Battle_Ai_FrameState();
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  u_short heights[kAiFrameGridSide][kAiFrameGridSide]{};
  u_short flags[kAiFrameGridSide][kAiFrameGridSide]{};

  TerrainFixture() {
    auto* frame = Battle_Ai_FrameState();
    frame->heightMap = heights;
    frame->tileFlags = flags;
    const std::array<u_char, 8> directions{0x30, 0x33, 0x03, 0x13,
                                           0x10, 0x11, 0x01, 0x31};
    std::copy(directions.begin(), directions.end(), frame->directions);
    Battle_Ai_tileFlagMap = flags;
  }

  ~TerrainFixture() {
    *Battle_Ai_FrameState() = saved;
    Battle_Ai_tileFlagMap = saved_flags;
  }
};

std::pair<int, int> ReferenceHeights(unsigned cell) {
  const int low = static_cast<int>(cell % 1024);
  const int raw = (low < 512 ? low : low - 1024) * 64 -
                  static_cast<int>((cell / 1024) % 8) * 128;
  int ground = raw % 32768;
  if (ground < 0) ground += 32768;
  if (ground >= 16384) ground -= 32768;
  return {raw, ground};
}
}  // namespace

TEST_CASE("Tile height decoding covers every 16-bit cell value",
          "[ai][terrain]") {
  TerrainFixture fixture;
  for (unsigned cell = 0; cell < 65536; ++cell) {
    fixture.heights[9][17] = static_cast<u_short>(cell);
    const auto [raw, ground] = ReferenceHeights(cell);
    CAPTURE(cell);
    CHECK(Field_GroundHeightFlagsAtTile(17, 9) == raw);
    CHECK(Field_GroundHeightAtTile(17, 9) == ground);
  }
}

TEST_CASE("Packed height decoding ignores both non-coordinate bytes",
          "[ai][terrain]") {
  TerrainFixture fixture;
  for (unsigned cell = 0; cell < 65536; ++cell) {
    fixture.heights[9][17] = static_cast<u_short>(cell);
    const auto [raw, ground] = ReferenceHeights(cell);
    CAPTURE(cell);
    CHECK(Field_GroundHeightFlagsAtPackedTile(0xAB09CD11) == raw);
    CHECK(Field_GroundHeightAtPackedTile(0xAB09CD11) == ground);
  }
}

TEST_CASE("World height decoding selects the tile before decoding its cell",
          "[ai][terrain]") {
  TerrainFixture fixture;
  for (unsigned cell = 0; cell < 65536; ++cell) {
    fixture.heights[9][17] = static_cast<u_short>(cell);
    const auto [raw, ground] = ReferenceHeights(cell);
    CAPTURE(cell);
    CHECK(Field_GroundHeightFlagsAtWorldPosition(2303, 1155) == raw);
    CHECK(Field_GroundHeightAtWorldPosition(2303, 1155) == ground);
  }
}

TEST_CASE("Terrain lookup preserves full grid row stride and final cell bounds",
          "[ai][terrain]") {
  TerrainFixture fixture;
  for (int z = 0; z < kAiFrameGridSide; ++z) {
    for (int x = 0; x < kAiFrameGridSide; ++x) {
      fixture.heights[z][x] = static_cast<u_short>(z * 109 + x * 53);
    }
  }
  for (int z = 0; z < kAiFrameGridSide; ++z) {
    for (int x = 0; x < kAiFrameGridSide; ++x) {
      const auto [raw, ground] = ReferenceHeights(fixture.heights[z][x]);
      CHECK(Field_GroundHeightFlagsAtTile(x, z) == raw);
      CHECK(Field_GroundHeightAtWorldPosition(x * 128 + 127, z * 128 + 127) ==
            ground);
    }
  }
}

TEST_CASE(
    "AI destination reservation changes only its requested navigation bit",
    "[ai][terrain]") {
  TerrainFixture fixture;
  Battle_Ai_ReserveAiDestinationTile(13, 7);
  Battle_Ai_ReserveAiDestinationTile(13, 7);
  for (int z = 0; z < kAiFrameGridSide; ++z) {
    for (int x = 0; x < kAiFrameGridSide; ++x) {
      CHECK(fixture.flags[z][x] == (x == 13 && z == 7 ? 0x80 : 0));
    }
  }
  Battle_Ai_tileFlagMap = nullptr;
  Battle_Ai_ReserveAiDestinationTile(13, 7);
  CHECK(fixture.flags[7][13] == 0x80);
}

TEST_CASE(
    "AI adjacent steps use all signed directions and reject missing floors",
    "[ai][terrain]") {
  const auto direction = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const auto cell = GENERATE(0, 1, 1023);
  const std::array<int, 8> dx{0, -1, -1, -1, 0, 1, 1, 1};
  const std::array<int, 8> dz{-1, -1, 0, 1, 1, 1, 0, -1};
  TerrainFixture fixture;
  fixture.heights[15 + dz[direction]][15 + dx[direction]] =
      static_cast<u_short>(cell);
  CHECK(Battle_Ai_IsAdjacentTileStepBlocked(0x000F000F, direction) ==
        (cell != 0));
}

TEST_CASE(
    "AI adjacent steps treat grid exits as blocked without out-of-range reads",
    "[ai][terrain]") {
  TerrainFixture fixture;
  CHECK(Battle_Ai_IsAdjacentTileStepBlocked(0, 0) == 1);
  CHECK(Battle_Ai_IsAdjacentTileStepBlocked(0, 2) == 1);
  CHECK(Battle_Ai_IsAdjacentTileStepBlocked(0, 4) == 0);
  CHECK(Battle_Ai_IsAdjacentTileStepBlocked(0, 6) == 0);
  CHECK(Battle_Ai_IsAdjacentTileStepBlocked(0x001F001F, 4) == 1);
  CHECK(Battle_Ai_IsAdjacentTileStepBlocked(0x001F001F, 6) == 1);
}
