#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>

#include "Legacy/Psx.h"
#include "Render/RoomDraw.hpp"

struct Field_RoomTint;
struct Render_ScreenOffset;

// Live binding for the MPD room geometry (RoomDraw.cpp): the GTE, the frame
// camera and the current frame's primitive buffer. Room geometry is read
// through the extent its section header records, and every packet is bounded
// by the frame buffer the draw started in.
namespace VS::Render {
// The clip and texture level-of-detail settings room polygons draw with,
// owned here instead of in module statics.
[[nodiscard]] RoomClipState& LiveRoomClipState();
// The frame camera: its view matrix, far clip zoom and roll.
[[nodiscard]] RoomCamera LiveRoomCamera();

[[nodiscard]] bool ValidateLiveRoomGeometry(const void* geometry,
                                            std::size_t bytes);
// Rebuilds the group views from the frame camera; false (and logged) when
// the geometry is refused.
bool UpdateLiveRoomGroupTransforms(void* geometry, std::size_t bytes);
// Draws the visible groups into at most 64 KiB of the current frame buffer.
RoomDrawStatus DrawLiveRoomGeometry(const void* geometry, std::size_t bytes,
                                    int mode);
void BuildLiveRoomGeometryViewMatrix(const VECTOR& position,
                                     const VECTOR& angles, MATRIX& output);
void BuildLiveScaleCompensatedRoomView(const VECTOR& position,
                                       const VECTOR& angles, MATRIX& output);
// Draws the sky through the loaded GTE view at the farthest ordering entry.
RoomDrawStatus DrawLiveSkyDome(const void* sky, std::size_t bytes);
// Tints the room packets written between `start` and `end`, which must lie
// in the current frame buffer; false (and logged) when the range is refused
// or truncated.
bool TintLiveRoomGeometryPrimitives(void* start, void* end,
                                    const Field_RoomTint& tint, int flags);
[[nodiscard]] Render_ScreenOffset GetLiveProjectionScreenOffset();
void SetLiveProjectionScreenOffset(const Render_ScreenOffset& offset);
}  // namespace VS::Render
