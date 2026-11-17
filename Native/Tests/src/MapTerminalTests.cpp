#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/MapTerminal.hpp"

namespace {
namespace map = VS::Menu::map;

std::uint32_t Button(map::MapButton button) {
  return static_cast<std::uint32_t>(button);
}

class FrameHost final : public map::MapFrameHost {
 public:
  std::vector<std::string> calls;
  bool mask = true;
  void DrawControls() override { calls.emplace_back("controls"); }
  bool EnableMaskWrite() override {
    calls.emplace_back("mask");
    return mask;
  }
  void DrawDistortion() override { calls.emplace_back("distort"); }
  void SetPalingCaption() override { calls.emplace_back("caption"); }
  void ApplyView() override { calls.emplace_back("camera"); }
  void Darken() override { calls.emplace_back("darken"); }
  void DrawRooms() override { calls.emplace_back("rooms"); }
};

struct FramebufferEvent {
  std::int32_t slot = 0;
  std::uint16_t page = 0;
  std::optional<map::FramebufferStrip> strip;
};

class FramebufferHost final : public map::FramebufferHost {
 public:
  std::vector<FramebufferEvent> events;
  std::size_t capacity = std::numeric_limits<std::size_t>::max();
  bool TexturePage(std::int32_t slot, std::uint16_t page) override {
    if (events.size() == capacity) return false;
    events.push_back({slot, page, {}});
    return true;
  }
  bool Strip(std::int32_t slot, const map::FramebufferStrip& strip) override {
    if (events.size() == capacity) return false;
    events.push_back({slot, 0, strip});
    return true;
  }
};

map::FramebufferStrip StripAt(const FramebufferHost& host, std::size_t index) {
  const auto& strip = host.events.at(index).strip;
  if (!strip) throw std::runtime_error("Expected a framebuffer strip");
  return *strip;
}

class MenuHost final : public map::MapMenuHost {
 public:
  std::vector<std::string> calls;
  int selection = -1;
  int read_state = 1;
  int initial_room = 3;
  int adjacent_room = 2;
  int room_name = -1;
  int direction = 0;
  int brightness = 0;
  bool begin_ok = true;
  bool install_ok = true;
  bool text_ok = true;
  bool free_ok = true;
  void SetFarColorBlack() override { calls.emplace_back("far"); }
  void SetBackgroundPriority(std::int32_t slot, std::int32_t value) override {
    CHECK(slot == 2046);
    brightness = value;
    calls.emplace_back("priority");
  }
  void SetVisible(bool visible) override {
    calls.emplace_back(visible ? "show" : "hide");
  }
  void PlaySound(std::int32_t volume) override {
    calls.push_back("sound" + std::to_string(volume));
  }
  bool InitializeRoomText() override {
    calls.emplace_back("init-text");
    return text_ok;
  }
  bool ShowRoomName(std::int32_t room) override {
    room_name = room;
    calls.emplace_back("room-name");
    return text_ok;
  }
  void DismissRoomText() override { calls.emplace_back("dismiss"); }
  std::int32_t PollAreas(bool initialize) override {
    calls.emplace_back(initialize ? "init-areas" : "areas");
    return selection;
  }
  void ShowAreaName(std::int32_t scene, bool current) override {
    calls.push_back("area" + std::to_string(scene) +
                    (current ? "-current" : "-other"));
  }
  bool BeginSceneRead(std::int32_t scene, map::DiscFileRange file) override {
    calls.push_back("begin" + std::to_string(scene) + "-" +
                    std::to_string(file.bytes));
    return begin_ok;
  }
  std::int32_t SceneReadState() override {
    calls.emplace_back("poll-read");
    return read_state;
  }
  bool InstallSceneRead(std::int32_t scene, map::DiscFileRange file) override {
    calls.push_back("install" + std::to_string(scene) + "-" +
                    std::to_string(file.bytes));
    return install_ok;
  }
  bool FreeScene() override {
    calls.emplace_back("free-scene");
    return free_ok;
  }
  std::int32_t InitialRoom() override {
    calls.emplace_back("initial-room");
    return initial_room;
  }
  void CenterOnRoom(std::int32_t room) override {
    calls.push_back("center" + std::to_string(room));
  }
  std::int32_t SelectRoom(std::int32_t next,
                          std::int32_t /*current*/) override {
    direction = next;
    calls.emplace_back("adjacent");
    return adjacent_room;
  }
  void SetCenterTarget(std::int32_t room) override {
    calls.push_back("target" + std::to_string(room));
  }
  void DrawFrame() override { calls.emplace_back("draw"); }
  void AnimateCentering() override { calls.emplace_back("animate"); }
  bool Called(const std::string& call) const {
    return std::ranges::find(calls, call) != calls.end();
  }
};

struct MenuFixture {
  MenuHost host;
  map::MapMenuState state;
  map::MapInput input;
  std::array<std::uint8_t, 32> paling{};
  std::array<std::int32_t, 32> rows{};
  std::array<map::DiscFileRange, 32> files{};

