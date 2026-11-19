// Hand-maintained (tools/port/owned.csv).
#include "Menu/MapLoadingLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string_view>
#include <vector>

#include "Battle/StatusEffects.h"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Field/Map.h"
#include "Field/MapLive.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Map.h"
#include "Menu/MapLoading.hpp"
#include "Menu/MapNavigationLive.hpp"
#include "Menu/MapSceneStorageLive.hpp"
#include "Render/Camera.hpp"
#include "Render/DepthCueLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/Render.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"

namespace VS::Menu::map {
namespace {
// Menu_Map_mapNames' halfwords: the offsets, then the names.
constexpr std::size_t kMapNameHalfwords = std::size(Menu_Map_mapNames);
// A chooser row's two menu text entries: the name and no description.
constexpr std::size_t kMenuTextPerRow = 2;

static_assert(sizeof(Content_DiscFile) == sizeof(DiscFileRange));
static_assert(std::size(Menu_Map_sceneRowIdMap) == kSceneCount);
static_assert(sizeof(VS::State::stateFlags.map_paling) == kSceneCount);

std::uint64_t refused_calls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refused_calls)) {
    VS::Core::LogWarn("menu", "map loading: {} refused: {} ({} refusals)", call,
                      reason, refused_calls);
  }
}

// Scene `scene`'s name in Menu_Map_mapNames, or null when the scene or its
// offset is outside the table.
[[nodiscard]] char* AreaNameOf(std::int32_t scene) {
  if (scene < 0 || static_cast<std::size_t>(scene) >= kMapNameHalfwords) {
    return nullptr;
  }
  const std::size_t offset = Menu_Map_mapNames[scene];
  if (offset >= kMapNameHalfwords) return nullptr;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<char*>(&Menu_Map_mapNames[offset]);
}

[[nodiscard]] std::span<const DiscFileRange> SceneFiles() {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return {reinterpret_cast<const DiscFileRange*>(Menu_Map_kSceneArmFiles),
          static_cast<std::size_t>(kSceneCount)};
}

// vs_getXY: the packed VRAM position or size word.
[[nodiscard]] int PackedPair(std::int32_t low, std::int32_t high) {
  constexpr std::uint32_t kHalfBits = 16U;
  constexpr std::uint32_t kHalfMask = 0xFFFFU;
  return static_cast<int>((static_cast<std::uint32_t>(high) << kHalfBits) |
                          (static_cast<std::uint32_t>(low) & kHalfMask));
}

[[nodiscard]] Content_DiscFile DiscFileOf(DiscFileRange file) {
  return {.lba = file.sector, .size = file.bytes};
}

class LiveAreaMenu final : public AreaMenuHost {
 public:
  explicit LiveAreaMenu(char** menu_text) : menu_text_(menu_text) {}

