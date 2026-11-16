// Hand-maintained (tools/port/owned.csv).
// The manual page's frame: content clipping, the page background, the
// scroll fades, the page arrows and the control hint boxes (MENUE.PRG
// 494.c:577-616, 716-881, 962-987).
#include "Menu/ManualPage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Render/DisplayControl.hpp"

namespace VS::Menu::manual {
namespace {
using Core::Math::WrappingAdd;
using Core::Math::WrappingSubtract;

// GPU command codes (setPolyG4, setPolyFT4, setTile, setLineG2, setPolyF4).
constexpr std::uint32_t kGradientQuadCode = 0x38;
constexpr std::uint32_t kTexturedQuadCode = 0x2C;
constexpr std::uint32_t kTileCode = 0x60;
constexpr std::uint32_t kGradientLineCode = 0x50;
constexpr std::uint32_t kFlatQuadCode = 0x28;
// setShadeTex and setSemiTrans.
constexpr std::uint32_t kRawTextureFlag = 0x01;
constexpr std::uint32_t kSemiTransparentFlag = 0x02;
// _insertTPage (insertTPage.h): a dithered draw mode.
constexpr std::uint32_t kTexturePageCommand = 0xE1000200;
constexpr std::uint32_t kTexturePageMask = 0x1FF;
constexpr int kCommandShift = 24;
constexpr int kGreenShift = 8;
constexpr int kBlueShift = 16;
constexpr int kHighHalfShift = 16;

// _colorStops0 and _colorStops1 (494.c): each gradient's left and right
// colours.
constexpr std::array<PageColor, kPageGradients> kGradientLeft{{
    {0x00, 0x20, 0x50},
    {0x19, 0x82, 0x6C},
    {0x40, 0x30, 0x66},
    {0x40, 0x38, 0x20},
}};
constexpr std::array<PageColor, kPageGradients> kGradientRight{{
    {0x00, 0x05, 0x33},
    {0x01, 0x28, 0x26},
    {0x08, 0x08, 0x20},
    {0x10, 0x10, 0x08},
}};
// getTPage(0, 3, 0, 0) and getTPage(0, 2, 0, 0): subtractive and additive
// blending at page (0, 0).
constexpr std::uint16_t kSubtractivePage = 0x60;
constexpr std::uint16_t kAdditivePage = 0x40;

// The page arrows: 16x16 at x 152, from (type * 16, 48) of the 4-bit page
// at (768, 0), with the CLUT at (896, 223) while Square was held, otherwise
// (912, 223).
constexpr std::int32_t kArrowLeft = 152;
constexpr std::int32_t kArrowSize = 16;
constexpr std::int32_t kArrowTexelTop = 48;
constexpr std::uint16_t kArrowPage = 0x000C;
constexpr std::uint16_t kArrowDimClut = 0x37F8;
constexpr std::uint16_t kArrowClut = 0x37F9;
constexpr std::int32_t kArrowBounceFrames = 9;
constexpr std::int32_t kDownArrowTop = 184;
constexpr std::int32_t kDownArrowReturn = 190;
constexpr std::int32_t kUpArrowTop = 40;
constexpr std::int32_t kUpArrowReturn = 34;
constexpr int kArrowRiseShift = 1;
constexpr int kArrowFallShift = 2;

// The fades redraw the background from the 8-bit pages at x 640 and 704
// with the CLUT at (768, 227). The third chunk's page is (j + 32) | 0x9A, so
// it reuses x 640 rather than 768: the original ORs the chunk into the page
// bits instead of adding it (preserved).
constexpr std::uint16_t kFadeClut = 0x38F0;
constexpr std::uint16_t kFadePageBits = 0x9A;
constexpr std::uint16_t kFadePageFirst = 32;
constexpr int kFadeChunkShift = 7;
constexpr std::int32_t kUpperFadeTop = 52;
constexpr std::int32_t kLowerFadeTop = 187;
constexpr std::int16_t kFadeTileWidth = 320;
constexpr std::int16_t kFadeTileHeight = 1;

// _drawControlsBg: the shading runs from (64, 56, 32) to (16, 16, 8) and the
// shadow quad sits 2 pixels down and right.
constexpr PageColor kControlsLeft{64, 56, 32};
constexpr PageColor kControlsRight{16, 16, 8};
constexpr std::int32_t kControlsShadowOffset = 2;

std::uint32_t Rgb(const PageColor& color, std::uint32_t code) noexcept {
  return static_cast<std::uint32_t>(color.red) |
         (static_cast<std::uint32_t>(color.green) << kGreenShift) |
         (static_cast<std::uint32_t>(color.blue) << kBlueShift) |
         (code << kCommandShift);
}

std::uint32_t Xy(const PagePoint& point) noexcept {
  return (static_cast<std::uint32_t>(static_cast<std::uint16_t>(point.x))) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(point.y))
          << kHighHalfShift);
}