  MenuFixture() {
    state.view.menu_state = 1;
    state.view.current_scene = 1;
    state.view.current_scene_row = 1;
    state.view.camera_angles = {568, 2048, 177, 42};
    state.view.camera_distance = 768;
    state.view.background_brightness = 32;
    state.view.quit_state = 1;
    for (std::size_t index = 0; index < rows.size(); ++index) {
      rows[index] = static_cast<std::int32_t>(index);
      files[index] = {85000 + static_cast<std::int32_t>(index), 2048};
    }
    files[1].bytes = 8192;
  }
  std::optional<std::int32_t> Tick() {
    return map::UpdateMapMenu(host, state, input, paling, rows, files);
  }
};

class ScreenHost final : public map::MapScreenHost {
 public:
  std::vector<std::string> calls;
  int background = 0;
  int scene = 0;
  int ready = 0;
  int result = 0;
  bool resource_ok = true;
  int destination = -1;
  bool CaptureBackground() override {
    calls.emplace_back("capture");
    return resource_ok;
  }
  std::int32_t LoadBackground(bool begin) override {
    calls.emplace_back(begin ? "begin-background" : "poll-background");
    return background;
  }
  std::int32_t LoadScene() override {
    calls.emplace_back("load-scene");
    return scene;
  }
  void FadeBackground(bool fade) override {
    calls.emplace_back(fade ? "fade-in" : "fade-out");
  }
  void SetFieldRendering(bool enabled) override {
    calls.emplace_back(enabled ? "field-on" : "field-off");
  }
  void SetHudHidden(bool hidden) override {
    calls.emplace_back(hidden ? "hide-hud" : "show-hud");
  }
  void SetHudSlide(std::int32_t offset) override {
    calls.push_back("hud" + std::to_string(offset));
  }
  std::int32_t MenuRowsIdle() override {
    calls.emplace_back("rows-idle");
    return ready;
  }
  std::int32_t UpdateMenu() override {
    calls.emplace_back("menu");
    return result;
  }
  void RestoreMenuPriority() override { calls.emplace_back("priority"); }
  void ClearCommand() override { calls.emplace_back("clear-command"); }
  void ClearRows() override { calls.emplace_back("clear-rows"); }
  bool RestoreBackground() override {
    calls.emplace_back("restore");
    return resource_ok;
  }
  bool ReleaseBackground() override {
    calls.emplace_back("release");
    return resource_ok;
  }
  void SetDestination(std::uint8_t menu) override {
    destination = menu;
    calls.emplace_back("destination");
  }
  void ShowMenu() override { calls.emplace_back("show-menu"); }
};

void BeginAreaRead(MenuFixture& fixture) {
  auto& view = fixture.state.view;
  view.menu_state = 2;
  fixture.paling[1] = 1;
  fixture.host.selection = 11;
  REQUIRE(fixture.Tick() == 0);
  CHECK(std::array<int, 3>{view.current_scene, view.is_paling,
                           view.menu_state} == std::array{11, 1, 4});
  CHECK(fixture.host.Called("begin11-2048"));
}

void WaitForReadAndSlide(MenuFixture& fixture) {
  auto& view = fixture.state.view;
  fixture.host.read_state = 4;
  for (int tick = 0; tick < 5; ++tick) REQUIRE(fixture.Tick() == 0);
  CHECK(view.geometry_offset_x == 320);
  CHECK_FALSE(fixture.host.Called("install11-2048"));
  fixture.host.read_state = 2;
  REQUIRE(fixture.Tick() == 0);
  CHECK(std::array<int, 2>{view.geometry_offset_x, view.menu_state} ==
        std::array{384, 4});
}

void BeginScreenLoad(ScreenHost& host, map::MapScreenState& state,
                     std::uint8_t& step) {
  REQUIRE(map::UpdateScreen(host, state, step, 0) == 0);
  CHECK(step == 4);
  CHECK(host.calls == std::vector<std::string>{"capture", "begin-background"});
  host.calls.clear();
  REQUIRE(map::UpdateScreen(host, state, step, 0) == 0);
  CHECK(host.calls == std::vector<std::string>{"poll-background"});
  host.background = 1;
  host.scene = 1;
  REQUIRE(map::UpdateScreen(host, state, step, 0) == 0);
  CHECK(state.load_delay == 7);
  CHECK(step == 5);
}

void BeginScreenExit(ScreenHost& host, map::MapScreenState& state,
                     std::uint8_t& step) {
  REQUIRE(map::UpdateScreen(host, state, step, 0) == 0);
  CHECK(step == 8);
  CHECK(state.load_delay == 10);
  CHECK(host.calls == std::vector<std::string>{"menu", "priority", "show-hud",
                                               "hud0", "field-on",
                                               "clear-command", "clear-rows"});
  host.calls.clear();
  REQUIRE(map::UpdateScreen(host, state, step, 0) == 0);
  CHECK(step == 9);
}
}  // namespace

