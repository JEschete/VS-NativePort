#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/HeapLive.hpp"
#include "Field/Manual.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Manual.hpp"
#include "Menu/ManualTopics.hpp"
#include "Menu/ManualTopicsLive.hpp"
#include "Menu/TextBoxStateLive.hpp"

// Contracts of the manual's topic menu and page text (MENUE.PRG 494.c:76-118,
// 450-475, 514-575): the topic rows and their remembered cursor, the
// selection poll, and the ten-line help pages handed to text box 0.
namespace {
namespace manual = VS::Menu::manual;
using manual::PageTextError;
using manual::TopicCursor;
using manual::TopicError;

class FakeTopicHost final : public manual::TopicMenuHost {
 public:
  std::vector<TopicCursor> cursors;
  std::uint8_t cursor_row = 0;
  std::uint8_t setting = 7;
  std::uint8_t setting_while_creating = 0;
  int creations = 0;
  std::int32_t created_count = 0;
  std::int32_t created_info = 0;
  std::vector<std::uint32_t> created_offsets;
  std::vector<std::int32_t> created_types;
  std::int32_t next_row = -1;
  std::vector<std::int32_t> cleared;
  std::vector<std::array<std::int32_t, 2>> fly_outs;

  void SetTopicCursor(TopicCursor cursor) override {
    cursors.push_back(cursor);
    cursor_row = cursor.row;
  }
  [[nodiscard]] std::uint8_t TopicCursorRow() const override {
    return cursor_row;
  }
  [[nodiscard]] std::uint8_t CursorMemorySetting() const override {
    return setting;
  }
  void SetCursorMemorySetting(std::uint8_t value) override { setting = value; }
  void CreateRows(std::int32_t row_count, std::int32_t row_info,
                  std::span<const std::uint32_t> text_offsets,
                  std::span<const std::int32_t> row_types) override {
    ++creations;
    setting_while_creating = setting;
    created_count = row_count;
    created_info = row_info;
    created_offsets.assign(text_offsets.begin(), text_offsets.end());
    created_types.assign(row_types.begin(), row_types.end());
  }
  [[nodiscard]] std::int32_t SelectedRow() override { return next_row; }
  void ClearMenuExcept(std::int32_t row) override { cleared.push_back(row); }
  void FlyOutAndHoistSelection(std::int32_t row, std::int32_t offset) override {
    fly_outs.push_back({row, offset});
  }
};

// A topic table whose 28 strings each fill one halfword: a glyph and the
// terminator, starting right after the offsets.
std::vector<std::uint16_t> TopicTable() {
  std::vector<std::uint16_t> table(64, 0);
  for (std::uint16_t index = 0; index < manual::kTopicStrings; ++index) {
    table[index] = static_cast<std::uint16_t>(manual::kTopicStrings + index);
    table[manual::kTopicStrings + index] =
        static_cast<std::uint16_t>(0xE700U | (0x10U + index));
  }
  return table;
}

std::vector<std::uint32_t> ExpectedTopicOffsets() {
  std::vector<std::uint32_t> offsets;
  for (std::uint32_t topic = 0; topic < manual::kTopicCount; ++topic) {
    offsets.push_back((manual::kTopicStrings + topic) * 2);
    offsets.push_back((manual::kTopicStrings + manual::kTopicCount + topic) *
                      2);
  }
  return offsets;
}

// A help text file: 16 header bytes, the line count, the line table and the
// lines, each padded to a halfword and given as offsets from byte 16.
std::vector<std::uint8_t> HelpText(
    const std::vector<std::vector<std::uint8_t>>& lines) {
  std::vector<std::uint8_t> help(manual::kLineTableByte, 0);
  const auto count = static_cast<std::uint16_t>(lines.size());
  help.push_back(static_cast<std::uint8_t>(count & 0xFFU));
  help.push_back(static_cast<std::uint8_t>(count >> 8U));
  help.resize(help.size() + (lines.size() * 2));
  for (std::size_t line = 0; line < lines.size(); ++line) {
    const auto entry =
        static_cast<std::uint16_t>((help.size() - manual::kLineTableByte) / 2);
    help[manual::kLineTableByte + 2 + (line * 2)] =
        static_cast<std::uint8_t>(entry & 0xFFU);
    help[manual::kLineTableByte + 3 + (line * 2)] =
        static_cast<std::uint8_t>(entry >> 8U);
    help.insert(help.end(), lines[line].begin(), lines[line].end());
    help.push_back(manual::kTextTerminator);
    if (help.size() % 2 != 0) {
      help.push_back(0);
    }
  }
  return help;
}

// Lines `first`..`first + count - 1`, each two glyphs.
std::vector<std::vector<std::uint8_t>> Lines(int count) {
  std::vector<std::vector<std::uint8_t>> lines;
  for (int line = 0; line < count; ++line) {
    lines.push_back({static_cast<std::uint8_t>(17 + line),
                     static_cast<std::uint8_t>(40 + line)});
  }
  return lines;
}

std::vector<std::uint8_t> ExpectedPage(int first, int end) {
  std::vector<std::uint8_t> page{0xF8, 0x00, 0xFB, 0x06};
  for (int line = first; line < end; ++line) {
    page.push_back(static_cast<std::uint8_t>(17 + line));
    page.push_back(static_cast<std::uint8_t>(40 + line));
    page.push_back(manual::kTextNewline);
  }
  page.back() = manual::kTextTerminator;
  return page;
}

std::vector<std::uint8_t> Composed(std::span<const std::uint8_t> help,
                                   std::int32_t scroll) {
  std::array<std::uint8_t, manual::kPageTextCapacity> page{};
  const auto length = manual::ComposePageText(help, scroll, page);
  if (!length.has_value()) {
    return {};
  }
  return {page.begin(), page.begin() + static_cast<std::ptrdiff_t>(*length)};
}
}  // namespace