std::uint32_t Uv(const PageTexel& texel, std::uint16_t high) noexcept {
  return static_cast<std::uint32_t>(texel.u) |
         (static_cast<std::uint32_t>(texel.v) << kGreenShift) |
         (static_cast<std::uint32_t>(high) << kHighHalfShift);
}

// The original's int coordinates, stored as halfwords.
PagePoint Point(std::int32_t x, std::int32_t y) noexcept {
  return {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)};
}

PageColor Grey(std::uint8_t level) noexcept { return {level, level, level}; }

// One chunk of a fade row: rows `top` and `top + 1` of the page background,
// with v equal to the screen row.
TexturedQuad FadeChunk(std::int32_t chunk, std::int32_t top,
                       std::uint8_t level) {
  const std::int32_t left = chunk << kFadeChunkShift;
  const std::int32_t right =
      left + kScrollFadeChunkWidths.at(static_cast<std::size_t>(chunk));
  const auto u = static_cast<std::uint8_t>(
      kScrollFadeChunkWidths.at(static_cast<std::size_t>(chunk)));
  const auto v_top = static_cast<std::uint8_t>(top);
  const auto v_bottom = static_cast<std::uint8_t>(top + 1);
  return {
      .corners = {Point(left, top), Point(right, top), Point(left, top + 1),
                  Point(right, top + 1)},
      .texels = {{{0, v_top}, {u, v_top}, {0, v_bottom}, {u, v_bottom}}},
      .color = Grey(level),
      .clut = kFadeClut,
      .texture_page = static_cast<std::uint16_t>(
          (static_cast<std::uint16_t>(chunk) + kFadePageFirst) | kFadePageBits),
      .raw_texture = false,
      .semi_transparent = true};
}

// `row_top(i)` gives the screen row of fade row i, row 0 the strongest.
template <typename RowTop>
void DrawScrollFade(PageArrow arrow, const PageArrowState& state,
                    RowTop row_top, PagePrimitiveTarget& target) {
  DrawPageArrow(arrow, state, target);
  for (std::int32_t row = 0; row < kScrollFadeRows; ++row) {
    for (std::int32_t chunk = 0; chunk < kScrollFadeChunks; ++chunk) {
      target.DrawTexturedQuad(
          kPageFadeSlot,
          FadeChunk(chunk, row_top(row),
                    kScrollFadeLevels.at(static_cast<std::size_t>(row))));
    }
  }
  for (std::int32_t row = 0; row < kScrollFadeRows; ++row) {
    target.DrawTile(
        kPageFadeSlot,
        {.position = Point(0, row_top(row)),
         .width = kFadeTileWidth,
         .height = kFadeTileHeight,
         .color = Grey(kScrollFadeLevels.at(static_cast<std::size_t>(row))),
         .semi_transparent = true});
  }
  target.SetTexturePage(kPageFadeSlot, kAdditivePage);
}
}  // namespace

std::array<std::uint32_t, kGradientQuadWords> PackGradientQuad(
    const GradientQuad& quad) noexcept {
  return {Rgb(quad.colors[0], kGradientQuadCode),
          Xy(quad.corners[0]),
          Rgb(quad.colors[1], 0),
          Xy(quad.corners[1]),
          Rgb(quad.colors[2], 0),
          Xy(quad.corners[2]),
          Rgb(quad.colors[kBottomRight], 0),
          Xy(quad.corners[kBottomRight])};
}

std::array<std::uint32_t, kTexturedQuadWords> PackTexturedQuad(
    const TexturedQuad& quad) noexcept {
  std::uint32_t code = kTexturedQuadCode;
  if (quad.raw_texture) {
    code |= kRawTextureFlag;
  }
  if (quad.semi_transparent) {
    code |= kSemiTransparentFlag;
  }
  return {Rgb(quad.color, code),
          Xy(quad.corners[0]),
          Uv(quad.texels[0], quad.clut),
          Xy(quad.corners[1]),
          Uv(quad.texels[1], quad.texture_page),
          Xy(quad.corners[2]),
          Uv(quad.texels[2], 0),
          Xy(quad.corners[kBottomRight]),
          Uv(quad.texels[kBottomRight], 0)};
}

