#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Audio/SoundPlaybackLive.hpp"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Boot/OverlayReset.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscReaderLive.hpp"
#include "Core/Gte.h"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Event/ScriptVm.h"
#include "Field/BackgroundLive.hpp"
#include "Field/BackgroundLiveState.hpp"
#include "Field/Doors.h"
#include "Field/EnvironmentLive.hpp"
#include "Field/Gimmicks.h"
#include "Field/Manual.h"
#include "Field/Map.h"
#include "Field/Minimap.h"
#include "Field/MinimapScene.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Field/RoomStateLive.hpp"
#include "Field/RoomStateLiveState.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/Map.h"
#include "Menu/MapDrawingLive.hpp"
#include "Menu/MapLoadingLive.hpp"
#include "Menu/MapNavigationLive.hpp"
#include "Menu/MapSceneStorageLive.hpp"
#include "Menu/MapTerminalLive.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/PageSpace.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Save/MenuAssetsLive.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"

namespace {
namespace map = VS::Menu::map;
namespace content = VS::Content;
namespace memory = VS::Core::Memory;
namespace framework = VS::Menu::Framework;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
constexpr std::size_t kPrimitiveWords = 0x20000 / sizeof(u_long);
constexpr std::size_t kSceneEntry = 18;
constexpr std::uint32_t kUnwritten = 0xDEADBEEF;
constexpr std::uint32_t kLinkMask = 0xFFFFFF;
constexpr std::size_t kTickBudget = 128;
constexpr std::byte kCancelledReadPoison{0xA5};
constexpr int kMapScreen = static_cast<int>(framework::Submenu::kMap) + 1;

struct RestoreMapView {
  map::SceneMapView view = map::ReadLiveMapView();
  ~RestoreMapView() { map::WriteLiveMapView(view); }
};

struct RestorePages {
  VS::Render::PageSpace pages = VS::Render::GamePages();
  ~RestorePages() { VS::Render::GamePages() = std::move(pages); }
};

struct HeapBlocker {
  void* payload = nullptr;

  HeapBlocker() {
    const std::size_t units = memory::GameHeap().LargestFreeUnits();
    REQUIRE(units > 1);
    payload =
        memory::AllocateLiveFromHighEnd((units - 1) * memory::kHeapUnitBytes);
    REQUIRE(payload != nullptr);
  }
  ~HeapBlocker() { Release(); }
  void Release() {
    if (payload != nullptr) {
      memory::FreeLiveFromHighEnd(payload);
      payload = nullptr;
    }
  }
};

struct LiveWorld {
  RestoreMapView saved_view;
  ValueRestore<void*> saved_scene{Battle_unusedSceneBuffer};
  ValueRestore<void*> saved_staging{Menu_Map_armFileBuff};
  ValueRestore<Content_DiscQueueSlot*> saved_scene_slot{
      Menu_Map_sceneCdQueueSlot};
  ValueRestore<Field_MapRoomName*> saved_names{Menu_Map_roomNamesTable};
  ValueRestore<int> saved_map_row{Menu_Map_currentMapRow};
  ValueRestore<int> saved_selected{Menu_Map_selectedMap};
  ValueRestore<int> saved_label_y{Menu_Map_mapLabelY};
  ValueRestore<int> saved_label_count{Menu_Map_mapLabelCount};
  ValueRestore<int> saved_current{Menu_Map_isCurrentScene};
  ValueRestore<SVECTOR> saved_center{Menu_Map_centerPoint};
  ArrayRestore<int, 32> saved_scene_rows{Menu_Map_sceneRowIdMap};
  ArrayRestore<u_short, 282> saved_map_names{Menu_Map_mapNames};
  ArrayRestore<Menu_Map_CompassBound, 4> saved_compass{Menu_Map_compassBounds};
  ArrayRestore<Menu_Map_MapLabel, 8> saved_labels{Menu_Map_mapLabels};
  ArrayRestore<std::int16_t, 2> saved_padding1{Menu_Map_layoutPadding1};
  ArrayRestore<int, 2> saved_padding2{Menu_Map_layoutPadding2};
  ValueRestore<VS::Render::FrameScratch> saved_frame{
      VS::Render::FrameScratchpad()};
  ArrayRestore<void*, 2> saved_buffers{VS::Render::legacyPrimitiveBuffers};
  ArrayRestore<void*, 2> saved_tables{VS::Render::legacyOrderingTables};
  ValueRestore<int> saved_framebuffer{VS::Render::frameBuf};
  ValueRestore<decltype(Field_roomData)> saved_room{Field_roomData};
  ValueRestore<decltype(VS::State::stateFlags)> saved_progress{
      VS::State::stateFlags};
  ValueRestore<decltype(VS::State::roomVisitProgress)> saved_visits{
      VS::State::roomVisitProgress};
  ValueRestore<decltype(VS::Input::buttonsPressed)> saved_pressed{
      VS::Input::buttonsPressed};
  ValueRestore<int> saved_previous{VS::Input::previousButtons};
  ValueRestore<int> saved_repeat{VS::Input::buttonRepeat};
  ValueRestore<decltype(VS::Input::stickPosBuf)> saved_sticks{
      VS::Input::stickPosBuf};
  ValueRestore<decltype(VS::Menu::Framework::menuState)> saved_menu{
      VS::Menu::Framework::menuState};
  ValueRestore<Menu_Framework_MenuRow*> saved_rows{
      VS::Menu::Framework::menuItems};
  ValueRestore<char> saved_hide{VS::Menu::Framework::hideMenu};
  ValueRestore<int> saved_fade{VS::Menu::Framework::backgroundFadeStep};
  ValueRestore<decltype(Battle_hudHidden)> saved_hud{Battle_hudHidden};
  ValueRestore<int> saved_hud_slide{Battle_hudSlideOffset};
  ValueRestore<decltype(VS::Field::isFieldRenderingEnabled)> saved_field{
      VS::Field::isFieldRenderingEnabled};
  RestorePages saved_pages;
  ArrayRestore<Menu_TextBoxState, std::size(Menu_textBoxes)> saved_boxes{
      Menu_textBoxes};
  ArrayRestore<Content_DiscQueueSlot, 32> saved_requests{
      Content_discRequestSlots};
  ValueRestore<Content_DiscRequestCounts> saved_counts{
      Content_discRequestCounts};
  ValueRestore<Content_DiscDrive> saved_drive{Content_discDriveState};
  content::DataStore::ScopedOverride store;
  std::vector<std::byte> arena = std::vector<std::byte>(0x80000);
  memory::ScopedLiveGameHeap heap{arena};
  std::vector<u_long> primitives =
      std::vector<u_long>(kPrimitiveWords + 2, kUnwritten);
  std::array<u_long, 2084> ordering{};
  std::array<Menu_Framework_MenuRow, framework::kRowCount> rows{};
  std::vector<std::uint8_t> minimap;
  Field_MpdEnvironment environment{};
  std::array<std::uint32_t, 0x180> sound_offsets{};
  std::array<std::uint16_t, 0x180> sound_banks{};
  VS::Audio::Playback::ScopedLiveEffectTables sounds{
      {.program_offsets = sound_offsets,
       .bank_selectors = sound_banks,
       .program_data = {}}};

