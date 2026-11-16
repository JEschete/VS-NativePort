#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Score/GazetteRows.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"

extern "C" {
#include "Battle/StatusEffects.h"
#include "Input/ControllerLiveState.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

// Contracts of the gazette's scrolling rows, top menu and unused menu
// wrappers (MENU9.PRG 238.c:54-267, 1430-1471).
namespace {
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;

constexpr std::uint8_t kEnd = 0xFF;
constexpr std::uint8_t kSpacing = 0xFA;
constexpr std::uint32_t kArrowTexture = 0x37F93000U;
constexpr std::uint32_t kLineOpen = 0xE1000220U;
constexpr std::uint32_t kLineClose = 0xE1000020U;
constexpr std::uint32_t kLineCode = 0x52000000U;
constexpr std::uint32_t kReflectionMode = 0xE1000120U;

constexpr std::uint32_t Xy(std::uint32_t x, std::uint32_t y) {
  return (y << 16) | (x & 0xFFFFU);
}

struct RecordingRowHost final : score::ScrollableRowHost {
  int frame = 1;
  std::vector<score::ScrollArrowSprite> arrows;
  std::vector<std::pair<std::uint32_t, int>> glyphs;
  std::vector<score::GradientLine> lines;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> tiles;
  std::vector<score::ReflectionSprite> reflections;
  std::vector<std::string> refusals;

  int FrameBufferIndex() override { return frame; }
  void DrawScrollArrow(const score::ScrollArrowSprite& arrow) override {
    arrows.push_back(arrow);
  }
  int DrawGlyph(std::uint32_t character, int x) override {
    glyphs.emplace_back(character, x);
    return x + 7;
  }
  std::uint32_t GradientStopColor(std::uint32_t gradient,
                                  std::uint32_t color_index,
                                  int brightness) override {
    return ((gradient & 0xFFU) << 16) | (color_index << 8) |
           static_cast<std::uint32_t>(brightness);
  }
  void AddGradientLine(const score::GradientLine& line) override {
    lines.push_back(line);
  }
  void AddShadowTile(std::uint32_t position, std::uint32_t size) override {
    tiles.emplace_back(position, size);
  }
  void AddReflection(const score::ReflectionSprite& reflection) override {
    reflections.push_back(reflection);
  }
  void Refuse(const char* reason) override { refusals.emplace_back(reason); }
};

fw::MenuRow ScrollRow(int x, int is_scrollable) {
  fw::MenuRow row;
  row.state = fw::RowState::kIdle;
  row.x = static_cast<std::int16_t>(x);
  row.y = 80;
  row.w = 100;
  row.background_width = 3;
  row.gradient_state = 2;
  row.is_scrollable = static_cast<std::uint8_t>(is_scrollable);
  row.text[0] = 10;
  row.text[1] = kSpacing;
  row.text[2] = 5;
  row.text[3] = 11;
  row.text[4] = kEnd;
  return row;
}

TEST_CASE("RenderScrollableRow draws a scrolling-in row with its shadow",
          "[gazette-rows]") {
  RecordingRowHost host;
  score::ScrollableRowAnimation animation{.previous_frame_buffer = 0,
                                          .step = 3};
  fw::MenuRow row = ScrollRow(60, 1);
  score::RenderScrollableRow(host, animation, row);
  CHECK(animation == score::ScrollableRowAnimation{1, 4});
  REQUIRE(host.arrows.size() == 1);
  CHECK(host.arrows[0] ==
        score::ScrollArrowSprite{128, Xy(46, 76), Xy(16, 16), kArrowTexture});
  CHECK(host.glyphs ==
        std::vector<std::pair<std::uint32_t, int>>{{10, 66}, {11, 78}});
  REQUIRE(host.lines.size() == 12);
  CHECK(host.lines[0] == score::GradientLine{kLineOpen, 0x060320U | kLineCode,
                                             Xy(60, 80), 0x020320U, Xy(159, 80),
                                             kLineClose});
  CHECK(host.lines[11][1] == (0x060378U | kLineCode));
}

TEST_CASE("RenderScrollableRow draws a scrolling-in row's tile and reflection",
          "[gazette-rows]") {
  RecordingRowHost host;
  score::ScrollableRowAnimation animation{.previous_frame_buffer = 0,
                                          .step = 3};
  fw::MenuRow row = ScrollRow(60, 1);
  score::RenderScrollableRow(host, animation, row);
  CHECK(host.tiles == std::vector<std::pair<std::uint32_t, std::uint32_t>>{
                          {Xy(62, 92), Xy(100, 2)}});
  CHECK(row.gradient_state == 1);
  REQUIRE(host.reflections.size() == 12);
  CHECK(host.reflections[0] ==
        score::ReflectionSprite{120, Xy(60, 80), Xy(100, 1), kReflectionMode,
                                60U | (80U << 8)});
  CHECK(host.reflections[11].brightness == 32);
}

TEST_CASE("RenderScrollableRow reverses a scrolling-out row past x 128",
          "[gazette-rows]") {
  RecordingRowHost host;
  host.frame = 0;
  score::ScrollableRowAnimation animation;
  fw::MenuRow row = ScrollRow(140, 2);
  row.gradient_state = 0;
  score::RenderScrollableRow(host, animation, row);
  CHECK(host.arrows.empty());
  CHECK(host.glyphs.front() == std::pair<std::uint32_t, int>{0x8000000AU, 146});
  CHECK(host.lines[0][1] == (0x080378U | kLineCode));
  CHECK(host.lines[11][3] == 0x000320U);
  CHECK(host.tiles.empty());
  CHECK(row.gradient_state == 0);
  CHECK(host.reflections[0] ==
        score::ReflectionSprite{32, Xy(140, 80), Xy(100, 1),
                                kReflectionMode | 7U, 12U | (80U << 8)});
  CHECK(host.reflections[11].texture == (12U | (91U << 8)));
}

TEST_CASE("RenderScrollableRow steps the arrow once per frame and wraps at 12",
          "[gazette-rows]") {
  RecordingRowHost host;
  host.frame = 0;
  score::ScrollableRowAnimation animation{.previous_frame_buffer = 1,
                                          .step = 11};
  fw::MenuRow row = ScrollRow(60, 2);
  row.w = 0;
  score::RenderScrollableRow(host, animation, row);
  CHECK(animation == score::ScrollableRowAnimation{0, 0});
  score::RenderScrollableRow(host, animation, row);
  CHECK(animation == score::ScrollableRowAnimation{0, 0});
  REQUIRE(host.arrows.size() == 2);
  CHECK(host.arrows[0].position == Xy(46, 81));
  CHECK(host.arrows[0].texture == (kArrowTexture | 0x10U));
  CHECK(host.lines.empty());
  CHECK(host.reflections.empty());
  CHECK(row.gradient_state == 2);
}

TEST_CASE("RenderScrollableRow keeps the unmasked arrow x of a row below 14",
          "[gazette-rows]") {
  RecordingRowHost host;
  score::ScrollableRowAnimation animation{.previous_frame_buffer = 1,
                                          .step = 0};
  fw::MenuRow row = ScrollRow(5, 1);
  score::RenderScrollableRow(host, animation, row);
  REQUIRE(host.arrows.size() == 1);
  CHECK(host.arrows[0].position == 0xFFFFFFF7U);
  row.state = fw::RowState::kSlideX;
  score::RenderScrollableRow(host, animation, row);
  CHECK(host.arrows.size() == 1);
}

TEST_CASE("RenderScrollableRow refuses text without an end and draws the rest",
          "[gazette-rows]") {
  RecordingRowHost host;
  score::ScrollableRowAnimation animation;
  fw::MenuRow row = ScrollRow(60, 1);
  row.text.fill(12);
  score::RenderScrollableRow(host, animation, row);
  CHECK(host.glyphs.size() == fw::kRowTextBytes);
  CHECK(host.refusals.size() == 1);
  CHECK(host.lines.size() == 12);
  row.text.fill(kSpacing);
  host.glyphs.clear();
  score::RenderScrollableRow(host, animation, row);
  CHECK(host.glyphs.empty());
  CHECK(host.refusals.size() == 2);
}

TEST_CASE("TopMenuCursorOf places short, early, late and middle rows",
          "[gazette-rows]") {
  using Cursor = score::TopMenuCursor;
  CHECK(score::TopMenuCursorOf(4, 3) == Cursor{3, 0});
  CHECK(score::TopMenuCursorOf(8, 20) == Cursor{20, 0});
  CHECK(score::TopMenuCursorOf(20, 7) == Cursor{7, 0});
  CHECK(score::TopMenuCursorOf(20, 12) == Cursor{1, 11});
  CHECK(score::TopMenuCursorOf(20, 19) == Cursor{8, 11});
  CHECK(score::TopMenuCursorOf(20, 11) == Cursor{4, 7});
  CHECK(score::TopMenuCursorOf(300, 299) == Cursor{8, 35});
  CHECK(score::TopMenuCursorOf(4, -3) == Cursor{253, 0});
}

struct RecordingMenuHost final : score::GazetteMenuHost {
  std::uint8_t gazette_row = 0;
  std::uint8_t cursor_row = 0;
  std::uint8_t setting = 0;
  std::int32_t selected = -1;
  std::int32_t idle_result = 0;
  std::vector<std::string> events;

  void ListTopMenuChoices() override { events.emplace_back("list"); }
  std::uint8_t GazetteRow() override { return gazette_row; }
  void SetTopMenuCursor(score::TopMenuCursor cursor) override {
    events.push_back(std::format("cursor {} {}", cursor.row, cursor.first_row));
  }
  std::uint8_t TopMenuCursorRow() override { return cursor_row; }
  std::uint8_t CursorMemorySetting() override { return setting; }
  void SetCursorMemorySetting(std::uint8_t value) override {
    events.push_back(std::format("setting {}", value));
  }
  void ShowRows(std::int32_t row_info,
                std::span<const std::int32_t> row_types) override {
    const bool plain = std::ranges::all_of(
        row_types, [](std::int32_t type) { return type == 0; });
    events.push_back(std::format("rows {:#x} {} {}", row_info, row_types.size(),
                                 plain ? "plain" : "typed"));
  }
  std::int32_t SelectedRow() override { return selected; }
  void ClearMenuExcept(std::int32_t row) override {
    events.push_back(std::format("clear {}", row));
  }
  void FlyOutAndHoist(std::int32_t row, std::int32_t offset) override {
    events.push_back(std::format("hoist {} {}", row, offset));
  }
  std::int32_t MenuRowsIdle() override {
    events.emplace_back("idle");
    return idle_result;
  }
  void Refuse(const char* reason) override {
    events.push_back(std::format("refuse {}", reason));
  }
};

TEST_CASE("RestoreTopMenuCursor forces the cursor memory around the rows",
          "[gazette-rows]") {
  RecordingMenuHost host;
  host.setting = 0;
  score::RestoreTopMenuCursor(host, 20, 15);
  CHECK(host.events == std::vector<std::string>{"cursor 4 11", "setting 1",
                                                "rows 0x101 20 plain",
                                                "setting 0"});
  host.events.clear();
  host.setting = 7;
  score::RestoreTopMenuCursor(host, 0, 0);
  CHECK(host.events == std::vector<std::string>{"cursor 0 0", "setting 1",
                                                "rows 0x101 0 plain",
                                                "setting 7"});
}

TEST_CASE("RestoreTopMenuCursor refuses counts outside 0-256",
          "[gazette-rows]") {
  RecordingMenuHost host;
  score::RestoreTopMenuCursor(host, -1, 0);
  score::RestoreTopMenuCursor(host, score::kMaxTopMenuRows + 1, 0);
  REQUIRE(host.events.size() == 2);
  CHECK(host.events[0].starts_with("refuse"));
  CHECK(host.events[1].starts_with("refuse"));
  host.events.clear();
  score::RestoreTopMenuCursor(host, score::kMaxTopMenuRows, 300);
  CHECK(host.events[0] == "cursor 53 247");
}

TEST_CASE("GetSelectedTopMenuRow clears on cancel and hoists a choice",
          "[gazette-rows]") {
  RecordingMenuHost host;
  host.cursor_row = 200;
  host.selected = -2;
  CHECK(score::GetSelectedTopMenuRow(host) == -2);
  host.selected = -1;
  CHECK(score::GetSelectedTopMenuRow(host) == -1);
  host.selected = 0;
  CHECK(score::GetSelectedTopMenuRow(host) == 0);
  CHECK(host.events == std::vector<std::string>{"clear 5", "hoist 200 1"});
}

TEST_CASE("UpdateTopMenu shows the choices then polls until one is chosen",
          "[gazette-rows]") {
  RecordingMenuHost host;
  host.gazette_row = 2;
  score::TopMenuState state{.step = 1, .selected = 9};
  CHECK(score::UpdateTopMenu(host, state, 1) == score::kTopMenuPending);
  CHECK(state == score::TopMenuState{1, 0});
  CHECK(host.events ==
        std::vector<std::string>{"list", "cursor 2 0", "setting 1",
                                 "rows 0x101 4 plain", "setting 0"});
  host.events.clear();
  CHECK(score::UpdateTopMenu(host, state, 0) == score::kTopMenuPending);
  CHECK(state == score::TopMenuState{1, -1});
  host.selected = 3;
  host.cursor_row = 3;
  CHECK(score::UpdateTopMenu(host, state, 0) == 3);
  CHECK(state == score::TopMenuState{1, 3});
  CHECK(host.events == std::vector<std::string>{"hoist 3 1"});
}

TEST_CASE("UpdateTopMenu stays pending past its steps and returns a cancel",
          "[gazette-rows]") {
  RecordingMenuHost host;
  score::TopMenuState state{.step = 2, .selected = 4};
  CHECK(score::UpdateTopMenu(host, state, 0) == score::kTopMenuPending);
  CHECK(host.events.empty());
  CHECK(state == score::TopMenuState{2, 4});
  state.step = 1;
  host.selected = -2;
  CHECK(score::UpdateTopMenu(host, state, 0) == -2);
  CHECK(host.events == std::vector<std::string>{"clear 5"});
}

TEST_CASE("ClearMenuExceptRow8 and PollMenuReady reach their menu helpers",
          "[gazette-rows]") {
  RecordingMenuHost host;
  score::ClearMenuExceptRow8(host);
  score::PollMenuReady(host);
  CHECK(host.events == std::vector<std::string>{"clear 8", "idle"});
}

TEST_CASE("GetSelectedTopMenuRow hoists the cursor row, not the chosen row",
          "[gazette-rows]") {
  RecordingMenuHost host;
  host.cursor_row = 1;
  host.selected = 3;
  CHECK(score::GetSelectedTopMenuRow(host) == 3);
  host.selected = -3;
  CHECK(score::GetSelectedTopMenuRow(host) == -3);
  CHECK(host.events == std::vector<std::string>{"hoist 1 1", "clear 5"});
}

TEST_CASE("PollMenuReady polls once and drops a busy or idle answer",
          "[gazette-rows]") {
  RecordingMenuHost host;
  host.idle_result = GENERATE(0, 1);
  score::PollMenuReady(host);
  CHECK(host.events == std::vector<std::string>{"idle"});
}

// The frame's packet buffer and overlay ordering table.
struct LivePacketsFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  void* saved_table = Render_legacyOrderingTables[0];
  int saved_framebuffer = Render_frameBuf;
  score::ScrollableRowAnimation saved_animation =
      score::LiveScrollableRowAnimation();
  std::vector<u_long> storage =
      std::vector<u_long>((0x20000 / sizeof(u_long)) + 2, 0xDEADBEEF);
  std::array<u_long, 8> ordering{};

  LivePacketsFixture() {
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data() + 1;
    Render_legacyOrderingTables[0] = ordering.data();
    Render_frameScratch.overlayOrderingTable = ordering.data();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_frameScratch.primitiveCursor = storage.data() + 1;
  }
  ~LivePacketsFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_legacyOrderingTables[0] = saved_table;
    Render_frameBuf = saved_framebuffer;
    score::LiveScrollableRowAnimation() = saved_animation;
  }
  LivePacketsFixture(const LivePacketsFixture&) = delete;
  LivePacketsFixture& operator=(const LivePacketsFixture&) = delete;

  // The packets linked before ordering entry `entry`, nearest first.
  [[nodiscard]] std::vector<const u_long*> PacketsBefore(std::size_t entry) {
    std::vector<const u_long*> packets;
    const void* packet = nextPrim(&ordering[entry]);
    const auto begin = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto end = begin + (storage.size() * sizeof(u_long));
    while (reinterpret_cast<std::uintptr_t>(packet) >= begin &&
           reinterpret_cast<std::uintptr_t>(packet) < end &&
           packets.size() < 64) {
      packets.push_back(static_cast<const u_long*>(packet));
      packet = nextPrim(packet);
    }
    return packets;
  }
};