// --- Topic menu --------------------------------------------------------------

TEST_CASE("Topic cursor keeps short lists and top rows at the list top",
          "[manual-topics]") {
  CHECK(manual::TopicCursorFor(8, 7) == TopicCursor{.row = 7, .scroll = 0});
  CHECK(manual::TopicCursorFor(14, 7) == TopicCursor{.row = 7, .scroll = 0});
  CHECK(manual::TopicCursorFor(3, 200) == TopicCursor{.row = 200, .scroll = 0});
}

TEST_CASE("Topic cursor shows the last nine rows for rows in the last eight",
          "[manual-topics]") {
  CHECK(manual::TopicCursorFor(14, 8) == TopicCursor{.row = 3, .scroll = 5});
  CHECK(manual::TopicCursorFor(14, 13) == TopicCursor{.row = 8, .scroll = 5});
  CHECK(manual::TopicCursorFor(20, 12) == TopicCursor{.row = 1, .scroll = 11});
}

TEST_CASE("Topic cursor centres a middle row of a long list on row four",
          "[manual-topics]") {
  CHECK(manual::TopicCursorFor(20, 8) == TopicCursor{.row = 4, .scroll = 4});
  CHECK(manual::TopicCursorFor(20, 11) == TopicCursor{.row = 4, .scroll = 7});
}

TEST_CASE("Topic rows flag the last row and force cursor memory on",
          "[manual-topics]") {
  FakeTopicHost host;
  const std::vector<std::uint32_t> offsets{2, 4, 6, 8, 10, 12, 14, 16};
  REQUIRE(manual::SetTopicRows(host, 3, 1, offsets).has_value());
  CHECK(host.cursors == std::vector<TopicCursor>{{.row = 1, .scroll = 0}});
  CHECK(host.created_info == manual::kTopicRowInfo);
  CHECK(host.created_offsets == std::vector<std::uint32_t>{2, 4, 6, 8, 10, 12});
  CHECK(host.created_types == std::vector<std::int32_t>{0, 0, 4});
  CHECK(host.setting_while_creating == 1);
  CHECK(host.setting == 7);
}