  explicit LiveWorld(bool data = false) {
    REQUIRE(Battle_unusedSceneBuffer == nullptr);
    if (data && !content::DataStore::Open(R"(Native\data)")) {
      throw std::runtime_error(
          "Map terminal Live integration needs extracted data");
    }
    content::ResetLiveDiscDrive();
    content::InitializeLiveDiscQueue();
    REQUIRE(map::ResetMapTerminalColdState());
    framework::ResetLiveMainMenuStatics();
    VS::Menu::Framework::menuItems = rows.data();
    VS::Menu::Framework::menuState = {};
    VS::Menu::Framework::backgroundFadeStep = 0;
    VS::Menu::Framework::hideMenu = 0;
    Battle_hudHidden = 0;
    Battle_hudSlideOffset = 0;
    VS::Field::isFieldRenderingEnabled = 1;
    VS::Input::buttonsPressed = 0;
    VS::Input::previousButtons = 0;
    VS::Input::buttonRepeat = 0;
    VS::Input::stickPosBuf.l_stick_x = 128;
    VS::Input::stickPosBuf.l_stick_y = 128;
    VS::Input::stickPosBuf.r_stick_x = 128;
    VS::Input::stickPosBuf.r_stick_y = 128;
    sound_offsets.fill(std::numeric_limits<std::uint32_t>::max());
    std::ranges::fill(VS::State::stateFlags.map_paling, 0);
    std::ranges::fill(Menu_textBoxes, Menu_TextBoxState{});
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
    Core_Gte_SetGeomOffset(160, 112);
    VS::Render::frameBuf = 0;
    for (int buffer = 0; buffer < 2; ++buffer) {
      VS::Render::legacyPrimitiveBuffers[buffer] = primitives.data() + 1;
      VS::Render::legacyOrderingTables[buffer] = ordering.data();
    }
    ResetFrame();
    if (data) {
      const auto* entry = content::DataStore::Get()->Find("MENU/MENU5.PRG");
      REQUIRE(entry != nullptr);
      REQUIRE(content::DataStore::Get()->Read(
          *entry, 0x5BFC,
          {reinterpret_cast<std::uint8_t*>(Menu_Map_mapNames),
           sizeof(Menu_Map_mapNames)}));
      minimap = SceneFile(1);
      Field_roomData.minimapScene =
          reinterpret_cast<Field_MapScene*>(minimap.data());
      Field_roomData.header.minimapSceneLength =
          static_cast<u_int>(minimap.size());
      environment.sceneId = 1;
      Field_roomData.environment = &environment;
      Field_roomData.header.environmentLength = sizeof(environment);
    }
  }

  ~LiveWorld() {
    if (!map::ResetMapTerminalColdState()) std::terminate();
    if (VS::Menu::Framework::itemsText != nullptr) {
      static_cast<void>(framework::CloseLiveRowsAndRememberCursor());
    }
    framework::ClearLiveMenuExcept(framework::kNoRow);
    framework::ResetLiveMainMenuStatics();
    content::ResetLiveDiscDrive();
    content::InitializeLiveDiscQueue();
    Core_Gte_Reset();
  }

  void ResetFrame() {
    VS::Render::FrameScratchpad().primitive_cursor = primitives.data() + 1;
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + kSceneEntry;
    VS::Render::FrameScratchpad().overlay_ordering_table = ordering.data() + 2;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }

  static std::vector<std::uint8_t> SceneFile(int scene) {
    const auto* data = content::DataStore::Get();
    REQUIRE(data != nullptr);
    const auto* entry = data->Find(std::format("SMALL/SCEN{:03}.ARM", scene));
    REQUIRE(entry != nullptr);
    const auto bytes = data->ReadAll(*entry);
    REQUIRE(bytes.has_value());
    return *bytes;
  }

  void LoadScene(int scene) {
    const auto file = SceneFile(scene);
    const auto length =
        static_cast<std::size_t>(Menu_Map_kSceneArmFiles[scene].size);
    auto* buffer = memory::AllocateLiveFromHighEnd(length);
    REQUIRE(buffer != nullptr);
    std::memset(buffer, 0, length);
    REQUIRE(file.size() <= length);
    std::memcpy(buffer, file.data(), file.size());
    REQUIRE(VS::Field::IsValidMapScene(
        {static_cast<const std::byte*>(buffer), length}));
    Battle_unusedSceneBuffer = buffer;
    map::BindLiveMapScene(buffer, length);
    Menu_Map_roomNamesTable =
        VS::Field::ResolveLiveSceneRoomData(buffer, length);
    REQUIRE(Menu_Map_roomNamesTable != nullptr);
    Menu_Map_currentScene = scene;
    Menu_Map_currentSceneRow = scene;
    Menu_Map_currentRoomIndex = 0;
    Menu_Map_menuState = 1;
    Menu_Map_backgroundBrightness = 32;
    Menu_Map_quitState = 1;
    Menu_Map_cameraDistance = 768;
    Menu_Map_cameraAngles = {568, 2048, 0, 0};
    Menu_Map_centerPoint = {};
    map::LiveMarkAllRoomsVisited(static_cast<int*>(buffer));
    VS::Menu::InitLiveTextBox(0, 0, 60, 224, 16, 1, 60, 224);
  }

  int Tick(std::uint32_t pressed = 0, int held = 0, int repeat = 0,
           bool framework_tick = false) {
    ResetFrame();
    VS::Input::buttonsPressed = pressed;
    VS::Input::previousButtons = held;
    VS::Input::buttonRepeat = repeat;
    if (framework_tick) framework::UpdateLiveScreen();
    return map::LiveUpdateMapMenu();
  }

  bool ReachInput(std::uint8_t& screen_step) {
    for (std::size_t tick = 0; tick < kTickBudget; ++tick) {
      content::UpdateLiveDiscQueue();
      ResetFrame();
      framework::UpdateLiveScreen();
      if (map::LiveUpdateScreen(&screen_step) != 0) return false;
      if (screen_step == 7 && Menu_Map_menuState == 1) return true;
    }
    return false;
  }

  bool OpenChooser() {
    VS::State::roomVisitProgress.area_flags[0] = (1 << 1) | (1 << 11);
    if (Tick(0x20) != 0 || Menu_Map_menuState != 2) return false;
    for (std::size_t tick = 0; tick < 40; ++tick) Tick(0, 0, 0, true);
    return Menu_Map_menuState == 2;
  }

