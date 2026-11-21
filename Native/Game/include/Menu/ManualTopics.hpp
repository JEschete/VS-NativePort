#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

// The manual's topic menu and page text (MENUE.PRG 494.c:76-118, 450-475,
// 514-575): the fourteen topic rows with their remembered cursor, the
// selection poll, and the ten-line pages composed from a help text file for
// the text box. Topic strings and help lines are byte offsets into their
// tables (import metadata); the cores never form native pointers, and the
// live binding maps them onto the loaded module data (ManualTopicsLive.cpp).
namespace VS::Menu::manual {
// --- Topic menu --------------------------------------------------------------

// The topics, each a name and a description string.
inline constexpr std::int32_t kTopicCount = 14;
inline constexpr std::size_t kTopicStrings = std::size_t{kTopicCount} * 2;
// The topic strings' byte offsets, name then description per topic.
using TopicTextOffsetArray = std::array<std::uint32_t, kTopicStrings>;
// vs_mainmenu_setMenuRows' row info word for the manual's list.
inline constexpr std::int32_t kTopicRowInfo = 0x102;
// The row type bit that marks the last row.
inline constexpr std::int32_t kLastRowType = 4;
// The list shows nine rows; past eight a highlighted row scrolls the list.
inline constexpr std::int32_t kVisibleTopicRows = 9;
// The screen row a highlighted topic in the middle of a long list sits on.
inline constexpr std::int32_t kMiddleTopicRow = 4;
// vs_mainMenu_clearMenuExcept's kept row when the menu is left.
inline constexpr std::int32_t kTopicClearKeptRow = 5;
// GetSelectedTopicRow: no choice yet this frame.
inline constexpr std::int32_t kNoTopicSelected = -1;

// The remembered list cursor (D_800F4EE8.cursorMemories[4] and [5]): the
// highlighted screen row and the first listed row, stored as bytes.
struct TopicCursor {
  std::uint8_t row = 0;
  std::uint8_t scroll = 0;

