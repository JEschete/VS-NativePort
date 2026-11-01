// Hand-maintained tests for the typed sphere terrain slice (#247).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/SphereTerrain.hpp"
#include "Content/ModuleData.h"

namespace {
using VS::Battle::kSphereTerrainCells;
using VS::Battle::kSphereWireEdges;

constexpr int kGridCells = 9;
constexpr int kEdgesPerRow = 19;
constexpr int kEdgesPerQuadrant = 180;
constexpr int kNibble = 4;
constexpr int kEndpointShift = 8;
constexpr int kScreenOffset = 1024;
constexpr int kScreenScale = 8;
constexpr int kBlockedCell = 1 << 17;

struct Line {
  int y;
  int begin;
  int end;
  int depth;
};

// A top-down orthographic canvas recording every drawn line.
class RecordingCanvas final : public VS::Battle::SphereTerrainCanvas {
 public:
  explicit RecordingCanvas(int depth) : depth_(depth) {}
  [[nodiscard]] VS::Battle::SphereScreenPoint Project(int x, int /*y*/,
                                                      int z) override {
    return {(x + kScreenOffset) / kScreenScale,
            (z + kScreenOffset) / kScreenScale, depth_};
  }
  void DrawLine(int y, int begin, int end, int depth) override {
    lines.push_back({y, begin, end, depth});
  }

  std::vector<Line> lines;

 private:
  int depth_;
};

std::uint16_t PackEdge(int from_x, int from_z, int to_x, int to_z) {
  const auto from = static_cast<unsigned>(from_x | (from_z << kNibble));
  const auto to = static_cast<unsigned>(to_x | (to_z << kNibble));
  return static_cast<std::uint16_t>(from | (to << kEndpointShift));
}

// The grid edges in the order the sphere caches them: per row, the horizontal
// edges then the vertical ones, the same for every quadrant.
std::array<std::uint16_t, kSphereWireEdges> GridEdges() {
  std::array<std::uint16_t, kSphereWireEdges> edges{};
  for (std::size_t index = 0; index < edges.size(); ++index) {
    const int edge = static_cast<int>(index) % kEdgesPerQuadrant;
    const int row = edge / kEdgesPerRow;
    const int column = edge % kEdgesPerRow;
    edges[index] = column < kGridCells ? PackEdge(column, row, column + 1, row)
                                       : PackEdge(column - kGridCells, row,
                                                  column - kGridCells, row + 1);
  }
  return edges;
}

struct Terrain {
  std::array<int, kSphereTerrainCells> heights{};
  std::array<std::uint16_t, kSphereWireEdges> edges = GridEdges();
  std::array<std::uint8_t, kSphereTerrainCells> corners{};

  [[nodiscard]] VS::Battle::SphereTerrainTables Tables() const {
    return {heights, edges, corners, 0};
  }
};

constexpr VS::Battle::SphereVector kCenter{0, -64, 0};
constexpr VS::Battle::SphereShape kShape{256, 0, 0, 256};
constexpr int kFullScale = 4096;
}  // namespace

TEST_CASE("Sphere slices draw the circle across open terrain",
          "[combat][targeting][sphere]") {
  const Terrain terrain;
  RecordingCanvas canvas(100);
  VS::Battle::DrawSphereTerrainIntersection(kCenter, kShape, kFullScale,
                                            terrain.Tables(), canvas);
  REQUIRE_FALSE(canvas.lines.empty());
  for (const auto& line : canvas.lines) {
    CHECK(line.begin < line.end);
    CHECK(line.depth == 100);
    CHECK(line.begin >= (kScreenOffset - 256) / kScreenScale - 1);
    CHECK(line.end <= (kScreenOffset + 256) / kScreenScale + 1);
  }
}

TEST_CASE("Sphere slices skip blocked terrain and unlinked depths",
          "[combat][targeting][sphere]") {
  Terrain blocked;
  blocked.heights.fill(kBlockedCell);
  RecordingCanvas canvas(100);
  VS::Battle::DrawSphereTerrainIntersection(kCenter, kShape, kFullScale,
                                            blocked.Tables(), canvas);
  CHECK(canvas.lines.empty());
  const Terrain open;
  RecordingCanvas too_deep(VS::Battle::kSphereSliceDepths);
  VS::Battle::DrawSphereTerrainIntersection(kCenter, kShape, kFullScale,
                                            open.Tables(), too_deep);
  CHECK(too_deep.lines.empty());
}

TEST_CASE("Sphere slices reject malformed scale and corner orientation",
          "[combat][targeting][sphere]") {
  Terrain terrain;
  RecordingCanvas canvas(100);
  CHECK_THROWS_AS(
      VS::Battle::DrawSphereTerrainIntersection(kCenter, kShape, kFullScale + 1,
                                                terrain.Tables(), canvas),
      std::runtime_error);
  CHECK_THROWS_AS(VS::Battle::DrawSphereTerrainIntersection(
                      kCenter, kShape, -1, terrain.Tables(), canvas),
                  std::runtime_error);
  terrain.corners[40] = 7;
  CHECK_THROWS_AS(VS::Battle::DrawSphereTerrainIntersection(
                      kCenter, kShape, kFullScale, terrain.Tables(), canvas),
                  std::runtime_error);
}

TEST_CASE("The promoted sphere slice has no original module address",
          "[combat][targeting][sphere]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800C2E24u) == nullptr);
}