  void ListAreas(std::span<const std::int32_t> areas) override {
    listed_text_.assign(kSceneCount * kMenuTextPerRow, nullptr);
    for (std::size_t row = 0; row < areas.size(); ++row) {
      listed_text_[row * kMenuTextPerRow] = AreaNameOf(areas[row]);
    }
    menu_text_ = listed_text_.data();
  }
  void SetAreaCursor(AreaListCursor cursor) override {
    VS::Menu::Framework::menuMemory.cursor_memories[0] = cursor.row;
    VS::Menu::Framework::menuMemory.cursor_memories[1] = cursor.first_row;
  }
  std::uint8_t AreaCursorRow() override {
    return VS::Menu::Framework::menuMemory.cursor_memories[0];
  }
  std::uint8_t CursorMemorySetting() override {
    return static_cast<std::uint8_t>(VS::Input::settings.cursor_memory);
  }
  void SetCursorMemorySetting(std::uint8_t setting) override {
    VS::Input::settings.cursor_memory = static_cast<char>(setting);
  }
  void ShowAreaRows(std::int32_t row_info,
                    std::span<const std::int32_t> row_types) override {
    std::vector<int> types(row_types.begin(), row_types.end());
    int no_rows = 0;
    Framework::CreateLiveMenuRows(static_cast<int>(row_types.size()), row_info,
                                  menu_text_,
                                  types.empty() ? &no_rows : types.data());
  }
  std::int32_t SelectedRow() override { return Framework::LiveSelectedRow(); }
  void ClearMenuExcept(std::int32_t row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void FlyOutAndHoist(std::int32_t row, std::int32_t offset) override {
    Framework::FlyOutLiveRowsAndHoistSelection(row, offset);
  }

 private:
  char** menu_text_;
  std::vector<char*> listed_text_;
};

class LiveMenuReady final : public MenuReadyHost {
 public:
  std::int32_t MenuRowsIdle() override {
    return Framework::AreLiveMenuRowsIdle();
  }
};

// The background's request, image and step: the original's statics.
struct BackgroundLoad {
  Content_DiscFile file{};
  Content_DiscQueueSlot* request = nullptr;
  std::uint64_t generation = 0;
  void* image = nullptr;
  std::int32_t step = 0;
};

BackgroundLoad background;
Content_DiscQueueSlot* initial_scene_request = nullptr;
std::uint64_t initial_scene_generation = 0;
void* initial_scene_destination = nullptr;
std::size_t initial_scene_bytes = 0;

class LiveBackground final : public BackgroundLoadHost {
 public:
  void AllocateBackgroundRead(DiscFileRange file) override {
    background.file = DiscFileOf(file);
    background.request = Content::AllocateLiveDiscRequest(&background.file);
    background.generation =
        Content::LiveDiscRequestGeneration(background.request);
  }
  void AllocateBackgroundImage(std::int32_t bytes) override {
    background.image =
        Core::Memory::AllocateLiveFromHighEnd(static_cast<std::size_t>(bytes));
  }
  void EnqueueBackgroundRead() override {
    if (background.request != nullptr) {
      Content::EnqueueLiveDiscRequest(background.request, background.image);
    } else {
      background.request = Content::CompleteLiveRefusedDiscRequest(
          &background.file, background.image);
    }
    background.generation =
        Content::LiveDiscRequestGeneration(background.request);
  }
  std::int32_t BackgroundReadState() override {
    if (background.request == nullptr || background.generation == 0 ||
        Content::LiveDiscRequestGeneration(background.request) !=
            background.generation) {
      Refuse("LoadBackground", "missing or reused background read");
      return Content_DiscRequestState_Free;
    }
    return background.request->state;
  }
  void ReleaseBackgroundRead() override {
    Content::ReleaseLiveDiscRequest(background.request);
    background.request = nullptr;
    background.generation = 0;
  }
  void LoadBackgroundClut(std::int32_t row, std::int32_t first,
                          std::int32_t count) override {
    Render::LoadLiveClutColors(Image(), static_cast<u_int>(row),
                               static_cast<u_int>(first),
                               static_cast<u_int>(count));
  }
  void UploadBackground(std::int32_t x, std::int32_t y, std::int32_t offset,
                        std::int32_t width, std::int32_t height) override {
    Render::UploadLiveImageRectangle(PackedPair(x, y),
                                     std::next(Image(), offset),
                                     PackedPair(width, height));
  }
  void SetBackgroundPriority(std::int32_t order_offset,
                             std::int32_t brightness) override {
    Framework::SetLiveBackgroundPriority(order_offset, brightness);
  }
  void FreeBackgroundImage() override {
    if (background.image == nullptr) {
      Refuse("LoadBackground", "background image already freed");
      return;
    }
    Core::Memory::FreeLiveFromHighEnd(background.image);
    background.image = nullptr;
  }

