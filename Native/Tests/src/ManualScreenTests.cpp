#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Hud.h"
#include "Content/CdDriveLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Field/Manual.h"
#include "Field/RoomRenderingLiveState.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Manual.h"
#include "Menu/ManualScreen.hpp"
#include "Menu/ManualScreenLive.hpp"
#include "Menu/ManualTopics.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPrimitives.h"

// Contracts of the manual screen's lifecycle and help pages (MENUE.PRG
// 494.c:132-448): the unused menu wrappers, the screen's open, wait, show and
// close steps, the help state's reset, and the help page loop's topic choice,
// asset and text loads, paging, drawing and release.
namespace {
namespace manual = VS::Menu::manual;
namespace fw = VS::Menu::Framework;
using manual::HelpAssetHeader;
using manual::HelpFile;
using manual::HelpPagesState;
using manual::ScreenState;

using Events = std::vector<std::string>;

// --- Screen -----------------------------------------------------------------

class FakeScreenHost final : public manual::ScreenHost {
 public:
  Events events;
  std::int32_t idle_result = 0;
  std::int32_t init_result = 1;
  std::int32_t pages_result = 0;
  std::int32_t fade_step = 0;

  void ClearMenuExcept(std::int32_t row) override {
    events.push_back(std::format("clear:{}", row));
  }
  [[nodiscard]] std::int32_t MenuRowsIdle() override {
    events.emplace_back("idle");
    return idle_result;
  }
  void SetBackgroundFade(std::int32_t fade) override {
    events.push_back(std::format("fade:{}", fade));
  }
  [[nodiscard]] std::int32_t InitializeHelp() override {
    events.emplace_back("init");
    return init_result;
  }
  [[nodiscard]] std::int32_t UpdateHelpPages() override {
    events.emplace_back("pages");
    return pages_result;
  }
  void SetFieldRenderingEnabled(std::int32_t enabled) override {
    events.push_back(std::format("field:{}", enabled));
  }
  void SetHudHidden(std::int32_t hidden) override {
    events.push_back(std::format("hud:{}", hidden));
  }
  void SetHudSlideOffset(std::int32_t offset) override {
    events.push_back(std::format("slide:{:X}", offset));
  }
  void SetBackgroundPriority(std::int32_t layer,
                             std::int32_t brightness) override {
    events.push_back(std::format("priority:{},{}", layer, brightness));
  }
  void SetMenuCommand(std::int32_t command) override {
    events.push_back(std::format("command:{}", command));
  }
  [[nodiscard]] std::int32_t BackgroundFadeStep() const override {
    return fade_step;
  }
  void SetCurrentMenu(std::int32_t menu) override {
    events.push_back(std::format("menu:{}", menu));
  }
  void RevealMenu() override { events.emplace_back("reveal"); }
};

// One UpdateScreen call: its return, the step after it and its host calls.
struct ScreenFrame {
  std::int32_t result = 0;
  std::uint8_t step = 0;
  Events events;

  friend bool operator==(const ScreenFrame&, const ScreenFrame&) = default;
  friend std::ostream& operator<<(std::ostream& out, const ScreenFrame& frame) {
    out << "result " << frame.result << ", step " << int{frame.step}
        << ", events";
    for (const std::string& event : frame.events) {
      out << ' ' << event;
    }
    return out;
  }
};

ScreenFrame RunScreen(FakeScreenHost& host, std::uint8_t& step,
                      ScreenState& state) {
  host.events.clear();
  const std::int32_t result = manual::UpdateScreen(host, step, state);
  return {.result = result, .step = step, .events = std::move(host.events)};
}

TEST_CASE("F04159 ClearMenuExceptRow8 clears every menu row but row 8",
          "[manual-screen]") {
  FakeScreenHost host;
  manual::ClearMenuExceptRow8(host);
  CHECK(host.events == Events{"clear:8"});
}

TEST_CASE("F04159 ClearMenuExceptRow8 repeats the same clear on reentry",
          "[manual-screen]") {
  FakeScreenHost host;
  manual::ClearMenuExceptRow8(host);
  manual::ClearMenuExceptRow8(host);
  CHECK(host.events == Events{"clear:8", "clear:8"});
}

TEST_CASE("F04160 PollMenuReady polls once and drops a busy or idle answer",
          "[manual-screen]") {
  FakeScreenHost host;
  host.idle_result = GENERATE(0, 1, -1);
  manual::PollMenuReady(host);
  CHECK(host.events == Events{"idle"});
}

TEST_CASE("F04160 PollMenuReady makes no other menu call on reentry",
          "[manual-screen]") {
  FakeScreenHost host;
  manual::PollMenuReady(host);
  manual::PollMenuReady(host);
  CHECK(host.events == Events{"idle", "idle"});
}

TEST_CASE("F04161 UpdateScreen fades in then waits eight frames to open",
          "[manual-screen]") {
  FakeScreenHost host;
  ScreenState state{.open_delay = 3, .initialized = 9, .help_result = 5};
  std::uint8_t step = manual::kScreenOpen;
  CHECK(RunScreen(host, step, state) ==
        ScreenFrame{.step = manual::kScreenInitialize, .events = {"fade:1"}});
  CHECK(state ==
        ScreenState{.open_delay = 8, .initialized = 0, .help_result = 5});

  CHECK(RunScreen(host, step, state) ==
        ScreenFrame{.step = manual::kScreenInitialize, .events = {"init"}});
  CHECK(state.open_delay == 7);
  std::vector<ScreenFrame> waits;
  for (int frame = 0; frame < 7; ++frame) {
    waits.push_back(RunScreen(host, step, state));
  }
  CHECK(waits ==
        std::vector<ScreenFrame>(
            7, ScreenFrame{.step = manual::kScreenInitialize, .events = {}}));
  CHECK(state.open_delay == 0);
  CHECK(RunScreen(host, step, state) ==
        ScreenFrame{.step = manual::kScreenWaitReady,
                    .events = {"field:0", "hud:1", "slide:200000"}});
}

TEST_CASE("F04161 UpdateScreen retries initialization and keeps its low byte",
          "[manual-screen]") {
  FakeScreenHost host;
  ScreenState state{.open_delay = 0, .initialized = 0, .help_result = 0};
  std::uint8_t step = manual::kScreenInitialize;
  host.init_result = 0;
  CHECK(manual::UpdateScreen(host, step, state) == 0);
  host.init_result = 0x100;
  CHECK(manual::UpdateScreen(host, step, state) == 0);
  CHECK(host.events == Events{"init", "init"});
  CHECK(state.initialized == 0);
  CHECK(step == manual::kScreenInitialize);

  host.init_result = 0x102;
  host.events.clear();
  CHECK(manual::UpdateScreen(host, step, state) == 0);
  CHECK(state.initialized == 2);
  CHECK(host.events == Events{"init", "field:0", "hud:1", "slide:200000"});
  CHECK(step == manual::kScreenWaitReady);
}

TEST_CASE("F04161 UpdateScreen adds the menu ready answer to its step byte",
          "[manual-screen]") {
  FakeScreenHost host;
  ScreenState state;
  std::uint8_t step = manual::kScreenWaitReady;
  CHECK(manual::UpdateScreen(host, step, state) == 0);
  CHECK(step == manual::kScreenWaitReady);
  host.idle_result = 1;
  CHECK(manual::UpdateScreen(host, step, state) == 0);
  CHECK(step == manual::kScreenShow);
  step = manual::kScreenWaitReady;
  host.idle_result = -5;
  CHECK(manual::UpdateScreen(host, step, state) == 0);
  CHECK(step == manual::kScreenClosed);
  CHECK(host.events == Events{"idle", "idle", "idle"});
}

TEST_CASE("F04161 UpdateScreen closes the help and restores the HUD",
          "[manual-screen]") {
  FakeScreenHost host;
  ScreenState state;
  std::uint8_t step = manual::kScreenShow;
  CHECK(RunScreen(host, step, state) ==
        ScreenFrame{.step = manual::kScreenShow, .events = {"pages"}});

  host.pages_result = manual::kHelpExitToMenu;
  CHECK(RunScreen(host, step, state) ==
        ScreenFrame{.step = manual::kScreenShutDown,
                    .events = {"pages", "priority:-4,128", "hud:0", "slide:0",
                               "field:1", "command:0", "clear:40"}});
  CHECK(state.help_result == manual::kHelpExitToMenu);
  CHECK(RunScreen(host, step, state) ==
        ScreenFrame{.step = manual::kScreenExit, .events = {"fade:0"}});

  host.fade_step = 3;
  CHECK(RunScreen(host, step, state) ==
        ScreenFrame{.step = manual::kScreenExit, .events = {}});
  host.fade_step = 0;
  CHECK(RunScreen(host, step, state) ==
        ScreenFrame{
            .result = 1, .step = manual::kScreenClosed, .events = {"reveal"}});
}

TEST_CASE("F04161 UpdateScreen returns to the game menu after a game exit",
          "[manual-screen]") {
  FakeScreenHost host;
  ScreenState state{.open_delay = 0,
                    .initialized = 1,
                    .help_result = manual::kHelpExitToGame};
  std::uint8_t step = manual::kScreenExit;
  CHECK(manual::UpdateScreen(host, step, state) == 1);
  CHECK(host.events == Events{"menu:10", "reveal"});
  CHECK(step == manual::kScreenClosed);
}

TEST_CASE("F04161 UpdateScreen ignores idle steps", "[manual-screen]") {
  FakeScreenHost host;
  ScreenState state{.open_delay = 4, .initialized = 1, .help_result = 2};
  const std::uint8_t idle =
      GENERATE(std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2},
               std::uint8_t{9}, std::uint8_t{255});
  std::uint8_t step = idle;
  CHECK(manual::UpdateScreen(host, step, state) == 0);
  CHECK(step == idle);
  CHECK(host.events.empty());
  CHECK(state ==
        ScreenState{.open_delay = 4, .initialized = 1, .help_result = 2});
}

