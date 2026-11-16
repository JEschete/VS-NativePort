#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENU9.PRG score screen (gazette): the scrolling list row renderer the
// records and encyclopedia lists draw their rows with, the top menu's rows,
// cursor and choice, and two unused menu wrappers (238.c:54-267 and
// 1430-1471; F02799, F02800, F02801, F02806, F04154, F04155). The row
// renderer's arrow animation and the top menu's step are their own state,
// separate from the menu rows and the shared cursor memory, which the rules
// reach through hosts; GazetteRowsLive.hpp binds them to the copied globals.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// _topMenu's choices: the first four _menuText strings.
inline constexpr std::int32_t kTopMenuChoiceCount = 4;
// More rows than this scroll (_setRows).
inline constexpr std::int32_t kTopMenuVisibleRows = 8;
// The cursor row _setRows keeps for a row in the middle of a long list.
inline constexpr std::int32_t kTopMenuMiddleRow = 4;
// _setRows' menuRowInfo(1, 0, 1).
inline constexpr std::int32_t kTopMenuRowInfo = 0x101;
// The row _getSelectedRow keeps when the menu is cancelled.
inline constexpr std::int32_t kTopMenuKeptRow = 5;
// The row _clearMenuExceptRow8 keeps.
inline constexpr std::int32_t kUnusedKeptRow = 8;
// _topMenu's result while the player is choosing.
inline constexpr std::int32_t kTopMenuPending = -1;
// Native bound: _setRows' row types are a stack array of the row count.
inline constexpr std::int32_t kMaxTopMenuRows = 256;

// The row arrow bobs through this many frames (four per phase).
inline constexpr std::uint8_t kScrollArrowSteps = 12;
// The row's 12 background lines and their 12 reflection lines.
inline constexpr int kRowBackgroundLines = 12;

// _renderScrollableMenuItem's statics: the frame buffer seen last (a byte)
// and the arrow's animation step.
struct ScrollableRowAnimation {
  std::uint8_t previous_frame_buffer = 0;
  std::uint8_t step = 0;

  friend bool operator==(const ScrollableRowAnimation&,
                         const ScrollableRowAnimation&) = default;
};

// The scroll arrow: a textured 16x16 sprite with its texture page.
struct ScrollArrowSprite {
  int brightness = 0;
  std::uint32_t position = 0;
  std::uint32_t size = 0;
  // The sprite's UV/CLUT word (packet word 4).
  std::uint32_t texture = 0;

  friend bool operator==(const ScrollArrowSprite&,
                         const ScrollArrowSprite&) = default;
};

// A background line's packet words after its tag: the opening draw mode,
// the first colour and code, the start, the second colour, the end and the
// closing draw mode.
inline constexpr std::size_t kGradientLineWords = 6;
using GradientLine = std::array<std::uint32_t, kGradientLineWords>;

// One reflection line: a sprite whose draw mode (word 1) and UV (word 4) the
// renderer overwrites.
struct ReflectionSprite {
  int brightness = 0;
  std::uint32_t position = 0;
  std::uint32_t size = 0;
  std::uint32_t draw_mode = 0;
  std::uint32_t texture = 0;

  friend bool operator==(const ReflectionSprite&,
                         const ReflectionSprite&) = default;
};

class ScrollableRowHost {
 public:
  ScrollableRowHost() = default;
  ScrollableRowHost(const ScrollableRowHost&) = delete;
  ScrollableRowHost& operator=(const ScrollableRowHost&) = delete;
  ScrollableRowHost(ScrollableRowHost&&) = delete;
  ScrollableRowHost& operator=(ScrollableRowHost&&) = delete;
  virtual ~ScrollableRowHost() = default;

  // vs_main_frameBuf.
  [[nodiscard]] virtual int FrameBufferIndex() = 0;
  // vs_main_setSpriteDefaultTexPage before the row's ordering entry.
  virtual void DrawScrollArrow(const ScrollArrowSprite& arrow) = 0;
  // vs_mainMenu_renderRowLabel: the pen after `character`.
  [[nodiscard]] virtual int DrawGlyph(std::uint32_t character, int x) = 0;
  // vs_battle_uiGradientStop.
  [[nodiscard]] virtual std::uint32_t GradientStopColor(
      std::uint32_t gradient, std::uint32_t color_index, int brightness) = 0;
  // A LINE_G2 packet (seven words, tag linked by the host).
  virtual void AddGradientLine(const GradientLine& line) = 0;
  // vs_battle_addTile: the dark shadow tile under a scrolling-in row.
  virtual void AddShadowTile(std::uint32_t position, std::uint32_t size) = 0;
  // vs_main_setSprite with its draw mode and UV replaced.
  virtual void AddReflection(const ReflectionSprite& reflection) = 0;
  virtual void Refuse(const char* reason) = 0;
};

