// Hand-maintained (tools/port/owned.csv).
// MPD room geometry drawing (BATTLE.PRG 2EA3C.s) over bounded spans. The GTE
// and the frame's ordering table are reached only through RoomTransformer and
// RoomPacketQueue; RoomDrawLive.cpp binds both to the running frame.
#include "Render/RoomDraw.hpp"

#include <algorithm>
#include <climits>
#include <cstring>
#include <optional>

namespace VS::Render {
namespace {
namespace Math = VS::Core::Math;

using TriangleVertices = std::array<RoomVertex, kRoomTriangleCorners>;
using QuadVertices = std::array<RoomVertex, kRoomQuadCorners>;

constexpr std::size_t kGroupCountBytes = sizeof(std::uint32_t);
constexpr std::size_t kPolygonCountBytes = 2 * sizeof(std::uint32_t);
constexpr std::size_t kVertexPacketWords = 3;
constexpr int kFixedFractionBits = 12;
constexpr std::int32_t kFixedOneSquared = 1 << (2 * kFixedFractionBits);
constexpr int kHalfwordBits = 16;
constexpr std::uint32_t kColorMask = 0xFFFFFF;

// _mpdGeometryHeader group fields (146C.h:31).
constexpr std::size_t kGroupBoundsOffset = 4;
constexpr std::size_t kGroupAnglesOffset = 16;
constexpr std::size_t kGroupPolygonOffset = 28;
constexpr std::size_t kGroupRotationOffset = 32;
constexpr std::size_t kGroupTranslationOffset = 52;
static_assert(kGroupTranslationOffset + kRoomAxes * sizeof(std::int32_t) ==
              kRoomGroupRecordBytes);

// Triangle and quad record fields (2EA3C.s:104-500). The quad's fourth
// corner delta and colour follow the triangle-sized prefix.
constexpr std::size_t kCommandRecordOffset = 15;
constexpr std::size_t kClutRecordOffset = 26;
constexpr std::size_t kPageRecordOffset = 30;
constexpr std::size_t kQuadDeltaRecordOffset = kRoomTriangleRecordBytes;
using CornerOffsets = std::array<std::size_t, kRoomQuadCorners>;
constexpr CornerOffsets kColorRecordOffsets{12, 16, 20, 36};
constexpr CornerOffsets kURecordOffsets{24, 28, 19, 35};
constexpr CornerOffsets kVRecordOffsets{25, 29, 23, 39};
constexpr std::size_t kFourthCorner = kRoomQuadCorners - 1;

constexpr std::uint32_t kFullPrecisionFlag = 0x800;
constexpr std::uint32_t kPackedCoordinateShift = 3;
constexpr std::uint32_t kCoordinateShiftMask = 31;
constexpr std::uint32_t kNoBackfaceCullCommand = 1;
constexpr std::uint32_t kQuadCommandFlag = 8;
constexpr std::uint32_t kPacketCommandMask = 0xFE;
constexpr int kPacketCommandShift = 24;
constexpr std::size_t kPalettePacketWord = 3;
constexpr std::size_t kPagePacketWord = 6;
constexpr std::uint32_t kLowDetailUvMask = 0x7F7F;
constexpr std::uint32_t kMinimumSubdivisionArea = 512;
constexpr std::int32_t kProjectedCoordinateBound = 1024;
constexpr std::uint32_t kSubdivisionMode = 1;
constexpr int kSubdivisionLevelShift = 13;
constexpr std::uint32_t kSubdivisionLevelMask = 3;
constexpr int kSubdivisionDepthShift = 7;
constexpr std::size_t kRecursiveTriangleChildren = 4;

constexpr std::uint32_t kSkipViewUpdateFlag = 0x100;
constexpr std::uint32_t kCameraFacingFlag = 0x400;
constexpr std::uint32_t kHiddenGroupFlags = 0x1100;
constexpr int kAlternatePassShift = 15;
constexpr std::uint32_t kFacingMask = 0xFF;

constexpr std::size_t kSkyColorOffset = 8;
constexpr std::int32_t kSkyDrawHeight = 240;
constexpr std::int64_t kSkyCenterX = kRoomViewportWidth / 2;
constexpr std::int64_t kSkyCenterY = kSkyDrawHeight / 2;
constexpr std::uint32_t kSkyGouraudCommand = 0x30000000;

constexpr std::size_t kTintChannels = 3;
constexpr int kTintChannelFractionBits = 4;
constexpr std::int64_t kTintLuminanceRed = 5;
constexpr std::int64_t kTintLuminanceGreen = 9;
constexpr std::int64_t kTintLuminanceBlue = 2;
constexpr int kTintOffsetScale = 4;
constexpr std::uint32_t kTintPaddingFlag = 2;
constexpr std::size_t kTintPaddingWords = 2;
constexpr int kPacketLengthShift = 24;

template <typename T>
[[nodiscard]] T Read(std::span<const std::byte> bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.subspan(offset, sizeof(T)).data(), sizeof(T));
  return value;
}

template <typename T>
void Write(std::span<std::byte> bytes, std::size_t offset, const T& value) {
  std::memcpy(bytes.subspan(offset, sizeof(T)).data(), &value, sizeof(T));
}

[[nodiscard]] std::uint32_t ReadByte(std::span<const std::byte> bytes,
                                     std::size_t offset) {
  return std::to_integer<std::uint32_t>(bytes[offset]);
}

[[nodiscard]] RoomPosition ReadPosition(std::span<const std::byte> bytes,
                                        std::size_t offset) {
  return {Read<std::int16_t>(bytes, offset),
          Read<std::int16_t>(bytes, offset + sizeof(std::int16_t)),
          Read<std::int16_t>(bytes, offset + 2 * sizeof(std::int16_t))};
}

[[nodiscard]] std::int32_t ScreenX(std::int32_t screen) {
  return Math::WrapSigned16(static_cast<std::uint32_t>(screen));
}

[[nodiscard]] std::int32_t ScreenY(std::int32_t screen) {
  return Math::WrapSigned16(static_cast<std::uint32_t>(screen) >>
                            kHalfwordBits);
}

[[nodiscard]] std::int32_t Negate(std::int32_t value) {
  return static_cast<std::int32_t>(0U - static_cast<std::uint32_t>(value));
}

// The stored group count when every group record fits in `geometry`.
[[nodiscard]] std::optional<std::size_t> GroupCount(
    std::span<const std::byte> geometry) {
  if (geometry.size() < kGroupCountBytes) {
    return std::nullopt;
  }
  const std::size_t count = Read<std::uint32_t>(geometry, 0);
  if (count > kRoomGroupLimit ||
      (geometry.size() - kGroupCountBytes) / kRoomGroupRecordBytes < count) {
    return std::nullopt;
  }
  return count;
}

// Group `index`'s record, or nothing when it is past the stored count or
// the span.
[[nodiscard]] std::optional<std::size_t> GroupRecordOffset(
    std::span<const std::byte> geometry, std::size_t index) {
  if (geometry.size() < kGroupCountBytes ||
      index >= Read<std::uint32_t>(geometry, 0) || index >= kRoomGroupLimit) {
    return std::nullopt;
  }
  const std::size_t offset = kGroupCountBytes + index * kRoomGroupRecordBytes;
  if (offset > geometry.size() ||
      geometry.size() - offset < kRoomGroupRecordBytes) {
    return std::nullopt;
  }
  return offset;
}

struct PolygonCounts {
  std::uint32_t triangles = 0;
  std::uint32_t quads = 0;
};

// The two counts at the start of `polygons` when every record they claim
// fits after them.
[[nodiscard]] std::optional<PolygonCounts> ReadPolygonCounts(
    std::span<const std::byte> polygons) {
  if (polygons.size() < kPolygonCountBytes) {
    return std::nullopt;
  }
  const PolygonCounts counts{
      Read<std::uint32_t>(polygons, 0),
      Read<std::uint32_t>(polygons, sizeof(std::uint32_t))};
  const std::uint64_t bytes =
      std::uint64_t{counts.triangles} * kRoomTriangleRecordBytes +
      std::uint64_t{counts.quads} * kRoomQuadRecordBytes;
  if (bytes > polygons.size() - kPolygonCountBytes) {
    return std::nullopt;
  }
  return counts;
}

[[nodiscard]] RoomView ReadView(std::span<const std::byte> record) {
  RoomView view;
  std::size_t offset = kGroupRotationOffset;
  for (auto& row : view.rotation.m) {
    for (auto& element : row) {
      element = Read<std::int16_t>(record, offset);
      offset += sizeof(std::int16_t);
    }
  }
  constexpr std::size_t kWord = sizeof(std::int32_t);
  view.translation = {
      Read<std::int32_t>(record, kGroupTranslationOffset),
      Read<std::int32_t>(record, kGroupTranslationOffset + kWord),
      Read<std::int32_t>(record, kGroupTranslationOffset + 2 * kWord)};
  return view;
}

void WriteView(std::span<std::byte> record, const RoomView& view) {
  std::size_t offset = kGroupRotationOffset;
  for (const auto& row : view.rotation.m) {
    for (const std::int16_t element : row) {
      Write(record, offset, element);
      offset += sizeof(std::int16_t);
    }
  }
  constexpr std::size_t kWord = sizeof(std::int32_t);
  Write(record, kGroupTranslationOffset, view.translation.x);
  Write(record, kGroupTranslationOffset + kWord, view.translation.y);
  Write(record, kGroupTranslationOffset + 2 * kWord, view.translation.z);
}

// decomp: BATTLE.PRG func_800985AC / func_80098648 (2EA3C.s:1383, 1429).
[[nodiscard]] RoomView BuildRoomView(const RoomCamera& camera,
                                     const RoomPosition& position,
                                     const Math::Angles3& angles,
                                     bool inverse_scale) {
  Math::Mat3 view = camera.view.rotation;
  if (inverse_scale) {
    if (camera.scale <= 0) {
      return RoomView{};
    }
    Math::Mat3 inverse{};
    const auto diagonal =
        static_cast<std::int16_t>(kFixedOneSquared / camera.scale);
    inverse.m[0][0] = inverse.m[1][1] = inverse.m[2][2] = diagonal;
    view = Math::MultiplyMatrixIntoRight(inverse, view);
  }
  const Math::Angles3 rotation{
      Math::WrapSigned16(static_cast<std::uint32_t>(angles.x)),
      Math::WrapSigned16(static_cast<std::uint32_t>(angles.y)),
      Math::WrapSigned16(static_cast<std::uint32_t>(angles.z))};
  RoomView output;
  output.rotation = Math::MultiplyMatrixIntoRight(
      view, Math::BuildEulerRotationMatrix(rotation));
  const std::array<std::int64_t, kRoomAxes> coordinates{position.x, position.y,
                                                        position.z};
  const std::array<std::int64_t, kRoomAxes> origin{camera.view.translation.x,
                                                   camera.view.translation.y,
                                                   camera.view.translation.z};
  std::array<std::int32_t, kRoomAxes> translated{};
  for (std::size_t row = 0; row < kRoomAxes; ++row) {
    std::int64_t sum = 0;
    for (std::size_t axis = 0; axis < kRoomAxes; ++axis) {
      sum += std::int64_t{view.m[row][axis]} * coordinates[axis];
    }
    translated[row] =
        static_cast<std::int32_t>((sum >> kFixedFractionBits) + origin[row]);
  }
  output.translation = {translated[0], translated[1], translated[2]};
  return output;
}

// The rotation a camera-facing group (flag 0x400) draws with: the camera
// roll in the screen plane, scaled by its zoom (2EA3C.s:1265).
[[nodiscard]] Math::Mat3 CameraFacingRotation(const RoomCamera& camera) {
  const std::int32_t sine =
      Math::MultiplyFixed(Math::Sine(camera.roll), camera.scale);
  const std::int32_t cosine =
      Math::MultiplyFixed(Math::Cosine(camera.roll), camera.scale);
  Math::Mat3 rotation{};
  rotation.m[0][0] = rotation.m[1][1] =
      Math::WrapSigned16(static_cast<std::uint32_t>(cosine));
  rotation.m[0][1] =
      Math::WrapSigned16(static_cast<std::uint32_t>(Negate(sine)));
  rotation.m[1][0] = Math::WrapSigned16(static_cast<std::uint32_t>(sine));
  rotation.m[2][2] =
      Math::WrapSigned16(static_cast<std::uint32_t>(camera.scale));
  return rotation;
}

// A polygon's corners after projection, in packet order.
struct ProjectedPolygon {
  QuadVertices vertices{};
  std::array<std::int32_t, kRoomQuadCorners> screens{};
  std::size_t corners = 0;
  // The deepest corner.
  std::int32_t depth = 0;
  RoomScreenBounds bounds{kProjectedCoordinateBound, kProjectedCoordinateBound,
                          -kProjectedCoordinateBound,
                          -kProjectedCoordinateBound};
};

void AddCorner(ProjectedPolygon& polygon, const RoomVertex& vertex,
               const RoomProjectedPoint& point) {
  const std::size_t index = polygon.corners++;
  polygon.vertices[index] = vertex;
  polygon.screens[index] = point.screen;
  polygon.depth = std::max(polygon.depth, point.depth);
  const std::int32_t x = ScreenX(point.screen);
  const std::int32_t y = ScreenY(point.screen);
  polygon.bounds.left = std::min(polygon.bounds.left, x);
  polygon.bounds.right = std::max(polygon.bounds.right, x);
  polygon.bounds.top = std::min(polygon.bounds.top, y);
  polygon.bounds.bottom = std::max(polygon.bounds.bottom, y);
}

// The record's corners decoded and projected, with the winding of the first
// three and the deepest of those three.
struct RecordProjection {
  ProjectedPolygon polygon;
  std::int32_t winding = 0;
  std::int32_t subdivision_depth = 0;
};

// The quad's fourth corner: the first corner plus a signed byte delta per
// axis at the packed coordinate scale.
[[nodiscard]] RoomPosition QuadCorner(std::span<const std::byte> record,
                                      const RoomPosition& origin,
                                      std::uint32_t shift) {
  const std::int32_t scale = 1 << shift;
  const auto axis = [&](std::size_t index, std::int16_t base) {
    const auto delta =
        Read<std::int8_t>(record, kQuadDeltaRecordOffset + index);
    return Math::WrapSigned16(static_cast<std::uint32_t>(base + delta * scale));
  };
  return {axis(0, origin.x), axis(1, origin.y), axis(2, origin.z)};
}

[[nodiscard]] std::uint32_t ReadColor(std::span<const std::byte> record,
                                      std::size_t offset) {
  return ReadByte(record, offset) | ReadByte(record, offset + 1) << CHAR_BIT |
         ReadByte(record, offset + 2) << (2 * CHAR_BIT);
}

// decomp: BATTLE.PRG func_80097388 (2EA3C.s:104-500), the per-record
// decode and projection ahead of the cull tests.
[[nodiscard]] RecordProjection ProjectRecord(std::span<const std::byte> record,
                                             std::size_t corners,
                                             std::uint32_t flags,
                                             RoomTransformer& transformer) {
  const std::uint32_t shift =
      (flags & kFullPrecisionFlag) != 0 ? 0 : kPackedCoordinateShift;
  const RoomTriangleProjection triangle = ProjectAndDecodeRoomPolygon(
      record.first<kRoomCoordinateBytes>(), shift, transformer);
  RecordProjection projection;
  projection.winding = triangle.winding;
  for (std::size_t index = 0; index < corners; ++index) {
    RoomVertex vertex;
    vertex.color = ReadColor(record, kColorRecordOffsets[index]);
    vertex.uv = static_cast<std::uint16_t>(
        ReadByte(record, kURecordOffsets[index]) |
        ReadByte(record, kVRecordOffsets[index]) << CHAR_BIT);
    RoomProjectedPoint point;
    if (index < kRoomTriangleCorners) {
      vertex.position = triangle.positions[index];
      point = triangle.points[index];
    } else {
      vertex.position = QuadCorner(record, triangle.positions[0], shift);
      point = transformer.Project(vertex.position);
    }
    AddCorner(projection.polygon, vertex, point);
    if (index == kRoomTriangleCorners - 1) {
      projection.subdivision_depth = projection.polygon.depth;
    }
  }
  return projection;
}

// Adds `part` to `total`; false once a part stopped on a full queue.
bool Accumulate(RoomPolygonResult& total, const RoomPolygonResult& part) {
  total.emitted += part.emitted;
  total.status = part.status;
  return part.status == RoomDrawStatus::kDrawn;
}

class RoomPolygonEmitter {
 public:
  RoomPolygonEmitter(const RoomClipState& clip, RoomTransformer& transformer,
                     RoomPacketQueue& queue)
      : clip_(clip), transformer_(transformer), queue_(queue) {}