 private:
  [[nodiscard]] static const u_short* Image() {
    return static_cast<const u_short*>(background.image);
  }
};

class LiveSceneMap final : public SceneMapHost {
 public:
  bool HasSceneBuffer() override { return Battle_unusedSceneBuffer != nullptr; }
  std::uint16_t CameraYaw() override {
    return static_cast<std::uint16_t>(
        VS::Render::FrameScratchpad().camera.angles.vy);
  }
  std::int32_t CurrentSceneId() override { return Field::LiveCurrentSceneId(); }
  void AllocateSceneBuffer(std::int32_t bytes) override {
    Battle_unusedSceneBuffer =
        Core::Memory::AllocateLiveFromHighEnd(static_cast<std::size_t>(bytes));
    BindLiveMapScene(Battle_unusedSceneBuffer, static_cast<std::size_t>(bytes));
  }
  void AllocateSceneRead(std::int32_t scene) override {
    file_ = &Menu_Map_kSceneArmFiles[scene];
    Menu_Map_sceneCdQueueSlot = Content::AllocateLiveDiscRequest(file_);
  }
  void EnqueueSceneRead() override {
    if (Menu_Map_sceneCdQueueSlot != nullptr) {
      Content::EnqueueLiveDiscRequest(Menu_Map_sceneCdQueueSlot,
                                      Battle_unusedSceneBuffer);
    } else {
      Menu_Map_sceneCdQueueSlot = Content::CompleteLiveRefusedDiscRequest(
          file_, Battle_unusedSceneBuffer);
    }
    initial_scene_request = Menu_Map_sceneCdQueueSlot;
    initial_scene_generation =
        Content::LiveDiscRequestGeneration(initial_scene_request);
    initial_scene_destination = Battle_unusedSceneBuffer;
    initial_scene_bytes = static_cast<std::size_t>(file_->size);
  }
  std::int32_t SceneReadState() override {
    if (Menu_Map_sceneCdQueueSlot == nullptr ||
        Menu_Map_sceneCdQueueSlot != initial_scene_request ||
        initial_scene_generation == 0 ||
        Content::LiveDiscRequestGeneration(initial_scene_request) !=
            initial_scene_generation) {
      Refuse("LoadSceneMap", "missing or reused scene read");
      return Content_DiscRequestState_Free;
    }
    return Menu_Map_sceneCdQueueSlot->state;
  }
  void ReleaseSceneRead() override {
    Content::ReleaseLiveDiscRequest(Menu_Map_sceneCdQueueSlot);
    Menu_Map_sceneCdQueueSlot = nullptr;
    initial_scene_request = nullptr;
    initial_scene_generation = 0;
    initial_scene_destination = nullptr;
    initial_scene_bytes = 0;
  }
  void ResolveRoomNames() override {
    Menu_Map_roomNamesTable =
        Field::ResolveLiveSceneRoomData(Battle_unusedSceneBuffer, SceneBytes());
  }
  void RestoreRoomVisits() override {
    Field::RestoreLiveSceneRoomVisits(Battle_unusedSceneBuffer, SceneBytes());
  }
  void ScaleRooms(std::int32_t factor) override {
    LiveScaleRoomVertices(Scene(), factor);
  }
  std::int32_t InitialRoom() override { return LiveInitialRoomIndex(Scene()); }
  void CenterOnRoom(std::int32_t room) override {
    Menu_Map_currentRoomIndex = room;
    LiveCenterOnRoom(Scene(), room);
  }
  void SetFogNear(std::int32_t near_distance) override {
    Render::SetLiveFogNearDistance(near_distance,
                                   VS::Render::projectionDistance);
  }
  void SetFarColor(std::int32_t red, std::int32_t green,
                   std::int32_t blue) override {
    Core_Gte_SetFarColor(red, green, blue);
  }
  void ShowAreaName(std::int32_t scene, std::int32_t font_color) override {
    LiveShowAreaName(AreaNameOf(scene), static_cast<char>(font_color));
  }

 private:
  [[nodiscard]] static Field_MapScene* Scene() {
    return static_cast<Field_MapScene*>(Battle_unusedSceneBuffer);
  }
  // The loaded scene's ARM file size (Menu_Map_currentScene is checked
  // before the read is polled).
  [[nodiscard]] static std::size_t SceneBytes() {
    return static_cast<std::size_t>(
        Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);
  }