// --- Help pages -------------------------------------------------------------

std::string_view Name(HelpFile file) {
  return file == HelpFile::kText ? "text" : "assets";
}

class FakeHelpHost final : public manual::HelpPagesHost {
 public:
  Events events;
  std::vector<std::string> refusals;
  std::uint8_t setting = 0;
  manual::TopicCursor cursor{.row = 2, .scroll = 1};
  std::int32_t topic_row = manual::kNoTopicSelected;
  std::uint16_t current_manual = 0;
  std::array<std::uint16_t, manual::kTopicCount> memories{};
  std::array<bool, 2> held{};
  std::array<std::int16_t, 2> file_states{};
  std::optional<HelpAssetHeader> header = HelpAssetHeader{};
  std::int32_t last_line = 0;
  std::uint32_t pressed = 0;
  std::uint32_t previous = 0;
  std::uint32_t repeated = 0;
  std::vector<manual::TextBoxPlacement> placements;

  [[nodiscard]] std::uint8_t CursorMemorySetting() const override {
    return setting;
  }
  void ClearDisplayState() override { events.emplace_back("clear-display"); }
  void ReserveTexturePages(std::uint32_t first, std::uint32_t count,
                           std::int32_t owner) override {
    events.push_back(std::format("reserve:{},{},{}", first, count, owner));
  }
  void RetireTexturePages(std::uint32_t first, std::uint32_t count,
                          std::int32_t owner) override {
    events.push_back(std::format("retire:{},{},{}", first, count, owner));
  }
  void SetBackgroundPriority(std::int32_t layer,
                             std::int32_t brightness) override {
    events.push_back(std::format("priority:{},{}", layer, brightness));
  }
  void InitializeInformationBox() override { events.emplace_back("info"); }
  void DismissInformationBox() override { events.emplace_back("dismiss"); }
  [[nodiscard]] std::int32_t UpdateTopicMenu(std::int32_t init) override {
    events.push_back(std::format("topics:{}", init));
    return topic_row;
  }
  [[nodiscard]] manual::TopicCursor TopicMenuCursor() const override {
    return cursor;
  }
  [[nodiscard]] std::uint16_t CurrentManual() const override {
    return current_manual;
  }
  void SetCurrentManual(std::uint16_t manual) override {
    events.push_back(std::format("manual:{}", manual));
    current_manual = manual;
  }
  [[nodiscard]] std::uint16_t ScrollMemory(std::size_t topic) const override {
    return memories.at(topic);
  }
  void SetScrollMemory(std::size_t topic, std::uint16_t scroll) override {
    events.push_back(std::format("memory:{}={}", topic, scroll));
    memories.at(topic) = scroll;
  }
  [[nodiscard]] bool HelpBufferHeld(HelpFile file) const override {
    return held.at(static_cast<std::size_t>(file));
  }
  // vs_main_freeHeapR keeps the pointer.
  void FreeHelpBuffer(HelpFile file) override {
    events.push_back(std::format("free:{}", Name(file)));
  }
  void ForgetHelpBuffer(HelpFile file) override {
    events.push_back(std::format("forget:{}", Name(file)));
    held.at(static_cast<std::size_t>(file)) = false;
  }
  void AllocateHelpBuffer(HelpFile file, std::int32_t topic) override {
    events.push_back(std::format("alloc:{}:{}", Name(file), topic));
    held.at(static_cast<std::size_t>(file)) = true;
  }
  void RequestHelpFile(HelpFile file, std::int32_t topic) override {
    events.push_back(std::format("request:{}:{}", Name(file), topic));
  }
  [[nodiscard]] std::int16_t HelpFileState(HelpFile file) const override {
    return file_states.at(static_cast<std::size_t>(file));
  }
  void ReleaseHelpFile(HelpFile file) override {
    events.push_back(std::format("release:{}", Name(file)));
  }
  void CancelHelpFile(HelpFile file) override {
    events.push_back(std::format("cancel:{}", Name(file)));
  }
  [[nodiscard]] std::optional<HelpAssetHeader> HelpAssets() const override {
    return header;
  }
  void UploadSpriteTiles(std::uint32_t byte_offset,
                         std::uint32_t count) override {
    events.push_back(std::format("tiles:{},{}", byte_offset, count));
  }
  void UploadCluts(std::uint32_t byte_offset, std::uint32_t count) override {
    events.push_back(std::format("cluts:{},{}", byte_offset, count));
  }
  void InitTextBox(const manual::TextBoxPlacement& placement) override {
    events.emplace_back("textbox");
    placements.push_back(placement);
  }
  void HideTextBox(std::int32_t id) override {
    events.push_back(std::format("hide:{}", id));
  }
  [[nodiscard]] std::int32_t LastPageLineOffset() override {
    events.emplace_back("last-line");
    return last_line;
  }
  [[nodiscard]] std::uint32_t PressedButtons() const override {
    return pressed;
  }
  [[nodiscard]] std::uint32_t PreviousButtons() const override {
    return previous;
  }
  [[nodiscard]] std::uint32_t RepeatedButtons() const override {
    return repeated;
  }
  void PlaySfx(std::int32_t group, std::int32_t effect) override {
    events.push_back(std::format("sfx:{},{}", group, effect));
  }
  void DrawButtonIcon(std::int32_t button, std::int32_t x, std::int32_t y,
                      std::int32_t layer) override {
    events.push_back(std::format("button:{},{},{},{}", button, x, y, layer));
  }
  void DrawText(std::string_view text, std::int32_t xy, std::int32_t color,
                std::int32_t layer) override {
    events.push_back(
        std::format("text:{}@{:X}/{:X}/{}", text, xy, color, layer));
  }
  void DrawControlsBackground(const manual::ScreenRect& rect) override {
    events.push_back(
        std::format("controls:{},{},{},{}", rect.x, rect.y, rect.w, rect.h));
  }
  void DrawPageBackground(const manual::ScreenRect& rect,
                          std::int32_t color) override {
    events.push_back(std::format("page-bg:{},{},{},{},{}", rect.x, rect.y,
                                 rect.w, rect.h, color));
  }
  void DrawUpperScrollFade() override { events.emplace_back("fade-up"); }
  void DrawLowerScrollFade() override { events.emplace_back("fade-down"); }
  void DrawPageContent() override { events.emplace_back("content"); }
  void CopyPageText() override { events.emplace_back("copy"); }
  void Refuse(std::string_view reason) override {
    refusals.emplace_back(reason);
  }
};