TEST_CASE("Topic rows refuse an empty list and missing strings untouched",
          "[manual-topics]") {
  FakeTopicHost host;
  const std::vector<std::uint32_t> offsets{2, 4, 6};
  const auto empty = manual::SetTopicRows(host, 0, 0, offsets);
  REQUIRE_FALSE(empty.has_value());
  CHECK(empty.error() == TopicError::kNoRows);
  const auto short_strings = manual::SetTopicRows(host, 2, 0, offsets);
  REQUIRE_FALSE(short_strings.has_value());
  CHECK(short_strings.error() == TopicError::kTooFewStrings);
  CHECK(host.cursors.empty());
  CHECK(host.creations == 0);
}

TEST_CASE("Topic selection leaves an undecided menu alone", "[manual-topics]") {
  FakeTopicHost host;
  host.next_row = -1;
  CHECK(manual::GetSelectedTopicRow(host) == -1);
  CHECK(host.cleared.empty());
  CHECK(host.fly_outs.empty());
}

TEST_CASE("Topic selection flies out a chosen row and clears a cancel",
          "[manual-topics]") {
  FakeTopicHost host;
  host.cursor_row = 6;
  host.next_row = 0;
  CHECK(manual::GetSelectedTopicRow(host) == 0);
  CHECK(host.fly_outs == std::vector<std::array<std::int32_t, 2>>{{6, 1}});
  host.next_row = -2;
  CHECK(manual::GetSelectedTopicRow(host) == -2);
  CHECK(host.cleared == std::vector<std::int32_t>{5});
  CHECK(host.fly_outs.size() == 1);
}

TEST_CASE("Topic strings pair each name with its description",
          "[manual-topics]") {
  const auto offsets = manual::TopicTextOffsets(TopicTable());
  REQUIRE(offsets.has_value());
  CHECK(std::vector<std::uint32_t>(offsets->begin(), offsets->end()) ==
        ExpectedTopicOffsets());
}

TEST_CASE("Topic strings refuse a short table and strings past it",
          "[manual-topics]") {
  std::vector<std::uint16_t> table = TopicTable();
  const auto short_table = manual::TopicTextOffsets(std::span(table).first(27));
  REQUIRE_FALSE(short_table.has_value());
  CHECK(short_table.error() == TopicError::kShortTopicTable);
  table[20] = 64;
  const auto outside = manual::TopicTextOffsets(table);
  REQUIRE_FALSE(outside.has_value());
  CHECK(outside.error() == TopicError::kTopicTextOutsideTable);
}

TEST_CASE("Topic strings refuse a string without a terminator",
          "[manual-topics]") {
  std::vector<std::uint16_t> table = TopicTable();
  std::fill(table.begin() + 50, table.end(), 0x2020);
  const auto unterminated = manual::TopicTextOffsets(table);
  REQUIRE_FALSE(unterminated.has_value());
  CHECK(unterminated.error() == TopicError::kUnterminatedTopicText);
}

TEST_CASE("Topic menu lists the rows once then polls the selection",
          "[manual-topics]") {
  FakeTopicHost host;
  manual::TopicMenuState state{.step = 9, .selected_row = 3};
  CHECK(manual::UpdateTopicMenu(state, true, TopicTable(), 13, host) == -1);
  CHECK(state.step == manual::kTopicStepSelectRow);
  CHECK(host.created_count == manual::kTopicCount);
  CHECK(host.created_offsets == ExpectedTopicOffsets());
  CHECK(host.cursors.back() == TopicCursor{.row = 8, .scroll = 5});
  CHECK(manual::UpdateTopicMenu(state, false, TopicTable(), 13, host) == -1);
  host.next_row = 4;
  CHECK(manual::UpdateTopicMenu(state, false, TopicTable(), 13, host) == 4);
  CHECK(state.selected_row == 4);
}

TEST_CASE("Topic menu returns a cancel and restarts on reentry",
          "[manual-topics]") {
  FakeTopicHost host;
  manual::TopicMenuState state{.step = manual::kTopicStepSelectRow};
  host.next_row = -2;
  CHECK(manual::UpdateTopicMenu(state, false, TopicTable(), 0, host) == -2);
  CHECK(host.cleared == std::vector<std::int32_t>{5});
  CHECK(manual::UpdateTopicMenu(state, true, TopicTable(), 0, host) == -1);
  CHECK(state.selected_row == 0);
  CHECK(host.creations == 1);
}