TEST_CASE("F02689 orders controls, mask, camera and the selected scene",
          "[map-terminal][F02689]") {
  FrameHost host;
  const bool leaving = GENERATE(false, true);
  REQUIRE(map::DrawFrame(host, false, leaving));
  std::vector<std::string> expected{"controls", "mask", "camera", "darken"};
  if (!leaving) expected.emplace_back("rooms");
  CHECK(host.calls == expected);
}

TEST_CASE("F02689 Paling replaces geometry and sets its original caption",
          "[map-terminal][F02689]") {
  FrameHost host;
  REQUIRE(map::DrawFrame(host, true, GENERATE(false, true)));
  CHECK(host.calls ==
        std::vector<std::string>{"controls", "mask", "distort", "caption"});
}

TEST_CASE("F02689 missing mask capacity is a refusal, not a drawn frame",
          "[map-terminal][F02689]") {
  FrameHost host;
  host.mask = false;
  CHECK_FALSE(map::DrawFrame(host, false, false));
  CHECK(host.calls == std::vector<std::string>{"controls", "mask"});
}

TEST_CASE("F02690 preserves camera halfword padding and rotates around origin",
          "[map-terminal][F02690]") {
  const auto angles =
      GENERATE(map::MapAngles{0, 0, 7, 13}, map::MapAngles{0, 1024, 7, 13},
               map::MapAngles{1024, 0, 7, 13});
  const auto camera = map::ApplyCamera(angles, 768, {51, 52, 53, -1234});
  const auto expected =
      angles.x == 1024 ? map::MapAngles{0, -768, 0, -1234}
                       : (angles.y == 1024 ? map::MapAngles{768, 0, 0, -1234}
                                           : map::MapAngles{0, 0, -768, -1234});
  CHECK(camera.position == expected);
  CHECK(camera.transform.translation == VS::Core::Math::Vec3{0, 0, 768});
}

