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
  // decomp: rings.h angleB, (-jitter.vy) / 2 truncated toward zero.
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
// decomp: PLG049.BIN func_800FBA00 (src/EFFECT/PLG049.BIN/E0.c:10).
bool Plg049::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG050.BIN func_800F98E0 (src/EFFECT/PLG050.BIN/E0.c:2).
bool Plg050::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG061.BIN func_800FAF00 (src/EFFECT/PLG061.BIN/18.c:15).
bool Plg061::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG065.BIN func_800FB71C (src/EFFECT/PLG065.BIN/F8.c:15).
bool Plg065::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG073.BIN func_800FA7F0 (src/EFFECT/PLG073.BIN/F8.c:6).
// decomp: PLG074.BIN func_800FA7F0 (src/EFFECT/PLG074.BIN/F8.c:5).
bool Plg073::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG081.BIN func_800FA6B8 (src/EFFECT/PLG081.BIN/0.c:11).
bool Plg081::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG129.BIN func_800F9800 (src/EFFECT/PLG129.BIN/0.c:2).
bool Plg129::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG129.BIN func_800FBAC8 (src/EFFECT/PLG129.BIN/0.c:17).
// rings.h:323 undefines VS_RING_TRANSPOSED before this second instance is
// compiled, so it draws upright (the generated copy transposed it).
bool Plg129::Ring2(Templates::RingCommand command, std::int32_t packed,
                   std::span<const Templates::RingPaletteEntry> palette,
                   Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG138.BIN func_800FA6D0 (src/EFFECT/PLG138.BIN/18.c:5).
bool Plg138::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG142.BIN func_800FB2D8 (src/EFFECT/PLG142.BIN/18.c:16).
bool Plg142::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG146.BIN func_800FCB38 (src/EFFECT/PLG146.BIN/1D8.c:34).
bool Plg146::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG147.BIN func_800F98F8 (src/EFFECT/PLG147.BIN/F8.c:2).
bool Plg147::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG149.BIN func_800F9800 (src/EFFECT/PLG149.BIN/0.c:2).
bool Plg149::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG150.BIN func_800FBDF8 (src/EFFECT/PLG150.BIN/18.c:13).
bool Plg150::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG153.BIN func_800FA41C (src/EFFECT/PLG153.BIN/110.c:7).
bool Plg153::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG160.BIN func_800FA6F8 (src/EFFECT/PLG160.BIN/0.c:6).
bool Plg160::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG161.BIN func_800FA7B0 (src/EFFECT/PLG161.BIN/F8.c:5).
bool Plg161::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG161.BIN func_800FE044 (src/EFFECT/PLG161.BIN/F8.c:23).
// rings.h:323 undefines VS_RING_TRANSPOSED before this second instance is
// compiled, so it draws upright (the generated copy transposed it).
bool Plg161::Ring2(Templates::RingCommand command, std::int32_t packed,
                   std::span<const Templates::RingPaletteEntry> palette,
                   Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG162.BIN func_800FB5F8 (src/EFFECT/PLG162.BIN/0.c:339).
bool Plg162::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG166.BIN func_800FC29C (src/EFFECT/PLG166.BIN/0.c:20).
bool Plg166::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG169.BIN func_800FBA00 (src/EFFECT/PLG169.BIN/E0.c:18).
bool Plg169::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG170.BIN func_800FB348 (src/EFFECT/PLG170.BIN/18.c:16).
bool Plg170::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG198.BIN func_800F9800 (src/EFFECT/PLG198.BIN/0.c:2).
bool Plg198::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG236.BIN func_800FC56C (src/EFFECT/PLG236.BIN/0.c:345).
bool Plg236::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}

// decomp: PLG263.BIN func_800FA4A8 (src/EFFECT/PLG263.BIN/110.c:6).
bool Plg263::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kUpright, command, packed,
                         palette, host);
}

// decomp: PLG297.BIN func_800FBF4C (src/EFFECT/PLG297.BIN/18.c:20).
bool Plg297::Ring(Templates::RingCommand command, std::int32_t packed,
                  std::span<const Templates::RingPaletteEntry> palette,
                  Templates::RingHost& host) {
  return Templates::Ring(Templates::RingOrientation::kTransposed, command,
                         packed, palette, host);
}
}  // namespace VS::Effects
