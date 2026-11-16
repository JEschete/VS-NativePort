#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Render/DisplayControl.hpp"

// The manual page's frame (MENUE.PRG 494.c:577-616, 716-881, 962-987): the
// clipped content pass, the gradient page background, the scroll fades at
// the panel's top and bottom edges with their bouncing page arrows, and the
// shaded boxes behind the control hints. The cores hand typed primitives,
// each with the ordering-table slot the original linked it into, to a
// target; they reach no frame scratch, primitive cursor or global state
// (ManualPageLive.cpp).
namespace VS::Menu::manual {
// Ordering-table slots relative to the frame's scene table
// (Render_frameScratch.orderingTable).
inline constexpr std::int32_t kPageContentSlot = 0;
// The page background and the control hint boxes.
inline constexpr std::int32_t kPagePanelSlot = 7;
// The scroll fades and page arrows, seven entries in front of the scene
// table (in the overlay table).
inline constexpr std::int32_t kPageFadeSlot = -7;

struct PagePoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const PagePoint&, const PagePoint&) = default;
};

struct PageColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  friend bool operator==(const PageColor&, const PageColor&) = default;
};

struct PageTexel {
  std::uint8_t u = 0;
  std::uint8_t v = 0;

  friend bool operator==(const PageTexel&, const PageTexel&) = default;
};

// Quad corners in the GPU's order: top left, top right, bottom left, bottom
// right.
inline constexpr std::size_t kQuadCorners = 4;
inline constexpr std::size_t kBottomRight = 3;
using PageCorners = std::array<PagePoint, kQuadCorners>;

// POLY_G4: a Gouraud-shaded quad.
struct GradientQuad {
  PageCorners corners{};
  std::array<PageColor, kQuadCorners> colors{};

  friend bool operator==(const GradientQuad&, const GradientQuad&) = default;
};

// POLY_FT4: a textured quad. A raw-texture quad ignores its colour.
struct TexturedQuad {
  PageCorners corners{};
  std::array<PageTexel, kQuadCorners> texels{};
  PageColor color{};
  std::uint16_t clut = 0;
  std::uint16_t texture_page = 0;
  bool raw_texture = false;
  bool semi_transparent = false;

  friend bool operator==(const TexturedQuad&, const TexturedQuad&) = default;
};

// TILE: a flat rectangle.
struct PageTile {
  PagePoint position{};
  std::int16_t width = 0;
  std::int16_t height = 0;
  PageColor color{};
  bool semi_transparent = false;

  friend bool operator==(const PageTile&, const PageTile&) = default;
};

// LINE_G2: a Gouraud-shaded line.
struct GradientLine {
  PagePoint from{};
  PagePoint to{};
  PageColor from_color{};
  PageColor to_color{};

  friend bool operator==(const GradientLine&, const GradientLine&) = default;
};

// POLY_F4: a flat quad.
struct FlatQuad {
  PageCorners corners{};
  PageColor color{};

  friend bool operator==(const FlatQuad&, const FlatQuad&) = default;
};

class PagePrimitiveTarget {
 public:
  virtual ~PagePrimitiveTarget() = default;
  // Each call queues one packet in front of ordering-table entry `slot`.
  virtual void DrawGradientQuad(std::int32_t slot,
                                const GradientQuad& quad) = 0;
  virtual void DrawTexturedQuad(std::int32_t slot,
                                const TexturedQuad& quad) = 0;
  virtual void DrawTile(std::int32_t slot, const PageTile& tile) = 0;
  virtual void DrawGradientLine(std::int32_t slot,
                                const GradientLine& line) = 0;
  virtual void DrawFlatQuad(std::int32_t slot, const FlatQuad& quad) = 0;
  // _insertTPage (insertTPage.h): a dithered draw-mode command selecting
  // `texture_page`'s page and blending.
  virtual void SetTexturePage(std::int32_t slot,
                              std::uint16_t texture_page) = 0;
};

// The packet words after each primitive's tag, as the GPU reads them. Bytes
// the original never wrote (padding, and a raw-texture quad's colour) are 0.
inline constexpr std::size_t kGradientQuadWords = 8;
inline constexpr std::size_t kTexturedQuadWords = 9;
inline constexpr std::size_t kPageTileWords = 3;
inline constexpr std::size_t kGradientLineWords = 4;
inline constexpr std::size_t kFlatQuadWords = 5;
inline constexpr std::size_t kTexturePageWords = 2;
[[nodiscard]] std::array<std::uint32_t, kGradientQuadWords> PackGradientQuad(
    const GradientQuad& quad) noexcept;
[[nodiscard]] std::array<std::uint32_t, kTexturedQuadWords> PackTexturedQuad(
    const TexturedQuad& quad) noexcept;
[[nodiscard]] std::array<std::uint32_t, kPageTileWords> PackPageTile(
    const PageTile& tile) noexcept;
[[nodiscard]] std::array<std::uint32_t, kGradientLineWords> PackGradientLine(
    const GradientLine& line) noexcept;
[[nodiscard]] std::array<std::uint32_t, kFlatQuadWords> PackFlatQuad(
    const FlatQuad& quad) noexcept;
[[nodiscard]] std::array<std::uint32_t, kTexturePageWords> PackTexturePage(
    std::uint16_t texture_page) noexcept;

enum class PageDrawResult : std::uint8_t {
  kDrawn,
  // An input the original would have read or looped past its data with;
  // nothing was drawn.
  kRefused,
};