TEST_CASE("RenderLiveScrollableRow links the row's packets and its arrow",
          "[gazette-rows-live]") {
  LivePacketsFixture fixture;
  score::ResetGazetteRowsLive();
  Menu_Framework_MenuRow row{};
  row.state = 1;
  row.x = 60;
  row.y = 80;
  row.w = 100;
  row.isScrollable = 1;
  row.gradientState = 3;
  row.text[0] = static_cast<char>(kEnd);
  const std::uint64_t refused = score::LiveGazetteRowsRefusals();
  score::RenderLiveScrollableRow(&row);
  const auto packets = fixture.PacketsBefore(2);
  CHECK(packets.size() == 25);
  CHECK(packets.back()[1] == kLineOpen);
  const auto arrow = fixture.PacketsBefore(1);
  REQUIRE(arrow.size() == 1);
  CHECK(arrow[0][4] == kArrowTexture);
  CHECK(row.gradientState == 2);
  CHECK(score::LiveScrollableRowAnimation() ==
        score::ScrollableRowAnimation{0, 0});
  CHECK(score::LiveGazetteRowsRefusals() == refused);
  CHECK(fixture.storage.front() == 0xDEADBEEF);
}

TEST_CASE("RenderLiveScrollableRow refuses a null row or ordering table",
          "[gazette-rows-live]") {
  LivePacketsFixture fixture;
  Menu_Framework_MenuRow row{};
  row.w = 10;
  row.text[0] = static_cast<char>(kEnd);
  const std::uint64_t refused = score::LiveGazetteRowsRefusals();
  score::RenderLiveScrollableRow(nullptr);
  Render_frameScratch.overlayOrderingTable = nullptr;
  score::RenderLiveScrollableRow(&row);
  CHECK(score::LiveGazetteRowsRefusals() == refused + 2);
  CHECK(Render_frameScratch.primitiveCursor == fixture.storage.data() + 1);
}

