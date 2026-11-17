#include "Menu/Score/GazetteStatistics.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "Core/Math/ScalarMath.hpp"
#include "Core/String/GameText.hpp"
#include "Menu/Score/GazetteTimes.hpp"

namespace VS::Menu::Score {
namespace {
namespace math = Core::Math;
namespace text = Core::String;
constexpr std::int16_t kTitleX = 60;
constexpr std::int16_t kStatisticX = 16;
constexpr int kTitleWidth = 200;
constexpr int kStatisticWidth = 142;
constexpr int kFirstRowY = 56;
constexpr int kRowHeight = 16;
constexpr int kPresentationStride = 23;
constexpr int kScreenHeight = 240;
constexpr int kCursorInset = 12;
constexpr int kCursorRaise = 8;
constexpr std::uint8_t kSettledStep = 8;
constexpr std::uint8_t kLockedColor = 3;
constexpr std::uint8_t kScrollUp = 1;
constexpr std::uint8_t kScrollDown = 2;
constexpr int kNumberInset = 14;
constexpr std::size_t kStatisticTextBytes = 32;
constexpr std::size_t kAlignmentBytes = 2;
constexpr int kValueRightEdge = 288;
constexpr int kValueOriginBias = 114;
constexpr int kGlyphWidth = 6;
constexpr std::uint8_t kSpacingCode = 0xFA;
constexpr std::uint32_t kPercentScale = 100;
constexpr std::uint32_t kChestCount = 52;
constexpr std::uint32_t kRoomCount = 361;
constexpr std::size_t kFirstEnemyStatistic = 7;
constexpr std::size_t kFirstWeaponStatistic = 13;
constexpr int kMaximumTitleNumber = 99;
constexpr int kDecimalBase = 10;
constexpr int kNumberRightOffset = 19;
constexpr int kDigitAdvance = 5;
constexpr int kDigitWidth = 6;
constexpr int kDigitHeight = 10;
constexpr int kDigitRaise = 4;
constexpr std::uint16_t kDigitPage = 0x0C;
constexpr std::uint16_t kDimmedDigitPage = 0x2C;
constexpr std::uint16_t kDigitClut = 0x37F4;
constexpr std::uint16_t kDimmedDigitClut = 0x37F7;
constexpr std::uint8_t kDigitBrightness = 128;
constexpr std::uint8_t kDimmedDigitBrightness = 32;

struct StatisticText {
  std::array<std::uint8_t, kStatisticTextBytes> bytes{};
  std::size_t size = 0;
};

enum class SummaryStatistic : std::size_t {
  kClearCount,
  kFastestClear,
  kOpenedChests,
  kBestOpenedChests,
  kMapCompletion,
  kBestMapCompletion,
  kMaxChain,
};

bool Terminated(GazetteText value) {
  return std::ranges::find(value, text::kGameTextTerminator) != value.end();
}

bool ValidList(std::span<const GazetteRecordRow> rows, std::size_t count,
               GazetteRecordCursor cursor) {
  if (rows.size() < count ||
      cursor.first_row > count - kGazetteVisibleRecords) {
    return false;
  }
  for (const auto& row :
       rows.subspan(cursor.first_row, kGazetteVisibleRecords)) {
    if (!Terminated(row.title)) return false;
  }
  const std::size_t selected =
      static_cast<std::size_t>(cursor.first_row) + cursor.selected_row;
  return selected >= count || rows[selected].selected != 0 ||
         Terminated(rows[selected].description);
}

void SelectRows(std::span<GazetteRecordRow> rows, GazetteRecordCursor cursor,
                bool titles, GazetteRecordHost& host) {
  const std::size_t selected =
      static_cast<std::size_t>(cursor.first_row) + cursor.selected_row;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    auto& row = rows[index];
    if (index == selected) {
      if (row.selected == 0) host.SetInformation(row.description);
      row.selected = 1;
      if (!titles || row.unlocked != 0) row.highlight_step = kSettledStep;
    } else {
      row.selected = 0;
      if (row.highlight_step != 0) --row.highlight_step;
    }
  }
}

void Cursor(const GazetteRecordRow& row, std::int32_t step, int y,
            GazetteRecordAnimation& animation, GazetteRecordHost& host) {
  if (row.selected != 0 && step == kSettledStep) {
    animation.cursor_step = static_cast<std::uint8_t>(
        host.DrawCursor(animation.cursor_step, row.x - kCursorInset,
                        math::WrappingSubtract(y, kCursorRaise)));
  }
}

void ScrollFlags(Framework::MenuRow& row, std::size_t visible,
                 std::uint8_t first, std::size_t count) {
  if (visible == 0 && first != 0) row.is_scrollable = kScrollUp;
  if (visible == kGazetteVisibleRecords - 1 &&
      first != count - kGazetteVisibleRecords) {
    row.is_scrollable = kScrollDown;
  }
}

std::size_t FormatStatistic(std::span<std::uint8_t> output, std::size_t index,
                            const GazetteStatisticsValues& values) {
  switch (index) {
    case std::to_underlying(SummaryStatistic::kClearCount):
      return text::FormatGameInteger(output, values.current.clear_count);
    case std::to_underlying(SummaryStatistic::kFastestClear):
      return FormatFastestClearTime(
          output,
          {values.current.clear_count,
           std::bit_cast<std::int32_t>(values.best_clear_round),
           std::bit_cast<std::int32_t>(values.best.completion_time_minutes)},
          values.no_clear);
    case std::to_underlying(SummaryStatistic::kOpenedChests):
      return text::FormatGamePercent(
          output, static_cast<std::int32_t>(values.current.opened_chest_count *
                                            kPercentScale / kChestCount));
    case std::to_underlying(SummaryStatistic::kBestOpenedChests):
      return text::FormatGamePercent(
          output, static_cast<std::int32_t>(values.best.opened_chest_count *
                                            kPercentScale / kChestCount));
    case std::to_underlying(SummaryStatistic::kMapCompletion):
      return text::FormatGamePercent(
          output, static_cast<std::int32_t>(values.current.map_completion *
                                            kPercentScale / kRoomCount));
    case std::to_underlying(SummaryStatistic::kBestMapCompletion):
      return text::FormatGamePercent(
          output, static_cast<std::int32_t>(values.best.map_completion *
                                            kPercentScale / kRoomCount));
    case std::to_underlying(SummaryStatistic::kMaxChain):
      return text::FormatGameInteger(output, values.best.max_chain);
    default:
      if (index < kFirstWeaponStatistic) {
        return text::FormatGameInteger(
            output, values.best.enemy_kills[index - kFirstEnemyStatistic]);
      }
      // The weapon list is classes 1..9, followed by class 0 (bare hands).
      return text::FormatGameInteger(
          output,
          values.best.weapon_attacks[(index - kFirstWeaponStatistic + 1) %
                                     kScoreWeaponClassCount]);
  }
}

void DrawStatisticValue(const GazetteRecordRow& row, StatisticText& value,
                        int y, GazetteRecordCursor cursor, std::size_t visible,
                        std::int32_t step, GazetteRecordHost& host) {
  const auto content =
      std::span(value.bytes).subspan(kAlignmentBytes, value.size);
  const int left = math::WrappingSubtract(
      kValueRightEdge,
      math::WrappingMultiply(host.MeasureText(content), kGlyphWidth));
  value.bytes[0] = kSpacingCode;
  value.bytes[1] = static_cast<std::uint8_t>(
      math::WrappingSubtract(left, row.x - kValueOriginBias));
  auto& menu = host.CreateRow(
      {.x = row.x + kStatisticWidth, .y = y, .w = kStatisticWidth},
      std::span(value.bytes).first(value.size + kAlignmentBytes));
  menu.gradient_state =
      static_cast<std::uint8_t>(kSettledStep - row.highlight_step);
  ScrollFlags(menu, visible, cursor.first_row, kGazetteStatisticCount);
  host.DrawRow(menu.is_scrollable != 0 && step == kSettledStep);
}
}  // namespace