TEST_CASE("F02690 distance products retain low words rather than widen",
          "[map-terminal][F02690]") {
  const auto camera = map::ApplyCamera(
      {0, 1024, 0, 0}, std::numeric_limits<std::int32_t>::max(), {});
  CHECK(camera.position == map::MapAngles{-1, 0, 0, 0});
  CHECK(camera.transform.translation == VS::Core::Math::Vec3{0, 0, -1});
}

TEST_CASE("F02690 original ApplyMatrix returns MAC words beyond saturated IR",
          "[map-terminal][F02690]") {
  const auto camera =
      map::ApplyCamera({568, 767, 177, 42}, 40000, {0, 0, 0, 77});
  CHECK(camera.position == map::MapAngles{23777, -30605, -9889, 77});
  CHECK(camera.transform.translation == VS::Core::Math::Vec3{-1, 2, 39995});
}

TEST_CASE("F02699 retains reverse-linked framebuffer halves and page modes",
          "[map-terminal][F02699]") {
  FramebufferHost host;
  const bool back = GENERATE(false, true);
  REQUIRE(map::DarkenBackground(host, back));
  REQUIRE(host.events.size() == 5);
  CHECK(host.events[0].page == 0x60);
  CHECK(host.events[1].strip ==
        map::FramebufferStrip{0, 0, 192, 240, 0, true, true});
  CHECK(host.events[2].page == (back ? 0x105 : 0x100));
  CHECK(host.events[3].strip ==
        map::FramebufferStrip{192, 0, 128, 240, 0, true, true});
  CHECK(host.events[4].page == (back ? 0x108 : 0x103));
  CHECK(std::ranges::all_of(
      host.events, [](const auto& event) { return event.slot == 2046; }));
}

TEST_CASE("F02699 stops emitting after a bounded packet refusal",
          "[map-terminal][F02699]") {
  FramebufferHost host;
  host.capacity = GENERATE(0, 1, 2, 3, 4);
  CHECK_FALSE(map::DarkenBackground(host, false));
  CHECK(host.events.size() == host.capacity);
}

TEST_CASE(
    "F02702 emits exactly 239 alternating scanlines without clearing right "
    "CLUT",
    "[map-terminal][F02702]") {
  FramebufferHost host;
  REQUIRE(map::DrawPalingDistortion(host, true, 0));
  REQUIRE(host.events.size() == 957);
  CHECK(std::pair<int, int>{host.events[0].slot, host.events[0].page} ==
        std::pair{2046, 0x60});
  CHECK(host.events[1].strip ==
        map::FramebufferStrip{0, 0, 192, 1, 0, false, true});
  CHECK(host.events[3].strip ==
        map::FramebufferStrip{192, 0, 128, 1, 0, false, false});
  CHECK(std::pair<int, int>{StripAt(host, 5).x, StripAt(host, 7).x} ==
        std::pair{3, 195});
  const auto last = StripAt(host, 955);
  CHECK(std::pair<int, int>{last.y, last.texture_y} == std::pair{238, 238});
  CHECK(host.events[956].page == 0x108);
  CHECK(std::ranges::all_of(host.events | std::views::drop(1),
                            [](const auto& event) { return event.slot == 8; }));
}

TEST_CASE("F02702 signed phase wrap keeps division toward zero",
          "[map-terminal][F02702]") {
  FramebufferHost host;
  REQUIRE(map::DrawPalingDistortion(host, false, -1));
  CHECK(StripAt(host, 1).x == 3);
  CHECK(StripAt(host, 3).x == 195);
  FramebufferHost wrapped;
  REQUIRE(map::DrawPalingDistortion(wrapped, false,
                                    std::numeric_limits<std::int32_t>::max()));
  CHECK(wrapped.events[1].strip == host.events[1].strip);
}

