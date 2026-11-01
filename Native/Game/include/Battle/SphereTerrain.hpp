#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// The terrain-clipped elevation slice drawn under a spell's target sphere. The
// original kept its scanline workspace in scratch RAM; the native slice owns
// its interval storage and draws through SphereTerrainCanvas.
namespace VS::Battle {
// The 9x9 terrain cells around the sphere centre.
inline constexpr std::size_t kSphereTerrainCells = 81;
// The packed wireframe edges for each of the four camera quadrants.
inline constexpr std::size_t kSphereWireEdges = 4 * 180;
// The screen rows a slice can cover.
inline constexpr int kSphereSliceRows = 224;
// The ordering table depths a slice line can be linked at.
inline constexpr int kSphereSliceDepths = 2048;

struct SphereVector {
  int x = 0;
  int y = 0;
  int z = 0;
};

// The horizontal 4.12 terms of the sphere's shape matrix.
struct SphereShape {
  int x_x = 0;
  int z_x = 0;
  int x_z = 0;
  int z_z = 0;
};

struct SphereScreenPoint {
  int x = 0;
  int y = 0;
  int depth = 0;
};

// The target sphere's cached terrain around its centre.
struct SphereTerrainTables {
  // Packed floor, ceiling and wall flag per cell.
  std::span<const int, kSphereTerrainCells> heights;
  // Packed edge endpoints, by quadrant then edge.
  std::span<const std::uint16_t, kSphereWireEdges> wire_edges;
  // Diagonal corner orientation per cell, a multiple of six or zero.
  std::span<const std::uint8_t, kSphereTerrainCells> corner_flags;
  int camera_yaw = 0;
};

// Where the slice is projected and drawn.
class SphereTerrainCanvas {
 public:
  virtual ~SphereTerrainCanvas() = default;
  // Projects a world point to the screen and its ordering depth.
  [[nodiscard]] virtual SphereScreenPoint Project(int x, int y, int z) = 0;
  // Draws the visible slice from begin to end on screen row y.
  virtual void DrawLine(int y, int begin, int end, int depth) = 0;
};

// Draws the sphere's elevation circle where it meets visible terrain
// (func_800C2E24). scale is 4.12 and must be within 0 to 4096; malformed
// terrain throws std::runtime_error.
void DrawSphereTerrainIntersection(SphereVector position,
                                   const SphereShape& shape, int scale,
                                   const SphereTerrainTables& tables,
                                   SphereTerrainCanvas& canvas);
}  // namespace VS::Battle