// decomp: _setTitleRows (src/MENU/MENU9.PRG/238.c:1722),
// MENU9.PRG 801056B8-8010579C.
bool CreateTitleRows(std::span<GazetteRecordRow> rows, std::uint32_t titles,
                     const GazetteTitleLabels& labels) {
  if (rows.size() < kGazetteTitleCount) return false;
  for (std::size_t index = 0; index < kGazetteTitleCount; ++index) {
    const bool unlocked = (titles & (std::uint32_t{1} << index)) != 0;
    if (!Terminated(unlocked ? labels.titles[index] : labels.locked_title) ||
        !Terminated(unlocked ? labels.descriptions[index]
                             : labels.locked_description)) {
      return false;
    }
  }
  for (std::size_t index = 0; index < kGazetteTitleCount; ++index) {
    const bool unlocked = (titles & (std::uint32_t{1} << index)) != 0;
    rows[index] = {
        .unlocked = static_cast<std::uint8_t>(unlocked),
        .x = kTitleX,
        .number = static_cast<std::int16_t>(index + 1),
        .title = unlocked ? labels.titles[index] : labels.locked_title,
        .description =
            unlocked ? labels.descriptions[index] : labels.locked_description};
  }
  return true;
}

// decomp: _renderTitleMenu (src/MENU/MENU9.PRG/238.c:1756),
// MENU9.PRG 8010579C-80105A0C.
bool RenderTitles(std::span<GazetteRecordRow> rows, GazetteRecordCursor cursor,
                  std::int32_t presentation_step,
                  GazetteRecordAnimation& animation, GazetteRecordHost& host) {
  if (!ValidList(rows, kGazetteTitleCount, cursor)) {
    host.Refuse("invalid title rows, page or text");
    return false;
  }
  SelectRows(rows.first(kGazetteTitleCount), cursor, true, host);
  const int entrance = math::WrappingSubtract(
      math::WrappingMultiply(presentation_step, kPresentationStride),
      kRowHeight);
  for (std::size_t visible = 0; visible < kGazetteVisibleRecords; ++visible) {
    const auto& row = rows[cursor.first_row + visible];
    const int y =
        std::min(entrance, kFirstRowY + static_cast<int>(visible) * kRowHeight);
    Cursor(row, presentation_step, y, animation, host);
    auto& menu =
        host.CreateRow({.x = row.x, .y = y, .w = kTitleWidth}, row.title);
    if (row.unlocked == 0) menu.font_color = kLockedColor;
    menu.gradient_state = row.highlight_step;
    ScrollFlags(menu, visible, cursor.first_row, kGazetteTitleCount);
    host.DrawNumber({row.x - kNumberInset, y, row.number,
                     (menu.is_scrollable & kScrollUp) != 0});
    host.DrawRow(menu.is_scrollable != 0 && presentation_step == kSettledStep);
  }
  host.Row().state = Framework::RowState::kInactive;
  return true;
}