// The control hints and page number one shown frame draws.
Events Controls(int page, int last_page) {
  return {"button:1,8,16,6",
          "text:TURN    PAGE@12001C/808080/6",
          "controls:16,18,96,12",
          "button:2,8,34,6",
          "text:MENU@24001C/808080/6",
          "controls:16,36,64,12",
          "text:PAGE@C400D8/808080/7",
          std::format("text:#{}/@C40118/808080/7", page),
          std::format("text:#{}@C40130/808080/7", last_page)};
}

Events Concat(Events first, const Events& second) {
  first.insert(first.end(), second.begin(), second.end());
  return first;
}

HelpPagesState ShownPage(std::int32_t scroll, std::int32_t line_count) {
  return {.step = manual::kHelpShowPage,
          .scroll = scroll,
          .line_count = line_count,
          .arrow_frame = 0,
          .load_counter = 0};
}

TEST_CASE("F04162 InitHelpState forgets the topics with cursor memory off",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = 5,
                       .scroll = 30,
                       .line_count = 40,
                       .arrow_frame = 7,
                       .load_counter = 8};
  host.held = {true, true};
  CHECK(manual::InitHelpState(host, state) == 1);
  CHECK(host.events == Events{"clear-display", "reserve:29,3,5", "forget:text",
                              "forget:assets"});
  CHECK(state == HelpPagesState{.step = 0,
                                .scroll = 0,
                                .line_count = 0,
                                .arrow_frame = 7,
                                .load_counter = 8});
}

TEST_CASE("F04162 InitHelpState keeps the remembered topics with memory on",
          "[manual-screen]") {
  FakeHelpHost host;
  host.setting = GENERATE(std::uint8_t{1}, std::uint8_t{0x80});
  HelpPagesState state = ShownPage(12, 20);
  CHECK(manual::InitHelpState(host, state) == 1);
  CHECK(host.events ==
        Events{"reserve:29,3,5", "forget:text", "forget:assets"});
  CHECK(state.step == manual::kHelpStart);
}

TEST_CASE("F04162 InitHelpState drops held buffers without freeing them",
          "[manual-screen]") {
  FakeHelpHost host;
  host.held = {true, true};
  HelpPagesState state = ShownPage(3, 9);
  CHECK(manual::InitHelpState(host, state) == 1);
  CHECK(manual::InitHelpState(host, state) == 1);
  CHECK(std::ranges::none_of(host.events, [](const std::string& event) {
    return event.starts_with("free");
  }));
  CHECK(host.held == std::array<bool, 2>{false, false});
  CHECK(host.refusals.empty());
}

TEST_CASE("F04163 UpdateHelpPages raises the background then lists topics",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state;
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"priority:2046,128"});
  CHECK(state.step == manual::kHelpListTopics);
  host.events.clear();
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"info", "topics:1"});
  CHECK(state.step == manual::kHelpSelectTopic);
}

TEST_CASE("F04163 UpdateHelpPages waits on the topics and leaves on a cancel",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpSelectTopic};
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"topics:0"});
  CHECK(state.step == manual::kHelpSelectTopic);

  const auto [row, step] = GENERATE(std::pair{-2, manual::kHelpLeaveToMenu},
                                    std::pair{-4, manual::kHelpLeaveToMenu},
                                    std::pair{-3, manual::kHelpLeaveToGame});
  host.events.clear();
  host.topic_row = row;
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"topics:0", "manual:3", "dismiss"});
  CHECK(state.step == step);
}

TEST_CASE("F04163 UpdateHelpPages requests the chosen topic's files",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpSelectTopic};
  host.topic_row = 13;
  host.cursor = {.row = 5, .scroll = 8};
  host.held = {true, true};
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"topics:0", "free:text", "free:assets",
                              "alloc:text:13", "alloc:assets:13",
                              "request:assets:13", "request:text:13",
                              "manual:13", "dismiss"});
  CHECK(state.step == manual::kHelpLoadAssets);

  host.events.clear();
  host.held = {false, false};
  host.topic_row = 0;
  state.step = manual::kHelpSelectTopic;
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"topics:0", "alloc:text:0", "alloc:assets:0",
                              "request:assets:0", "request:text:0", "manual:13",
                              "dismiss"});
}

TEST_CASE("F04163 UpdateHelpPages refuses a topic row past the help files",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpSelectTopic};
  host.topic_row = GENERATE(14, 0x7FFFFFFF);
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"topics:0", "manual:3", "dismiss"});
  CHECK(host.refusals ==
        std::vector<std::string>{"Manual topic row past the help files"});
  CHECK(state.step == manual::kHelpLeaveToMenu);
}

TEST_CASE("F04163 UpdateHelpPages uploads the loaded assets and frees them",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpLoadAssets};
  host.held = {true, true};
  host.file_states = {3, 4};
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events.empty());
  CHECK(state.step == manual::kHelpLoadAssets);

  host.file_states = {4, 0};
  host.header = HelpAssetHeader{.sprite_tiles = 3, .cluts = 2};
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"release:assets", "tiles:8,3", "cluts:392,2",
                              "free:assets", "forget:assets"});
  CHECK(state.step == manual::kHelpLoadText);
  CHECK(host.held == std::array<bool, 2>{false, true});
}

TEST_CASE("F04163 UpdateHelpPages wraps the CLUT offset past the tiles",
          "[manual-screen]") {
  // The CLUT offset wraps like the original's 32-bit sum.
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpLoadAssets};
  host.file_states = {4, 0};
  host.header = HelpAssetHeader{.sprite_tiles = 0x02000001, .cluts = 1};
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"release:assets", "tiles:8,33554433",
                              "cluts:136,1", "forget:assets"});
  CHECK(state.step == manual::kHelpLoadText);
}

TEST_CASE("F04163 UpdateHelpPages refuses loaded assets without a buffer",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpLoadAssets};
  host.file_states = {4, 0};
  host.header = std::nullopt;
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"release:assets", "forget:assets"});
  CHECK(host.refusals ==
        std::vector<std::string>{"Manual help assets loaded without an asset "
                                 "buffer"});
  CHECK(state.step == manual::kHelpLoadText);
}

TEST_CASE("F04163 UpdateHelpPages opens the text at its remembered position",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpLoadText,
                       .scroll = 99,
                       .line_count = 99,
                       .arrow_frame = 5,
                       .load_counter = 6};
  host.file_states = {0, 1};
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events.empty());

  host.file_states = {0, 4};
  host.current_manual = 6;
  host.memories[6] = 17;
  host.last_line = 30;
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"release:text", "textbox", "last-line"});
  CHECK(host.placements ==
        std::vector<manual::TextBoxPlacement>{manual::kPageTextPlacement});
  CHECK(state == HelpPagesState{.step = manual::kHelpShowPage,
                                .scroll = 17,
                                .line_count = 30,
                                .arrow_frame = 5,
                                .load_counter = 0});
}

TEST_CASE("F04163 UpdateHelpPages refuses a topic past the scroll memories",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpLoadText, .scroll = 99};
  host.file_states = {0, 4};
  host.current_manual = GENERATE(std::uint16_t{14}, std::uint16_t{0xFFFF});
  host.last_line = 10;
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(state.scroll == 0);
  CHECK(state.step == manual::kHelpShowPage);

  host.events.clear();
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(std::ranges::none_of(host.events, [](const std::string& event) {
    return event.starts_with("memory");
  }));
  CHECK(host.refusals ==
        std::vector<std::string>{"Manual topic past the scroll memories",
                                 "Manual topic past the scroll memories"});
}

