#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

// The area map's area list, background and scene-map loading (MENU5.PRG
// 4D8.c:129-238, 448-520 and 979-1045): the visited-area chooser's rows,
// cursor and polling, the area-name title row, the menu-ready poll, the
// background image's read-and-upload steps and the ARM scene map's read and
// view reset. The rules keep their own state (the chooser's step, the
// background step and the map view) and reach the menu rows, the disc reads,
// the heap, the image uploads and the loaded scene through small hosts;
// MapLoadingLive.hpp binds them to the copied globals. The loaded scene's
// rooms are scaled, chosen and centered by MapNavigation.hpp's rules through
// the scene-map host.
namespace VS::Menu::map {
// _sceneArmFiles' and areaFlags' extent: scenes 0 to 31.
inline constexpr std::int32_t kSceneCount = 32;
// More area rows than this scroll.
inline constexpr std::int32_t kVisibleAreaRows = 8;
// The row type bit that marks the area Ashley is in.
inline constexpr std::int32_t kCurrentAreaRowType = 4;
// The current-area argument's flag that disables that mark.
inline constexpr std::int32_t kNoCurrentAreaFlag = 0x80;
// The area list's row info word.
inline constexpr std::int32_t kAreaRowInfo = 0x100;
// The row the chooser keeps when cancelled.
inline constexpr std::int32_t kAreaListKeptRow = 5;
// _mapSelectionMenu's results besides a chosen row.
inline constexpr std::int32_t kAreaSelectionPending = -1;
inline constexpr std::int32_t kNoVisitedAreas = -2;
// vs_main_CdQueueStateLoaded.
inline constexpr std::int32_t kDiscReadLoaded = 4;

// D_800F4EE8.cursorMemories[0] and [1]: the cursor row and the first shown
// row, each stored as a byte.
struct AreaListCursor {
  std::uint8_t row = 0;
  std::uint8_t first_row = 0;

  friend bool operator==(const AreaListCursor&,
                         const AreaListCursor&) = default;
};

class AreaMenuHost {
 public:
  AreaMenuHost() = default;
  AreaMenuHost(const AreaMenuHost&) = delete;
  AreaMenuHost& operator=(const AreaMenuHost&) = delete;
  AreaMenuHost(AreaMenuHost&&) = delete;
  AreaMenuHost& operator=(AreaMenuHost&&) = delete;
  virtual ~AreaMenuHost() = default;

