// Hand-maintained (tools/port/owned.csv).
#include "Menu/MapLoading.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::map {
namespace {
using Core::Math::WrappingSubtract;

constexpr std::int32_t kAreaRowMask = 0x7F;
constexpr std::int32_t kCenteredCursorRow = 4;
// The first scene the chooser lists; scene 0 is the dummy ARM file.
constexpr std::int32_t kFirstListedScene = 1;
// Row 10 at (320, 34), 0x8C wide with an 8-wide background.
constexpr MenuRowPlacement kAreaNamePlacement{10, 320, 34, 0x8C, 8};
constexpr auto kAreaNameState = Framework::RowState::kSlideX;
constexpr std::int16_t kAreaNameTargetX = 0xB4;

[[nodiscard]] std::uint8_t LowByte(std::int32_t value) {
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(value));
}
}  // namespace

// decomp: MENU5.PRG _configureMapMenu (4D8.c:129), the cursor.
AreaListCursor AreaListCursorOf(std::int32_t row_count, std::int32_t area) {
  const std::int32_t row = area & kAreaRowMask;
  if (row_count <= kVisibleAreaRows || row < kVisibleAreaRows) {
    return {.row = LowByte(row), .first_row = 0};
  }
  const std::int32_t last_first_row =
      WrappingSubtract(row_count, kVisibleAreaRows + 1);
  if (row >= WrappingSubtract(row_count, kVisibleAreaRows)) {
    return {.row = LowByte(WrappingSubtract(row, last_first_row)),
            .first_row = LowByte(last_first_row)};
  }
  return {.row = LowByte(kCenteredCursorRow),
          .first_row = LowByte(row - kCenteredCursorRow)};
}

// decomp: MENU5.PRG _configureMapMenu (4D8.c:129).
void ConfigureAreaList(AreaMenuHost& host, std::int32_t row_count,
                       std::int32_t current_area, std::int32_t& current_row) {
  if (current_row == -1) current_row = current_area;
  std::vector<std::int32_t> row_types(
      row_count > 0 ? static_cast<std::size_t>(row_count) : 0, 0);
  if ((current_area & kNoCurrentAreaFlag) == 0 && current_row >= 0 &&
      static_cast<std::size_t>(current_row) < row_types.size()) {
    row_types[static_cast<std::size_t>(current_row)] |= kCurrentAreaRowType;
  }
  host.SetAreaCursor(AreaListCursorOf(row_count, current_area));
  const std::uint8_t setting = host.CursorMemorySetting();
  host.SetCursorMemorySetting(1);
  host.ShowAreaRows(kAreaRowInfo, row_types);
  host.SetCursorMemorySetting(setting);
}

// decomp: MENU5.PRG _mapSelection (4D8.c:175).
std::int32_t PollAreaSelection(AreaMenuHost& host) {
  const std::int32_t row = host.SelectedRow();
  if (row < kAreaSelectionPending) {
    host.ClearMenuExcept(kAreaListKeptRow);
  } else if (row >= 0) {
    host.FlyOutAndHoist(host.AreaCursorRow(), 1);
  }
  return row;
}

// decomp: MENU5.PRG _mapSelectionMenu (4D8.c:979).
std::int32_t UpdateAreaSelection(AreaMenuHost& host, AreaSelection& selection,
                                 const AreaSelectionInput& input,
                                 std::span<std::int32_t> scene_rows,
                                 std::int32_t& current_row,
                                 std::int32_t initialize) {
  if (initialize != 0) selection = {};
  if (selection.step == 0) {
    std::vector<std::int32_t> areas;
    std::int32_t current_area = 0;
    for (std::int32_t scene = kFirstListedScene; scene < kSceneCount; ++scene) {
      if ((input.visited_areas & (1U << static_cast<unsigned>(scene))) == 0) {
        continue;
      }
      if (areas.size() >= scene_rows.size()) return kNoVisitedAreas;
      scene_rows[areas.size()] = scene;
      if (input.current_scene == scene) {
        current_area = static_cast<std::int32_t>(areas.size());
      }
      areas.push_back(scene);
    }
    if (areas.empty()) return kNoVisitedAreas;
    if (input.current_scene_row == 0) current_area = kNoCurrentAreaFlag;
    host.ListAreas(areas);
    ConfigureAreaList(host, static_cast<std::int32_t>(areas.size()),
                      current_area, current_row);
    ++selection.step;
    return kAreaSelectionPending;
  }
  if (selection.step == 1) {
    selection.selected = PollAreaSelection(host);
    if (selection.selected != kAreaSelectionPending) return selection.selected;
  }
  return kAreaSelectionPending;
}

