// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/MirrorSurface.hpp"

#include <array>
#include <cstdint>

#include "Effects/Templates/SurfaceCore.hpp"

namespace VS::Effects::Templates {
namespace {
// The profile from its last row to its first, then back out flipped.
constexpr std::array<SheetProfileRow, kMirrorSurfaceRows> kMirrorRowOrder = {
    SheetProfileRow{.row = 4},
    SheetProfileRow{.row = 3},
    SheetProfileRow{.row = 2},
    SheetProfileRow{.row = 1},
    SheetProfileRow{.row = 0},
    SheetProfileRow{.row = 1, .flipped = true},
    SheetProfileRow{.row = 2, .flipped = true},
    SheetProfileRow{.row = 3, .flipped = true},
    SheetProfileRow{.row = 4, .flipped = true},
};
}  // namespace

// decomp: mirroredSurfaces.h:137-172 (the points of the nine rows).
SheetPoints MirrorSurfacePoints(const SheetFrame& frame,
                                SurfaceProfile profile) {
  return ScaleSheetProfile(frame, profile, kMirrorRowOrder);
}

// decomp: EFFECT VS_MIRROR_SURFACE_FUNCTION (src/EFFECT/mirroredSurfaces.h:34).
bool MirrorSurface(SurfaceTextureSpan span, SurfaceCommand command,
                   std::int32_t packed, SurfaceProfile profile,
                   SheetHost& host) {
  return RunSheet(command, packed, kMirrorSurfaceRows, span, host,
                  [profile](const SheetFrame& frame) {
                    return MirrorSurfacePoints(frame, profile);
                  });
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG146.BIN vs_effect_blackNebula_1 (src/EFFECT/surfaces.h:62; was
// func_800FA8D0).
bool Plg146::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG158.BIN vs_effect_hexFlux_1 (src/EFFECT/surfaces.h:62; was
// func_800FA398).
bool Plg158::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG166.BIN vs_effect_sanctusFlare_1 (src/EFFECT/surfaces.h:62; was
// func_800FA6B8).
bool Plg166::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG170.BIN vs_effect_retribution_0 (src/EFFECT/surfaces.h:62; was
// func_800F9818).
bool Plg170::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG273.BIN vs_effect_plg273_0 (src/EFFECT/surfaces.h:62; was
// func_800F9800).
bool Plg273::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(Templates::SurfaceTextureSpan::kFourColumns,
                                  command, packed, profile, host);
}

// decomp: PLG297.BIN vs_effect_plg297_0 (src/EFFECT/surfaces.h:62; was
// func_800F9818).
bool Plg297::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG300.BIN vs_effect_plg300_0 (src/EFFECT/surfaces.h:62; was
// func_800F9910).
bool Plg300::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}
}  // namespace VS::Effects
