// Hand-maintained (tools/port/owned.csv).
#include "Render/ModelGeometry.hpp"

#include <algorithm>
#include <climits>
#include <cstring>
#include <optional>
#include <utility>

namespace VS::Render {
namespace {
namespace Math = VS::Core::Math;

constexpr std::size_t kGroupSectionOffset = 0x34;
constexpr std::size_t kVertexSectionOffset = 0x38;
constexpr std::size_t kPolygonSectionOffset = 0x3C;
constexpr std::size_t kBoneBytes = 0x10;
constexpr std::size_t kGroupBytes = 2 * sizeof(std::uint16_t);
constexpr std::size_t kVertexBytes = 4 * sizeof(std::int16_t);
constexpr std::uint32_t kSectionAlignmentMask = sizeof(std::uint32_t) - 1;
constexpr int kTriangleCorners = 3;
constexpr int kQuadCorners = 4;
constexpr int kCoordinateBits = sizeof(std::int16_t) * CHAR_BIT;
constexpr std::uint32_t kDepthLimit = UINT16_MAX;

constexpr std::uint32_t kFlatTriangleCommand = 0x24;
constexpr std::uint32_t kFlatQuadCommand = 0x2C;
constexpr std::uint32_t kGouraudTriangleCommand = 0x34;
constexpr std::uint32_t kGouraudQuadCommand = 0x3C;
constexpr std::size_t kFlatTriangleBytes = 16;
constexpr std::size_t kFlatQuadBytes = 20;
constexpr std::size_t kGouraudTriangleBytes = 24;
constexpr std::size_t kGouraudQuadBytes = 32;
constexpr std::size_t kFlatIndexOffset = sizeof(std::uint32_t);
constexpr std::size_t kIndexBytes = sizeof(std::uint16_t);
// The UVs follow the three or four vertex indices.
constexpr auto kFlatTriangleUvOffset = kFlatIndexOffset + 3 * kIndexBytes;
constexpr auto kFlatQuadUvOffset = kFlatIndexOffset + 4 * kIndexBytes;
constexpr std::size_t kFlatCommandOffset = 0;
constexpr std::size_t kGouraudCommandOffset = 11;
constexpr std::size_t kFlatPrefixBytes = 2;
constexpr std::size_t kGouraudPrefixBytes = 12;
constexpr std::size_t kGouraudFirstUvOffset = 6;
constexpr std::size_t kGouraudTriangleTailUvOffset = 20;
constexpr std::size_t kGouraudQuadUvOffset = 24;
constexpr std::size_t kGouraudColorOffset = 2 * sizeof(std::uint32_t);
constexpr std::size_t kGouraudSpecialFlagsOffset = 19;
constexpr std::size_t kFlatSpecialFlagsOffset = 2;
constexpr std::size_t kFlatVertexWords = 2;
constexpr std::size_t kGouraudVertexWords = 3;
constexpr std::size_t kClutPacketWord = 3;
constexpr std::size_t kFlatPagePacketWord = 5;
constexpr std::size_t kGouraudPagePacketWord = 6;

constexpr std::uint32_t kColorMask = 0xFFFFFF;
constexpr int kCommandShift = 3 * CHAR_BIT;
constexpr std::uint32_t kQuadCommandFlag = 8;
constexpr std::uint32_t kSemiTransparentCommandFlag = 2;
constexpr std::uint32_t kBlendModeMask = 0x60;
constexpr std::uint32_t kSemiTransparencyFlag = 0x10;
constexpr std::uint32_t kAppearanceMaterialMask = 0xF0;
constexpr std::uint32_t kAppearanceSubtractiveBlend = 0x40;
constexpr std::uint32_t kAppearanceAdditiveBlend = 0x20;
constexpr std::uint32_t kAppearanceFadeFirstState = 4;
constexpr std::uint32_t kAppearanceFadeEndState = 9;
constexpr std::uint32_t kAppearanceOpaqueState = 6;
constexpr std::uint32_t kAppearanceBlendBoundary = 7;
constexpr std::uint32_t kAppearanceForcedOverride = 15;
constexpr std::uint32_t kNoBackfaceCullFlag = 1;
constexpr std::uint32_t kAdaptiveSplitMode = 3;
constexpr std::uint32_t kDepthBiasMask = 3;
constexpr int kDepthBiasStep = 4;

constexpr std::size_t kPolygonGroupCount = 3;
constexpr std::size_t kSpecialPolygonGroup = 2;
constexpr int kNormalDepthBias = -1;
constexpr int kSpecialDepthBias = -5;
constexpr int kProjectedCoordinateBound = 1024;
constexpr int kViewportWidth = 320;
constexpr int kViewportHeight = 240;
constexpr std::uint32_t kTriangleDepthReciprocal = 341;
constexpr int kDepthReciprocalBits = 12;
constexpr int kQuadDepthShift = 4;
constexpr std::uint32_t kModelOrderingLimit = 0x7C0;
constexpr std::size_t kLimbPaletteGroups = 8;
constexpr std::uint32_t kFirstLimbPaletteBit = 0x80;
constexpr std::uint32_t kPulsedClutOffset = 0x480;

constexpr std::uint32_t kShadowDrawMode = 0xE1000640;
constexpr std::uint32_t kShadowGouraudCommand = 0x32000000;
constexpr std::uint32_t kShadowGrayChannels = 0x010101;
constexpr std::size_t kShadowPacketWords = 8;
constexpr std::size_t kShadowFirstColorWord = 2;
constexpr std::size_t kShadowVertexWords = 2;
constexpr std::size_t kShadowBandFaces = 8;
constexpr int kShadowMinimumExtent = 20;
constexpr int kShadowSlopeLimit = 640;
constexpr int kShadowHeightShift = 3;
constexpr int kShadowExtentShift = 7;
constexpr int kShadowFractionBits = 12;
constexpr int kShadowNeutralLight = 64;
constexpr int kShadowFadeStep = 4;
constexpr std::uint32_t kShadowBucketBias = 2;
constexpr int kShadowDepthShift = 2;

template <typename T>
[[nodiscard]] T Read(std::span<const std::byte> bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.subspan(offset, sizeof(T)).data(), sizeof(T));
  return value;
}

[[nodiscard]] std::int32_t ScreenX(std::int32_t screen) {
  return Math::WrapSigned16(static_cast<std::uint32_t>(screen));
}

[[nodiscard]] std::int32_t ScreenY(std::int32_t screen) {
  return Math::WrapSigned16(static_cast<std::uint32_t>(screen) >>
                            kCoordinateBits);
}

// The GTE NCLIP determinant of three packed screen positions, truncated to a
// word.
[[nodiscard]] std::int32_t NormalClip(std::int32_t first, std::int32_t second,
                                      std::int32_t third) {
  const std::int64_t x0 = ScreenX(first);
  const std::int64_t y0 = ScreenY(first);
  const std::int64_t x1 = ScreenX(second);
  const std::int64_t y1 = ScreenY(second);
  const std::int64_t x2 = ScreenX(third);
  const std::int64_t y2 = ScreenY(third);
  const std::int64_t sum =
      x0 * y1 + x1 * y2 + x2 * y0 - x0 * y2 - x1 * y0 - x2 * y1;
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(sum));
}

