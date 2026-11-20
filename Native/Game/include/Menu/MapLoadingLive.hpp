#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binds MapLoading.hpp to the generated map menu: the area chooser over
// VS::State::roomVisitProgress.area_flags, sceneRowIdMap, currentMapRow,
// mapNames and the framework's menu rows; the background over its own disc
// request and image; and the ARM scene map over VS::Battle::sceneBuffer,
// sceneCdQueueSlot and the map view statics (Menu/Map.hpp), through
// MapNavigationLive.hpp's room rules. Each entry keeps the original call shape.
// Missing menu text or names, a name offset or scene outside its table, a
// missing disc request and a missing menu row are refused and logged, where the
// original reads or writes unchecked.

namespace VS::Menu::map {
struct SceneMapView;
// Shared by the loader and terminal; callbacks can update the same view.
[[nodiscard]] SceneMapView ReadLiveMapView();
void WriteLiveMapView(const SceneMapView& view);
void LiveConfigureAreaList(int row_count, int current_map, char* menu_text[]);
int LivePollAreaSelection();
void LiveShowAreaName(char* map_name, char font_color);
void LivePollMenuReady();
// The background's image is freed once; later calls in the shown step log
// the repeated free the original made and still return 1.
int LiveLoadBackground(int begin_background_load);
int LiveLoadSceneMap();
int LiveUpdateAreaSelection(int initialize);
// Cancel writes before freeing their buffers; false retains ownership.
// Call before a MENU5 reload or heap replacement, not after heap reset.
[[nodiscard]] bool DrainMapLoadingLive();
// Requires successful drainage; restores the file-backed chooser/background
// controllers without touching progression, text or the active scene owner.
[[nodiscard]] bool ResetMapLoadingModuleData();
}  // namespace VS::Menu::map