  bool ChooseOtherScene() {
    const int original = Menu_Map_currentScene;
    if (!OpenChooser()) return false;
    Tick(0, original == 1 ? 0x4000 : 0x1000, original == 1 ? 0x4000 : 0x1000,
         true);
    Tick(0x20, 0, 0, true);
    for (std::size_t tick = 0; tick < 40; ++tick) {
      if (Menu_Map_menuState == 4) return true;
      Tick(0, 0, 0, true);
    }
    return false;
  }
};

void CheckNoPendingRead() {
  CHECK(Menu_Map_armFileBuff == nullptr);
  CHECK(Menu_Map_sceneCdQueueSlot == nullptr);
}

void CheckNoSceneOwners() {
  CHECK(Battle_unusedSceneBuffer == nullptr);
  CHECK(Menu_Map_roomNamesTable == nullptr);
  CheckNoPendingRead();
  CHECK(VS::Menu::ReadLiveTextBox(0).text == nullptr);
}

void HideResetAndCancel(LiveWorld& world) {
  REQUIRE(world.Tick(0x100) == 0);
  CHECK(std::pair<int, int>{Menu_Map_triggerHideUi,
                            VS::Menu::Framework::hideMenu} == std::pair{1, 1});
  REQUIRE(world.Tick(0, 0x800) == 0);
  CHECK(std::pair<int, int>{Menu_Map_geomOffsetX, Menu_Map_cameraDistance} ==
        std::pair{0, 768});
  REQUIRE(world.Tick(0x40) == 0);
  CHECK(Menu_Map_menuState == 3);
  int result = 0;
  for (int tick = 0; tick < 8 && result == 0; ++tick) result = world.Tick();
  CHECK(result == 1);
  CheckNoSceneOwners();
}

void ExitScreen(LiveWorld& world, std::uint8_t& step) {
  VS::Input::buttonsPressed = 0x10;
  REQUIRE(map::LiveUpdateScreen(&step) == 0);
  VS::Input::buttonsPressed = 0;
  int result = 0;
  for (std::size_t tick = 0; tick < kTickBudget && result == 0; ++tick) {
    world.ResetFrame();
    framework::UpdateLiveScreen();
    result = map::LiveUpdateScreen(&step);
  }
  CHECK(result == 1);
  CHECK(step == 0);
}

void CheckRestoredDisplay() {
  CHECK(VS::Field::isFieldRenderingEnabled == 1);
  CHECK(Battle_hudHidden == 0);
  CHECK(Battle_hudSlideOffset == 0);
  CHECK(VS::Menu::Framework::hideMenu == 0);
}

void CheckRestoredPixels() {
  CHECK(VS::Render::GamePages().At(672, 256) == 0x1234);
  CHECK(VS::Render::GamePages().At(767, 495) == 0x1234);
  CHECK(VS::Render::GamePages().At(640, 256) == 0);
  CHECK(VS::Render::GamePages().At(768, 256) == 0);
  CHECK(VS::Render::GamePages().At(768, 227) == 0x4321);
}

void CheckResetExports() {
  CHECK(map::ReadLiveMapView() == map::SceneMapView{});
  CHECK(Menu_Map_currentMapRow == -1);
  CHECK(std::to_array<int>({Menu_Map_selectedMap, Menu_Map_layoutPadding1[0],
                            Menu_Map_layoutPadding2[0], Menu_Map_mapLabelCount,
                            Menu_Map_mapLabels[0].x,
                            Menu_Map_compassBounds[0].x}) ==
        std::array{0, 0, 0, 0, 0, 0});
  CheckNoSceneOwners();
}

void CheckPreservedData(
    const decltype(std::to_array(Menu_Map_mapNames))& names,
    const decltype(VS::Render::FrameScratchpad().camera)& camera) {
  CHECK(std::to_array(Menu_Map_mapNames) == names);
  CHECK(VS::State::stateFlags.map_paling[5] == 19);
  CHECK(std::memcmp(&VS::Render::FrameScratchpad().camera, &camera,
                    sizeof(camera)) == 0);
}

void CheckReleasedInitialRead(void* buffer, Content_DiscQueueSlot* request,
                              std::uint64_t generation) {
  CHECK_FALSE(map::BoundLiveMapScene(buffer));
  CHECK_FALSE(memory::LivePayloadCapacity(buffer));
  CHECK(content::LiveDiscRequestGeneration(request) != generation);
}

Content_DiscQueueSlot* RecycleRequest(Content_DiscQueueSlot* request,
                                      void* destination) {
  const auto generation = content::LiveDiscRequestGeneration(request);
  const auto capacity = memory::LivePayloadCapacity(destination);
  REQUIRE(capacity.has_value());
  REQUIRE(content::CancelLiveDiscWrites(
      {static_cast<std::byte*>(destination), *capacity}));
  content::ReleaseLiveDiscRequest(request);
  const Content_DiscFile other_file{86100, 2048};
  auto* replacement = content::AllocateLiveDiscRequest(&other_file);
  REQUIRE(replacement == request);
  REQUIRE(content::LiveDiscRequestGeneration(replacement) != generation);
  return replacement;
}

std::array<std::int32_t, 3> TransformOrigin() {
  const SVECTOR origin{};
  VECTOR translated{};
  Core_Gte_LoadV0(&origin);
  Core_Gte_Rtv0Tr();
  Core_Gte_StoreLvnl(&translated);
  return {translated.vx, translated.vy, translated.vz};
}

void SetFieldView() {
  VS::Render::FrameScratchpad().view_matrix = {};
  for (int axis = 0; axis < 3; ++axis) {
    VS::Render::FrameScratchpad().view_matrix.m[axis][axis] = 4096;
  }
  VS::Render::FrameScratchpad().view_matrix.t[0] = 77;
  VS::Render::FrameScratchpad().view_matrix.t[1] = 88;
  VS::Render::FrameScratchpad().view_matrix.t[2] = 99;
  Core_Gte_SetGeomOffset(171, 93);
}

std::array<std::uint8_t, 30> OriginalPalingCaption() {
  const auto* data = content::DataStore::Get();
  REQUIRE(data != nullptr);
  const auto* entry = data->Find("MENU/MENU5.PRG");
  REQUIRE(entry != nullptr);
  std::array<std::uint8_t, 30> bytes{};
  REQUIRE(data->Read(*entry, 0x64C6, bytes));
  return bytes;
}

void CheckSceneExtent(void* scene, std::size_t bytes) {
  const auto bound = map::BoundLiveMapScene(scene);
  if (bound) {
    CHECK(bound->size() == bytes);
  } else {
    FAIL("The loaded scene has no bound extent");
  }
}

void CheckPendingExtent(void* old, int initial) {
  CHECK(Menu_Map_currentScene == (initial == 1 ? 11 : 1));
  CHECK(Battle_unusedSceneBuffer == old);
  CheckSceneExtent(
      old, static_cast<std::size_t>(Menu_Map_kSceneArmFiles[initial].size));
}

void KeepDrawingWhileReading(LiveWorld& world, void* old, std::size_t size,
                             void* staging) {
  REQUIRE(world.Tick(0x40) == 0);
  CHECK(Menu_Map_menuState == 4);
  CHECK(Menu_Map_armFileBuff == staging);
  const std::vector<std::byte> before(static_cast<std::byte*>(old),
                                      static_cast<std::byte*>(old) + size);
  map::LiveScaleRoomVertices(static_cast<Field_MapScene*>(old), 2);
  CHECK(std::memcmp(before.data(), old, size) != 0);
  map::LiveSetRoomCenterTarget(static_cast<Field_MapScene*>(old), 0);
  map::LiveAnimateCentering(static_cast<Field_MapScene*>(old));
}

void CheckDrainedRead(void* old, void* staging, Content_DiscQueueSlot* request,
                      std::size_t staging_bytes, std::uint64_t refusals) {
  CheckNoSceneOwners();
  CHECK(content::LiveDiscRequestGeneration(request) == 0);
  CHECK_FALSE(memory::LivePayloadCapacity(old));
  CHECK_FALSE(memory::LivePayloadCapacity(staging));
  CHECK(memory::RefusedLiveHeapRequestCount() == refusals);
  const auto discarded =
      std::span{static_cast<std::byte*>(staging), staging_bytes};
  std::ranges::fill(discarded, kCancelledReadPoison);
  for (std::size_t tick = 0; tick < kTickBudget; ++tick) {
    content::UpdateLiveDiscQueue();
  }
  CHECK(std::ranges::all_of(discarded, [](std::byte value) {
    return value == kCancelledReadPoison;
  }));
  CHECK(memory::GameHeap().Empty());
}

void FailSwapAllocation(LiveWorld& world, void* staging) {
  const auto refusals = memory::RefusedLiveHeapRequestCount();
  for (int tick = 0; tick < 8 && Battle_unusedSceneBuffer != nullptr; ++tick) {
    REQUIRE(world.Tick() == 0);
  }
  REQUIRE(Battle_unusedSceneBuffer == nullptr);
  CHECK(Menu_Map_menuState == 4);
  CHECK(Menu_Map_armFileBuff == staging);
  CHECK(Menu_Map_sceneCdQueueSlot == nullptr);
  CHECK(memory::LivePayloadCapacity(staging).has_value());
  CHECK(memory::RefusedLiveHeapRequestCount() > refusals);
}
}  // namespace