TEST_CASE("Topic menu does nothing past its selection step",
          "[manual-topics]") {
  FakeTopicHost host;
  manual::TopicMenuState state{.step = 2, .selected_row = 6};
  host.next_row = 1;
  CHECK(manual::UpdateTopicMenu(state, false, TopicTable(), 0, host) == -1);
  CHECK(state.step == 2);
  CHECK(state.selected_row == 6);
  CHECK(host.creations == 0);
}

TEST_CASE("Topic menu refuses an unlistable table before advancing",
          "[manual-topics]") {
  FakeTopicHost host;
  manual::TopicMenuState state;
  std::vector<std::uint16_t> table = TopicTable();
  table[3] = 500;
  const auto result = manual::UpdateTopicMenu(state, true, table, 0, host);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == TopicError::kTopicTextOutsideTable);
  CHECK(state.step == manual::kTopicStepListRows);
  CHECK(host.creations == 0);
}

// --- Page text ---------------------------------------------------------------

TEST_CASE("Last page offset rounds the line count up to whole pages",
          "[manual-topics]") {
  CHECK(manual::LastPageLineOffset(0) == 0);
  CHECK(manual::LastPageLineOffset(5) == 0);
  CHECK(manual::LastPageLineOffset(10) == 0);
  CHECK(manual::LastPageLineOffset(11) == 10);
  CHECK(manual::LastPageLineOffset(20) == 10);
  CHECK(manual::LastPageLineOffset(23) == 20);
  CHECK(manual::LastPageLineOffset(0xFFFF) == 65530);
}

TEST_CASE("Help line count reads the halfword after the header",
          "[manual-topics]") {
  const std::vector<std::uint8_t> help = HelpText(Lines(259));
  const auto count = manual::HelpLineCount(help);
  REQUIRE(count.has_value());
  CHECK(*count == 259);
  const auto short_text = manual::HelpLineCount(std::span(help).first(17));
  REQUIRE_FALSE(short_text.has_value());
  CHECK(short_text.error() == PageTextError::kShortHelpText);
}

TEST_CASE("Encoded lines copy to the terminator and end with a newline",
          "[manual-topics]") {
  const std::array<std::uint8_t, 6> source{0x10, 0x11, 0xE7, 0x20, 0x21, 0x22};
  std::array<std::uint8_t, 8> destination{};
  const auto written = manual::CopyEncodedLine(source, destination);
  REQUIRE(written.has_value());
  CHECK(*written == 3);
  CHECK(destination == std::array<std::uint8_t, 8>{0x10, 0x11, 0xE8});
}

TEST_CASE("Encoded lines copy a control code's operand even a terminator",
          "[manual-topics]") {
  const std::array<std::uint8_t, 5> source{0xEC, 0xE7, 0xEB, 0x05, 0xE7};
  std::array<std::uint8_t, 8> destination{};
  const auto written = manual::CopyEncodedLine(source, destination);
  REQUIRE(written.has_value());
  CHECK(*written == 5);
  CHECK(destination ==
        std::array<std::uint8_t, 8>{0xEC, 0xE7, 0xEB, 0x05, 0xE8});
}

TEST_CASE("Encoded lines refuse an unterminated line and a split code",
          "[manual-topics]") {
  std::array<std::uint8_t, 8> destination{};
  const std::array<std::uint8_t, 2> plain{0x10, 0x11};
  const auto unterminated = manual::CopyEncodedLine(plain, destination);
  REQUIRE_FALSE(unterminated.has_value());
  CHECK(unterminated.error() == PageTextError::kUnterminatedLine);
  const std::array<std::uint8_t, 2> split{0x10, 0xF0};
  const auto split_code = manual::CopyEncodedLine(split, destination);
  REQUIRE_FALSE(split_code.has_value());
  CHECK(split_code.error() == PageTextError::kUnterminatedLine);
}

TEST_CASE("Encoded lines refuse a destination without room for the newline",
          "[manual-topics]") {
  const std::array<std::uint8_t, 3> source{0x10, 0x11, 0xE7};
  std::array<std::uint8_t, 3> fits{};
  CHECK(manual::CopyEncodedLine(source, fits) == 3U);
  std::array<std::uint8_t, 2> no_newline{};
  const auto overflow = manual::CopyEncodedLine(source, no_newline);
  REQUIRE_FALSE(overflow.has_value());
  CHECK(overflow.error() == PageTextError::kPageOverflow);
}

