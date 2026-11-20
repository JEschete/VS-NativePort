#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Legacy/Psx.h"
#include "Menu/Map.hpp"
#include "Menu/MapLoading.hpp"
#include "Menu/MapLoadingLive.hpp"
#include "State/Progress.hpp"

namespace {
namespace map = VS::Menu::map;
using map::AreaListCursor;

class FakeAreaMenu final : public map::AreaMenuHost {
 public:
  void ListAreas(std::span<const std::int32_t> listed) override {
    areas.assign(listed.begin(), listed.end());
  }
  void SetAreaCursor(AreaListCursor value) override {
    cursor = value;
    events.emplace_back("cursor");
  }
  std::uint8_t AreaCursorRow() override { return cursor.row; }
  std::uint8_t CursorMemorySetting() override { return setting; }
  void SetCursorMemorySetting(std::uint8_t value) override {
    setting = value;
    events.push_back("setting " + std::to_string(value));
  }
  void ShowAreaRows(std::int32_t row_info,
                    std::span<const std::int32_t> types) override {
    info = row_info;
    row_types.assign(types.begin(), types.end());
    shown_setting = setting;
    events.emplace_back("rows");
  }
  std::int32_t SelectedRow() override { return selected; }
  void ClearMenuExcept(std::int32_t row) override {
    events.push_back("clear " + std::to_string(row));
  }
  void FlyOutAndHoist(std::int32_t row, std::int32_t offset) override {
    events.push_back("flyout " + std::to_string(row) + " " +
                     std::to_string(offset));
  }