TEST_CASE("F02699 Live packets preserve untouched RGB and native chain heads",
          "[map-terminal][map-terminal-live][F02699]") {
  LiveWorld world;
  VS::Render::frameBuf = GENERATE(0, 1);
  auto& head = world.ordering[kSceneEntry + 2046];
  head = (head & kLinkMask) | 0x5A000000;
  const auto initial = head;
  map::LiveDarkenBackground();
  const auto* packets = world.primitives.data() + 1;
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == packets + 19);
  CHECK(packets[0] == (0x02000000 | (initial & kLinkMask)));
  CHECK(std::to_array<std::uint32_t>(
            {packets[4] & kLinkMask, packets[12] & kLinkMask}) ==
        std::array{kUnwritten & kLinkMask, kUnwritten & kLinkMask});
  CHECK(std::pair<std::uint32_t, std::uint32_t>{packets[6], packets[14]} ==
        std::pair{0U, 0U});
  CHECK(
      std::to_array<std::uint32_t>({packets[9] & 0x1FF, packets[17] & 0x1FF}) ==
      std::array{VS::Render::frameBuf ? 0x105U : 0x100U,
                 VS::Render::frameBuf ? 0x108U : 0x103U});
  CHECK((head & 0xFF000000) == 0x5A000000);
  CHECK(nextPrim(&head) == packets + 16);
  CHECK(std::to_array<std::uint32_t>(
            {world.primitives.front(), world.primitives[20]}) ==
        std::array{kUnwritten, kUnwritten});
}

TEST_CASE("F02702 Live scanlines retain the second strip's prior CLUT",
          "[map-terminal][map-terminal-live][F02702]") {
  LiveWorld world;
  VS::Render::frameBuf = GENERATE(0, 1);
  Menu_Map_menuFramesCounter = -1;
  map::LiveDrawPalingDistortion();
  const auto* packets = world.primitives.data() + 1;
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == packets + 3827);
  CHECK(packets[4] == 0x66808080);
  CHECK(packets[5] == 3);
  CHECK(packets[6] == 0);
  CHECK(packets[12] == 0x66808080);
  CHECK(packets[14] == 0xDEAD0000);
  CHECK((packets[3813] >> 16) == 238);
  CHECK(world.primitives[3828] == kUnwritten);
}

TEST_CASE(
    "F02690 Live camera leaves the field's stored view and projection intact",
    "[map-terminal][map-terminal-live][F02690]") {
  LiveWorld world;
  Menu_Map_cameraAngles = {0, 1024, 17, -19};
  Menu_Map_cameraPos = {51, 52, 53, -1234};
  Menu_Map_cameraDistance = 768;
  const auto frame_view = VS::Render::FrameScratchpad().view_matrix;
  map::LiveApplyCamera();
  CHECK(map::ReadLiveMapView().camera_position ==
        map::MapAngles{768, 0, 0, -1234});
  for (std::size_t axis = 0; axis < std::size(frame_view.m); ++axis) {
    CHECK(std::ranges::equal(
        frame_view.m[axis], VS::Render::FrameScratchpad().view_matrix.m[axis]));
  }
  CHECK(std::ranges::equal(frame_view.t,
                           VS::Render::FrameScratchpad().view_matrix.t));
  CHECK(TransformOrigin() == std::array{0, 0, 768});
  const auto offset = VS::Render::GetLiveProjectionScreenOffset();
  CHECK(std::pair<int, int>{offset.x, offset.y} == std::pair{160, 112});
}

TEST_CASE(
    "F02689 Live Paling uses original caption bytes and cold reset clears its "
    "borrow",
    "[map-terminal][map-terminal-live][F02689]") {
  LiveWorld world(true);
  Menu_Map_currentScene = 1;
  VS::State::stateFlags.map_paling[1] = 1;
  VS::Menu::InitLiveTextBox(0, 0, 60, 224, 16, 1, 60, 224);
  map::LiveDrawFrame();
  const auto caption = map::LiveMapPalingText();
  REQUIRE(caption.size() == 30);
  CHECK(caption.back() == 0xE7);
  CHECK(std::ranges::equal(caption, OriginalPalingCaption()));
  CHECK(VS::Menu::ReadLiveTextBox(0).text ==
        reinterpret_cast<const char*>(caption.data()));
  REQUIRE(map::ResetMapTerminalModuleData());
  CHECK(map::LiveMapPalingText().data() == caption.data());
  CHECK(VS::State::stateFlags.map_paling[1] == 1);
  REQUIRE(map::ResetMapTerminalColdState());
  CHECK(VS::Menu::ReadLiveTextBox(0).text == nullptr);
}

TEST_CASE(
    "F04151 Live controller actions rotate pan reset and cancel without an "
    "output override",
    "[map-terminal][map-terminal-live][F04151]") {
  LiveWorld world(true);
  world.LoadScene(1);
  REQUIRE(world.Tick(0, 0x8001) == 0);
  CHECK(Menu_Map_cameraAngles.vy == 1984);
  CHECK(Menu_Map_menuCommandsState == 3);
  REQUIRE(world.Tick(0, 0x4002) == 0);
  CHECK(Menu_Map_cameraDistance == 784);
  REQUIRE(world.Tick(0, 0x2000) == 0);
  CHECK(Menu_Map_geomOffsetX == 4);
  HideResetAndCancel(world);
}