[[nodiscard]] std::int32_t ClampDepth(std::int32_t depth) {
  return std::clamp<std::int32_t>(depth, 0, kDepthLimit);
}

struct ModelSections {
  std::uint32_t groups = 0;
  std::uint32_t vertices = 0;
  std::uint32_t polygons = 0;
};

// The group, vertex and polygon offsets, when they are aligned, ordered,
// inside `model` and leave room for the header bones and vertex groups.
[[nodiscard]] std::optional<ModelSections> ReadSections(
    const ModelCounts& counts, std::span<const std::byte> model) {
  if (model.size() < kModelHeaderBytes || counts.bones > kModelBoneLimit) {
    return std::nullopt;
  }
  const ModelSections sections{
      Read<std::uint32_t>(model, kGroupSectionOffset),
      Read<std::uint32_t>(model, kVertexSectionOffset),
      Read<std::uint32_t>(model, kPolygonSectionOffset)};
  if (sections.groups < kModelHeaderBytes + counts.bones * kBoneBytes ||
      sections.groups > sections.vertices ||
      sections.vertices > sections.polygons ||
      sections.polygons > model.size() ||
      counts.groups * kGroupBytes > sections.vertices - sections.groups ||
      ((sections.groups | sections.vertices | sections.polygons) &
       kSectionAlignmentMask) != 0) {
    return std::nullopt;
  }
  return sections;
}

[[nodiscard]] int PolygonCorners(std::uint32_t command, bool colored) {
  if (command == (colored ? kGouraudTriangleCommand : kFlatTriangleCommand)) {
    return kTriangleCorners;
  }
  if (command == (colored ? kGouraudQuadCommand : kFlatQuadCommand)) {
    return kQuadCorners;
  }
  return 0;
}

[[nodiscard]] std::size_t PolygonBytes(std::uint32_t command) {
  switch (command) {
    case kFlatTriangleCommand:
      return kFlatTriangleBytes;
    case kFlatQuadCommand:
      return kFlatQuadBytes;
    case kGouraudTriangleCommand:
      return kGouraudTriangleBytes;
    case kGouraudQuadCommand:
      return kGouraudQuadBytes;
    default:
      return 0;
  }
}

[[nodiscard]] std::array<std::uint32_t, kPolygonGroupCount> PolygonCounts(
    const ModelCounts& counts) {
  return {counts.triangles, counts.quads, counts.other};
}

// The polygon starting at `offset` and its corner count, or nothing when the
// command does not match its group or the polygon runs past `model`.
struct PolygonSpan {
  std::span<const std::byte> data;
  int corners = 0;
};

enum class PolygonRead : std::uint8_t { kRead, kInvalid, kTruncated };