TEST_CASE("F02702 refuses capacity exhaustion without emitting a partial strip",
          "[map-terminal][F02702]") {
  FramebufferHost host;
  host.capacity = GENERATE(0, 1, 4, 800);
  CHECK_FALSE(map::DrawPalingDistortion(host, false, 1));
  CHECK(host.events.size() == host.capacity);
}

TEST_CASE("F04151 initial dimming and slide precede room text creation",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  auto& view = fixture.state.view;
  view.menu_state = 0;
  view.background_brightness = 128;
  view.geometry_offset_x = -384;
  for (int tick = 0; tick < 6; ++tick) REQUIRE(fixture.Tick() == 0);
  CHECK(std::array<int, 3>{view.background_brightness, view.geometry_offset_x,
                           view.menu_state} == std::array{32, 0, 0});
  CHECK_FALSE(fixture.host.Called("init-text"));
  REQUIRE(fixture.Tick() == 0);
  CHECK(std::array<int, 4>{view.menu_state, fixture.host.room_name,
                           view.menu_frames,
                           fixture.state.label_y} == std::array{1, 0, 7, 188});
  CHECK(fixture.host.Called("init-text"));
}

TEST_CASE("F04151 Select toggles visibility once and Circle opens the chooser",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  fixture.input.pressed = Button(map::MapButton::kSelect);
  REQUIRE(fixture.Tick() == 0);
  CHECK(std::pair<int, int>{fixture.state.view.trigger_hide_ui,
                            fixture.state.view.hide_ui} == std::pair{1, 1});
  CHECK(fixture.host.Called("hide"));
  fixture.host.calls.clear();
  fixture.input.pressed =
      Button(map::MapButton::kCircle) | Button(map::MapButton::kTriangle);
  REQUIRE(fixture.Tick() == 0);
  CHECK(std::to_array<int>({fixture.state.view.menu_state,
                            fixture.state.view.hide_ui,
                            fixture.state.view.trigger_hide_ui,
                            fixture.state.view.menu_commands_state}) ==
        std::array{2, 0, 1, 5});
  CHECK(std::array<bool, 4>{fixture.host.Called("show"),
                            fixture.host.Called("sound5"),
                            fixture.host.Called("init-areas"),
                            fixture.host.Called("dismiss")} ==
        std::array{true, true, true, false});
}

TEST_CASE("F04151 Cross and Triangle retain distinct exit destinations",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  const bool triangle = GENERATE(false, true);
  fixture.input.pressed =
      Button(triangle ? map::MapButton::kTriangle : map::MapButton::kCross);
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.quit_state == (triangle ? 2 : 1));
  CHECK(fixture.state.view.menu_state == 3);
  CHECK(fixture.host.Called("sound6"));
  CHECK(fixture.host.Called("dismiss"));
  CHECK(fixture.state.view.menu_commands_state == 0);
}

TEST_CASE("F04151 Paling disables view movement but not exit",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  fixture.paling[1] = 255;
  fixture.input.left_x = 255;
  fixture.input.held = Button(map::MapButton::kStart);
  const auto before = fixture.state.view.camera_angles;
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.camera_angles == before);
  CHECK(fixture.state.view.menu_commands_state == 5);
  CHECK(fixture.host.brightness == 128);
  fixture.input.pressed = Button(map::MapButton::kCross);
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.menu_state == 3);
}

TEST_CASE("F04151 left stick wins over modifiers and tests strict deadzone",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  const auto axis = GENERATE(std::uint8_t{192}, std::uint8_t{193});
  fixture.input.left_x = axis;
  fixture.input.held =
      Button(map::MapButton::kL2) | Button(map::MapButton::kR2);
  fixture.input.right_y = 0;
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.camera_angles.y == (axis == 193 ? 2112 : 2048));
  CHECK(fixture.state.view.camera_angles.x == (axis == 193 ? 568 : 632));
  CHECK(fixture.state.view.camera_distance == 768);
}

