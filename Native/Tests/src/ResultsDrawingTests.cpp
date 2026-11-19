#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Images.hpp"
#include "Render/Render.hpp"
#include "Render/TextureRegionDraw.hpp"
#include "Render/UiPrimitives.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

struct TextDraw {
  std::string text;
  std::int32_t xy = 0;
  std::uint32_t color = 0;
};

// Records quads and text in draw order and refuses quads past `capacity`.
class RecordTarget final : public r::ResultsDrawTarget {
 public:
  std::size_t capacity = 64;
  std::vector<render::UiTexturedQuad> quads;
  std::vector<TextDraw> texts;
  std::vector<char> order;
  int refused = 0;

  bool DrawQuad(const render::UiTexturedQuad& quad) override {
    order.push_back('Q');
    if (quads.size() >= capacity) {
      ++refused;
      return false;
    }
    quads.push_back(quad);
    return true;
  }
  void DrawText(std::string_view text, std::int32_t xy,
                std::uint32_t color) override {
    order.push_back('T');
    texts.push_back({std::string(text), xy, color});
  }
};

std::vector<render::TextureRegion> Atlas() {
  std::vector<render::TextureRegion> atlas(127);
  for (std::size_t i = 0; i < atlas.size(); ++i) {
    atlas[i] = {.u = static_cast<std::uint8_t>(i * 2),
                .v = static_cast<std::uint8_t>(i + 100),
                .width = static_cast<std::uint8_t>(8 + (i % 5)),
                .height = static_cast<std::uint8_t>(12 + (i % 3)),
                .texture_page = static_cast<std::uint16_t>(0x100 + i),
                .clut = static_cast<std::uint16_t>(0x7000 + i)};
  }
  return atlas;
}

bool SameColor(const render::UiTexturedQuad& quad, std::uint8_t level) {
  return quad.tint.has_value() && quad.tint->r == level &&
         quad.tint->g == level && quad.tint->b == level;
}

// Whether `quad` shows palette `clut` of page `page` at brightness `level`.
bool Shows(const render::UiTexturedQuad& quad, std::uint16_t page,
           std::uint16_t clut, std::uint8_t level) {
  return quad.texture.texture_page == page && quad.texture.clut == clut &&
         SameColor(quad, level);
}

// Whether `quad` is a shaded, semi-transparent copy of `texture` at (x, y).
bool FadingAt(const render::UiTexturedQuad& quad,
              const render::TextureRegion& texture, std::int32_t x,
              std::int32_t y) {
  return quad.top_left.x == x && quad.top_left.y == y &&
         quad.bottom_right.x == x + texture.width &&
         quad.bottom_right.y == y + texture.height && quad.translucent &&
         !quad.raw_texture && quad.texture.u == texture.u &&
         quad.texture.v == texture.v;
}

constexpr std::int32_t kHiddenXy = INT32_MIN;

// The shown prompt, or one whose positions match no drawn prompt.
r::PressButtonPromptLayout Shown(
    const std::optional<r::PressButtonPromptLayout>& layout) {
  return layout.value_or(r::PressButtonPromptLayout{.press_xy = kHiddenXy,
                                                    .button_xy = kHiddenXy});
}

