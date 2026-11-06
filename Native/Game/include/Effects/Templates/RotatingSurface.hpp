#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>

#include "Core/Math/Interpolation.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/SurfaceCore.hpp"

// The rotating surface template (decomp src/EFFECT/rotatingSurfaces.h:34
// vs_rotatingSurface): five rows of sixteen points around the vertical axis,
// each row turned further than the one below it and stacked by the growing
// height, the whole sheet turning by a speed picked from the speed range
// every tick; drawn as four bands of sixteen Gouraud-shaded quads
// (DrawSheet).
namespace VS::Effects::Templates {
// The turn of one tick: the speed range's start when it has one value,
// otherwise a random value between its bounds.
[[nodiscard]] std::int32_t RotatingSurfaceStep(Core::Math::Vec2 speed,
                                               SurfaceHost& host);

// The five rows of a rotating surface tick (the first rows of the result);
// turns the frame's angle by `step` first.
[[nodiscard]] SheetPoints RotatingSurfacePoints(SheetFrame& frame,
                                                std::int32_t step);

// One renderer command of a rotating surface; false releases the slot.
bool RotatingSurface(SurfaceTextureSpan span, SurfaceCommand command,
                     std::int32_t packed, SheetHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (RotatingSurface.cpp): each binds the template to its
// overlay's texture repeat.
namespace VS::Effects {
namespace Plg049 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg049
namespace Plg065 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg065
namespace Plg073 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg073
namespace Plg081 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg081
namespace Plg086 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg086
namespace Plg128 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg128
namespace Plg129 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg129
namespace Plg138 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg138
namespace Plg142 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg142
namespace Plg158 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg158
namespace Plg161 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg161
namespace Plg162 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg162
namespace Plg165 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg165
namespace Plg166 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg166
namespace Plg167 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg167
namespace Plg170 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg170
namespace Plg236 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg236
namespace Plg246 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg246
namespace Plg250 {
bool RotatingSurface(Templates::SurfaceCommand command, std::int32_t packed,
                     Templates::SheetHost& host);
}  // namespace Plg250
}  // namespace VS::Effects