TEST_CASE("F04151 digital pitch preserves original pre-step overshoot",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  fixture.state.view.camera_angles.x = 1;
  fixture.input.held =
      Button(map::MapButton::kL2) | Button(map::MapButton::kDown);
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.camera_angles.x == -63);
  fixture.input.right_y = 255;
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.camera_angles.x == 0);
}

TEST_CASE("F04151 zoom and pan distinguish analog clamps from digital steps",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  fixture.state.view.camera_distance = 519;
  fixture.input.held =
      Button(map::MapButton::kR2) | Button(map::MapButton::kUp);
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.camera_distance == 503);
  fixture.input.right_y = 0;
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.camera_distance == 512);
  fixture.input = {};
  fixture.state.view.geometry_offset_y = 79;
  fixture.input.held = Button(map::MapButton::kDown);
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.geometry_offset_y == 83);
  fixture.input.right_y = 255;
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.geometry_offset_y == 80);
}

TEST_CASE("F04151 shoulder repeat changes the room unless both are held",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  const bool both = GENERATE(false, true);
  fixture.input.held = both ? 12U : 8U;
  fixture.input.repeated = 12;
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.current_room == (both ? 0 : 2));
  CHECK(fixture.host.direction == (both ? 0 : 1));
  CHECK(fixture.host.room_name == (both ? -1 : 2));
  CHECK(fixture.host.Called("target2") == !both);
}

TEST_CASE(
    "F04151 Start recenters without changing roll or refreshing room text",
    "[map-terminal][F04151]") {
  MenuFixture fixture;
  fixture.input.held = Button(map::MapButton::kStart);
  fixture.input.field_yaw = 0xFFFF;
  fixture.state.view.geometry_offset_y = 80;
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.view.camera_angles == map::MapAngles{568, 2049, 177, 42});
  CHECK(fixture.state.view.current_room == 3);
  CHECK(fixture.state.view.geometry_offset_y == 0);
  CHECK(fixture.host.Called("center3"));
  CHECK_FALSE(fixture.host.Called("room-name"));
}

TEST_CASE("F04151 area cancellation either restores title or exits to menu",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  fixture.state.view.menu_state = 2;
  fixture.host.selection = GENERATE(-1, -2, -3);
  REQUIRE(fixture.Tick() == 0);
  CHECK(fixture.state.selected_map == fixture.host.selection);
  CHECK(fixture.state.view.menu_state ==
        (fixture.host.selection == -1
             ? 2
             : (fixture.host.selection == -2 ? 1 : 3)));
  CHECK(fixture.host.Called("area1-current") == (fixture.host.selection == -2));
  CHECK(fixture.host.Called("dismiss") == (fixture.host.selection == -3));
}

TEST_CASE(
    "F04151 asynchronous area switch waits for both read and outgoing slide",
    "[map-terminal][F04151]") {
  MenuFixture fixture;
  auto& view = fixture.state.view;
  BeginAreaRead(fixture);
  WaitForReadAndSlide(fixture);
  fixture.host.read_state = 4;
  REQUIRE(fixture.Tick() == 0);
  CHECK(std::array<int, 4>{view.menu_state, view.geometry_offset_x,
                           view.is_paling,
                           view.current_room} == std::array{5, -384, 0, 3});
  CHECK(fixture.host.Called("install11-2048"));
  for (int tick = 0; tick < 6; ++tick) REQUIRE(fixture.Tick() == 0);
  CHECK(view.menu_state == 1);
  CHECK(view.geometry_offset_x == 0);
}

TEST_CASE("F04151 completed quit frees once and skips frame increment",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  auto& view = fixture.state.view;
  view.menu_state = 3;
  view.quit_state = 2;
  for (int tick = 0; tick < 6; ++tick) REQUIRE(fixture.Tick() == 0);
  CHECK(view.background_brightness == 128);
  CHECK(view.ui_animation_state == 2);
  fixture.host.calls.clear();
  REQUIRE(fixture.Tick() == 2);
  CHECK(view.menu_frames == 6);
  CHECK(fixture.host.Called("free-scene"));
  CHECK_FALSE(fixture.host.Called("draw"));
  CHECK_FALSE(fixture.host.Called("animate"));
}