  const Content_DiscFile* file_ = nullptr;
};

[[nodiscard]] SceneMapView ViewOfGlobals() {
  const auto angles = [](const SVECTOR& value) {
    return MapAngles{
        .x = value.vx, .y = value.vy, .z = value.vz, .pad = value.pad};
  };
  return {.trigger_hide_ui = Menu_Map_triggerHideUi,
          .hide_ui = Menu_Map_hideUi,
          .camera_angles = angles(Menu_Map_cameraAngles),
          .camera_position = angles(Menu_Map_cameraPos),
          .current_room = Menu_Map_currentRoomIndex,
          .current_scene = Menu_Map_currentScene,
          .current_scene_row = Menu_Map_currentSceneRow,
          .menu_state = Menu_Map_menuState,
          .background_brightness = Menu_Map_backgroundBrightness,
          .menu_frames = Menu_Map_menuFramesCounter,
          .geometry_offset_x = Menu_Map_geomOffsetX,
          .geometry_offset_y = Menu_Map_geomOffsetY,
          .quit_state = Menu_Map_quitState,
          .camera_distance = Menu_Map_cameraDistance,
          .menu_commands_state = Menu_Map_menuCommandsState,
          .ui_animation_state = Menu_Map_uiAnimationState,
          .is_paling = Menu_Map_isPaling};
}

void StoreView(const SceneMapView& view) {
  const auto store = [](SVECTOR& target, MapAngles value) {
    target.vx = value.x;
    target.vy = value.y;
    target.vz = value.z;
    target.pad = value.pad;
  };
  Menu_Map_triggerHideUi = view.trigger_hide_ui;
  Menu_Map_hideUi = view.hide_ui;
  store(Menu_Map_cameraAngles, view.camera_angles);
  store(Menu_Map_cameraPos, view.camera_position);
  Menu_Map_currentRoomIndex = view.current_room;
  Menu_Map_currentScene = view.current_scene;
  Menu_Map_currentSceneRow = view.current_scene_row;
  Menu_Map_menuState = view.menu_state;
  Menu_Map_backgroundBrightness = view.background_brightness;
  Menu_Map_menuFramesCounter = view.menu_frames;
  Menu_Map_geomOffsetX = view.geometry_offset_x;
  Menu_Map_geomOffsetY = view.geometry_offset_y;
  Menu_Map_quitState = view.quit_state;
  Menu_Map_cameraDistance = view.camera_distance;
  Menu_Map_menuCommandsState = view.menu_commands_state;
  Menu_Map_uiAnimationState = view.ui_animation_state;
  Menu_Map_isPaling = view.is_paling;
}

AreaSelection area_selection;
}  // namespace

SceneMapView ReadLiveMapView() { return ViewOfGlobals(); }

void WriteLiveMapView(const SceneMapView& view) { StoreView(view); }

// decomp: MENU5.PRG _configureMapMenu (4D8.c:129).
void LiveConfigureAreaList(int row_count, int current_map, char* menu_text[]) {
  if (row_count < 0) {
    Refuse("ConfigureAreaList", "negative row count");
    return;
  }
  if (menu_text == nullptr && row_count > 0) {
    Refuse("ConfigureAreaList", "no menu text");
    return;
  }
  LiveAreaMenu host(menu_text);
  ConfigureAreaList(host, row_count, current_map, Menu_Map_currentMapRow);
}

// decomp: MENU5.PRG _mapSelection (4D8.c:175).
int LivePollAreaSelection() {
  LiveAreaMenu host(nullptr);
  return PollAreaSelection(host);
}

// decomp: MENU5.PRG _setMenuItemMapName (4D8.c:188).
void LiveShowAreaName(char* map_name, char font_color) {
  if (map_name == nullptr) {
    Refuse("ShowAreaName", "no area name");
    return;
  }
  const AreaNameRow area = AreaNameRowOf(font_color);
  Framework::MenuRow* row = Framework::SetLiveMenuItem(
      area.placement.row, area.placement.x, area.placement.y,
      area.placement.width, area.placement.background_width, map_name);
  if (row == nullptr) {
    Refuse("ShowAreaName", "no menu row");
    return;
  }
  row->state = area.state;
  row->target0 = area.target_x;
  row->selected = area.selected;
  row->font_color = area.font_color;
}

// decomp: MENU5.PRG _menuReady (4D8.c:204).
void LivePollMenuReady() {
  LiveMenuReady host;
  PollMenuReady(host);
}

// decomp: MENU5.PRG _loadMapBackground (4D8.c:206).
int LiveLoadBackground(int begin_background_load) {
  LiveBackground host;
  return LoadBackground(host, background.step, begin_background_load);
}

// decomp: MENU5.PRG _loadArmFile (4D8.c:448).
int LiveLoadSceneMap() {
  if (Battle_unusedSceneBuffer != nullptr &&
      (Menu_Map_currentScene < 0 || Menu_Map_currentScene >= kSceneCount)) {
    Refuse("LoadSceneMap", "scene outside the ARM files");
    return 0;
  }
  LiveSceneMap host;
  SceneMapView view = ViewOfGlobals();
  std::uint8_t* paling = VS::State::stateFlags.map_paling;
  SceneMapProgress progress{
      .map_paling = {paling, static_cast<std::size_t>(kSceneCount)},
      .room_flags = static_cast<std::uint32_t>(
          VS::State::roomVisitProgress.room_flags[1])};
  const SceneMapLoad result = LoadSceneMap(host, view, progress, SceneFiles());
  VS::State::roomVisitProgress.room_flags[1] =
      static_cast<int>(progress.room_flags);
  StoreView(view);
  if (result == SceneMapLoad::kRefused) {
    Refuse("LoadSceneMap", "scene outside the ARM files");
  }
  return result == SceneMapLoad::kLoaded ? 1 : 0;
}

