#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Render/LibGpu.hpp"
#include "Render/PageSpace.hpp"

using VS::Legacy::getClut;
using VS::Legacy::getTPage;

namespace {
using VS::u16;
using VS::u32;
using VS::Render::PageSpace;

constexpr int kWidth = 42;
constexpr int kHeight = 10;
constexpr u16 kGreen = 0x03E0;
constexpr u16 kBlue = 0x7C00;

constexpr u32 Position(int x, int y) {
  return static_cast<u32>(x) | (static_cast<u32>(y) << 16);
}

constexpr u32 Texture(int u, int v) {
  return static_cast<u32>(u) | (static_cast<u32>(v) << 8);
}

std::array<u32, 9> Tile(int x, int y, int u, int v, bool reverse) {
  std::array<u32, 4> xy{Position(x, y), Position(x + kWidth, y),
                        Position(x, y + kHeight),
                        Position(x + kWidth, y + kHeight)};
  std::array<u32, 4> uv{Texture(u, v), Texture(u + 64, v), Texture(u, v + 15),
                        Texture(u + 64, v + 15)};
  if (reverse) {
    std::swap(xy[1], xy[2]);
    std::swap(uv[1], uv[2]);
  }
  return {0x2D808080u,
          xy[0],
          uv[0] | (getClut(768, 0) << 16),
          xy[1],
          uv[1] | (getTPage(1, 0, 640, 256) << 16),
          xy[2],
          uv[2],
          xy[3],
          uv[3]};
}

struct Atlas {
  PageSpace pages;

  Atlas() {
    const std::array<u16, 4> palette{0, kGreen, kBlue, 0x001F};
    pages.Upload({768, 0, 4, 1}, palette.data());
    std::array<u16, std::size_t{33} * 16> pixels{};
    pixels.fill(0x0303);
    for (std::ptrdiff_t row = 0; row < 15; ++row) {
      std::fill_n(pixels.begin() + row * 33, 32, 0x0101);
    }
    pages.Upload({640, 256, 33, 16}, pixels.data());
    for (std::ptrdiff_t row = 0; row < 15; ++row) {
      std::fill_n(pixels.begin() + row * 33, 32, 0x0202);
    }
    pages.Upload({704, 288, 33, 16}, pixels.data());
  }
};
}  // namespace

TEST_CASE("Scaled atlas quads exclude lower-right texture endpoints",
          "[render][gpu][raster][bounds]") {
  const bool reverse = GENERATE(false, true);
  const bool clipped = GENERATE(false, true);
  Atlas fixture;
  if (clipped) {
    fixture.pages.SetDrawArea(20, 13, 50, 18);
  }
  CHECK(fixture.pages.Execute(Tile(10, 10, 0, 0, reverse)) == 9);
  for (int y = 9; y <= 21; ++y) {
    for (int x = 9; x <= 53; ++x) {
      const bool inside = x >= 10 && x < 52 && y >= 10 && y < 20;
      const bool visible =
          !clipped || (x >= 20 && x <= 50 && y >= 13 && y <= 18);
      CAPTURE(reverse, clipped, x, y);
      CHECK(fixture.pages.At(x, y) == (inside && visible ? kGreen : 0));
    }
  }
}

TEST_CASE("Adjacent scaled atlas tiles never sample neighboring atlas storage",
          "[render][gpu][raster][atlas]") {
  const bool reverse = GENERATE(false, true);
  const bool vertical = GENERATE(false, true);
  Atlas fixture;
  const int next_x = vertical ? 10 : 10 + kWidth;
  const int next_y = vertical ? 10 + kHeight : 10;
  CHECK(fixture.pages.Execute(Tile(next_x, next_y, 128, 32, reverse)) == 9);
  CHECK(fixture.pages.Execute(Tile(10, 10, 0, 0, reverse)) == 9);
  const int right = vertical ? 10 + kWidth : 10 + 2 * kWidth;
  const int bottom = vertical ? 10 + 2 * kHeight : 10 + kHeight;
  for (int y = 9; y <= bottom; ++y) {
    for (int x = 9; x <= right; ++x) {
      u16 expected = 0;
      if (x >= 10 && x < right && y >= 10 && y < bottom) {
        expected = (vertical ? y >= next_y : x >= next_x) ? kBlue : kGreen;
      }
      CAPTURE(reverse, vertical, x, y);
      CHECK(fixture.pages.At(x, y) == expected);
    }
  }
}

