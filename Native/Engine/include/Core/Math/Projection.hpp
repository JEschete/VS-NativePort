#pragma once

#include <array>
#include <cstdint>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

// The GTE's rotate-translate (RTV0TR) and rotate-translate-perspective (RTPS)
// as functions of their inputs (MA-11, EC-03). The caller passes the
// transform, the projection and the vertex and receives every result the
// hardware left in its registers, so no shared register, FIFO or flag carries
// state between calls and independent views cannot interfere. The libgte
// bridge (Core/Gte.hpp) runs this same arithmetic on its shared state.
namespace VS::Core::Math {
// The projection distance before any SetGeomScreen (the bridge's reset value).
inline constexpr std::uint16_t kDefaultProjectionDistance = 1000;
// The screen offset registers hold whole pixels in their high halfword.
inline constexpr unsigned kScreenOffsetShift = 16;

// The projection registers: the screen offset in 16.16 (OFX/OFY), the
// projection distance (H) and the depth cue coefficient (DQA) and offset (DQB).
struct PerspectiveProjection {
  std::int32_t offset_x = 0;
  std::int32_t offset_y = 0;
  std::uint16_t distance = kDefaultProjectionDistance;
  std::int16_t depth_cue_coefficient = 0;
  std::int32_t depth_cue_offset = 0;

  friend bool operator==(const PerspectiveProjection&,
                         const PerspectiveProjection&) = default;
};

// SetGeomOffset's whole pixels as the 16.16 offset holds them (the low
// halfword survives), and GetGeomOffset's conversion back.
[[nodiscard]] constexpr std::int32_t ScreenOffsetFixed(
    std::int32_t pixels) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(pixels)
                                   << kScreenOffsetShift);
}
[[nodiscard]] constexpr std::int32_t ScreenOffsetPixels(
    std::int32_t fixed) noexcept {
  return fixed >> kScreenOffsetShift;
}

// RTV0TR's results: MAC1-3, IR1-3 (each MAC saturated to a signed halfword)
// and FLAG.
struct RotatedVertex {
  Vec3 mac{};
  std::array<std::int16_t, kMatrixOrder> ir{};
  std::uint32_t flag = 0;
};

// RTPS's results: the rotation's, the screen point (SX2/SY2), its depth (SZ3),
// the depth cue (MAC0 and IR0) and FLAG for the whole operation.
struct PerspectiveVertex {
  Vec3 mac{};
  std::array<std::int16_t, kMatrixOrder> ir{};
  std::int16_t screen_x = 0;
  std::int16_t screen_y = 0;
  std::uint16_t depth = 0;
  std::int32_t mac0 = 0;
  std::int16_t ir0 = 0;
  std::uint32_t flag = 0;

  // SXY2 as RotTransPers stores it: x in the low halfword, y in the high.
  [[nodiscard]] constexpr std::int32_t PackedScreen() const noexcept {
    return static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(static_cast<std::uint16_t>(screen_y))
         << kScreenOffsetShift) |
        static_cast<std::uint16_t>(screen_x));
  }
  // SZ3 / 4, which RotTransPers returns.
  [[nodiscard]] constexpr std::int32_t QuarterDepth() const noexcept {
    return depth >> 2;
  }
};

// RTV0TR: MAC = (T * 4096 + R * v) >> 12 with every partial sum flagged and
// wrapped to the GTE's signed 44 bits, IR saturated from MAC. Math::
// RotateTranslate is the MAC alone, without the wrap or the flags.
[[nodiscard]] RotatedVertex RotateTranslateVertex(const Transform3& transform,
                                                  Position3 vertex) noexcept;

// RTPS (SDK RotTransPers, libgte/SMP_02.s at 0x80041814): RTV0TR, then the
// perspective divide, the screen offset and the depth cue.
[[nodiscard]] PerspectiveVertex ProjectVertex(
    const Transform3& transform, const PerspectiveProjection& projection,
    Position3 vertex) noexcept;
}  // namespace VS::Core::Math