TEST_CASE(
    "F04149 Live screen restores its captured pixels HUD field and alternate "
    "destination",
    "[map-terminal][map-terminal-live][F04149]") {
  LiveWorld world(true);
  std::vector<std::uint16_t> image(std::size_t{96} * 240, 0x1234);
  std::array<std::uint16_t, 256> clut{};
  clut.fill(0x4321);
  VS::Render::GamePages().Upload({672, 256, 96, 240}, image.data());
  VS::Render::GamePages().Upload({768, 227, 256, 1}, clut.data());
  std::uint8_t step = 3;
  REQUIRE(map::LiveUpdateScreen(&step) == 0);
  REQUIRE(world.ReachInput(step));
  CHECK(VS::Field::isFieldRenderingEnabled == 0);
  CHECK(Battle_hudHidden == 1);
  CHECK(Battle_hudSlideOffset == 0x200000);
  VS::Render::GamePages().Fill({640, 256, 160, 240}, 255, 0, 0);
  ExitScreen(world, step);
  CHECK(VS::Menu::Framework::menuState.currentMenu == 6);
  CheckRestoredDisplay();
  CheckRestoredPixels();
  CHECK(memory::GameHeap().Empty());
}

TEST_CASE(
    "Actual chooser cancellation preserves its scene and distinct destinations",
    "[map-terminal][map-terminal-live][F04151]") {
  LiveWorld world(true);
  world.LoadScene(1);
  auto* scene = Battle_unusedSceneBuffer;
  REQUIRE(world.Tick(0x100) == 0);
  REQUIRE(world.OpenChooser());
  CHECK(std::pair<int, int>{VS::Menu::Framework::hideMenu,
                            Menu_Map_triggerHideUi} == std::pair{0, 1});
  const bool triangle = GENERATE(false, true);
  REQUIRE(world.Tick(triangle ? 0x10U : 0x40U, 0, 0, true) == 0);
  for (std::size_t tick = 0; tick < 40 && Menu_Map_menuState == 2; ++tick) {
    world.Tick(0, 0, 0, true);
  }
  CHECK(Menu_Map_menuState == (triangle ? 3 : 1));
  CHECK(Menu_Map_quitState == (triangle ? 2 : 1));
  CHECK(Battle_unusedSceneBuffer == scene);
  CheckNoPendingRead();
}

TEST_CASE(
    "Map module reload drains queued initial reads and preserves immutable or "
    "external state",
    "[map-terminal][map-terminal-live][F04149][F04151]") {
  LiveWorld world(true);
  REQUIRE(map::LiveLoadSceneMap() == 0);
  REQUIRE(Battle_unusedSceneBuffer != nullptr);
  auto* request = Menu_Map_sceneCdQueueSlot;
  REQUIRE(request != nullptr);
  const auto generation = content::LiveDiscRequestGeneration(request);
  auto* buffer = Battle_unusedSceneBuffer;
  VS::Menu::InitLiveTextBox(0, 0, 60, 224, 16, 1, 60, 224);
  VS::Menu::SetLiveTextBoxText(0, static_cast<char*>(buffer));
  const auto names = std::to_array(Menu_Map_mapNames);
  Menu_Map_selectedMap = 27;
  Menu_Map_cameraDistance = 512;
  Menu_Map_layoutPadding1[0] = 13;
  Menu_Map_layoutPadding2[0] = 17;
  Menu_Map_mapLabelCount = 8;
  Menu_Map_mapLabels[0].x = 123;
  Menu_Map_compassBounds[0].x = 456;
  VS::State::stateFlags.map_paling[5] = 19;
  const auto field_camera = VS::Render::FrameScratchpad().camera;
  REQUIRE(map::ResetMapTerminalModuleData());
  CheckResetExports();
  CheckReleasedInitialRead(buffer, request, generation);
  CheckPreservedData(names, field_camera);
  REQUIRE(map::ResetMapTerminalModuleData());
  CHECK(memory::GameHeap().Empty());
}

TEST_CASE("Map background drainage cannot release a recycled queue slot",
          "[map-terminal][map-terminal-live][F04149]") {
  LiveWorld world(true);
  REQUIRE(map::LiveLoadBackground(1) == 0);
  auto* request = &Content_discRequestSlots[0];
  REQUIRE(request->vram != nullptr);
  auto* destination = request->vram;
  auto* replacement = RecycleRequest(request, destination);
  REQUIRE(map::DrainMapTerminalLive());
  CHECK(replacement->state == Content_DiscRequestState_Allocated);
  CHECK_FALSE(memory::LivePayloadCapacity(destination));
  content::ReleaseLiveDiscRequest(replacement);
}

TEST_CASE(
    "Map reads retain the loaded extent across both size directions and "
    "external cancellation",
    "[map-terminal][map-terminal-live][F04151]") {
  LiveWorld world(true);
  const int initial = GENERATE(1, 11);
  world.LoadScene(initial);
  auto* old = Battle_unusedSceneBuffer;
  const auto old_size =
      static_cast<std::size_t>(Menu_Map_kSceneArmFiles[initial].size);
  REQUIRE(world.ChooseOtherScene());
  CheckPendingExtent(old, initial);
  REQUIRE(Menu_Map_armFileBuff != nullptr);
  auto* staging = Menu_Map_armFileBuff;
  auto* request = Menu_Map_sceneCdQueueSlot;
  REQUIRE(request != nullptr);
  KeepDrawingWhileReading(world, old, old_size, staging);
  content::UpdateLiveDiscQueue();
  REQUIRE(request->state != Content_DiscRequestState_Loaded);
  const auto failures = memory::RefusedLiveHeapRequestCount();
  const auto staging_bytes = static_cast<std::size_t>(
      Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);
  REQUIRE(map::DrainMapTerminalLive());
  CheckDrainedRead(old, staging, request, staging_bytes, failures);
}

TEST_CASE(
    "Map abandonment restores the captured projection and field camera before "
    "heap reset",
    "[map-terminal][map-terminal-live][F04149][F02690]") {
  LiveWorld world(true);
  SetFieldView();
  std::uint8_t step = 3;
  REQUIRE(map::LiveUpdateScreen(&step) == 0);
  REQUIRE(world.ReachInput(step));
  Core_Gte_SetGeomOffset(17, 19);
  REQUIRE(map::ResetMapTerminalModuleData());
  const auto projection = VS::Render::GetLiveProjectionScreenOffset();
  CHECK(std::pair<int, int>{projection.x, projection.y} == std::pair{171, 93});
  CHECK(TransformOrigin() == std::array{77, 88, 99});
  CheckRestoredDisplay();
  CHECK(map::LiveMapScreenState() == map::MapScreenState{});
  CHECK(memory::GameHeap().Empty());
}

TEST_CASE(
    "Map async swap completes through production queue and installs its own "
    "extent",
    "[map-terminal][map-terminal-live][F04151]") {
  LiveWorld world(true);
  world.LoadScene(11);
  REQUIRE(world.ChooseOtherScene());
  auto* staging = Menu_Map_armFileBuff;
  for (std::size_t tick = 0; tick < kTickBudget && Menu_Map_menuState != 1;
       ++tick) {
    content::UpdateLiveDiscQueue();
    world.Tick(0, 0, 0, true);
  }
  REQUIRE(std::pair<int, int>{Menu_Map_menuState, Menu_Map_currentScene} ==
          std::pair{1, 1});
  CheckNoPendingRead();
  CheckSceneExtent(Battle_unusedSceneBuffer, 8192);
  CHECK(VS::Menu::ReadLiveTextBox(0).text != nullptr);
  CHECK_FALSE(memory::LivePayloadCapacity(staging));
  CHECK(Menu_Map_geomOffsetX == 0);
}

