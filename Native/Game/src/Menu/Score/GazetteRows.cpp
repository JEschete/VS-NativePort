// Hand-maintained (tools/port/owned.csv).
#include "Menu/Score/GazetteRows.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::Score {
namespace {
constexpr unsigned kHalfwordShift = 16;
constexpr std::uint32_t kLowHalfword = 0xFFFFU;
constexpr std::uint32_t kHighHalfword = 0xFFFF0000U;
constexpr std::uint32_t kOneLine = 1U << kHalfwordShift;

// The arrow: shown left of this x, 16 pixels square, 14 left of the row,
// bobbing one pixel per four frames; the rising arrow starts 5 higher.
constexpr int kArrowMaxX = 128;
constexpr int kArrowBrightness = 128;
constexpr int kArrowIndent = 14;
constexpr std::uint32_t kArrowSize = (16U << kHalfwordShift) | 16U;
constexpr unsigned kArrowPhaseShift = 2;
constexpr int kArrowRise = 5;
constexpr unsigned kArrowFlipShift = 4;
// vs_getUV0Clut(0, 48, 912, 223).
constexpr std::uint32_t kArrowTexture = 0x37F93000U;

// The glyphs start 6 right of the row; each carries the reversed feather
// flag in bit 31.
constexpr int kTextIndent = 6;
constexpr unsigned kReversedShift = 31;
constexpr std::uint8_t kTextEnd = 0xFF;
constexpr std::uint8_t kTextSpacing = 0xFA;

// The background lines: LINE_G2 semi-transparent between additive draw
// modes, dithered (open) then not (close); the fade spans 8 steps and
// the line brightness 32 to 120 in steps of 8.
constexpr std::uint32_t kLineOpenDrawMode = 0xE1000220U;
constexpr std::uint32_t kLineCloseDrawMode = 0xE1000020U;
constexpr std::uint32_t kLineCode = 0x52000000U;
constexpr std::uint32_t kFadeSteps = 8;
constexpr int kDimLine = 32;
constexpr int kBrightLine = 120;
constexpr int kLineBrightnessStep = 8;
// The shadow tile: 2 lines below the background, 2 lines tall.
constexpr std::uint32_t kShadowDrop = 2;
constexpr std::uint32_t kShadowHeight = 2U << kHalfwordShift;

// The reflection: one-line sprites sampling the frame buffer the row was
// drawn in (the other one's x), subtractive, 128 columns further right for
// a row at x 128 or more.
constexpr std::uint32_t kOtherFrameX = 320;
constexpr int kShineMinX = 128;
constexpr std::uint32_t kShineShift = 128;
constexpr unsigned kTexturePageShift = 6;
constexpr std::uint32_t kReflectionDrawMode = 0xE1000120U;
constexpr std::uint32_t kLowByte = 0xFFU;
constexpr unsigned kTextureVShift = 8;

[[nodiscard]] std::uint32_t Word(int value) {
  return static_cast<std::uint32_t>(value);
}

// The row's position word (x low, y high), as the original's lw.
[[nodiscard]] std::uint32_t RowPosition(const Framework::MenuRow& row) {
  return Word(static_cast<std::uint16_t>(row.x)) |
         (Word(row.y) << kHalfwordShift);
}

void AdvanceArrowAnimation(ScrollableRowAnimation& animation, int frame) {
  if (frame == animation.previous_frame_buffer) return;
  ++animation.step;
  animation.previous_frame_buffer = static_cast<std::uint8_t>(frame);
  if (animation.step >= kScrollArrowSteps) animation.step = 0;
}

void DrawArrow(ScrollableRowHost& host, const ScrollableRowAnimation& animation,
               const Framework::MenuRow& row, int scroll) {
  if (row.state != Framework::RowState::kIdle || row.x < 0 ||
      row.x >= kArrowMaxX) {
    return;
  }
  const int phase = animation.step >> kArrowPhaseShift;
  const int bob = scroll == 0 ? phase - kArrowRise : 1 - phase;
  // The original ORs the unmasked x - 14, so x below 14 fills the high
  // halfword (kept).
  host.DrawScrollArrow(
      {.brightness = kArrowBrightness,
       .position =
           Word(row.x - kArrowIndent) | (Word(row.y + bob) << kHalfwordShift),
       .size = kArrowSize,
       .texture = (Word(scroll) << kArrowFlipShift) | kArrowTexture});
}

void DrawGlyphs(ScrollableRowHost& host, const Framework::MenuRow& row,
                int scroll) {
  const std::uint32_t reversed = Word(scroll) << kReversedShift;
  int pen = row.x + kTextIndent;
  std::size_t i = 0;
  while (true) {
    if (i >= row.text.size()) {
      host.Refuse("scrollable row text without an end");
      return;
    }
    const std::uint8_t c = row.text[i++];
    if (c == kTextEnd) return;
    if (c == kTextSpacing) {
      if (i >= row.text.size()) {
        host.Refuse("scrollable row text without an end");
        return;
      }
      pen += row.text[i++];
    } else {
      pen = host.DrawGlyph(c | reversed, pen);
    }
  }
}

// The 12 gradient lines; returns the position below them.
[[nodiscard]] std::uint32_t DrawBackgroundLines(ScrollableRowHost& host,
                                                const Framework::MenuRow& row,
                                                int scroll, std::uint32_t xy) {
  const std::uint32_t w = row.w;
  const std::uint32_t fade = row.gradient_state;
  const std::uint32_t color_index = row.background_width;
  for (int i = 0; i < kRowBackgroundLines; ++i) {
    const int level = scroll == 0 ? (i * kLineBrightnessStep) + kDimLine
                                  : kBrightLine - (i * kLineBrightnessStep);
    const std::uint32_t first =
        host.GradientStopColor(kFadeSteps - fade, color_index, level);
    const std::uint32_t second =
        host.GradientStopColor(fade, color_index, level);
    host.AddGradientLine({kLineOpenDrawMode, first | kLineCode, xy, second,
                          ((xy + w - 1) & kLowHalfword) | (xy & kHighHalfword),
                          kLineCloseDrawMode});
    xy += kOneLine;
  }
  return xy;
}

void DrawReflection(ScrollableRowHost& host, const Framework::MenuRow& row,
                    int scroll, int frame, std::uint32_t xy) {
  const std::uint32_t size = row.w | kOneLine;
  const std::uint32_t shine = row.x >= kShineMinX ? kShineShift : 0;
  const std::uint32_t page = frame == 0 ? kOtherFrameX : 0;
  const std::uint32_t draw_mode =
      ((page + shine) >> kTexturePageShift) | kReflectionDrawMode;
  for (int i = 0; i < kRowBackgroundLines; ++i) {
    host.AddReflection(
        {.brightness = scroll == 0 ? kBrightLine - (i * kLineBrightnessStep)
                                   : (i * kLineBrightnessStep) + kDimLine,
         .position = xy,
         .size = size,
         .draw_mode = draw_mode,
         .texture = ((xy - shine) & kLowByte) |
                    ((xy >> kHalfwordShift) << kTextureVShift)});
    xy += kOneLine;
  }
}

std::vector<std::int32_t> PlainRowTypes(std::int32_t row_count) {
  return std::vector<std::int32_t>(static_cast<std::size_t>(row_count), 0);
}
}  // namespace