// decomp: MENU5.PRG _mapSelectionMenu (4D8.c:979).
int LiveUpdateAreaSelection(int initialize) {
  if (initialize != 0 || area_selection.step == 0) {
    for (std::int32_t scene = 1; scene < kSceneCount; ++scene) {
      const bool visited = (static_cast<std::uint32_t>(
                                VS::State::roomVisitProgress.area_flags[0]) &
                            (1U << static_cast<unsigned>(scene))) != 0;
      if (visited && AreaNameOf(scene) == nullptr) {
        Refuse("UpdateAreaSelection", "area name outside the table");
        return kNoVisitedAreas;
      }
    }
  }
  const AreaSelectionInput input{
      .visited_areas = static_cast<std::uint32_t>(
          VS::State::roomVisitProgress.area_flags[0]),
      .current_scene = Menu_Map_currentScene,
      .current_scene_row = Menu_Map_currentSceneRow};
  LiveAreaMenu host(nullptr);
  return UpdateAreaSelection(host, area_selection, input,
                             Menu_Map_sceneRowIdMap, Menu_Map_currentMapRow,
                             initialize);
}

bool DrainMapLoadingLive() {
  if (background.image != nullptr) {
    const auto capacity = Core::Memory::LivePayloadCapacity(background.image);
    if (!capacity ||
        !Content::CancelLiveDiscWrites(
            {static_cast<std::byte*>(background.image), *capacity})) {
      Refuse("Drain", "background destination is not safe to release");
      return false;
    }
  }
  if (initial_scene_destination != nullptr) {
    const auto capacity =
        Core::Memory::LivePayloadCapacity(initial_scene_destination);
    if (!capacity || *capacity < initial_scene_bytes) {
      Refuse("Drain", "initial scene destination is not owned");
      return false;
    }
    const auto bytes = std::span{
        static_cast<std::byte*>(initial_scene_destination), *capacity};
    ClearLiveMapTextBorrow(bytes);
    if (!Content::CancelLiveDiscWrites(bytes)) {
      Refuse("Drain", "initial scene destination is not safe to release");
      return false;
    }
  }
  if (background.request != nullptr && background.generation != 0 &&
      Content::LiveDiscRequestGeneration(background.request) ==
          background.generation) {
    Content::ReleaseLiveDiscRequest(background.request);
  }
  if (background.image != nullptr) {
    Core::Memory::FreeLiveFromHighEnd(background.image);
  }
  background = {};
  if (initial_scene_request != nullptr && initial_scene_generation != 0 &&
      Content::LiveDiscRequestGeneration(initial_scene_request) ==
          initial_scene_generation) {
    Content::ReleaseLiveDiscRequest(initial_scene_request);
  }
  if (Menu_Map_sceneCdQueueSlot == initial_scene_request) {
    Menu_Map_sceneCdQueueSlot = nullptr;
  }
  if (initial_scene_destination != nullptr) {
    ForgetLiveMapScene(initial_scene_destination);
    if (Battle_unusedSceneBuffer == initial_scene_destination) {
      Battle_unusedSceneBuffer = nullptr;
      Menu_Map_roomNamesTable = nullptr;
    }
    Core::Memory::FreeLiveFromHighEnd(initial_scene_destination);
  }
  initial_scene_request = nullptr;
  initial_scene_generation = 0;
  initial_scene_destination = nullptr;
  initial_scene_bytes = 0;
  return true;
}

bool ResetMapLoadingModuleData() {
  if (background.image != nullptr || background.request != nullptr ||
      initial_scene_destination != nullptr ||
      initial_scene_request != nullptr) {
    Refuse("Reset", "drain map loads before restoring module data");
    return false;
  }
  background = {};
  area_selection = {};
  return true;
}
}  // namespace VS::Menu::map