TEST_CASE(
    "A completed map read survives allocation refusal for retry or drainage",
    "[map-terminal][map-terminal-live][F04151]") {
  LiveWorld world(true);
  world.LoadScene(11);
  REQUIRE(world.ChooseOtherScene());
  auto* request = Menu_Map_sceneCdQueueSlot;
  REQUIRE(request != nullptr);
  for (std::size_t tick = 0;
       tick < kTickBudget && request->state != Content_DiscRequestState_Loaded;
       ++tick) {
    content::UpdateLiveDiscQueue();
  }
  REQUIRE(request->state == Content_DiscRequestState_Loaded);
  HeapBlocker blocker;
  FailSwapAllocation(world, Menu_Map_armFileBuff);
  if (GENERATE(false, true)) {
    blocker.Release();
    REQUIRE(world.Tick() == 0);
    CHECK(Menu_Map_menuState == 5);
    CheckSceneExtent(Battle_unusedSceneBuffer, 8192);
  } else {
    REQUIRE(map::DrainMapTerminalLive());
    CheckNoSceneOwners();
    CHECK(memory::LivePayloadCapacity(blocker.payload).has_value());
  }
}

TEST_CASE("Map refuses a foreign scene and does not report completed quit",
          "[map-terminal][map-terminal-live][F04151]") {
  LiveWorld world;
  std::array<std::byte, 64> foreign{};
  Battle_unusedSceneBuffer = foreign.data();
  map::BindLiveMapScene(foreign.data(), foreign.size());
  Menu_Map_currentScene = 1;
  Menu_Map_menuState = 3;
  Menu_Map_backgroundBrightness = 128;
  Menu_Map_quitState = 2;
  const auto refusals = map::MapTerminalLiveRefusalCount();
  CHECK(map::LiveUpdateMapMenu() == 0);
  CHECK(map::MapTerminalLiveRefusalCount() > refusals);
  CHECK(Battle_unusedSceneBuffer == foreign.data());
  CHECK_FALSE(map::ResetMapTerminalModuleData());
  map::ForgetLiveMapScene(foreign.data());
  Battle_unusedSceneBuffer = nullptr;
}

namespace {
template <typename Entry>
Entry MapModuleEntry(unsigned address, Entry expected) {
  auto* resolved = content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                                 "MENU/MENU5.PRG", address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved != reinterpret_cast<void*>(expected)) {
    throw std::runtime_error("incorrect map terminal module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}

struct InitialRead {
  void* buffer;
  Content_DiscQueueSlot* request;
  std::uint64_t generation;

  static InitialRead Begin() {
    REQUIRE(map::LiveLoadSceneMap() == 0);
    REQUIRE(Battle_unusedSceneBuffer != nullptr);
    REQUIRE(Menu_Map_sceneCdQueueSlot != nullptr);
    return {Battle_unusedSceneBuffer, Menu_Map_sceneCdQueueSlot,
            content::LiveDiscRequestGeneration(Menu_Map_sceneCdQueueSlot)};
  }
  void CheckReleased() const {
    CheckNoSceneOwners();
    CheckReleasedInitialRead(buffer, request, generation);
  }
};

void BeginVisibleMapSwap(LiveWorld& world) {
  std::uint8_t phase = 3;
  REQUIRE(map::LiveUpdateScreen(&phase) == 0);
  REQUIRE(world.ReachInput(phase));
  REQUIRE(world.ChooseOtherScene());
  REQUIRE(Battle_unusedSceneBuffer != nullptr);
  REQUIRE(Menu_Map_armFileBuff != nullptr);
  REQUIRE(Menu_Map_sceneCdQueueSlot != nullptr);
}

struct FullDiscQueue {
  std::array<Content_DiscQueueSlot*, 32> requests{};
  std::array<std::uint64_t, 32> generations{};

  FullDiscQueue() {
    for (std::size_t index = 0; index < requests.size(); ++index) {
      auto* request =
          content::AllocateLiveDiscRequest(&Menu_Map_kSceneArmFiles[1]);
      REQUIRE(request != nullptr);
      requests[index] = request;
      generations[index] = content::LiveDiscRequestGeneration(request);
    }
  }
  ~FullDiscQueue() {
    for (std::size_t index = 0; index < requests.size(); ++index) {
      if (content::LiveDiscRequestGeneration(requests[index]) ==
          generations[index]) {
        content::ReleaseLiveDiscRequest(requests[index]);
      }
    }
  }
  void CheckPreserved() const {
    for (std::size_t index = 0; index < requests.size(); ++index) {
      CHECK(content::LiveDiscRequestGeneration(requests[index]) ==
            generations[index]);
      CHECK(requests[index]->state == Content_DiscRequestState_Allocated);
    }
  }
};

struct RestoreRoomRuntime {
  const game_clear_live_tests::SavedGlobals<
      VS::Field::backgroundReferenceVectors, Field_entranceDoorRecord,
      Battle_unusedMenu9CursorMemory, Field_manualDisplayState,
      VS::Field::roomPalettes, Battle_floatingLabels,
      Battle_placedLabelPositions, Field_enteredDoorId,
      VS::Field::areRoomUpdatesPaused, VS::Field::roomTint,
      Battle_unusedSubmenuState, Field_isMinimapVisible,
      Field_minimapSlideFrame, Field_gimmickAnimationQueue,
      Field_pendingGimmickAnimationCount>
      saved;
  VS::Field::RoomBackgroundState background =
      VS::Field::LoadLiveRoomBackgroundState();
  VS::Field::RoomWeatherState weather = VS::Field::LoadLiveRoomWeatherState();

  ~RestoreRoomRuntime() {
    VS::Field::StoreLiveRoomBackgroundState(background);
    VS::Field::StoreLiveRoomWeatherState(weather);
  }
};

void CheckMapFrameEntries(LiveWorld& world) {
  world.LoadScene(1);
  MapModuleEntry(0x801042B0, &map::LiveDrawFrame)();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor !=
        world.primitives.data() + 1);
  world.ResetFrame();
  MapModuleEntry(0x8010625C, &map::LiveDarkenBackground)();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        world.primitives.data() + 20);
  world.ResetFrame();
  MapModuleEntry(0x801066E0, &map::LiveDrawPalingDistortion)();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        world.primitives.data() + 3828);
}
}  // namespace

TEST_CASE("Original map drawing entries execute the native frame and camera",
          "[map-terminal-integration][map-terminal-module]"
          "[F02689][F02690][F02699][F02702]") {
  LiveWorld world(true);
  CheckMapFrameEntries(world);
  Menu_Map_cameraAngles = {0, 1024, 0, 0};
  Menu_Map_cameraDistance = 768;
  MapModuleEntry(0x80104384, &map::LiveApplyCamera)();
  CHECK(TransformOrigin() == std::array{0, 0, 768});
}