TEST_CASE("F04163 UpdateHelpPages draws the controls page numbers and page",
          "[manual-screen]") {
  FakeHelpHost host;
  host.current_manual = 3;
  HelpPagesState state = ShownPage(12, 20);
  state.arrow_frame = 15;
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events ==
        Concat(Controls(2, 3), {"memory:3=12", "page-bg:8,52,310,136,0",
                                "fade-up", "fade-down", "copy"}));
  CHECK(state == HelpPagesState{.step = manual::kHelpShowPage,
                                .scroll = 12,
                                .line_count = 20,
                                .arrow_frame = 0,
                                .load_counter = 1});

  // The first page has no upper fade, the last no lower; after eight frames
  // the illustrations are drawn.
  host.events.clear();
  state = ShownPage(0, 0);
  state.load_counter = manual::kPageContentDelay;
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events ==
        Concat(Controls(1, 1),
               {"memory:3=0", "page-bg:8,52,310,136,0", "content", "copy"}));
  CHECK(state.load_counter == manual::kPageContentDelay);
  CHECK(state.arrow_frame == 1);
}

TEST_CASE("F04163 UpdateHelpPages turns whole pages while Square is held",
          "[manual-screen]") {
  FakeHelpHost host;
  host.previous = manual::kButtonSquare;
  struct Turn {
    std::uint32_t pressed;
    std::uint32_t repeated;
    std::int32_t scroll;
    std::int32_t line_count;
    std::int32_t turned;
    int page;
  };
  const Turn turn = GENERATE(Turn{.pressed = manual::kButtonUp,
                                  .repeated = 0,
                                  .scroll = 4,
                                  .line_count = 20,
                                  .turned = 20,
                                  .page = 3},
                             Turn{.pressed = manual::kButtonDown,
                                  .repeated = 0,
                                  .scroll = 25,
                                  .line_count = 20,
                                  .turned = 0,
                                  .page = 1},
                             Turn{.pressed = 0,
                                  .repeated = manual::kButtonDown,
                                  .scroll = 5,
                                  .line_count = 20,
                                  .turned = 10,
                                  .page = 2},
                             Turn{.pressed = 0,
                                  .repeated = manual::kButtonUp,
                                  .scroll = 15,
                                  .line_count = 20,
                                  .turned = 0,
                                  .page = 1});
  host.pressed = turn.pressed;
  host.repeated = turn.repeated;
  HelpPagesState state = ShownPage(turn.scroll, turn.line_count);
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(state.scroll == turn.turned);
  REQUIRE(host.events.size() > 10);
  CHECK(host.events.front() == "sfx:126,4");
  CHECK(host.events[8] == std::format("text:#{}/@C40118/808080/7", turn.page));
}

TEST_CASE("F04163 UpdateHelpPages keeps a page without a turn silently",
          "[manual-screen]") {
  FakeHelpHost host;
  host.previous = manual::kButtonSquare;
  host.pressed = manual::kButtonUp;
  host.repeated = manual::kButtonUp | manual::kButtonDown;
  HelpPagesState state = ShownPage(7, 9);
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(state.scroll == 7);
  CHECK(host.events.front() == "button:1,8,16,6");
}

TEST_CASE("F04163 UpdateHelpPages scrolls lines and draws the frame's page",
          "[manual-screen]") {
  FakeHelpHost host;
  struct Scroll {
    std::uint32_t pressed;
    std::uint32_t repeated;
    std::int32_t scroll;
    std::int32_t scrolled;
  };
  const Scroll scroll = GENERATE(Scroll{.pressed = 0,
                                        .repeated = manual::kButtonDown,
                                        .scroll = 9,
                                        .scrolled = 10},
                                 Scroll{.pressed = 0,
                                        .repeated = manual::kButtonUp,
                                        .scroll = 10,
                                        .scrolled = 9},
                                 Scroll{.pressed = manual::kButtonUp,
                                        .repeated = 0,
                                        .scroll = 0,
                                        .scrolled = 20},
                                 Scroll{.pressed = manual::kButtonDown,
                                        .repeated = 0,
                                        .scroll = 20,
                                        .scrolled = 0});
  host.pressed = scroll.pressed;
  host.repeated = scroll.repeated;
  HelpPagesState state = ShownPage(scroll.scroll, 20);
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(state.scroll == scroll.scrolled);
  REQUIRE(host.events.size() > 10);
  CHECK(host.events.front() == "sfx:126,4");
  // The drawn page is the frame's first one; the memory is the new position.
  CHECK(host.events[8] ==
        std::format("text:#{}/@C40118/808080/7", (scroll.scroll / 10) + 1));
  CHECK(host.events[10] == std::format("memory:0={}", scroll.scrolled));
}

TEST_CASE("F04163 UpdateHelpPages Start returns to the first page silently",
          "[manual-screen]") {
  FakeHelpHost host;
  host.pressed = manual::kButtonStart | manual::kButtonDown;
  host.previous = GENERATE(0U, manual::kButtonSquare);
  host.repeated = manual::kButtonDown;
  HelpPagesState state = ShownPage(15, 20);
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(state.scroll == 0);
  CHECK(host.events ==
        Concat(Controls(1, 3),
               {"memory:0=0", "page-bg:8,52,310,136,0", "fade-down", "copy"}));
}

TEST_CASE("F04163 UpdateHelpPages leaves the page on Circle Cross or Triangle",
          "[manual-screen]") {
  FakeHelpHost host;
  struct Leave {
    std::uint32_t pressed;
    std::int32_t step;
    std::int32_t sfx;
  };
  const Leave leave =
      GENERATE(Leave{.pressed = manual::kButtonCircle,
                     .step = manual::kHelpListTopics,
                     .sfx = manual::kSfxSelect},
               Leave{.pressed = manual::kButtonCross,
                     .step = manual::kHelpListTopics,
                     .sfx = manual::kSfxLeave},
               Leave{.pressed = manual::kButtonTriangle,
                     .step = manual::kHelpLeaveToGame,
                     .sfx = manual::kSfxLeave},
               Leave{.pressed = manual::kButtonTriangle | manual::kButtonCircle,
                     .step = manual::kHelpLeaveToGame,
                     .sfx = manual::kSfxSelect});
  host.pressed = leave.pressed;
  host.held = {false, true};
  host.current_manual = 4;
  HelpPagesState state = ShownPage(10, 20);
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(state.step == leave.step);
  CHECK(host.events ==
        Concat(Controls(2, 3),
               {"memory:4=10", std::format("sfx:126,{}", leave.sfx), "hide:0",
                "free:text", "forget:text"}));
  CHECK(state.arrow_frame == 0);
  CHECK(state.load_counter == 0);
}

TEST_CASE("F04163 UpdateHelpPages keeps the scroll memory's low halfword",
          "[manual-screen]") {
  FakeHelpHost host;
  host.current_manual = 13;
  HelpPagesState state = ShownPage(0x10005, 0x10005);
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.memories[13] == 5);
  CHECK(host.events[7] == "text:#6555/@C40118/808080/7");
}

TEST_CASE("F04163 UpdateHelpPages leaves with both buffers freed and kept",
          "[manual-screen]") {
  FakeHelpHost host;
  host.held = {true, true};
  HelpPagesState state{.step = manual::kHelpLeaveToMenu};
  CHECK(manual::UpdateHelpPages(host, state) == manual::kHelpExitToMenu);
  CHECK(host.events == Events{"free:text", "free:assets", "retire:29,3,5"});
  CHECK(host.held == std::array<bool, 2>{true, true});
  CHECK(state.step == manual::kHelpLeaveToMenu);

  host.events.clear();
  host.held = {false, false};
  state.step = manual::kHelpLeaveToGame;
  CHECK(manual::UpdateHelpPages(host, state) == manual::kHelpExitToGame);
  CHECK(host.events == Events{"retire:29,3,5"});
}

TEST_CASE("F04163 UpdateHelpPages ignores idle steps", "[manual-screen]") {
  FakeHelpHost host;
  const std::int32_t idle = GENERATE(-1, 8, 0x7FFFFFFF);
  HelpPagesState state{.step = idle};
  const HelpPagesState before = state;
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events.empty());
  CHECK(state == before);
}

// --- Live bindings ----------------------------------------------------------

constexpr std::size_t kPacketWords = 0x8000;
constexpr std::size_t kTableWords = 0x822;
constexpr std::size_t kFrameSlotBase = 18;
// The main menu's information box (FrameworkLive.cpp).
constexpr int kInformationBox = 7;