// decomp: MENU5.PRG _setMenuItemMapName (4D8.c:188).
AreaNameRow AreaNameRowOf(std::int32_t font_color) {
  return {.placement = kAreaNamePlacement,
          .state = kAreaNameState,
          .target_x = kAreaNameTargetX,
          .selected = 1,
          .font_color = LowByte(font_color)};
}

// decomp: MENU5.PRG _menuReady (4D8.c:204).
void PollMenuReady(MenuReadyHost& host) {
  static_cast<void>(host.MenuRowsIdle());
}

// decomp: MENU5.PRG _loadMapBackground (4D8.c:206).
std::int32_t LoadBackground(BackgroundLoadHost& host, std::int32_t& step,
                            std::int32_t begin) {
  if (begin != 0) {
    host.AllocateBackgroundRead(kMapBackgroundFile);
    host.AllocateBackgroundImage(kMapBackgroundFile.bytes);
    host.EnqueueBackgroundRead();
    step = static_cast<std::int32_t>(BackgroundStep::kReading);
    return 0;
  }
  switch (static_cast<BackgroundStep>(step)) {
    case BackgroundStep::kReading:
      if (host.BackgroundReadState() == kDiscReadLoaded) {
        host.ReleaseBackgroundRead();
        host.LoadBackgroundClut(kBackgroundClutRow, 0, kBackgroundClutColors);
        host.UploadBackground(kBackgroundX, kBackgroundY,
                              kBackgroundImageOffset, kBackgroundWidth,
                              kBackgroundHeight);
        host.SetBackgroundPriority(kBackgroundOrderOffset,
                                   kBackgroundBrightness);
        step = static_cast<std::int32_t>(BackgroundStep::kShown);
      }
      return 0;
    case BackgroundStep::kShown:
      host.FreeBackgroundImage();
      return 1;
    default:
      return 0;
  }
}

// decomp: MENU5.PRG _loadArmFile (4D8.c:448).
SceneMapLoad LoadSceneMap(SceneMapHost& host, SceneMapView& view,
                          SceneMapProgress& progress,
                          std::span<const DiscFileRange> files) {
  if (progress.map_paling.empty()) return SceneMapLoad::kRefused;
  progress.map_paling[0] = 1;
  if (!host.HasSceneBuffer()) {
    if ((progress.room_flags & kRestoreRoomFlag) != 0) {
      progress.room_flags |= kRestoredRoomFlag;
    }
    view.trigger_hide_ui = 0;
    view.hide_ui = 0;
    view.camera_angles = {};
    view.camera_position = {};
    view.camera_angles.x = kMapCameraPitch;
    view.current_room = 0;
    const auto yaw = static_cast<std::int16_t>(static_cast<std::uint16_t>(
        WrappingSubtract(kMapYawBase, host.CameraYaw())));
    view.camera_angles.y = yaw;
    view.camera_angles.z = yaw;
    std::int32_t scene = host.CurrentSceneId();
    if (scene >= kSceneCount) scene = 0;
    view.current_scene = scene;
    if (scene < 0 || static_cast<std::size_t>(scene) >= files.size() ||
        static_cast<std::size_t>(scene) >= progress.map_paling.size()) {
      return SceneMapLoad::kRefused;
    }
    const auto at = static_cast<std::size_t>(scene);
    view.current_scene_row = scene;
    view.menu_state = 0;
    view.background_brightness = kMapBackgroundBrightness;
    view.menu_frames = 0;
    view.geometry_offset_x = kMapGeometryOffsetX;
    view.geometry_offset_y = 0;
    view.quit_state = 1;
    view.camera_distance = kMapCameraDistance;
    view.menu_commands_state = kMapCommandsState;
    view.ui_animation_state = 0;
    view.is_paling = progress.map_paling[at];
    host.AllocateSceneBuffer(files[at].bytes);
    host.AllocateSceneRead(scene);
    host.EnqueueSceneRead();
    return SceneMapLoad::kLoading;
  }
  if (host.SceneReadState() != kDiscReadLoaded) return SceneMapLoad::kLoading;
  host.ReleaseSceneRead();
  host.ResolveRoomNames();
  host.RestoreRoomVisits();
  host.ScaleRooms(kMapRoomScale);
  view.current_room = host.InitialRoom();
  host.CenterOnRoom(view.current_room);
  host.SetFogNear(kMapFogNear);
  host.SetFarColor(0, 0, 0);
  host.ShowAreaName(view.current_scene, kCurrentAreaNameColor);
  return SceneMapLoad::kLoaded;
}
}  // namespace VS::Menu::map
