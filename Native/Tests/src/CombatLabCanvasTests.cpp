#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "CombatLab/Canvas.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Render/Text/CompactFont.h"
#include "SystemResources.h"
}

namespace {
using VS::combat_lab::Canvas;
constexpr std::size_t kImageOffset = sizeof(Content_SystemDatHeader);
constexpr auto kImageBytes = std::size_t{64} * 96 * sizeof(std::uint16_t);
constexpr auto kUiOffset = kImageOffset + sizeof(RECT) + kImageBytes;
constexpr auto kPaletteOffset = kUiOffset + (std::size_t{223} * 64 + 32) * 2;
constexpr int kGlyphIndex = 'A' - ' ';

template <typename T>
void Store(std::vector<std::uint8_t>& bytes, std::size_t offset,
           const T& value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::vector<std::uint8_t> FontData() {
  std::vector<std::uint8_t> bytes(kPaletteOffset + 16 * sizeof(std::uint16_t));
  Content_SystemDatHeader header{};
  header.image0Offset = static_cast<std::uint32_t>(kImageOffset);
  header.interfaceDataOffset = static_cast<std::uint32_t>(kUiOffset);
  Store(bytes, 0, header);
  const RECT rect{768, 0, 64, 96};
  Store(bytes, kImageOffset, rect);
  for (std::size_t row = 76; row < 86; ++row) {
    const std::uint16_t packed = 0x3210;
    Store(bytes, kImageOffset + sizeof(RECT) + row * 64 * 2, packed);
  }
  const std::array<std::uint16_t, 16> palette{0, 0x8000, 0x4210, 0x7FFF};
  Store(bytes, kPaletteOffset, palette);
  return bytes;
}

struct GlyphScope {
  unsigned char width = Render_compactGlyphWidths[kGlyphIndex / 2];
  unsigned char index = Render_compactGlyphIndices[kGlyphIndex];

  GlyphScope() {
    Render_compactGlyphWidths[kGlyphIndex / 2] = 0x40;
    Render_compactGlyphIndices[kGlyphIndex] = 0;
  }
  ~GlyphScope() {
    Render_compactGlyphWidths[kGlyphIndex / 2] = width;
    Render_compactGlyphIndices[kGlyphIndex] = index;
  }
};
}  // namespace

TEST_CASE("Combat Lab font retains transparent, black and shaded texels",
          "[combat-lab][render][font]") {
  GlyphScope glyph;
  Canvas canvas(FontData());
  canvas.Clear();
  const auto background = canvas.Pixels()[20 * Canvas::kWidth + 20];
  canvas.Text(20, 20, "A", 0xFFFFFFFF);
  for (std::ptrdiff_t row = 20; row < 30; ++row) {
    const auto* pixels = canvas.Pixels() + row * Canvas::kWidth + 20;
    CHECK(pixels[0] == background);
    CHECK(pixels[1] == 0xFF000000);
    CHECK(pixels[2] == 0xFF838383);
    CHECK(pixels[3] == 0xFFFFFFFF);
    CHECK(pixels[4] == background);
  }
}

TEST_CASE("Combat Lab font tests transparency after palette lookup",
          "[combat-lab][render][font]") {
  GlyphScope glyph;
  auto bytes = FontData();
  const std::uint16_t opaque_zero_index = 0x7FFF;
  const std::uint16_t transparent_nonzero_index = 0;
  Store(bytes, kPaletteOffset, opaque_zero_index);
  Store(bytes, kPaletteOffset + 3 * sizeof(std::uint16_t),
        transparent_nonzero_index);
  Canvas canvas(bytes);
  canvas.Clear();
  const auto background = canvas.Pixels()[20 * Canvas::kWidth + 20];
  canvas.Text(20, 20, "A", 0xFF804020);
  const auto* pixels =
      canvas.Pixels() + std::ptrdiff_t{20} * Canvas::kWidth + 20;
  CHECK(pixels[0] == 0xFF804020);
  CHECK(pixels[1] == 0xFF000000);
  CHECK(pixels[2] == 0xFF422110);
  CHECK(pixels[3] == background);
}

TEST_CASE("Combat Lab glyphs clip without wrapping across canvas edges",
          "[combat-lab][render][font]") {
  GlyphScope glyph;
  Canvas canvas(FontData());
  canvas.Clear();
  const auto background = canvas.Pixels()[0];
  canvas.Text(-2, -9, "A", 0xFFFFFFFF);
  CHECK(canvas.Pixels()[0] == 0xFF838383);
  CHECK(canvas.Pixels()[1] == 0xFFFFFFFF);
  CHECK(canvas.Pixels()[2] == background);
  canvas.Text(Canvas::kWidth - 3, Canvas::kHeight - 1, "A", 0xFFFFFFFF);
  CHECK(canvas.Pixels()[Canvas::kWidth * Canvas::kHeight - 1] == 0xFF838383);
  CHECK(canvas.Pixels()[Canvas::kWidth] == background);
}

TEST_CASE("Combat Lab rejects truncated or invalid font assets",
          "[combat-lab][render][font]") {
  auto bytes = FontData();
  SECTION("header") { bytes.resize(sizeof(Content_SystemDatHeader) - 1); }
  SECTION("image") { bytes.resize(kUiOffset - 1); }
  SECTION("palette") { bytes.resize(kPaletteOffset + 31); }
  SECTION("palette offset overflow") {
    const std::uint32_t invalid = UINT32_MAX;
    Store(bytes, offsetof(Content_SystemDatHeader, interfaceDataOffset),
          invalid);
  }
  SECTION("image offset overflow") {
    const std::uint32_t invalid = UINT32_MAX;
    Store(bytes, offsetof(Content_SystemDatHeader, image0Offset), invalid);
  }
  SECTION("wrong texture page") {
    const RECT invalid{832, 0, 64, 96};
    Store(bytes, kImageOffset, invalid);
  }
  SECTION("undersized font page") {
    const RECT invalid{768, 0, 63, 96};
    Store(bytes, kImageOffset, invalid);
  }
  CHECK_THROWS_AS(Canvas{bytes}, std::runtime_error);
}
