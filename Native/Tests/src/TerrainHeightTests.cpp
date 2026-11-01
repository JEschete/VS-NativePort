#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/CurrentTerrain.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/Distance.hpp"
#include "Field/TerrainHeight.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Field/Collision.h"
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

VS::Field::TerrainGrid Grid() { return VS::Battle::Ai::CurrentTerrain(); }

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
    CHECK(VS::Field::GroundHeightFlagsAtTile(Grid(), 17, 9) == raw);
    CHECK(VS::Field::GroundHeightAtTile(Grid(), 17, 9) == ground);
  }
}

TEST_CASE("Packed height decoding ignores both non-coordinate bytes",
          "[ai][terrain]") {
  TerrainFixture fixture;
  for (unsigned cell = 0; cell < 65536; ++cell) {
    fixture.heights[9][17] = static_cast<u_short>(cell);
    const auto [raw, ground] = ReferenceHeights(cell);
    CAPTURE(cell);
    CHECK(VS::Field::GroundHeightFlagsAtPackedTile(
              Grid(), VS::Core::Math::UnpackTileCoordinate(0xAB09CD11)) == raw);
    CHECK(VS::Field::GroundHeightAtPackedTile(
              Grid(), VS::Core::Math::UnpackTileCoordinate(0xAB09CD11)) ==
          ground);
  }
}

TEST_CASE("Original terrain occlusion blocks equality and below-ground rays",
          "[ai][terrain][occlusion]") {
  const int height = GENERATE(-1, 0, 1);
  TerrainFixture fixture;
  SVECTOR from{64, static_cast<std::int16_t>(height), 64, 0};
  SVECTOR to{576, static_cast<std::int16_t>(height), 64, 0};
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 0, 0) == (height >= 0));
}

TEST_CASE("Original terrain occlusion detects every one of sixteen samples",
          "[ai][terrain][occlusion]") {
  const int blocked =
      GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  TerrainFixture fixture;
  for (auto& row : fixture.heights) {
    std::fill_n(row, kAiFrameGridSide, 1023);
  }
  SVECTOR from{64, -128, 64, 0};
  SVECTOR to{2112, -128, 64, 0};
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 0, 0) == 0);
  fixture.heights[0][blocked] = 1022;
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 0, 0) == 1);
}

TEST_CASE("Original terrain ray adjusts extents before testing the surface",
          "[ai][terrain][occlusion]") {
  TerrainFixture fixture;
  SVECTOR from{64, 64, 64, 0};
  SVECTOR to{576, 64, 64, 0};
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 128, 128) == 0);
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 128, 0) == 1);
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 0, 128) == 1);
}

TEST_CASE("Original above-ground comparison covers every encoded terrain cell",
          "[ai][terrain][occlusion]") {
  TerrainFixture fixture;
  for (unsigned cell = 0; cell < 65536; ++cell) {
    fixture.heights[9][17] = static_cast<u_short>(cell);
    const int decoded = ReferenceHeights(cell).second;
    const int ground = (cell & 0x2000) != 0 ? 0 : decoded;
    CAPTURE(cell);
    CHECK(VS::Field::IsPositionAboveGround(Grid(), 2240, ground - 1, 1216) ==
          1);
    CHECK(VS::Field::IsPositionAboveGround(Grid(), 2240, ground, 1216) == 0);
    CHECK(VS::Field::IsPositionAboveGround(Grid(), 2240, ground + 1, 1216) ==
          0);
  }
}

TEST_CASE("Terrain ray preserves the original zero-ground flag semantics",
          "[ai][terrain][occlusion]") {
  TerrainFixture fixture;
  for (auto& row : fixture.heights) {
    std::fill_n(row, kAiFrameGridSide, 1023);
  }
  SVECTOR from{64, -128, 64, 0};
  SVECTOR to{576, -128, 64, 0};
  fixture.heights[0][2] = 1021;
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 0, 0) == 1);
  fixture.heights[0][2] |= 0x2000;
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 0, 0) == 0);
}

TEST_CASE(
    "Terrain ray keeps arithmetic step rounding and endpoint quantization",
    "[ai][terrain][occlusion]") {
  TerrainFixture fixture;
  for (auto& row : fixture.heights) {
    std::fill_n(row, kAiFrameGridSide, 1023);
  }
  fixture.heights[0][1] = 1022;
  SVECTOR from{256, -128, 64, 0};
  SVECTOR to{241, -128, 64, 0};
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 0, 0) == 1);
  from.vx = 127;
  to.vx = 142;
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 0, 0) == 0);
}

TEST_CASE("Terrain ray advances before testing its first point",
          "[ai][terrain][occlusion]") {
  TerrainFixture fixture;
  SVECTOR from{64, 1, 64, 0};
  SVECTOR to{64, -1023, 64, 0};
  CHECK(VS::Field::IsTerrainSegmentBlocked(
            Grid(), VS::Core::Math::PositionOf(from),
            VS::Core::Math::PositionOf(to), 0, 0) == 0);
}

