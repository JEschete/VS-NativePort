#include "Menu/TextBoxFrames.hpp"

#include <algorithm>
#include <bit>
#include <utility>
#include <vector>

namespace VS::Menu {
namespace {

constexpr std::size_t kRecordBytes = 4;
constexpr std::size_t kMaximumEncodedPart = 15;
constexpr std::uint8_t kTriangleSentinel = 255;
constexpr std::uint32_t kOutlineColor = 0x40303030;
constexpr std::uint32_t kFrameClut = 0x373E;
constexpr std::uint32_t kFramePage = 0x17;
constexpr std::uint32_t kClearTextureWindow = 0xE2000000;
constexpr std::uint32_t kFrameTextureWindow = 0xE2000318;
constexpr std::uint32_t kFrameDrawMode = 0xE1000000 | kFramePage;
constexpr std::uint32_t kShadowOutline = 0x48000000;
constexpr std::uint32_t kPolylineEnd = 0x55555555;
constexpr std::size_t kShadowWords = 13;
constexpr int kByteBits = 8;
constexpr int kHalfwordBits = 16;
constexpr std::size_t kTriangleCorners = 3;
constexpr std::size_t kBaseFacesOffset = 3;
constexpr std::size_t kFirstPositionWord = 2;
constexpr std::size_t kFirstTextureWord = 3;
constexpr std::size_t kVertexWordStride = 2;
constexpr std::uint32_t kTexturedTriangle = 0x24000000;
constexpr std::uint32_t kTexturedQuad = 0x2C000000;
constexpr std::uint32_t kShadowTriangle = 0x20000000;
constexpr std::uint32_t kShadowQuad = 0x28000000;
constexpr std::uint32_t kBrightnessChannels = 0x010101;

struct FramePoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
};

using FramePoints = std::vector<FramePoint>;

std::uint32_t PackPoint(FramePoint point, int offset = 0) {
  return static_cast<std::uint16_t>(point.x + offset) |
         (static_cast<std::uint32_t>(
              static_cast<std::uint16_t>(point.y + offset))
          << kHalfwordBits);
}

std::int16_t ScalePoint(std::int32_t origin, int coordinate,
                        std::int32_t extent) {
  const auto scaled =
      origin + ((static_cast<std::int64_t>(coordinate) * extent) >> kByteBits);
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(scaled));
}

std::array<int, 2> AlignedPoint(std::array<std::uint8_t, 2> point,
                                std::uint8_t alignment) {
  constexpr int kByteMaximum = 255;
  if ((alignment & 1) != 0) {
    point[1] = static_cast<std::uint8_t>(kByteMaximum - point[1]);
  }
  if ((alignment & 2) != 0) {
    point[0] = static_cast<std::uint8_t>(kByteMaximum - point[0]);
  }
  return {point[0], point[1]};
}

FramePoints TransformPoints(const FrameDefinition& frame,
                            const FrameRequest& request) {
  FramePoints points(frame.vertices.size());
  const auto transform = [&request](const std::array<int, 2>& point) {
    return FramePoint{ScalePoint(request.x, point[0], request.width),
                      ScalePoint(request.y, point[1], request.height)};
  };
  for (std::size_t index = 0; index < frame.base_vertices; ++index) {
    points[index] =
        transform(AlignedPoint(frame.vertices[index], request.alignment));
  }
  if (request.part != 0) {
    constexpr int kSignedByteBoundary = 128;
    constexpr int kByteRange = 256;
    const FramePart& part = frame.parts[request.part - 1];
    const auto anchor =
        AlignedPoint(frame.vertices[part.start_edge], request.alignment);
    for (std::size_t index = frame.base_vertices; index < frame.vertices.size();
         ++index) {
      auto relative = AlignedPoint(frame.vertices[index], request.alignment);
      for (std::size_t axis = 0; axis < relative.size(); ++axis) {
        if (relative[axis] >= kSignedByteBoundary) {
          relative[axis] -= kByteRange;
        }
        relative[axis] += anchor[axis];
      }
      points[index] = transform(relative);
    }
  }
  return points;
}

bool EmitLine(Render::PacketQueue& queue, FramePoint start, FramePoint end) {
  return Render::InsertThreeWordPrimitive(
      queue, {kOutlineColor, PackPoint(start), PackPoint(end)});
}