  friend bool operator==(const TopicCursor&, const TopicCursor&) = default;
};

class TopicMenuHost {
 public:
  virtual ~TopicMenuHost() = default;
  virtual void SetTopicCursor(TopicCursor cursor) = 0;
  [[nodiscard]] virtual std::uint8_t TopicCursorRow() const = 0;
  // vs_main_settings.cursorMemory.
  [[nodiscard]] virtual std::uint8_t CursorMemorySetting() const = 0;
  virtual void SetCursorMemorySetting(std::uint8_t value) = 0;
  // vs_mainmenu_setMenuRows: `text_offsets` holds each row's name and
  // description as byte offsets into the topic string table.
  virtual void CreateRows(std::int32_t row_count, std::int32_t row_info,
                          std::span<const std::uint32_t> text_offsets,
                          std::span<const std::int32_t> row_types) = 0;
  // vs_mainmenu_getSelectedRow: a row, -1 while undecided, below -1 when the
  // menu is cancelled.
  [[nodiscard]] virtual std::int32_t SelectedRow() = 0;
  // vs_mainMenu_clearMenuExcept.
  virtual void ClearMenuExcept(std::int32_t row) = 0;
  // vs_mainMenu_flyoutMenuRightAndHoistSelection.
  virtual void FlyOutAndHoistSelection(std::int32_t row,
                                       std::int32_t offset) = 0;
};

enum class TopicError : std::uint8_t {
  // A list needs at least one row.
  kNoRows,
  // Fewer than two strings per row.
  kTooFewStrings,
  // The table is too short for the topics' string offsets.
  kShortTopicTable,
  // A topic string starts past the table.
  kTopicTextOutsideTable,
  // A topic string has no terminator before the table ends.
  kUnterminatedTopicText,
};

// decomp: MENUE.PRG setMenuRows (src/MENU/MENUE.PRG/494.c:82; was
// _setMenuRows), the cursor placement. Short lists and rows above the eighth
// keep the list at its top; rows in the last eight show the list's last nine
// rows; other rows sit on screen row 4.
[[nodiscard]] TopicCursor TopicCursorFor(std::int32_t row_count,
                                         std::int32_t highlighted_row) noexcept;

// decomp: MENUE.PRG setMenuRows (src/MENU/MENUE.PRG/494.c:82; was
// _setMenuRows). Lists `row_count` rows, the last flagged kLastRowType, with
// `highlighted_row` remembered and the cursor memory setting forced on while
// the rows are created.
std::expected<void, TopicError> SetTopicRows(
    TopicMenuHost& host, std::int32_t row_count, std::int32_t highlighted_row,
    std::span<const std::uint32_t> text_offsets);

// decomp: MENUE.PRG getSelectedRow (src/MENU/MENUE.PRG/494.c:110; was
// _getSelectedRow). A cancel (below -1) clears the menu but row 5; a chosen row
// flies the rows out and hoists the remembered screen row.
std::int32_t GetSelectedTopicRow(TopicMenuHost& host);

// The topic strings' byte offsets, name then description per topic, from the
// halfword offsets at the start of the topic string table (_menuText).
// Every string must start in the table and end with a terminator before it.
[[nodiscard]] std::expected<TopicTextOffsetArray, TopicError> TopicTextOffsets(
    std::span<const std::uint16_t> topic_table);

// _topMenu_state and _selectedRow.
struct TopicMenuState {
  std::int32_t step = 0;
  std::int32_t selected_row = 0;
};
inline constexpr std::int32_t kTopicStepListRows = 0;
inline constexpr std::int32_t kTopicStepSelectRow = 1;

// decomp: MENUE.PRG topMenu (src/MENU/MENUE.PRG/494.c:454; was _topMenu).
// `reset` restarts at listing the rows; the listing step returns -1 and moves
// on, the selection step returns the chosen or cancelled row (or -1 while
// undecided), and any other step returns -1. A topic table that cannot be
// listed is refused before any row is created, leaving the listing step to
// retry.
std::expected<std::int32_t, TopicError> UpdateTopicMenu(
    TopicMenuState& state, bool reset,
    std::span<const std::uint16_t> topic_table, std::int32_t current_topic,
    TopicMenuHost& host);

// --- Page text ---------------------------------------------------------------

inline constexpr std::int32_t kLinesPerPage = 10;
// The help text file's line table: a halfword line count at byte 16, then
// one halfword per line, each the line's offset in halfwords from byte 16.
inline constexpr std::size_t kLineTableByte = 16;
inline constexpr std::size_t kPageTextCapacity = 1024;
// The page's header: chunk size 0 (no chunking), then font table 0.
inline constexpr std::size_t kPageHeaderBytes = 4;

// Text codes (vs_string.h): two-byte codes start at kTextControl.
inline constexpr std::uint8_t kTextTerminator = 0xE7;
inline constexpr std::uint8_t kTextNewline = 0xE8;
inline constexpr std::uint8_t kTextControl = 0xEC;
inline constexpr std::uint8_t kTextFontTable = 0xFB;
inline constexpr std::uint8_t kTextChunkSize = 0xF8;
inline constexpr std::uint8_t kTextNoChunking = 0x00;
inline constexpr std::uint8_t kTextFontTableZero = 0x06;

enum class PageTextError : std::uint8_t {
  // Pages start at line 0 or later.
  kNegativeScroll,
  // The file ends before its line count.
  kShortHelpText,
  // A line's table entry or its first byte lies past the file.
  kLineOutsideText,
  // A line (or a two-byte code) runs past the file before its terminator.
  kUnterminatedLine,
  // The composed text does not fit its destination.
  kPageOverflow,
};

// The help text file's line count (the halfword at kLineTableByte).
[[nodiscard]] std::expected<std::uint16_t, PageTextError> HelpLineCount(
    std::span<const std::uint8_t> help_text);

// decomp: MENUE.PRG getRoundedLineCount (src/MENU/MENUE.PRG/494.c:518; was
// _getRoundedLineCount). The first line of the last ten-line page: the line
// count rounded up to a whole page, less a page, and never negative.
[[nodiscard]] std::int32_t LastPageLineOffset(
    std::uint16_t line_count) noexcept;

// decomp: MENUE.PRG vsStrcpy (src/MENU/MENUE.PRG/494.c:565; was _vsStrcpy).
// Copies an encoded line up to its terminator, copying a two-byte code's second
// byte whatever it is, then ends it with a newline. Returns the bytes written,
// newline included.
std::expected<std::size_t, PageTextError> CopyEncodedLine(
    std::span<const std::uint8_t> source, std::span<std::uint8_t> destination);

// decomp: MENUE.PRG copyNextPageText (src/MENU/MENUE.PRG/494.c:535; was
// _copyNextPageText). Composes the page from line `scroll_position` (up to ten
// lines, to the last line): the header, each line ended by a newline, the last
// newline (or, for an empty page, the font table 0 code) replaced by the
// terminator. Byte 4 is first set to a newline as in the original. Returns the
// composed length; on refusal the page holds a partial composition.
std::expected<std::size_t, PageTextError> ComposePageText(
    std::span<const std::uint8_t> help_text, std::int32_t scroll_position,
    std::span<std::uint8_t, kPageTextCapacity> page);
}  // namespace VS::Menu::manual