  std::vector<std::int32_t> areas;
  AreaListCursor cursor;
  std::uint8_t setting = 0;
  std::uint8_t shown_setting = 0;
  std::int32_t info = 0;
  std::vector<std::int32_t> row_types;
  std::int32_t selected = -1;
  std::vector<std::string> events;
};

TEST_CASE("The area cursor scrolls past eight rows", "[map-loading]") {
  CHECK(map::AreaListCursorOf(8, 7) == AreaListCursor{.row = 7});
  CHECK(map::AreaListCursorOf(20, 7) == AreaListCursor{.row = 7});
  // Within the last eight rows the list shows its last page.
  CHECK(map::AreaListCursorOf(20, 12) ==
        AreaListCursor{.row = 1, .first_row = 11});
  CHECK(map::AreaListCursorOf(20, 19) ==
        AreaListCursor{.row = 8, .first_row = 11});
  // Otherwise the row is centered on the fifth line.
  CHECK(map::AreaListCursorOf(20, 11) ==
        AreaListCursor{.row = 4, .first_row = 7});
  // Only bits 0-6 choose the row.
  CHECK(map::AreaListCursorOf(20, 0x80 | 11) ==
        AreaListCursor{.row = 4, .first_row = 7});
  CHECK(map::AreaListCursorOf(4, 0x80) == AreaListCursor{});
}

TEST_CASE("Configuring the area list marks the remembered row",
          "[map-loading]") {
  FakeAreaMenu host;
  host.setting = 0;
  std::int32_t current_row = -1;
  map::ConfigureAreaList(host, 4, 2, current_row);
  CHECK(current_row == 2);
  CHECK(host.row_types == std::vector<std::int32_t>{0, 0, 4, 0});
  CHECK(host.info == map::kAreaRowInfo);
  CHECK(host.cursor == AreaListCursor{.row = 2});
  CHECK(host.shown_setting == 1);
  CHECK(host.setting == 0);
  CHECK(host.events ==
        std::vector<std::string>{"cursor", "setting 1", "rows", "setting 0"});
  // The remembered row stays even when the cursor moves.
  map::ConfigureAreaList(host, 4, 3, current_row);
  CHECK(host.row_types == std::vector<std::int32_t>{0, 0, 4, 0});
}

TEST_CASE("The no-current-area flag and rows outside the list mark nothing",
          "[map-loading]") {
  FakeAreaMenu host;
  std::int32_t current_row = -1;
  map::ConfigureAreaList(host, 3, map::kNoCurrentAreaFlag, current_row);
  CHECK(current_row == map::kNoCurrentAreaFlag);
  CHECK(host.row_types == std::vector<std::int32_t>{0, 0, 0});
  CHECK(host.cursor == AreaListCursor{});
  current_row = 5;
  map::ConfigureAreaList(host, 3, 1, current_row);
  CHECK(host.row_types == std::vector<std::int32_t>{0, 0, 0});
  current_row = -4;
  map::ConfigureAreaList(host, 3, 1, current_row);
  CHECK(host.row_types == std::vector<std::int32_t>{0, 0, 0});
  CHECK(current_row == -4);
}

TEST_CASE("Polling the chooser clears on cancellation and flies out a choice",
          "[map-loading]") {
  FakeAreaMenu host;
  host.cursor = {.row = 3};
  host.selected = -1;
  CHECK(map::PollAreaSelection(host) == -1);
  CHECK(host.events.empty());
  host.selected = -2;
  CHECK(map::PollAreaSelection(host) == -2);
  CHECK(host.events == std::vector<std::string>{"clear 5"});
  host.events.clear();
  host.selected = 6;
  CHECK(map::PollAreaSelection(host) == 6);
  CHECK(host.events == std::vector<std::string>{"flyout 3 1"});
}

TEST_CASE("The area chooser lists visited scenes and marks the current one",
          "[map-loading]") {
  FakeAreaMenu host;
  map::AreaSelection selection{.step = 7, .selected = 9};
  std::array<std::int32_t, map::kSceneCount> rows{};
  std::int32_t current_row = -1;
  // Scene 0 is never listed.
  const map::AreaSelectionInput input{
      .visited_areas = 0x80000025U, .current_scene = 5, .current_scene_row = 5};
  CHECK(map::UpdateAreaSelection(host, selection, input, rows, current_row,
                                 1) == map::kAreaSelectionPending);
  CHECK(selection.step == 1);
  CHECK(selection.selected == 0);
  CHECK(host.areas == std::vector<std::int32_t>{2, 5, 31});
  CHECK(rows[0] == 2);
  CHECK(rows[1] == 5);
  CHECK(rows[2] == 31);
  CHECK(current_row == 1);
  CHECK(host.row_types == std::vector<std::int32_t>{0, 4, 0});
}

TEST_CASE("The area chooser marks nothing without a current scene row",
          "[map-loading]") {
  FakeAreaMenu host;
  map::AreaSelection selection;
  std::array<std::int32_t, map::kSceneCount> rows{};
  std::int32_t current_row = -1;
  const map::AreaSelectionInput input{
      .visited_areas = 0x6U, .current_scene = 2, .current_scene_row = 0};
  CHECK(map::UpdateAreaSelection(host, selection, input, rows, current_row,
                                 0) == map::kAreaSelectionPending);
  CHECK(current_row == map::kNoCurrentAreaFlag);
  CHECK(host.row_types == std::vector<std::int32_t>{0, 0});
  CHECK(host.cursor == AreaListCursor{});
}

TEST_CASE("The area chooser without visited areas stays on its first step",
          "[map-loading]") {
  FakeAreaMenu host;
  map::AreaSelection selection;
  std::array<std::int32_t, map::kSceneCount> rows{};
  rows.fill(-7);
  std::int32_t current_row = -1;
  const map::AreaSelectionInput input{.visited_areas = 1U};
  CHECK(map::UpdateAreaSelection(host, selection, input, rows, current_row,
                                 1) == map::kNoVisitedAreas);
  CHECK(selection.step == 0);
  CHECK(rows[0] == -7);
  CHECK(host.events.empty());
  CHECK(current_row == -1);
  // Too few scene rows for the visited scenes is refused.
  std::array<std::int32_t, 1> short_rows{};
  const map::AreaSelectionInput two{.visited_areas = 0x6U};
  CHECK(map::UpdateAreaSelection(host, selection, two, short_rows, current_row,
                                 1) == map::kNoVisitedAreas);
  CHECK(host.events.empty());
}

TEST_CASE("The area chooser returns a choice or a cancellation",
          "[map-loading]") {
  FakeAreaMenu host;
  map::AreaSelection selection{.step = 1};
  std::array<std::int32_t, map::kSceneCount> rows{};
  std::int32_t current_row = 0;
  const map::AreaSelectionInput input{.visited_areas = 0x6U};
  host.selected = -1;
  CHECK(map::UpdateAreaSelection(host, selection, input, rows, current_row,
                                 0) == map::kAreaSelectionPending);
  CHECK(selection.step == 1);
  host.selected = -3;
  CHECK(map::UpdateAreaSelection(host, selection, input, rows, current_row,
                                 0) == -3);
  CHECK(selection.selected == -3);
  CHECK(host.events == std::vector<std::string>{"clear 5"});
  host.selected = 1;
  CHECK(map::UpdateAreaSelection(host, selection, input, rows, current_row,
                                 0) == 1);
  selection.step = 2;
  CHECK(map::UpdateAreaSelection(host, selection, input, rows, current_row,
                                 0) == map::kAreaSelectionPending);
}

TEST_CASE("The area name row slides in with the given colour",
          "[map-loading]") {
  const map::AreaNameRow row = map::AreaNameRowOf(0x103);
  CHECK(
      row.placement ==
      map::MenuRowPlacement{
          .row = 10, .x = 320, .y = 34, .width = 0x8C, .background_width = 8});
  CHECK(row.state == VS::Menu::Framework::RowState::kSlideX);
  CHECK(row.target_x == 0xB4);
  CHECK(row.selected == 1);
  CHECK(row.font_color == 3);
}

class FakeMenuReady final : public map::MenuReadyHost {
 public:
  std::int32_t MenuRowsIdle() override {
    ++polls;
    return 1;
  }
  int polls = 0;
};

TEST_CASE("The menu-ready wrapper polls once", "[map-loading]") {
  FakeMenuReady host;
  map::PollMenuReady(host);
  CHECK(host.polls == 1);
}

class FakeBackground final : public map::BackgroundLoadHost {
 public:
  void AllocateBackgroundRead(map::DiscFileRange file) override {
    events.push_back("read " + std::to_string(file.sector) + " " +
                     std::to_string(file.bytes));
  }
  void AllocateBackgroundImage(std::int32_t bytes) override {
    events.push_back("image " + std::to_string(bytes));
  }
  void EnqueueBackgroundRead() override { events.emplace_back("enqueue"); }
  std::int32_t BackgroundReadState() override { return state; }
  void ReleaseBackgroundRead() override { events.emplace_back("release"); }
  void LoadBackgroundClut(std::int32_t row, std::int32_t first,
                          std::int32_t count) override {
    events.push_back("clut " + std::to_string(row) + " " +
                     std::to_string(first) + " " + std::to_string(count));
  }
  void UploadBackground(std::int32_t x, std::int32_t y, std::int32_t offset,
                        std::int32_t width, std::int32_t height) override {
    events.push_back("upload " + std::to_string(x) + " " + std::to_string(y) +
                     " " + std::to_string(offset) + " " +
                     std::to_string(width) + " " + std::to_string(height));
  }
  void SetBackgroundPriority(std::int32_t order_offset,
                             std::int32_t brightness) override {
    events.push_back("priority " + std::to_string(order_offset) + " " +
                     std::to_string(brightness));
  }
  void FreeBackgroundImage() override { events.emplace_back("free"); }