// The menu rows, cursor memory, settings, gazette row and top menu strings,
// with a game heap for the rows' text.
struct LiveMenuFixture {
  Menu_Framework_MenuRow* saved_rows = Menu_Framework_menuItems;
  Menu_Framework_MenuMemory saved_memory = Menu_Framework_menuMemory;
  State_Settings saved_settings = Input_settings;
  Menu_Score_CursorMemory saved_cursor = Battle_unusedMenu9CursorMemory;
  std::array<u_short, std::size(Menu_Score_menuText)> saved_text{};
  score::TopMenuState saved_top_menu = score::LiveTopMenuState();
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 256> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<Menu_Framework_MenuRow, 40> rows{};
  std::array<std::array<char, 8>, 4> names{};
  std::array<char*, 8> text{};

  LiveMenuFixture() {
    std::copy_n(Menu_Score_menuText, saved_text.size(), saved_text.begin());
    Menu_Framework_menuItems = rows.data();
    Input_settings.cursorMemory = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
      names[i][0] = static_cast<char>(10 + i);
      names[i][1] = static_cast<char>(0xE7);
      text[i * 2] = names[i].data();
      // Each choice: an offset to halfword 4 + 2i, then [10 + i, 0xE7].
      Menu_Score_menuText[i] = static_cast<u_short>(4 + (2 * i));
      Menu_Score_menuText[4 + (2 * i)] =
          static_cast<u_short>(0xE700 | (10 + i));
    }
    score::ResetGazetteRowsLive();
  }
  ~LiveMenuFixture() {
    if (fw::LiveSelectedRow() == -1) fw::CloseLiveRowsAndRememberCursor();
    std::ranges::copy(saved_text, Menu_Score_menuText);
    Menu_Framework_menuItems = saved_rows;
    Menu_Framework_menuMemory = saved_memory;
    Input_settings = saved_settings;
    Battle_unusedMenu9CursorMemory = saved_cursor;
    score::LiveTopMenuState() = saved_top_menu;
  }
  LiveMenuFixture(const LiveMenuFixture&) = delete;
  LiveMenuFixture& operator=(const LiveMenuFixture&) = delete;
};

