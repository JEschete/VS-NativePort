#include "Menu/Score/GazetteTimes.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Core/String/GameText.hpp"

namespace VS::Menu::Score {
namespace {
namespace text = Core::String;
namespace math = Core::Math;
constexpr std::int32_t kMinutesPerHour = 60;
constexpr std::int32_t kMaximumHours = 99;
constexpr std::uint8_t kRoundGlyph = 0x1B;
constexpr std::uint8_t kSpacingGlyph = 0xFA;
constexpr std::uint8_t kRoundSpacing = 12;
constexpr std::uint8_t kColonGlyph = 0x9E;
constexpr std::int32_t kDecimalBase = 10;
constexpr std::size_t kMinimumTimeDigits = 2;
constexpr int kOffscreenX = 320;
constexpr std::int16_t kTrialRowX = 194;
constexpr int kFirstTrialY = 50;
constexpr int kTrialRowHeight = 16;
constexpr int kTrialRowWidth = 136;
constexpr std::uint8_t kCancelledDelay = 255;
constexpr std::uint8_t kMaximumRoundIndex = 127;
constexpr std::int32_t kSettledStep = 8;
constexpr int kCursorX = 182;
constexpr int kCursorY = 42;
constexpr std::uint32_t kMissingColor = 0x505050;
constexpr std::uint32_t kRecordColor = 0x808080;
constexpr int kHeaderIndent = 120;
constexpr int kRankIndent = 104;
constexpr int kTimeIndent = 40;
constexpr int kRoundIndent = 60;
constexpr int kRecordSpacing = 30;
constexpr int kRoundY = 62;
constexpr int kHeadingY = 32;
constexpr int kReferenceHeadingY = 140;
constexpr int kReferenceTimeX = 88;
constexpr int kReferenceTimeY = 158;
constexpr int kHeadingPanelIndent = 128;
constexpr int kRecordPanelIndent = 112;
constexpr int kHeadingPanelWidth = 96;
constexpr int kReferenceHeadingWidth = 112;
constexpr int kRecordPanelWidth = 128;
constexpr int kPanelLineHeight = 12;
constexpr int kRecordPanelHeight = 84;
constexpr int kHeadingGradient = 1;
constexpr int kRecordGradient = 2;
constexpr std::string_view kMissingTime = "$--:--:--";
constexpr std::array<std::string_view, kTimeTrialPlaces> kPlaceNames{{
    "1ST",
    "2ND",
    "3RD",
}};
// decomp: _timeAttackReferenceTimes (src/MENU/MENU9.PRG/238.c), 80109878.
constexpr std::array<std::string_view, kTimeTrialCount> kReferenceTimes{{
    "$00:25:00",
    "$00:30:00",
    "$00:40:00",
    "$00:50:00",
    "$01:00:00",
    "$01:15:00",
    "$01:00:00",
    "$01:25:00",
}};

bool Terminated(TimeTrialText value) {
  return std::ranges::find(value, text::kGameTextTerminator) != value.end();
}

std::size_t WriteInteger(std::span<std::uint8_t> output, std::int32_t value,
                         bool pad) noexcept {
  const bool zero_prefix = pad && value >= 0 && value < kDecimalBase;
  if (zero_prefix) output.front() = 0;
  const std::size_t prefix = zero_prefix ? 1U : 0U;
  return prefix + text::FormatGameInteger(output.subspan(prefix), value) - 1;
}

std::string Decimal(std::uint8_t value, bool pad) {
  // Use the shared formatter, translating its digit glyphs back to ASCII.
  std::array<std::uint8_t, kFastestClearTimeBytes> glyphs{};
  const std::size_t length = text::FormatGameInteger(glyphs, value) - 1;
  std::string result;
  if (pad && length < kMinimumTimeDigits) result.push_back('0');
  for (std::size_t index = 0; index < length; ++index) {
    result.push_back(static_cast<char>('0' + glyphs[index]));
  }
  return result;
}

std::string RecordTime(const TimeTrialTime& time) {
  return "$" + Decimal(time.minutes, true) + ":" + Decimal(time.seconds, true) +
         ":" + Decimal(time.centiseconds, true);
}

bool ValidRows(std::span<const TimeTrialRow> rows,
               const TimeTrialScores& scores, std::size_t selected) {
  if (rows.size() < kTimeTrialCount || selected >= kTimeTrialCount) {
    return false;
  }
  for (const TimeTrialRecord& record : scores[selected]) {
    if (record.time && record.round_index > kMaximumRoundIndex) return false;
  }
  for (const TimeTrialRow& row : rows.first(kTimeTrialCount)) {
    if (row.entrance_delay == 1 && !Terminated(row.title)) return false;
  }
  return rows[selected].selected != 0 || Terminated(rows[selected].description);
}

std::size_t AnimateRows(std::span<TimeTrialRow> rows, TimeTrialHost& host) {
  std::size_t settled = 0;
  for (std::size_t index = 0; index < kTimeTrialCount; ++index) {
    TimeTrialRow& row = rows[index];
    if (row.entrance_delay != 0) {
      if (--row.entrance_delay == 0) {
        Framework::MenuRow& menu_row = host.CreateRow(
            {.row = static_cast<int>(index),
             .x = kOffscreenX,
             .y = kFirstTrialY + static_cast<int>(index) * kTrialRowHeight,
             .w = kTrialRowWidth},
            row.title);
        menu_row.unselectable = row.disabled;
        menu_row.state = Framework::RowState::kSlideX;
        menu_row.target0 = kTrialRowX;
      }
    } else if (host.Row(static_cast<TimeTrialId>(index)).state ==
               Framework::RowState::kIdle) {
      ++settled;
    }
  }
  return settled;
}

void SelectRow(std::span<TimeTrialRow> rows, std::size_t selected,
               TimeTrialAnimation& animation, TimeTrialHost& host) {
  for (std::size_t index = 0; index < kTimeTrialCount; ++index) {
    TimeTrialRow& row = rows[index];
    Framework::MenuRow& menu_row = host.Row(static_cast<TimeTrialId>(index));
    if (index == selected) {
      if (row.selected == 0) host.SetInformation(row.description);
      row.selected = 1;
      menu_row.selected = 1;
      animation.cursor_step = static_cast<std::uint8_t>(host.DrawCursor(
          animation.cursor_step, kCursorX,
          kCursorY + static_cast<int>(selected) * kTrialRowHeight));
    } else {
      row.selected = 0;
      menu_row.selected = 0;
    }
  }
}

void DrawRecords(const TimeTrialScores& scores, std::size_t selected,
                 std::int32_t offset, TimeTrialHost& host) {
  for (std::size_t place = 0; place < kTimeTrialPlaces; ++place) {
    const TimeTrialRecord& record = scores[selected][place];
    const int row_y = static_cast<int>(place) * kRecordSpacing;
    const int time_x = math::WrappingSubtract(offset, kTimeIndent);
    if (record.time) {
      host.DrawText(RecordTime(*record.time), time_x, kFirstTrialY + row_y,
                    kRecordColor);
      host.DrawText(
          "ROUND    " +
              Decimal(static_cast<std::uint8_t>(record.round_index + 1), false),
          math::WrappingSubtract(offset, kRoundIndent), kRoundY + row_y,
          kRecordColor);
    } else {
      host.DrawText(kMissingTime, time_x, kFirstTrialY + row_y, kMissingColor);
    }
  }
}

void DrawRecordPanels(const TimeTrialScores& scores, std::size_t selected,
                      std::int32_t offset, bool settled, bool disabled,
                      TimeTrialHost& host) {
  const int header_x = math::WrappingSubtract(offset, kHeaderIndent);
  const int heading_panel_x =
      math::WrappingSubtract(offset, kHeadingPanelIndent);
  const int record_panel_x = math::WrappingSubtract(offset, kRecordPanelIndent);
  host.DrawText("PLAYER TIME", header_x, kHeadingY, kRecordColor);
  host.DrawPanel(
      {heading_panel_x, kHeadingY, kHeadingPanelWidth, kPanelLineHeight},
      kHeadingGradient);
  for (std::size_t place = 0; place < kTimeTrialPlaces; ++place) {
    host.DrawText(
        kPlaceNames[place], math::WrappingSubtract(offset, kRankIndent),
        kFirstTrialY + static_cast<int>(place) * kRecordSpacing, kRecordColor);
  }
  DrawRecords(scores, selected, offset, host);
  host.DrawPanel(
      {record_panel_x, kFirstTrialY, kRecordPanelWidth, kRecordPanelHeight},
      kRecordGradient);
  host.DrawText("REFERENCE TIME", header_x, kReferenceHeadingY, kRecordColor);
  host.DrawPanel({heading_panel_x, kReferenceHeadingY, kReferenceHeadingWidth,
                  kPanelLineHeight},
                 kHeadingGradient);
  if (settled) {
    host.DrawText(disabled ? kMissingTime : kReferenceTimes[selected],
                  kReferenceTimeX, kReferenceTimeY,
                  disabled ? kMissingColor : kRecordColor);
  }
  host.DrawPanel(
      {record_panel_x, kReferenceTimeY, kRecordPanelWidth, kPanelLineHeight},
      kRecordGradient);
}
}  // namespace

// decomp: _printFastestClearTime (src/MENU/MENU9.PRG/238.c:2400),
// MENU9.PRG 80106DE0-80106F9C.
std::size_t FormatFastestClearTime(std::span<std::uint8_t> destination,
                                   const FastestClearTime& value,
                                   TimeTrialText no_clear) noexcept {
  if (value.clear_count == 0) return text::CopyGameText(destination, no_clear);
  std::int32_t hours = value.completion_minutes / kMinutesPerHour;
  std::int32_t minutes = value.completion_minutes % kMinutesPerHour;
  if (hours > kMaximumHours) {
    hours = kMaximumHours;
    minutes = kMinutesPerHour - 1;
  }
  std::array<std::uint8_t, kFastestClearTimeBytes> buffer{};
  const std::span output(buffer);
  std::size_t next = 0;
  buffer[next++] = kRoundGlyph;
  next += WriteInteger(output.subspan(next),
                       math::WrappingAdd(value.round_index, 1), false);
  buffer[next++] = kSpacingGlyph;
  buffer[next++] = kRoundSpacing;
  next += WriteInteger(output.subspan(next), hours, true);
  buffer[next++] = kColonGlyph;
  next += WriteInteger(output.subspan(next), minutes, true);
  buffer[next++] = text::kGameTextTerminator;
  if (destination.size() < next) return 0;
  std::ranges::copy(output.first(next), destination.begin());
  return next;
}

// decomp: _printBossRushMenuRow (src/MENU/MENU9.PRG/238.c:2461),
// MENU9.PRG 80106F9C-80107090.
bool CreateTimeTrialRows(std::span<TimeTrialRow> rows,
                         const TimeTrialScores& scores,
                         const TimeTrialLabels& labels) {
  if (rows.size() < kTimeTrialCount) return false;
  for (std::size_t index = 0; index < kTimeTrialCount; ++index) {
    const bool present = scores[index][0].time.has_value();
    if (!Terminated(present ? labels.titles[index] : labels.missing_title) ||
        !Terminated(present ? labels.descriptions[index]
                            : labels.missing_description)) {
      return false;
    }
  }
  for (std::size_t index = 0; index < kTimeTrialCount; ++index) {
    TimeTrialRow& row = rows[index];
    const bool present = scores[index][0].time.has_value();
    row.x = kTrialRowX;
    row.disabled = present ? 0 : 1;
    row.title = present ? labels.titles[index] : labels.missing_title;
    row.description =
        present ? labels.descriptions[index] : labels.missing_description;
    row.selected = 0;
    row.entrance_delay = static_cast<std::uint8_t>(index + 1);
    row.enabled_style = 1;
  }
  return true;
}

// decomp: _leaveBossRushMenu (src/MENU/MENU9.PRG/238.c:2488),
// MENU9.PRG 80107090-80107120.
bool LeaveTimeTrialMenu(std::span<TimeTrialRow> rows, TimeTrialHost& host) {
  if (rows.size() < kTimeTrialCount) {
    host.Refuse("time trial rows shorter than eight");
    return false;
  }
  for (std::size_t index = 0; index < kTimeTrialCount; ++index) {
    TimeTrialRow& row = rows[index];
    if (row.entrance_delay == 0) {
      Framework::MenuRow& menu_row = host.Row(static_cast<TimeTrialId>(index));
      menu_row.state = Framework::RowState::kSlideX;
      menu_row.target0 = kOffscreenX;
    } else {
      row.entrance_delay = kCancelledDelay;
    }
  }
  return true;
}

// decomp: _renderBossRushRecords (src/MENU/MENU9.PRG/238.c:2507),
// MENU9.PRG 80107120-80107600.
bool RenderTimeTrialRecords(std::span<TimeTrialRow> rows,
                            const TimeTrialScores& scores,
                            TimeTrialId selected_trial,
                            std::int32_t presentation_step,
                            TimeTrialAnimation& animation,
                            TimeTrialHost& host) {
  const auto selected = static_cast<std::size_t>(selected_trial);
  if (!ValidRows(rows, scores, selected)) {
    host.Refuse("invalid time trial rows, selection, text or round");
    return false;
  }
  const bool settled = AnimateRows(rows, host) == kTimeTrialCount &&
                       presentation_step == kSettledStep;
  if (settled) SelectRow(rows, selected, animation, host);
  DrawRecordPanels(scores, selected,
                   math::WrappingMultiply(presentation_step, kTrialRowHeight),
                   settled, rows[selected].disabled != 0, host);
  return true;
}
}  // namespace VS::Menu::Score