[[nodiscard]] PolygonRead ReadPolygon(std::span<const std::byte> model,
                                      std::size_t offset, bool colored,
                                      std::size_t part, PolygonSpan& polygon) {
  const std::size_t prefix = colored ? kGouraudPrefixBytes : kFlatPrefixBytes;
  if (offset > model.size() || model.size() - offset < prefix) {
    return PolygonRead::kTruncated;
  }
  const auto command = Read<std::uint8_t>(
      model, offset + (colored ? kGouraudCommandOffset : kFlatCommandOffset));
  const int corners = PolygonCorners(command, colored);
  if (corners == 0 || (part != kSpecialPolygonGroup &&
                       corners != static_cast<int>(part) + kTriangleCorners)) {
    return PolygonRead::kInvalid;
  }
  const std::size_t extent = PolygonBytes(command);
  if (extent > model.size() - offset) {
    return PolygonRead::kTruncated;
  }
  polygon = {model.subspan(offset, extent), corners};
  return PolygonRead::kRead;
}

struct PolygonStyle {
  bool colored = false;
  std::uint32_t tint = kNeutralModelTint;
  std::uint32_t pulse_mask = 0;
  std::uint32_t material = 0;
};

struct QueuedPolygon {
  std::span<const std::byte> data;
  int corners = 0;
  bool colored = false;
  std::uint32_t flags = 0;
  int bias = 0;
  std::uint32_t tint = 0;
  std::uint32_t palette = 0;
  bool uniform_color = false;
};

class PolygonWriter {
 public:
  PolygonWriter(const ModelProjectionView& projection,
                const ModelSurface& surface, ModelPacketQueue& queue)
      : projection_(projection),
        vertex_count_(
            std::min(projection.screens.size(), projection.depths.size())),
        surface_(surface),
        queue_(queue) {}

  [[nodiscard]] ModelDrawStatus Queue(const QueuedPolygon& polygon) {
    std::array<std::uint16_t, kQuadCorners> indices{};
    const std::size_t index_offset = polygon.colored ? 0 : kFlatIndexOffset;
    std::array<std::int32_t, kQuadCorners> screen{};
    std::uint32_t depths = 0;
    int left = kProjectedCoordinateBound;
    int top = kProjectedCoordinateBound;
    int right = -kProjectedCoordinateBound;
    int bottom = -kProjectedCoordinateBound;
    for (int corner = 0; corner < polygon.corners; ++corner) {
      const auto slot = static_cast<std::size_t>(corner);
      indices.at(slot) = Read<std::uint16_t>(
          polygon.data, index_offset + slot * sizeof(std::uint16_t));
      if ((indices.at(slot) & kSectionAlignmentMask) != 0 ||
          indices.at(slot) / sizeof(std::int32_t) >= vertex_count_) {
        return ModelDrawStatus::kInvalidVertexIndex;
      }
      const std::size_t vertex = indices.at(slot) / sizeof(std::int32_t);
      depths += static_cast<std::uint32_t>(projection_.depths[vertex]);
      screen.at(slot) = projection_.screens[vertex];
      const int x = ScreenX(screen.at(slot));
      const int y = ScreenY(screen.at(slot));
      left = std::min(left, x);
      right = std::max(right, x);
      top = std::min(top, y);
      bottom = std::max(bottom, y);
    }
    if (!polygon.colored && polygon.corners == kQuadCorners &&
        (polygon.flags & kAdaptiveSplitMode) == kAdaptiveSplitMode) {
      return QueueAdaptiveTriangles(polygon, indices, screen);
    }
    if (right < 0 || bottom < 0 || left >= kViewportWidth ||
        top >= kViewportHeight ||
        ((polygon.flags & kNoBackfaceCullFlag) == 0 &&
         NormalClip(screen[0], screen[1], screen[2]) <= 0)) {
      return ModelDrawStatus::kDrawn;
    }
    int depth = static_cast<int>(polygon.corners == kTriangleCorners
                                     ? (depths * kTriangleDepthReciprocal) >>
                                           kDepthReciprocalBits
                                     : depths >> kQuadDepthShift);
    depth +=
        polygon.bias + static_cast<int>((polygon.flags >> 2) & kDepthBiasMask) *
                           kDepthBiasStep;
    if (depth < 0 || std::cmp_greater_equal(depth, kModelOrderingLimit)) {
      return ModelDrawStatus::kDrawn;
    }
    return Write(polygon, screen, static_cast<std::uint32_t>(depth));
  }

