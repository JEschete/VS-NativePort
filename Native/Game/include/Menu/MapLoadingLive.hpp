#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binds MapLoading.hpp to the generated map menu: the area chooser over
// State_roomVisitProgress.areaFlags, Menu_Map_sceneRowIdMap,
// Menu_Map_currentMapRow, Menu_Map_mapNames and the framework's menu rows;
// the background over its own disc request and image; and the ARM scene map
// over Battle_unusedSceneBuffer, Menu_Map_sceneCdQueueSlot and the Menu_Map_
// view globals, through MapNavigationLive.hpp's room rules. Each entry keeps
// the original call shape. Missing menu text or names, a name offset or
// scene outside its table, a missing disc request and a missing menu row are
// refused and logged, where the original reads or writes unchecked.

namespace VS::Menu::map {
void LiveConfigureAreaList(int row_count, int current_map, char* menu_text[]);
int LivePollAreaSelection();
void LiveShowAreaName(char* map_name, char font_color);
void LivePollMenuReady();
// The background's image is freed once; later calls in the shown step log
// the repeated free the original made and still return 1.
int LiveLoadBackground(int begin_background_load);
int LiveLoadSceneMap();
int LiveUpdateAreaSelection(int initialize);
}  // namespace VS::Menu::map