// _renderScrollableMenuItem: advances the arrow animation once per frame
// buffer flip, draws the bobbing scroll arrow for an idle row left of x 128,
// the row's glyphs, and for a row with a width its 12 gradient lines, the
// shadow tile (for is_scrollable 1) and 12 reflection lines, stepping the
// row's gradient fade. Text without an end is refused and skips the glyphs
// (the original reads past the text).
void RenderScrollableRow(ScrollableRowHost& host,
                         ScrollableRowAnimation& animation,
                         Framework::MenuRow& row);

// D_800F4EE8.cursorMemories[2] and [3]: the cursor row and the first shown
// row, each stored as a byte.
struct TopMenuCursor {
  std::uint8_t row = 0;
  std::uint8_t first_row = 0;

  friend bool operator==(const TopMenuCursor&, const TopMenuCursor&) = default;
};

class GazetteMenuHost {
 public:
  GazetteMenuHost() = default;
  GazetteMenuHost(const GazetteMenuHost&) = delete;
  GazetteMenuHost& operator=(const GazetteMenuHost&) = delete;
  GazetteMenuHost(GazetteMenuHost&&) = delete;
  GazetteMenuHost& operator=(GazetteMenuHost&&) = delete;
  virtual ~GazetteMenuHost() = default;

  // _menuText's first kTopMenuChoiceCount strings become the listed text
  // (only UpdateTopMenu lists them).
  virtual void ListTopMenuChoices() = 0;
  // vs_battle_menu9CursorMemory.gazetteRow.
  [[nodiscard]] virtual std::uint8_t GazetteRow() = 0;
  virtual void SetTopMenuCursor(TopMenuCursor cursor) = 0;
  // D_800F4EE8.cursorMemories[2].
  [[nodiscard]] virtual std::uint8_t TopMenuCursorRow() = 0;
  // vs_main_settings.cursorMemory.
  [[nodiscard]] virtual std::uint8_t CursorMemorySetting() = 0;
  virtual void SetCursorMemorySetting(std::uint8_t setting) = 0;
  // vs_mainmenu_setMenuRows over the listed text, one type per row.
  virtual void ShowRows(std::int32_t row_info,
                        std::span<const std::int32_t> row_types) = 0;
  // vs_mainmenu_getSelectedRow.
  [[nodiscard]] virtual std::int32_t SelectedRow() = 0;
  // vs_mainMenu_clearMenuExcept.
  virtual void ClearMenuExcept(std::int32_t row) = 0;
  // vs_mainMenu_flyoutMenuRightAndHoistSelection.
  virtual void FlyOutAndHoist(std::int32_t row, std::int32_t offset) = 0;
  // vs_mainmenu_ready.
  virtual std::int32_t MenuRowsIdle() = 0;
  virtual void Refuse(const char* reason) = 0;
};

// The cursor _setRows stores for `row_count` rows with `absolute_row`
// chosen: the row itself for a short list or an early row, the row's place
// on the last page near the end, and kTopMenuMiddleRow otherwise.
[[nodiscard]] TopMenuCursor TopMenuCursorOf(std::int32_t row_count,
                                            std::int32_t absolute_row);

// _setRows: places the cursor and shows `row_count` plain rows of the listed
// text with the cursor memory forced on and then restored. A count below 0
// or above kMaxTopMenuRows is refused (the original sizes a stack array with
// it).
void RestoreTopMenuCursor(GazetteMenuHost& host, std::int32_t row_count,
                          std::int32_t absolute_row);

// _getSelectedRow: the selected row (-1 while choosing). Below -1
// (cancelled) clears the menu but kTopMenuKeptRow; a chosen row flies out
// and hoists the cursor's row.
[[nodiscard]] std::int32_t GetSelectedTopMenuRow(GazetteMenuHost& host);

// _topMenu's statics.
struct TopMenuState {
  std::int32_t step = 0;
  std::int32_t selected = 0;

  friend bool operator==(const TopMenuState&, const TopMenuState&) = default;
};

// _topMenu: `initialize` restarts it. The first step shows the four choices
// with the gazette's remembered row; the next polls the choice.
// kTopMenuPending until a row is chosen or the menu is cancelled (below -1);
// any other step stays pending.
[[nodiscard]] std::int32_t UpdateTopMenu(GazetteMenuHost& host,
                                         TopMenuState& state,
                                         std::int32_t initialize);

// _clearMenuExceptRow8 (unused): clears the menu but row kUnusedKeptRow.
void ClearMenuExceptRow8(GazetteMenuHost& host);

// _menuReady (unused): polls whether the menu rows are idle and drops the
// answer.
void PollMenuReady(GazetteMenuHost& host);
}  // namespace VS::Menu::Score
