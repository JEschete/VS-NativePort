#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/SpecialTerrain.hpp"

namespace {
using VS::Battle::Ai::BuildAiSpecialTerrainHeights;
using VS::Battle::Ai::FindAdjacentAiGroundHeight;
using VS::Battle::Ai::FloodFillAiTerrainHeights;
using VS::Battle::Ai::FloorHeightQuery;
using VS::Battle::Ai::kSpecialTerrainCells;
using VS::Battle::Ai::NavigationDirectionCodes;
using VS::Battle::Ai::SpecialTerrainMaps;

// The battle module's table at 0x800F16EC: even entries step north, west,
// south and east.
constexpr NavigationDirectionCodes OriginalDirections() {
  return {0x30, 0x33, 0x03, 0x13, 0x10, 0x11, 0x01, 0x31};
}
constexpr NavigationDirectionCodes kOriginalDirections = OriginalDirections();
constexpr std::uint16_t kSpecial = 0x20;
constexpr std::uint16_t kAssigned = 0x2000;

constexpr int Centre(int tile) { return (tile << 7) | 0x40; }

struct Maps {
  std::array<std::uint16_t, kSpecialTerrainCells> flags{};
  std::array<std::uint16_t, kSpecialTerrainCells> heights{};

  SpecialTerrainMaps View() { return {flags, heights}; }
  std::uint16_t& Flag(int x, int z) {
    return flags.at((static_cast<std::size_t>(z) * 32) +
                    static_cast<std::size_t>(x));
  }
  std::uint16_t& Height(int x, int z) {
    return heights.at((static_cast<std::size_t>(z) * 32) +
                      static_cast<std::size_t>(x));
  }
};

struct FloorRecorder {
  std::vector<std::pair<int, int>> queries;
  int value = 0;

  FloorHeightQuery Query() {
    return [this](int x, int z) {
      queries.emplace_back(x, z);
      return value;
    };
  }
};
}  // namespace

TEST_CASE(
    "Terrain fill assigns its region's low height bits and keeps high bits",
    "[ai][terrain-fill]") {
  Maps maps;
  maps.Flag(1, 1) = maps.Flag(2, 1) = maps.Flag(2, 2) = maps.Flag(3, 2) =
      kSpecial;
  maps.Height(2, 1) = 0xC155;
  CHECK(
      FloodFillAiTerrainHeights(maps.View(), kOriginalDirections, 1, 1, 0x7FF));
  CHECK(maps.Height(1, 1) == 0x23FF);
  CHECK(maps.Height(2, 1) == 0xE3FF);
  CHECK(maps.Height(2, 2) == 0x23FF);
  CHECK(maps.Height(3, 2) == 0x23FF);
  CHECK(maps.Height(1, 2) == 0);
  CHECK(maps.Flag(2, 1) == kSpecial);
}

TEST_CASE("Terrain fill stops at assigned, plain and diagonal tiles",
          "[ai][terrain-fill]") {
  Maps maps;
  maps.Flag(5, 5) = maps.Flag(6, 5) = maps.Flag(7, 5) = maps.Flag(6, 6) =
      kSpecial;
  maps.Height(6, 5) = kAssigned | 0x10;
  // Only the special bit decides; the ground-blocking bits do not stop a fill.
  maps.Flag(4, 5) = 0x70;
  CHECK(FloodFillAiTerrainHeights(maps.View(), kOriginalDirections, 5, 5, 7));
  CHECK(maps.Height(5, 5) == 0x2007);
  CHECK(maps.Height(4, 5) == 0x2007);
  CHECK(maps.Height(6, 5) == (kAssigned | 0x10));
  CHECK(maps.Height(7, 5) == 0);
  CHECK(maps.Height(6, 6) == 0);
  CHECK(maps.Height(5, 6) == 0);
}

TEST_CASE("Terrain fill writes its start tile even when plain or assigned",
          "[ai][terrain-fill]") {
  Maps maps;
  maps.Height(0, 0) = 0x2ABC;
  maps.Flag(1, 0) = kSpecial;
  CHECK(FloodFillAiTerrainHeights(maps.View(), kOriginalDirections, 0, 0, 5));
  CHECK(maps.Height(0, 0) == 0x2805);
  CHECK(maps.Height(1, 0) == 0x2005);
}

TEST_CASE("Terrain fill refuses a start tile outside the grid",
          "[ai][terrain-fill]") {
  Maps maps;
  maps.flags.fill(kSpecial);
  for (const auto& [x, z] : {std::pair{-1, 0}, std::pair{0, -1},
                             std::pair{32, 0}, std::pair{0, 32}}) {
    CHECK_FALSE(
        FloodFillAiTerrainHeights(maps.View(), kOriginalDirections, x, z, 1));
  }
  CHECK(maps.heights == std::array<std::uint16_t, kSpecialTerrainCells>{});
}

TEST_CASE("Terrain fill covers the whole grid as one region",
          "[ai][terrain-fill]") {
  Maps maps;
  maps.flags.fill(kSpecial);
  maps.heights.fill(0x8000);
  CHECK(FloodFillAiTerrainHeights(maps.View(), kOriginalDirections, 31, 31,
                                  0x1155));
  for (const auto height : maps.heights) CHECK(height == 0xA155);
}