  // The chooser's areas in row order, before ShowAreaRows (only
  // UpdateAreaSelection lists them).
  virtual void ListAreas(std::span<const std::int32_t> areas) = 0;
  virtual void SetAreaCursor(AreaListCursor cursor) = 0;
  // D_800F4EE8.cursorMemories[0].
  [[nodiscard]] virtual std::uint8_t AreaCursorRow() = 0;
  // vs_main_settings.cursorMemory.
  [[nodiscard]] virtual std::uint8_t CursorMemorySetting() = 0;
  virtual void SetCursorMemorySetting(std::uint8_t setting) = 0;
  // vs_mainmenu_setMenuRows over the listed area names.
  virtual void ShowAreaRows(std::int32_t row_info,
                            std::span<const std::int32_t> row_types) = 0;
  // vs_mainmenu_getSelectedRow.
  [[nodiscard]] virtual std::int32_t SelectedRow() = 0;
  // vs_mainMenu_clearMenuExcept.
  virtual void ClearMenuExcept(std::int32_t row) = 0;
  // vs_mainMenu_flyoutMenuRightAndHoistSelection.
  virtual void FlyOutAndHoist(std::int32_t row, std::int32_t offset) = 0;
};

// _configureMapMenu: remembers `current_area` (packed: the row in bits 0-6,
// kNoCurrentAreaFlag) as the current row the first time (`current_row` -1),
// marks that row unless the flag is set, places the cursor and shows
// `row_count` rows with the cursor memory forced on and then restored. A
// remembered row outside the rows marks none (the original writes before its
// row types for one below -1).
void ConfigureAreaList(AreaMenuHost& host, std::int32_t row_count,
                       std::int32_t current_area, std::int32_t& current_row);

// The cursor _configureMapMenu stores for `row_count` rows and the area row
// `area` (bits 0-6 used).
[[nodiscard]] AreaListCursor AreaListCursorOf(std::int32_t row_count,
                                              std::int32_t area);

// _mapSelection: the selected row (-1 while choosing). Below -1 (cancelled)
// clears the menu but kAreaListKeptRow; a chosen row flies out and hoists
// the cursor's row.
[[nodiscard]] std::int32_t PollAreaSelection(AreaMenuHost& host);

// _mapSelectionMenu's statics.
struct AreaSelection {
  std::int32_t step = 0;
  std::int32_t selected = 0;
};

// The area globals _mapSelectionMenu reads and writes.
struct AreaSelectionInput {
  // vs_main_mapStatus.areaFlags[0]: bit n marks scene n visited.
  std::uint32_t visited_areas = 0;
  std::int32_t current_scene = 0;
  std::int32_t current_scene_row = 0;
};

// _mapSelectionMenu: `initialize` restarts the chooser. The first step lists
// the visited scenes 1-31 into `scene_rows` (kSceneCount entries; the rows'
// scenes) and shows them, the current scene marked unless
// `current_scene_row` is 0; kNoVisitedAreas when none is visited. Later steps
// poll the chooser. kAreaSelectionPending until a row is chosen or the
// chooser is cancelled (below -1). `scene_rows` shorter than the visited
// scenes refuses with kNoVisitedAreas, where the original writes past it.
[[nodiscard]] std::int32_t UpdateAreaSelection(
    AreaMenuHost& host, AreaSelection& selection,
    const AreaSelectionInput& input, std::span<std::int32_t> scene_rows,
    std::int32_t& current_row, std::int32_t initialize);

// vs_battle_setMenuItem's placement for the area-name title row.
struct MenuRowPlacement {
  std::int32_t row = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t background_width = 0;

  friend bool operator==(const MenuRowPlacement&,
                         const MenuRowPlacement&) = default;
};

// The title row's fields _setMenuItemMapName sets after placing it.
struct AreaNameRow {
  MenuRowPlacement placement;
  std::uint8_t state = 0;
  std::int16_t target_x = 0;
  std::uint8_t selected = 0;
  std::uint8_t font_color = 0;

  friend bool operator==(const AreaNameRow&, const AreaNameRow&) = default;
};

// _setMenuItemMapName: the sliding area-name row in `font_color` (its low
// byte).
[[nodiscard]] AreaNameRow AreaNameRowOf(std::int32_t font_color);

class MenuReadyHost {
 public:
  MenuReadyHost() = default;
  MenuReadyHost(const MenuReadyHost&) = delete;
  MenuReadyHost& operator=(const MenuReadyHost&) = delete;
  MenuReadyHost(MenuReadyHost&&) = delete;
  MenuReadyHost& operator=(MenuReadyHost&&) = delete;
  virtual ~MenuReadyHost() = default;

  // vs_mainmenu_ready.
  virtual std::int32_t MenuRowsIdle() = 0;
};

// _menuReady: polls whether the menu rows are idle and drops the answer.
void PollMenuReady(MenuReadyHost& host);

// A disc file: its first sector and byte size (vs_main_CdFile).
struct DiscFileRange {
  std::int32_t sector = 0;
  std::int32_t bytes = 0;