  RoomPolygonResult Queue(std::span<const std::byte> record,
                          std::size_t corners, std::uint32_t flags,
                          std::int32_t mode);
  RoomPolygonResult SubdivideTriangle(const TriangleVertices& vertices,
                                      int levels,
                                      const RoomPolygonMaterial& material);
  RoomPolygonResult SubdivideQuad(const QuadVertices& vertices, int levels,
                                  const RoomPolygonMaterial& material);

 private:
  RoomPolygonResult EmitLeaf(std::span<const RoomVertex> vertices,
                             const RoomPolygonMaterial& material);
  RoomPolygonResult Write(const ProjectedPolygon& polygon,
                          const RoomPolygonMaterial& material, bool low_detail);

  const RoomClipState& clip_;
  RoomTransformer& transformer_;
  RoomPacketQueue& queue_;
};

// The packet write and link shared by whole and subdivided polygons
// (2EA3C.s:395-470): Gouraud-textured, the half-resolution page past the
// level-of-detail depth.
RoomPolygonResult RoomPolygonEmitter::Write(const ProjectedPolygon& polygon,
                                            const RoomPolygonMaterial& material,
                                            bool low_detail) {
  std::array<std::uint32_t, kRoomQuadPacketWords> packet{};
  auto page = static_cast<std::uint16_t>(material.page);
  if (low_detail) {
    page = static_cast<std::uint16_t>(page - 1U);
  }
  for (std::size_t index = 0; index < polygon.corners; ++index) {
    const RoomVertex& vertex = polygon.vertices[index];
    const std::size_t word = 1 + index * kVertexPacketWords;
    packet[word] = vertex.color & kColorMask;
    packet[word + 1] = static_cast<std::uint32_t>(polygon.screens[index]);
    packet[word + 2] = low_detail
                           ? (std::uint32_t{vertex.uv} >> 1) & kLowDetailUvMask
                           : std::uint32_t{vertex.uv};
  }
  packet[1] |= (material.command & kPacketCommandMask) << kPacketCommandShift;
  packet[kPalettePacketWord] |= material.clut << kHalfwordBits;
  packet[kPagePacketWord] |= std::uint32_t{page} << kHalfwordBits;
  const std::size_t words = 1 + polygon.corners * kVertexPacketWords;
  switch (LinkRoomPolygonByDepth(
      std::span<const std::uint32_t>(packet).first(words), polygon.depth,
      material.flags, clip_, queue_)) {
    case RoomLinkStatus::kLinked:
      return {RoomDrawStatus::kDrawn, 1};
    case RoomLinkStatus::kBufferFull:
      return {RoomDrawStatus::kBufferFull, 0};
    case RoomLinkStatus::kRejected:
    case RoomLinkStatus::kInvalidPacket:
      break;
  }
  return {};
}

// A subdivision leaf: projected afresh, never culled by winding, and drawn
// at full texture detail.
RoomPolygonResult RoomPolygonEmitter::EmitLeaf(
    std::span<const RoomVertex> vertices, const RoomPolygonMaterial& material) {
  ProjectedPolygon polygon;
  for (const RoomVertex& vertex : vertices) {
    AddCorner(polygon, vertex, transformer_.Project(vertex.position));
  }
  if (!IsRoomPolygonOnScreen(polygon.bounds, clip_.screen) ||
      polygon.depth < clip_.near_depth) {
    return {};
  }
  RoomPolygonMaterial leaf = material;
  if (vertices.size() == kRoomTriangleCorners) {
    leaf.command &= ~kQuadCommandFlag;
  }
  return Write(polygon, leaf, false);
}

// decomp: BATTLE.PRG func_80097AEC (2EA3C.s:612).
RoomPolygonResult RoomPolygonEmitter::SubdivideTriangle(
    const TriangleVertices& vertices, int levels,
    const RoomPolygonMaterial& material) {
  if (levels == 0) {
    return EmitLeaf(vertices, material);
  }
  const auto& [corner0, corner1, corner2] = vertices;
  const RoomVertex first = InterpolateRoomVertexMidpoint(corner0, corner1);
  const RoomVertex second = InterpolateRoomVertexMidpoint(corner1, corner2);
  const RoomVertex third = InterpolateRoomVertexMidpoint(corner2, corner0);
  constexpr std::size_t kChildren = 7;
  const std::array<TriangleVertices, kChildren> children{
      {{corner0, first, third},
       {second, first, corner1},
       {second, corner2, third},
       {first, second, third},
       {corner2, corner0, third},
       {corner2, second, corner1},
       {first, corner0, corner1}}};
  RoomPolygonResult total;
  for (std::size_t child = 0; child < children.size(); ++child) {
    const int child_levels =
        child < kRecursiveTriangleChildren ? levels - 1 : 0;
    if (!Accumulate(total, SubdivideTriangle(children[child], child_levels,
                                             material))) {
      break;
    }
  }
  return total;
}

// decomp: BATTLE.PRG func_80097CF0 (2EA3C.s:752).
RoomPolygonResult RoomPolygonEmitter::SubdivideQuad(
    const QuadVertices& vertices, int levels,
    const RoomPolygonMaterial& material) {
  if (levels == 0) {
    return EmitLeaf(vertices, material);
  }
  const auto& [corner0, corner1, corner2, corner3] = vertices;
  const RoomVertex top = InterpolateRoomVertexMidpoint(corner0, corner1);
  const RoomVertex right = InterpolateRoomVertexMidpoint(corner1, corner3);
  const RoomVertex bottom = InterpolateRoomVertexMidpoint(corner2, corner3);
  const RoomVertex left = InterpolateRoomVertexMidpoint(corner0, corner2);
  const RoomVertex center = InterpolateRoomVertexMidpoint(corner1, corner2);
  const std::array<QuadVertices, kRoomQuadCorners> children{
      {{corner0, top, left, center},
       {corner1, top, right, center},
       {corner3, bottom, right, center},
       {corner2, bottom, left, center}}};
  const std::array<TriangleVertices, kRoomQuadCorners> edges{
      {{corner2, bottom, corner3},
       {corner2, corner0, left},
       {top, corner0, corner1},
       {corner3, right, corner1}}};
  RoomPolygonResult total;
  for (const QuadVertices& child : children) {
    if (!Accumulate(total, SubdivideQuad(child, levels - 1, material))) {
      return total;
    }
  }
  for (const TriangleVertices& edge : edges) {
    if (!Accumulate(total, SubdivideTriangle(edge, 0, material))) {
      return total;
    }
  }
  return total;
}

// decomp: BATTLE.PRG func_80097388 (2EA3C.s:104-500), one record: screen and
// near culling, backface culling unless command bit 0 is set, then either
// subdivision or a single packet.
RoomPolygonResult RoomPolygonEmitter::Queue(std::span<const std::byte> record,
                                            std::size_t corners,
                                            std::uint32_t flags,
                                            std::int32_t mode) {
  const RecordProjection projection =
      ProjectRecord(record, corners, flags, transformer_);
  const ProjectedPolygon& polygon = projection.polygon;
  if (!IsRoomPolygonOnScreen(polygon.bounds, clip_.screen) ||
      polygon.depth < clip_.near_depth) {
    return {};
  }
  const std::uint32_t command = ReadByte(record, kCommandRecordOffset);
  const bool culls_backfaces = (command & kNoBackfaceCullCommand) == 0;
  const bool is_quad = corners == kRoomQuadCorners;
  // The quad's second triangle: corners 1, 4 and 3.
  const auto far_winding = [&polygon] {
    return RoomPolygonWinding({polygon.screens[1],
                               polygon.screens[kFourthCorner],
                               polygon.screens[2]});
  };
  if (culls_backfaces && projection.winding <= 0 &&
      (!is_quad || far_winding() <= 0)) {
    return {};
  }
  const RoomPolygonMaterial material{
      flags, command, Read<std::uint16_t>(record, kClutRecordOffset),
      Read<std::uint16_t>(record, kPageRecordOffset)};
  const int levels = static_cast<int>((flags >> kSubdivisionLevelShift) &
                                      kSubdivisionLevelMask) -
                     static_cast<int>(static_cast<std::uint32_t>(
                                          projection.subdivision_depth) >>
                                      kSubdivisionDepthShift);
  std::int32_t area = projection.winding;
  if (!is_quad && area < 0) {
    area = Negate(area);
  }
  if (is_quad && projection.winding < 0 && culls_backfaces) {
    area = far_winding();
  }
  if ((static_cast<std::uint32_t>(mode) & kSubdivisionMode) != 0 &&
      static_cast<std::uint32_t>(area) >= kMinimumSubdivisionArea &&
      levels > 0) {
    if (is_quad) {
      return SubdivideQuad(polygon.vertices, levels, material);
    }
    const TriangleVertices triangle{polygon.vertices[0], polygon.vertices[1],
                                    polygon.vertices[2]};
    return SubdivideTriangle(triangle, levels, material);
  }
  const auto depth = ResolveRoomPolygonDepth(polygon.depth, flags, clip_);
  if (!depth) {
    return {};
  }
  return Write(polygon, material, *depth >= clip_.lod_depth);
}

// Hidden (0x1100), the alternate pass while `mode` bit 0 is set, or facing
// away from the first-person view.
[[nodiscard]] bool IsGroupSkipped(std::uint32_t flags,
                                  const RoomDrawPass& pass) {
  const auto mode = static_cast<std::uint32_t>(pass.mode);
  return (flags & kHiddenGroupFlags) != 0 ||
         (((flags >> kAlternatePassShift) & 1U) & mode) != 0 ||
         ((pass.view_facing >> kHalfwordBits) == 0 &&
          (~flags & (pass.view_facing & kFacingMask)) != 0);
}

[[nodiscard]] bool SkyFacesValid(std::span<const std::byte> faces) {
  for (std::size_t face = 0; face < kSkyDomeFaceCount; ++face) {
    for (std::size_t corner = 0; corner < kRoomTriangleCorners; ++corner) {
      if (ReadByte(faces, face * kSkyDomeFaceBytes + corner) >=
          kSkyDomeVertexCount) {
        return false;
      }
    }
  }
  return true;
}

using SkyPacket = std::array<std::uint32_t, kSkyDomePacketWords>;

// One sky face as a Gouraud triangle centred on the 320x240 frame, or nothing
// when it is behind the camera or off screen.
[[nodiscard]] std::optional<SkyPacket> BuildSkyPacket(
    const std::array<Math::Vec3, kSkyDomeVertexCount>& positions,
    const std::array<std::uint32_t, kSkyDomeVertexCount>& colors,
    const std::array<std::size_t, kRoomTriangleCorners>& indices) {
  if (std::ranges::all_of(indices, [&positions](std::size_t index) {
        return positions[index].z <= 0;
      })) {
    return std::nullopt;
  }
  SkyPacket packet{};
  std::int64_t left = INT64_MAX;
  std::int64_t right = INT64_MIN;
  std::int64_t top = INT64_MAX;
  std::int64_t bottom = INT64_MIN;
  for (std::size_t corner = 0; corner < kRoomTriangleCorners; ++corner) {
    const Math::Vec3& position = positions[indices[corner]];
    const std::int64_t x = std::int64_t{position.x} + kSkyCenterX;
    const std::int64_t y = std::int64_t{position.y} + kSkyCenterY;
    left = std::min(left, x);
    right = std::max(right, x);
    top = std::min(top, y);
    bottom = std::max(bottom, y);
    packet[1 + corner * 2] = colors[indices[corner]] & kColorMask;
    packet[2 + corner * 2] = static_cast<std::uint16_t>(x) |
                             std::uint32_t{static_cast<std::uint16_t>(y)}
                                 << kHalfwordBits;
  }
  if (right < 0 || left >= kRoomViewportWidth || bottom < 0 ||
      top >= kRoomViewportHeight) {
    return std::nullopt;
  }
  packet[1] |= kSkyGouraudCommand;
  return packet;
}
}  // namespace