TEST_CASE("RestoreLiveTopMenuCursor recalls the row and restores the setting",
          "[gazette-rows-live]") {
  LiveMenuFixture fixture;
  score::RestoreLiveTopMenuCursor(4, 2, fixture.text.data());
  CHECK(Menu_Framework_menuMemory.cursorMemories[2] == 2);
  CHECK(Menu_Framework_menuMemory.cursorMemories[3] == 0);
  CHECK(Input_settings.cursorMemory == 0);
  CHECK(fw::LiveSelectedRow() == -1);
  CHECK(fw::LiveHighlightedRow() == 2);
}

TEST_CASE("RestoreLiveTopMenuCursor refuses missing text and bad counts",
          "[gazette-rows-live]") {
  LiveMenuFixture fixture;
  Menu_Framework_menuMemory.cursorMemories[2] = 6;
  const std::uint64_t refused = score::LiveGazetteRowsRefusals();
  score::RestoreLiveTopMenuCursor(2, 1, nullptr);
  score::RestoreLiveTopMenuCursor(-1, 1, fixture.text.data());
  CHECK(score::LiveGazetteRowsRefusals() == refused + 2);
  CHECK(Menu_Framework_menuMemory.cursorMemories[2] == 6);
  CHECK(fixture.rows[10].state == 0);
}

