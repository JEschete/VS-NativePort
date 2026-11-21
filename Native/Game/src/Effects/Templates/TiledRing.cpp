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
// decomp: PLG044.BIN vs_effect_solidShock_0 (src/EFFECT/closedRings.h:60; was
// vs_effect_solidShock).
bool Plg044::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG049.BIN vs_spiritSurge_2 (src/EFFECT/closedRings.h:60; was
// func_800FC624).
bool Plg049::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG069.BIN vs_effect_avalancheLv1_2 (src/EFFECT/closedRings.h:60; was
// func_800FC50C).
bool Plg069::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG073.BIN vs_effect_radialSurgeLv1_0 (src/EFFECT/closedRings.h:60;
// was func_800F98F8). decomp: PLG074.BIN func_800F98F8
// (src/EFFECT/PLG074.BIN/F8.c:3).
bool Plg073::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG085.BIN vs_effect_acidFlow_1 (src/EFFECT/closedRings.h:60; was
// func_800FA368).
bool Plg085::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG137.BIN vs_effect_cherryRonde_3 (src/EFFECT/closedRings.h:60; was
// func_800FD074).
bool Plg137::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG138.BIN vs_effect_papillonReel_3 (src/EFFECT/closedRings.h:60; was
// func_800FBE44).
bool Plg138::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG139.BIN vs_effect_sunder_0 (src/EFFECT/closedRings.h:60; was
// func_800F9818).
bool Plg139::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG143.BIN vs_effect_mistralEdge_0 (src/EFFECT/closedRings.h:60; was
// func_800F9800).
bool Plg143::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_0 (src/EFFECT/closedRings.h:60; was
// func_800F99D8).
bool Plg146::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG148.BIN vs_effect_accursedUmbra_1 (src/EFFECT/closedRings.h:60;
// was func_800FBA00).
bool Plg148::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG150.BIN vs_effect_emeticBomb_0 (src/EFFECT/closedRings.h:60; was
// func_800F9818).
bool Plg150::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG160.BIN vs_effect_scytheWind_0 (src/EFFECT/closedRings.h:60; was
// func_800F9800).
bool Plg160::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG242.BIN vs_effect_plg242_0 (src/EFFECT/closedRings.h:60; was
// func_800F98F8).
bool Plg242::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}

// decomp: PLG265.BIN vs_effect_plg265_0 (src/EFFECT/closedRings.h:60; was
// func_800F9AE8).
bool Plg265::TiledRing(Templates::RingCommand command, std::int32_t packed,
                       std::span<const Templates::RingPaletteEntry> palette,
                       Templates::RingHost& host) {
  return Templates::TiledRing(command, packed, palette, host);
}
}  // namespace VS::Effects