bool ValidateRoomGeometry(std::span<const std::byte> geometry) {
  const auto count = GroupCount(geometry);
  if (!count) {
    return false;
  }
  const std::size_t header_end =
      kGroupCountBytes + *count * kRoomGroupRecordBytes;
  for (std::size_t index = 0; index < *count; ++index) {
    const std::size_t offset = Read<std::uint32_t>(
        geometry,
        kGroupCountBytes + index * kRoomGroupRecordBytes + kGroupPolygonOffset);
    if (offset < header_end || offset > geometry.size() ||
        !ReadPolygonCounts(geometry.subspan(offset))) {
      return false;
    }
  }
  return true;
}

std::optional<RoomGroup> ReadRoomGroup(std::span<const std::byte> geometry,
                                       std::size_t index) {
  const auto offset = GroupRecordOffset(geometry, index);
  if (!offset) {
    return std::nullopt;
  }
  const auto record = geometry.subspan(*offset, kRoomGroupRecordBytes);
  RoomGroup group;
  group.flags = Read<std::uint32_t>(record, 0);
  std::size_t bound_offset = kGroupBoundsOffset;
  for (auto& axis : group.bounds) {
    for (auto& bound : axis) {
      bound = Read<std::int16_t>(record, bound_offset);
      bound_offset += sizeof(std::int16_t);
    }
  }
  constexpr std::size_t kWord = sizeof(std::int32_t);
  group.angles = {Read<std::int32_t>(record, kGroupAnglesOffset),
                  Read<std::int32_t>(record, kGroupAnglesOffset + kWord),
                  Read<std::int32_t>(record, kGroupAnglesOffset + 2 * kWord)};
  group.polygon_offset = Read<std::uint32_t>(record, kGroupPolygonOffset);
  group.view = ReadView(record);
  return group;
}