// decomp: MENU9.PRG _renderScrollableMenuItem
// (src/MENU/MENU9.PRG/238.c:54), 80102A7C
void RenderScrollableRow(ScrollableRowHost& host,
                         ScrollableRowAnimation& animation,
                         Framework::MenuRow& row) {
  const int frame = host.FrameBufferIndex();
  const int scroll = row.is_scrollable - 1;
  AdvanceArrowAnimation(animation, frame);
  DrawArrow(host, animation, row, scroll);
  DrawGlyphs(host, row, scroll);
  if (row.w == 0) return;
  const std::uint32_t below =
      DrawBackgroundLines(host, row, scroll, RowPosition(row));
  if (scroll == 0) {
    host.AddShadowTile(
        ((below + kShadowDrop) & kLowHalfword) | (below & kHighHalfword),
        row.w | kShadowHeight);
  }
  if (row.gradient_state != 0) --row.gradient_state;
  DrawReflection(host, row, scroll, frame,
                 below - (Word(kRowBackgroundLines) << kHalfwordShift));
}

TopMenuCursor TopMenuCursorOf(std::int32_t row_count,
                              std::int32_t absolute_row) {
  if (row_count <= kTopMenuVisibleRows || absolute_row < kTopMenuVisibleRows) {
    return {.row = static_cast<std::uint8_t>(absolute_row), .first_row = 0};
  }
  const std::int32_t last_first_row = row_count - (kTopMenuVisibleRows + 1);
  if (absolute_row >= row_count - kTopMenuVisibleRows) {
    return {.row = static_cast<std::uint8_t>(absolute_row - last_first_row),
            .first_row = static_cast<std::uint8_t>(last_first_row)};
  }
  return {
      .row = static_cast<std::uint8_t>(kTopMenuMiddleRow),
      .first_row = static_cast<std::uint8_t>(absolute_row - kTopMenuMiddleRow)};
}