// Every global the screen and help bindings read or write, a menu row table,
// a game heap, the disc requests and a frame's primitive buffer and
// ordering table; all are restored afterwards.
struct LiveManualFixture {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 1024> arena{};
  Menu_Framework_MenuRow* saved_rows = VS::Menu::Framework::menuItems;
  int* saved_text = Menu_Manual_helpText;
  u_long* saved_assets = Menu_Manual_helpAssets;
  Content_DiscQueueSlot* saved_text_request = Menu_Manual_helpTextCdQueue;
  Content_DiscQueueSlot* saved_assets_request = Menu_Manual_helpAssetsCdQueue;
  void* saved_primitives = Render_legacyPrimitiveBuffers[0];
  void* saved_ordering = Render_legacyOrderingTables[0];
  Menu_Framework_MenuControlState saved_menu_state =
      VS::Menu::Framework::menuState;
  std::vector<u_long> storage = std::vector<u_long>(kPacketWords, 0xDEADBEEF);
  std::vector<u_long> ordering = std::vector<u_long>(kTableWords);
  Menu_TextBoxState saved_box = *VS::Menu::LiveTextBox(manual::kPageTextBox);
  Menu_TextBoxState saved_information_box =
      *VS::Menu::LiveTextBox(kInformationBox);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Render_FrameScratch saved_frame = Render_frameScratch;
  std::array<Content_DiscQueueSlot, 32> saved_requests =
      std::to_array(Content_discRequestSlots);
  std::array<Menu_Framework_MenuRow, 40> rows{};
  int saved_hud_slide = Battle_hudSlideOffset;
  int saved_field_rendering = VS::Field::isFieldRenderingEnabled;
  int saved_fade_step = VS::Menu::Framework::backgroundFadeStep;
  int saved_selected = VS::Menu::Framework::selectedRow;
  u_int saved_pressed = VS::Input::buttonsPressed;
  int saved_previous = VS::Input::previousButtons;
  int saved_repeat = VS::Input::buttonRepeat;
  int saved_step = Menu_Manual_showMenuState;
  int saved_scroll = Menu_Manual_scrollPosition;
  int saved_line_count = Menu_Manual_lineCount;
  int saved_arrow = Menu_Manual_pageArrowAnimState;
  int saved_load_counter = Menu_Manual_helpPageLoadCounter;
  int saved_topic_step = Menu_Manual_topMenuState;
  int saved_topic_row = Menu_Manual_selectedRow;
  int saved_frame_index = Render_frameBuf;
  ScreenState saved_screen = manual::LiveManualScreenState();
  State_Settings saved_settings = VS::Input::settings;
  DRAWENV saved_back_env = Render_legacyDrawEnvironments[1];
  Content_DiscRequestCounts saved_request_counts = Content_discRequestCounts;
  Field_ManualDisplayState saved_display = Field_manualDisplayState;
  Menu_Framework_MenuMemory saved_memory = VS::Menu::Framework::menuMemory;
  char saved_hud_hidden = Battle_hudHidden;
  char saved_lower_screen = Battle_lowerScreenUiState;
  char saved_hide_menu = VS::Menu::Framework::hideMenu;
  VS::Render::TexturePageTable saved_pages = VS::Render::LiveTexturePages();

  LiveManualFixture() {
    std::ranges::fill(Content_discRequestSlots, Content_DiscQueueSlot{});
    Content_discRequestCounts = Content_DiscRequestCounts{};
    VS::Menu::Framework::menuItems = rows.data();
    VS::Input::buttonsPressed = 0;
    VS::Input::previousButtons = 0;
    VS::Input::buttonRepeat = 0;
    Menu_Manual_showMenuState = manual::kHelpStart;
    Menu_Manual_scrollPosition = 0;
    Menu_Manual_lineCount = 0;
    Menu_Manual_pageArrowAnimState = 0;
    Menu_Manual_helpPageLoadCounter = 0;
    Menu_Manual_helpText = nullptr;
    Menu_Manual_helpAssets = nullptr;
    Menu_Manual_helpTextCdQueue = nullptr;
    Menu_Manual_helpAssetsCdQueue = nullptr;
    Field_manualDisplayState = Field_ManualDisplayState{};
    // The page text box as the manual opens it (ManualTopicsTests).
    VS::Menu::LiveTextBox(manual::kPageTextBox)->flags.speakerTexture = 32;
    VS::Render::InitializeLiveTexturePages();
    manual::ResetLiveManualScreenStatics();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_frameBuf = 0;
    Render_legacyOrderingTables[0] = ordering.data();
    Render_legacyPrimitiveBuffers[0] = storage.data();
    Render_frameScratch.orderingTable = ordering.data() + kFrameSlotBase;
    Render_frameScratch.primitiveCursor = storage.data();
  }
  LiveManualFixture(const LiveManualFixture&) = delete;
  LiveManualFixture& operator=(const LiveManualFixture&) = delete;
  LiveManualFixture(LiveManualFixture&&) = delete;
  LiveManualFixture& operator=(LiveManualFixture&&) = delete;
  ~LiveManualFixture() {
    fw::ResetLiveMainMenuStatics();
    Battle_hudHidden = saved_hud_hidden;
    Battle_hudSlideOffset = saved_hud_slide;
    Battle_lowerScreenUiState = saved_lower_screen;
    VS::Field::isFieldRenderingEnabled = saved_field_rendering;
    VS::Menu::Framework::hideMenu = saved_hide_menu;
    VS::Menu::Framework::menuState = saved_menu_state;
    VS::Menu::Framework::backgroundFadeStep = saved_fade_step;
    VS::Menu::Framework::menuMemory = saved_memory;
    VS::Menu::Framework::menuItems = saved_rows;
    VS::Menu::Framework::selectedRow = saved_selected;
    VS::Input::settings = saved_settings;
    VS::Input::buttonsPressed = saved_pressed;
    VS::Input::previousButtons = saved_previous;
    VS::Input::buttonRepeat = saved_repeat;
    Field_manualDisplayState = saved_display;
    Menu_Manual_showMenuState = saved_step;
    Menu_Manual_scrollPosition = saved_scroll;
    Menu_Manual_lineCount = saved_line_count;
    Menu_Manual_pageArrowAnimState = saved_arrow;
    Menu_Manual_helpPageLoadCounter = saved_load_counter;
    Menu_Manual_helpText = saved_text;
    Menu_Manual_helpAssets = saved_assets;
    Menu_Manual_helpTextCdQueue = saved_text_request;
    Menu_Manual_helpAssetsCdQueue = saved_assets_request;
    Menu_Manual_topMenuState = saved_topic_step;
    Menu_Manual_selectedRow = saved_topic_row;
    std::ranges::copy(saved_requests, std::begin(Content_discRequestSlots));
    Content_discRequestCounts = saved_request_counts;
    *VS::Menu::LiveTextBox(manual::kPageTextBox) = saved_box;
    *VS::Menu::LiveTextBox(kInformationBox) = saved_information_box;
    VS::Render::LiveTexturePages() = saved_pages;
    manual::LiveManualScreenState() = saved_screen;
    Render_frameScratch = saved_frame;
    Render_legacyDrawEnvironments[1] = saved_back_env;
    Render_legacyPrimitiveBuffers[0] = saved_primitives;
    Render_legacyOrderingTables[0] = saved_ordering;
    Render_frameBuf = saved_frame_index;
  }

  // A game heap payload holding `bytes`.
  template <typename T>
  [[nodiscard]] static T* Payload(std::span<const std::uint8_t> bytes) {
    void* const payload =
        VS::Core::Memory::AllocateLiveFromHighEnd(bytes.size());
    REQUIRE(payload != nullptr);
    std::memcpy(payload, bytes.data(), bytes.size());
    return static_cast<T*>(payload);
  }

  // A disc request for the help file that has finished loading.
  [[nodiscard]] static Content_DiscQueueSlot* LoadedRequest() {
    static constexpr Content_DiscFile kFile{.lba = 100, .size = 2048};
    Content_DiscQueueSlot* const request =
        VS::Content::AllocateLiveDiscRequest(&kFile);
    REQUIRE(request != nullptr);
    request->state = Content_DiscRequestState_Loaded;
    return request;
  }

