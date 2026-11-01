// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "Legacy/Psx.h"
#include "Render/ScaledFont.hpp"

extern "C" {
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
}

namespace {
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;
constexpr unsigned kTextBoxCount = 8;
constexpr std::size_t kTextBoxOrderStride = 4;
constexpr std::size_t kTextForegroundLayer = 3;
static_assert(sizeof Render_fontColor == sizeof(std::uint32_t));

// The active text box's foreground layer in the current frame's primitive
// buffer.
class LiveGlyphTarget final : public VS::Render::ScaledGlyphTarget {
 public:
  [[nodiscard]] bool CanDraw() const override {
    return Render_activeTextBoxIndex < kTextBoxCount &&
           Menu_activeTextBoxOrderingTable != nullptr;
  }

  void Draw(const VS::Render::ScaledGlyphQuad& quad) override {
    const auto base = reinterpret_cast<std::uintptr_t>(
        Render_legacyPrimitiveBuffers[Render_frameBuf & 1]);
    const auto cursor =
        reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
    if (base == 0 || cursor < base ||
        cursor - base > kPrimitiveBufferBytes - sizeof(POLY_FT4)) {
      return;
    }
    auto* primitive =
        static_cast<POLY_FT4*>(Render_frameScratch.primitiveCursor);
    std::memset(primitive, 0, sizeof *primitive);
    setPolyFT4(primitive);
    std::memcpy(&primitive->r0, &quad.color, sizeof quad.color);
    setXY4(primitive, quad.left, quad.top, quad.right, quad.top, quad.left,
           quad.bottom, quad.right, quad.bottom);
    const unsigned u = quad.u;
    const unsigned v = quad.v;
    const unsigned cell = VS::Render::kScaledFontGlyphCellSize;
    setUV4(primitive, u, v, u + cell, v, u, v + cell, u + cell, v + cell);
    primitive->clut = quad.clut;
    primitive->tpage = quad.texture_page;
    AddPrim(Menu_activeTextBoxOrderingTable +
                std::size_t{Render_activeTextBoxIndex} * kTextBoxOrderStride +
                kTextForegroundLayer,
            primitive);
    Render_frameScratch.primitiveCursor = primitive + 1;
  }
};
}  // namespace

namespace VS::Render {
int DrawLiveScaledFontChar(int char_id, int x, int y, int width, int height,
                           int scale) {
  ScaledFontStyle style;
  style.glyph_widths = std::as_bytes(std::span(Render_variableFontGlyphWidths));
  style.table_select = static_cast<std::uint8_t>(Render_fontTableSelect);
  style.palette_offset = static_cast<std::uint8_t>(Render_fontPaletteOffset);
  std::memcpy(&style.color, &Render_fontColor, sizeof style.color);
  LiveGlyphTarget target;
  return DrawScaledFontChar(char_id, x, y, width, height, scale, style, target);
}
}  // namespace VS::Render