// A rectangle in the original's int arguments, before 16-bit truncation.
struct PageRect {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
};

// The gradient palette (_colorStops0, _colorStops1): left and right colours.
inline constexpr std::int32_t kPageGradients = 4;

// decomp: MENUE.PRG _setPageBg (494.c:716). A left-to-right gradient quad
// covering `rect`, then the dithered opaque texture page, both in the panel
// slot. A gradient outside the palette is refused.
PageDrawResult DrawPageBackground(const PageRect& rect, std::int32_t gradient,
                                  PagePrimitiveTarget& target);

enum class PageArrow : std::uint8_t {
  kUp = 0,
  kDown = 1,
};

// The page arrows' shared state: the 16-frame bounce counter
// (_pageArrowAnimState) and whether Square was held last frame, which picks
// the dimmed palette.
struct PageArrowState {
  std::int32_t animation_state = 0;
  bool square_held = false;
};

// The arrow's top edge: the up arrow bounces around y 36-40, the down arrow
// around y 184-188.
[[nodiscard]] std::int32_t PageArrowTop(PageArrow arrow,
                                        std::int32_t animation_state) noexcept;

// decomp: MENUE.PRG _drawPaginationArrow (494.c:844). A 16x16 raw-texture
// arrow at x 152 in the fade slot.
void DrawPageArrow(PageArrow arrow, const PageArrowState& state,
                   PagePrimitiveTarget& target);

// The fades' rows: 16 rows of 3 textured chunks (128, 128 and 64 pixels
// wide), dimming from 128 at the panel edge to 0.
inline constexpr std::int32_t kScrollFadeRows = 16;
inline constexpr std::int32_t kScrollFadeChunks = 3;
using ScrollFadeWidths = std::array<std::int32_t, kScrollFadeChunks>;
inline constexpr ScrollFadeWidths kScrollFadeChunkWidths{128, 128, 64};
// The original table's second level, 115, is darker than its third
// (preserved).
inline constexpr std::array<std::uint8_t, kScrollFadeRows> kScrollFadeLevels{
    128, 115, 121, 88, 76, 64, 53, 42, 33, 24, 17, 11, 6, 3, 1, 0,
};

// decomp: MENUE.PRG _fadeMenuUpper (494.c:740). The up arrow, then rows
// 52-67 of the page background redrawn additively and darkened by additive
// 320x1 tiles, row 52 strongest, then the dithered additive texture page,
// all in the fade slot.
void DrawUpperScrollFade(const PageArrowState& arrow,
                         PagePrimitiveTarget& target);
// decomp: MENUE.PRG _fadeMenuLower (494.c:791). The down arrow and rows
// 187 up to 172, row 187 strongest.
void DrawLowerScrollFade(const PageArrowState& arrow,
                         PagePrimitiveTarget& target);

// Which fades the help page shows (494.c _showMenu): the upper one once
// scrolled down, the lower one while lines remain below.
struct ScrollFades {
  bool upper = false;
  bool lower = false;

  friend bool operator==(const ScrollFades&, const ScrollFades&) = default;
};
[[nodiscard]] ScrollFades VisibleScrollFades(std::int32_t scroll_position,
                                             std::int32_t line_count) noexcept;

// More shaded rows than VRAM is tall are refused (the original looped
// through any height).
inline constexpr std::int64_t kMaxControlRows = 512;

// decomp: MENUE.PRG _drawControlsBg (494.c:962). One gradient line per row
// but the last, each a pixel shorter than the one above, then a black quad
// 2 pixels down and right whose bottom edge is `height` pixels shorter, all
// in the panel slot.
PageDrawResult DrawControlsBackground(const PageRect& rect,
                                      PagePrimitiveTarget& target);

class PageContentTarget {
 public:
  virtual ~PageContentTarget() = default;
  // A DR_AREA in front of the content slot.
  virtual void SetDrawArea(const Render::ScreenRect& area) = 0;
  // _drawContentLines (494.c:883).
  virtual void DrawDiagramLines() = 0;
  // _drawSprite (494.c:629) on the illustration record `halfword_offset`
  // halfwords into the help text.
  virtual void DrawIllustration(std::size_t halfword_offset) = 0;
};

// The content panel's clip: x 13-312 (333-632 on the right framebuffer),
// y 55-184.
inline constexpr std::int16_t kContentClipLeft = 13;
inline constexpr std::int16_t kContentClipRightBufferLeft = 333;
inline constexpr std::int16_t kContentClipTop = 55;
inline constexpr std::int16_t kContentClipWidth = 300;
inline constexpr std::int16_t kContentClipHeight = 130;
inline constexpr std::int16_t kRightFramebufferX = 320;

// decomp: MENUE.PRG _drawContent (494.c:577). `help_text` is the help text
// file as halfwords; its illustration list follows the 16-byte header and
// the text section (word 0, in bytes): a count, then each record's halfword
// offset from the list. The draw area set to `clip` and the panel clip are
// linked around the diagram lines and illustrations, so the GPU, walking
// the slot from its head, clips to the panel first and restores `clip`
// after. A list outside `help_text` is refused.
PageDrawResult DrawPageContent(std::span<const std::int16_t> help_text,
                               const Render::ScreenRect& clip,
                               PageContentTarget& target);
}  // namespace VS::Menu::manual