TEST_CASE("F03040 fading texture dims and blends additively below eight steps",
          "[results-drawing]") {
  const auto atlas = Atlas();
  RecordTarget target;
  CHECK(r::DrawFadingTexture(atlas, 40, 50, 5, 3, target) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(target.quads.size() == 1);
  CHECK(Shows(target.quads[0], 0x105 | 0x20, 0x7006, 48));
  CHECK(FadingAt(target.quads[0], atlas[5], 40, 50));
}

TEST_CASE(
    "F03040 fading texture at eight steps uses the plain page and palette",
    "[results-drawing]") {
  const auto atlas = Atlas();
  RecordTarget target;
  CHECK(r::DrawFadingTexture(atlas, -3, 7, 9, 8, target) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawFadingTexture(atlas, -3, 7, 9, 0x7FFFFFFF, target) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(target.quads.size() == 2);
  for (const auto& quad : target.quads) {
    CHECK(SameColor(quad, 128));
    CHECK(quad.texture.texture_page == 0x109);
    CHECK(quad.texture.clut == 0x7009);
  }
  CHECK(r::DrawFadingTexture(atlas, -3, 7, 9, 7, target) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(target.quads.size() == 3);
  CHECK(SameColor(target.quads[2], 112));
}

TEST_CASE("F03040 hides at zero brightness before checking the index",
          "[results-drawing]") {
  const auto atlas = Atlas();
  RecordTarget target;
  CHECK(r::DrawFadingTexture(atlas, 0, 0, 999, 0, target) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawFadingTexture(atlas, 0, 0, -1, -5, target) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawFadingTexture(atlas, 0, 0, 127, 1, target) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawFadingTexture(atlas, 0, 0, -1, 9, target) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(target.order.empty());
}

TEST_CASE("F03040 fading texture reports a full target", "[results-drawing]") {
  const auto atlas = Atlas();
  RecordTarget target;
  target.capacity = 0;
  CHECK(r::DrawFadingTexture(atlas, 0, 0, 1, 9, target) ==
        r::ResultsDrawResult::kFull);
  CHECK(target.refused == 1);
  CHECK(target.quads.empty());
}

TEST_CASE("F03040 fading packet keeps the original FT4 words",
          "[results-drawing]") {
  const render::TextureRegion texture{.u = 250,
                                      .v = 20,
                                      .width = 10,
                                      .height = 240,
                                      .texture_page = 0x1F,
                                      .clut = 0xFFFF};
  const auto quad =
      r::FadingTextureQuad(texture, {.x = 0x12345, .y = -2}, 2, texture.clut);
  REQUIRE(quad.has_value());
  render::UiTexturedQuadPacket storage{};
  storage.fill(0xA5A5A5A5);
  const auto packet = render::BuildUiTexturedQuadPacket(
      quad.value_or(render::UiTexturedQuad{}), storage, 0x80123456);
  const render::UiTexturedQuadPacket expected{0x09123456,
                                              0x2E202020,
                                              0xFFFE2345,
                                              0x0000U << 16 | 20 << 8 | 250,
                                              0xFFFEU << 16 | 0x234F,
                                              0x3FU << 16 | 20 << 8 | 4,
                                              0x00EEU << 16 | 0x2345,
                                              0xA5A50000 | 4 << 8 | 250,
                                              0x00EEU << 16 | 0x234F,
                                              0xA5A50000 | 4 << 8 | 4};
  CHECK(packet == expected);
}

TEST_CASE("F03041 palette argument truncates and steps with the fade",
          "[results-drawing]") {
  const auto atlas = Atlas();
  RecordTarget target;
  CHECK(r::DrawFadingTextureClut(atlas, 1, 2, 3, 1, 0x1FFFF, target) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawFadingTextureClut(atlas, 1, 2, 3, 8, 0x12345, target) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawFadingTextureClut(atlas, 1, 2, 300, 8, 0, target) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawFadingTextureClut(atlas, 1, 2, 300, 0, 0, target) ==
        r::ResultsDrawResult::kHidden);
  REQUIRE(target.quads.size() == 2);
  CHECK(Shows(target.quads[0], 0x103 | 0x20, 0x0000, 16));
  CHECK(Shows(target.quads[1], 0x103, 0x2345, 128));
}

TEST_CASE("F03042 time glyphs print two digits per byte and dashes for no time",
          "[results-drawing]") {
  const auto as_string = [](const auto& glyphs) {
    return std::string(glyphs.begin(), glyphs.end());
  };
  CHECK(as_string(r::TimeGlyphs(0x01020304)) == "020304");
  CHECK(as_string(r::TimeGlyphs(0x000B3B09)) == "115909");
  CHECK(as_string(r::TimeGlyphs(0x00FF0001)) == "255000");
  CHECK(as_string(r::TimeGlyphs(0x0007C80A)) == "072001");
  CHECK(as_string(r::TimeGlyphs(0x800000)) == "------");
  CHECK(as_string(r::TimeGlyphs(0x800001)) == "128000");
  CHECK(as_string(r::TimeGlyphs(-1)) == "255255");
}

TEST_CASE("F03042 time draws digits then primes in order",
          "[results-drawing]") {
  const auto atlas = Atlas();
  RecordTarget target;
  CHECK(r::DrawTime(atlas, 100, 60, 4, 0x00010203, false, target) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(target.quads.size() == 8);
  struct Expected {
    std::size_t index;
    std::int16_t x;
    std::int16_t y;
  };
  const std::array<Expected, 8> expected{{{0, 100, 57},
                                          {1, 112, 57},
                                          {73, 124, 58},
                                          {0, 130, 57},
                                          {2, 142, 57},
                                          {74, 154, 58},
                                          {0, 160, 57},
                                          {3, 172, 57}}};
  for (std::size_t i = 0; i < expected.size(); ++i) {
    const auto& quad = target.quads[i];
    const auto& texture = atlas[expected[i].index];
    CHECK(quad.texture.u == texture.u);
    CHECK(quad.texture.v == texture.v);
    CHECK(quad.top_left.x == expected[i].x);
    CHECK(quad.top_left.y == expected[i].y);
    CHECK(quad.texture.clut == r::kResultsTimeClut + 1);
  }
}

TEST_CASE(
    "F03042 no time draws dashes level with the primes and new record palette",
    "[results-drawing]") {
  const auto atlas = Atlas();
  RecordTarget target;
  CHECK(r::DrawTime(atlas, 0, 20, 8, r::kResultsNoTime, true, target) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(target.quads.size() == 8);
  for (std::size_t i : {0U, 1U, 3U, 4U, 6U, 7U}) {
    CHECK(target.quads[i].texture.u == atlas[r::kResultsTimeDash].u);
    CHECK(target.quads[i].top_left.y == 20);
  }
  for (const auto& quad : target.quads) {
    CHECK(quad.texture.clut == r::kResultsNewRecordClut);
  }
}

TEST_CASE("F03042 time continues after a refused quad and reports full",
          "[results-drawing]") {
  const auto atlas = Atlas();
  RecordTarget target;
  target.capacity = 3;
  CHECK(r::DrawTime(atlas, 0, 0, 9, 0, false, target) ==
        r::ResultsDrawResult::kFull);
  CHECK(target.quads.size() == 3);
  CHECK(target.order.size() == 8);
  CHECK(target.refused == 5);
  CHECK(target.quads[2].texture.u == atlas[r::kResultsTimePrime].u);
}

TEST_CASE("F03042 time refuses a short atlas and hides without a timer",
          "[results-drawing]") {
  const auto atlas = Atlas();
  RecordTarget target;
  CHECK(r::DrawTime(std::span(atlas).first(83), 0, 0, 9, 0, false, target) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawTime(atlas, 0, 0, 0, 0, false, target) ==
        r::ResultsDrawResult::kHidden);
  CHECK(target.order.empty());
}

TEST_CASE("F03044 negative slide draws nothing", "[results-drawing]") {
  RecordTarget target;
  CHECK(r::DrawPressButtonPrompt(0, 0, -1, 0, target) ==
        r::ResultsDrawResult::kHidden);
  CHECK_FALSE(r::PressButtonPrompt({}, -100, 0).has_value());
  CHECK(target.order.empty());
}

TEST_CASE("F03044 prompt slides in and clamps at forty eight steps",
          "[results-drawing]") {
  const r::ResultsPoint origin{.x = 10, .y = 100};
  const auto start = Shown(r::PressButtonPrompt(origin, 0, 0));
  const auto end = Shown(r::PressButtonPrompt(origin, 48, 0));
  const auto past = Shown(r::PressButtonPrompt(origin, 1000, 0));
  CHECK(start.icon.top_left.x == 298);
  CHECK(start.press_xy == (316 | (100 << 16)));
  CHECK(start.button_xy == (316 | (110 << 16)));
  CHECK(end.icon.top_left.x == 10);
  CHECK(past.icon.top_left.x == 10);
  CHECK(past.press_xy == end.press_xy);
  CHECK(Shown(r::PressButtonPrompt({.x = -30, .y = -5}, 48, 0)).press_xy ==
        static_cast<std::int32_t>(0xFFFBFFF4));
}

TEST_CASE("F03044 icon bounces down then back up", "[results-drawing]") {
  const std::array<std::int16_t, 16> tops{104, 104, 105, 105, 106, 106,
                                          107, 107, 108, 108, 108, 108,
                                          107, 107, 107, 107};
  for (std::int32_t step = 0; step < 16; ++step) {
    const auto layout =
        Shown(r::PressButtonPrompt({.x = 0, .y = 100}, 48, step + 0x30));
    CHECK(layout.press_xy != kHiddenXy);
    CHECK(layout.icon.top_left.y == tops[static_cast<std::size_t>(step)]);
    CHECK(layout.icon.bottom_right.y ==
          tops[static_cast<std::size_t>(step)] + 16);
    CHECK(layout.icon.bottom_right.x == 16);
  }
}

TEST_CASE("F03044 prompt text precedes the raw icon", "[results-drawing]") {
  RecordTarget target;
  CHECK(r::DrawPressButtonPrompt(0, 50, 48, 0, target) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(target.order == std::vector<char>{'T', 'T', 'Q'});
  REQUIRE(target.texts.size() == 2);
  CHECK(target.texts[0].text == "PRESS");
  CHECK(target.texts[1].text == "BUTTON");
  CHECK(target.texts[0].color == 0x8080FF);
  REQUIRE(target.quads.size() == 1);
  CHECK((target.quads[0].raw_texture && !target.quads[0].translucent &&
         !target.quads[0].tint.has_value()));
}

TEST_CASE("F03044 prompt icon keeps the original FT4 words",
          "[results-drawing]") {
  RecordTarget target;
  CHECK(r::DrawPressButtonPrompt(0, 50, 48, 0, target) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(target.quads.size() == 1);
  render::UiTexturedQuadPacket storage{};
  storage.fill(0x11223344);
  const auto packet =
      render::BuildUiTexturedQuadPacket(target.quads[0], storage, 0xFFFFFF);
  CHECK(packet[1] == 0x2D223344);
  CHECK(packet[3] == 0x37FB8020);
  CHECK(packet[5] == 0x000C8030);
}

TEST_CASE("F03044 prompt reports a full target after drawing its text",
          "[results-drawing]") {
  RecordTarget target;
  target.capacity = 0;
  CHECK(r::DrawPressButtonPrompt(0, 50, 48, 0, target) ==
        r::ResultsDrawResult::kFull);
  CHECK(target.order == std::vector<char>{'T', 'T', 'Q'});
  CHECK(target.texts.size() == 2);
  CHECK(target.refused == 1);
}

constexpr std::size_t kLiveOrderingWords = 0x2088 / sizeof(u_long);
constexpr std::size_t kLiveSceneHead = 16;
constexpr std::size_t kLiveOverlayHead = 2;
constexpr std::size_t kLivePrimitiveWords = 0x20000 / sizeof(u_long);
constexpr u_long kLiveEnd = 0xFFFFFF;

struct LiveResultsFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_ordering = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  std::vector<u_long> storage = std::vector<u_long>(kLivePrimitiveWords, 0);
  std::vector<u_long> ordering =
      std::vector<u_long>(kLiveOrderingWords, kLiveEnd);

  LiveResultsFixture() {
    VS::Render::frameBuf = 0;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::legacyPrimitiveBuffers[0] = storage.data();
    VS::Render::FrameScratchpad() = {};
    VS::Render::FrameScratchpad().primitive_cursor = storage.data();
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + kLiveSceneHead;
    VS::Render::FrameScratchpad().overlay_ordering_table =
        ordering.data() + kLiveOverlayHead;
  }
  ~LiveResultsFixture() {
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_ordering;
    VS::Render::frameBuf = saved_framebuffer;
  }
  LiveResultsFixture(const LiveResultsFixture&) = delete;
  LiveResultsFixture& operator=(const LiveResultsFixture&) = delete;
  LiveResultsFixture(LiveResultsFixture&&) = delete;
  LiveResultsFixture& operator=(LiveResultsFixture&&) = delete;

  [[nodiscard]] std::ptrdiff_t Used() const {
    return static_cast<u_long*>(
               VS::Render::FrameScratchpad().primitive_cursor) -
           storage.data();
  }
  u_long& QuadEntry() { return ordering[kLiveSceneHead - 7]; }
};

TEST_CASE(
    "Live fading texture links the atlas quad seven entries before the scene",
    "[results-drawing-live]") {
  LiveResultsFixture frame;
  const VS::Render::TextureRegion& entry = Menu_Results_disMap[10];
  r::LiveDrawFadingTexture(30, 40, 10, 2);
  REQUIRE(frame.Used() == 10);
  CHECK(nextPrim(&frame.QuadEntry()) == frame.storage.data());
  CHECK((frame.storage[0] & 0xFFFFFF) == kLiveEnd);
  CHECK(frame.storage[1] == 0x2E202020);
  CHECK(frame.storage[2] == (30U | 40U << 16));
  CHECK((frame.storage[3] >> 16) == static_cast<std::uint16_t>(entry.clut + 1));
  CHECK((frame.storage[5] >> 16) == (entry.texture_page | 0x20U));
}

TEST_CASE("Live fading texture with a palette links after the previous quad",
          "[results-drawing-live]") {
  LiveResultsFixture frame;
  r::LiveDrawFadingTexture(30, 40, 10, 2);
  r::LiveDrawFadingTextureClut(0, 0, 10, 9, 0x7FF2);
  REQUIRE(frame.Used() == 20);
  CHECK(nextPrim(&frame.QuadEntry()) == frame.storage.data() + 10);
  CHECK(nextPrim(frame.storage.data() + 10) == frame.storage.data());
  CHECK((frame.storage[13] >> 16) == 0x7FF2U);
}

TEST_CASE("Live fading texture refuses an index outside the atlas",
          "[results-drawing-live]") {
  LiveResultsFixture frame;
  r::LiveDrawFadingTexture(0, 0, 127, 9);
  r::LiveDrawFadingTextureClut(0, 0, -1, 9, 0);
  CHECK(frame.Used() == 0);
  CHECK(frame.QuadEntry() == kLiveEnd);
}

TEST_CASE("Live results quads refuse a full primitive buffer",
          "[results-drawing-live]") {
  LiveResultsFixture frame;
  VS::Render::FrameScratchpad().primitive_cursor =
      frame.storage.data() + kLivePrimitiveWords - 9;
  r::LiveDrawTime(0, 0, 9, 0, 0);
  r::LiveDrawFadingTexture(0, 0, 1, 9);
  CHECK(frame.Used() == static_cast<std::ptrdiff_t>(kLivePrimitiveWords - 9));
  CHECK(frame.QuadEntry() == kLiveEnd);
  VS::Render::FrameScratchpad().primitive_cursor =
      frame.storage.data() + kLivePrimitiveWords - 10;
  r::LiveDrawFadingTexture(0, 0, 1, 9);
  CHECK(frame.Used() == static_cast<std::ptrdiff_t>(kLivePrimitiveWords));
}

TEST_CASE("Live time chains eight quads in draw order",
          "[results-drawing-live]") {
  LiveResultsFixture frame;
  r::LiveDrawTime(10, 20, 9, 0x00010203, 1);
  REQUIRE(frame.Used() == 80);
  CHECK(nextPrim(&frame.QuadEntry()) == frame.storage.data() + 70);
  CHECK(nextPrim(frame.storage.data() + 10) == frame.storage.data());
  CHECK((frame.storage[0] & 0xFFFFFF) == kLiveEnd);
  for (std::size_t quad = 0; quad < 8; ++quad) {
    CHECK((frame.storage[quad * 10 + 3] >> 16) == 0x7FF8U);
  }
}

TEST_CASE("Live prompt queues the text before the icon",
          "[results-drawing-live]") {
  LiveResultsFixture frame;
  r::LiveDrawPressButtonPrompt(0, 100, 48, 0);
  const auto used = frame.Used();
  REQUIRE(used >= 10);
  const auto icon = static_cast<std::size_t>(used - 10);
  CHECK(nextPrim(&frame.QuadEntry()) == frame.storage.data() + icon);
  CHECK((frame.storage[icon + 1] >> 24) == 0x2DU);
  CHECK(frame.storage[icon + 2] == (104U << 16));
  r::LiveDrawPressButtonPrompt(0, 100, -1, 0);
  CHECK(frame.Used() == used);
}
}  // namespace