 private:
  // 4B16C.s: a flat quad in adaptive-split mode becomes two triangles split
  // along the diagonal the projected volume's sign selects.
  [[nodiscard]] ModelDrawStatus QueueAdaptiveTriangles(
      const QueuedPolygon& polygon,
      const std::array<std::uint16_t, kQuadCorners>& indices,
      const std::array<std::int32_t, kQuadCorners>& screen) {
    std::array<std::array<std::int64_t, kTriangleCorners>, kTriangleCorners>
        delta{};
    const auto vertex_depth = [&](std::size_t corner) {
      return projection_.depths[indices.at(corner) / sizeof(std::int32_t)];
    };
    for (std::size_t vertex = 0; vertex < kTriangleCorners; ++vertex) {
      delta.at(vertex)[0] = ScreenX(screen.at(vertex + 1)) - ScreenX(screen[0]);
      delta.at(vertex)[1] = ScreenY(screen.at(vertex + 1)) - ScreenY(screen[0]);
      const std::int32_t difference =
          vertex_depth(vertex + 1) - vertex_depth(0);
      delta.at(vertex)[2] =
          vertex < 2
              ? Math::WrapSigned16(static_cast<std::uint32_t>(difference))
              : difference;
    }
    const std::int64_t volume =
        (delta[0][1] * delta[1][2] - delta[0][2] * delta[1][1]) * delta[2][0] +
        (delta[0][2] * delta[1][0] - delta[0][0] * delta[1][2]) * delta[2][1] +
        (delta[0][0] * delta[1][1] - delta[0][1] * delta[1][0]) * delta[2][2];
    constexpr std::array<std::array<std::size_t, kTriangleCorners>, 2> kNormal{
        {{0, 1, 2}, {1, 3, 2}}};
    constexpr std::array<std::array<std::size_t, kTriangleCorners>, 2>
        kAlternate{{{0, 1, 3}, {0, 3, 2}}};
    const auto& order =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(volume)) > 0
            ? kNormal
            : kAlternate;
    for (const auto& corners : order) {
      std::array<std::byte, kFlatTriangleBytes> triangle{};
      triangle[0] = static_cast<std::byte>(kFlatTriangleCommand);
      triangle[1] = static_cast<std::byte>(kFlatTriangleBytes);
      for (std::size_t corner = 0; corner < kTriangleCorners; ++corner) {
        const std::size_t vertex = corners.at(corner);
        const std::uint16_t index = indices.at(vertex);
        std::memcpy(
            &triangle.at(kFlatIndexOffset + corner * sizeof(std::uint16_t)),
            &index, sizeof index);
        const std::size_t uv_target =
            kFlatTriangleUvOffset +
            ((corner + 1) % kTriangleCorners) * sizeof(std::uint16_t);
        const auto uv = Read<std::uint16_t>(
            polygon.data, kFlatQuadUvOffset + vertex * sizeof(std::uint16_t));
        std::memcpy(&triangle.at(uv_target), &uv, sizeof uv);
      }
      QueuedPolygon part = polygon;
      part.data = triangle;
      part.corners = kTriangleCorners;
      part.flags = polygon.flags & kAppearanceMaterialMask;
      part.bias = kNormalDepthBias;
      const ModelDrawStatus status = Queue(part);
      if (status != ModelDrawStatus::kDrawn) {
        return status;
      }
    }
    return ModelDrawStatus::kDrawn;
  }

  [[nodiscard]] static std::size_t UvOffset(const QueuedPolygon& polygon,
                                            std::size_t corner) {
    const bool triangle = polygon.corners == kTriangleCorners;
    const std::size_t uv_corner =
        triangle ? (corner + 1) % kTriangleCorners : corner;
    if (!polygon.colored) {
      return kFlatIndexOffset +
             (static_cast<std::size_t>(polygon.corners) + uv_corner) *
                 sizeof(std::uint16_t);
    }
    if (!triangle) {
      return kGouraudQuadUvOffset + uv_corner * sizeof(std::uint16_t);
    }
    return uv_corner == 0 ? kGouraudFirstUvOffset
                          : kGouraudTriangleTailUvOffset +
                                (uv_corner - 1) * sizeof(std::uint16_t);
  }

  [[nodiscard]] ModelDrawStatus Write(
      const QueuedPolygon& polygon,
      const std::array<std::int32_t, kQuadCorners>& screen,
      std::uint32_t depth) {
    const std::size_t vertex_words =
        polygon.colored ? kGouraudVertexWords : kFlatVertexWords;
    const std::size_t words =
        (polygon.colored ? 1 : 2) +
        static_cast<std::size_t>(polygon.corners) * vertex_words;
    std::array<std::uint32_t, kModelPacketWords> packet{};
    const std::uint32_t page =
        surface_.texture_page | (polygon.flags & kBlendModeMask);
    for (std::size_t corner = 0;
         corner < static_cast<std::size_t>(polygon.corners); ++corner) {
      const std::size_t xy_word = 2 + corner * vertex_words;
      packet.at(xy_word) = static_cast<std::uint32_t>(screen.at(corner));
      packet.at(xy_word + 1) =
          Read<std::uint16_t>(polygon.data, UvOffset(polygon, corner));
      if (polygon.colored) {
        const std::uint32_t color =
            polygon.uniform_color
                ? polygon.tint
                : Read<std::uint32_t>(
                      polygon.data,
                      kGouraudColorOffset + corner * sizeof(std::uint32_t));
        packet.at(xy_word - 1) = color & kColorMask;
      }
    }
    const std::uint32_t command =
        (polygon.colored ? kGouraudTriangleCommand : kFlatTriangleCommand) |
        (polygon.corners == kQuadCorners ? kQuadCommandFlag : 0) |
        ((polygon.flags & kSemiTransparencyFlag) != 0
             ? kSemiTransparentCommandFlag
             : 0);
    if (!polygon.colored) packet[1] = polygon.tint & kColorMask;
    packet[1] |= command << kCommandShift;
    packet[kClutPacketWord] |= polygon.palette << kCoordinateBits;
    packet.at(polygon.colored ? kGouraudPagePacketWord : kFlatPagePacketWord) |=
        page << kCoordinateBits;
    return queue_.Emit(std::span(packet).first(words), depth)
               ? ModelDrawStatus::kDrawn
               : ModelDrawStatus::kBufferFull;
  }

  ModelProjectionView projection_;
  std::size_t vertex_count_;
  ModelSurface surface_;
  ModelPacketQueue& queue_;
};