  std::int32_t state = 0;
  std::vector<std::string> events;
};

TEST_CASE("The background read starts and waits", "[map-loading]") {
  FakeBackground host;
  std::int32_t step = 9;
  CHECK(map::LoadBackground(host, step, 1) == 0);
  CHECK(step == 0);
  CHECK(host.events ==
        std::vector<std::string>{"read 85088 77824", "image 77824", "enqueue"});
  host.events.clear();
  host.state = 3;
  CHECK(map::LoadBackground(host, step, 0) == 0);
  CHECK(host.events.empty());
  CHECK(step == 0);
}

TEST_CASE("The loaded background is shown then freed", "[map-loading]") {
  FakeBackground host;
  std::int32_t step = 0;
  host.state = map::kDiscReadLoaded;
  CHECK(map::LoadBackground(host, step, 0) == 0);
  CHECK(step == 1);
  CHECK(host.events == std::vector<std::string>{"release", "clut 3 0 256",
                                                "upload 640 256 256 160 240",
                                                "priority -4 128"});
  host.events.clear();
  CHECK(map::LoadBackground(host, step, 0) == 1);
  CHECK(host.events == std::vector<std::string>{"free"});
}

TEST_CASE("A background step outside both does nothing", "[map-loading]") {
  FakeBackground host;
  std::int32_t step = 2;
  host.state = map::kDiscReadLoaded;
  CHECK(map::LoadBackground(host, step, 0) == 0);
  step = -1;
  CHECK(map::LoadBackground(host, step, 0) == 0);
  CHECK(host.events.empty());
  CHECK(step == -1);
}

class FakeSceneMap final : public map::SceneMapHost {
 public:
  bool HasSceneBuffer() override { return buffer; }
  std::uint16_t CameraYaw() override { return yaw; }
  std::int32_t CurrentSceneId() override { return scene; }
  void AllocateSceneBuffer(std::int32_t bytes) override {
    buffer = true;
    events.push_back("buffer " + std::to_string(bytes));
  }
  void AllocateSceneRead(std::int32_t read_scene) override {
    events.push_back("read " + std::to_string(read_scene));
  }
  void EnqueueSceneRead() override { events.emplace_back("enqueue"); }
  std::int32_t SceneReadState() override { return state; }
  void ReleaseSceneRead() override { events.emplace_back("release"); }
  void ResolveRoomNames() override { events.emplace_back("names"); }
  void RestoreRoomVisits() override { events.emplace_back("visits"); }
  void ScaleRooms(std::int32_t factor) override {
    events.push_back("scale " + std::to_string(factor));
  }
  std::int32_t InitialRoom() override { return 6; }
  void CenterOnRoom(std::int32_t room) override {
    events.push_back("center " + std::to_string(room));
  }
  void SetFogNear(std::int32_t near_distance) override {
    events.push_back("fog " + std::to_string(near_distance));
  }
  void SetFarColor(std::int32_t red, std::int32_t green,
                   std::int32_t blue) override {
    events.push_back("far " + std::to_string(red) + std::to_string(green) +
                     std::to_string(blue));
  }
  void ShowAreaName(std::int32_t name_scene, std::int32_t color) override {
    events.push_back("name " + std::to_string(name_scene) + " " +
                     std::to_string(color));
  }

