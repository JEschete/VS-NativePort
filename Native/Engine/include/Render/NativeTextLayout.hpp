#pragma once

#include <expected>
#include <span>
#include <string>
#include <vector>

#include "Content/NativeText.hpp"

namespace VS::Render {
struct TextBox {
  double width = 0;
  double height = 0;
  double scale = 1;
};
struct PlacedTextGlyph {
  Content::GlyphMetric source;
  double x = 0;
  double y = 0;
  double width = 0;
  double height = 0;
};
struct TextLayout {
  std::vector<PlacedTextGlyph> glyphs;
  double width = 0;
  double height = 0;
};
// Instant Latin/symbol text: break at spaces/newlines, emergency-wrap long
// words at scalar boundaries. Missing glyphs, unbound roles or overflow fail;
// never truncate a caution or silently reduce its requested scale.
[[nodiscard]] std::expected<TextLayout, std::string> LayoutNativeText(
    std::span<const Content::TextToken> text, const Content::NativeFont& font,
    TextBox box);
}  // namespace VS::Render