  friend bool operator==(const DiscFileRange&, const DiscFileRange&) = default;
};

// MAPBG.BIN (sector 85088, 77824 bytes): a 256-colour CLUT then a 160x240
// image of 16-bit texels.
inline constexpr DiscFileRange kMapBackgroundFile{85088, 77824};
inline constexpr std::int32_t kBackgroundClutRow = 3;
inline constexpr std::int32_t kBackgroundClutColors = 256;
// The image's texels follow the CLUT's colours (in halfwords).
inline constexpr std::int32_t kBackgroundImageOffset = 256;
inline constexpr std::int32_t kBackgroundX = 640;
inline constexpr std::int32_t kBackgroundY = 256;
inline constexpr std::int32_t kBackgroundWidth = 160;
inline constexpr std::int32_t kBackgroundHeight = 240;
inline constexpr std::int32_t kBackgroundOrderOffset = -4;
inline constexpr std::int32_t kBackgroundBrightness = 128;

// The background's read and image are the host's; it holds at most one of
// each, from BeginBackgroundRead until released and freed.
class BackgroundLoadHost {
 public:
  BackgroundLoadHost() = default;
  BackgroundLoadHost(const BackgroundLoadHost&) = delete;
  BackgroundLoadHost& operator=(const BackgroundLoadHost&) = delete;
  BackgroundLoadHost(BackgroundLoadHost&&) = delete;
  BackgroundLoadHost& operator=(BackgroundLoadHost&&) = delete;
  virtual ~BackgroundLoadHost() = default;

  // vs_main_allocateCdQueueSlot.
  virtual void AllocateBackgroundRead(DiscFileRange file) = 0;
  // vs_main_allocHeapR.
  virtual void AllocateBackgroundImage(std::int32_t bytes) = 0;
  // vs_main_cdEnqueue of the read into the image.
  virtual void EnqueueBackgroundRead() = 0;
  // The read's state word.
  [[nodiscard]] virtual std::int32_t BackgroundReadState() = 0;
  // vs_main_freeCdQueueSlot.
  virtual void ReleaseBackgroundRead() = 0;
  // vs_main_loadClut from the image's start.
  virtual void LoadBackgroundClut(std::int32_t row, std::int32_t first,
                                  std::int32_t count) = 0;
  // vs_battle_renderImage from `offset` halfwords into the image.
  virtual void UploadBackground(std::int32_t x, std::int32_t y,
                                std::int32_t offset, std::int32_t width,
                                std::int32_t height) = 0;
  // vs_mainMenu_setBackgroundRenderPriority.
  virtual void SetBackgroundPriority(std::int32_t order_offset,
                                     std::int32_t brightness) = 0;
  // vs_main_freeHeapR.
  virtual void FreeBackgroundImage() = 0;
};

// _loadMapBackground's step.
enum class BackgroundStep : std::int32_t { kReading = 0, kShown = 1 };

// _loadMapBackground: `begin` starts the read (0); later calls show the
// image once the read has loaded (0), then free it (1). The original leaves
// the step at kShown, so each later call frees the image again; the caller
// stops at 1. A step outside both returns 0.
[[nodiscard]] std::int32_t LoadBackground(BackgroundLoadHost& host,
                                          std::int32_t& step,
                                          std::int32_t begin);

// SVECTOR's halves.
struct MapAngles {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::int16_t pad = 0;

  friend bool operator==(const MapAngles&, const MapAngles&) = default;
};

// The map view globals _loadArmFile resets.
struct SceneMapView {
  std::int32_t trigger_hide_ui = 0;
  std::int32_t hide_ui = 0;
  MapAngles camera_angles;
  MapAngles camera_position;
  std::int32_t current_room = 0;
  std::int32_t current_scene = 0;
  std::int32_t current_scene_row = 0;
  std::int32_t menu_state = 0;
  std::int32_t background_brightness = 0;
  std::int32_t menu_frames = 0;
  std::int32_t geometry_offset_x = 0;
  std::int32_t geometry_offset_y = 0;
  std::int32_t quit_state = 0;
  std::int32_t camera_distance = 0;
  std::int16_t menu_commands_state = 0;
  std::int16_t ui_animation_state = 0;
  std::int32_t is_paling = 0;

