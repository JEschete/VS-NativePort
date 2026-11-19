// Hand-maintained (tools/port/owned.csv).
// Live binding for the manual's topic menu and page text (ManualTopics.cpp):
// the topic string table, the menu framework's rows and cursor memory, the
// help text file on the game heap and text box 0.
#include "Menu/ManualTopicsLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/Manual.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Manual.h"
#include "Menu/ManualTopics.hpp"
#include "Menu/TextBoxStateLive.hpp"

namespace VS::Menu::manual {
namespace {
constexpr std::uint32_t kRefusalLogLimit = 16;
constexpr std::uint32_t kRefusalLogInterval = 1024;
// D_800F4EE8.cursorMemories: the manual's list cursor row and first row.
constexpr std::size_t kCursorRowMemory = 4;
constexpr std::size_t kCursorScrollMemory = 5;
constexpr int kTopicMenuCancelled = -2;
constexpr int kTextBox = 0;
std::uint64_t g_refusals = 0;
// The composed page outlives the call: text box 0 keeps printing from it.
std::array<std::uint8_t, kPageTextCapacity> g_page_text{};

void Refuse(std::string_view reason) {
  ++g_refusals;
  if (g_refusals <= kRefusalLogLimit || g_refusals % kRefusalLogInterval == 0) {
    VS::Core::LogWarn("menu.manual", "{} (refusal {})", reason, g_refusals);
  }
}

std::span<const std::uint16_t> TopicTable() {
  return std::span<const std::uint16_t>(Menu_Manual_menuText);
}

class LiveTopicMenuHost final : public TopicMenuHost {
 public:
  void SetTopicCursor(TopicCursor cursor) override {
    VS::Menu::Framework::menuMemory.cursor_memories[kCursorRowMemory] =
        cursor.row;
    VS::Menu::Framework::menuMemory.cursor_memories[kCursorScrollMemory] =
        cursor.scroll;
  }
  [[nodiscard]] std::uint8_t TopicCursorRow() const override {
    return VS::Menu::Framework::menuMemory.cursor_memories[kCursorRowMemory];
  }
  [[nodiscard]] std::uint8_t CursorMemorySetting() const override {
    return static_cast<std::uint8_t>(VS::Input::settings.cursor_memory);
  }
  void SetCursorMemorySetting(std::uint8_t value) override {
    VS::Input::settings.cursor_memory = static_cast<char>(value);
  }
  void CreateRows(std::int32_t row_count, std::int32_t row_info,
                  std::span<const std::uint32_t> text_offsets,
                  std::span<const std::int32_t> row_types) override {
    auto* const table = reinterpret_cast<char*>(Menu_Manual_menuText);
    std::vector<char*> strings;
    strings.reserve(text_offsets.size());
    for (const std::uint32_t offset : text_offsets) {
      // The cores only hand over offsets they checked against the table.
      strings.push_back(
          table + std::min<std::size_t>(offset, TopicTable().size_bytes() - 1));
    }
    std::vector<int> types(row_types.begin(), row_types.end());
    Framework::CreateLiveMenuRows(row_count, row_info, strings.data(),
                                  types.data());
  }
  [[nodiscard]] std::int32_t SelectedRow() override {
    return Framework::LiveSelectedRow();
  }
  void ClearMenuExcept(std::int32_t row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void FlyOutAndHoistSelection(std::int32_t row, std::int32_t offset) override {
    Framework::FlyOutLiveRowsAndHoistSelection(row, offset);
  }
};

// The topic table byte offsets of `strings`, or nothing when one lies
// outside the table.
std::optional<std::vector<std::uint32_t>> TopicStringOffsets(
    char* const* strings, std::size_t count) {
  if (strings == nullptr) {
    return std::nullopt;
  }
  const auto table = std::as_bytes(TopicTable());
  const auto begin = reinterpret_cast<std::uintptr_t>(table.data());
  std::vector<std::uint32_t> offsets;
  offsets.reserve(count);
  for (const char* text : std::span(strings, count)) {
    const auto address = reinterpret_cast<std::uintptr_t>(text);
    if (address < begin || address - begin >= table.size()) {
      return std::nullopt;
    }
    offsets.push_back(static_cast<std::uint32_t>(address - begin));
  }
  return offsets;
}

// The loaded help text file, from its start to the end of its heap payload.
std::optional<std::span<const std::uint8_t>> HelpText() {
  const auto* first = reinterpret_cast<const std::byte*>(Menu_Manual_helpText);
  if (first == nullptr) {
    return std::nullopt;
  }
  const auto payload = Core::Memory::LivePayloadContaining(first);
  if (!payload.has_value()) {
    return std::nullopt;
  }
  const std::byte* end = payload->data() + payload->size();
  if (first < payload->data() || first >= end) {
    return std::nullopt;
  }
  return std::span(reinterpret_cast<const std::uint8_t*>(first),
                   static_cast<std::size_t>(end - first));
}

std::string_view PageRefusal(PageTextError error) {
  switch (error) {
    case PageTextError::kNegativeScroll:
      return "Manual page scroll position is negative";
    case PageTextError::kShortHelpText:
      return "Manual help text ends before its line count";
    case PageTextError::kLineOutsideText:
      return "Manual help line lies past its help text";
    case PageTextError::kUnterminatedLine:
      return "Manual help line runs past its help text";
    case PageTextError::kPageOverflow:
      return "Manual page text overflows its buffer";
  }
  return "Manual page text refused";
}
}  // namespace

// decomp: MENUE.PRG _setMenuRows (494.c:76).
void LiveSetTopicRows(int row_count, int highlighted_row, char** strings) {
  if (row_count < 1) {
    Refuse("Manual topic list has no rows");
    return;
  }
  const auto offsets =
      TopicStringOffsets(strings, static_cast<std::size_t>(row_count) * 2);
  if (!offsets.has_value()) {
    Refuse("Manual topic strings lie outside the topic table");
    return;
  }
  LiveTopicMenuHost host;
  if (!SetTopicRows(host, row_count, highlighted_row, *offsets).has_value()) {
    Refuse("Manual topic rows refused");
  }
}

// decomp: MENUE.PRG _getSelectedRow (494.c:104).
int LiveGetSelectedTopicRow() {
  LiveTopicMenuHost host;
  return GetSelectedTopicRow(host);
}

// decomp: MENUE.PRG _topMenu (494.c:450).
int LiveUpdateTopicMenu(int init) {
  TopicMenuState state{.step = Menu_Manual_topMenuState,
                       .selected_row = Menu_Manual_selectedRow};
  LiveTopicMenuHost host;
  const auto result =
      UpdateTopicMenu(state, init != 0, TopicTable(),
                      Field_manualDisplayState.currentManual, host);
  Menu_Manual_topMenuState = state.step;
  Menu_Manual_selectedRow = state.selected_row;
  if (!result.has_value()) {
    Refuse("Manual topic table cannot be listed");
    return kTopicMenuCancelled;
  }
  return *result;
}

// decomp: MENUE.PRG _getRoundedLineCount (494.c:514).
int LiveLastPageLineOffset() {
  const auto help_text = HelpText();
  if (!help_text.has_value()) {
    Refuse("Manual help text is outside the game heap");
    return 0;
  }
  const auto line_count = HelpLineCount(*help_text);
  if (!line_count.has_value()) {
    Refuse(PageRefusal(line_count.error()));
    return 0;
  }
  return LastPageLineOffset(*line_count);
}

// decomp: MENUE.PRG _copyNextPageText (494.c:531).
void LiveCopyPageText() {
  const auto help_text = HelpText();
  if (!help_text.has_value()) {
    Refuse("Manual help text is outside the game heap");
    return;
  }
  std::array<std::uint8_t, kPageTextCapacity> page{};
  const auto length =
      ComposePageText(*help_text, Menu_Manual_scrollPosition, page);
  if (!length.has_value()) {
    Refuse(PageRefusal(length.error()));
    return;
  }
  g_page_text = page;
  SetLiveTextBoxText(kTextBox, reinterpret_cast<char*>(g_page_text.data()));
}

std::uint64_t LiveTopicRefusalCount() { return g_refusals; }
}  // namespace VS::Menu::manual