// decomp: MENU9.PRG _setRows (src/MENU/MENU9.PRG/238.c:196), 80102E10
void RestoreTopMenuCursor(GazetteMenuHost& host, std::int32_t row_count,
                          std::int32_t absolute_row) {
  if (row_count < 0 || row_count > kMaxTopMenuRows) {
    host.Refuse("top menu row count outside 0-256");
    return;
  }
  const std::vector<std::int32_t> row_types = PlainRowTypes(row_count);
  host.SetTopMenuCursor(TopMenuCursorOf(row_count, absolute_row));
  const std::uint8_t setting = host.CursorMemorySetting();
  host.SetCursorMemorySetting(1);
  host.ShowRows(kTopMenuRowInfo, row_types);
  host.SetCursorMemorySetting(setting);
}

// decomp: MENU9.PRG _getSelectedRow (src/MENU/MENU9.PRG/238.c:228), 80102F0C
std::int32_t GetSelectedTopMenuRow(GazetteMenuHost& host) {
  const std::int32_t row = host.SelectedRow();
  if (row < kTopMenuPending) {
    host.ClearMenuExcept(kTopMenuKeptRow);
  } else if (row >= 0) {
    host.FlyOutAndHoist(host.TopMenuCursorRow(), 1);
  }
  return row;
}

// decomp: MENU9.PRG _topMenu (src/MENU/MENU9.PRG/238.c:1430), 80104BD0
std::int32_t UpdateTopMenu(GazetteMenuHost& host, TopMenuState& state,
                           std::int32_t initialize) {
  if (initialize != 0) state = {};
  switch (state.step) {
    case 0:
      host.ListTopMenuChoices();
      RestoreTopMenuCursor(host, kTopMenuChoiceCount, host.GazetteRow());
      ++state.step;
      break;
    case 1:
      state.selected = GetSelectedTopMenuRow(host);
      if (state.selected != kTopMenuPending) return state.selected;
      break;
    default:
      break;
  }
  return kTopMenuPending;
}

// decomp: MENU9.PRG _clearMenuExceptRow8 (src/MENU/MENU9.PRG/238.c:258),
// 80102FB8
void ClearMenuExceptRow8(GazetteMenuHost& host) {
  host.ClearMenuExcept(kUnusedKeptRow);
}

// decomp: MENU9.PRG _menuReady (src/MENU/MENU9.PRG/238.c:264), 80102FD8
void PollMenuReady(GazetteMenuHost& host) {
  static_cast<void>(host.MenuRowsIdle());
}
}  // namespace VS::Menu::Score
