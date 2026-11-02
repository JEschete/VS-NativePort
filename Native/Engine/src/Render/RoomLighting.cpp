// Hand-maintained (tools/port/owned.csv).
#include "Render/RoomLighting.hpp"

#include <bit>
#include <climits>
#include <cstring>
#include <limits>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Render {
namespace {

constexpr std::uint32_t kNeutralLight = 0x808080;
constexpr std::size_t kHeaderWords = 3;
constexpr std::size_t kFloorCountWord = 1;
constexpr std::size_t kCeilingCountWord = 2;
constexpr std::size_t kColorChannels = 3;
constexpr unsigned kChannelBits = CHAR_BIT;
constexpr std::uint32_t kChannelMask = std::numeric_limits<std::uint8_t>::max();
constexpr std::size_t kCornerOffset = 8;
constexpr std::size_t kColorOffset = 20;
constexpr std::size_t kHalfBytes = sizeof(std::int16_t);
constexpr std::size_t kWordBytes = sizeof(std::uint32_t);

static_assert(kHeaderWords * kWordBytes == kRoomLightingHeaderBytes);
static_assert(kColorOffset + kRoomLightTriangleVertices * kWordBytes ==
              kRoomLightTriangleBytes);

std::uint32_t ReadWord(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    value |= std::to_integer<std::uint32_t>(bytes[offset + index])
             << (index * kChannelBits);
  }
  return value;
}

std::int16_t ReadHalf(std::span<const std::byte> bytes, std::size_t offset) {
  const auto low = std::to_integer<std::uint16_t>(bytes[offset]);
  const auto high = std::to_integer<std::uint16_t>(bytes[offset + 1]);
  return std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(low | (high << kChannelBits)));
}

RoomLightPoint ReadPoint(std::span<const std::byte> bytes, std::size_t offset) {
  return {ReadHalf(bytes, offset), ReadHalf(bytes, offset + kHalfBytes)};
}

// The original NCLIP: MAC0 keeps the low 32 bits of the cross product.
std::int32_t TriangleArea(RoomLightPoint first, RoomLightPoint second,
                          RoomLightPoint third) {
  const std::int64_t area =
      static_cast<std::int64_t>(second.x - first.x) * (third.z - first.z) -
      static_cast<std::int64_t>(second.z - first.z) * (third.x - first.x);
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(area));
}

RoomLightColor ColorFromWord(std::uint32_t word) {
  return {
      static_cast<std::uint8_t>(word & kChannelMask),
      static_cast<std::uint8_t>((word >> kChannelBits) & kChannelMask),
      static_cast<std::uint8_t>((word >> (2 * kChannelBits)) & kChannelMask),
      static_cast<std::uint8_t>(word >> (kColorChannels * kChannelBits))};
}

}  // namespace

RoomLightPoint UnpackRoomLightPoint(std::uint32_t packed) {
  return {Core::Math::WrapSigned16(packed),
          Core::Math::WrapSigned16(packed >> (kHalfBytes * CHAR_BIT))};
}

RoomLightTriangle DecodeRoomLightTriangle(
    std::span<const std::byte, kRoomLightTriangleBytes> record) {
  RoomLightTriangle triangle;
  triangle.minimum = ReadPoint(record, 0);
  triangle.maximum = {ReadHalf(record, 2 * kHalfBytes),
                      ReadHalf(record, kColorChannels * kHalfBytes)};
  for (std::size_t corner = 0; corner < kRoomLightTriangleVertices; ++corner) {
    triangle.corners[corner] =
        ReadPoint(record, kCornerOffset + corner * 2 * kHalfBytes);
    triangle.colors[corner] =
        ReadWord(record, kColorOffset + corner * kWordBytes);
  }
  return triangle;
}

// decomp: BATTLE.PRG func_80098C3C (src/BATTLE/BATTLE.PRG/2EA3C.s:1857).
std::optional<RoomLightRgb> InterpolateRoomLightTriangle(
    RoomLightPoint point, const RoomLightTriangle& triangle) {
  const auto& corners = triangle.corners;
  const std::int32_t area = TriangleArea(corners[0], corners[2], corners[1]);
  if (area <= 0) return std::nullopt;
  const std::int32_t first = TriangleArea(point, corners[2], corners[1]);
  const std::int32_t second = TriangleArea(point, corners[1], corners[0]);
  const auto third = std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(area) - static_cast<std::uint32_t>(first) -
      static_cast<std::uint32_t>(second));
  if (first < 0 || second < 0 || third < 0) return std::nullopt;
  std::array<std::uint8_t, kColorChannels> channels{};
  for (std::size_t channel = 0; channel < kColorChannels; ++channel) {
    const auto shift = static_cast<unsigned>(channel * kChannelBits);
    // The original multiplies and sums in unsigned 32-bit registers.
    const std::uint32_t total = ((triangle.colors[0] >> shift) & kChannelMask) *
                                    static_cast<std::uint32_t>(first) +
                                ((triangle.colors[2] >> shift) & kChannelMask) *
                                    static_cast<std::uint32_t>(second) +
                                ((triangle.colors[1] >> shift) & kChannelMask) *
                                    static_cast<std::uint32_t>(third);
    std::uint32_t value = total / static_cast<std::uint32_t>(area);
    if (value >
        static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
      value = 0;
    }
    channels[channel] =
        static_cast<std::uint8_t>(value > kChannelMask ? kChannelMask : value);
  }
  return RoomLightRgb{channels[0], channels[1], channels[2]};
}

// decomp: BATTLE.PRG func_80098B38 (src/BATTLE/BATTLE.PRG/2EA3C.s:1782).
RoomLightColor SampleRoomLighting(std::span<const std::byte> lighting,
                                  RoomLightPoint point, RoomLightLayer layer,
                                  RoomLightColor current) {
  if (lighting.size() < kRoomLightingHeaderBytes) {
    return ColorFromWord(kNeutralLight);
  }
  const RoomLightColor fallback = ColorFromWord(ReadWord(lighting, 0));
  const std::uint32_t floor_count =
      ReadWord(lighting, kFloorCountWord * kWordBytes);
  const bool ceiling = layer == RoomLightLayer::kCeiling;
  const std::uint32_t count =
      ceiling ? ReadWord(lighting, kCeilingCountWord * kWordBytes)
              : floor_count;
  const std::size_t available =
      (lighting.size() - kRoomLightingHeaderBytes) / kRoomLightTriangleBytes;
  const std::size_t first = ceiling ? floor_count : 0;
  if (first > available || count > available - first) return fallback;
  for (std::size_t index = first; index < first + count; ++index) {
    const RoomLightTriangle triangle = DecodeRoomLightTriangle(
        lighting
            .subspan(kRoomLightingHeaderBytes + index * kRoomLightTriangleBytes)
            .first<kRoomLightTriangleBytes>());
    if (point.x < triangle.minimum.x || point.x > triangle.maximum.x ||
        point.z < triangle.minimum.z || point.z > triangle.maximum.z) {
      continue;
    }
    if (const auto rgb = InterpolateRoomLightTriangle(point, triangle)) {
      return {rgb->red, rgb->green, rgb->blue, current.code};
    }
  }
  return fallback;
}

}  // namespace VS::Render