bool WriteRoomGroup(std::span<std::byte> geometry, std::size_t index,
                    const RoomGroup& group) {
  const auto offset = GroupRecordOffset(geometry, index);
  if (!offset) {
    return false;
  }
  const auto record = geometry.subspan(*offset, kRoomGroupRecordBytes);
  Write(record, 0, group.flags);
  std::size_t bound_offset = kGroupBoundsOffset;
  for (const auto& axis : group.bounds) {
    for (const std::int16_t bound : axis) {
      Write(record, bound_offset, bound);
      bound_offset += sizeof(std::int16_t);
    }
  }
  constexpr std::size_t kWord = sizeof(std::int32_t);
  Write(record, kGroupAnglesOffset, group.angles.x);
  Write(record, kGroupAnglesOffset + kWord, group.angles.y);
  Write(record, kGroupAnglesOffset + 2 * kWord, group.angles.z);
  Write(record, kGroupPolygonOffset, group.polygon_offset);
  WriteView(record, group.view);
  return true;
}

bool WriteRoomGroupView(std::span<std::byte> geometry, std::size_t index,
                        const RoomView& view) {
  const auto offset = GroupRecordOffset(geometry, index);
  if (!offset) {
    return false;
  }
  WriteView(geometry.subspan(*offset, kRoomGroupRecordBytes), view);
  return true;
}