TEST_CASE("Terrain fill steps by the even direction codes as signed pairs",
          "[ai][terrain-fill]") {
  Maps maps;
  // Even entries step two west, two north and twice in place; the odd
  // entries, one east, are never used.
  constexpr NavigationDirectionCodes kDirections{0x02, 0x01, 0x20, 0x01,
                                                 0x00, 0x01, 0x00, 0x01};
  maps.Flag(2, 4) = maps.Flag(4, 2) = maps.Flag(5, 4) = maps.Flag(3, 4) =
      kSpecial;
  CHECK(FloodFillAiTerrainHeights(maps.View(), kDirections, 4, 4, 9));
  CHECK(maps.Height(4, 4) == 0x2009);
  CHECK(maps.Height(2, 4) == 0x2009);
  CHECK(maps.Height(4, 2) == 0x2009);
  CHECK(maps.Height(5, 4) == 0);
  CHECK(maps.Height(3, 4) == 0);
}

TEST_CASE("Adjacent ground height reads the first open even neighbor's centre",
          "[ai][terrain-fill]") {
  Maps maps;
  maps.Flag(3, 2) = 0x10;
  maps.Flag(2, 3) = 0x40;
  maps.Flag(3, 4) = 0x8080;
  FloorRecorder floor;
  floor.value = 0x1280;
  CHECK(FindAdjacentAiGroundHeight(maps.flags, floor.Query(),
                                   kOriginalDirections, 3,
                                   3) == std::optional<std::int16_t>{0x4A});
  CHECK(floor.queries ==
        std::vector<std::pair<int, int>>{{Centre(3), Centre(4)}});
}

TEST_CASE("Adjacent ground height shifts the floor's low halfword signed",
          "[ai][terrain-fill]") {
  Maps maps;
  FloorRecorder floor;
  const auto height = [&maps, &floor](int value) {
    floor.value = value;
    return FindAdjacentAiGroundHeight(maps.flags, floor.Query(),
                                      kOriginalDirections, 0, 0);
  };
  CHECK(height(0x12345) == std::optional<std::int16_t>{0x8D});
  CHECK(height(0x8000) == std::optional<std::int16_t>{-512});
  CHECK(height(-1) == std::optional<std::int16_t>{-1});
  CHECK(height(0x3F) == std::optional<std::int16_t>{0});
  // North and west of the corner are outside the grid; south comes next.
  CHECK(floor.queries.back() == std::pair{Centre(0), Centre(1)});
}

TEST_CASE("Adjacent ground height is absent without an open neighbor or query",
          "[ai][terrain-fill]") {
  Maps maps;
  maps.Flag(0, 1) = kSpecial;
  maps.Flag(1, 0) = 0x40;
  FloorRecorder floor;
  CHECK_FALSE(FindAdjacentAiGroundHeight(maps.flags, floor.Query(),
                                         kOriginalDirections, 0, 0));
  CHECK(floor.queries.empty());
  CHECK_FALSE(FindAdjacentAiGroundHeight(maps.flags, FloorHeightQuery{},
                                         kOriginalDirections, 5, 5));
  // A tile outside the grid still reads its neighbor inside it.
  floor.value = 0x80;
  CHECK(FindAdjacentAiGroundHeight(maps.flags, floor.Query(),
                                   kOriginalDirections, -1,
                                   5) == std::optional<std::int16_t>{2});
  CHECK(floor.queries ==
        std::vector<std::pair<int, int>>{{Centre(0), Centre(5)}});
}

TEST_CASE("Special terrain build scans rows then columns and skips negatives",
          "[ai][terrain-fill]") {
  Maps maps;
  maps.Flag(5, 1) = maps.Flag(5, 2) = maps.Flag(6, 2) = kSpecial;
  maps.Flag(20, 0) = maps.Flag(20, 1) = kSpecial;
  maps.Flag(10, 10) = kSpecial;
  maps.Height(10, 10) = kAssigned | 1;
  std::vector<std::pair<int, int>> queries;
  const FloorHeightQuery floor = [&queries](int x, int z) {
    queries.emplace_back(x, z);
    if (x == Centre(5) && z == Centre(0)) return -64;
    if (x == Centre(4) && z == Centre(2)) return 0x1000;
    if (x == Centre(19) && z == Centre(0)) return 0x0C80;
    return 0x0400;
  };
  BuildAiSpecialTerrainHeights(maps.View(), floor, kOriginalDirections);
  CHECK(maps.Height(20, 0) == 0x2032);
  CHECK(maps.Height(20, 1) == 0x2032);
  CHECK(maps.Height(5, 1) == 0x2040);
  CHECK(maps.Height(5, 2) == 0x2040);
  CHECK(maps.Height(6, 2) == 0x2040);
  CHECK(maps.Height(10, 10) == (kAssigned | 1));
  CHECK(queries == std::vector<std::pair<int, int>>{{Centre(19), Centre(0)},
                                                    {Centre(5), Centre(0)},
                                                    {Centre(4), Centre(2)}});
}
