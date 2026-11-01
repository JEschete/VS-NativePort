#include "Battle/SphereTerrain.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"

// BATTLE.PRG func_800C2E24, src/BATTLE/BATTLE.PRG/5A624.s.
// The original scanline workspace was in scratch RAM and the target sphere.
// Keep its terrain/occlusion passes, but own the interval storage here.
namespace {
using VS::Battle::SphereScreenPoint;
using VS::Battle::SphereTerrainCanvas;
using VS::Battle::SphereTerrainTables;
using VS::Battle::SphereVector;

constexpr int kRows = VS::Battle::kSphereSliceRows;
constexpr int kCells = 9;
constexpr std::size_t kCellCount = VS::Battle::kSphereTerrainCells;
constexpr int kCellWidth = 128;
constexpr int kCellShift = 7;
constexpr int kEdgesPerRow = 19;
constexpr int kEdges = 180;
constexpr int kCirclePoints = 32;
constexpr int kFixedOne = 4096;
constexpr int kFixedBits = 12;
constexpr int kFixedHalf = kFixedOne / 2;
constexpr int kSpanOne = 65536;
constexpr int kSpanBits = 16;
constexpr int kSpanHalf = kSpanOne / 2;
constexpr int kMaximumWall = 48;
constexpr int kFlatEdge = 0x7F;
constexpr int kReverseEdge = 0x80;
constexpr int kEdgeCoordinateBits = 4;
constexpr unsigned kEdgeCoordinateMask = 15;
constexpr int kCornerBits = 6;
constexpr int kCornerOrientations = 4;
constexpr int kTrianglePoints = 3;
constexpr int kQuadrantShift = 10;
constexpr int kQuadrants = 4;
static_assert(static_cast<std::size_t>(kCells) * kCells == kCellCount);
static_assert(static_cast<std::size_t>(kEdges) * kQuadrants ==
              VS::Battle::kSphereWireEdges);

using Point = SphereScreenPoint;

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
  VS_LOG_ERROR("sphere", "{}", message);
  throw std::runtime_error(message);
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

std::array<int, kCellCount> Heights(const SphereTerrainTables& tables,
                                    int elevation, int quadrant) {
  std::array<int, kCellCount> heights{};
  for (int row = 0; row < kCells; ++row) {
    for (int column = 0; column < kCells; ++column) {
      heights[row * kCells + column] = TerrainHeight(
          tables.heights[RotatedCell(row, column, quadrant)], elevation);
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

class Slice {
 public:
  Slice(SphereVector center, const SphereTerrainTables& tables,
        SphereTerrainCanvas& canvas)
      : center_(center), tables_(tables), canvas_(canvas) {}

  void Draw(const VS::Battle::SphereShape& shape, int scale) {
    const int quadrant =
        ((tables_.camera_yaw - kFixedOne / 8) >> kQuadrantShift) & 3;
    const auto heights = Heights(tables_, center_.y, quadrant);
    std::array<Edge, kEdges> edges{};
    auto rows = TerrainRows(quadrant, heights, edges);
    OccludeWalls(rows, heights, edges);
    OccludeCorners(rows);
    DrawRows(rows, CircleRows(shape, scale));
  }

 private:
  std::array<Point, 2> ProjectEdge(unsigned coordinates, int elevation) {
    const int x = ((center_.x >> kCellShift) - kCells / 2) * kCellWidth;
    const int z = ((center_.z >> kCellShift) - kCells / 2) * kCellWidth;
    const auto vertex = [this, x, z, elevation](unsigned packed) {
      return canvas_.Project(
          x + static_cast<int>(packed & kEdgeCoordinateMask) * kCellWidth,
          elevation,
          z + static_cast<int>((packed >> kEdgeCoordinateBits) &
                               kEdgeCoordinateMask) *
                  kCellWidth);
    };
    return {vertex(coordinates),
            vertex(coordinates >> (kEdgeCoordinateBits * 2))};
  }

  Rows TerrainRows(int quadrant, const std::array<int, kCellCount>& heights,
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
      edge.coordinates = tables_.wire_edges[quadrant * kEdges + index];
      auto [from, to] = ProjectEdge(edge.coordinates, center_.y);
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
      if ((row.size() & 1) != 0) {
        Invalid("sphere terrain boundary is not closed");
      }
      for (std::size_t index = 0; index < row.size(); index += 2) {
        result[y].push_back({row[index].x, row[index + 1].x});
      }
    }
    return result;
  }

  void OccludeWalls(Rows& rows, const std::array<int, kCellCount>& heights,
                    const std::array<Edge, kEdges>& edges) {
    for (int index = kCells + 1; index < kEdges - kCells - 1; ++index) {
      const auto& edge = edges[index];
      const int height = WallHeight(heights, index);
      if (edge.flags == 0 || height < 2) continue;
      auto [from, to] = ProjectEdge(edge.coordinates, center_.y - height);
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

  void OccludeCorner(Rows& rows, int row, int column, unsigned flags) {
    constexpr unsigned kCornerVertices = 0x38847780;
    if (flags > kCornerOrientations * kCornerBits || flags % kCornerBits != 0) {
      Invalid("sphere terrain corner has invalid orientation");
    }
    const unsigned corners = kCornerVertices >> flags;
    std::array<Point, kTrianglePoints> points{};
    for (int vertex = 0; vertex < kTrianglePoints; ++vertex) {
      const int x = (center_.x >> kCellShift) + column - kCells / 2 +
                    static_cast<int>((corners >> (vertex * 2)) & 1);
      const int z = (center_.z >> kCellShift) + row - kCells / 2 +
                    static_cast<int>((corners >> (vertex * 2 + 1)) & 1);
      points[vertex] =
          canvas_.Project(x * kCellWidth, center_.y, z * kCellWidth);
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

  void OccludeCorners(Rows& rows) {
    for (int row = 0; row < kCells; ++row) {
      for (int column = 0; column < kCells; ++column) {
        const unsigned flags = tables_.corner_flags[row * kCells + column];
        if (flags != 0) OccludeCorner(rows, row, column, flags);
      }
    }
  }

  Crossings CircleRows(const VS::Battle::SphereShape& shape, int scale) {
    const int xx = (shape.x_x * scale + kFixedHalf) >> kFixedBits;
    const int zx = (shape.z_x * scale + kFixedHalf) >> kFixedBits;
    const int xz = (shape.x_z * scale + kFixedHalf) >> kFixedBits;
    const int zz = (shape.z_z * scale + kFixedHalf) >> kFixedBits;
    std::array<Point, kCirclePoints> points{};
    for (int index = 0; index < kCirclePoints; ++index) {
      const int angle = index * kFixedOne / kCirclePoints;
      const int sine = VS::Core::Math::Sine(angle);
      const int cosine = VS::Core::Math::Cosine(angle);
      points[index] = canvas_.Project(
          center_.x + ((xx * cosine + zx * sine + kFixedHalf) >> kFixedBits),
          center_.y,
          center_.z + ((xz * cosine + zz * sine + kFixedHalf) >> kFixedBits));
    }
    Crossings result;
    for (std::size_t index = 0; index < points.size(); ++index) {
      ScanEdge(points[index], points[(index + 1) % points.size()],
               [&result](Point point) { result[point.y].push_back(point); });
    }
    return result;
  }

  void DrawRows(const Rows& terrain, const Crossings& circle) {
    for (int y = 0; y < kRows; ++y) {
      const auto& points = circle[y];
      if (points.empty()) continue;
      const auto [left, right] =
          std::minmax_element(points.begin(), points.end(),
                              [](Point a, Point b) { return a.x < b.x; });
      if (left->depth < 0 || left->depth >= VS::Battle::kSphereSliceDepths) {
        continue;
      }
      for (const auto span : terrain[y]) {
        const int begin = std::max(span.left, left->x);
        const int end = std::min(span.right, right->x);
        if (begin < end) canvas_.DrawLine(y, begin, end, left->depth);
      }
    }
  }

  SphereVector center_;
  const SphereTerrainTables& tables_;
  SphereTerrainCanvas& canvas_;
};
}  // namespace

namespace VS::Battle {
void DrawSphereTerrainIntersection(SphereVector position,
                                   const SphereShape& shape, int scale,
                                   const SphereTerrainTables& tables,
                                   SphereTerrainCanvas& canvas) {
  if (scale < 0 || scale > kFixedOne) {
    Invalid("sphere terrain scale is outside 0 to 4096");
  }
  Slice(position, tables, canvas).Draw(shape, scale);
}
}  // namespace VS::Battle