std::array<std::uint32_t, kPageTileWords> PackPageTile(
    const PageTile& tile) noexcept {
  const std::uint32_t code =
      tile.semi_transparent ? kTileCode | kSemiTransparentFlag : kTileCode;
  return {Rgb(tile.color, code), Xy(tile.position),
          Xy({tile.width, tile.height})};
}

std::array<std::uint32_t, kGradientLineWords> PackGradientLine(
    const GradientLine& line) noexcept {
  return {Rgb(line.from_color, kGradientLineCode), Xy(line.from),
          Rgb(line.to_color, 0), Xy(line.to)};
}

std::array<std::uint32_t, kFlatQuadWords> PackFlatQuad(
    const FlatQuad& quad) noexcept {
  return {Rgb(quad.color, kFlatQuadCode), Xy(quad.corners[0]),
          Xy(quad.corners[1]), Xy(quad.corners[2]),
          Xy(quad.corners[kBottomRight])};
}

std::array<std::uint32_t, kTexturePageWords> PackTexturePage(
    std::uint16_t texture_page) noexcept {
  // The original's third word, a GPU no-op, stays in the packet.
  return {kTexturePageCommand | (texture_page & kTexturePageMask), 0};
}

// decomp: MENUE.PRG _setPageBg (494.c:716)
PageDrawResult DrawPageBackground(const PageRect& rect, std::int32_t gradient,
                                  PagePrimitiveTarget& target) {
  if (gradient < 0 || gradient >= kPageGradients) {
    return PageDrawResult::kRefused;
  }
  const std::int32_t right =
      WrappingSubtract(WrappingAdd(rect.x, rect.width), 1);
  const std::int32_t bottom =
      WrappingSubtract(WrappingAdd(rect.y, rect.height), 1);
  const PageColor left_color =
      kGradientLeft.at(static_cast<std::size_t>(gradient));
  const PageColor right_color =
      kGradientRight.at(static_cast<std::size_t>(gradient));
  target.DrawGradientQuad(
      kPagePanelSlot,
      {.corners = {Point(rect.x, rect.y), Point(right, rect.y),
                   Point(rect.x, bottom), Point(right, bottom)},
       .colors = {left_color, right_color, left_color, right_color}});
  target.SetTexturePage(kPagePanelSlot, kSubtractivePage);
  return PageDrawResult::kDrawn;
}

std::int32_t PageArrowTop(PageArrow arrow,
                          std::int32_t animation_state) noexcept {
  // Arithmetic shifts of the signed counter, as the original.
  if (arrow != PageArrow::kUp) {
    return animation_state < kArrowBounceFrames
               ? WrappingAdd(animation_state >> kArrowRiseShift, kDownArrowTop)
               : kDownArrowReturn - (animation_state >> kArrowFallShift);
  }
  return animation_state < kArrowBounceFrames
             ? kUpArrowTop - (animation_state >> kArrowRiseShift)
             : (animation_state >> kArrowFallShift) + kUpArrowReturn;
}

// decomp: MENUE.PRG _drawPaginationArrow (494.c:844)
void DrawPageArrow(PageArrow arrow, const PageArrowState& state,
                   PagePrimitiveTarget& target) {
  const std::int32_t top = PageArrowTop(arrow, state.animation_state);
  const std::int32_t bottom = top + kArrowSize;
  const std::int32_t right = kArrowLeft + kArrowSize;
  const auto u =
      static_cast<std::uint8_t>(static_cast<std::int32_t>(arrow) * kArrowSize);
  const auto u_right = static_cast<std::uint8_t>(u + kArrowSize);
  constexpr auto kTexelBottom =
      static_cast<std::uint8_t>(kArrowTexelTop + kArrowSize);
  constexpr auto kTexelTop = static_cast<std::uint8_t>(kArrowTexelTop);
  target.DrawTexturedQuad(
      kPageFadeSlot,
      {.corners = {Point(kArrowLeft, top), Point(right, top),
                   Point(kArrowLeft, bottom), Point(right, bottom)},
       .texels = {{{u, kTexelTop},
                   {u_right, kTexelTop},
                   {u, kTexelBottom},
                   {u_right, kTexelBottom}}},
       .color = {},
       .clut = state.square_held ? kArrowDimClut : kArrowClut,
       .texture_page = kArrowPage,
       .raw_texture = true,
       .semi_transparent = false});
}

// decomp: MENUE.PRG _fadeMenuUpper (494.c:740)
void DrawUpperScrollFade(const PageArrowState& arrow,
                         PagePrimitiveTarget& target) {
  DrawScrollFade(
      PageArrow::kUp, arrow,
      [](std::int32_t row) { return kUpperFadeTop + row; }, target);
}

