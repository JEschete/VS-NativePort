#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

#include "Effects/Templates/RingCore.hpp"

// The closed ring template (decomp src/EFFECT/closedRings.h:41
// vs_closedRing): sixteen points per row around the whole circle, drawn as
// two bands of sixteen textured quads, the last closing the ring onto column
// 0 (DrawSixteenColumnRing). One copy of the texture spans four or sixteen
// columns.
namespace VS::Effects::Templates {
// One renderer command of a closed ring; false releases the slot.
bool ClosedRing(ClosedRingColumns columns, RingCommand command,
                std::int32_t packed, std::span<const RingPaletteEntry> palette,
                RingHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (ClosedRing.cpp): each binds the template to its
// overlay's texture repeat; RingsLive.hpp passes the overlay's palette.
namespace VS::Effects {
namespace Plg047 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg047
namespace Plg051 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg051
namespace Plg053 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg053
namespace Plg061 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg061
namespace Plg065 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg065
namespace Plg081 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg081
namespace Plg082 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg082
namespace Plg086 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg086
namespace Plg087 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg087
namespace Plg128 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg128
namespace Plg134 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg134
namespace Plg135 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg135
namespace Plg142 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg142
namespace Plg143 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg143
namespace Plg152 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg152
namespace Plg154 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg154
namespace Plg158 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg158
namespace Plg162 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg162
namespace Plg165 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg165
namespace Plg166 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg166
namespace Plg167 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg167
namespace Plg169 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg169
namespace Plg170 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg170
namespace Plg236 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg236
namespace Plg250 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg250
namespace Plg272 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg272
namespace Plg291 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg291
namespace Plg297 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg297
namespace Plg301 {
bool ClosedRing(Templates::RingCommand command, std::int32_t packed,
                std::span<const Templates::RingPaletteEntry> palette,
                Templates::RingHost& host);
}  // namespace Plg301
}  // namespace VS::Effects
