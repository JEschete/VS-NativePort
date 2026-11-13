#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudRenderingLive.hpp"
#include "Battle/HudRenderingLiveFixture.hpp"
#include "Render/Sprites.hpp"
#include "Render/UiPackets.hpp"

namespace {
namespace b = VS::Battle;
namespace f = hud_rendering_fixture;
namespace r = VS::Render;
}  // namespace

TEST_CASE(
    "F01354 Live uses production gauge, numeric and label packet policies",
    "[hud-rendering-live][F01354]") {
  f::LiveFrame frame;
  b::DrawLiveHudStatGauge(0, 7, 10);
  CHECK(frame.UsedWords() == 44);
  CHECK(frame.primitives[2] == 0x0012000A);
  CHECK(frame.primitives[14 + r::kSpritePositionWord] == 0x000A0042);
  CHECK(frame.primitives[26 + r::kSpriteTextureWord] == 0x37F40B20);
  CHECK(frame.primitives[38 + r::kSpriteSizeWord] == 0x000900FE);
  CHECK(frame.primitives[38 + r::kSpriteTextureWord] == 0x37F600E1);
  CHECK(frame.ordering[1] == LegacyGpu_Handle(frame.primitives.data() + 38));
}

TEST_CASE("F01354 Live zero maximum needs no render target",
          "[hud-rendering-live][F01354]") {
  f::LiveFrame frame;
  frame.Disconnect();
  b::DrawLiveHudStatGauge(3, -1, 0);
  CHECK(frame.UsedWords() == 0);
  CHECK(frame.ordering[1] == f::kEndLink);
  CHECK(Battle_hudGaugeLabelTexU[3] == static_cast<char>(0xE1));
}

TEST_CASE("F01354 Live fixed-point risk rounds only its numeric presentation",
          "[hud-rendering-live][F01354]") {
  f::LiveFrame frame;
  b::DrawLiveHudStatGauge(3, 257, 25600);
  CHECK(frame.UsedWords() == 26);
  const auto number = r::BuildNumericGlyphPacket(
      {.font = 1, .position = 0x00150040, .digit = 2}, f::kEndLink);
  CHECK(frame.primitives[14 + r::kSpriteTextureWord] ==
        number[r::kSpriteTextureWord]);
  CHECK(frame.primitives[14 + r::kSpritePositionWord] ==
        number[r::kSpritePositionWord]);
  CHECK(frame.primitives[20 + r::kSpriteColorWord] == 0x66808080);
}

TEST_CASE("F01355 Live emits both clipped production sprites",
          "[hud-rendering-live][F01355]") {
  f::LiveFrame frame;
  b::DrawLiveSlidingStatusIcon(16, -1, 0);
  CHECK(frame.UsedWords() == 12);
  CHECK(frame.primitives[r::kSpritePositionWord] == 0x000800A0);
  CHECK(frame.primitives[r::kSpriteSizeWord] == 0x0010000F);
  CHECK(frame.primitives[r::kSpriteTextureWord] == 0x37F91081);
  CHECK(frame.primitives[6 + r::kSpritePositionWord] == 0x000800A7);
  CHECK(frame.primitives[6 + r::kSpriteTextureWord] == 0x37FF3068);
  CHECK(frame.ordering[3] == LegacyGpu_Handle(frame.primitives.data() + 6));
}

TEST_CASE("F01355 Live skips sentinel and panel-hidden invalid icons",
          "[hud-rendering-live][F01355]") {
  f::LiveFrame frame;
  frame.Disconnect();
  b::DrawLiveSlidingStatusIcon(255, -1, -1);
  Battle_hudSlideOffset = 1;
  b::DrawLiveSlidingStatusIcon(-1, -1, -1);
  CHECK(frame.UsedWords() == 0);
  CHECK(frame.ordering[3] == f::kEndLink);
}

TEST_CASE("F01360 Live emits the exact production shortcut marker packet",
          "[hud-rendering-live][F01360]") {
  f::LiveFrame frame;
  b::DrawLiveShortcutButtonMarker(-1, 0x140);
  auto expected = r::BuildSpritePacket({.draw_mode = 0xE1000017,
                                        .brightness_transparency = 0x140,
                                        .packed_position = -1,
                                        .packed_size = 0x00060006},
                                       f::kEndLink);
  expected[r::kSpriteTextureWord] = 0x373D80C0;
  CHECK(frame.UsedWords() == expected.size());
  CHECK(std::ranges::equal(std::span(frame.primitives).first(expected.size()),
                           expected));
  CHECK(frame.ordering[f::kHudOverlayHead] ==
        LegacyGpu_Handle(frame.primitives.data()));
  CHECK(frame.ordering[3] == f::kEndLink);
}
