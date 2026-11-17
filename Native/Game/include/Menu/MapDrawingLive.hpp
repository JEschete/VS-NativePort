#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binds MapDrawing.hpp to the generated map menu: the GTE projection and
// depth cue, the frame's primitive cursor and ordering table, the map
// markers (MapMarkersLive.hpp), text box 0, the button and caption glyphs and
// the copied map globals (Menu_Map_*). Each entry keeps the original call
// shape. A room outside the menu's loaded scene, a malformed room, a scene
// outside the state flags, an over-full label queue, an oversized backdrop
// and a primitive buffer or ordering-table entry without room are refused
// and logged, where the original reads or writes unchecked.

struct Field_MapScene;

namespace VS::Menu::map {
void LiveDrawVisitedRooms(Field_MapScene* scene);
// `room_data` must lie in the ARM scene the menu loaded or the current
// room's minimap scene; 1 when drawn, 0 otherwise.
int LiveDrawRoom(int room_index, void* room_data, int zone_id, int map_id);
void LiveDrawControlBackdrop(int x, int y, int w, int h);
void LiveDrawControls();
void LiveDrawCompass(int x, int y, int angle);
// Leaves the corners in Menu_Map_compassBounds.
void LiveComputeCompassCorners(int center_x, int center_y, int width,
                               int height, int angle);
void LiveDrawLabels();
// Clears what the original kept in MENU5.PRG statics and the scratchpad:
// _drawUIControls' slide offset and _renderRoom's projected vertex rows.
void ResetMapDrawingLive();
// Ordinary MENU5 reload restores only its file-backed slide offset. The
// scratch workspace is not part of the overlay image.
void ResetMapDrawingModuleData();
}  // namespace VS::Menu::map