// decomp: BATTLE.PRG func_800985AC (src/BATTLE/BATTLE.PRG/2EA3C.s:1383).
RoomView BuildRoomGeometryViewMatrix(const RoomCamera& camera,
                                     const RoomPosition& position,
                                     const Core::Math::Angles3& angles) {
  return BuildRoomView(camera, position, angles, false);
}

// decomp: BATTLE.PRG func_80098648 (src/BATTLE/BATTLE.PRG/2EA3C.s:1429).
RoomView BuildScaleCompensatedRoomView(const RoomCamera& camera,
                                       const RoomPosition& position,
                                       const Core::Math::Angles3& angles) {
  return BuildRoomView(camera, position, angles, true);
}

// decomp: BATTLE.PRG func_800983F8 (src/BATTLE/BATTLE.PRG/2EA3C.s:1265).
bool UpdateRoomGroupTransforms(std::span<std::byte> geometry,
                               const RoomCamera& camera) {
  const auto count = GroupCount(geometry);
  if (!count) {
    return false;
  }
  for (std::size_t index = 0; index < *count; ++index) {
    const auto group = ReadRoomGroup(geometry, index);
    if (!group) {
      return false;
    }
    if ((group->flags & kSkipViewUpdateFlag) != 0) {
      continue;
    }
    const RoomPosition origin{group->bounds[0][0], group->bounds[1][0],
                              group->bounds[2][0]};
    RoomView view = BuildRoomGeometryViewMatrix(camera, origin, group->angles);
    if ((group->flags & kCameraFacingFlag) != 0) {
      view.rotation = CameraFacingRotation(camera);
    }
    WriteRoomGroupView(geometry, index, view);
  }
  return true;
}