  bool buffer = false;
  std::uint16_t yaw = 0;
  std::int32_t scene = 0;
  std::int32_t state = 0;
  std::vector<std::string> events;
};

std::array<map::DiscFileRange, map::kSceneCount> SceneFiles() {
  std::array<map::DiscFileRange, map::kSceneCount> files{};
  for (std::size_t scene = 0; scene < files.size(); ++scene) {
    files[scene] = {.sector = static_cast<std::int32_t>(1000 + scene),
                    .bytes = static_cast<std::int32_t>(0x100 * (scene + 1))};
  }
  return files;
}

TEST_CASE("Opening the scene map resets the view and starts the read",
          "[map-loading]") {
  FakeSceneMap host;
  host.yaw = 0x900;
  host.scene = 3;
  std::array<std::uint8_t, map::kSceneCount> paling{};
  paling[3] = 7;
  map::SceneMapProgress progress{.map_paling = paling, .room_flags = 0x800001U};
  map::SceneMapView view;
  view.camera_position = {.x = 5, .y = 6, .z = 7, .pad = 8};
  view.current_room = 9;
  view.menu_state = 4;
  const auto files = SceneFiles();
  CHECK(map::LoadSceneMap(host, view, progress, files) ==
        map::SceneMapLoad::kLoading);
  CHECK(paling[0] == 1);
  CHECK(progress.room_flags == 0xC00001U);
  // 0x800 - 0x900 wraps to the halfword 0xFF00.
  map::SceneMapView expected;
  expected.camera_angles = {.x = 568, .y = -256, .z = -256, .pad = 0};
  expected.current_scene = 3;
  expected.current_scene_row = 3;
  expected.background_brightness = 128;
  expected.geometry_offset_x = -384;
  expected.quit_state = 1;
  expected.camera_distance = 0x300;
  expected.menu_commands_state = 5;
  expected.is_paling = 7;
  CHECK(view == expected);
  CHECK(host.events ==
        std::vector<std::string>{"buffer 1024", "read 3", "enqueue"});
}

TEST_CASE("Scenes past the ARM files fall back and below zero are refused",
          "[map-loading]") {
  FakeSceneMap host;
  host.scene = 32;
  std::array<std::uint8_t, map::kSceneCount> paling{};
  map::SceneMapProgress progress{.map_paling = paling, .room_flags = 1U};
  map::SceneMapView view;
  const auto files = SceneFiles();
  CHECK(map::LoadSceneMap(host, view, progress, files) ==
        map::SceneMapLoad::kLoading);
  CHECK(view.current_scene == 0);
  CHECK(progress.room_flags == 1U);
  CHECK(host.events ==
        std::vector<std::string>{"buffer 256", "read 0", "enqueue"});
  FakeSceneMap negative;
  negative.scene = -1;
  CHECK(map::LoadSceneMap(negative, view, progress, files) ==
        map::SceneMapLoad::kRefused);
  CHECK(negative.events.empty());
  map::SceneMapProgress none;
  CHECK(map::LoadSceneMap(negative, view, none, files) ==
        map::SceneMapLoad::kRefused);
}

TEST_CASE("The scene map waits for its read then prepares the rooms",
          "[map-loading]") {
  FakeSceneMap host;
  host.buffer = true;
  host.state = 2;
  std::array<std::uint8_t, map::kSceneCount> paling{};
  map::SceneMapProgress progress{.map_paling = paling};
  map::SceneMapView view;
  view.current_room = 1;
  view.current_scene = 12;
  const auto files = SceneFiles();
  CHECK(map::LoadSceneMap(host, view, progress, files) ==
        map::SceneMapLoad::kLoading);
  CHECK(host.events.empty());
  CHECK(paling[0] == 1);
  host.state = map::kDiscReadLoaded;
  CHECK(map::LoadSceneMap(host, view, progress, files) ==
        map::SceneMapLoad::kLoaded);
  CHECK(view.current_room == 6);
  CHECK(host.events == std::vector<std::string>{
                           "release", "names", "visits", "scale 4", "center 6",
                           "fog 768", "far 000", "name 12 1"});
}

struct LiveLoadingState {
  LiveLoadingState()
      : arm(Battle_unusedSceneBuffer),
        slot(map::sceneCdQueueSlot),
        scene(map::currentScene),
        scene_row(map::currentSceneRow),
        room(map::currentRoomIndex),
        map_row(map::currentMapRow),
        areas(VS::State::roomVisitProgress.area_flags[0]),
        rows(std::to_array(map::sceneRowIdMap)) {}
  LiveLoadingState(const LiveLoadingState&) = delete;
  LiveLoadingState& operator=(const LiveLoadingState&) = delete;
  LiveLoadingState(LiveLoadingState&&) = delete;
  LiveLoadingState& operator=(LiveLoadingState&&) = delete;
  ~LiveLoadingState() {
    Battle_unusedSceneBuffer = arm;
    map::sceneCdQueueSlot = slot;
    map::currentScene = scene;
    map::currentSceneRow = scene_row;
    map::currentRoomIndex = room;
    map::currentMapRow = map_row;
    VS::State::roomVisitProgress.area_flags[0] = areas;
    for (std::size_t row = 0; row < rows.size(); ++row) {
      map::sceneRowIdMap[row] = rows[row];
    }
  }