// 4B16C.s: func_800B516C / func_800B5C30 select the pulsed CLUT only inside
// the eight per-limb ranges; any remaining faces keep the base CLUT.
[[nodiscard]] std::uint32_t LimbPalette(std::span<const std::byte> model,
                                        std::size_t part, std::uint32_t face,
                                        std::uint32_t mask,
                                        std::uint32_t base_palette) {
  std::uint32_t end = 0;
  for (std::size_t limb = 0; limb < kLimbPaletteGroups; ++limb) {
    end += Read<std::uint16_t>(
        model, (part * kLimbPaletteGroups + limb) * sizeof(std::uint16_t));
    if (face < end) {
      return base_palette + ((mask & (kFirstLimbPaletteBit >> limb)) != 0
                                 ? kPulsedClutOffset
                                 : 0);
    }
  }
  return base_palette;
}

[[nodiscard]] bool LimbRangesFit(const ModelCounts& counts,
                                 std::span<const std::byte> model) {
  const auto polygon_counts = PolygonCounts(counts);
  for (std::size_t part = 0; part < kPolygonGroupCount; ++part) {
    std::uint32_t sum = 0;
    for (std::size_t limb = 0; limb < kLimbPaletteGroups; ++limb) {
      sum += Read<std::uint16_t>(
          model, (part * kLimbPaletteGroups + limb) * sizeof(std::uint16_t));
    }
    if (sum > polygon_counts.at(part)) {
      return false;
    }
  }
  return true;
}

ModelDrawStatus DrawPolygons(const ModelPolygonSource& source,
                             const PolygonStyle& style,
                             const ModelSurface& surface,
                             ModelPacketQueue& queue) {
  const std::span<const std::byte> model = source.model;
  if (source.projection.screens.empty() || source.projection.depths.empty()) {
    return ModelDrawStatus::kNoVertices;
  }
  if (model.size() < kModelHeaderBytes) {
    return ModelDrawStatus::kTruncatedModel;
  }
  if (style.pulse_mask != 0 && !LimbRangesFit(source.counts, model)) {
    return ModelDrawStatus::kInvalidLimbRanges;
  }
  PolygonWriter writer(source.projection, surface, queue);
  const auto counts = PolygonCounts(source.counts);
  std::size_t offset = Read<std::uint32_t>(model, kPolygonSectionOffset);
  for (std::size_t part = 0; part < kPolygonGroupCount; ++part) {
    for (std::uint32_t face = 0; face < counts.at(part); ++face) {
      PolygonSpan polygon;
      switch (ReadPolygon(model, offset, style.colored, part, polygon)) {
        case PolygonRead::kRead:
          break;
        case PolygonRead::kInvalid:
          return ModelDrawStatus::kInvalidPolygon;
        case PolygonRead::kTruncated:
          return ModelDrawStatus::kTruncatedModel;
      }
      std::uint32_t flags = 0;
      if (part == kSpecialPolygonGroup) {
        flags = Read<std::uint8_t>(polygon.data,
                                   style.colored ? kGouraudSpecialFlagsOffset
                                                 : kFlatSpecialFlagsOffset);
      }
      if (style.material != 0) {
        flags = (flags & ~kAppearanceMaterialMask) | style.material;
      }
      const std::uint32_t palette =
          style.pulse_mask == 0
              ? surface.palette
              : LimbPalette(model, part, face, style.pulse_mask,
                            surface.palette);
      const ModelDrawStatus status =
          writer.Queue({.data = polygon.data,
                        .corners = polygon.corners,
                        .colored = style.colored,
                        .flags = flags,
                        .bias = part == kSpecialPolygonGroup ? kSpecialDepthBias
                                                             : kNormalDepthBias,
                        .tint = style.tint,
                        .palette = palette,
                        .uniform_color = style.material != 0});
      if (status != ModelDrawStatus::kDrawn) {
        return status;
      }
      offset += polygon.data.size();
    }
  }
  return ModelDrawStatus::kDrawn;
}

[[nodiscard]] int ShadowSlopeAngle(int first, int second,
                                   std::uint16_t radius) {
  const unsigned first_magnitude = first < 0 ? 0U - static_cast<unsigned>(first)
                                             : static_cast<unsigned>(first);
  const unsigned second_magnitude = second < 0
                                        ? 0U - static_cast<unsigned>(second)
                                        : static_cast<unsigned>(second);
  const int angle = Math::Arctangent2(
      first_magnitude <= second_magnitude ? first : second, radius);
  return angle >= -kShadowSlopeLimit && angle < kShadowSlopeLimit ? angle : 0;
}

