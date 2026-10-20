#include <array>
#include <cstddef>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Render/PageSpace.hpp"

namespace {
using VS::u32;
using VS::Render::PageSpace;

void CheckPresentedPixels(const std::vector<u32>& frame) {
  for (int y = 0; y < 240; ++y) {
    for (int x = 0; x < 320; ++x) {
      CAPTURE(x, y);
      const u32 expected = y >= 8 && y < 232 ? 0xFF00FF00u : 0xFF000000u;
      CHECK(frame[static_cast<std::size_t>(y) * 320 + x] == expected);
    }
  }
}
}  // namespace

TEST_CASE("Presentation excludes poisoned storage around both framebuffers",
          "[render][gpu][viewport]") {
  const int source_x = GENERATE(0, 320);
  const bool mask = GENERATE(false, true);
  PageSpace pages;
  pages.Fill({0, 0, PageSpace::Width, PageSpace::Height}, 255, 0, 255);
  pages.SetDrawArea(source_x, 0, source_x + 319, 223);
  pages.SetDrawOffset(source_x, 0);
  pages.SetMask(mask, false);
  const std::array<u32, 3> rectangle{0x6000FF00u, 0xFFFFFFFFu,
                                     322u | (226u << 16)};
  REQUIRE(pages.Execute(rectangle) == rectangle.size());
  auto& display = pages.DisplayState();
  display.area = {source_x, 0, 320, 224};
  display.screenY = 8;
  display.screenH = 224;
  display.enabled = true;
  std::vector<u32> frame;
  int width = 0, height = 0;
  REQUIRE(pages.Present(frame, width, height));
  REQUIRE(width == 320);
  REQUIRE(height == 240);
  REQUIRE(frame.size() == std::size_t{320} * 240);
  CheckPresentedPixels(frame);
}
