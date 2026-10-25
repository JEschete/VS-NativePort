#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Input/Controller.h"
#include "Menu/Framework/TextBoxes.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Render.h"
#include "Render/Text/VariableFont.h"
}

namespace {
constexpr u_short kBackground = 0x1234;
constexpr int kWidth = 58;
constexpr int kHeight = 30;

struct TextTransitionFixture {
  Core_HeapBlockHeader saved_heap = Core_legacyHeapList;
  Core_HeapBlockHeader saved_secondary = Core_legacySecondaryHeapList;
  std::array<Core_HeapBlockHeader, 1024> heap{};
  decltype(std::to_array(Render_variableFontGlyphWidths)) widths =
      std::to_array(Render_variableFontGlyphWidths);
  decltype(std::to_array(Render_kGlyphPixelExpansion)) expansion =
      std::to_array(Render_kGlyphPixelExpansion);
  char font_table = Render_fontTableSelect;
  int frame_buffer = Render_frameBuf;
  int buttons = Input_buttonsPressed.allButtons;
  RECT target{32, 32, kWidth, kHeight};
  RECT atlas{832, 108, 63, 12};
  std::vector<u_short> saved_target = std::vector<u_short>(kWidth * kHeight);
  std::vector<u_short> saved_atlas = std::vector<u_short>(63 * 12);
  Menu_TextBoxState box{};

  TextTransitionFixture() {
    StoreImage(&target, reinterpret_cast<u_long*>(saved_target.data()));
    StoreImage(&atlas, reinterpret_cast<u_long*>(saved_atlas.data()));
    Core_Memory_InitializeHeap(heap.data(), sizeof(heap));
    std::fill(std::begin(Render_variableFontGlyphWidths),
              std::end(Render_variableFontGlyphWidths), 6);
    Render_kGlyphPixelExpansion[1] = 0x7FFF;
    Render_kGlyphPixelExpansion[2] = 0x001F;
    Render_kGlyphPixelExpansion[3] = 0x03E0;
    Render_kGlyphPixelExpansion[4] = 0x7C00;
    Glyph(0, 1);
    Glyph(10, 2);
    Glyph(11, 3);
    Glyph(12, 4);
    std::vector<u_short> pixels(kWidth * kHeight, kBackground);
    LoadImage(&target, reinterpret_cast<u_long*>(pixels.data()));
    Render_frameBuf = 0;
    Input_buttonsPressed.allButtons = 0;
    box.x = box.xIndent = target.x;
    box.y = box.yIndent = target.y;
    box.charsPerLine = 4;
    box.lineCount = 2;
    box.speed = 1;
  }

  ~TextTransitionFixture() {
    LoadImage(&target, reinterpret_cast<u_long*>(saved_target.data()));
    LoadImage(&atlas, reinterpret_cast<u_long*>(saved_atlas.data()));
    std::copy(widths.begin(), widths.end(),
              std::begin(Render_variableFontGlyphWidths));
    std::copy(expansion.begin(), expansion.end(),
              std::begin(Render_kGlyphPixelExpansion));
    Render_fontTableSelect = font_table;
    Render_frameBuf = frame_buffer;
    Input_buttonsPressed.allButtons = buttons;
    Core_legacyHeapList = saved_heap;
    Core_legacySecondaryHeapList = saved_secondary;
  }

  void Glyph(unsigned code, u_short index) {
    const unsigned glyph = code + 189;
    RECT location{static_cast<std::int16_t>(832 + glyph % 21 * 3),
                  static_cast<std::int16_t>(glyph / 21 * 12), 3, 12};
    std::array<u_short, 36> pixels{};
    for (unsigned row = 0; row < 12; ++row) {
      pixels[row * 3] = static_cast<u_short>(index * 0x1111);
      pixels[row * 3 + 1] = static_cast<u_short>(index * 0x11);
    }
    LoadImage(&location, reinterpret_cast<u_long*>(pixels.data()));
  }

  template <std::size_t Size>
  std::vector<u_short> Draw(std::array<u_char, Size>& text) {
    INFO(std::string("Encoded transition bytes: ") + std::to_string(Size));
    box.string = reinterpret_cast<char*>(text.data());
    Render_BlendTextBoxIntoFramebuffer(&box, 0x10000);
    std::vector<u_short> pixels(kWidth * kHeight);
    StoreImage(&target, reinterpret_cast<u_long*>(pixels.data()));
    return pixels;
  }
};
}  // namespace

TEST_CASE("Control-only transition text stops before zero padding",
          "[text-transition]") {
  TextTransitionFixture fixture;
  // The final byte bounds the legacy ASCII-control bug in the red regression.
  std::array<u_char, 8> text{0xFC, 4, 0xE7, 0, 0, 0, 0, 0x0E};
  const auto pixels = fixture.Draw(text);
  CHECK(std::all_of(pixels.begin(), pixels.end(),
                    [](u_short pixel) { return pixel == kBackground; }));
  const auto repeated = fixture.Draw(text);
  CHECK(std::equal(pixels.begin(), pixels.end(), repeated.begin()));
}

TEST_CASE("Framebuffer transitions draw encoded letters beyond ASCII controls",
          "[text-transition]") {
  TextTransitionFixture fixture;
  std::array<u_char, 3> text{12, 0xE7, 0x0E};
  const auto pixels = fixture.Draw(text);
  CHECK(pixels[0] == 0x7C00);
  CHECK(pixels[6] == kBackground);
}

TEST_CASE("Framebuffer transitions advance encoded spaces without a glyph",
          "[text-transition]") {
  TextTransitionFixture fixture;
  std::array<u_char, 5> text{10, 0x8F, 11, 0xE7, 0x0E};
  const auto pixels = fixture.Draw(text);
  CHECK(pixels[0] == 0x001F);
  CHECK(pixels[6] == kBackground);
  CHECK(pixels[12] == 0x03E0);
}

TEST_CASE("Framebuffer transition newlines use the encoded control range",
          "[text-transition]") {
  TextTransitionFixture fixture;
  std::array<u_char, 5> text{10, 0xE8, 11, 0xE7, 0x0E};
  const auto pixels = fixture.Draw(text);
  CHECK(pixels[0] == 0x001F);
  CHECK(pixels[6] == kBackground);
  CHECK(pixels[13 * kWidth] == 0x03E0);
}
