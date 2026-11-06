#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

#include "Effects/Templates/RingCore.hpp"

// The tiled ring template (decomp src/EFFECT/tiledRings.h:35 vs_tiledRing):
// the closed ring's sixteen points per row and closing quad, with the whole
// texture stretched over every quad.
namespace VS::Effects::Templates {
// Every quad shows the whole texture: u runs from u to u + w (wrapping in a
// byte), which is DrawSixteenColumnRing with a one-column texture span.
inline constexpr std::size_t kTiledRingTextureColumns = 1;

// One renderer command of a tiled ring; false releases the slot.
bool TiledRing(RingCommand command, std::int32_t packed,
               std::span<const RingPaletteEntry> palette, RingHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (TiledRing.cpp); RingsLive.hpp passes each overlay's
// palette.
namespace VS::Effects {
namespace Plg044 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg044
namespace Plg049 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg049
namespace Plg069 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg069
namespace Plg073 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg073
namespace Plg085 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg085
namespace Plg137 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg137
namespace Plg138 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg138
namespace Plg139 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg139
namespace Plg143 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg143
namespace Plg146 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg146
namespace Plg148 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg148
namespace Plg150 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg150
namespace Plg160 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg160
namespace Plg242 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg242
namespace Plg265 {
bool TiledRing(Templates::RingCommand command, std::int32_t packed,
               std::span<const Templates::RingPaletteEntry> palette,
               Templates::RingHost& host);
}  // namespace Plg265
}  // namespace VS::Effects