TEST_CASE("Original map controller entries consume native loading and input",
          "[map-terminal-integration][map-terminal-module]"
          "[F04149][F04151]") {
  LiveWorld world(true);
  std::array<std::uint8_t, 2> phase{3, 0xAA};
  CHECK(MapModuleEntry(0x801030F4, &map::LiveUpdateScreen)(phase.data()) == 0);
  CHECK(phase == std::array<std::uint8_t, 2>{4, 0xAA});
  REQUIRE(world.ReachInput(phase[0]));
  VS::Input::previousButtons = 0x4002;
  world.ResetFrame();
  CHECK(MapModuleEntry(0x80103684, &map::LiveUpdateMapMenu)() == 0);
  CHECK(Menu_Map_cameraDistance == 784);
}

TEST_CASE("Production framework dispatch uses the real map state byte",
          "[map-terminal-integration][map-terminal-framework][F04149]") {
  LiveWorld world(true);
  const game_clear_live_tests::SavedGlobals<
      VS::Menu::Framework::loadedSubMenu,
      VS::Menu::Framework::openedWithoutMainMenu,
      VS::Menu::Framework::actionRestrictions,
      VS::Menu::Framework::submenuStates>
      saved;
  VS::Menu::Framework::loadedSubMenu = kMapScreen;
  VS::Menu::Framework::openedWithoutMainMenu = 1;
  VS::Menu::Framework::menuState.currentMenu = kMapScreen;
  VS::Menu::Framework::menuState.loading = 0;
  VS::Menu::Framework::actionRestrictions = 0;
  VS::Menu::Framework::submenuStates[kMapScreen] = 3;
  VS::Menu::Framework::submenuStates[kMapScreen - 1] = static_cast<char>(0xAA);
  Menu_Map_currentMapRow = 23;
  REQUIRE(VS::Save::MemoryCard::LiveMenuAssetPhase() ==
          VS::Save::MemoryCard::MenuAssetPhase::kIdle);
  framework::UpdateLiveMainMenu(kMapScreen);
  CHECK(VS::Menu::Framework::submenuStates[kMapScreen] == 4);
  CHECK(static_cast<unsigned char>(
            VS::Menu::Framework::submenuStates[kMapScreen - 1]) == 0xAA);
  CHECK(Menu_Map_currentMapRow == -1);
  CHECK(Content_discRequestCounts.waiting != 0);
  CHECK(std::ranges::any_of(Content_discRequestSlots, [](const auto& slot) {
    return slot.cdFile.lba == VS_MAPBG_BIN_LBA &&
           slot.cdFile.size == VS_MAPBG_BIN_SIZE && slot.vram != nullptr &&
           memory::LivePayloadCapacity(slot.vram).has_value();
  }));
  CHECK(VS::Save::MemoryCard::LiveMenuAssetPhase() ==
        VS::Save::MemoryCard::MenuAssetPhase::kIdle);
}

TEST_CASE("Only completed MENU5 drains map reads and restores its module state",
          "[map-terminal-integration][map-terminal-boundary-prehook]"
          "[F04149][F04151]") {
  LiveWorld world(true);
  const bool map_reload = GENERATE(false, true);
  const auto owner = InitialRead::Begin();
  Menu_Map_selectedMap = 27;
  Menu_Map_cameraDistance = 512;
  const auto names = std::to_array(Menu_Map_mapNames);
  VS::State::stateFlags.map_paling[5] = 19;
  const auto camera = VS::Render::FrameScratchpad().camera;
  const Content_DiscFile file =
      map_reload ? Content_DiscFile{VS_MENU5_PRG_LBA, VS_MENU5_PRG_SIZE}
                 : Content_DiscFile{VS_MENU4_PRG_LBA, VS_MENU4_PRG_SIZE};
  menu_overlay_tests::Load overlay(kMapScreen, file);
  CHECK(overlay.Poll() == 0);
  CHECK(memory::LivePayloadCapacity(owner.buffer).has_value());
  CHECK(Menu_Map_selectedMap == 27);
  CHECK(overlay.Complete() != 0);
  if (!map_reload) {
    CHECK(memory::LivePayloadCapacity(owner.buffer).has_value());
    CHECK(content::LiveDiscRequestGeneration(owner.request) ==
          owner.generation);
    CHECK(Menu_Map_selectedMap == 27);
    return;
  }
  owner.CheckReleased();
  CheckResetExports();
  CheckPreservedData(names, camera);
}

TEST_CASE("Cold session drains the map before disc and UI invalidation",
          "[map-terminal-integration][map-terminal-boundary-prehook]"
          "[F04149][F04151]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<Menu_textBoxSelector,
                                            Event_isTextBoxAwaitingClose>
      saved_ui;
  LiveWorld world(true);
  const auto owner = InitialRead::Begin();
  auto* foreign = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(foreign != nullptr);
  CHECK(VS::Boot::RunLiveSessionLoop([] { return 7; }, [] { return 9; }) == 7);
  owner.CheckReleased();
  CHECK(memory::LivePayloadCapacity(foreign).has_value());
  memory::FreeLiveFromLowEnd(foreign);
}

TEST_CASE("Stopped-frame heap reset drains map ownership and permits reentry",
          "[map-terminal-integration][map-terminal-heap][F04149][F04151]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  LiveWorld world(true);
  const auto owner = InitialRead::Begin();
  VS::Boot::ResetLiveSessionHeap();
  owner.CheckReleased();
  CHECK(memory::GameHeap().Empty());
  const auto next = InitialRead::Begin();
  CHECK(memory::LivePayloadCapacity(next.buffer).has_value());
  CHECK(content::LiveDiscRequestGeneration(next.request) != 0);
}

TEST_CASE(
    "Map cleanup refuses a live foreign alias without freeing either owner",
    "[map-terminal-integration][map-terminal-ownership][F04151]") {
  LiveWorld world(true);
  world.LoadScene(1);
  auto* owner = Battle_unusedSceneBuffer;
  auto* foreign = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(foreign != nullptr);
  Battle_unusedSceneBuffer = foreign;
  VS::Menu::SetLiveTextBoxText(0, static_cast<char*>(foreign));
  CHECK_FALSE(map::DrainMapTerminalLive());
  CHECK(memory::LivePayloadCapacity(foreign).has_value());
  CHECK(memory::LivePayloadCapacity(owner).has_value());
  CHECK(VS::Menu::ReadLiveTextBox(0).text == static_cast<char*>(foreign));
  // Preserve a failed pre-hook observation while allowing safe fixture cleanup.
  Battle_unusedSceneBuffer = owner;
  REQUIRE(map::DrainMapTerminalLive());
  VS::Menu::SetLiveTextBoxText(0, nullptr);
  if (memory::LivePayloadCapacity(foreign)) {
    memory::FreeLiveFromLowEnd(foreign);
  }
}

TEST_CASE("Map initial-read cleanup clears its borrow even after alias loss",
          "[map-terminal-integration][map-terminal-ownership][F04151]") {
  LiveWorld world(true);
  const auto owner = InitialRead::Begin();
  VS::Menu::SetLiveTextBoxText(0, static_cast<char*>(owner.buffer));
  Battle_unusedSceneBuffer = nullptr;
  REQUIRE(map::DrainMapTerminalLive());
  owner.CheckReleased();
}