TEST_CASE("UpdateLiveTopMenu shows the choices and returns the chosen row",
          "[gazette-rows-live]") {
  LiveMenuFixture fixture;
  Battle_unusedMenu9CursorMemory.gazetteRow = 3;
  CHECK(score::UpdateLiveTopMenu(1) == score::kTopMenuPending);
  CHECK(score::LiveTopMenuState() == score::TopMenuState{1, 0});
  CHECK(Menu_Framework_menuMemory.cursorMemories[2] == 3);
  CHECK(fw::LiveHighlightedRow() == 3);
  CHECK(fw::LiveSelectedRow() == -1);
  CHECK(score::UpdateLiveTopMenu(0) == score::kTopMenuPending);
  fw::CloseLiveRowsAndRememberCursor();
  CHECK(score::UpdateLiveTopMenu(0) == 3);
  CHECK(fixture.rows[13].state == static_cast<u_char>(fw::RowState::kSlideXy));
  CHECK(score::LiveTopMenuState().selected == 3);
}

TEST_CASE("LiveSelectedTopMenuRow hoists the remembered cursor row",
          "[gazette-rows-live]") {
  LiveMenuFixture fixture;
  score::RestoreLiveTopMenuCursor(4, 1, fixture.text.data());
  CHECK(score::LiveSelectedTopMenuRow() == -1);
  fw::CloseLiveRowsAndRememberCursor();
  CHECK(score::LiveSelectedTopMenuRow() == 1);
  CHECK(fixture.rows[11].state == static_cast<u_char>(fw::RowState::kSlideXy));
  CHECK(fixture.rows[11].targetPosition0 == 180);
}

