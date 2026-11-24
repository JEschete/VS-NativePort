#include "Render/NativeTextLayout.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace VS::Render {
namespace {
using Content::TextTokenKind;
constexpr double kMaximumLayoutExtent = 65536;
std::expected<double, std::string> WordWidth(
    std::span<const Content::TextToken> text, const Content::NativeFont& font,
    double scale) {
  double width = 0;
  double pen = 0;
  for (const auto& token : text) {
    if (token.kind == TextTokenKind::kNewline || token.identity == " ") break;
    auto glyph = font.glyphs.find(token.identity);
    if (token.kind != TextTokenKind::kGlyph || glyph == font.glyphs.end()) {
      return std::unexpected("text layout: missing glyph or unbound argument " +
                             token.identity);
    }
    const auto& metric = glyph->second;
    width =
        std::max(width, pen + std::max(metric.width, metric.advance) * scale);
    pen += metric.advance * scale;
  }
  return width;
}
}  // namespace

std::expected<TextLayout, std::string> LayoutNativeText(
    std::span<const Content::TextToken> text, const Content::NativeFont& font,
    TextBox box) {
  if (!std::isfinite(box.width) || !std::isfinite(box.height) ||
      !std::isfinite(box.scale) || box.width <= 0 || box.height <= 0 ||
      box.width > kMaximumLayoutExtent || box.height > kMaximumLayoutExtent ||
      box.scale < Content::kMinimumTextScale ||
      box.scale > Content::kMaximumTextScale || font.line_height <= 0) {
    return std::unexpected("text layout: invalid bounds or scale");
  }
  TextLayout result;
  double x = 0, y = 0;
  const double line = font.line_height * box.scale;
  for (std::size_t at = 0; at < text.size(); ++at) {
    const auto& token = text[at];
    if (token.kind == TextTokenKind::kNewline) {
      x = 0;
      y += line;
      if (y + line > box.height) {
        return std::unexpected("text layout: vertical overflow");
      }
      continue;
    }
    const auto found = font.glyphs.find(token.identity);
    if (token.kind != TextTokenKind::kGlyph || found == font.glyphs.end()) {
      return std::unexpected("text layout: missing glyph or unbound argument " +
                             token.identity);
    }
    const auto& metric = found->second;
    if (metric.advance <= 0 || metric.width <= 0 || metric.height <= 0) {
      return std::unexpected("text layout: invalid glyph metrics");
    }
    const double advance = metric.advance * box.scale;
    const double width = metric.width * box.scale;
    const double height = metric.height * box.scale;
    if (x > 0 && token.identity != " " &&
        (at == 0 || text[at - 1].identity == " ")) {
      auto word = WordWidth(text.subspan(at), font, box.scale);
      if (!word) return std::unexpected(word.error());
      if (*word <= box.width && x + *word > box.width) {
        x = 0;
        y += line;
      }
    }
    if (x + (metric.visible ? std::max(width, advance) : advance) > box.width) {
      x = 0;
      y += line;
      if (token.identity == " ") continue;
    }
    if (width > box.width || y + height > box.height) {
      return std::unexpected("text layout: text exceeds its box");
    }
    if (metric.visible) result.glyphs.push_back({metric, x, y, width, height});
    result.width =
        std::max(result.width,
                 x + (metric.visible ? std::max(width, advance) : advance));
    x += advance;
    result.height = std::max(result.height, y + height);
  }
  return result;
}
}  // namespace VS::Render
