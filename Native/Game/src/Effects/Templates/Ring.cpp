// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/Ring.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/RingCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

constexpr std::int32_t kJitterCentre = 2;
}  // namespace

std::array<RingPoint, kRingRows * kOpenRingColumns> OpenRingPoints(
    const RingFrame& frame) {
  std::array<RingPoint, kRingRows * kOpenRingColumns> points{};
  const math::Vec3 origin = frame.scratch.position;
  const math::Vec3 jitter =
      frame.scratch.direction_jitter.value_or(math::Vec3{});
  const auto segments = static_cast<std::int32_t>(kOpenRingSegments);
  // decomp: src/EFFECT/rings.h angleB, (-jitter.vy) / 2 truncated toward zero.
  const std::int32_t spread_start =
      math::R3000Quotient(math::WrappingNegate(jitter.y), kJitterCentre);
  for (std::size_t column = 0; column < kOpenRingColumns; ++column) {
    const auto step = static_cast<std::int32_t>(column);
    const std::int32_t along = math::WrappingAdd(
        frame.state.angle,
        math::R3000Quotient(math::WrappingMultiply(jitter.x, step), segments));
    const std::int32_t spread = math::WrappingAdd(
        spread_start,
        math::R3000Quotient(math::WrappingMultiply(jitter.y, step), segments));
    const std::int32_t angle = math::WrappingAdd(along, spread);
    const std::int32_t cosine = math::Cosine(angle);
    const std::int32_t sine = math::Sine(angle);
    for (std::size_t row = 0; row < kRingRows; ++row) {
      const std::int32_t radius = frame.radius.at(row);
      points.at((row * kOpenRingColumns) + column) =
          RingPointAt({math::WrappingAdd(origin.x, RingFixed(radius, cosine)),
                       math::WrappingAdd(frame.height.at(row), origin.y),
                       math::WrappingAdd(origin.z, RingFixed(radius, sine))});
    }
  }
  return points;
}

RingQuadCorners OpenRingCorners(RingOrientation orientation, std::size_t band,
                                std::size_t column) noexcept {
  if (orientation == RingOrientation::kTransposed) {
    return {RingCorner{band, column}, RingCorner{band + 1, column},
            RingCorner{band, column + 1}, RingCorner{band + 1, column + 1}};
  }
  return {RingCorner{band, column}, RingCorner{band, column + 1},
          RingCorner{band + 1, column}, RingCorner{band + 1, column + 1}};
}

RingQuadUvs OpenRingUvs(RingOrientation orientation, const RingTexture& texture,
                        std::int32_t fraction, std::size_t band,
                        std::size_t column) noexcept {
  RingQuadUvs uvs{};
  if (band >= kRingBands || column >= kOpenRingSegments) {
    return uvs;
  }
  const bool transposed = orientation == RingOrientation::kTransposed;
  // The coordinate along the ring steps through eighths of the texture; the
  // one across it takes the row's edge or inner-extent fraction.
  const std::array<std::uint8_t, kRingRows> across =
      transposed ? RingRowCoordinates(texture.u, texture.w, fraction)
                 : RingRowCoordinates(texture.v, texture.h, fraction);
  const RingQuadCorners corners = OpenRingCorners(orientation, band, column);
  for (std::size_t corner = 0; corner < kRingQuadCorners; ++corner) {
    const RingCorner at = corners[corner];
    if (transposed) {
      uvs[corner] = {.u = across[at.row],
                     .v = RingTextureStep(texture.v, texture.h, at.column,
                                          kOpenRingSegments)};
    } else {
      uvs[corner] = {.u = RingTextureStep(texture.u, texture.w, at.column,
                                          kOpenRingSegments),
                     .v = across[at.row]};
    }
  }
  return uvs;
}