TEST_CASE("ClearLiveMenuExceptRow8 flies out every row but row 8",
          "[gazette-rows-live]") {
  LiveMenuFixture fixture;
  fixture.rows[7].state = 1;
  fixture.rows[8].state = 1;
  score::ClearLiveMenuExceptRow8();
  CHECK(fixture.rows[7].state == static_cast<u_char>(fw::RowState::kSlideX));
  CHECK(fixture.rows[7].targetPosition0 == 320);
  CHECK(fixture.rows[8].state == 1);
}

TEST_CASE("ClearLiveMenuExceptRow8 flies out rows 0-31 but not action rows",
          "[gazette-rows-live]") {
  LiveMenuFixture fixture;
  fixture.rows[0].state = 1;
  fixture.rows[31].state = 1;
  fixture.rows[fw::kActionRowBase].state = 1;
  score::ClearLiveMenuExceptRow8();
  CHECK(fixture.rows[0].state == static_cast<u_char>(fw::RowState::kSlideX));
  CHECK(fixture.rows[31].state == static_cast<u_char>(fw::RowState::kSlideX));
  CHECK(fixture.rows[fw::kActionRowBase].state == 1);
  CHECK(fixture.rows[20].state == 0);
}

TEST_CASE("PollLiveMenuReady only reads the rows", "[gazette-rows-live]") {
  LiveMenuFixture fixture;
  fixture.rows[3].state = static_cast<u_char>(fw::RowState::kSlideX);
  const auto before = fixture.rows;
  const std::uint64_t refused = score::LiveGazetteRowsRefusals();
  score::PollLiveMenuReady();
  CHECK(std::ranges::equal(std::as_bytes(std::span(before)),
                           std::as_bytes(std::span(fixture.rows))));
  CHECK(score::LiveGazetteRowsRefusals() == refused);
}
}  // namespace