// The terrain slope under the actor: none on a platform, else the angle of
// the support heights a radius away along each axis.
[[nodiscard]] Math::Angles3 ShadowGroundRotation(const ActorShadowSource& actor,
                                                 int floor,
                                                 ShadowGround& ground) {
  const Math::Angles3 flat{};
  Math::Angles3 slope{};
  if (ground.OnPlatform()) {
    return slope;
  }
  const int radius = actor.radius;
  const int plus_x = ground.SupportHeight(radius, 0);
  if (ground.OnPlatform()) {
    return slope;
  }
  const int minus_x = ground.SupportHeight(-radius, 0);
  if (ground.OnPlatform()) {
    return slope;
  }
  if (plus_x != minus_x) {
    slope.z = ShadowSlopeAngle(floor - minus_x, plus_x - floor, actor.radius);
  }
  const int minus_z = ground.SupportHeight(0, -radius);
  if (ground.OnPlatform()) {
    return flat;
  }
  const int plus_z = ground.SupportHeight(0, radius);
  if (ground.OnPlatform()) {
    return flat;
  }
  if (plus_z != minus_z) {
    slope.x = ShadowSlopeAngle(floor - plus_z, minus_z - floor, actor.radius);
  }
  return slope;
}

[[nodiscard]] std::int32_t ShadowColumnScale(std::uint32_t size,
                                             std::int16_t model_scale,
                                             std::uint16_t blend) {
  // MULTU/MFLO truncates before each shift; LH then SRL retains the sign bits.
  const std::uint32_t half_blend =
      static_cast<std::uint32_t>(
          static_cast<std::int32_t>(static_cast<std::int16_t>(blend))) >>
      1;
  std::uint32_t value =
      (size * static_cast<std::uint16_t>(model_scale)) >> kShadowFractionBits;
  value = (value * half_blend) >> kShadowFractionBits;
  return static_cast<std::int32_t>(value);
}

[[nodiscard]] ModelTransform BuildShadowTransform(
    const ActorShadowSource& actor, const ModelTransform& camera,
    const ModelVertex& root_offset, int floor, ShadowGround& ground,
    ModelTransformer& transformer) {
  const Math::Mat3 terrain = Math::MultiplyMatrixIntoRight(
      camera.rotation,
      Math::BuildRotationMatrixYXZ(ShadowGroundRotation(actor, floor, ground)));
  const Math::Angles3 angles{
      actor.rotation_x,
      Math::WrapSigned16(static_cast<std::uint32_t>(actor.facing) +
                         actor.blend_level),
      actor.rotation_z};
  Math::Mat3 rotation = Math::MultiplyMatrixIntoRight(
      terrain, Math::BuildRotationMatrixYXZ(angles));
  const int extent =
      std::max(((root_offset.y - floor) >> kShadowHeightShift) + actor.radius,
               kShadowMinimumExtent);
  const std::uint32_t size = static_cast<std::uint32_t>(extent)
                             << kShadowExtentShift;
  rotation = Math::ScaleMatrixColumns(
      rotation,
      {ShadowColumnScale(size, actor.scale_y, actor.radius_shrink_rate),
       actor.scale_z,
       ShadowColumnScale(size, actor.scale_x, actor.radius_expand_rate)});
  transformer.LoadTransform(camera);
  const Math::Vec3 anchor = transformer.Transform(
      {root_offset.x, Math::WrapSigned16(static_cast<std::uint32_t>(floor)),
       root_offset.z});
  return {rotation, anchor};
}

[[nodiscard]] bool ShadowMeshFits(const ShadowMesh& mesh) {
  if (mesh.vertices.size() < kShadowVertexCount ||
      mesh.faces.size() < kShadowFaceCount) {
    return false;
  }
  return std::ranges::all_of(
      mesh.faces.first(kShadowFaceCount), [](std::uint32_t face) {
        for (int corner = 0; corner < kTriangleCorners; ++corner) {
          const std::uint32_t offset =
              (face >> (corner * CHAR_BIT)) & UINT8_MAX;
          if (offset % sizeof(std::int32_t) != 0 ||
              offset / sizeof(std::int32_t) >= kShadowVertexCount) {
            return false;
          }
        }
        return true;
      });
}

struct ShadowProjection {
  std::array<std::int32_t, kShadowVertexCount> screen{};
  std::array<std::uint32_t, kShadowVertexCount> depth{};
};