bool DrawOutline(Render::PacketQueue& queue, const FrameDefinition& frame,
                 const FramePoints& points, std::size_t selected) {
  const FramePart* part = selected == 0 ? nullptr : &frame.parts[selected - 1];
  for (std::size_t edge = 0; edge < frame.base_vertices; ++edge) {
    if (part != nullptr && edge == part->start_edge) {
      for (std::size_t added = part->first_vertex;
           added < part->first_vertex + part->vertex_count; ++added) {
        if (!EmitLine(queue, points[edge], points[added])) {
          return false;
        }
        edge = added;
      }
      if (!EmitLine(queue, points[part->resume_edge], points[edge])) {
        return false;
      }
      edge = part->resume_edge;
    }
    if (!EmitLine(queue, points[edge],
                  points[(edge + 1) % frame.base_vertices])) {
      return false;
    }
  }
  return Render::InsertDrawModeCommand(queue, kClearTextureWindow);
}

std::uint32_t TextureCoordinate(FramePoint point, FramePoint origin) {
  constexpr std::uint32_t kByteMask = 255;
  return (static_cast<std::uint32_t>(point.x - origin.x) & kByteMask) |
         ((static_cast<std::uint32_t>(point.y - origin.y) & kByteMask)
          << kByteBits);
}

bool DrawFace(Render::PacketQueue& foreground, Render::PacketQueue& shadow,
              const FramePoints& points, const FrameFace& face,
              std::uint8_t brightness) {
  const bool quad = face[kTriangleCorners] != kTriangleSentinel;
  const std::size_t corners = quad ? kFrameFaceCorners : kTriangleCorners;
  std::array<FramePoint, kFrameFaceCorners> vertices{};
  FramePoint minimum = points[face[0]];
  for (std::size_t corner = 0; corner < corners; ++corner) {
    vertices[corner] = points[face[corner]];
    minimum.x = std::min(minimum.x, vertices[corner].x);
    minimum.y = std::min(minimum.y, vertices[corner].y);
  }
  constexpr int kTextureTileSize = 64;
  const FramePoint origin{
      static_cast<std::int16_t>((minimum.x >> 6) * kTextureTileSize),
      static_cast<std::int16_t>((minimum.y >> 6) * kTextureTileSize)};
  constexpr std::size_t kMaximumFillWords = 10;
  std::array<std::uint32_t, kMaximumFillWords> fill{};
  fill[0] = foreground.ChainLink() & Render::kPacketLinkMask;
  const std::size_t fill_words =
      corners * kVertexWordStride + kFirstPositionWord;
  fill[0] |= static_cast<std::uint32_t>(fill_words - 1)
             << Render::kPacketLengthShift;
  fill[1] = (quad ? kTexturedQuad : kTexturedTriangle) |
            (static_cast<std::uint32_t>(brightness) * kBrightnessChannels);
  for (std::size_t corner = 0; corner < corners; ++corner) {
    fill[kFirstPositionWord + corner * kVertexWordStride] =
        PackPoint(vertices[corner]);
    fill[kFirstTextureWord + corner * kVertexWordStride] =
        TextureCoordinate(vertices[corner], origin);
  }
  fill[kFirstTextureWord] |= kFrameClut << kHalfwordBits;
  fill[kFirstTextureWord + kVertexWordStride] |= kFramePage << kHalfwordBits;
  if (!foreground.Emit(std::span(fill).first(fill_words))) {
    return false;
  }
  const auto first = PackPoint(vertices[0], 2);
  const auto second = PackPoint(vertices[1], 2);
  const auto third = PackPoint(vertices[2], 2);
  const auto fourth = quad ? PackPoint(vertices[kTriangleCorners], 2) : 0;
  const std::array<std::uint32_t, kShadowWords> packet{
      (shadow.ChainLink() & Render::kPacketLinkMask) | 0x0C000000U,
      quad ? kShadowQuad : kShadowTriangle,
      first,
      second,
      third,
      fourth,
      kShadowOutline,
      first,
      second,
      quad ? fourth : second,
      third,
      first,
      kPolylineEnd};
  return shadow.Emit(packet);
}

bool ValidFace(const FrameFace& face, std::size_t initialized_vertices) {
  for (std::size_t corner = 0; corner < face.size(); ++corner) {
    const std::size_t vertex = face[corner];
    if (corner == face.size() - 1 && vertex == kTriangleSentinel) {
      continue;
    }
    if (vertex >= initialized_vertices) {
      return false;
    }
  }
  return true;
}

}  // namespace