// decomp: _renderTitleNo (src/MENU/MENU9.PRG/238.c:1835),
// MENU9.PRG 80105A0C-80105BCC. The bounded decimal replaces sprintf (#65).
bool DrawTitleNumber(const TitleNumber& number, TitleNumberTarget& target) {
  if (number.value < 0 || number.value > kMaximumTitleNumber) return false;
  int x = math::WrappingAdd(number.x, kNumberRightOffset);
  const auto shade = number.dimmed ? kDimmedDigitBrightness : kDigitBrightness;
  const std::array digits{number.value % kDecimalBase,
                          number.value / kDecimalBase};
  for (const int digit : digits) {
    const Render::UiTexturedQuad quad{
        .top_left = {math::WrapSigned16(static_cast<std::uint32_t>(x)),
                     math::WrapSigned16(static_cast<std::uint32_t>(
                         math::WrappingSubtract(number.y, kDigitRaise)))},
        .bottom_right = {math::WrapSigned16(static_cast<std::uint32_t>(
                             math::WrappingAdd(x, kDigitWidth))),
                         math::WrapSigned16(
                             static_cast<std::uint32_t>(math::WrappingAdd(
                                 number.y, kDigitHeight - kDigitRaise)))},
        .texture = {.u = static_cast<std::uint8_t>(digit * kDigitWidth),
                    .width = kDigitWidth,
                    .height = kDigitHeight,
                    .texture_page =
                        number.dimmed ? kDimmedDigitPage : kDigitPage,
                    .clut = number.dimmed ? kDimmedDigitClut : kDigitClut},
        .tint = Render::QuadColor{shade, shade, shade},
        .raw_texture = false,
        .translucent = true};
    if (!target.DrawQuad(quad)) return false;
    x = math::WrappingSubtract(x, kDigitAdvance);
  }
  return true;
}

