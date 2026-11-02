#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Render {

inline constexpr std::size_t kRoomLightTriangleVertices = 3;
inline constexpr std::size_t kRoomLightTriangleBytes = 32;
inline constexpr std::size_t kRoomLightingHeaderBytes = 12;

struct RoomLightPoint {
  std::int16_t x = 0;
  std::int16_t z = 0;

  friend bool operator==(const RoomLightPoint&,
                         const RoomLightPoint&) = default;
};

// One imported lighting triangle: inclusive bounds, three corners and their
// packed 0x00BBGGRR colours.
struct RoomLightTriangle {
  RoomLightPoint minimum;
  RoomLightPoint maximum;
  std::array<RoomLightPoint, kRoomLightTriangleVertices> corners{};
  std::array<std::uint32_t, kRoomLightTriangleVertices> colors{};

  friend bool operator==(const RoomLightTriangle&,
                         const RoomLightTriangle&) = default;
};

struct RoomLightRgb {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  friend bool operator==(const RoomLightRgb&, const RoomLightRgb&) = default;
};

// The room's four-byte colour code: three channels and the primitive code
// byte, which an interpolated sample leaves unchanged.
struct RoomLightColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t code = 0;

  friend bool operator==(const RoomLightColor&,
                         const RoomLightColor&) = default;
};

enum class RoomLightLayer : std::uint8_t { kFloor, kCeiling };

// The low half is x and the high half z, both signed 16-bit.
[[nodiscard]] RoomLightPoint UnpackRoomLightPoint(std::uint32_t packed);

// Decodes one little-endian 32-byte triangle record.
[[nodiscard]] RoomLightTriangle DecodeRoomLightTriangle(
    std::span<const std::byte, kRoomLightTriangleBytes> record);

[[nodiscard]] std::optional<RoomLightRgb> InterpolateRoomLightTriangle(
    RoomLightPoint point, const RoomLightTriangle& triangle);

// Samples the room lighting block; an absent or malformed block yields the
// neutral grey with a zero code byte.
[[nodiscard]] RoomLightColor SampleRoomLighting(
    std::span<const std::byte> lighting, RoomLightPoint point,
    RoomLightLayer layer, RoomLightColor current);

}  // namespace VS::Render