// decomp: BATTLE.PRG func_8009797C (src/BATTLE/BATTLE.PRG/2EA3C.s:506).
std::array<RoomPosition, kRoomTriangleCorners> DecodeRoomPolygonVertices(
    std::span<const std::byte, kRoomCoordinateBytes> coordinates,
    std::uint32_t coordinate_shift) {
  const std::span<const std::byte> bytes = coordinates;
  const std::uint32_t shift = coordinate_shift & kCoordinateShiftMask;
  const RoomPosition base = ReadPosition(bytes, 0);
  const std::array<std::int16_t, kRoomAxes> origin{base.x, base.y, base.z};
  constexpr std::size_t kDeltaOffset = kRoomAxes * sizeof(std::int16_t);
  std::array<RoomPosition, kRoomTriangleCorners> positions{};
  for (std::size_t vertex = 0; vertex < kRoomTriangleCorners; ++vertex) {
    std::array<std::int16_t, kRoomAxes> axes{};
    for (std::size_t axis = 0; axis < kRoomAxes; ++axis) {
      const std::int8_t delta =
          vertex == 0
              ? std::int8_t{0}
              : Read<std::int8_t>(
                    bytes, kDeltaOffset + (vertex - 1) * kRoomAxes + axis);
      const std::uint32_t shifted =
          static_cast<std::uint32_t>(static_cast<std::int32_t>(delta)) << shift;
      axes[axis] = Math::WrapSigned16(
          static_cast<std::uint32_t>(static_cast<std::int32_t>(origin[axis])) +
          shifted);
    }
    positions[vertex] = {axes[0], axes[1], axes[2]};
  }
  return positions;
}

std::int32_t RoomPolygonWinding(
    const std::array<std::int32_t, kRoomTriangleCorners>& screens) {
  std::array<std::int64_t, kRoomTriangleCorners> x{};
  std::array<std::int64_t, kRoomTriangleCorners> y{};
  for (std::size_t corner = 0; corner < kRoomTriangleCorners; ++corner) {
    x[corner] = ScreenX(screens[corner]);
    y[corner] = ScreenY(screens[corner]);
  }
  const std::int64_t cross = x[0] * y[1] + x[1] * y[2] + x[2] * y[0] -
                             x[0] * y[2] - x[1] * y[0] - x[2] * y[1];
  return static_cast<std::int32_t>(cross);
}

// decomp: BATTLE.PRG func_80097A10 (src/BATTLE/BATTLE.PRG/2EA3C.s:548).
RoomTriangleProjection ProjectAndDecodeRoomPolygon(
    std::span<const std::byte, kRoomCoordinateBytes> coordinates,
    std::uint32_t coordinate_shift, RoomTransformer& transformer) {
  RoomTriangleProjection projection;
  projection.positions =
      DecodeRoomPolygonVertices(coordinates, coordinate_shift);
  std::array<std::int32_t, kRoomTriangleCorners> screens{};
  for (std::size_t corner = 0; corner < kRoomTriangleCorners; ++corner) {
    projection.points[corner] =
        transformer.Project(projection.positions[corner]);
    screens[corner] = projection.points[corner].screen;
  }
  projection.winding = RoomPolygonWinding(screens);
  return projection;
}

// decomp: BATTLE.PRG func_80097F70 (src/BATTLE/BATTLE.PRG/2EA3C.s:921).
RoomVertex InterpolateRoomVertexMidpoint(const RoomVertex& first,
                                         const RoomVertex& second) {
  const auto half = [](std::int32_t left, std::int32_t right) {
    return static_cast<std::int16_t>((left + right) >> 1);
  };
  RoomVertex midpoint;
  midpoint.position = {half(first.position.x, second.position.x),
                       half(first.position.y, second.position.y),
                       half(first.position.z, second.position.z)};
  for (std::size_t channel = 0; channel < kTintChannels; ++channel) {
    const std::size_t shift = channel * CHAR_BIT;
    const std::uint32_t sum = ((first.color >> shift) & UINT8_MAX) +
                              ((second.color >> shift) & UINT8_MAX);
    midpoint.color |= (sum >> 1) << shift;
  }
  const std::uint32_t u =
      ((first.uv & UINT8_MAX) + (second.uv & UINT8_MAX)) >> 1;
  const std::uint32_t v =
      ((first.uv >> CHAR_BIT) + (second.uv >> CHAR_BIT)) >> 1;
  midpoint.uv = static_cast<std::uint16_t>(u | v << CHAR_BIT);
  return midpoint;
}

// decomp: BATTLE.PRG func_80098014 (src/BATTLE/BATTLE.PRG/2EA3C.s:969).
bool IsRoomPolygonOnScreen(const RoomScreenBounds& projected,
                           const RoomScreenBounds& clip) {
  if (projected.left > projected.right || projected.top > projected.bottom) {
    return false;
  }
  return !(projected.right < clip.left || projected.bottom < clip.top ||
           projected.left >= clip.right || projected.top >= clip.bottom);
}

