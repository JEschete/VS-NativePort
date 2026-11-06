#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>

#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/SurfaceCore.hpp"

// The mirrored surface template (decomp src/EFFECT/mirroredSurfaces.h:34
// vs_mirrorSurface): the overlay's five-row profile read from its last row
// to its first and back out again, the second half flipped downwards, for
// nine rows that share the first profile row; drawn as eight bands of
// sixteen Gouraud-shaded quads (DrawSheet).
namespace VS::Effects::Templates {
// The nine rows of a mirrored surface tick.
[[nodiscard]] SheetPoints MirrorSurfacePoints(const SheetFrame& frame,
                                              SurfaceProfile profile);

// One renderer command of a mirrored surface; false releases the slot.
bool MirrorSurface(SurfaceTextureSpan span, SurfaceCommand command,
                   std::int32_t packed, SurfaceProfile profile,
                   SheetHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (MirrorSurface.cpp): each binds the template to its
// overlay's texture repeat; SurfacesLive.hpp passes the overlay's profile.
namespace VS::Effects {
namespace Plg146 {
bool MirrorSurface(Templates::SurfaceCommand command, std::int32_t packed,
                   Templates::SurfaceProfile profile,
                   Templates::SheetHost& host);
}  // namespace Plg146
namespace Plg158 {
bool MirrorSurface(Templates::SurfaceCommand command, std::int32_t packed,
                   Templates::SurfaceProfile profile,
                   Templates::SheetHost& host);
}  // namespace Plg158
namespace Plg166 {
bool MirrorSurface(Templates::SurfaceCommand command, std::int32_t packed,
                   Templates::SurfaceProfile profile,
                   Templates::SheetHost& host);
}  // namespace Plg166
namespace Plg170 {
bool MirrorSurface(Templates::SurfaceCommand command, std::int32_t packed,
                   Templates::SurfaceProfile profile,
                   Templates::SheetHost& host);
}  // namespace Plg170
namespace Plg273 {
bool MirrorSurface(Templates::SurfaceCommand command, std::int32_t packed,
                   Templates::SurfaceProfile profile,
                   Templates::SheetHost& host);
}  // namespace Plg273
namespace Plg297 {
bool MirrorSurface(Templates::SurfaceCommand command, std::int32_t packed,
                   Templates::SurfaceProfile profile,
                   Templates::SheetHost& host);
}  // namespace Plg297
namespace Plg300 {
bool MirrorSurface(Templates::SurfaceCommand command, std::int32_t packed,
                   Templates::SurfaceProfile profile,
                   Templates::SheetHost& host);
}  // namespace Plg300
}  // namespace VS::Effects
