// Hand-maintained (tools/port/owned.csv).
// The manual's topic menu and page text (MENUE.PRG 494.c:76-118, 450-575).
#include "Menu/ManualTopics.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::manual {
namespace {
using Core::Math::WrappingAdd;
using Core::Math::WrappingSubtract;

constexpr unsigned kBitsPerByte = 8;
constexpr std::uint8_t kByteMask = 0xFF;
// The rows above which the highlighted row keeps the list at its top.
constexpr std::int32_t kTopRows = kVisibleTopicRows - 1;

// Byte `index` of a little-endian halfword table.
std::uint8_t TableByte(std::span<const std::uint16_t> table,
                       std::size_t index) {
  const unsigned shift = static_cast<unsigned>(index % 2) * kBitsPerByte;
  return static_cast<std::uint8_t>((table[index / 2] >> shift) & kByteMask);
}

std::uint16_t ReadHalfword(std::span<const std::uint8_t> bytes,
                           std::size_t index) {
  return static_cast<std::uint16_t>(bytes[index] |
                                    (bytes[index + 1] << kBitsPerByte));
}

// The byte offset of the topic string at halfword index `entry`.
std::expected<std::uint32_t, TopicError> TopicTextOffset(
    std::span<const std::uint16_t> table, std::size_t entry) {
  const std::size_t table_bytes = table.size() * 2;
  const std::size_t start = std::size_t{table[entry]} * 2;
  if (start >= table_bytes) {
    return std::unexpected(TopicError::kTopicTextOutsideTable);
  }
  for (std::size_t index = start; index < table_bytes; ++index) {
    if (TableByte(table, index) == kTextTerminator) {
      return static_cast<std::uint32_t>(start);
    }
  }
  return std::unexpected(TopicError::kUnterminatedTopicText);
}
}  // namespace

// decomp: MENUE.PRG _setMenuRows (494.c:76), cursorMemories[4] and [5].
TopicCursor TopicCursorFor(std::int32_t row_count,
                           std::int32_t highlighted_row) noexcept {
  if (row_count < kVisibleTopicRows || highlighted_row < kTopRows) {
    return {.row = static_cast<std::uint8_t>(highlighted_row), .scroll = 0};
  }
  const std::int32_t last_scroll =
      WrappingSubtract(row_count, kVisibleTopicRows);
  if (highlighted_row >= WrappingSubtract(row_count, kTopRows)) {
    return {.row = static_cast<std::uint8_t>(
                WrappingSubtract(highlighted_row, last_scroll)),
            .scroll = static_cast<std::uint8_t>(last_scroll)};
  }
  return {.row = static_cast<std::uint8_t>(kMiddleTopicRow),
          .scroll = static_cast<std::uint8_t>(
              WrappingSubtract(highlighted_row, kMiddleTopicRow))};
}

// decomp: MENUE.PRG _setMenuRows (494.c:76).
std::expected<void, TopicError> SetTopicRows(
    TopicMenuHost& host, std::int32_t row_count, std::int32_t highlighted_row,
    std::span<const std::uint32_t> text_offsets) {
  if (row_count < 1) {
    return std::unexpected(TopicError::kNoRows);
  }
  const auto rows = static_cast<std::size_t>(row_count);
  if (text_offsets.size() / 2 < rows) {
    return std::unexpected(TopicError::kTooFewStrings);
  }
  std::vector<std::int32_t> row_types(rows, 0);
  row_types.back() |= kLastRowType;
  host.SetTopicCursor(TopicCursorFor(row_count, highlighted_row));
  const std::uint8_t saved = host.CursorMemorySetting();
  host.SetCursorMemorySetting(1);
  host.CreateRows(row_count, kTopicRowInfo, text_offsets.first(rows * 2),
                  row_types);
  host.SetCursorMemorySetting(saved);
  return {};
}

// decomp: MENUE.PRG _getSelectedRow (494.c:104).
std::int32_t GetSelectedTopicRow(TopicMenuHost& host) {
  const std::int32_t row = host.SelectedRow();
  if (row < kNoTopicSelected) {
    host.ClearMenuExcept(kTopicClearKeptRow);
    return row;
  }
  if (row >= 0) {
    host.FlyOutAndHoistSelection(host.TopicCursorRow(), 1);
  }
  return row;
}

// decomp: MENUE.PRG _topMenu (494.c:450), the strings loop.
std::expected<TopicTextOffsetArray, TopicError> TopicTextOffsets(
    std::span<const std::uint16_t> topic_table) {
  if (topic_table.size() < kTopicStrings) {
    return std::unexpected(TopicError::kShortTopicTable);
  }
  TopicTextOffsetArray offsets{};
  for (std::size_t topic = 0; topic < std::size_t{kTopicCount}; ++topic) {
    const auto name = TopicTextOffset(topic_table, topic);
    if (!name.has_value()) {
      return std::unexpected(name.error());
    }
    const auto description = TopicTextOffset(topic_table, kTopicCount + topic);
    if (!description.has_value()) {
      return std::unexpected(description.error());
    }
    offsets[topic * 2] = *name;
    offsets[(topic * 2) + 1] = *description;
  }
  return offsets;
}

