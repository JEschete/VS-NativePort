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

// decomp: surfaces.h:137-152 (the points of the five rows).
SheetPoints SurfacePoints(const SheetFrame& frame, SurfaceProfile profile) {
  return ScaleSheetProfile(frame, profile, kSurfaceRowOrder);
}

// decomp: EFFECT VS_SURFACE_FUNCTION (src/EFFECT/surfaces.h:34).
bool Surface(SurfaceTextureSpan span, SurfaceCommand command,
             std::int32_t packed, SurfaceProfile profile, SheetHost& host) {
  return RunSheet(command, packed, kSurfaceRows, span, host,
                  [profile](const SheetFrame& frame) {
                    return SurfacePoints(frame, profile);
                  });
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG061.BIN func_800F9818 (src/EFFECT/PLG061.BIN/18.c:8).
bool Plg061::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}

// decomp: PLG077.BIN func_800FBDE4 (src/EFFECT/PLG077.BIN/18.c:344).
bool Plg077::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG087.BIN func_800F99F0 (src/EFFECT/PLG087.BIN/1F0.c:8).
bool Plg087::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG128.BIN func_800F98F8 (src/EFFECT/PLG128.BIN/F8.c:44).
bool Plg128::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG128.BIN func_800FCE0C (src/EFFECT/PLG128.BIN/F8.c:61).
bool Plg128::Surface2(Templates::SurfaceCommand command, std::int32_t packed,
                      Templates::SurfaceProfile profile,
                      Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}

// decomp: PLG129.BIN func_800FAF30 (src/EFFECT/PLG129.BIN/0.c:12).
bool Plg129::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG150.BIN func_800FB260 (src/EFFECT/PLG150.BIN/18.c:9).
bool Plg150::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}

// decomp: PLG158.BIN func_800F9800 (src/EFFECT/PLG158.BIN/0.c:8).
bool Plg158::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG170.BIN func_800FCABC (src/EFFECT/PLG170.BIN/18.c:24).
bool Plg170::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kFourColumns,
                            command, packed, profile, host);
}

// decomp: PLG263.BIN func_800F9910 (src/EFFECT/PLG263.BIN/110.c:2).
bool Plg263::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}

// decomp: PLG272.BIN func_800FB2D8 (src/EFFECT/PLG272.BIN/18.c:16).
bool Plg272::Surface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SurfaceProfile profile,
                     Templates::SheetHost& host) {
  return Templates::Surface(Templates::SurfaceTextureSpan::kSixteenColumns,
                            command, packed, profile, host);
}
}  // namespace VS::Effects
