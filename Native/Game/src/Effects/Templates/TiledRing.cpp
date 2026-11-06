// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/TiledRing.hpp"

#include <cstdint>
#include <optional>
#include <span>

#include "Effects/Templates/RingCore.hpp"

namespace VS::Effects::Templates {
// decomp: EFFECT vs_tiledRing (src/EFFECT/tiledRings.h:35).
bool TiledRing(RingCommand command, std::int32_t packed,
               std::span<const RingPaletteEntry> palette, RingHost& host) {
  if (command != RingCommand::kTick) {
    return ApplyRingCommand(command, packed,
                            RingTextureSource::kSegmentsAndPositions, host);
  }
  std::optional<RingFrame> frame = BeginRingTick(palette, host);
  if (!frame) {
    return false;
  }
  DrawSixteenColumnRing(*frame, kTiledRingTextureColumns, host);
  return FinishRingTick(*frame, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG044.BIN vs_effect_solidShock (src/EFFECT/PLG044.BIN/0.c:4).
bool Plg044::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG049.BIN func_800FC624 (src/EFFECT/PLG049.BIN/E0.c:17).
bool Plg049::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG069.BIN func_800FC50C (src/EFFECT/PLG069.BIN/E0.c:17).
bool Plg069::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG073.BIN func_800F98F8 (src/EFFECT/PLG073.BIN/F8.c:2).
// decomp: PLG074.BIN func_800F98F8 (src/EFFECT/PLG074.BIN/F8.c:3).
bool Plg073::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG085.BIN func_800FA368 (src/EFFECT/PLG085.BIN/18.c:5).
bool Plg085::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG137.BIN func_800FD074 (src/EFFECT/PLG137.BIN/F8.c:20).
bool Plg137::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG138.BIN func_800FBE44 (src/EFFECT/PLG138.BIN/18.c:12).
bool Plg138::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG139.BIN func_800F9818 (src/EFFECT/PLG139.BIN/18.c:2).
bool Plg139::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG143.BIN func_800F9800 (src/EFFECT/PLG143.BIN/0.c:8).
bool Plg143::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG146.BIN func_800F99D8 (src/EFFECT/PLG146.BIN/1D8.c:2).
bool Plg146::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG148.BIN func_800FBA00 (src/EFFECT/PLG148.BIN/E0.c:12).
bool Plg148::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG150.BIN func_800F9818 (src/EFFECT/PLG150.BIN/18.c:2).
bool Plg150::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG160.BIN func_800F9800 (src/EFFECT/PLG160.BIN/0.c:2).
bool Plg160::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG242.BIN func_800F98F8 (src/EFFECT/PLG242.BIN/F8.c:2).
bool Plg242::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG265.BIN func_800F9AE8 (src/EFFECT/PLG265.BIN/2E8.c:2).
bool Plg265::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}
}  // namespace VS::Effects
