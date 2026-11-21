// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ClosedRing.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Effects/Templates/RingCore.hpp"

namespace VS::Effects::Templates {
// decomp: EFFECT vs_closedRing (src/EFFECT/closedRings.h:41).
bool ClosedRing(ClosedRingColumns columns, RingCommand command,
                std::int32_t packed, std::span<const RingPaletteEntry> palette,
                RingHost& host) {
  if (command != RingCommand::kTick) {
    return ApplyRingCommand(command, packed,
                            RingTextureSource::kSegmentsAndPositions, host);
  }
  std::optional<RingFrame> frame = BeginRingTick(palette, host);
  if (!frame) {
    return false;
  }
  DrawSixteenColumnRing(*frame, static_cast<std::size_t>(columns), host);
  return FinishRingTick(*frame, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG047.BIN vs_effect_vulcanLance_0 (src/EFFECT/closedRings.h:60; was
// vs_effect_vulcanLance).
bool Plg047::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG051.BIN vs_effect_exorcism_0 (src/EFFECT/closedRings.h:60; was
// vs_effect_exorcism).
bool Plg051::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG053.BIN vs_effect_explosionLv1_1 (src/EFFECT/closedRings.h:60; was
// func_800FA368). decomp: PLG054.BIN func_800FA368
// (src/EFFECT/PLG054.BIN/18.c:5). decomp: PLG055.BIN func_800FA368
// (src/EFFECT/PLG055.BIN/18.c:5). decomp: PLG077.BIN vs_effect_meteorLv1_4
// (src/EFFECT/closedRings.h:60; was func_800FC97C).
bool Plg053::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG061.BIN vs_effect_flameSphereLv1_3 (src/EFFECT/closedRings.h:60;
// was func_800FBB24).
bool Plg061::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG065.BIN vs_effect_gaeaStrikeLv1_1 (src/EFFECT/closedRings.h:60;
// was func_800FA7B0).
bool Plg065::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG081.BIN vs_effect_tornado_2 (src/EFFECT/closedRings.h:60; was
// func_800FB2DC).
bool Plg081::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG082.BIN vs_effect_thunderbolt_2 (src/EFFECT/closedRings.h:60; was
// func_800FC11C).
bool Plg082::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG086.BIN vs_effect_judgment_0 (src/EFFECT/closedRings.h:60; was
// func_800F98F8).
bool Plg086::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_1 (src/EFFECT/closedRings.h:60; was
// func_800FA588).
bool Plg087::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG128.BIN vs_effect_bloodySin_1 (src/EFFECT/closedRings.h:60; was
// func_800FA490).
bool Plg128::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG134.BIN vs_effect_wyrmScorn_1 (src/EFFECT/closedRings.h:60; was
// func_800FBA00).
bool Plg134::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG135.BIN vs_effect_rendingGale_0 (src/EFFECT/closedRings.h:60; was
// func_800F98E0).
bool Plg135::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG142.BIN vs_effect_adventSign_1 (src/EFFECT/closedRings.h:60; was
// func_800FA368).
bool Plg142::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG143.BIN vs_effect_mistralEdge_1 (src/EFFECT/closedRings.h:60; was
// func_800FA6F8).
bool Plg143::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG152.BIN vs_effect_riskbreak_2 (src/EFFECT/closedRings.h:60; was
// func_800FC11C).
bool Plg152::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG154.BIN vs_effect_trinityPulse_1 (src/EFFECT/closedRings.h:60; was
// func_800FA30C).
bool Plg154::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG158.BIN vs_effect_hexFlux_2 (src/EFFECT/closedRings.h:60; was
// func_800FB00C).
bool Plg158::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG162.BIN vs_effect_spiralScourge_3 (src/EFFECT/closedRings.h:60;
// was func_800FC21C).
bool Plg162::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG165.BIN vs_effect_deathWail_3 (src/EFFECT/closedRings.h:60; was
// func_800FD420).
bool Plg165::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG166.BIN vs_effect_sanctusFlare_2 (src/EFFECT/closedRings.h:60; was
// func_800FB32C).
bool Plg166::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG167.BIN vs_effect_lotusPalm_0 (src/EFFECT/closedRings.h:60; was
// func_800F98E0).
bool Plg167::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG169.BIN vs_effect_vermillionAura_2 (src/EFFECT/closedRings.h:60;
// was func_800FC624).
bool Plg169::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG170.BIN vs_effect_retribution_5 (src/EFFECT/closedRings.h:60; was
// func_800FD654).
bool Plg170::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG236.BIN vs_effect_plg236_0 (src/EFFECT/closedRings.h:60; was
// func_800F9800).
bool Plg236::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG250.BIN vs_effect_plg250_1 (src/EFFECT/closedRings.h:60; was
// func_800FA6BC).
bool Plg250::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG272.BIN vs_effect_plg272_1 (src/EFFECT/closedRings.h:60; was
// func_800FA368).
bool Plg272::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG291.BIN vs_effect_plg291_2 (src/EFFECT/closedRings.h:60; was
// func_800FC568).
bool Plg291::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG297.BIN vs_effect_plg297_1 (src/EFFECT/closedRings.h:60; was
// func_800FA48C).
bool Plg297::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG301.BIN vs_effect_plg301_1 (src/EFFECT/closedRings.h:60; was
// func_800FB5B4).
bool Plg301::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}
}  // namespace VS::Effects