TEST_CASE("World height decoding selects the tile before decoding its cell",
          "[ai][terrain]") {
  TerrainFixture fixture;
  for (unsigned cell = 0; cell < 65536; ++cell) {
    fixture.heights[9][17] = static_cast<u_short>(cell);
    const auto [raw, ground] = ReferenceHeights(cell);
    CAPTURE(cell);
    CHECK(VS::Field::GroundHeightFlagsAtWorldPosition(Grid(), 2303, 1155) ==
          raw);
    CHECK(VS::Field::GroundHeightAtWorldPosition(Grid(), 2303, 1155) == ground);
  }
}

TEST_CASE(
    "Optional tile height queries distinguish invalid and missing terrain",
    "[ai][terrain]") {
  TerrainFixture fixture;
  fixture.heights[9][17] = 1023;
  CHECK(VS::Field::TryGroundHeightAtTile(Grid(), 17, 9) == -64);
  CHECK_FALSE(VS::Field::TryGroundHeightAtTile(Grid(), -1, 9).has_value());
  CHECK_FALSE(VS::Field::TryGroundHeightAtTile(Grid(), 32, 9).has_value());

  Battle_Ai_FrameState()->heightMap = nullptr;
  CHECK(Grid().empty());
  CHECK_FALSE(VS::Field::TryGroundHeightAtTile(Grid(), 17, 9).has_value());
}

TEST_CASE("Terrain queries read only the grid they are given",
          "[ai][terrain][promotion]") {
  // Two independently owned grids, neither installed in the AI frame: a mod
  // or tool can query any room it holds without touching global state.
  std::uint16_t first[VS::Field::kTerrainGridSide]
                     [VS::Field::kTerrainGridSide]{};
  std::uint16_t second[VS::Field::kTerrainGridSide]
                      [VS::Field::kTerrainGridSide]{};
  first[9][17] = 1023;
  second[9][17] = 1;
  const VS::Field::TerrainGrid first_grid{
      std::span<const VS::Field::TerrainRow, VS::Field::kTerrainGridSide>(
          first, VS::Field::kTerrainGridSide)};
  const VS::Field::TerrainGrid second_grid{
      std::span<const VS::Field::TerrainRow, VS::Field::kTerrainGridSide>(
          second, VS::Field::kTerrainGridSide)};
  CHECK(VS::Field::GroundHeightAtTile(first_grid, 17, 9) == -64);
  CHECK(VS::Field::GroundHeightAtTile(second_grid, 17, 9) == 64);
  CHECK(VS::Field::GroundHeightAtPackedTile(
            first_grid, VS::Core::Math::TileCoordinate{17, 9}) == -64);
  CHECK(VS::Field::GroundHeightAtWorldPosition(second_grid, 2240, 1216) == 64);
  CHECK(VS::Field::IsPositionAboveGround(first_grid, 2240, -65, 1216));
  CHECK_FALSE(VS::Field::IsPositionAboveGround(first_grid, 2240, -64, 1216));
  CHECK(VS::Field::IsPositionAboveGround(second_grid, 2240, 63, 1216));
  CHECK(VS::Field::TerrainGrid{}.empty());
  CHECK_FALSE(VS::Field::TerrainGrid{}.Contains(0, 0));
}

TEST_CASE("Terrain API takes decoded values instead of register words",
          "[ai][terrain][promotion]") {
  using PackedQuery =
      int (*)(VS::Field::TerrainGrid, VS::Core::Math::TileCoordinate);
  using SegmentQuery =
      bool (*)(VS::Field::TerrainGrid, VS::Core::Math::Position3,
               VS::Core::Math::Position3, int, int);
  CHECK(std::is_same_v<decltype(&VS::Field::GroundHeightAtPackedTile),
                       PackedQuery>);
  CHECK(std::is_same_v<decltype(&VS::Field::GroundHeightFlagsAtPackedTile),
                       PackedQuery>);
  CHECK(std::is_same_v<decltype(&VS::Field::IsTerrainSegmentBlocked),
                       SegmentQuery>);
}

TEST_CASE("Promoted terrain queries have no original module addresses",
          "[ai][terrain][promotion]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // BATTLE.PRG only reached these through direct jal instructions, so the
  // promoted API is called by name and no address resolves to it.
  for (const unsigned address :
       {0x800E42D4u, 0x800E42E0u, 0x800E42ECu, 0x800E4308u, 0x800E4318u,
        0x800E4320u, 0x800E4478u, 0x800E44E8u}) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
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
      CHECK(VS::Field::GroundHeightFlagsAtTile(Grid(), x, z) == raw);
      CHECK(VS::Field::GroundHeightAtWorldPosition(Grid(), x * 128 + 127,
                                                   z * 128 + 127) == ground);
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