  void* arm;
  Content_DiscQueueSlot* slot;
  int scene;
  int scene_row;
  int room;
  int map_row;
  int areas;
  std::array<int, 32> rows;
};

TEST_CASE("The live chooser without visited areas lists nothing",
          "[map-loading-live]") {
  const LiveLoadingState saved;
  VS::State::roomVisitProgress.area_flags[0] = 1;
  map::sceneRowIdMap[0] = 17;
  map::currentMapRow = -1;
  CHECK(map::LiveUpdateAreaSelection(1) == map::kNoVisitedAreas);
  CHECK(map::sceneRowIdMap[0] == 17);
  CHECK(map::currentMapRow == -1);
}

TEST_CASE("The live area list refuses missing text and names",
          "[map-loading-live]") {
  const LiveLoadingState saved;
  map::currentMapRow = -1;
  map::LiveConfigureAreaList(2, 1, nullptr);
  map::LiveConfigureAreaList(-1, 1, nullptr);
  CHECK(map::currentMapRow == -1);
  map::LiveShowAreaName(nullptr, 1);
  map::LiveConfigureAreaList(-3, 0, nullptr);
  CHECK(map::currentMapRow == -1);
}

TEST_CASE("The live scene map refuses a missing read or scene",
          "[map-loading-live]") {
  const LiveLoadingState saved;
  std::array<std::byte, 16> buffer{};
  Battle_unusedSceneBuffer = buffer.data();
  map::currentRoomIndex = 3;
  map::currentScene = 40;
  CHECK(map::LiveLoadSceneMap() == 0);
  map::currentScene = -1;
  CHECK(map::LiveLoadSceneMap() == 0);
  map::currentScene = 2;
  map::sceneCdQueueSlot = nullptr;
  CHECK(map::LiveLoadSceneMap() == 0);
  CHECK(map::currentRoomIndex == 3);
  CHECK(map::currentScene == 2);
}

struct LiveBackgroundState {
  LiveBackgroundState()
      : counts(Content_discRequestCounts),
        lower_screen(Battle_lowerScreenUiState) {
    std::memcpy(slots.data(), Content_discRequestSlots,
                sizeof Content_discRequestSlots);
    std::memset(Content_discRequestSlots, 0, sizeof Content_discRequestSlots);
    Content_discRequestCounts = Content_DiscRequestCounts{};
  }
  LiveBackgroundState(const LiveBackgroundState&) = delete;
  LiveBackgroundState& operator=(const LiveBackgroundState&) = delete;
  LiveBackgroundState(LiveBackgroundState&&) = delete;
  LiveBackgroundState& operator=(LiveBackgroundState&&) = delete;
  ~LiveBackgroundState() {
    std::memcpy(Content_discRequestSlots, slots.data(),
                sizeof Content_discRequestSlots);
    Content_discRequestCounts = counts;
    Battle_lowerScreenUiState = lower_screen;
  }

