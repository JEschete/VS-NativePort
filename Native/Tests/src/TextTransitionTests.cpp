#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBoxTextLive.hpp"
#include "Render/Display.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/Text/VariableFont.hpp"

using VS::Legacy::LoadImage;
using VS::Legacy::RECT;
using VS::Legacy::StoreImage;

namespace {
constexpr std::uint16_t kBackground = 0x1234;
constexpr int kWidth = 58;
constexpr int kHeight = 30;

struct TextTransitionFixture {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 1024> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  decltype(std::to_array(VS::Render::variableFontGlyphWidths)) widths =
      std::to_array(VS::Render::variableFontGlyphWidths);
  decltype(std::to_array(VS::Render::kGlyphPixelExpansion)) expansion =
      std::to_array(VS::Render::kGlyphPixelExpansion);
  VS::Render::FontStyle font_style = VS::Render::LiveFontStyle();
  int frame_buffer = VS::Render::frameBuf;
  decltype(VS::Input::buttonsPressed) buttons = VS::Input::buttonsPressed;
  RECT target{32, 32, kWidth, kHeight};
  RECT atlas{832, 108, 63, 12};
  std::vector<std::uint16_t> saved_target =
      std::vector<std::uint16_t>(std::size_t{kWidth} * kHeight);
  std::vector<std::uint16_t> saved_atlas =
      std::vector<std::uint16_t>(std::size_t{63} * 12);
  VS::Menu::TextBoxState box{};

  TextTransitionFixture() {
    StoreImage(&target, reinterpret_cast<std::uint32_t*>(saved_target.data()));
    StoreImage(&atlas, reinterpret_cast<std::uint32_t*>(saved_atlas.data()));
    std::fill(std::begin(VS::Render::variableFontGlyphWidths),
              std::end(VS::Render::variableFontGlyphWidths), 6);
    VS::Render::kGlyphPixelExpansion[1] = 0x7FFF;
    VS::Render::kGlyphPixelExpansion[2] = 0x001F;
    VS::Render::kGlyphPixelExpansion[3] = 0x03E0;
    VS::Render::kGlyphPixelExpansion[4] = 0x7C00;
    Glyph(0, 1);
    Glyph(10, 2);
    Glyph(11, 3);
    Glyph(12, 4);
    std::vector<std::uint16_t> pixels(std::size_t{kWidth} * kHeight,
                                      kBackground);
    LoadImage(&target, reinterpret_cast<std::uint32_t*>(pixels.data()));
    VS::Render::frameBuf = 0;
    VS::Input::buttonsPressed = 0;
    box.x = box.x_indent = target.x;
    box.y = box.y_indent = target.y;
    box.chars_per_line = 4;
    box.line_count = 2;
    box.speed = 1;
  }

  ~TextTransitionFixture() {
    LoadImage(&target, reinterpret_cast<std::uint32_t*>(saved_target.data()));
    LoadImage(&atlas, reinterpret_cast<std::uint32_t*>(saved_atlas.data()));
    std::copy(widths.begin(), widths.end(),
              std::begin(VS::Render::variableFontGlyphWidths));
    std::copy(expansion.begin(), expansion.end(),
              std::begin(VS::Render::kGlyphPixelExpansion));
    VS::Render::LiveFontStyle() = font_style;
    VS::Render::frameBuf = frame_buffer;
    VS::Input::buttonsPressed = buttons;
  }

  void Glyph(unsigned code, std::uint16_t index) {
    const unsigned glyph = code + 189;
    RECT location{static_cast<std::int16_t>(832 + glyph % 21 * 3),
                  static_cast<std::int16_t>(glyph / 21 * 12), 3, 12};
    std::array<std::uint16_t, 36> pixels{};
    for (unsigned row = 0; row < 12; ++row) {
      pixels[std::size_t{row} * 3] = static_cast<std::uint16_t>(index * 0x1111);
      pixels[std::size_t{row} * 3 + 1] =
          static_cast<std::uint16_t>(index * 0x11);
    }
    LoadImage(&location, reinterpret_cast<std::uint32_t*>(pixels.data()));
  }

  template <std::size_t Size>
  std::vector<std::uint16_t> Draw(std::array<std::uint8_t, Size>& text) {
    INFO(std::string("Encoded transition bytes: ") + std::to_string(Size));
    box.text = reinterpret_cast<char*>(text.data());
    VS::Menu::BlendLiveTextBoxIntoFramebuffer(&box, 0x10000);
    std::vector<std::uint16_t> pixels(std::size_t{kWidth} * kHeight);
    StoreImage(&target, reinterpret_cast<std::uint32_t*>(pixels.data()));
    return pixels;
  }
};
}  // namespace

TEST_CASE("Control-only transition text stops before zero padding",
          "[text-transition]") {
  TextTransitionFixture fixture;
  // The final byte bounds the legacy ASCII-control bug in the red regression.
  std::array<std::uint8_t, 8> text{0xFC, 4, 0xE7, 0, 0, 0, 0, 0x0E};
  const auto pixels = fixture.Draw(text);
  CHECK(std::all_of(pixels.begin(), pixels.end(),
                    [](std::uint16_t pixel) { return pixel == kBackground; }));
  const auto repeated = fixture.Draw(text);
  CHECK(std::equal(pixels.begin(), pixels.end(), repeated.begin()));
}

TEST_CASE("Framebuffer transitions draw encoded letters beyond ASCII controls",
          "[text-transition]") {
  TextTransitionFixture fixture;
  std::array<std::uint8_t, 3> text{12, 0xE7, 0x0E};
  const auto pixels = fixture.Draw(text);
  CHECK(pixels[0] == 0x7C00);
  CHECK(pixels[6] == kBackground);
}

TEST_CASE("Framebuffer transitions advance encoded spaces without a glyph",
          "[text-transition]") {
  TextTransitionFixture fixture;
  std::array<std::uint8_t, 5> text{10, 0x8F, 11, 0xE7, 0x0E};
  const auto pixels = fixture.Draw(text);
  CHECK(pixels[0] == 0x001F);
  CHECK(pixels[6] == kBackground);
  CHECK(pixels[12] == 0x03E0);
}

TEST_CASE("Framebuffer transition newlines use the encoded control range",
          "[text-transition]") {
  TextTransitionFixture fixture;
  std::array<std::uint8_t, 5> text{10, 0xE8, 11, 0xE7, 0x0E};
  const auto pixels = fixture.Draw(text);
  CHECK(pixels[0] == 0x001F);
  CHECK(pixels[6] == kBackground);
  CHECK(pixels[std::size_t{13} * kWidth] == 0x03E0);
}