// decomp: _setStatText (src/MENU/MENU9.PRG/238.c:2201),
// MENU9.PRG 80106780-80106808.
bool CreateStatisticRows(std::span<GazetteRecordRow> rows,
                         const GazetteStatisticLabels& labels) {
  if (rows.size() < kGazetteStatisticCount) return false;
  for (std::size_t index = 0; index < kGazetteStatisticCount; ++index) {
    if (!Terminated(labels.titles[index]) ||
        !Terminated(labels.descriptions[index])) {
      return false;
    }
  }
  for (std::size_t index = 0; index < kGazetteStatisticCount; ++index) {
    rows[index] = {.unlocked = 1,
                   .x = kStatisticX,
                   .number = static_cast<std::int16_t>(index + 1),
                   .title = labels.titles[index],
                   .description = labels.descriptions[index]};
  }
  return true;
}

// decomp: _renderBossRushDetails (src/MENU/MENU9.PRG/238.c:2226),
// MENU9.PRG 80106808-80106CEC. This is the general statistics page.
bool RenderStatistics(std::span<GazetteRecordRow> rows,
                      const GazetteStatisticsValues& values,
                      GazetteRecordCursor cursor,
                      std::int32_t presentation_step,
                      GazetteRecordAnimation& animation,
                      GazetteRecordHost& host) {
  if (!ValidList(rows, kGazetteStatisticCount, cursor)) {
    host.Refuse("invalid statistic rows, page or text");
    return false;
  }
  std::array<StatisticText, kGazetteVisibleRecords> formatted{};
  for (std::size_t visible = 0; visible < formatted.size(); ++visible) {
    auto& value = formatted[visible];
    value.size =
        FormatStatistic(std::span(value.bytes).subspan(kAlignmentBytes),
                        cursor.first_row + visible, values);
    if (value.size == 0) {
      host.Refuse("statistic value exceeds its row or has no end");
      return false;
    }
  }
  SelectRows(rows.first(kGazetteStatisticCount), cursor, false, host);
  const int entrance = math::WrappingSubtract(
      kScreenHeight,
      math::WrappingMultiply(presentation_step, kPresentationStride));
  for (std::size_t visible = 0; visible < kGazetteVisibleRecords; ++visible) {
    const auto& row = rows[cursor.first_row + visible];
    const int y =
        std::max(entrance, kFirstRowY + static_cast<int>(visible) * kRowHeight);
    Cursor(row, presentation_step, y, animation, host);
    auto& menu =
        host.CreateRow({.x = row.x, .y = y, .w = kStatisticWidth}, row.title);
    menu.gradient_state = row.highlight_step;
    ScrollFlags(menu, visible, cursor.first_row, kGazetteStatisticCount);
    host.DrawRow(menu.is_scrollable != 0 && presentation_step == kSettledStep);
    DrawStatisticValue(row, formatted[visible], y, cursor, visible,
                       presentation_step, host);
  }
  host.Row().state = Framework::RowState::kInactive;
  return true;
}
}  // namespace VS::Menu::Score
