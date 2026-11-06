#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>

#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/SurfaceCore.hpp"

// The surface template (decomp src/EFFECT/surfaces.h:34 vs_surface): five
// rows of sixteen points from the overlay's unit profile, scaled out by a
// radius and up by a height that both grow from row to row, drawn as four
// bands of sixteen Gouraud-shaded quads (DrawSheet), each row coloured by its
// intensity.
namespace VS::Effects::Templates {
// The five rows of a surface tick (the first rows of the result): the
// profile scaled by the frame's radius and height around the scratch
// position.
[[nodiscard]] SheetPoints SurfacePoints(const SheetFrame& frame,
                                        SurfaceProfile profile);

// One renderer command of a surface; false releases the slot.
bool Surface(SurfaceTextureSpan span, SurfaceCommand command,
             std::int32_t packed, SurfaceProfile profile, SheetHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (Surface.cpp): each binds the template to its
// overlay's texture repeat; SurfacesLive.hpp passes the overlay's profile.
namespace VS::Effects {
namespace Plg061 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg061
namespace Plg077 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg077
namespace Plg087 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg087
namespace Plg128 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
bool Surface2(Templates::SurfaceCommand command, std::int32_t packed,
              Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg128
namespace Plg129 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg129
namespace Plg150 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg150
namespace Plg158 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg158
namespace Plg170 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg170
namespace Plg263 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg263
namespace Plg272 {
bool Surface(Templates::SurfaceCommand command, std::int32_t packed,
             Templates::SurfaceProfile profile, Templates::SheetHost& host);
}  // namespace Plg272
}  // namespace VS::Effects