// #507: the MAP001 window glow (E207) stacks dozens of dim additive layers,
// drawn with dithering off. The GPU modulates each texel and truncates it to
// the 5-bit frame before adding, so a dark texel adds nothing however often it
// is drawn. A true-colour emulator mode adds at 8 bits and shows a brighter,
// greener glow than the hardware.
TEST_CASE("Additive textured layers truncate to the 5-bit frame each draw",
          "[render][gpu][raster][blend]") {
  PageSpace pages;
  // Two STP words from E207_1.FBC: channels 5,5,4 and 2,2,1.
  const std::array<u16, 3> palette{0, 0x90A5, 0x8442};
  pages.Upload({768, 0, 3, 1}, palette.data());
  // 4-bit texels: index 1 for u 0..7, index 2 for u 8..15.
  std::array<u16, std::size_t{4} * 8> texels{};
  for (std::size_t row = 0; row < 8; ++row) {
    std::fill_n(texels.begin() + static_cast<std::ptrdiff_t>(row * 4), 2,
                u16{0x1111});
    std::fill_n(texels.begin() + static_cast<std::ptrdiff_t>(row * 4 + 2), 2,
                u16{0x2222});
  }
  pages.Upload({640, 256, 4, 8}, texels.data());
  // Modulated, semi-transparent quad at E207's vertex colour 63; the page
  // selects additive blending.
  const auto layer = [&pages](int x, int u) {
    const std::array<u32, 9> quad{
        0x2E3F3F3Fu,
        Position(x, 10),
        Texture(u, 0) | (getClut(768, 0) << 16),
        Position(x + 8, 10),
        Texture(u + 8, 0) | (getTPage(0, 1, 640, 256) << 16),
        Position(x, 18),
        Texture(u, 8),
        Position(x + 8, 18),
        Texture(u + 8, 8)};
    CHECK(pages.Execute(quad) == quad.size());
  };
  for (int draw = 0; draw < 8; ++draw) {
    layer(10, 0);
    layer(30, 8);
  }
  // Each bright layer adds 40*63/128 = 19 -> 2 (red, green) and
  // 32*63/128 = 15 -> 1 (blue); 8-bit accumulation would reach 19,19,15.
  constexpr u16 kBright = 0x8000 | 16 | (16 << 5) | (8 << 10);
  // Each dark layer adds 7 -> 0 and 3 -> 0: only the STP bit is written.
  constexpr u16 kDark = 0x8000;
  for (int y = 9; y <= 18; ++y) {
    for (int x = 9; x <= 38; ++x) {
      const bool rows = y >= 10 && y < 18;
      u16 expected = 0;
      if (rows && x >= 10 && x < 18) {
        expected = kBright;
      } else if (rows && x >= 30 && x < 38) {
        expected = kDark;
      }
      CAPTURE(x, y);
      CHECK(pages.At(x, y) == expected);
    }
  }
}

TEST_CASE("Quad diagonals shade shared pixels exactly once",
          "[render][gpu][raster][blend]") {
  const bool reverse = GENERATE(false, true);
  PageSpace pages;
  std::array<u32, 5> words{0x2A000040u, Position(10, 10), Position(30, 10),
                           Position(10, 30), Position(30, 30)};
  if (reverse) {
    std::swap(words[2], words[3]);
  }
  CHECK(pages.Execute(words) == words.size());
  for (int y = 9; y <= 31; ++y) {
    for (int x = 9; x <= 31; ++x) {
      const bool inside = x >= 10 && x < 30 && y >= 10 && y < 30;
      CAPTURE(reverse, x, y);
      CHECK(pages.At(x, y) == (inside ? 4 : 0));
    }
  }
}
