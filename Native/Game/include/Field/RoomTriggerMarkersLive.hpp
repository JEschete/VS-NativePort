#pragma once

#include <cstdint>

namespace VS::Field {
// Runs VS::Field::DrawRoomTriggerMarkers over the live room's floor triggers
// with the frame timers, the collision floor and the BATTLE.PRG marker.
void DrawLiveRoomTriggerMarkers();
// Draws one marker with the BATTLE.PRG definition and the frame view.
void DrawLiveRoomTriggerMarker(int world_x, int world_z, std::int16_t height,
                               std::uint32_t brightness);
}  // namespace VS::Field