TEST_CASE("Page text composes ten lines from the scroll position",
          "[manual-topics]") {
  const std::vector<std::uint8_t> help = HelpText(Lines(12));
  CHECK(Composed(help, 0) == ExpectedPage(0, 10));
  CHECK(Composed(help, 1) == ExpectedPage(1, 11));
}

TEST_CASE("Page text stops a short final page at the last line",
          "[manual-topics]") {
  const std::vector<std::uint8_t> help = HelpText(Lines(12));
  CHECK(Composed(help, 10) == ExpectedPage(10, 12));
  CHECK(Composed(help, 11) == ExpectedPage(11, 12));
}

TEST_CASE("Page text past the last line is an empty terminated page",
          "[manual-topics]") {
  const std::vector<std::uint8_t> help = HelpText(Lines(3));
  for (const std::int32_t scroll :
       {3, 40, std::numeric_limits<std::int32_t>::max()}) {
    std::array<std::uint8_t, manual::kPageTextCapacity> page{};
    CHECK(manual::ComposePageText(help, scroll, page) == 4U);
    CHECK(page[3] == manual::kTextTerminator);
    CHECK(page[4] == manual::kTextNewline);
  }
}

TEST_CASE("Page text refuses a negative scroll and lines past the file",
          "[manual-topics]") {
  std::vector<std::uint8_t> help = HelpText(Lines(2));
  std::array<std::uint8_t, manual::kPageTextCapacity> page{};
  const auto negative = manual::ComposePageText(help, -1, page);
  REQUIRE_FALSE(negative.has_value());
  CHECK(negative.error() == PageTextError::kNegativeScroll);
  help[manual::kLineTableByte + 4] = 0xFF;
  const auto outside = manual::ComposePageText(help, 0, page);
  REQUIRE_FALSE(outside.has_value());
  CHECK(outside.error() == PageTextError::kLineOutsideText);
}

TEST_CASE("Page text refuses a line table that runs past the file",
          "[manual-topics]") {
  std::vector<std::uint8_t> help = HelpText(Lines(2));
  help[manual::kLineTableByte] = 200;
  std::array<std::uint8_t, manual::kPageTextCapacity> page{};
  const auto past = manual::ComposePageText(help, 0, page);
  REQUIRE_FALSE(past.has_value());
  CHECK(past.error() == PageTextError::kLineOutsideText);
}

TEST_CASE("Page text refuses lines that overflow the page buffer",
          "[manual-topics]") {
  const std::vector<std::uint8_t> long_line(150, 0x20);
  const std::vector<std::uint8_t> help =
      HelpText(std::vector<std::vector<std::uint8_t>>(10, long_line));
  std::array<std::uint8_t, manual::kPageTextCapacity> page{};
  const auto overflow = manual::ComposePageText(help, 0, page);
  REQUIRE_FALSE(overflow.has_value());
  CHECK(overflow.error() == PageTextError::kPageOverflow);
  CHECK(Composed(help, 4).size() == 4 + (6 * 151));
}

// --- Live bindings ----------------------------------------------------------

namespace {
struct LiveTopicFixture {
  VS::Input::Settings saved_settings = VS::Input::settings;
  VS::Menu::Framework::MenuMemory saved_memory =
      VS::Menu::Framework::menuMemory;
  VS::Menu::Framework::MenuRow* saved_items = VS::Menu::Framework::menuItems;
  u_short* saved_text = VS::Menu::Framework::itemsText;
  int saved_selected = VS::Menu::Framework::selectedRow;
  u_short saved_count = VS::Menu::Framework::subMenuRowCount;
  std::array<u_short, 516> saved_table = std::to_array(manual::menuTextTable);
  int saved_step = manual::topMenu_state;
  int saved_row = manual::menuSelectedRow;
  u_short saved_manual = Field_manualDisplayState.currentManual;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 768> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  struct {
    std::array<VS::Menu::Framework::MenuRow, 40> rows{};
    std::array<std::byte, 0x124> scratch{};
  } workspace;