TEST_CASE("F04151 refuses out-of-range scenes before observable work",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  fixture.state.view.current_scene = GENERATE(-1, 32);
  CHECK_FALSE(fixture.Tick());
  CHECK(fixture.host.calls.empty());
}

TEST_CASE("F04151 an unowned scene cannot complete the quit transition",
          "[map-terminal][F04151]") {
  MenuFixture fixture;
  fixture.state.view.menu_state = 3;
  fixture.state.view.background_brightness = 128;
  fixture.host.free_ok = false;
  CHECK_FALSE(fixture.Tick());
  CHECK(fixture.state.view.menu_frames == 0);
  CHECK_FALSE(fixture.host.Called("animate"));
}

TEST_CASE(
    "F04151 refuses invalid chosen rows and preserves a failed read's old view",
    "[map-terminal][F04151]") {
  MenuFixture fixture;
  fixture.state.view.menu_state = 2;
  fixture.host.selection = GENERATE(-4, 32, 1);
  fixture.host.begin_ok = false;
  CHECK_FALSE(fixture.Tick());
  CHECK(fixture.state.view.menu_state == 2);
  CHECK(fixture.state.view.current_scene == 1);
  CHECK(fixture.state.view.menu_frames == 0);
  CHECK_FALSE(fixture.host.Called("animate"));
}

TEST_CASE(
    "F04149 background and scene loading retain original fallthrough delays",
    "[map-terminal][F04149]") {
  ScreenHost host;
  map::MapScreenState state;
  std::uint8_t step = 3;
  BeginScreenLoad(host, state, step);
  for (int tick = 0; tick < 7; ++tick) {
    REQUIRE(map::UpdateScreen(host, state, step, 0) == 0);
  }
  CHECK(step == 5);
  host.calls.clear();
  host.ready = 1;
  REQUIRE(map::UpdateScreen(host, state, step, 0) == 0);
  CHECK(step == 7);
  CHECK(host.calls == std::vector<std::string>{"field-off", "hide-hud",
                                               "hud2097152", "rows-idle"});
}

TEST_CASE("F04149 pending scene keeps HUD and field unchanged at zero delay",
          "[map-terminal][F04149]") {
  ScreenHost host;
  map::MapScreenState state;
  std::uint8_t step = 5;
  REQUIRE(map::UpdateScreen(host, state, step, 0) == 0);
  CHECK(step == 5);
  CHECK(host.calls == std::vector<std::string>{"load-scene"});
}

TEST_CASE(
    "F04149 exit restores field HUD background and only alternate destination",
    "[map-terminal][F04149]") {
  ScreenHost host;
  map::MapScreenState state;
  std::uint8_t step = 7;
  host.result = GENERATE(1, 2);
  BeginScreenExit(host, state, step);
  REQUIRE(map::UpdateScreen(host, state, step, 1) == 0);
  CHECK(step == 9);
  REQUIRE(map::UpdateScreen(host, state, step, 0) == 0);
  CHECK(step == 10);
  REQUIRE(map::UpdateScreen(host, state, step, 0) == 1);
  CHECK(step == 0);
  CHECK(host.destination == (host.result == 2 ? 6 : -1));
  CHECK(std::array<std::string, 3>{host.calls.front(), host.calls[1],
                                   host.calls.back()} ==
        std::array<std::string, 3>{"fade-out", "restore", "show-menu"});
}

TEST_CASE("F04149 missing resources refuse without claiming completion",
          "[map-terminal][F04149]") {
  ScreenHost host;
  host.resource_ok = false;
  map::MapScreenState state;
  auto step = GENERATE(std::uint8_t{3}, std::uint8_t{9}, std::uint8_t{10});
  const auto before = step;
  CHECK_FALSE(map::UpdateScreen(host, state, step, 0));
  CHECK(step == before);
  REQUIRE(host.calls.size() == 1);
}
