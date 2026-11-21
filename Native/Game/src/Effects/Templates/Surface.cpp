// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/Surface.hpp"

#include <array>
#include <cstdint>

#include "Effects/Templates/SurfaceCore.hpp"

namespace VS::Effects::Templates {
namespace {
// The profile rows in order.
constexpr std::array<SheetProfileRow, kSurfaceRows> kSurfaceRowOrder = {
    SheetProfileRow{.row = 0}, SheetProfileRow{.row = 1},
    SheetProfileRow{.row = 2}, SheetProfileRow{.row = 3},
    SheetProfileRow{.row = 4},
};
}  // namespace

// decomp: src/EFFECT/surfaces.h:137-152 (the points of the five rows).
SheetPoints SurfacePoints(const SheetFrame& frame, SurfaceProfile profile) {
  return ScaleSheetProfile(frame, profile, kSurfaceRowOrder);
}

// decomp: EFFECT VS_SURFACE_FUNCTION (src/EFFECT/surfaces.h:62).
bool Surface(SurfaceTextureSpan span, SurfaceCommand command,
             std::int32_t packed, SurfaceProfile profile, SheetHost& host) {
  return RunSheet(command, packed, kSurfaceRows, span, host,
                  [profile](const SheetFrame& frame) {
                    return SurfacePoints(frame, profile);
                  });
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG061.BIN vs_effect_flameSphereLv1_0 (src/EFFECT/surfaces.h:62; was
// func_800F9818).
bool Plg061::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}

// decomp: PLG077.BIN vs_effect_meteorLv1_3 (src/EFFECT/surfaces.h:62; was
// func_800FBDE4).
bool Plg077::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_0 (src/EFFECT/surfaces.h:62; was
// func_800F99F0).
bool Plg087::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG128.BIN vs_effect_bloodySin_0 (src/EFFECT/surfaces.h:62; was
// func_800F98F8).
bool Plg128::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG128.BIN vs_effect_bloodySin_4 (src/EFFECT/surfaces.h:62; was
// func_800FCE0C).
bool Plg128::Surface2(Templates::SurfaceCommand command, std::int32_t packed,
                      Templates::SurfaceProfile profile,
                      Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}

// decomp: PLG129.BIN vs_effect_lastAscension_2 (src/EFFECT/surfaces.h:62; was
// func_800FAF30).
bool Plg129::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG150.BIN vs_effect_emeticBomb_2 (src/EFFECT/surfaces.h:62; was
// func_800FB260).
bool Plg150::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}

// decomp: PLG158.BIN vs_effect_hexFlux_0 (src/EFFECT/surfaces.h:62; was
// func_800F9800).
bool Plg158::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG170.BIN vs_effect_retribution_4 (src/EFFECT/surfaces.h:62; was
// func_800FCABC).
bool Plg170::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG263.BIN vs_effect_plg263_0 (src/EFFECT/surfaces.h:62; was
// func_800F9910).
bool Plg263::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}

// decomp: PLG272.BIN vs_effect_plg272_2 (src/EFFECT/surfaces.h:62; was
// func_800FB2D8).
bool Plg272::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}
}  // namespace VS::Effects