TEST_CASE("Map completed-scene release cancels writes into its storage",
          "[map-terminal-integration][map-terminal-ownership][F04151]") {
  LiveWorld world(true);
  world.LoadScene(1);
  auto* owner = Battle_unusedSceneBuffer;
  auto* request = content::AllocateLiveDiscRequest(&Menu_Map_kSceneArmFiles[1]);
  REQUIRE(request != nullptr);
  const auto generation = content::LiveDiscRequestGeneration(request);
  content::EnqueueLiveDiscRequest(request, owner);
  REQUIRE(map::DrainMapTerminalLive());
  CHECK_FALSE(map::BoundLiveMapScene(owner));
  CHECK_FALSE(memory::LivePayloadCapacity(owner));
  CHECK(request->state == Content_DiscRequestState_Error);
  CHECK(request->vram == nullptr);
  CHECK(content::LiveDiscRequestGeneration(request) == generation);
  const auto discarded = std::span{static_cast<std::byte*>(owner), 8192};
  std::ranges::fill(discarded, kCancelledReadPoison);
  for (std::size_t tick = 0; tick < kTickBudget; ++tick) {
    content::UpdateLiveDiscQueue();
  }
  CHECK(std::ranges::all_of(discarded, [](std::byte value) {
    return value == kCancelledReadPoison;
  }));
  content::ReleaseLiveDiscRequest(request);
}

TEST_CASE("Full production queue still completes both map size transitions",
          "[map-terminal-integration][map-terminal-queue][F04151]") {
  LiveWorld world(true);
  world.LoadScene(1);
  FullDiscQueue busy;
  for (int destination : {11, 1}) {
    REQUIRE(world.ChooseOtherScene());
    auto* request = Menu_Map_sceneCdQueueSlot;
    REQUIRE(request != nullptr);
    CHECK(std::ranges::find(busy.requests, request) == busy.requests.end());
    CHECK(request->state == Content_DiscRequestState_Loaded);
    for (std::size_t tick = 0; tick < kTickBudget && Menu_Map_menuState != 1;
         ++tick) {
      world.Tick();
    }
    CHECK(Menu_Map_menuState == 1);
    CHECK(Menu_Map_currentScene == destination);
    CHECK(content::LiveDiscRequestGeneration(request) == 0);
    CheckSceneExtent(Battle_unusedSceneBuffer, destination == 1 ? 8192 : 2048);
    busy.CheckPreserved();
  }
}

TEST_CASE("Actual heap replacement drains a visible map and its pending swap",
          "[map-terminal-integration][map-terminal-heap][F04149][F04151]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  LiveWorld world(true);
  SetFieldView();
  BeginVisibleMapSwap(world);
  auto* scene = Battle_unusedSceneBuffer;
  auto* staging = Menu_Map_armFileBuff;
  auto* request = Menu_Map_sceneCdQueueSlot;
  const auto refusals = memory::RefusedLiveHeapRequestCount();
  VS::Boot::ResetLiveSessionHeap();
  CheckDrainedRead(scene, staging, request, 2048, refusals);
  CHECK(VS::Field::isFieldRenderingEnabled == 1);
  CHECK(Battle_hudHidden == 0);
  const auto projection = VS::Render::GetLiveProjectionScreenOffset();
  CHECK(std::pair<int, int>{projection.x, projection.y} == std::pair{171, 93});
  CHECK(TransformOrigin() == std::array{77, 88, 99});
}

TEST_CASE(
    "Room runtime initialization drains map before replacing room records",
    "[map-terminal-integration][map-terminal-room][F04151]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  LiveWorld world(true);
  const RestoreRoomRuntime saved_room;
  REQUIRE(Field_pendingGimmickAnimationCount == 0);
  world.LoadScene(1);
  auto* scene = Battle_unusedSceneBuffer;
  VS::Menu::SetLiveTextBoxText(
      0, reinterpret_cast<char*>(Menu_Map_roomNamesTable));
  VS::Field::LiveInitializeRoomRuntimeState();
  CheckNoSceneOwners();
  CHECK_FALSE(map::BoundLiveMapScene(scene));
  CHECK_FALSE(memory::LivePayloadCapacity(scene));
  CHECK(Field_roomData.minimapScene == nullptr);
  CHECK(VS::Field::roomPalettes != nullptr);
  CHECK(Battle_floatingLabels != nullptr);
  CHECK(Battle_placedLabelPositions != nullptr);
  memory::FreeLiveFromLowEnd(VS::Field::roomPalettes);
  memory::FreeLiveFromLowEnd(Battle_floatingLabels);
  memory::FreeLiveFromLowEnd(Battle_placedLabelPositions);
}

TEST_CASE("Original map caption survives module reset but not a cold session",
          "[map-terminal-integration][map-terminal-caption][F02689]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<Menu_textBoxSelector,
                                            Event_isTextBoxAwaitingClose>
      saved_ui;
  LiveWorld world(true);
  world.LoadScene(1);
  VS::State::stateFlags.map_paling[1] = 1;
  MapModuleEntry(0x801042B0, &map::LiveDrawFrame)();
  const auto caption = map::LiveMapPalingText();
  REQUIRE_FALSE(caption.empty());
  const std::vector<std::uint8_t> original(caption.begin(), caption.end());
  auto* borrow = VS::Menu::ReadLiveTextBox(0).text;
  CHECK(borrow == reinterpret_cast<const char*>(caption.data()));
  VS::Boot::ResetMenu5Statics();
  CHECK(VS::Menu::ReadLiveTextBox(0).text == borrow);
  CHECK(std::ranges::equal(caption, original));
  CHECK(VS::Boot::RunLiveSessionLoop([] { return 7; }, [] { return 9; }) == 7);
  CHECK(VS::Menu::ReadLiveTextBox(0).text == nullptr);
  const content::DataStore::ScopedOverride unavailable;
  CHECK(map::LiveMapPalingText().empty());
}

TEST_CASE("Normal map exit leaves the stored view for the next field draw",
          "[map-terminal-integration][map-terminal-field][F02690][F04149]") {
  LiveWorld world(true);
  const game_clear_live_tests::SavedGlobals<Field_roomPrimitiveRangeStart,
                                            Field_roomPrimitiveRangeEnd>
      saved_ranges;
  SetFieldView();
  std::uint8_t phase = 3;
  REQUIRE(map::LiveUpdateScreen(&phase) == 0);
  REQUIRE(world.ReachInput(phase));
  ExitScreen(world, phase);
  CHECK(VS::Field::isFieldRenderingEnabled == 1);
  std::uint32_t empty_geometry = 0;
  Field_roomData.header.geometryLength = sizeof(empty_geometry);
  world.ResetFrame();
  REQUIRE(VS::Render::ValidateLiveRoomGeometry(&empty_geometry,
                                               sizeof(empty_geometry)));
  VS::Field::LiveDrawRoomGeometry(&empty_geometry);
  CHECK(TransformOrigin() == std::array{77, 88, 99});
}