  [[nodiscard]] bool Linked(std::int32_t slot) {
    u_long* const entry =
        ordering.data() + kFrameSlotBase + static_cast<std::size_t>(slot);
    const auto* const next = static_cast<const u_long*>(nextPrim(entry));
    return next >= storage.data() && next < storage.data() + storage.size();
  }
};

[[nodiscard]] bool Freed(const void* payload) {
  return !VS::Core::Memory::LivePayloadCapacity(payload).has_value();
}

// The HUD and field rendering the screen hides and restores.
struct LiveHud {
  int hidden = 0;
  int slide = 0;
  int field_rendering = 0;

  friend bool operator==(const LiveHud&, const LiveHud&) = default;
  friend std::ostream& operator<<(std::ostream& out, const LiveHud& hud) {
    return out << "hidden " << hud.hidden << ", slide " << hud.slide
               << ", field rendering " << hud.field_rendering;
  }
};

LiveHud CurrentHud() {
  return {.hidden = Battle_hudHidden,
          .slide = Battle_hudSlideOffset,
          .field_rendering = VS::Field::isFieldRenderingEnabled};
}

// The help page statics as the typed state.
HelpPagesState CurrentHelpPages() {
  return {.step = Menu_Manual_showMenuState,
          .scroll = Menu_Manual_scrollPosition,
          .line_count = Menu_Manual_lineCount,
          .arrow_frame = Menu_Manual_pageArrowAnimState,
          .load_counter = Menu_Manual_helpPageLoadCounter};
}

// The row fields a menu row animation moves.
bool SameRowMotion(const Menu_Framework_MenuRow& left,
                   const Menu_Framework_MenuRow& right) {
  return left.state == right.state &&
         left.animationStep == right.animationStep && left.x == right.x &&
         left.y == right.y && left.targetPosition0 == right.targetPosition0 &&
         left.targetPosition1 == right.targetPosition1;
}

// Help text of `lines` two-glyph lines, laid out as _helpText (ManualTopics).
std::vector<std::uint8_t> HelpText(int lines) {
  std::vector<std::uint8_t> help(manual::kLineTableByte, 0);
  const auto count = static_cast<std::uint16_t>(lines);
  help.push_back(static_cast<std::uint8_t>(count & 0xFFU));
  help.push_back(static_cast<std::uint8_t>(count >> 8U));
  help.resize(help.size() + (static_cast<std::size_t>(lines) * 2));
  for (int line = 0; line < lines; ++line) {
    const auto entry =
        static_cast<std::uint16_t>((help.size() - manual::kLineTableByte) / 2);
    const std::size_t at =
        manual::kLineTableByte + 2 + (static_cast<std::size_t>(line) * 2);
    help[at] = static_cast<std::uint8_t>(entry & 0xFFU);
    help[at + 1] = static_cast<std::uint8_t>(entry >> 8U);
    help.push_back(static_cast<std::uint8_t>(17 + line));
    help.push_back(static_cast<std::uint8_t>(40 + line));
    help.push_back(manual::kTextTerminator);
    help.push_back(0);
  }
  return help;
}

TEST_CASE("F04159 LiveClearMenuExceptRow8 flies out every row but row 8",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  fixture.rows[7].state = 1;
  fixture.rows[8].state = 1;
  fixture.rows[31].state = 1;
  manual::LiveClearMenuExceptRow8();
  CHECK(fixture.rows[7].state == static_cast<u_char>(fw::RowState::kSlideX));
  CHECK(fixture.rows[31].state == static_cast<u_char>(fw::RowState::kSlideX));
  CHECK(fixture.rows[8].state == 1);
}

TEST_CASE("F04159 LiveClearMenuExceptRow8 keeps the action rows",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  fixture.rows[fw::kActionRowBase].state = 1;
  const std::uint64_t refusals = manual::LiveManualScreenRefusalCount();
  manual::LiveClearMenuExceptRow8();
  CHECK(fixture.rows[fw::kActionRowBase].state == 1);
  CHECK(fixture.rows[20].state == 0);
  CHECK(manual::LiveManualScreenRefusalCount() == refusals);
}

TEST_CASE("F04160 LivePollMenuReady only reads the rows",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  fixture.rows[3].state = static_cast<u_char>(fw::RowState::kSlideX);
  const auto before = fixture.rows;
  const ScreenState screen = manual::LiveManualScreenState();
  manual::LivePollMenuReady();
  CHECK(std::ranges::equal(before, fixture.rows, SameRowMotion));
  CHECK(manual::LiveManualScreenState() == screen);
}

TEST_CASE("F04161 LiveUpdateScreen refuses a null step",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  const std::uint64_t refusals = manual::LiveManualScreenRefusalCount();
  CHECK(manual::LiveUpdateScreen(nullptr) == 0);
  CHECK(manual::LiveManualScreenRefusalCount() == refusals + 1);
  CHECK(manual::LiveManualScreenState() == ScreenState{});
}

TEST_CASE("F04161 LiveUpdateScreen fades in and arms the open delay",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  VS::Menu::Framework::backgroundFadeStep = 0;
  u_char step = manual::kScreenOpen;
  CHECK(manual::LiveUpdateScreen(&step) == 0);
  CHECK(step == manual::kScreenInitialize);
  CHECK(manual::LiveManualScreenState() ==
        ScreenState{.open_delay = 8, .initialized = 0, .help_result = 0});
}

TEST_CASE("F04161 LiveUpdateScreen initializes the help and hides the HUD",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  VS::Input::settings.cursorMemory = 1;
  Menu_Manual_showMenuState = 5;
  Battle_hudHidden = 0;
  VS::Field::isFieldRenderingEnabled = 1;
  u_char step = manual::kScreenInitialize;
  CHECK(manual::LiveUpdateScreen(&step) == 0);
  CHECK(manual::LiveManualScreenState().initialized == 1);
  CHECK(Menu_Manual_showMenuState == manual::kHelpStart);
  CHECK(VS::Render::LiveTexturePages()[0x1D].state ==
        VS::Render::TexturePageState::kOwned);
  CHECK(step == manual::kScreenWaitReady);
  CHECK(CurrentHud() == LiveHud{.hidden = 1,
                                .slide = manual::kHudSlideHidden,
                                .field_rendering = 0});
}

TEST_CASE("F04161 LiveUpdateScreen leaves the help for the game",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  REQUIRE(VS::Render::ReserveLiveTexturePageRange(0x1D, 3, 5) == 0x1D);
  Menu_Manual_showMenuState = manual::kHelpLeaveToGame;
  Battle_hudHidden = 1;
  Battle_hudSlideOffset = manual::kHudSlideHidden;
  VS::Field::isFieldRenderingEnabled = 0;
  u_char step = manual::kScreenShow;
  CHECK(manual::LiveUpdateScreen(&step) == 0);
  CHECK(step == manual::kScreenShutDown);
  CHECK(manual::LiveManualScreenState().help_result == manual::kHelpExitToGame);
  CHECK(VS::Render::LiveTexturePages()[0x1D].state ==
        VS::Render::TexturePageState::kRetiring);
  CHECK(CurrentHud() == LiveHud{.hidden = 0, .slide = 0, .field_rendering = 1});
}

TEST_CASE("F04161 LiveUpdateScreen waits for the fade to reach the game menu",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  manual::LiveManualScreenState().help_result = manual::kHelpExitToGame;
  VS::Menu::Framework::hideMenu = 1;
  VS::Menu::Framework::menuState.currentMenu = 0;
  u_char step = manual::kScreenShutDown;
  CHECK(manual::LiveUpdateScreen(&step) == 0);
  CHECK(step == manual::kScreenExit);
  VS::Menu::Framework::backgroundFadeStep = 2;
  CHECK(manual::LiveUpdateScreen(&step) == 0);
  CHECK(VS::Menu::Framework::hideMenu == 1);
  VS::Menu::Framework::backgroundFadeStep = 0;
  CHECK(manual::LiveUpdateScreen(&step) == 1);
  CHECK(step == manual::kScreenClosed);
  CHECK(VS::Menu::Framework::menuState.currentMenu == manual::kGameMenu);
  CHECK(VS::Menu::Framework::hideMenu == 0);
}