  LiveTopicFixture() {
    VS::Menu::Framework::menuItems = workspace.rows.data();
    VS::Menu::Framework::itemsText = nullptr;
    VS::Menu::Framework::menuMemory = {};
    VS::Input::settings.menu_flags = 0x3FF;
    std::fill(std::begin(manual::menuTextTable),
              std::end(manual::menuTextTable), u_short{0});
    const std::vector<std::uint16_t> table = TopicTable();
    std::copy(table.begin(), table.end(), manual::menuTextTable);
  }
  LiveTopicFixture(const LiveTopicFixture&) = delete;
  LiveTopicFixture& operator=(const LiveTopicFixture&) = delete;
  LiveTopicFixture(LiveTopicFixture&&) = delete;
  LiveTopicFixture& operator=(LiveTopicFixture&&) = delete;
  ~LiveTopicFixture() {
    ReleaseRows();
    VS::Input::settings = saved_settings;
    VS::Menu::Framework::menuMemory = saved_memory;
    VS::Menu::Framework::menuItems = saved_items;
    VS::Menu::Framework::itemsText = saved_text;
    VS::Menu::Framework::selectedRow = saved_selected;
    VS::Menu::Framework::subMenuRowCount = saved_count;
    std::copy(saved_table.begin(), saved_table.end(), manual::menuTextTable);
    manual::topMenu_state = saved_step;
    manual::menuSelectedRow = saved_row;
    Field_manualDisplayState.currentManual = saved_manual;
  }

  static void ReleaseRows() {
    if (VS::Menu::Framework::itemsText != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(VS::Menu::Framework::itemsText);
      VS::Menu::Framework::itemsText = nullptr;
    }
  }
};

struct LivePageFixture {
  int* saved_help_text = manual::helpText;
  int saved_scroll = manual::scrollPosition;
  VS::Menu::TextBoxState saved_box = *VS::Menu::LiveTextBox(0);
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, std::size_t{16} * 1024> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};

  LivePageFixture() {
    manual::scrollPosition = 0;
    VS::Menu::LiveTextBox(0)->flags.speaker_texture = 32;
  }
  LivePageFixture(const LivePageFixture&) = delete;
  LivePageFixture& operator=(const LivePageFixture&) = delete;
  LivePageFixture(LivePageFixture&&) = delete;
  LivePageFixture& operator=(LivePageFixture&&) = delete;
  ~LivePageFixture() {
    *VS::Menu::LiveTextBox(0) = saved_box;
    manual::helpText = saved_help_text;
    manual::scrollPosition = saved_scroll;
  }

  void Load(std::span<const std::uint8_t> help) {
    auto* payload = static_cast<std::uint8_t*>(
        VS::Core::Memory::AllocateLiveFromHighEnd(help.size()));
    REQUIRE(payload != nullptr);
    std::ranges::copy(help, payload);
    manual::helpText = reinterpret_cast<int*>(payload);
  }

  static std::vector<std::uint8_t> BoxText(std::size_t length) {
    const char* text = VS::Menu::LiveTextBox(0)->text;
    if (text == nullptr) {
      return {};
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(text);
    return {bytes, bytes + length};
  }
};
}  // namespace

TEST_CASE("Live topic menu lists the module's topic rows then polls",
          "[manual-topics-live]") {
  LiveTopicFixture fixture;
  Field_manualDisplayState.currentManual = 10;
  CHECK(manual::LiveUpdateTopicMenu(1) == -1);
  CHECK(manual::topMenu_state == 1);
  CHECK(VS::Menu::Framework::menuMemory.cursor_memories[4] == 5);
  CHECK(VS::Menu::Framework::menuMemory.cursor_memories[5] == 5);
  CHECK(VS::Input::settings.cursor_memory ==
        fixture.saved_settings.cursor_memory);
  REQUIRE(VS::Menu::Framework::itemsText != nullptr);
  CHECK(manual::LiveUpdateTopicMenu(0) == -1);
  CHECK(manual::topMenu_state == 1);
}