std::optional<std::uint32_t> ResolveRoomPolygonDepth(
    std::int32_t projected_depth, std::uint32_t flags,
    const RoomClipState& clip) {
  if (projected_depth < clip.near_depth) {
    return std::nullopt;
  }
  const std::int64_t biased = std::int64_t{projected_depth} +
                              Math::WrapSigned16(flags >> kHalfwordBits);
  if (biased < 0 || biased < clip.near_depth || biased >= clip.far_depth ||
      biased >= kRoomLastOrderingSlot) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(biased);
}

// decomp: BATTLE.PRG func_800980F8 (src/BATTLE/BATTLE.PRG/2EA3C.s:1036).
RoomLinkStatus LinkRoomPolygonByDepth(std::span<const std::uint32_t> packet,
                                      std::int32_t projected_depth,
                                      std::uint32_t flags,
                                      const RoomClipState& clip,
                                      RoomPacketQueue& queue) {
  if (packet.size() != kRoomTrianglePacketWords &&
      packet.size() != kRoomQuadPacketWords) {
    return RoomLinkStatus::kInvalidPacket;
  }
  const auto depth = ResolveRoomPolygonDepth(projected_depth, flags, clip);
  if (!depth) {
    return RoomLinkStatus::kRejected;
  }
  return queue.Emit(packet, *depth) ? RoomLinkStatus::kLinked
                                    : RoomLinkStatus::kBufferFull;
}

// decomp: BATTLE.PRG func_80097AEC (src/BATTLE/BATTLE.PRG/2EA3C.s:612).
RoomPolygonResult SubdivideRoomTriangle(
    std::span<const RoomVertex, kRoomTriangleCorners> vertices, int levels,
    const RoomPolygonMaterial& material, const RoomClipState& clip,
    RoomTransformer& transformer, RoomPacketQueue& queue) {
  if (levels < 0 || levels > kRoomMaximumSubdivisionLevels) {
    return {RoomDrawStatus::kInvalidLevels, 0};
  }
  TriangleVertices captured{};
  std::ranges::copy(vertices, captured.begin());
  const RoomPolygonMaterial captured_material = material;
  return RoomPolygonEmitter(clip, transformer, queue)
      .SubdivideTriangle(captured, levels, captured_material);
}

// decomp: BATTLE.PRG func_80097CF0 (src/BATTLE/BATTLE.PRG/2EA3C.s:752).
RoomPolygonResult SubdivideRoomQuad(
    std::span<const RoomVertex, kRoomQuadCorners> vertices, int levels,
    const RoomPolygonMaterial& material, const RoomClipState& clip,
    RoomTransformer& transformer, RoomPacketQueue& queue) {
  if (levels < 0 || levels > kRoomMaximumSubdivisionLevels) {
    return {RoomDrawStatus::kInvalidLevels, 0};
  }
  QuadVertices captured{};
  std::ranges::copy(vertices, captured.begin());
  const RoomPolygonMaterial captured_material = material;
  return RoomPolygonEmitter(clip, transformer, queue)
      .SubdivideQuad(captured, levels, captured_material);
}

// decomp: BATTLE.PRG func_80097388 (src/BATTLE/BATTLE.PRG/2EA3C.s:104).
RoomPolygonResult DrawRoomGeometryPolygons(std::span<const std::byte> polygons,
                                           std::uint32_t flags,
                                           std::int32_t mode,
                                           const RoomClipState& clip,
                                           RoomTransformer& transformer,
                                           RoomPacketQueue& queue) {
  const auto counts = ReadPolygonCounts(polygons);
  if (!counts) {
    return {RoomDrawStatus::kInvalidGeometry, 0};
  }
  RoomPolygonEmitter emitter(clip, transformer, queue);
  RoomPolygonResult total;
  std::size_t offset = kPolygonCountBytes;
  for (std::uint32_t face = 0; face < counts->triangles; ++face) {
    if (!Accumulate(total, emitter.Queue(polygons.subspan(
                                             offset, kRoomTriangleRecordBytes),
                                         kRoomTriangleCorners, flags, mode))) {
      return total;
    }
    offset += kRoomTriangleRecordBytes;
  }
  for (std::uint32_t face = 0; face < counts->quads; ++face) {
    if (!Accumulate(
            total, emitter.Queue(polygons.subspan(offset, kRoomQuadRecordBytes),
                                 kRoomQuadCorners, flags, mode))) {
      return total;
    }
    offset += kRoomQuadRecordBytes;
  }
  return total;
}

// decomp: BATTLE.PRG func_8009723C (src/BATTLE/BATTLE.PRG/2EA3C.s:10).
RoomDrawStatus DrawRoomGeometry(std::span<const std::byte> geometry,
                                const RoomDrawPass& pass,
                                const RoomClipState& clip,
                                RoomTransformer& transformer,
                                RoomPacketQueue& queue) {
  if (!ValidateRoomGeometry(geometry)) {
    return RoomDrawStatus::kInvalidGeometry;
  }
  const std::size_t count = Read<std::uint32_t>(geometry, 0);
  for (std::size_t index = 0; index < count; ++index) {
    const auto group = ReadRoomGroup(geometry, index);
    if (!group) {
      return RoomDrawStatus::kInvalidGeometry;
    }
    if (IsGroupSkipped(group->flags, pass)) {
      continue;
    }
    transformer.LoadView(group->view);
    const RoomPolygonResult result = DrawRoomGeometryPolygons(
        geometry.subspan(group->polygon_offset), group->flags, pass.mode, clip,
        transformer, queue);
    if (result.status != RoomDrawStatus::kDrawn) {
      return result.status;
    }
  }
  return RoomDrawStatus::kDrawn;
}

// decomp: BATTLE.PRG func_80098160.
void SetRoomPolygonNearClip(RoomClipState& clip, std::int32_t depth) {
  clip.near_depth = Math::WrapSigned16(static_cast<std::uint32_t>(depth));
}

// decomp: BATTLE.PRG func_8009817C (src/BATTLE/BATTLE.PRG/2EA3C.s:1078).
void SetRoomPolygonFarClip(RoomClipState& clip, std::int32_t depth) {
  clip.far_depth = Math::WrapSigned16(static_cast<std::uint32_t>(depth));
}

// decomp: BATTLE.PRG func_80098194.
void SetRoomTextureLodDepth(RoomClipState& clip, std::int32_t depth) {
  clip.lod_depth = static_cast<std::uint32_t>(static_cast<std::int32_t>(
      Math::WrapSigned16(static_cast<std::uint32_t>(depth))));
}

