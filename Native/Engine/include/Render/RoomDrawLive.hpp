#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>

#include "Legacy/Psx.hpp"
#include "Render/RoomDraw.hpp"

namespace VS::Render {
struct ScreenOffset;
}  // namespace VS::Render

// Live binding for the MPD room geometry (RoomDraw.cpp): the GTE, the frame
// camera and the gameplay frame's draw list (Render/DrawList.hpp). Room
// geometry is read through the extent its section header records; its
// polygons become typed items in the scene table slots the frame's ordering
// pointer names, and the primitive cursor advances by the size their packets
// had, bounded by the frame buffer the draw started in.
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
// Draws the visible groups as items of the open scene draw list, within the
// 64 KiB of the current frame buffer their packets had; nothing (logged)
// when no scene list is open over the frame's ordering table.
RoomDrawStatus DrawLiveRoomGeometry(const void* geometry, std::size_t bytes,
                                    int mode);
void BuildLiveRoomGeometryViewMatrix(const VS::Core::Gte::VECTOR& position,
                                     const VS::Core::Gte::VECTOR& angles,
                                     VS::Core::Gte::MATRIX& output);
void BuildLiveScaleCompensatedRoomView(const VS::Core::Gte::VECTOR& position,
                                       const VS::Core::Gte::VECTOR& angles,
                                       VS::Core::Gte::MATRIX& output);
// Draws the sky rotated by `view` at the farthest ordering entry.
RoomDrawStatus DrawLiveSkyDome(const void* sky, std::size_t bytes,
                               const VS::Core::Gte::MATRIX& view);
// Tints the scene draw items with indices in [first, end), the range a room
// draw added; false (and logged) when the range is not in the open list.
bool TintLiveRoomGeometry(std::size_t first, std::size_t end,
                          const RoomTint& tint);
[[nodiscard]] VS::Render::ScreenOffset GetLiveProjectionScreenOffset();
void SetLiveProjectionScreenOffset(const VS::Render::ScreenOffset& offset);
}  // namespace VS::Render
