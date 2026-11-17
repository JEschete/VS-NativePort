#include "Effects/Templates/RepeatingRings.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace VS::Effects::Templates {
namespace {
constexpr auto kRepeatColumns = std::to_underlying(ClosedRingColumns::kFour);

[[nodiscard]] RingQuadUvs TransposedUvs(const RingTexture& texture,
                                        std::int32_t fraction, std::size_t band,
                                        std::size_t column) {
  const auto across = RingRowCoordinates(texture.u, texture.w, fraction);
  const auto first =
      RingTextureStep(texture.v, texture.h, column, kClosedRingColumns);
  const auto second =
      RingTextureStep(texture.v, texture.h, column + 1, kClosedRingColumns);
  return {RingUv{across.at(band), first}, RingUv{across.at(band + 1), first},
          RingUv{across.at(band), second}, RingUv{across.at(band + 1), second}};
}

void SubmitForeground(const RingFrame& frame, RingProjectedGrid grid,
                      const RingQuadCorners& corners, const RingQuadUvs& uvs,
                      const RingTexture& texture, RingHost& host) {
  RingQuad quad{.uvs = uvs, .clut = texture.clut, .tpage = texture.tpage};
  std::int32_t sum = 0;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    const auto at = corners.at(corner);
    const auto& point = grid.points[(at.row * grid.columns) + at.column];
    quad.screens.at(corner) = point.screen;
    quad.colors.at(corner) = frame.colors.at(at.row);
    sum = Core::Math::WrappingAdd(sum, point.depth);
  }
  const auto depth = Core::Math::R3000Quotient(
      sum, static_cast<std::int32_t>(kRingQuadCorners));
  if (depth < kRingFarDepth && depth > frame.inputs.near_clip) {
    host.Submit(quad, kForegroundRingDepth);
  }
}

void DrawSpecialRing(const RingFrame& frame, RingOrientation orientation,
                     bool foreground, RingHost& host) {
  const auto points = ClosedRingPoints(frame);
  std::array<RingProjection, kRingRows * kClosedRingColumns> projected{};
  ProjectRingPoints(frame, points, projected, host);
  const auto texture = host.AdvanceTexture();
  if (!texture) return;
  const RingProjectedGrid grid{projected, kClosedRingColumns};
  for (std::size_t band = 0; band < kRingBands; ++band) {
    for (std::size_t column = 0; column < kClosedRingColumns; ++column) {
      auto corners = ClosedRingCorners(band, column);
      auto uvs = SixteenColumnRingUvs(kRepeatColumns, *texture, frame.fraction,
                                      band, column);
      if (orientation == RingOrientation::kTransposed) {
        std::swap(corners[1], corners[2]);
        uvs = TransposedUvs(*texture, frame.fraction, band, column);
      }
      if (foreground) {
        SubmitForeground(frame, grid, corners, uvs, *texture, host);
      } else {
        SubmitRingQuad(frame, grid, corners, uvs, *texture, host);
      }
    }
  }
}
}  // namespace

