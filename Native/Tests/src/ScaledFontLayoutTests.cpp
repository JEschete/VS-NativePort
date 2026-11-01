// Hand-maintained tests for the typed scaled font glyph layout (#247).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"
#include "Render/ScaledFont.hpp"

namespace {
constexpr int kOne = 1 << VS::Render::kScaledFontPositionFractionBits;
constexpr std::size_t kWidthCount = 400;

class RecordingTarget final : public VS::Render::ScaledGlyphTarget {
 public:
  bool available = true;
  std::vector<VS::Render::ScaledGlyphQuad> quads;

  [[nodiscard]] bool CanDraw() const override { return available; }
  void Draw(const VS::Render::ScaledGlyphQuad& quad) override {
    quads.push_back(quad);
  }
};

struct Font {
  std::array<std::byte, kWidthCount> widths{};
  VS::Render::ScaledFontStyle style;

  Font() { style.glyph_widths = widths; }
};

// Draws glyph 5 of the second font table, nine units wide, at (10, 20) with a
// 12 x 12 cell and twice the base advance.
struct SecondTableGlyph {
  Font font;
  RecordingTarget target;
  int next = 0;

  SecondTableGlyph() {
    font.style.table_select = 1;
    font.style.palette_offset = 0x10;
    font.style.color = 0x2C808080;
    font.widths[5 + VS::Render::kScaledFontGlyphsPerTable] = std::byte{9};
    next =
        VS::Render::DrawScaledFontChar(5, 10 * kOne, 20 * kOne, 12 * kOne,
                                       12 * kOne, 2 * kOne, font.style, target);
  }
};
}  // namespace

TEST_CASE("Scaled font glyphs cover their cell and advance the pen",
          "[render][scaled-font]") {
  const SecondTableGlyph drawn;
  CHECK(drawn.next == 28 * kOne);
  REQUIRE(drawn.target.quads.size() == 1);
  const auto& quad = drawn.target.quads[0];
  CHECK(quad.left == 10);
  CHECK(quad.top == 20);
  CHECK(quad.right == 22);
  CHECK(quad.bottom == 32);
}

TEST_CASE("Scaled font glyphs sample their texture cell and palette",
          "[render][scaled-font]") {
  const SecondTableGlyph drawn;
  REQUIRE(drawn.target.quads.size() == 1);
  const auto& quad = drawn.target.quads[0];
  CHECK(quad.u == 60);
  CHECK(quad.v == 108);
  CHECK(quad.clut == 0x3784);
  CHECK(quad.texture_page == VS::Render::kScaledFontTexturePage);
  CHECK(quad.color == 0x2C808080U);
}

TEST_CASE("Scaled font palettes wrap within the font's palette row",
          "[render][scaled-font]") {
  Font font;
  font.style.palette_offset = 0x0C;
  RecordingTarget target;
  (void)VS::Render::DrawScaledFontChar(5, 0, 0, 0, 0, kOne, font.style, target);
  REQUIRE(target.quads.size() == 1);
  CHECK(target.quads[0].clut == 0x3780);
}

TEST_CASE("Scaled font spaces advance and unknown glyphs stay put",
          "[render][scaled-font]") {
  Font font;
  RecordingTarget target;
  target.available = false;
  CHECK(VS::Render::DrawScaledFontChar(VS::Render::kScaledFontSpaceGlyph,
                                       3 * kOne, 0, 0, 0, 2 * kOne, font.style,
                                       target) == 15 * kOne);
  target.available = true;
  font.style.table_select = 3;
  CHECK(VS::Render::DrawScaledFontChar(5, 3 * kOne, 0, 0, 0, kOne, font.style,
                                       target) == 3 * kOne);
  font.style.table_select = 0;
  font.widths.back() = std::byte{4};
  CHECK(VS::Render::DrawScaledFontChar(static_cast<int>(kWidthCount - 1), 0, 0,
                                       0, 0, kOne, font.style,
                                       target) == 4 * kOne);
  CHECK(VS::Render::DrawScaledFontChar(static_cast<int>(kWidthCount), 0, 0, 0,
                                       0, kOne, font.style, target) == 0);
  CHECK(target.quads.size() == 1);
}

TEST_CASE("An unavailable scaled glyph target neither draws nor advances",
          "[render][scaled-font]") {
  Font font;
  font.widths[5] = std::byte{7};
  RecordingTarget target;
  target.available = false;
  CHECK(VS::Render::DrawScaledFontChar(5, 2 * kOne, 0, 0, 0, kOne, font.style,
                                       target) == 2 * kOne);
  CHECK(target.quads.empty());
}

TEST_CASE("The promoted scaled font glyph has no original module address",
          "[render][scaled-font]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800C79D8u) == nullptr);
}
