#include "Battle/SphereTerrain.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Sphere.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

// BATTLE.PRG func_800C2E24, src/BATTLE/BATTLE.PRG/5A624.s.
// The original scanline workspace was in scratch RAM and the target sphere.
// Keep its terrain/occlusion passes, but own the interval storage here.
namespace {
constexpr int kRows = 224;
constexpr int kCells = 9;
constexpr std::size_t kCellCount = static_cast<std::size_t>(kCells) * kCells;
constexpr int kCellWidth = 128;
constexpr int kEdgesPerRow = 19;
constexpr int kEdges = 180;
constexpr int kCirclePoints = 32;
constexpr int kFixedOne = 4096;
constexpr int kFixedBits = 12;
constexpr int kFixedHalf = kFixedOne / 2;
constexpr int kSpanOne = 65536;
constexpr int kSpanBits = 16;
constexpr int kSpanHalf = kSpanOne / 2;
constexpr int kDepthCount = 2048;
constexpr int kMaximumWall = 48;
constexpr int kFlatEdge = 0x7F;
constexpr int kReverseEdge = 0x80;
constexpr int kCoordinateBits = 16;
constexpr int kEdgeCoordinateBits = 4;
constexpr unsigned kEdgeCoordinateMask = 15;
constexpr int kCornerBits = 6;
constexpr int kCornerOrientations = 4;
constexpr int kTrianglePoints = 3;

struct Point {
  int x;
  int y;
  int depth;
};

struct Span {
  int left;
  int right;
};

struct Edge {
  unsigned flags = 0;
  int end_y = 0;
  unsigned coordinates = 0;
};

using Rows = std::array<std::vector<Span>, kRows>;
using Crossings = std::array<std::vector<Point>, kRows>;

[[noreturn]] void Invalid(const char* message) {
  Core_Debugf("sphere", "%s", message);
  throw std::runtime_error(message);
}

Point Project(int x, int y, int z) {
  const SVECTOR vertex{static_cast<std::int16_t>(x),
                       static_cast<std::int16_t>(y),
                       static_cast<std::int16_t>(z), 0};
  Core_GteWord xy = 0;
  Core_GteWord depth = 0;
  Core_Gte_LoadV0(&vertex);
  Core_Gte_Rtps();
  Core_Gte_StoreSxy2(&xy);
  Core_Gte_StoreSz3Otz(&depth);
  return {static_cast<std::int16_t>(xy),
          static_cast<std::int16_t>(xy >> kCoordinateBits),
          static_cast<int>(depth)};
}

std::int64_t Slope(int from, int to, int height) {
  // D_800EA130: floor(32768 / height), not floating-point interpolation.
  return static_cast<std::int64_t>(to - from) * 2 * (kSpanHalf / height);
}

template <typename Callback>
void ScanEdge(Point from, Point to, Callback visit) {
  if (from.y == to.y) return;
  if (from.y > to.y) std::swap(from, to);
  const int begin = std::max(from.y, 0);
  const int end = std::min(to.y, kRows);
  const auto step_x = Slope(from.x, to.x, to.y - from.y);
  const auto step_depth = Slope(from.depth, to.depth, to.y - from.y);
  auto x = static_cast<std::int64_t>(from.x) * kSpanOne + kSpanHalf +
           step_x * (begin - from.y);
  auto depth = static_cast<std::int64_t>(from.depth) * kSpanOne + kSpanHalf +
               step_depth * (begin - from.y);
  for (int y = begin; y < end; ++y) {
    visit(Point{static_cast<int>(x >> kSpanBits), y,
                static_cast<int>(depth >> kSpanBits)});
    x += step_x;
    depth += step_depth;
  }
}

void Subtract(std::vector<Span>& row, int left, int right) {
  if (left >= right) return;
  for (std::size_t index = 0; index < row.size();) {
    const auto span = row[index];
    if (span.right <= left || right <= span.left) {
      ++index;
    } else if (left <= span.left && span.right <= right) {
      row.erase(row.begin() + static_cast<std::ptrdiff_t>(index));
    } else if (span.left < left && right < span.right) {
      row[index].right = left;
      row.insert(row.begin() + static_cast<std::ptrdiff_t>(index + 1),
                 Span{right, span.right});
      index += 2;
    } else {
      row[index] =
          span.left < left ? Span{span.left, left} : Span{right, span.right};
      ++index;
    }
  }
}

int TerrainHeight(int packed, int elevation) {
  const int floor =
      (static_cast<std::int16_t>(static_cast<std::uint32_t>(packed) >> 1) >> 1);
  const int ceiling = packed >> 17;
  if (elevation >= floor) {
    return (packed & 1) != 0 ? std::min(elevation - floor, kMaximumWall) : 1;
  }
  return ceiling == 0 || ceiling < elevation ? 0 : 1;
}

int RotatedCell(int row, int column, int quadrant) {
  switch (quadrant) {
    case 0:
      return row * kCells + column;
    case 1:
      return (kCells - 1 - column) * kCells + row;
    case 2:
      return (kCells - 1 - row) * kCells + kCells - 1 - column;
    default:
      return column * kCells + kCells - 1 - row;
  }
}

std::array<int, kCellCount> Heights(const Battle_TargetSphere& sphere,
                                    int elevation, int quadrant) {
  std::array<int, kCellCount> heights{};
  for (int row = 0; row < kCells; ++row) {
    for (int column = 0; column < kCells; ++column) {
      heights[row * kCells + column] = TerrainHeight(
          sphere.terrainHeights[RotatedCell(row, column, quadrant)], elevation);
    }
  }
  return heights;
}

int WallHeight(const std::array<int, kCellCount>& heights, int index) {
  // D_800EA6B0 maps each edge to its two neighboring cells, with a zero border.
  const int row = index / kEdgesPerRow;
  const int column = index % kEdgesPerRow;
  const auto at = [&heights](int y, int x) {
    return y < 0 || y >= kCells || x < 0 || x >= kCells
               ? 0
               : heights[y * kCells + x];
  };
  if (column < kCells) return at(row - 1, column) + at(row, column);
  const int x = column - kCells;
  return at(row, x - 1) + at(row, x);
}

std::array<Point, 2> ProjectEdge(unsigned coordinates, const SVECTOR& center,
                                 int elevation) {
  const int x = ((center.vx >> 7) - kCells / 2) * kCellWidth;
  const int z = ((center.vz >> 7) - kCells / 2) * kCellWidth;
  const auto vertex = [x, z, elevation](unsigned packed) {
    return Project(
        x + static_cast<int>(packed & kEdgeCoordinateMask) * kCellWidth,
        elevation,
        z + static_cast<int>((packed >> kEdgeCoordinateBits) &
                             kEdgeCoordinateMask) *
                kCellWidth);
  };
  return {vertex(coordinates),
          vertex(coordinates >> (kEdgeCoordinateBits * 2))};
}

Rows TerrainRows(const Battle_TargetSphere& sphere, const SVECTOR& center,
                 int quadrant, const std::array<int, kCellCount>& heights,
                 std::array<Edge, kEdges>& edges) {
  std::array<bool, kEdges> boundary{};
  for (int row = 0; row < kCells; ++row) {
    for (int column = 0; column < kCells; ++column) {
      if (heights[row * kCells + column] != 0) continue;
      const int edge = row * kEdgesPerRow + column;
      for (const int offset : {0, kCells, kCells + 1, kEdgesPerRow}) {
        boundary[edge + offset] = !boundary[edge + offset];
      }
    }
  }

  Crossings crossings;
  for (int index = 0; index < kEdges; ++index) {
    if (!boundary[index]) continue;
    auto& edge = edges[index];
    std::uint16_t coordinates = 0;
    std::memcpy(
        &coordinates,
        sphere.wireEdges + (quadrant * kEdges + index) * sizeof coordinates,
        sizeof coordinates);
    edge.coordinates = coordinates;
    auto [from, to] = ProjectEdge(coordinates, center, center.vy);
    edge.end_y = std::clamp(std::max(from.y, to.y), 0, kRows);
    if (std::abs(from.y - to.y) <= 2) {
      edge.flags = kFlatEdge | (to.x < from.x ? kReverseEdge : 0);
    } else {
      const bool reverse = from.y > to.y;
      if (reverse) std::swap(from, to);
      const int begin = std::max(from.y, 0);
      if (begin < std::min(to.y, kRows)) {
        const auto count = static_cast<unsigned>(crossings[begin].size());
        if ((to.x >= from.x && (count & 1) == 0) ||
            (to.x < from.x && (count & 1) != 0)) {
          edge.flags = count + 1 + (reverse ? kReverseEdge : 0);
        }
      }
    }
    ScanEdge(from, to, [&crossings](Point point) {
      crossings[point.y].push_back(point);
    });
  }

  Rows result;
  for (int y = 0; y < kRows; ++y) {
    auto& row = crossings[y];
    std::sort(row.begin(), row.end(),
              [](Point left, Point right) { return left.x < right.x; });
    if ((row.size() & 1) != 0) Invalid("sphere terrain boundary is not closed");
    for (std::size_t index = 0; index < row.size(); index += 2) {
      result[y].push_back({row[index].x, row[index + 1].x});
    }
  }
  return result;
}

void OccludeWalls(Rows& rows, const SVECTOR& center,
                  const std::array<int, kCellCount>& heights,
                  const std::array<Edge, kEdges>& edges) {
  for (int index = kCells + 1; index < kEdges - kCells - 1; ++index) {
    const auto& edge = edges[index];
    const int height = WallHeight(heights, index);
    if (edge.flags == 0 || height < 2) continue;
    auto [from, to] = ProjectEdge(edge.coordinates, center, center.vy - height);
    if ((edge.flags & kReverseEdge) != 0) std::swap(from, to);
    if ((edge.flags & kFlatEdge) == kFlatEdge) {
      for (int y = std::max(from.y, 0); y < edge.end_y; ++y) {
        Subtract(rows[y], from.x, to.x);
      }
      continue;
    }
    if (to.y <= from.y) Invalid("sphere wall edge has inverted projection");
    const auto step = Slope(from.x, to.x, to.y - from.y);
    const int begin = std::max(from.y, 0);
    const int end = std::min(std::max(to.y, edge.end_y), kRows);
    for (int y = begin; y < end; ++y) {
      const auto x_fixed = static_cast<std::int64_t>(from.x) * kSpanOne +
                           kSpanHalf + step * (std::min(y, to.y) - from.y);
      const int x = static_cast<int>(x_fixed >> kSpanBits);
      Subtract(rows[y], std::min(from.x, x), std::max(from.x, x));
    }
  }
}

void OccludeCorners(Rows& rows, const Battle_TargetSphere& sphere,
                    const SVECTOR& center) {
  constexpr unsigned kCornerVertices = 0x38847780;
  for (int row = 0; row < kCells; ++row) {
    for (int column = 0; column < kCells; ++column) {
      const unsigned flags = sphere.terrainRowFlags[row * kCells + column];
      if (flags == 0) continue;
      if (flags > kCornerOrientations * kCornerBits ||
          flags % kCornerBits != 0) {
        Invalid("sphere terrain corner has invalid orientation");
      }
      const unsigned corners = kCornerVertices >> flags;
      std::array<Point, kTrianglePoints> points{};
      for (int vertex = 0; vertex < kTrianglePoints; ++vertex) {
        const int x = (center.vx >> 7) + column - kCells / 2 +
                      static_cast<int>((corners >> (vertex * 2)) & 1);
        const int z = (center.vz >> 7) + row - kCells / 2 +
                      static_cast<int>((corners >> (vertex * 2 + 1)) & 1);
        points[vertex] = Project(x * kCellWidth, center.vy, z * kCellWidth);
      }
      Crossings crossings;
      for (std::size_t edge = 0; edge < points.size(); ++edge) {
        ScanEdge(
            points[edge], points[(edge + 1) % points.size()],
            [&crossings](Point point) { crossings[point.y].push_back(point); });
      }
      for (int y = 0; y < kRows; ++y) {
        const auto& line = crossings[y];
        if (line.empty()) continue;
        if (line.size() != 2) Invalid("sphere corner boundary is not closed");
        Subtract(rows[y], std::min(line[0].x, line[1].x),
                 std::max(line[0].x, line[1].x));
      }
    }
  }
}

Crossings CircleRows(const SVECTOR& center, const MATRIX& shape, int scale) {
  const int xx = (shape.m[0][0] * scale + kFixedHalf) >> kFixedBits;
  const int zx = (shape.m[2][0] * scale + kFixedHalf) >> kFixedBits;
  const int xz = (shape.m[0][2] * scale + kFixedHalf) >> kFixedBits;
  const int zz = (shape.m[2][2] * scale + kFixedHalf) >> kFixedBits;
  std::array<Point, kCirclePoints> points{};
  for (int index = 0; index < kCirclePoints; ++index) {
    const int angle = index * kFixedOne / kCirclePoints;
    const int sine = Core_Gte_Sin(angle);
    const int cosine = Core_Gte_Cos(angle);
    points[index] = Project(
        center.vx + ((xx * cosine + zx * sine + kFixedHalf) >> kFixedBits),
        center.vy,
        center.vz + ((xz * cosine + zz * sine + kFixedHalf) >> kFixedBits));
  }
  Crossings result;
  for (std::size_t index = 0; index < points.size(); ++index) {
    ScanEdge(points[index], points[(index + 1) % points.size()],
             [&result](Point point) { result[point.y].push_back(point); });
  }
  return result;
}

std::uint32_t ScreenPosition(int x, int y) {
  return static_cast<std::uint16_t>(x) |
         (static_cast<std::uint32_t>(y) << kCoordinateBits);
}

void DrawRows(const Rows& terrain, const Crossings& circle) {
  constexpr std::uint32_t kPacketSize = 4U << 24;
  constexpr std::uint32_t kAdditiveDrawMode = 0xE1000220;
  constexpr std::uint32_t kTranslucentLine = 0x42000000;
  constexpr std::uint32_t kLinkMask = 0xFFFFFF;
  constexpr int kGreenShift = 8;
  constexpr std::size_t kLinePacketWords = 5;
  for (int y = 0; y < kRows; ++y) {
    const auto& points = circle[y];
    if (points.empty()) continue;
    const auto [left, right] =
        std::minmax_element(points.begin(), points.end(),
                            [](Point a, Point b) { return a.x < b.x; });
    if (left->depth < 0 || left->depth >= kDepthCount) continue;
    for (const auto span : terrain[y]) {
      const int begin = std::max(span.left, left->x);
      const int end = std::min(span.right, right->x);
      if (begin >= end) continue;
      auto* packet = static_cast<u_long*>(Render_frameScratch.primitiveCursor);
      auto* ordering =
          static_cast<u_long*>(Render_frameScratch.orderingTable) + left->depth;
      const std::array<u_long, kLinePacketWords> words{
          (*ordering & kLinkMask) | kPacketSize, kAdditiveDrawMode,
          kTranslucentLine | (static_cast<unsigned>(y / 2) << kGreenShift),
          ScreenPosition(begin, y), ScreenPosition(end, y)};
      std::copy(words.begin(), words.end(), packet);
      *ordering = LegacyGpu_Handle(packet);
      Render_frameScratch.primitiveCursor = packet + words.size();
    }
  }
}
}  // namespace

extern "C" void Battle_DrawSphereTerrainIntersection(SVECTOR* position,
                                                     MATRIX* shape, int scale) {
  if (position == nullptr || shape == nullptr ||
      Battle_targetSphere == nullptr ||
      Render_frameScratch.primitiveCursor == nullptr ||
      Render_frameScratch.orderingTable == nullptr || scale < 0 ||
      scale > kFixedOne) {
    Invalid("sphere terrain drawing requires initialized targeting state");
  }
  static_assert(offsetof(Battle_TargetSphere, wireEdges) ==
                offsetof(Battle_TargetSphereSetupView, wireEdges));
  const auto& sphere =
      *reinterpret_cast<const Battle_TargetSphere*>(Battle_targetSphere);
  const int quadrant = ((sphere.cameraYaw - kFixedOne / 8) >> 10) & 3;
  const auto heights = Heights(sphere, position->vy, quadrant);
  std::array<Edge, kEdges> edges{};
  auto rows = TerrainRows(sphere, *position, quadrant, heights, edges);
  OccludeWalls(rows, *position, heights, edges);
  OccludeCorners(rows, sphere, *position);
  DrawRows(rows, CircleRows(*position, *shape, scale));
}
