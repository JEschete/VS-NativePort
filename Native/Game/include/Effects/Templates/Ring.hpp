#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Effects/Templates/RingCore.hpp"

// The open ring template (decomp src/EFFECT/rings.h:26 vs_ring): nine points
// per row spread over an arc by the direction jitter, drawn as two bands of
// eight textured quads. RingCore.hpp holds the tick it shares with the closed
// and tiled rings.
namespace VS::Effects::Templates {
inline constexpr std::size_t kOpenRingSegments = kOpenRingColumns - 1;

// The open ring's points, row by row. Column c sits at the ring angle plus
// jitter.x * c / 8 + (jitter.y * c / 8 - jitter.y / 2); a frame without a
// jitter sample spreads nothing.
[[nodiscard]] std::array<RingPoint, kRingRows * kOpenRingColumns>
OpenRingPoints(const RingFrame& frame);

// The corners of the quad at `column` of `band`: the upright ring lists
// them row-major, the transposed ring column-major.
[[nodiscard]] RingQuadCorners OpenRingCorners(RingOrientation orientation,
                                              std::size_t band,
                                              std::size_t column) noexcept;

// The texture coordinates of that quad. The upright ring runs u along the
// ring and v across it (inner edge, inner-extent fraction, outer edge); the
// transposed ring swaps the roles of u and v.
[[nodiscard]] RingQuadUvs OpenRingUvs(RingOrientation orientation,
                                      const RingTexture& texture,
                                      std::int32_t fraction, std::size_t band,
                                      std::size_t column) noexcept;

// One renderer command of an open ring; false releases the slot.
bool Ring(RingOrientation orientation, RingCommand command, std::int32_t packed,
          std::span<const RingPaletteEntry> palette, RingHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (Ring.cpp): each binds the template to its overlay's
// texture orientation; RingsLive.hpp passes the overlay's palette.
namespace VS::Effects {
namespace Plg049 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg049
namespace Plg050 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg050
namespace Plg061 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg061
namespace Plg065 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg065
namespace Plg073 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg073
namespace Plg081 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg081
namespace Plg129 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
bool Ring2(Templates::RingCommand command, std::int32_t packed,
           std::span<const Templates::RingPaletteEntry> palette,
           Templates::RingHost& host);
}  // namespace Plg129
namespace Plg138 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg138
namespace Plg142 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg142
namespace Plg146 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg146
namespace Plg147 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg147
namespace Plg149 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg149
namespace Plg150 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg150
namespace Plg153 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg153
namespace Plg160 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg160
namespace Plg161 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
bool Ring2(Templates::RingCommand command, std::int32_t packed,
           std::span<const Templates::RingPaletteEntry> palette,
           Templates::RingHost& host);
}  // namespace Plg161
namespace Plg162 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg162
namespace Plg166 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg166
namespace Plg169 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg169
namespace Plg170 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg170
namespace Plg198 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg198
namespace Plg236 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg236
namespace Plg263 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg263
namespace Plg297 {
bool Ring(Templates::RingCommand command, std::int32_t packed,
          std::span<const Templates::RingPaletteEntry> palette,
          Templates::RingHost& host);
}  // namespace Plg297
}  // namespace VS::Effects