// decomp: BATTLE.PRG func_800981B8 (src/BATTLE/BATTLE.PRG/2EA3C.s:1101).
void SetRoomPolygonScreenBounds(RoomClipState& clip,
                                const RoomScreenBounds& bounds) {
  const auto wrap = [](std::int32_t edge) -> std::int32_t {
    return Math::WrapSigned16(static_cast<std::uint32_t>(edge));
  };
  clip.screen = {wrap(bounds.left), wrap(bounds.top), wrap(bounds.right),
                 wrap(bounds.bottom)};
}

// decomp: BATTLE.PRG func_8009820C.
RoomDrawStatus DrawSkyDome(std::span<const std::byte> sky,
                           RoomTransformer& transformer,
                           RoomPacketQueue& queue) {
  if (sky.size() < kSkyDomeBytes) {
    return RoomDrawStatus::kInvalidGeometry;
  }
  const auto faces = sky.subspan(kSkyDomeVertexCount * kSkyDomeVertexBytes,
                                 kSkyDomeFaceCount * kSkyDomeFaceBytes);
  if (!SkyFacesValid(faces)) {
    return RoomDrawStatus::kInvalidGeometry;
  }
  std::array<Math::Vec3, kSkyDomeVertexCount> positions{};
  std::array<std::uint32_t, kSkyDomeVertexCount> colors{};
  for (std::size_t vertex = 0; vertex < kSkyDomeVertexCount; ++vertex) {
    const std::size_t offset = vertex * kSkyDomeVertexBytes;
    positions[vertex] = transformer.Transform(ReadPosition(sky, offset));
    colors[vertex] = Read<std::uint32_t>(sky, offset + kSkyColorOffset);
  }
  for (std::size_t face = 0; face < kSkyDomeFaceCount; ++face) {
    const std::size_t offset = face * kSkyDomeFaceBytes;
    const std::array<std::size_t, kRoomTriangleCorners> indices{
        ReadByte(faces, offset), ReadByte(faces, offset + 1),
        ReadByte(faces, offset + 2)};
    const auto packet = BuildSkyPacket(positions, colors, indices);
    if (packet && !queue.Emit(*packet, kRoomLastOrderingSlot)) {
      return RoomDrawStatus::kBufferFull;
    }
  }
  return RoomDrawStatus::kDrawn;
}

// decomp: BATTLE.PRG func_80098E50 (src/BATTLE/BATTLE.PRG/2EA3C.s:2018).
std::uint32_t TintRoomPrimitiveColor(std::uint32_t color,
                                     const RoomColorAdjustment& adjustment) {
  const std::array<std::int16_t, kTintChannels> offsets{
      adjustment.red, adjustment.green, adjustment.blue};
  const auto channel_of = [color](std::size_t channel) {
    return std::int64_t{(color >> (channel * CHAR_BIT)) & UINT8_MAX};
  };
  const auto clamp = [](std::int64_t value) {
    return static_cast<std::uint32_t>(
        std::clamp<std::int64_t>(value, 0, UINT8_MAX));
  };
  const std::int64_t luminance = channel_of(0) * kTintLuminanceRed +
                                 channel_of(1) * kTintLuminanceGreen +
                                 channel_of(2) * kTintLuminanceBlue;
  constexpr std::int64_t kChannelScale = 1 << kTintChannelFractionBits;
  constexpr std::int64_t kBlendScale =
      std::int64_t{1} << (kFixedFractionBits + kTintChannelFractionBits);
  std::uint32_t result = color & ~kColorMask;
  for (std::size_t channel = 0; channel < kTintChannels; ++channel) {
    std::int64_t value = channel_of(channel);
    if (adjustment.strength != 0) {
      const std::int64_t numerator =
          value * kBlendScale +
          adjustment.strength * (luminance - value * kChannelScale);
      value = clamp(numerator <= 0 ? 0 : numerator / kBlendScale);
    }
    result |= clamp(value + offsets[channel]) << (channel * CHAR_BIT);
  }
  return result;
}

// decomp: BATTLE.PRG func_80098D6C (src/BATTLE/BATTLE.PRG/2EA3C.s:1944).
// The whole walk is measured before any colour changes, so a truncated span
// is left untouched.
RoomTintStatus TintRoomGeometryPrimitives(std::span<std::uint32_t> packets,
                                          const RoomTint& tint,
                                          std::uint32_t flags) {
  const std::size_t padding =
      (flags & kTintPaddingFlag) != 0 ? kTintPaddingWords : 0;
  const auto corners_of = [](std::uint32_t tag) -> std::size_t {
    const std::size_t length = tag >> kPacketLengthShift;
    return length == kRoomTriangleCorners * kVertexPacketWords ||
                   length == kRoomQuadCorners * kVertexPacketWords
               ? length / kVertexPacketWords
               : 0;
  };
  RoomTintStatus status = RoomTintStatus::kTinted;
  std::size_t end = 0;
  while (end < packets.size()) {
    const std::size_t corners = corners_of(packets[end]);
    if (corners == 0) {
      status = RoomTintStatus::kStoppedAtForeignPacket;
      break;
    }
    const std::size_t words = 1 + corners * kVertexPacketWords + padding;
    if (words > packets.size() - end) {
      return RoomTintStatus::kTruncated;
    }
    end += words;
  }
  const RoomColorAdjustment adjustment{
      tint.strength, static_cast<std::int16_t>(tint.red * kTintOffsetScale),
      static_cast<std::int16_t>(tint.green * kTintOffsetScale),
      static_cast<std::int16_t>(tint.blue * kTintOffsetScale)};
  for (std::size_t word = 0; word < end;) {
    const std::size_t corners = corners_of(packets[word]);
    for (std::size_t corner = 0; corner < corners; ++corner) {
      std::uint32_t& color = packets[word + 1 + corner * kVertexPacketWords];
      color = TintRoomPrimitiveColor(color, adjustment);
    }
    word += 1 + corners * kVertexPacketWords + padding;
  }
  return status;
}

// decomp: BATTLE.PRG vs_battle_getGeomOffset (2EA3C.s:2470).
RoomScreenOffset GetProjectionScreenOffset(const RoomTransformer& transformer) {
  const RoomScreenOffset offset = transformer.ScreenOffset();
  return {static_cast<std::uint16_t>(offset.x),
          static_cast<std::uint16_t>(offset.y)};
}

// decomp: BATTLE.PRG vs_battle_setGeomOffset (2EA3C.s:2482).
void SetProjectionScreenOffset(RoomTransformer& transformer,
                               const RoomScreenOffset& offset) {
  transformer.SetScreenOffset(offset);
}
}  // namespace VS::Render