TEST_CASE("F04162 LiveInitHelpState resets the statics and lends the pages",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  VS::Input::settings.cursorMemory = 0;
  Field_manualDisplayState.currentManual = 3;
  Field_manualDisplayState.scrollPositions[3] = 20;
  std::array<int, 2> text{};
  std::array<u_long, 2> assets{};
  Menu_Manual_helpText = text.data();
  Menu_Manual_helpAssets = assets.data();
  Menu_Manual_showMenuState = 5;
  Menu_Manual_scrollPosition = 20;
  Menu_Manual_lineCount = 30;
  Menu_Manual_pageArrowAnimState = 6;
  Menu_Manual_helpPageLoadCounter = 8;
  CHECK(manual::LiveInitHelpState() == 1);
  CHECK(Field_manualDisplayState.currentManual == 0);
  CHECK(Field_manualDisplayState.scrollPositions[3] == 0);
  CHECK(Menu_Manual_helpText == nullptr);
  CHECK(Menu_Manual_helpAssets == nullptr);
  CHECK(CurrentHelpPages() == HelpPagesState{.step = 0,
                                             .scroll = 0,
                                             .line_count = 0,
                                             .arrow_frame = 6,
                                             .load_counter = 8});
  const auto pages = std::span(VS::Render::LiveTexturePages()).subspan(0x1D, 3);
  CHECK(std::ranges::all_of(pages, [](const auto& page) {
    return page.state == VS::Render::TexturePageState::kOwned &&
           page.owner == 5;
  }));
}

TEST_CASE("F04162 LiveInitHelpState keeps the display state with memory on",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  VS::Input::settings.cursorMemory = 1;
  Field_manualDisplayState.currentManual = 3;
  Field_manualDisplayState.scrollPositions[3] = 20;
  CHECK(manual::LiveInitHelpState() == 1);
  CHECK(Field_manualDisplayState.currentManual == 3);
  CHECK(Field_manualDisplayState.scrollPositions[3] == 20);
}

TEST_CASE("F04163 LiveUpdateHelpPages chooses a cancel from the topic rows",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  Menu_Manual_showMenuState = manual::kHelpSelectTopic;
  Menu_Manual_topMenuState = 1;
  const auto [selected, step] = GENERATE(
      std::pair{-2, manual::kHelpLeaveToMenu},
      std::pair{manual::kTopicMenuExitToGame, manual::kHelpLeaveToGame});
  VS::Menu::Framework::selectedRow = selected;
  VS::Menu::Framework::menuMemory.cursorMemories[4] = 4;
  VS::Menu::Framework::menuMemory.cursorMemories[5] = 6;
  CHECK(manual::LiveUpdateHelpPages() == 0);
  CHECK(Menu_Manual_showMenuState == step);
  CHECK(Field_manualDisplayState.currentManual == 10);
  CHECK(Content_discRequestSlots[0].state == Content_DiscRequestState_Free);
}

TEST_CASE("F04163 LiveUpdateHelpPages uploads the loaded assets and frees them",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  const std::array<std::uint8_t, 16> assets{};
  Menu_Manual_helpAssets = LiveManualFixture::Payload<u_long>(assets);
  u_long* const buffer = Menu_Manual_helpAssets;
  Content_DiscQueueSlot* const request = LiveManualFixture::LoadedRequest();
  Menu_Manual_helpAssetsCdQueue = request;
  Menu_Manual_showMenuState = manual::kHelpLoadAssets;
  const std::uint64_t refusals = manual::LiveManualScreenRefusalCount();
  CHECK(manual::LiveUpdateHelpPages() == 0);
  CHECK(Menu_Manual_showMenuState == manual::kHelpLoadText);
  CHECK(Menu_Manual_helpAssets == nullptr);
  CHECK(Menu_Manual_helpAssetsCdQueue == nullptr);
  CHECK(request->state == Content_DiscRequestState_Free);
  CHECK(Freed(buffer));
  CHECK(manual::LiveManualScreenRefusalCount() == refusals);
}

TEST_CASE("F04163 LiveUpdateHelpPages refuses missing requests and uploads",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  Menu_Manual_showMenuState = manual::kHelpLoadAssets;
  const std::uint64_t refusals = manual::LiveManualScreenRefusalCount();
  // A missing request can never load: the topic is abandoned (#408).
  CHECK(manual::LiveUpdateHelpPages() == 0);
  CHECK(Menu_Manual_showMenuState == manual::kHelpListTopics);
  CHECK(manual::LiveManualScreenRefusalCount() == refusals + 2);

  // A header whose tiles run past the buffer: the CLUT upload is refused.
  const std::array<std::uint8_t, 16> assets{1, 0, 0, 0, 1, 0, 0, 0};
  Menu_Manual_helpAssets = LiveManualFixture::Payload<u_long>(assets);
  Menu_Manual_helpAssetsCdQueue = LiveManualFixture::LoadedRequest();
  Menu_Manual_showMenuState = manual::kHelpLoadAssets;
  CHECK(manual::LiveUpdateHelpPages() == 0);
  CHECK(Menu_Manual_showMenuState == manual::kHelpLoadText);
  CHECK(manual::LiveManualScreenRefusalCount() == refusals + 3);
}

TEST_CASE("F04163 LiveUpdateHelpPages opens the loaded text where it was left",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  Menu_Manual_helpText = LiveManualFixture::Payload<int>(HelpText(23));
  Menu_Manual_helpTextCdQueue = LiveManualFixture::LoadedRequest();
  Menu_Manual_showMenuState = manual::kHelpLoadText;
  Menu_Manual_helpPageLoadCounter = 5;
  Field_manualDisplayState.currentManual = 2;
  Field_manualDisplayState.scrollPositions[2] = 10;
  CHECK(manual::LiveUpdateHelpPages() == 0);
  CHECK(Menu_Manual_showMenuState == manual::kHelpShowPage);
  CHECK(Menu_Manual_scrollPosition == 10);
  CHECK(Menu_Manual_lineCount == 20);
  CHECK(Menu_Manual_helpPageLoadCounter == 0);
  CHECK(Menu_Manual_helpTextCdQueue == nullptr);
  CHECK(Content_discRequestSlots[0].state == Content_DiscRequestState_Free);
}

TEST_CASE("F04163 LiveUpdateHelpPages draws a page into the frame",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  Menu_Manual_helpText = LiveManualFixture::Payload<int>(HelpText(23));
  Menu_Manual_showMenuState = manual::kHelpShowPage;
  Menu_Manual_scrollPosition = 10;
  Menu_Manual_lineCount = 20;
  Menu_Manual_pageArrowAnimState = 15;
  Field_manualDisplayState.currentManual = 2;
  VS::Input::buttonRepeat = static_cast<int>(manual::kButtonDown);
  const std::uint64_t refusals = manual::LiveManualScreenRefusalCount();
  CHECK(manual::LiveUpdateHelpPages() == 0);
  CHECK(Menu_Manual_scrollPosition == 11);
  CHECK(Field_manualDisplayState.scrollPositions[2] == 11);
  CHECK(Menu_Manual_pageArrowAnimState == 0);
  CHECK(Menu_Manual_helpPageLoadCounter == 1);
  CHECK(fixture.Linked(manual::kControlsLayer));
  CHECK(fixture.Linked(manual::kPageNumberLayer));
  CHECK(manual::LiveManualScreenRefusalCount() == refusals);
}

TEST_CASE("F04163 LiveUpdateHelpPages refuses drawing without a frame table",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  Menu_Manual_helpText = LiveManualFixture::Payload<int>(HelpText(3));
  Menu_Manual_showMenuState = manual::kHelpShowPage;
  Render_legacyOrderingTables[0] = nullptr;
  const std::uint64_t refusals = manual::LiveManualScreenRefusalCount();
  CHECK(manual::LiveUpdateHelpPages() == 0);
  // Two button icons and five texts.
  CHECK(manual::LiveManualScreenRefusalCount() == refusals + 7);
  CHECK(Menu_Manual_helpPageLoadCounter == 1);
}