TEST_CASE("Live topic selection returns chosen and cancelled rows",
          "[manual-topics-live]") {
  LiveTopicFixture fixture;
  VS::Menu::Framework::menuMemory.cursor_memories[4] = 3;
  VS::Menu::Framework::selectedRow = 2;
  manual::topMenu_state = 1;
  CHECK(manual::LiveUpdateTopicMenu(0) == 2);
  CHECK(manual::menuSelectedRow == 2);
  VS::Menu::Framework::selectedRow = -2;
  CHECK(manual::LiveGetSelectedTopicRow() == -2);
}

TEST_CASE("Live topic menu refuses an unterminated table as a cancel",
          "[manual-topics-live]") {
  LiveTopicFixture fixture;
  std::fill(std::begin(manual::menuTextTable) + manual::kTopicStrings,
            std::end(manual::menuTextTable), u_short{0x2020});
  const auto refusals = manual::LiveTopicRefusalCount();
  CHECK(manual::LiveUpdateTopicMenu(1) == -2);
  CHECK(manual::LiveTopicRefusalCount() == refusals + 1);
  CHECK(manual::topMenu_state == 0);
  CHECK(VS::Menu::Framework::itemsText == nullptr);
}

TEST_CASE("Live topic rows refuse strings outside the topic table",
          "[manual-topics-live]") {
  LiveTopicFixture fixture;
  std::array<char, 4> outside{};
  std::array<char*, 2> strings{reinterpret_cast<char*>(manual::menuTextTable),
                               outside.data()};
  const auto refusals = manual::LiveTopicRefusalCount();
  manual::LiveSetTopicRows(1, 0, strings.data());
  manual::LiveSetTopicRows(0, 0, strings.data());
  CHECK(manual::LiveTopicRefusalCount() == refusals + 2);
  CHECK(VS::Menu::Framework::itemsText == nullptr);
}

TEST_CASE("Live topic rows list strings inside the topic table",
          "[manual-topics-live]") {
  LiveTopicFixture fixture;
  auto* table = reinterpret_cast<char*>(manual::menuTextTable);
  std::array<char*, 2> strings{table + 56, table + 84};
  const auto refusals = manual::LiveTopicRefusalCount();
  manual::LiveSetTopicRows(1, 0, strings.data());
  CHECK(manual::LiveTopicRefusalCount() == refusals);
  CHECK(VS::Menu::Framework::itemsText != nullptr);
  CHECK(VS::Menu::Framework::menuMemory.cursor_memories[4] == 0);
}

TEST_CASE("Live page text hands the composed page to text box zero",
          "[manual-topics-live]") {
  LivePageFixture fixture;
  const std::vector<std::uint8_t> help = HelpText(Lines(12));
  fixture.Load(help);
  manual::scrollPosition = 10;
  manual::LiveCopyPageText();
  const std::vector<std::uint8_t> expected = ExpectedPage(10, 12);
  CHECK(LivePageFixture::BoxText(expected.size()) == expected);
  CHECK(manual::LiveLastPageLineOffset() == 10);
}

TEST_CASE("Live page text refuses help text outside the game heap",
          "[manual-topics-live]") {
  LivePageFixture fixture;
  std::vector<std::uint8_t> help = HelpText(Lines(23));
  manual::helpText = reinterpret_cast<int*>(help.data());
  const char* before = VS::Menu::LiveTextBox(0)->text;
  const auto refusals = manual::LiveTopicRefusalCount();
  manual::LiveCopyPageText();
  CHECK(manual::LiveLastPageLineOffset() == 0);
  CHECK(manual::LiveTopicRefusalCount() == refusals + 2);
  CHECK(VS::Menu::LiveTextBox(0)->text == before);
}

TEST_CASE("Live page text refuses a negative scroll position untouched",
          "[manual-topics-live]") {
  LivePageFixture fixture;
  fixture.Load(HelpText(Lines(23)));
  CHECK(manual::LiveLastPageLineOffset() == 20);
  manual::scrollPosition = -10;
  const char* before = VS::Menu::LiveTextBox(0)->text;
  const auto refusals = manual::LiveTopicRefusalCount();
  manual::LiveCopyPageText();
  CHECK(manual::LiveTopicRefusalCount() == refusals + 1);
  CHECK(VS::Menu::LiveTextBox(0)->text == before);
}