  friend bool operator==(const SceneMapView&, const SceneMapView&) = default;
};

// The progress words _loadArmFile reads and writes.
struct SceneMapProgress {
  // vs_main_stateFlags.mapPaling: scene n's paling byte.
  std::span<std::uint8_t> map_paling;
  // vs_main_mapStatus.roomFlags[1].
  std::uint32_t room_flags = 0;
};

inline constexpr std::uint32_t kRestoreRoomFlag = 0x800000;
inline constexpr std::uint32_t kRestoredRoomFlag = 0x400000;
inline constexpr std::int16_t kMapCameraPitch = 568;
inline constexpr std::int32_t kMapYawBase = 2048;
inline constexpr std::int32_t kMapBackgroundBrightness = 128;
inline constexpr std::int32_t kMapGeometryOffsetX = -384;
inline constexpr std::int32_t kMapCameraDistance = 0x300;
inline constexpr std::int16_t kMapCommandsState = 5;
inline constexpr std::int32_t kMapRoomScale = 4;
inline constexpr std::int32_t kMapFogNear = 768;
inline constexpr std::int32_t kCurrentAreaNameColor = 1;

// The ARM scene buffer and its read are the host's: the buffer from
// AllocateSceneBuffer for as long as the map is open, the read from
// AllocateSceneRead until ReleaseSceneRead.
class SceneMapHost {
 public:
  SceneMapHost() = default;
  SceneMapHost(const SceneMapHost&) = delete;
  SceneMapHost& operator=(const SceneMapHost&) = delete;
  SceneMapHost(SceneMapHost&&) = delete;
  SceneMapHost& operator=(SceneMapHost&&) = delete;
  virtual ~SceneMapHost() = default;

  // vs_battle_sceneBuffer != NULL.
  [[nodiscard]] virtual bool HasSceneBuffer() = 0;
  // The camera yaw halfword (scratch 22).
  [[nodiscard]] virtual std::uint16_t CameraYaw() = 0;
  // vs_battle_getCurrentSceneId.
  [[nodiscard]] virtual std::int32_t CurrentSceneId() = 0;
  // vs_main_allocHeapR into vs_battle_sceneBuffer.
  virtual void AllocateSceneBuffer(std::int32_t bytes) = 0;
  // vs_main_allocateCdQueueSlot of _sceneArmFiles[scene].
  virtual void AllocateSceneRead(std::int32_t scene) = 0;
  // vs_main_cdEnqueue of the read into the buffer.
  virtual void EnqueueSceneRead() = 0;
  [[nodiscard]] virtual std::int32_t SceneReadState() = 0;
  // vs_main_freeCdQueueSlot.
  virtual void ReleaseSceneRead() = 0;
  // vs_battle_initSceneAndGetRoomNames into _roomNamesTable.
  virtual void ResolveRoomNames() = 0;
  // vs_battle_setRoomsUnk0.
  virtual void RestoreRoomVisits() = 0;
  // _scaleRoomVertices, _getCurrentRoomIndex and _snapMapToRoom.
  virtual void ScaleRooms(std::int32_t factor) = 0;
  [[nodiscard]] virtual std::int32_t InitialRoom() = 0;
  virtual void CenterOnRoom(std::int32_t room) = 0;
  // SetFogNear over the projection distance, then SetFarColor.
  virtual void SetFogNear(std::int32_t near_distance) = 0;
  virtual void SetFarColor(std::int32_t red, std::int32_t green,
                           std::int32_t blue) = 0;
  // _setMenuItemMapName with the scene's name.
  virtual void ShowAreaName(std::int32_t scene, std::int32_t font_color) = 0;
};

// _loadArmFile's results; kRefused is the native refusal of a scene id below
// zero or past `scene_files`, where the original reads before or past its
// tables.
enum class SceneMapLoad : std::uint8_t { kLoading, kLoaded, kRefused };

// _loadArmFile: without a scene buffer, resets `view` for the current scene
// and starts reading its ARM file (kLoading); then, once the read has loaded,
// releases it and prepares the scene (names, visits, scale, the initial room
// centered, fog and the area name) (kLoaded).
[[nodiscard]] SceneMapLoad LoadSceneMap(SceneMapHost& host, SceneMapView& view,
                                        SceneMapProgress& progress,
                                        std::span<const DiscFileRange> files);
}  // namespace VS::Menu::map