// decomp: BATTLE.PRG func_800CD3E4 (src/BATTLE/BATTLE.PRG/5BF94.c:4715).
std::expected<FrameDefinition, FrameDecodeError> DecodeTextBoxFrame(
    std::span<const std::uint8_t> bytes) {
  if (bytes.size() < kRecordBytes) {
    return std::unexpected(FrameDecodeError::kTruncated);
  }
  const std::size_t vertex_count = bytes[0];
  const std::size_t part_count = bytes[1];
  FrameDefinition frame;
  frame.base_vertices = bytes[2];
  frame.base_faces = bytes[kBaseFacesOffset];
  const std::size_t vertex_offset = (part_count + 1) * kRecordBytes;
  const std::size_t face_offset = vertex_offset + vertex_count * 2;
  if (bytes.size() < face_offset) {
    return std::unexpected(FrameDecodeError::kTruncated);
  }
  if (frame.base_vertices == 0 || frame.base_vertices > vertex_count) {
    return std::unexpected(FrameDecodeError::kVertexCount);
  }
  std::size_t next_vertex = frame.base_vertices;
  std::size_t next_face = frame.base_faces;
  for (std::size_t index = 0; index < part_count; ++index) {
    const auto record = bytes.subspan((index + 1) * kRecordBytes, kRecordBytes);
    FramePart part{record[0], record[1], next_vertex,
                   record[2], next_face, record[kBaseFacesOffset]};
    if (part.start_edge >= frame.base_vertices ||
        part.resume_edge >= frame.base_vertices ||
        part.resume_edge < part.start_edge) {
      return std::unexpected(FrameDecodeError::kOutline);
    }
    next_vertex += part.vertex_count;
    next_face += part.face_count;
    frame.parts.push_back(part);
  }
  if (next_vertex > vertex_count) {
    return std::unexpected(FrameDecodeError::kVertexCount);
  }
  if (next_face > (bytes.size() - face_offset) / kRecordBytes) {
    return std::unexpected(FrameDecodeError::kTruncated);
  }
  for (std::size_t index = 0; index < vertex_count; ++index) {
    frame.vertices.push_back({bytes[vertex_offset + index * 2],
                              bytes[vertex_offset + index * 2 + 1]});
  }
  for (std::size_t index = 0; index < next_face; ++index) {
    const auto record =
        bytes.subspan(face_offset + index * kRecordBytes, kRecordBytes);
    const FrameFace face{record[0], record[1], record[2],
                         record[kBaseFacesOffset]};
    const std::size_t initialized =
        index < frame.base_faces ? frame.base_vertices : vertex_count;
    if (!ValidFace(face, initialized)) {
      return std::unexpected(FrameDecodeError::kFace);
    }
    frame.faces.push_back(face);
  }
  return frame;
}

// decomp: BATTLE.PRG func_800CD3E4 (src/BATTLE/BATTLE.PRG/5BF94.c:4715).
std::expected<void, FrameDecodeError> DrawTextBoxFrame(
    Render::PacketQueue& foreground, Render::PacketQueue& shadow,
    std::span<const std::uint8_t> bytes, const FrameRequest& request) {
  const auto frame = DecodeTextBoxFrame(bytes);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  if (request.part > kMaximumEncodedPart) {
    return std::unexpected(FrameDecodeError::kSelectedPart);
  }
  auto effective = request;
  if (effective.part > frame->parts.size()) {
    effective.part = 0;
  }
  const auto points = TransformPoints(*frame, effective);
  if (!DrawOutline(foreground, *frame, points, effective.part)) {
    return std::unexpected(FrameDecodeError::kCapacity);
  }
  for (std::size_t index = 0; index < frame->faces.size(); ++index) {
    bool selected = index < frame->base_faces;
    if (!selected && effective.part != 0) {
      const auto& part = frame->parts[effective.part - 1];
      selected =
          index >= part.first_face && index - part.first_face < part.face_count;
    }
    if (selected && !DrawFace(foreground, shadow, points, frame->faces[index],
                              request.brightness)) {
      return std::unexpected(FrameDecodeError::kCapacity);
    }
  }
  if (!Render::InsertThreeWordPrimitive(
          foreground, {kFrameDrawMode, kFrameTextureWindow, 0})) {
    return std::unexpected(FrameDecodeError::kCapacity);
  }
  return {};
}

}  // namespace VS::Menu
