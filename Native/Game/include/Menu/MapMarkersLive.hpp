#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binds MapMarkers.hpp to the generated map menu: the frame's primitive
// cursor and ordering tables, row 0 of the menu framework, _mapLabels and
// its count (mapLabels, mapLabelCount), mapLabelY,
// the icon RECT tables, the door and connecting-area name tables, the area
// visit flags and the state flags. Each entry keeps the original call shape.
// An icon, name, area or state flag outside its table, a full label queue
// and a primitive buffer without room are refused and logged, where the
// original reads or writes unchecked.

#include "Legacy/Psx.h"

namespace VS::Menu::map {
void LiveDrawAshleyMarker(int id, int xy, int brightness, u_long* next_prim);
void LiveDrawFeatureMarker(int id, int x, int y);
void LiveDrawDoorStateMarker(int x, int y, int id);
void LiveDrawDoorLabel(int x, int y, int door_id);
void LiveAppendLabelLine(int x0, int y0, int x1, int y1);
void LiveDrawConnectingAreaLabel(int x, int y, int map_id);
void LiveQueueLabel(int x, int y, int type, int label_id);
// A state outside the state flags is refused and not drawn.
int LiveShouldDrawDoorStateMarker(int door_state_id, int zone_id, int map_id);
}  // namespace VS::Menu::map