// decomp: MENUE.PRG _topMenu (494.c:450).
std::expected<std::int32_t, TopicError> UpdateTopicMenu(
    TopicMenuState& state, bool reset,
    std::span<const std::uint16_t> topic_table, std::int32_t current_topic,
    TopicMenuHost& host) {
  if (reset) {
    state.step = kTopicStepListRows;
    state.selected_row = 0;
  }
  if (state.step == kTopicStepListRows) {
    const auto offsets = TopicTextOffsets(topic_table);
    if (!offsets.has_value()) {
      return std::unexpected(offsets.error());
    }
    const auto listed =
        SetTopicRows(host, kTopicCount, current_topic, *offsets);
    if (!listed.has_value()) {
      return std::unexpected(listed.error());
    }
    ++state.step;
  } else if (state.step == kTopicStepSelectRow) {
    state.selected_row = GetSelectedTopicRow(host);
    if (state.selected_row != kNoTopicSelected) {
      return state.selected_row;
    }
  }
  return kNoTopicSelected;
}

std::expected<std::uint16_t, PageTextError> HelpLineCount(
    std::span<const std::uint8_t> help_text) {
  if (help_text.size() < kLineTableByte + 2) {
    return std::unexpected(PageTextError::kShortHelpText);
  }
  return ReadHalfword(help_text, kLineTableByte);
}

// decomp: MENUE.PRG _getRoundedLineCount (494.c:514).
std::int32_t LastPageLineOffset(std::uint16_t line_count) noexcept {
  std::int32_t lines = line_count;
  const std::int32_t remainder = lines % kLinesPerPage;
  if (remainder > 0) {
    lines += kLinesPerPage - remainder;
  }
  return std::max(lines - kLinesPerPage, 0);
}

// decomp: MENUE.PRG _vsStrcpy (494.c:561).
std::expected<std::size_t, PageTextError> CopyEncodedLine(
    std::span<const std::uint8_t> source, std::span<std::uint8_t> destination) {
  // Bytes are read and written in the original's order, so a refusal names
  // the first read or write that would leave its span.
  std::size_t read = 0;
  std::size_t written = 0;
  const auto copy_byte = [&]() -> std::expected<void, PageTextError> {
    if (read >= source.size()) {
      return std::unexpected(PageTextError::kUnterminatedLine);
    }
    if (written >= destination.size()) {
      return std::unexpected(PageTextError::kPageOverflow);
    }
    destination[written++] = source[read++];
    return {};
  };
  while (true) {
    if (read >= source.size()) {
      return std::unexpected(PageTextError::kUnterminatedLine);
    }
    const std::uint8_t code = source[read];
    if (code == kTextTerminator) {
      break;
    }
    if (code >= kTextControl) {
      const auto code_copied = copy_byte();
      if (!code_copied.has_value()) {
        return std::unexpected(code_copied.error());
      }
    }
    const auto copied = copy_byte();
    if (!copied.has_value()) {
      return std::unexpected(copied.error());
    }
  }
  if (written >= destination.size()) {
    return std::unexpected(PageTextError::kPageOverflow);
  }
  destination[written] = kTextNewline;
  return written + 1;
}

// decomp: MENUE.PRG _copyNextPageText (494.c:531).
std::expected<std::size_t, PageTextError> ComposePageText(
    std::span<const std::uint8_t> help_text, std::int32_t scroll_position,
    std::span<std::uint8_t, kPageTextCapacity> page) {
  page[0] = kTextChunkSize;
  page[1] = kTextNoChunking;
  page[2] = kTextFontTable;
  page[kPageHeaderBytes - 1] = kTextFontTableZero;
  page[kPageHeaderBytes] = kTextNewline;
  if (scroll_position < 0) {
    return std::unexpected(PageTextError::kNegativeScroll);
  }
  const auto line_count = HelpLineCount(help_text);
  if (!line_count.has_value()) {
    return std::unexpected(line_count.error());
  }
  // The original's addiu wraps, so a page starting near INT32_MAX is empty.
  const std::int32_t page_end =
      std::min(WrappingAdd(scroll_position, kLinesPerPage),
               static_cast<std::int32_t>(*line_count));
  std::size_t length = kPageHeaderBytes;
  for (std::int32_t line = scroll_position; line < page_end; ++line) {
    const std::size_t entry =
        kLineTableByte + (static_cast<std::size_t>(line + 1) * 2);
    if (entry + 2 > help_text.size()) {
      return std::unexpected(PageTextError::kLineOutsideText);
    }
    const std::size_t start =
        kLineTableByte + (std::size_t{ReadHalfword(help_text, entry)} * 2);
    if (start >= help_text.size()) {
      return std::unexpected(PageTextError::kLineOutsideText);
    }
    const auto copied = CopyEncodedLine(help_text.subspan(start),
                                        std::span(page).subspan(length));
    if (!copied.has_value()) {
      return std::unexpected(copied.error());
    }
    length += *copied;
  }
  page[length - 1] = kTextTerminator;
  return length;
}
}  // namespace VS::Menu::manual