  std::array<Content_DiscQueueSlot, 32> slots{};
  Content_DiscRequestCounts counts;
  char lower_screen;
  std::vector<std::byte> arena =
      std::vector<std::byte>(std::size_t{128} * 1024);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
};

[[nodiscard]] Content_DiscQueueSlot* RequestInUse() {
  for (Content_DiscQueueSlot& slot : Content_discRequestSlots) {
    if (slot.state != Content_DiscRequestState_Free) return &slot;
  }
  return nullptr;
}

// Begins the background read and polls it until shown; returns its image.
[[nodiscard]] void* ShowLiveBackground() {
  if (map::LiveLoadBackground(1) != 0) return nullptr;
  Content_DiscQueueSlot* const request = RequestInUse();
  if (request == nullptr) return nullptr;
  void* const image = request->vram;
  CHECK(map::LiveLoadBackground(0) == 0);
  request->state = map::kDiscReadLoaded;
  CHECK(map::LiveLoadBackground(0) == 0);
  CHECK(map::LiveLoadBackground(0) == 1);
  return image;
}

// The Live background keeps the original's statics, and this is the only test
// that drives them (Catch2 orders test cases randomly). #402: the original's
// shown step frees its image on every further poll (4D8.c:233-235); its only
// poller stops at the first 1 (4D8.c:271-277).
TEST_CASE("The live background waits for its read and frees its image once",
          "[map-loading-live]") {
  CHECK(map::LiveLoadBackground(0) == 0);
  const LiveBackgroundState saved;
  void* const image = ShowLiveBackground();
  REQUIRE(image != nullptr);
  if (image == nullptr) return;
  void* const reused = VS::Core::Memory::AllocateLiveFromHighEnd(
      static_cast<std::size_t>(map::kMapBackgroundFile.bytes));
  CHECK(reused == image);
  CHECK(map::LiveLoadBackground(0) == 1);
  CHECK(VS::Core::Memory::LivePayloadCapacity(reused).has_value());
  VS::Core::Memory::FreeLiveFromHighEnd(reused);
}
}  // namespace