// decomp: PLG077.BIN 18.c:37-339; PLG162.BIN 0.c:32-334;
// PLG300.BIN 110.c:355-656. The complete lifecycle shares RingCore's motion,
// closed sixteen-column geometry and child-event path; UV/corner orientation
// and the foreground ordering entry are the three original body policies.
bool UpdateRepeatingRing(RepeatingRingStyle style, RingCommand command,
                         std::int32_t packed,
                         std::span<const RingPaletteEntry> palette,
                         RingHost& host) {
  if (command != RingCommand::kTick) {
    return ApplyRingCommand(command, packed,
                            RingTextureSource::kSegmentsAndPositions, host);
  }
  if (palette.size() <= kRingPaletteMask) return false;
  auto frame = BeginRingTick(palette, host);
  if (!frame) return false;
  if (style == RepeatingRingStyle::kRepeating) {
    DrawSixteenColumnRing(*frame, kRepeatColumns, host);
  } else {
    DrawSpecialRing(*frame,
                    style == RepeatingRingStyle::kTransposed
                        ? RingOrientation::kTransposed
                        : RingOrientation::kUpright,
                    style == RepeatingRingStyle::kForeground, host);
  }
  return FinishRingTick(*frame, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
using Templates::RepeatingRingStyle;
using Templates::RingCommand;
using Templates::RingHost;
using Templates::RingPaletteEntry;

// decomp: F01920 PLG052.BIN func_800F9800 (src/EFFECT/PLG052.BIN/0.c:3,
// INCLUDE_ASM). Independently compared against PLG052's original instructions;
// the generated predecessor borrowed PLG077's C body.
bool Plg052::RepeatingRing(RingCommand command, std::int32_t packed,
                           std::span<const RingPaletteEntry> palette,
                           RingHost& host) {
  constexpr auto kPlg052Style = RepeatingRingStyle::kRepeating;
  return Templates::UpdateRepeatingRing(kPlg052Style, command, packed, palette,
                                        host);
}

// decomp: F01998 PLG077.BIN func_800FA324 (src/EFFECT/PLG077.BIN/18.c:37).
bool Plg077::RepeatingRing(RingCommand command, std::int32_t packed,
                           std::span<const RingPaletteEntry> palette,
                           RingHost& host) {
  constexpr auto kPlg077Style = RepeatingRingStyle::kRepeating;
  return Templates::UpdateRepeatingRing(kPlg077Style, command, packed, palette,
                                        host);
}

// decomp: F02209 PLG162.BIN func_800F9800 (src/EFFECT/PLG162.BIN/0.c:32).
bool Plg162::TransposedRing(RingCommand command, std::int32_t packed,
                            std::span<const RingPaletteEntry> palette,
                            RingHost& host) {
  constexpr auto kPlg162Style = RepeatingRingStyle::kTransposed;
  return Templates::UpdateRepeatingRing(kPlg162Style, command, packed, palette,
                                        host);
}

// decomp: F02289 PLG236.BIN func_800FB62C (src/EFFECT/PLG236.BIN/0.c:41).
bool Plg236::TransposedRing(RingCommand command, std::int32_t packed,
                            std::span<const RingPaletteEntry> palette,
                            RingHost& host) {
  constexpr auto kPlg236Style = RepeatingRingStyle::kTransposed;
  return Templates::UpdateRepeatingRing(kPlg236Style, command, packed, palette,
                                        host);
}

// decomp: F02323 PLG263.BIN func_800FB0CC (src/EFFECT/PLG263.BIN/110.c:40).
bool Plg263::TransposedRing(RingCommand command, std::int32_t packed,
                            std::span<const RingPaletteEntry> palette,
                            RingHost& host) {
  constexpr auto kPlg263Style = RepeatingRingStyle::kTransposed;
  return Templates::UpdateRepeatingRing(kPlg263Style, command, packed, palette,
                                        host);
}

// decomp: F02397 PLG300.BIN func_800FC228 (src/EFFECT/PLG300.BIN/110.c:48).
bool Plg300::RepeatingRing(RingCommand command, std::int32_t packed,
                           std::span<const RingPaletteEntry> palette,
                           RingHost& host) {
  constexpr auto kPlg300Style = RepeatingRingStyle::kRepeating;
  return Templates::UpdateRepeatingRing(kPlg300Style, command, packed, palette,
                                        host);
}

// decomp: F02398 PLG300.BIN func_800FD198 (src/EFFECT/PLG300.BIN/110.c:355).
bool Plg300::ForegroundRepeatingRing(RingCommand command, std::int32_t packed,
                                     std::span<const RingPaletteEntry> palette,
                                     RingHost& host) {
  constexpr auto kPlg300ForegroundStyle = RepeatingRingStyle::kForeground;
  return Templates::UpdateRepeatingRing(kPlg300ForegroundStyle, command, packed,
                                        palette, host);
}
}  // namespace VS::Effects