[[nodiscard]] ActorShadowStatus QueueShadowFaces(
    const ShadowProjection& projected, std::span<const std::uint32_t> faces,
    std::uint32_t light, ModelPacketQueue& queue) {
  std::uint32_t center = kShadowGouraudCommand | light * kShadowGrayChannels;
  std::uint32_t edge = (light >> 1) * kShadowGrayChannels;
  for (std::size_t face = 0; face < kShadowFaceCount; ++face) {
    if (face == kShadowBandFaces) {
      center = kShadowGouraudCommand | edge;
      edge = 0;
    } else if (face == 2 * kShadowBandFaces) {
      edge = center;
      center = kShadowGouraudCommand;
    }
    std::uint32_t nearest = kDepthLimit;
    std::array<std::int32_t, kTriangleCorners> screen{};
    for (std::size_t corner = 0; corner < kTriangleCorners; ++corner) {
      const std::size_t vertex =
          ((faces[face] >> (corner * CHAR_BIT)) & UINT8_MAX) /
          sizeof(std::int32_t);
      screen.at(corner) = projected.screen.at(vertex);
      nearest = std::min(nearest, projected.depth.at(vertex));
    }
    const std::uint32_t bucket =
        (nearest >> kShadowDepthShift) - kShadowBucketBias;
    if (NormalClip(screen[0], screen[1], screen[2]) > 0 ||
        bucket >= kModelOrderingLimit) {
      continue;
    }
    std::array<std::uint32_t, kShadowPacketWords> packet{};
    packet[1] = kShadowDrawMode;
    for (std::size_t corner = 0; corner < kTriangleCorners; ++corner) {
      const std::size_t color_word =
          kShadowFirstColorWord + corner * kShadowVertexWords;
      packet.at(color_word) = corner == 0 ? center : edge;
      packet.at(color_word + 1) = static_cast<std::uint32_t>(screen.at(corner));
    }
    if (!queue.Emit(packet, bucket)) {
      return ActorShadowStatus::kBufferFull;
    }
  }
  return ActorShadowStatus::kDrawn;
}
}  // namespace

// decomp: BATTLE.PRG func_800B3A68 (src/BATTLE/BATTLE.PRG/4B16C.s:84).
ModelProjectionResult ProjectModelVertices(const ModelCounts& counts,
                                           std::span<const std::byte> model,
                                           std::size_t matrix_count,
                                           ModelTransformer& transformer,
                                           std::span<std::int32_t> screens,
                                           std::span<std::int32_t> depths) {
  if (counts.bones == 0 || counts.groups == 0) {
    return {};
  }
  const std::optional<ModelSections> sections = ReadSections(counts, model);
  if (!sections.has_value()) {
    return {};
  }
  const ModelSections& layout = *sections;
  const std::size_t capacity =
      std::min({kModelVertexCapacity, screens.size(), depths.size()});
  const std::size_t bone_limit =
      std::min<std::size_t>(counts.bones, matrix_count);
  const std::size_t vertex_room =
      (layout.polygons - layout.vertices) / kVertexBytes;
  const auto group_word = [&](std::size_t group, std::size_t word) {
    return Read<std::uint16_t>(model, layout.groups + group * kGroupBytes +
                                          word * sizeof(std::uint16_t));
  };
  std::size_t previous = 0;
  for (std::size_t group = 0; group < counts.groups; ++group) {
    const std::size_t end = group_word(group, 1);
    if (group_word(group, 0) >= bone_limit || end < previous ||
        end > capacity || end > vertex_room) {
      return {};
    }
    previous = end;
  }
  std::size_t vertex = 0;
  std::int32_t combined_depth = 0;
  for (std::size_t group = 0; group < counts.groups; ++group) {
    const std::size_t end = group_word(group, 1);
    transformer.LoadBone(group_word(group, 0));
    for (; vertex < end; ++vertex) {
      const std::size_t at = layout.vertices + vertex * kVertexBytes;
      const ProjectedPoint point = transformer.Project(
          {Read<std::int16_t>(model, at),
           Read<std::int16_t>(model, at + sizeof(std::int16_t)),
           Read<std::int16_t>(model, at + 2 * sizeof(std::int16_t))});
      const std::int32_t depth = ClampDepth(point.depth);
      screens[vertex] = point.screen;
      depths[vertex] = depth;
      combined_depth |= depth;
    }
  }
  return {static_cast<std::uint32_t>(vertex), combined_depth};
}

bool ValidateModelData(const ModelCounts& counts, bool colored,
                       std::span<const std::byte> model) {
  const std::optional<ModelSections> sections = ReadSections(counts, model);
  if (!sections.has_value()) {
    return false;
  }
  const auto polygon_counts = PolygonCounts(counts);
  std::size_t offset = sections->polygons;
  for (std::size_t part = 0; part < kPolygonGroupCount; ++part) {
    for (std::uint32_t face = 0; face < polygon_counts.at(part); ++face) {
      PolygonSpan polygon;
      if (ReadPolygon(model, offset, colored, part, polygon) !=
          PolygonRead::kRead) {
        return false;
      }
      offset += polygon.data.size();
    }
  }
  return true;
}

std::uint32_t ModelAppearanceMaterial(std::uint32_t state) {
  if (state == 0) {
    return 0;
  }
  return kSemiTransparencyFlag |
         (state < kAppearanceBlendBoundary ? kAppearanceSubtractiveBlend
                                           : kAppearanceAdditiveBlend);
}

