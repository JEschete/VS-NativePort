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
// decomp: PLG047.BIN vs_effect_vulcanLance (src/EFFECT/PLG047.BIN/0.c:4).
bool Plg047::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG051.BIN vs_effect_exorcism (src/EFFECT/PLG051.BIN/0.c:3).
bool Plg051::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG053.BIN func_800FA368 (src/EFFECT/PLG053.BIN/18.c:11).
// decomp: PLG054.BIN func_800FA368 (src/EFFECT/PLG054.BIN/18.c:5).
// decomp: PLG055.BIN func_800FA368 (src/EFFECT/PLG055.BIN/18.c:5).
// decomp: PLG077.BIN func_800FC97C (src/EFFECT/PLG077.BIN/18.c:352).
bool Plg053::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG061.BIN func_800FBB24 (src/EFFECT/PLG061.BIN/18.c:19).
bool Plg061::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG065.BIN func_800FA7B0 (src/EFFECT/PLG065.BIN/F8.c:11).
bool Plg065::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG081.BIN func_800FB2DC (src/EFFECT/PLG081.BIN/0.c:15).
bool Plg081::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG082.BIN func_800FC11C (src/EFFECT/PLG082.BIN/128.c:23).
bool Plg082::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG086.BIN func_800F98F8 (src/EFFECT/PLG086.BIN/F8.c:8).
bool Plg086::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG087.BIN func_800FA588 (src/EFFECT/PLG087.BIN/1F0.c:13).
bool Plg087::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG128.BIN func_800FA490 (src/EFFECT/PLG128.BIN/F8.c:49).
bool Plg128::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG134.BIN func_800FBA00 (src/EFFECT/PLG134.BIN/E0.c:18).
bool Plg134::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG135.BIN func_800F98E0 (src/EFFECT/PLG135.BIN/E0.c:8).
bool Plg135::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG142.BIN func_800FA368 (src/EFFECT/PLG142.BIN/18.c:11).
bool Plg142::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG143.BIN func_800FA6F8 (src/EFFECT/PLG143.BIN/0.c:12).
bool Plg143::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG152.BIN func_800FC11C (src/EFFECT/PLG152.BIN/128.c:23).
bool Plg152::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG154.BIN func_800FA30C (src/EFFECT/PLG154.BIN/0.c:13).
bool Plg154::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG158.BIN func_800FB00C (src/EFFECT/PLG158.BIN/0.c:17).
bool Plg158::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG162.BIN func_800FC21C (src/EFFECT/PLG162.BIN/0.c:343).
bool Plg162::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG165.BIN func_800FD420 (src/EFFECT/PLG165.BIN/F8.c:24).
bool Plg165::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG166.BIN func_800FB32C (src/EFFECT/PLG166.BIN/0.c:15).
bool Plg166::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG167.BIN func_800F98E0 (src/EFFECT/PLG167.BIN/E0.c:8).
bool Plg167::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG169.BIN func_800FC624 (src/EFFECT/PLG169.BIN/E0.c:22).
bool Plg169::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG170.BIN func_800FD654 (src/EFFECT/PLG170.BIN/18.c:29).
bool Plg170::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG236.BIN func_800F9800 (src/EFFECT/PLG236.BIN/0.c:8).
bool Plg236::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG250.BIN func_800FA6BC (src/EFFECT/PLG250.BIN/0.c:12).
bool Plg250::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kSixteen, command,
                               packed, palette, host);
}

// decomp: PLG272.BIN func_800FA368 (src/EFFECT/PLG272.BIN/18.c:11).
bool Plg272::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG291.BIN func_800FC568 (src/EFFECT/PLG291.BIN/F8.c:21).
bool Plg291::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG297.BIN func_800FA48C (src/EFFECT/PLG297.BIN/18.c:12).
bool Plg297::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}

// decomp: PLG301.BIN func_800FB5B4 (src/EFFECT/PLG301.BIN/110.c:20).
bool Plg301::ClosedRing(Templates::RingCommand command, std::int32_t packed,
                        std::span<const Templates::RingPaletteEntry> palette,
                        Templates::RingHost& host) {
  return Templates::ClosedRing(Templates::ClosedRingColumns::kFour, command,
                               packed, palette, host);
}
}  // namespace VS::Effects