// decomp: EFFECT vs_ring (src/EFFECT/rings.h:26).
bool Ring(RingOrientation orientation, RingCommand command, std::int32_t packed,
          std::span<const RingPaletteEntry> palette, RingHost& host) {
  if (command != RingCommand::kTick) {
    return ApplyRingCommand(command, packed,
                            RingTextureSource::kTextureAndAnimation, host);
  }
  std::optional<RingFrame> frame = BeginRingTick(palette, host);
  if (!frame) {
    return false;
  }
  const RingDefinition& definition = frame->inputs.definition;
  frame->scratch.direction_jitter = SampleRingRange(
      definition.direction_jitter, definition.curves.direction_jitter,
      frame->state.age, host);
  const std::array<RingPoint, kRingRows * kOpenRingColumns> points =
      OpenRingPoints(*frame);
  std::array<RingProjection, kRingRows * kOpenRingColumns> projections{};
  ProjectRingPoints(*frame, points, projections, host);
  if (const std::optional<RingTexture> texture = host.AdvanceTexture()) {
    const RingProjectedGrid grid{.points = projections,
                                 .columns = kOpenRingColumns};
    for (std::size_t band = 0; band < kRingBands; ++band) {
      for (std::size_t column = 0; column < kOpenRingSegments; ++column) {
        SubmitRingQuad(
            *frame, grid, OpenRingCorners(orientation, band, column),
            OpenRingUvs(orientation, *texture, frame->fraction, band, column),
            *texture, host);
      }
    }
  }
  return FinishRingTick(*frame, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG049.BIN vs_spiritSurge_1 (src/EFFECT/rings.h:22; was
// func_800FBA00).
bool Plg049::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG050.BIN vs_effect_darkChant_0 (src/EFFECT/rings.h:22; was
// func_800F98E0).
bool Plg050::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG061.BIN vs_effect_flameSphereLv1_2 (src/EFFECT/rings.h:22; was
// func_800FAF00).
bool Plg061::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG065.BIN vs_effect_gaeaStrikeLv1_2 (src/EFFECT/rings.h:22; was
// func_800FB71C).
bool Plg065::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG073.BIN vs_effect_radialSurgeLv1_1 (src/EFFECT/rings.h:22; was
// func_800FA7F0). decomp: PLG074.BIN func_800FA7F0
// (src/EFFECT/PLG074.BIN/F8.c:5).
bool Plg073::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG081.BIN vs_effect_tornado_1 (src/EFFECT/rings.h:22; was
// func_800FA6B8).
bool Plg081::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG129.BIN vs_effect_lastAscension_0 (src/EFFECT/rings.h:22; was
// func_800F9800).
bool Plg129::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG129.BIN vs_effect_lastAscension_3 (src/EFFECT/rings.h:22; was
// func_800FBAC8). rings.h:323 undefines VS_RING_TRANSPOSED before this second
// instance is compiled, so it draws upright (the generated copy transposed it).
bool Plg129::Ring2(Templates::RingCommand command, std::int32_t packed,
                   std::span<const Templates::RingPaletteEntry> palette,
                   Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG138.BIN vs_effect_papillonReel_1 (src/EFFECT/rings.h:22; was
// func_800FA6D0).
bool Plg138::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG142.BIN vs_effect_adventSign_2 (src/EFFECT/rings.h:22; was
// func_800FB2D8).
bool Plg142::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_3 (src/EFFECT/rings.h:22; was
// func_800FCB38).
bool Plg146::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG147.BIN vs_effect_bearClaw_0 (src/EFFECT/rings.h:22; was
// func_800F98F8).
bool Plg147::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG149.BIN vs_effect_ironRipper_0 (src/EFFECT/rings.h:22; was
// func_800F9800).
bool Plg149::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG150.BIN vs_effect_emeticBomb_3 (src/EFFECT/rings.h:22; was
// func_800FBDF8).
bool Plg150::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG153.BIN vs_effect_gravisAether_1 (src/EFFECT/rings.h:22; was
// func_800FA41C).
bool Plg153::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG160.BIN vs_effect_scytheWind_1 (src/EFFECT/rings.h:22; was
// func_800FA6F8).
bool Plg160::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG161.BIN vs_effect_gigaTempest_1 (src/EFFECT/rings.h:22; was
// func_800FA7B0).
bool Plg161::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG161.BIN vs_effect_gigaTempest_4 (src/EFFECT/rings.h:22; was
// func_800FE044). rings.h:323 undefines VS_RING_TRANSPOSED before this second
// instance is compiled, so it draws upright (the generated copy transposed it).
bool Plg161::Ring2(Templates::RingCommand command, std::int32_t packed,
                   std::span<const Templates::RingPaletteEntry> palette,
                   Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG162.BIN vs_effect_spiralScourge_2 (src/EFFECT/rings.h:22; was
// func_800FB5F8).
bool Plg162::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG166.BIN vs_effect_sanctusFlare_3 (src/EFFECT/rings.h:22; was
// func_800FC29C).
bool Plg166::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG169.BIN vs_effect_vermillionAura_1 (src/EFFECT/rings.h:22; was
// func_800FBA00).
bool Plg169::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG170.BIN vs_effect_retribution_2 (src/EFFECT/rings.h:22; was
// func_800FB348).
bool Plg170::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG198.BIN vs_effect_hpUp_0 (src/EFFECT/rings.h:22; was
// func_800F9800).
bool Plg198::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG236.BIN vs_effect_plg236_3 (src/EFFECT/rings.h:22; was
// func_800FC56C).
bool Plg236::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG263.BIN vs_effect_plg263_1 (src/EFFECT/rings.h:22; was
// func_800FA4A8).
bool Plg263::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG297.BIN vs_effect_plg297_3 (src/EFFECT/rings.h:22; was
// func_800FBF4C).
bool Plg297::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}
}  // namespace VS::Effects
