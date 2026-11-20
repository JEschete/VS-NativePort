// Hand-maintained (tools/port/owned.csv).
// Live binding for the menu and dialogue fonts (GameFont.cpp,
// TextBoxText.cpp): the font tables, the shared font style, the frame's
// primitive buffer and VRAM. Game binds the dialogue text boxes, their input
// and the battle string context (Menu/TextBoxTextLive.cpp).
#include "Render/GameFontLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Diagnostics.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Display.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFont.hpp"
#include "Render/Render.hpp"
#include "Render/ScaledFontLive.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/Text/VariableFont.hpp"
#include "Render/TextBoxText.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Legacy::LoadImage;
using VS::Legacy::RECT;
using VS::Legacy::setRECT;
using VS::Legacy::StoreImage;

namespace VS::Render {
namespace {
// A text box draws into four ordering-table layers; its glyphs go in front of
// the last.
constexpr std::size_t kTextForegroundLayer = 3;
constexpr std::size_t kTrustedCompactTextBytes = 0x1000;
constexpr int kFramebufferWidth = 320;

FontStyle fontStyle;
std::uint64_t refusedText = 0;

void RefuseText(const char* problem) {
  ++refusedText;
  // The first and then every power-of-two report, so a per-frame problem
  // cannot flood the log.
  if (std::has_single_bit(refusedText)) {
    VS::Core::Errorf("text", "text draw refused: %s (%" PRIu64 " refused)",
                     problem, refusedText);
  }
}

std::string_view CompactText(const char* text) {
  if (text == nullptr) {
    return {};
  }
  const void* end = std::memchr(text, '\0', kTrustedCompactTextBytes);
  return {text, end != nullptr ? static_cast<std::size_t>(
                                     static_cast<const char*>(end) - text)
                               : kTrustedCompactTextBytes};
}

CompactFontTables LiveCompactFontTables() {
  return {.widths = compactGlyphWidths,
          .left_trim = compactGlyphLeftTrim,
          .indices = compactGlyphIndices};
}

[[nodiscard]] bool Covers(const ScreenRect& area, std::size_t pixels) {
  return area.width >= 0 && area.height >= 0 &&
         pixels >= static_cast<std::size_t>(area.width) *
                       static_cast<std::size_t>(area.height);
}

// The framebuffer area around a text box, read and written through the GPU.
class LiveVramPixels final : public VramPixelTarget {
 public:
  void Read(const ScreenRect& area, std::span<std::uint16_t> pixels) override {
    if (!Covers(area, pixels.size())) {
      RefuseText("framebuffer read has too few pixels");
      return;
    }
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    StoreImage(&rect, reinterpret_cast<std::uint32_t*>(pixels.data()));
  }

  void Upload(const ScreenRect& area,
              std::span<const std::uint16_t> pixels) override {
    if (!Covers(area, pixels.size())) {
      RefuseText("framebuffer upload has too few pixels");
      return;
    }
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    // LoadImage only reads the pixels.
    LoadImage(&rect, reinterpret_cast<std::uint32_t*>(
                         const_cast<std::uint16_t*>(pixels.data())));
  }
};
}  // namespace

FontStyle& LiveFontStyle() noexcept { return fontStyle; }

std::uint64_t RefusedLiveTextCount() noexcept { return refusedText; }

void DrawLiveCompactTextColor(const char* text, int xy, int color,
                              std::uint32_t* chain) {
  LivePacketQueue queue(chain != nullptr
                            ? chain
                            : static_cast<std::uint32_t*>(
                                  FrameScratchpad().overlay_ordering_table));
  DrawCompactText(queue, LiveCompactFontTables(), CompactText(text), xy,
                  static_cast<std::uint32_t>(color));
}

void DrawLiveCompactText(const char* text, int xy, void* chain) {
  DrawLiveCompactTextColor(text, xy, static_cast<int>(kCompactTextGray),
                           static_cast<std::uint32_t*>(chain));
}

int DrawLiveVariableWidthGlyph(std::uint32_t glyph, int x, int y,
                               std::uint32_t* layers) {
  LivePacketQueue queue(layers != nullptr ? layers + kTextForegroundLayer
                                          : nullptr);
  return DrawVariableWidthGlyph(queue, fontStyle, LiveVariableFontGlyphWidths(),
                                glyph, x, y);
}

void SetLiveMenuFontStyle(int palette_offset) {
  SetMenuFontStyle(fontStyle, static_cast<std::uint8_t>(palette_offset));
}

std::optional<ProgressiveTextResult> DrawLiveProgressiveText(
    TextBoxText& box, const TextBoxInput& input,
    const EncodedTextArguments& arguments, std::uint32_t* layers) {
  if (layers == nullptr) {
    RefuseText("progressive text has no ordering-table layers");
    return std::nullopt;
  }
  LivePacketQueue glyphs(layers + kTextForegroundLayer);
  return DrawProgressiveTextBoxText(
      box, input, fontStyle, LiveVariableFontGlyphWidths(), arguments, glyphs);
}

void DrawLiveScaledText(const TextBoxText& box, int scale,
                        std::uint32_t* layers) {
  if (layers == nullptr) {
    RefuseText("scaled text has no ordering-table layers");
    return;
  }
  LiveScaledGlyphTarget target(layers + kTextForegroundLayer);
  DrawScaledTextBoxText(box, scale, fontStyle, LiveVariableFontGlyphWidths(),
                        target);
}

void BlendLiveTextIntoFramebuffer(TextBoxText& box, int glyph_alpha,
                                  bool skip_pressed) {
  const FramebufferTextBlend blend{
      .alpha = glyph_alpha,
      .framebuffer_x = frameBuf * kFramebufferWidth,
      .skip_pressed = skip_pressed};
  const VariableFontTables font{.glyph_widths = LiveVariableFontGlyphWidths(),
                                .pixel_expansion = kGlyphPixelExpansion};
  LiveVramPixels vram;
  BlendTextBoxIntoFramebuffer(box, blend, fontStyle, font, vram);
}
}  // namespace VS::Render