// decomp: MENUE.PRG _fadeMenuLower (494.c:791)
void DrawLowerScrollFade(const PageArrowState& arrow,
                         PagePrimitiveTarget& target) {
  DrawScrollFade(
      PageArrow::kDown, arrow,
      [](std::int32_t row) { return kLowerFadeTop - row; }, target);
}

ScrollFades VisibleScrollFades(std::int32_t scroll_position,
                               std::int32_t line_count) noexcept {
  return {.upper = scroll_position > 0, .lower = scroll_position < line_count};
}

// decomp: MENUE.PRG _drawControlsBg (494.c:962)
PageDrawResult DrawControlsBackground(const PageRect& rect,
                                      PagePrimitiveTarget& target) {
  // The loop runs while i < (y + h) - 1 in wrapping 32-bit arithmetic.
  const std::int32_t last_row =
      WrappingSubtract(WrappingAdd(rect.y, rect.height), 1);
  const std::int64_t rows = last_row > rect.y
                                ? static_cast<std::int64_t>(last_row) -
                                      static_cast<std::int64_t>(rect.y)
                                : 0;
  if (rows > kMaxControlRows) {
    return PageDrawResult::kRefused;
  }
  std::int32_t length = rect.width;
  for (std::int32_t row = rect.y; row < last_row; ++row) {
    target.DrawGradientLine(
        kPagePanelSlot,
        {.from = Point(rect.x, row),
         .to = Point(WrappingSubtract(WrappingAdd(rect.x, length), 1), row),
         .from_color = kControlsLeft,
         .to_color = kControlsRight});
    length = WrappingSubtract(length, 1);
  }
  const std::int32_t x = WrappingAdd(rect.x, kControlsShadowOffset);
  const std::int32_t y = WrappingAdd(rect.y, kControlsShadowOffset);
  const std::int32_t bottom = WrappingSubtract(WrappingAdd(y, rect.height), 1);
  target.DrawFlatQuad(
      kPagePanelSlot,
      {.corners = {Point(x, y),
                   Point(WrappingSubtract(WrappingAdd(x, rect.width), 1), y),
                   Point(x, bottom),
                   Point(WrappingSubtract(WrappingAdd(x, rect.width),
                                          rect.height),
                         bottom)},
       .color = {}});
  return PageDrawResult::kDrawn;
}

// decomp: MENUE.PRG _drawContent (494.c:577)
PageDrawResult DrawPageContent(std::span<const std::int16_t> help_text,
                               const Render::ScreenRect& clip,
                               PageContentTarget& target) {
  constexpr std::size_t kHeaderHalfwords = 8;
  constexpr std::int32_t kTextBytesPerListStep = 4;
  constexpr std::size_t kHalfwordsPerListStep = 2;
  if (help_text.size() < 2) {
    return PageDrawResult::kRefused;
  }
  // Word 0, the text section's size in bytes, rounded toward zero to words.
  const auto text_bytes = static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(help_text[0]))) |
      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(help_text[1]))
       << kHighHalfShift));
  const std::int32_t list_steps = text_bytes / kTextBytesPerListStep;
  if (list_steps < 0) {
    return PageDrawResult::kRefused;
  }
  const std::size_t list =
      kHeaderHalfwords +
      (static_cast<std::size_t>(list_steps) * kHalfwordsPerListStep);
  if (list >= help_text.size()) {
    return PageDrawResult::kRefused;
  }
  const std::size_t count = static_cast<std::uint16_t>(help_text[list]);
  if (count >= help_text.size() - list) {
    return PageDrawResult::kRefused;
  }
  for (std::size_t record = 0; record < count; ++record) {
    const std::size_t offset =
        list + static_cast<std::uint16_t>(help_text[list + 1 + record]);
    if (offset >= help_text.size()) {
      return PageDrawResult::kRefused;
    }
  }
  target.SetDrawArea(clip);
  target.DrawDiagramLines();
  for (std::size_t record = 0; record < count; ++record) {
    target.DrawIllustration(
        list + static_cast<std::uint16_t>(help_text[list + 1 + record]));
  }
  target.SetDrawArea({.x = clip.x >= kRightFramebufferX
                               ? kContentClipRightBufferLeft
                               : kContentClipLeft,
                      .y = kContentClipTop,
                      .width = kContentClipWidth,
                      .height = kContentClipHeight});
  return PageDrawResult::kDrawn;
}
}  // namespace VS::Menu::manual