// decomp: BATTLE.PRG func_800B3CF0 (src/BATTLE/BATTLE.PRG/4B16C.s:254).
ModelDrawStatus DrawModelFlatPolygons(const ModelPolygonSource& source,
                                      std::uint32_t tint,
                                      const ModelSurface& surface,
                                      ModelPacketQueue& queue) {
  return DrawPolygons(source, {.colored = false, .tint = tint}, surface, queue);
}

// decomp: BATTLE.PRG func_800B4594 (src/BATTLE/BATTLE.PRG/4B16C.s:835).
ModelDrawStatus DrawModelGouraudPolygons(const ModelPolygonSource& source,
                                         std::uint32_t tint,
                                         const ModelSurface& surface,
                                         ModelPacketQueue& queue) {
  return DrawPolygons(source, {.colored = true, .tint = tint}, surface, queue);
}

// decomp: BATTLE.PRG func_800B516C (src/BATTLE/BATTLE.PRG/4B16C.s:1646).
ModelDrawStatus DrawModelFlatPolygonsWithOverrides(
    const ModelPolygonSource& source, std::uint32_t tint,
    const ModelSurface& surface, const ModelOverrides& overrides,
    ModelPacketQueue& queue) {
  return DrawPolygons(
      source,
      {.colored = false,
       .tint = tint,
       .pulse_mask = overrides.pulse_mask,
       .material = ModelAppearanceMaterial(overrides.appearance_state)},
      surface, queue);
}

// decomp: BATTLE.PRG func_800B5C30 (src/BATTLE/BATTLE.PRG/4B16C.s:2387).
ModelDrawStatus DrawModelGouraudPolygonsWithOverrides(
    const ModelPolygonSource& source, std::uint32_t tint,
    const ModelSurface& surface, const ModelOverrides& overrides,
    ModelPacketQueue& queue) {
  return DrawPolygons(
      source,
      {.colored = true,
       .tint = tint,
       .pulse_mask = overrides.pulse_mask,
       .material = ModelAppearanceMaterial(overrides.appearance_state)},
      surface, queue);
}

ModelDrawStatus DrawActorModelPolygons(const ModelPolygonSource& source,
                                       bool colored, std::uint32_t tint,
                                       const ModelSurface& surface,
                                       const ModelOverrides& overrides,
                                       ModelPacketQueue& queue) {
  // 800B51DC..800B51FC chooses the subtractive/additive page mode and
  // 800B5DD4..800B5DE0 repeats the tint at each Gouraud vertex.
  const std::uint32_t state = overrides.appearance_state;
  const bool fading = state >= kAppearanceFadeFirstState &&
                      state < kAppearanceFadeEndState &&
                      state != kAppearanceOpaqueState;
  const bool override =
      overrides.pulse_mask != 0 || fading || state == kAppearanceForcedOverride;
  return DrawPolygons(
      source,
      {.colored = colored,
       .tint = tint,
       .pulse_mask = overrides.pulse_mask,
       .material = override ? ModelAppearanceMaterial(state) : 0},
      surface, queue);
}

// decomp: BATTLE.PRG func_800B4B14 (src/BATTLE/BATTLE.PRG/4B16C.s:1208).
ActorShadowResult DrawActorGroundShadow(const ActorShadowSource& actor,
                                        const ModelTransform& camera,
                                        const ModelVertex& root_offset,
                                        const ShadowMesh& mesh,
                                        ShadowGround& ground,
                                        ModelTransformer& transformer,
                                        ModelPacketQueue& queue) {
  if (!ShadowMeshFits(mesh)) {
    return {ActorShadowStatus::kInvalidMesh, {}};
  }
  const int floor = std::max(ground.SupportHeight(0, 0), actor.height);
  transformer.LoadTransform(BuildShadowTransform(actor, camera, root_offset,
                                                 floor, ground, transformer));
  ShadowProjection projected;
  for (std::size_t vertex = 0; vertex < kShadowVertexCount; ++vertex) {
    const std::uint32_t packed = mesh.vertices[vertex];
    const ProjectedPoint point =
        transformer.Project({Math::WrapSigned16(packed), 0,
                             Math::WrapSigned16(packed >> kCoordinateBits)});
    projected.screen.at(vertex) = point.screen;
    projected.depth.at(vertex) =
        static_cast<std::uint32_t>(ClampDepth(point.depth));
  }
  ActorShadowResult result;
  result.anchor = {static_cast<std::int16_t>(ScreenX(projected.screen[0])),
                   static_cast<std::int16_t>(ScreenY(projected.screen[0])),
                   Math::WrapSigned16(projected.depth[0])};
  const std::uint32_t rgb = actor.blend_color;
  // The original blue addend is the upper halfword, not a masked byte.
  const int light =
      std::clamp(static_cast<int>(((rgb & UINT8_MAX) +
                                   ((rgb >> CHAR_BIT) & UINT8_MAX) * 2 +
                                   (rgb >> (2 * CHAR_BIT))) >>
                                  2) +
                     kShadowNeutralLight - actor.fade_red * kShadowFadeStep,
                 0, static_cast<int>(UINT8_MAX));
  result.status = QueueShadowFaces(projected, mesh.faces,
                                   static_cast<std::uint32_t>(light), queue);
  return result;
}
}  // namespace VS::Render
