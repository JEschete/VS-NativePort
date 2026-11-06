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
// decomp: PLG146.BIN func_800FA8D0 (src/EFFECT/PLG146.BIN/1D8.c:6).
bool Plg146::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG158.BIN func_800FA398 (src/EFFECT/PLG158.BIN/0.c:13).
bool Plg158::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG166.BIN func_800FA6B8 (src/EFFECT/PLG166.BIN/0.c:11).
bool Plg166::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG170.BIN func_800F9818 (src/EFFECT/PLG170.BIN/18.c:8).
bool Plg170::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG273.BIN func_800F9800 (src/EFFECT/PLG273.BIN/0.c:2).
bool Plg273::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(Templates::SurfaceTextureSpan::kFourColumns,
                                  command, packed, profile, host);
}

// decomp: PLG297.BIN func_800F9818 (src/EFFECT/PLG297.BIN/18.c:8).
bool Plg297::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}

// decomp: PLG300.BIN func_800F9910 (src/EFFECT/PLG300.BIN/110.c:8).
bool Plg300::MirrorSurface(Templates::SurfaceCommand command,
                           std::int32_t packed,
                           Templates::SurfaceProfile profile,
                           Templates::SheetHost& host) {
  return Templates::MirrorSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, profile,
      host);
}
}  // namespace VS::Effects