TEST_CASE("F04163 LiveUpdateHelpPages leaves the page and frees its text",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  Menu_Manual_helpText = LiveManualFixture::Payload<int>(HelpText(3));
  int* const text = Menu_Manual_helpText;
  Menu_Manual_showMenuState = manual::kHelpShowPage;
  Menu_Manual_helpPageLoadCounter = 3;
  VS::Input::buttonsPressed = manual::kButtonCircle;
  VS::Menu::LiveTextBox(manual::kPageTextBox)->state = 3;
  CHECK(manual::LiveUpdateHelpPages() == 0);
  CHECK(Menu_Manual_showMenuState == manual::kHelpListTopics);
  CHECK(VS::Menu::LiveTextBox(manual::kPageTextBox)->state == 0);
  CHECK(Menu_Manual_helpText == nullptr);
  CHECK(Freed(text));
  CHECK(Menu_Manual_helpPageLoadCounter == 3);
}

TEST_CASE("F04163 LiveUpdateHelpPages frees both buffers and keeps them",
          "[manual-screen-live]") {
  LiveManualFixture fixture;
  REQUIRE(VS::Render::ReserveLiveTexturePageRange(0x1D, 3, 5) == 0x1D);
  const std::array<std::uint8_t, 16> bytes{};
  Menu_Manual_helpText = LiveManualFixture::Payload<int>(bytes);
  Menu_Manual_helpAssets = LiveManualFixture::Payload<u_long>(bytes);
  int* const text = Menu_Manual_helpText;
  u_long* const assets = Menu_Manual_helpAssets;
  Menu_Manual_showMenuState = manual::kHelpLeaveToMenu;
  CHECK(manual::LiveUpdateHelpPages() == manual::kHelpExitToMenu);
  CHECK(Freed(text));
  CHECK(Freed(assets));
  CHECK(Menu_Manual_helpText == text);
  CHECK(Menu_Manual_helpAssets == assets);
  CHECK(VS::Render::LiveTexturePages()[0x1F].state ==
        VS::Render::TexturePageState::kRetiring);
}

// #408: the original waits on a failed help read forever (494.c:308-311 and
// 322-325). Native cancels the topic's outstanding reads, frees both buffers
// and lists the topics again.
TEST_CASE("F04163 UpdateHelpPages returns to the topics when the assets fail",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpLoadAssets};
  host.held = {true, true};
  host.file_states = {manual::kHelpFileFailed, 3};
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"cancel:assets", "release:assets", "free:assets",
                              "forget:assets", "cancel:text", "release:text",
                              "free:text", "forget:text"});
  CHECK(host.refusals ==
        std::vector<std::string>{"Manual help file failed to load"});
  CHECK(state.step == manual::kHelpListTopics);
  CHECK(host.held == std::array<bool, 2>{false, false});

  host.events.clear();
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events == Events{"info", "topics:1"});
  CHECK(state.step == manual::kHelpSelectTopic);
}

TEST_CASE("F04163 UpdateHelpPages returns to the topics when the text fails",
          "[manual-screen]") {
  FakeHelpHost host;
  HelpPagesState state{.step = manual::kHelpLoadText,
                       .scroll = 99,
                       .line_count = 99,
                       .arrow_frame = 5,
                       .load_counter = 6};
  host.held = {false, true};
  host.file_states = {0, manual::kHelpFileFailed};
  CHECK(manual::UpdateHelpPages(host, state) == 0);
  CHECK(host.events ==
        Events{"cancel:text", "release:text", "free:text", "forget:text"});
  CHECK(host.refusals ==
        std::vector<std::string>{"Manual help file failed to load"});
  CHECK(state == HelpPagesState{.step = manual::kHelpListTopics,
                                .scroll = 99,
                                .line_count = 99,
                                .arrow_frame = 5,
                                .load_counter = 6});
  CHECK(host.held == std::array<bool, 2>{false, false});
}

// Extracted data that lists the first topic's help files without holding
// them, and a game heap large enough for both buffers.
struct MissingHelpFiles {
  struct alignas(VS::Core::Memory::kHeapUnitBytes) Arena {
    std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 4096> bytes{};
  };
  VS::Content::DataStore::ScopedOverride store;
  Content_DiscDrive saved_drive = Content_discDriveState;
  std::unique_ptr<Arena> arena = std::make_unique<Arena>();
  VS::Core::Memory::ScopedLiveGameHeap heap{arena->bytes};
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      std::format("vs-missing-help-{}",
                  std::chrono::steady_clock::now().time_since_epoch().count());

  MissingHelpFiles() {
    std::filesystem::create_directories(root / "disc");
    std::ofstream manifest(root / VS::Content::DataStore::ManifestName);
    manifest << "lba\tsize\tform\tpath\n";
    for (const auto file : {HelpFile::kAssets, HelpFile::kText}) {
      const Content_DiscFile& listed =
          Menu_Manual_kHelpFileCdFiles[static_cast<std::size_t>(file)];
      manifest << listed.lba << '\t' << listed.size << "\traw\tSMALL/MISSING"
               << static_cast<int>(file) << ".BIN\n";
    }
    manifest.close();
    REQUIRE(manifest.good());
    REQUIRE(VS::Content::DataStore::Open(root).has_value());
    VS::Content::ResetLiveDiscDrive();
    VS::Content::InitializeLiveDiscQueue();
  }
  MissingHelpFiles(const MissingHelpFiles&) = delete;
  MissingHelpFiles& operator=(const MissingHelpFiles&) = delete;
  MissingHelpFiles(MissingHelpFiles&&) = delete;
  MissingHelpFiles& operator=(MissingHelpFiles&&) = delete;
  ~MissingHelpFiles() noexcept(false) {
    VS::Content::ResetLiveDiscDrive();
    Content_discDriveState = saved_drive;
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
};

// The topic's reads are cancelled, its requests and buffers released, and the
// topic list shown again after one refusal.
void CheckTopicAbandoned(const int* text, const u_long* assets,
                         std::uint64_t refusals) {
  CHECK(Menu_Manual_showMenuState == manual::kHelpListTopics);
  CHECK(manual::LiveManualScreenRefusalCount() == refusals + 1);
  CHECK(Menu_Manual_helpText == nullptr);
  CHECK(Menu_Manual_helpAssets == nullptr);
  CHECK(Menu_Manual_helpTextCdQueue == nullptr);
  CHECK(Menu_Manual_helpAssetsCdQueue == nullptr);
  CHECK(Freed(text));
  CHECK(Freed(assets));
  CHECK(std::ranges::all_of(Content_discRequestSlots, [](const auto& slot) {
    return slot.state == Content_DiscRequestState_Free;
  }));
}

TEST_CASE(
    "F04163 LiveUpdateHelpPages returns to the topics when a help file is "
    "missing",
    "[manual-screen-live]") {
  LiveManualFixture fixture;
  MissingHelpFiles missing;
  Menu_Manual_showMenuState = manual::kHelpSelectTopic;
  Menu_Manual_topMenuState = manual::kTopicStepSelectRow;
  VS::Menu::Framework::selectedRow = 0;
  CHECK(manual::LiveUpdateHelpPages() == 0);
  REQUIRE(Menu_Manual_showMenuState == manual::kHelpLoadAssets);
  int* const text = Menu_Manual_helpText;
  u_long* const assets = Menu_Manual_helpAssets;
  REQUIRE(text != nullptr);
  REQUIRE(assets != nullptr);
  REQUIRE(Menu_Manual_helpTextCdQueue != nullptr);
  REQUIRE(Menu_Manual_helpAssetsCdQueue != nullptr);
  const std::uint64_t refusals = manual::LiveManualScreenRefusalCount();

  // Without the native return, the pages wait in kHelpLoadAssets for good.
  constexpr int kFrameBudget = 120;
  for (int frame = 0; frame < kFrameBudget &&
                      Menu_Manual_showMenuState == manual::kHelpLoadAssets;
       ++frame) {
    VS::Content::UpdateLiveDiscQueue();
    CHECK(manual::LiveUpdateHelpPages() == 0);
  }
  CheckTopicAbandoned(text, assets, refusals);
}
}  // namespace
